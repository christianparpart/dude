// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/FileScanner.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>

namespace cli
{

/// @brief Error information from git file-filter operations.
struct GitFileFilterError
{
    std::string message; ///< Description of the error.
};

/// @brief Queries git for non-ignored files and produces a file filter predicate.
///
/// Git interaction is isolated in the CLI layer; the core library remains git-free.
/// Uses `git ls-files` to determine which files are tracked or untracked-but-not-ignored,
/// correctly handling nested `.gitignore` files, global gitignore, and negation patterns.
class GitFileFilter
{
public:
    /// @brief Finds the git repository root for the given directory.
    /// @param directory A directory that may reside inside a git repository.
    /// @return The absolute path to the repository root, or an error if not a git repo.
    [[nodiscard]] static auto FindGitRoot(std::filesystem::path const& directory)
        -> std::expected<std::filesystem::path, GitFileFilterError>;

    /// @brief Queries git for all non-ignored files under the given directory.
    /// @param gitRoot The root of the git repository (from FindGitRoot).
    /// @param directory The directory to query files for.
    /// @return A set of canonical path strings for non-ignored files, or an error.
    [[nodiscard]] static auto QueryNonIgnoredFiles(std::filesystem::path const& gitRoot,
                                                   std::filesystem::path const& directory)
        -> std::expected<std::unordered_set<std::string>, GitFileFilterError>;

    /// @brief Creates a file filter predicate that excludes gitignored files.
    ///
    /// Combines FindGitRoot and QueryNonIgnoredFiles. Returns std::nullopt if the
    /// directory is not inside a git repository or git is not available, allowing
    /// graceful degradation to unfiltered scanning.
    ///
    /// @param directory The directory to create the filter for.
    /// @param verbose If true, prints diagnostic messages to stderr.
    /// @return A FileFilter predicate, or std::nullopt if git filtering is unavailable.
    [[nodiscard]] static auto CreateFilter(std::filesystem::path const& directory, bool verbose = false)
        -> std::optional<codedup::FileFilter>;
};

} // namespace cli
