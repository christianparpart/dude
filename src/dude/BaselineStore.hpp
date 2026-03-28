// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>
#include <dude/CloneDetector.hpp>
#include <dude/CodeBlock.hpp>
#include <dude/IntraFunctionDetector.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace dude
{

/// @brief Error information from baseline operations.
struct BaselineError
{
    std::string message; ///< Description of the error.
};

/// @brief Canonical identity of a clone group member, independent of block indices.
struct CloneGroupMember
{
    std::string filePath;     ///< Relative path from project root.
    std::string functionName; ///< Function/method name.
    uint32_t startLine = 0;   ///< Start line of the block.
    uint32_t endLine = 0;     ///< End line of the block.

    auto operator<=>(CloneGroupMember const&) const = default;
};

/// @brief Canonical identity of a clone group, independent of block indices.
///
/// A clone group is identified by the sorted set of its member locations.
/// This allows matching groups across runs even when block indices shift
/// due to added/removed files.
struct CloneIdentity
{
    std::vector<CloneGroupMember> members; ///< Sorted member list.
    double avgSimilarity = 0.0;            ///< Average similarity (not part of identity comparison).

    /// @brief Two identities are equal if their member lists match (similarity excluded).
    auto operator==(CloneIdentity const& other) const -> bool { return members == other.members; }
};

/// @brief Canonical identity of an intra-function clone pair.
struct IntraCloneIdentity
{
    std::string filePath;     ///< Relative path from project root.
    std::string functionName; ///< Function/method name.
    uint32_t startLine = 0;   ///< Block start line.
    uint32_t endLine = 0;     ///< Block end line.
    size_t regionAStart = 0;  ///< Region A start offset in normalized token sequence.
    size_t regionALength = 0; ///< Region A length.
    size_t regionBStart = 0;  ///< Region B start offset.
    size_t regionBLength = 0; ///< Region B length.
    double similarity = 0.0;  ///< Pair similarity (not part of identity comparison).

    /// @brief Two identities are equal if their location and region fields match (similarity excluded).
    auto operator==(IntraCloneIdentity const& other) const -> bool
    {
        return filePath == other.filePath && functionName == other.functionName && startLine == other.startLine &&
               endLine == other.endLine && regionAStart == other.regionAStart && regionALength == other.regionALength &&
               regionBStart == other.regionBStart && regionBLength == other.regionBLength;
    }
};

/// @brief A saved baseline snapshot of analysis results.
struct Baseline
{
    std::string name;                            ///< Baseline name (e.g., git commit SHA).
    std::string timestamp;                       ///< ISO 8601 creation timestamp.
    std::vector<CloneIdentity> cloneGroups;      ///< Inter-function clone identities.
    std::vector<IntraCloneIdentity> intraClones; ///< Intra-function clone identities.
};

/// @brief Saves and loads analysis baselines for differential reporting.
///
/// Baselines are stored as JSON files in a directory. Each baseline file
/// contains clone group and intra-clone identities that are independent
/// of block indices, enabling comparison across runs.
class DUDE_API BaselineStore
{
public:
    /// @brief Constructs a BaselineStore rooted at the given directory.
    /// @param baselineDir The directory to store baselines in (e.g., .dude-cache/baselines/).
    explicit BaselineStore(std::filesystem::path baselineDir);

    /// @brief Saves a baseline from the current analysis results.
    /// @param name The baseline name.
    /// @param groups Clone groups from the current run.
    /// @param intraResults Intra-clone results from the current run.
    /// @param blocks All code blocks.
    /// @param files File path vector for resolving file indices.
    /// @param projectRoot Project root for computing relative paths.
    /// @return void on success, or an error.
    auto Save(std::string const& name, std::vector<CloneGroup> const& groups,
              std::vector<IntraCloneResult> const& intraResults, std::vector<CodeBlock> const& blocks,
              std::span<std::filesystem::path const> files, std::filesystem::path const& projectRoot)
        -> std::expected<void, BaselineError>;

    /// @brief Loads a previously saved baseline.
    /// @param name The baseline name to load.
    /// @return The baseline data, or an error.
    [[nodiscard]] auto Load(std::string const& name) const -> std::expected<Baseline, BaselineError>;

    /// @brief Checks whether a named baseline exists.
    /// @param name The baseline name.
    /// @return True if the baseline file exists.
    [[nodiscard]] auto Exists(std::string const& name) const -> bool;

    /// @brief Lists all available baseline names.
    [[nodiscard]] auto List() const -> std::vector<std::string>;

    /// @brief Converts current clone groups to clone identities for comparison.
    [[nodiscard]] static auto
    BuildCloneIdentities(std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                         std::span<std::filesystem::path const> files, std::filesystem::path const& projectRoot)
        -> std::vector<CloneIdentity>;

    /// @brief Converts intra-clone results to identities.
    [[nodiscard]] static auto
    BuildIntraCloneIdentities(std::vector<IntraCloneResult> const& results, std::vector<CodeBlock> const& blocks,
                              std::span<std::filesystem::path const> files, std::filesystem::path const& projectRoot)
        -> std::vector<IntraCloneIdentity>;

    /// @brief Filters clone groups to only those NOT present in the baseline.
    /// @param currentGroups Current clone groups.
    /// @param baseline The baseline to compare against.
    /// @param blocks Current code blocks.
    /// @param files Current file paths.
    /// @param projectRoot Project root.
    /// @return Groups that are new (not in the baseline).
    [[nodiscard]] static auto FindNewCloneGroups(std::vector<CloneGroup> const& currentGroups, Baseline const& baseline,
                                                 std::vector<CodeBlock> const& blocks,
                                                 std::span<std::filesystem::path const> files,
                                                 std::filesystem::path const& projectRoot) -> std::vector<CloneGroup>;

    /// @brief Filters intra-clone results to only those NOT present in the baseline.
    [[nodiscard]] static auto FindNewIntraClones(std::vector<IntraCloneResult> const& currentResults,
                                                 Baseline const& baseline, std::vector<CodeBlock> const& blocks,
                                                 std::span<std::filesystem::path const> files,
                                                 std::filesystem::path const& projectRoot)
        -> std::vector<IntraCloneResult>;

private:
    /// @brief Returns the file path for a baseline name.
    [[nodiscard]] auto BaselinePath(std::string const& name) const -> std::filesystem::path;

    std::filesystem::path _baselineDir;
};

} // namespace dude
