// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/DiffRange.hpp>
#include <codedup/IntraFunctionDetector.hpp>

#include <filesystem>
#include <span>
#include <unordered_set>
#include <vector>

namespace codedup
{

/// @brief Filters clone detection results to only include blocks that overlap with changed lines.
///
/// This class provides post-hoc filtering of detection results based on a git diff,
/// enabling CI pipelines to check only changed code for duplication.
class CODEDUP_API DiffFilter
{
public:
    /// @brief Finds indices of code blocks whose source ranges overlap with any changed line range.
    ///
    /// File paths are compared using std::filesystem::weakly_canonical() for consistent matching
    /// between git diff paths (relative) and FileScanner paths (absolute).
    ///
    /// @param blocks All extracted code blocks.
    /// @param diff The parsed git diff result containing changed file/line information.
    /// @param projectRoot The project root directory for resolving relative diff paths.
    /// @param files The file path vector for resolving file indices to paths.
    /// @return A set of block indices that overlap with changed lines.
    [[nodiscard]] static auto FindChangedBlocks(std::vector<CodeBlock> const& blocks, DiffResult const& diff,
                                                std::filesystem::path const& projectRoot,
                                                std::span<std::filesystem::path const> files)
        -> std::unordered_set<size_t>;

    /// @brief Filters clone groups to only include those with at least one changed block.
    ///
    /// A group is kept if any of its block indices is in the changed set.
    /// The full group is preserved so the user sees what the changed function duplicates.
    ///
    /// @param groups The detected clone groups.
    /// @param changedBlocks The set of block indices overlapping with changed lines.
    /// @return Filtered clone groups.
    [[nodiscard]] static auto FilterCloneGroups(std::vector<CloneGroup> const& groups,
                                                std::unordered_set<size_t> const& changedBlocks)
        -> std::vector<CloneGroup>;

    /// @brief Filters intra-clone results to only include those for changed blocks.
    ///
    /// An IntraCloneResult is kept if its blockIndex is in the changed set.
    ///
    /// @param results The detected intra-clone results.
    /// @param changedBlocks The set of block indices overlapping with changed lines.
    /// @return Filtered intra-clone results.
    [[nodiscard]] static auto FilterIntraResults(std::vector<IntraCloneResult> const& results,
                                                 std::unordered_set<size_t> const& changedBlocks)
        -> std::vector<IntraCloneResult>;
};

} // namespace codedup
