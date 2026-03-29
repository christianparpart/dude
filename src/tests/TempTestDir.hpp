// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

namespace test_utils
{

/// @brief Returns a unique temporary path with the given prefix.
///
/// Uses a random seed + atomic counter to ensure uniqueness across
/// threads and parallel test processes.
inline auto UniqueTempPath(std::string_view prefix) -> std::filesystem::path
{
    static auto const seed = std::random_device{}();
    static std::atomic<unsigned> counter{0};
    return std::filesystem::temp_directory_path() / std::format("{}_{}_{}", prefix, seed, counter.fetch_add(1));
}

/// @brief RAII guard for a temporary directory that is removed on destruction.
///
/// Creates a unique temporary directory on construction and recursively
/// removes it on destruction. Supports writing files into the directory.
class TempTestDir
{
public:
    /// @brief Creates a unique temporary directory with the given prefix.
    /// @param prefix Name prefix for the temp directory (default: "dude_test").
    explicit TempTestDir(std::string_view prefix = "dude_test") : _path(UniqueTempPath(prefix))
    {
        std::filesystem::create_directories(_path);
    }

    ~TempTestDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }

    TempTestDir(TempTestDir const&) = delete;
    TempTestDir(TempTestDir&&) = delete;
    auto operator=(TempTestDir const&) -> TempTestDir& = delete;
    auto operator=(TempTestDir&&) -> TempTestDir& = delete;

    /// @brief Returns the root path of the temporary directory.
    [[nodiscard]] auto Path() const -> std::filesystem::path const& { return _path; }

    /// @brief Writes a file with the given content in the temporary directory.
    /// @param relativePath Relative path within the temp directory.
    /// @param content File content to write.
    void WriteFile(std::filesystem::path const& relativePath, std::string_view content = "// test\n") const
    {
        auto const fullPath = _path / relativePath;
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream out(fullPath);
        out << content;
    }

private:
    std::filesystem::path _path;
};

} // namespace test_utils
