// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace dude
{

/// @brief RAII wrapper for memory-mapped file I/O.
///
/// Provides zero-copy read access to file contents via the OS virtual memory system.
/// On UNIX, uses mmap(2) with MADV_SEQUENTIAL for read-ahead prefetching.
/// On Windows, uses CreateFileMapping/MapViewOfFile.
///
/// Move-only; not copyable.
class DUDE_API MappedFile
{
public:
    /// @brief Opens a file and maps it into memory.
    /// @param filePath Path to the file to map.
    /// @return The mapped file on success, or an error message on failure.
    [[nodiscard]] static auto Open(std::filesystem::path const& filePath) -> std::expected<MappedFile, std::string>
    {
#if defined(_WIN32)
        return OpenWindows(filePath);
#else
        return OpenUnix(filePath);
#endif
    }

    /// @brief Returns a string_view over the mapped file contents.
    [[nodiscard]] auto View() const noexcept -> std::string_view { return {static_cast<char const*>(_data), _size}; }

    /// @brief Returns the size of the mapped file in bytes.
    [[nodiscard]] auto Size() const noexcept -> size_t { return _size; }

    /// @brief Returns true if the mapping is valid.
    [[nodiscard]] auto IsValid() const noexcept -> bool { return _data != nullptr; }

    ~MappedFile() { Unmap(); }

    MappedFile(MappedFile const&) = delete;
    auto operator=(MappedFile const&) -> MappedFile& = delete;

    MappedFile(MappedFile&& other) noexcept
        : _data(other._data)
        , _size(other._size)
#if defined(_WIN32)
        , _fileHandle(other._fileHandle)
        , _mappingHandle(other._mappingHandle)
#endif
    {
        other._data = nullptr;
        other._size = 0;
#if defined(_WIN32)
        other._fileHandle = INVALID_HANDLE_VALUE;
        other._mappingHandle = nullptr;
#endif
    }

    auto operator=(MappedFile&& other) noexcept -> MappedFile&
    {
        if (this != &other)
        {
            Unmap();
            _data = other._data;
            _size = other._size;
#if defined(_WIN32)
            _fileHandle = other._fileHandle;
            _mappingHandle = other._mappingHandle;
            other._fileHandle = INVALID_HANDLE_VALUE;
            other._mappingHandle = nullptr;
#endif
            other._data = nullptr;
            other._size = 0;
        }
        return *this;
    }

private:
    void const* _data = nullptr;
    size_t _size = 0;

#if defined(_WIN32)
    HANDLE _fileHandle = INVALID_HANDLE_VALUE;
    HANDLE _mappingHandle = nullptr;
#endif

    MappedFile() = default;

    void Unmap() noexcept
    {
        if (!_data)
            return;

#if defined(_WIN32)
        UnmapViewOfFile(_data);
        if (_mappingHandle)
            CloseHandle(_mappingHandle);
        if (_fileHandle != INVALID_HANDLE_VALUE)
            CloseHandle(_fileHandle);
        _mappingHandle = nullptr;
        _fileHandle = INVALID_HANDLE_VALUE;
#else
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        ::munmap(const_cast<void*>(_data), _size);
#endif
        _data = nullptr;
        _size = 0;
    }

#if defined(_WIN32)
    [[nodiscard]] static auto OpenWindows(std::filesystem::path const& filePath)
        -> std::expected<MappedFile, std::string>
    {
        auto const wpath = filePath.wstring();

        HANDLE fileHandle = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE)
            return std::unexpected("Failed to open file: " + filePath.string());

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(fileHandle, &fileSize))
        {
            CloseHandle(fileHandle);
            return std::unexpected("Failed to get file size: " + filePath.string());
        }

        // Handle empty files — no mapping needed
        if (fileSize.QuadPart == 0)
        {
            CloseHandle(fileHandle);
            MappedFile result;
            return result;
        }

        HANDLE mappingHandle = CreateFileMappingW(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mappingHandle)
        {
            CloseHandle(fileHandle);
            return std::unexpected("Failed to create file mapping: " + filePath.string());
        }

        void const* data = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
        if (!data)
        {
            CloseHandle(mappingHandle);
            CloseHandle(fileHandle);
            return std::unexpected("Failed to map view of file: " + filePath.string());
        }

        MappedFile result;
        result._data = data;
        result._size = static_cast<size_t>(fileSize.QuadPart);
        result._fileHandle = fileHandle;
        result._mappingHandle = mappingHandle;
        return result;
    }
#else
    [[nodiscard]] static auto OpenUnix(std::filesystem::path const& filePath) -> std::expected<MappedFile, std::string>
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        auto const fd = ::open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd == -1)
            return std::unexpected("Failed to open file: " + filePath.string());

        struct stat st{};
        if (::fstat(fd, &st) == -1)
        {
            ::close(fd);
            return std::unexpected("Failed to stat file: " + filePath.string());
        }

        auto const fileSize = static_cast<size_t>(st.st_size);

        // Handle empty files — no mapping needed
        if (fileSize == 0)
        {
            ::close(fd);
            MappedFile result;
            return result;
        }

        void* data = ::mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd); // fd can be closed immediately after mmap

        if (data == MAP_FAILED) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
            return std::unexpected("Failed to mmap file: " + filePath.string());

        // Hint to OS: we will read sequentially, enable read-ahead
        ::madvise(data, fileSize, MADV_SEQUENTIAL);

        MappedFile result;
        result._data = data;
        result._size = fileSize;
        return result;
    }
#endif
};

} // namespace dude
