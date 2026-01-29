// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/DiffRange.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace cli
{

/// @brief Error information from git diff operations.
struct GitDiffError
{
    std::string message; ///< Description of the error.
};

/// @brief Runs git diff and parses the output into structured diff data.
///
/// Git interaction is isolated in the CLI layer; the core library remains git-free.
class GitDiffParser
{
public:
    /// @brief Executes `git diff --no-color -U0 <baseRef>...HEAD` in the given project root.
    /// @param projectRoot The directory in which to run git diff.
    /// @param baseRef The git ref to diff against (branch, tag, or SHA).
    /// @return The raw diff output string, or an error if git fails.
    [[nodiscard]] static auto runGitDiff(std::filesystem::path const& projectRoot, std::string const& baseRef)
        -> std::expected<std::string, GitDiffError>;

    /// @brief Parses unified diff output into structured file-change data.
    ///
    /// Extracts file paths from `diff --git a/... b/...` lines and line ranges from
    /// `@@ ... +start,count @@` hunk headers. Skips deleted files (where new path is /dev/null)
    /// and binary files. Optionally filters to files matching the given extensions.
    ///
    /// @param diffOutput Raw unified diff output from git.
    /// @param extensions File extensions to include (e.g., ".cpp"). Empty means include all.
    /// @return Parsed diff result with per-file changed line ranges.
    [[nodiscard]] static auto parseDiffOutput(std::string const& diffOutput,
                                              std::vector<std::string> const& extensions = {}) -> codedup::DiffResult;
};

} // namespace cli
