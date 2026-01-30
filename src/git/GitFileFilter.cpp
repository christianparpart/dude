// SPDX-License-Identifier: Apache-2.0

#include "GitFileFilter.hpp"

#include <array>
#include <cstdio>
#include <format>
#include <memory>
#include <print>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

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

    // NOLINTNEXTLINE(cert-env33-c) -- popen is intentional for git subprocess communication
    auto* pipe = popen(command.c_str(), "r");
    if (!pipe)
        return std::unexpected(GitFileFilterError{.message = "Failed to execute git (is git on PATH?)"});

    std::string output;
    std::array<char, 4096> buffer{};
    while (auto* result = fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
        output += result;

    auto const status = pclose(pipe);
    if (status != 0)
        return std::unexpected(GitFileFilterError{.message = "Not a git repository"});

    // Trim trailing newlines.
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();

    if (output.empty())
        return std::unexpected(GitFileFilterError{.message = "git rev-parse returned empty output"});

    return std::filesystem::path(output);
}

auto GitFileFilter::QueryNonIgnoredFiles(std::filesystem::path const& gitRoot, std::filesystem::path const& directory)
    -> std::expected<std::unordered_set<std::string>, GitFileFilterError>
{
    // --cached: tracked files, --others: untracked files, --exclude-standard: respect .gitignore
    // -z: NUL-delimited output for safe parsing of paths with spaces
    auto const command = std::format("git -C {} ls-files --cached --others --exclude-standard -z -- {} 2>{}",
                                     gitRoot.string(), directory.string(), nullDevice);

    // NOLINTNEXTLINE(cert-env33-c) -- popen is intentional for git subprocess communication
    auto* pipe = popen(command.c_str(), "r");
    if (!pipe)
        return std::unexpected(GitFileFilterError{.message = "Failed to execute git ls-files"});

    // Read binary output (NUL-delimited), so use fread instead of fgets.
    std::string output;
    std::array<char, 8192> buffer{};
    while (auto const bytesRead = fread(buffer.data(), 1, buffer.size(), pipe))
        output.append(buffer.data(), bytesRead);

    auto const status = pclose(pipe);
    if (status != 0)
        return std::unexpected(GitFileFilterError{.message = "git ls-files failed"});

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
            // cause weakly_canonical to throw. Skip such paths gracefully.
            try
            {
                auto const fullPath = std::filesystem::weakly_canonical(gitRoot / pathStr);
                files.insert(fullPath.string());
            }
            catch (std::filesystem::filesystem_error const&)
            {
                // Skip paths that cannot be canonicalized (e.g. reserved device names on Windows).
            }
        }
        start = (end == std::string::npos) ? output.size() : end + 1;
    }

    return files;
}

auto GitFileFilter::CreateFilter(std::filesystem::path const& directory, bool verbose)
    -> std::optional<codedup::FileFilter>
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

    return codedup::FileFilter(
        [allowedFiles](std::filesystem::path const& path) -> bool
        {
            auto const canonical = std::filesystem::weakly_canonical(path).string();
            return allowedFiles->contains(canonical);
        });
}

} // namespace git
