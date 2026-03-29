// SPDX-License-Identifier: Apache-2.0

#include "GitFileFilter.hpp"

#include "GitCommand.hpp"

#include <format>
#include <memory>
#include <print>

namespace git
{

#ifdef _WIN32
constexpr auto nullDevice = "NUL";
#else
constexpr auto nullDevice = "/dev/null";
#endif

auto GitFileFilter::FindGitRoot(std::filesystem::path const& directory)
    -> std::expected<std::filesystem::path, GitFileFilterError>
{
    auto const command = std::format("git -C {} rev-parse --show-toplevel 2>{}", directory.string(), nullDevice);

    auto result = RunCommand(command, "git rev-parse --show-toplevel");
    if (!result)
        return std::unexpected(GitFileFilterError{.message = "Not a git repository"});

    if (result->output.empty())
        return std::unexpected(GitFileFilterError{.message = "git rev-parse returned empty output"});

    return std::filesystem::path(result->output);
}

auto GitFileFilter::QueryNonIgnoredFiles(std::filesystem::path const& gitRoot, std::filesystem::path const& directory)
    -> std::expected<std::unordered_set<std::string>, GitFileFilterError>
{
    // --cached: tracked files, --others: untracked files, --exclude-standard: respect .gitignore
    // -z: NUL-delimited output for safe parsing of paths with spaces
    auto const command = std::format("git -C {} ls-files --cached --others --exclude-standard -z -- {} 2>{}",
                                     gitRoot.string(), directory.string(), nullDevice);

    auto rawResult = RunCommandRaw(command, "git ls-files");
    if (!rawResult)
        return std::unexpected(GitFileFilterError{.message = std::move(rawResult.error().message)});

    auto const& output = *rawResult;

    // Parse NUL-separated paths and resolve to canonical paths.
    std::unordered_set<std::string> files;
    size_t start = 0;
    while (start < output.size())
    {
        auto const end = output.find('\0', start);
        auto const pathStr = output.substr(start, end == std::string::npos ? end : end - start);
        if (!pathStr.empty())
        {
            // git ls-files outputs paths relative to the git root.
            // On Windows, paths containing reserved device names (NUL, CON, PRN, etc.)
            // cause weakly_canonical to fail. Skip such paths gracefully.
            std::error_code ec;
            auto const fullPath = std::filesystem::weakly_canonical(gitRoot / pathStr, ec);
            if (!ec)
                files.insert(fullPath.string());
        }
        start = (end == std::string::npos) ? output.size() : end + 1;
    }

    return files;
}

auto GitFileFilter::CreateFilter(std::filesystem::path const& directory, bool verbose)
    -> std::optional<dude::FileFilter>
{
    auto const gitRoot = FindGitRoot(directory);
    if (!gitRoot)
    {
        if (verbose)
            std::println(stderr, "Not a git repository, skipping .gitignore filtering");
        return std::nullopt;
    }

    if (verbose)
        std::println(stderr, "Git root: {}", gitRoot->string());

    auto filesResult = QueryNonIgnoredFiles(*gitRoot, directory);
    if (!filesResult)
    {
        if (verbose)
            std::println(stderr, "Warning: Failed to query git files: {}", filesResult.error().message);
        return std::nullopt;
    }

    if (verbose)
        std::println(stderr, "Git reports {} non-ignored files", filesResult->size());

    // Move the set into a shared_ptr so the lambda is copyable.
    auto allowedFiles = std::make_shared<std::unordered_set<std::string>>(std::move(*filesResult));

    return dude::FileFilter(
        [allowedFiles](std::filesystem::path const& path) -> bool
        {
            auto const canonical = std::filesystem::weakly_canonical(path).string();
            return allowedFiles->contains(canonical);
        });
}

} // namespace git
