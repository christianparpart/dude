// SPDX-License-Identifier: Apache-2.0
#include <codedup/FileScanner.hpp>
#include <codedup/LanguageRegistry.hpp>

#include <algorithm>

namespace codedup
{

auto FileScanner::Scan(std::filesystem::path const& directory, std::vector<std::string> const& extensions,
                       std::optional<FileFilter> const& filter)
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

        auto extensionMatched = false;
        for (auto const& allowedExt : extensions)
        {
            auto lowerAllowed = allowedExt;
            std::ranges::transform(lowerAllowed, lowerAllowed.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext == lowerAllowed)
            {
                extensionMatched = true;
                break;
            }
        }

        if (!extensionMatched)
            continue;

        if (filter.has_value() && !(*filter)(entry.path()))
            continue;

        result.push_back(entry.path());
    }

    if (ec)
        return std::unexpected(FileScanError{.message = "Error during directory scan: " + ec.message()});

    std::ranges::sort(result);
    return result;
}

auto FileScanner::DefaultExtensions() -> std::vector<std::string> const&
{
    static auto const extensions = LanguageRegistry::Instance().AllExtensions();
    return extensions;
}

} // namespace codedup
