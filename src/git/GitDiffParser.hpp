// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/DiffRange.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace git
{

/// @brief Error information from git diff operations.
struct GitDiffError
{
    std::string message; ///< Description of the error.
};

/// @brief Runs git diff and parses the output into structured diff data.
///
/// Git interaction is isolated from the core library, which remains git-free.
class GitDiffParser
{
public:
    /// @brief Executes `git diff --no-color -U0 <baseRef>...<sourceRef>` in the given project root.
    /// @param projectRoot The directory in which to run git diff.
    /// @param baseRef The git ref to diff against (branch, tag, or SHA).
    /// @param sourceRef The git ref for the source side of the diff (default: "HEAD").
    /// @return The raw diff output string, or an error if git fails.
    [[nodiscard]] static auto RunGitDiff(std::filesystem::path const& projectRoot, std::string const& baseRef,
                                         std::string const& sourceRef = "HEAD")
        -> std::expected<std::string, GitDiffError>;

    /// @brief Executes `git show --no-color -U0 --format=` for each commit SHA
    /// and returns the concatenated diff output.
    ///
    /// This enables analyzing changes introduced by specific commits rather than
    /// comparing two branches. The output format is identical to `git diff`, so
    /// it can be parsed by ParseDiffOutput().
    ///
    /// @param projectRoot The directory in which to run git show.
    /// @param commits One or more commit SHAs to produce diffs for.
    /// @return The concatenated raw diff output, or an error if git fails.
    [[nodiscard]] static auto RunGitShow(std::filesystem::path const& projectRoot,
                                         std::vector<std::string> const& commits)
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
    [[nodiscard]] static auto ParseDiffOutput(std::string const& diffOutput,
                                              std::vector<std::string> const& extensions = {}) -> dude::DiffResult;
};

} // namespace git
