// SPDX-License-Identifier: Apache-2.0
#include <codedup/FileScanner.hpp>

#include <algorithm>

namespace codedup
{

auto FileScanner::Scan(std::filesystem::path const& directory, std::vector<std::string> const& extensions)
    -> std::expected<std::vector<std::filesystem::path>, FileScanError>
{
    if (!std::filesystem::exists(directory))
        return std::unexpected(FileScanError{.message = "Directory does not exist: " + directory.string()});

    if (!std::filesystem::is_directory(directory))
        return std::unexpected(FileScanError{.message = "Path is not a directory: " + directory.string()});

    std::vector<std::filesystem::path> result;
    std::error_code ec;

    for (auto const& entry : std::filesystem::recursive_directory_iterator(directory, ec))
    {
        if (ec)
            return std::unexpected(FileScanError{.message = "Error scanning directory: " + ec.message()});

        if (!entry.is_regular_file())
            continue;

        auto ext = entry.path().extension().string();

        // Case-insensitive extension comparison
        std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return std::tolower(c); });

        for (auto const& allowedExt : extensions)
        {
            auto lowerAllowed = allowedExt;
            std::ranges::transform(lowerAllowed, lowerAllowed.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext == lowerAllowed)
            {
                result.push_back(entry.path());
                break;
            }
        }
    }

    if (ec)
        return std::unexpected(FileScanError{.message = "Error during directory scan: " + ec.message()});

    std::ranges::sort(result);
    return result;
}

auto FileScanner::DefaultExtensions() -> std::vector<std::string> const&
{
    static std::vector<std::string> const extensions = {
        ".cpp", ".cxx", ".cc", ".c", ".h", ".hpp", ".hxx",
    };
    return extensions;
}

} // namespace codedup
