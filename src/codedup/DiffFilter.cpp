// SPDX-License-Identifier: Apache-2.0

#include <codedup/DiffFilter.hpp>

#include <algorithm>
#include <ranges>

namespace codedup
{

auto DiffFilter::FindChangedBlocks(std::vector<CodeBlock> const& blocks, DiffResult const& diff,
                                   std::filesystem::path const& projectRoot) -> std::unordered_set<size_t>
{
    std::unordered_set<size_t> changedIndices;

    // Pre-canonicalize diff file paths for efficient comparison.
    struct CanonicalFileChanges
    {
        std::filesystem::path canonicalPath;
        std::vector<LineRange> const* changedRanges;
    };

    std::vector<CanonicalFileChanges> canonicalDiff;
    canonicalDiff.reserve(diff.size());
    for (auto const& fc : diff)
    {
        auto resolvedPath = fc.filePath.is_relative() ? projectRoot / fc.filePath : fc.filePath;
        canonicalDiff.push_back({
            .canonicalPath = std::filesystem::weakly_canonical(resolvedPath),
            .changedRanges = &fc.changedRanges,
        });
    }

    for (auto const blockIdx : std::views::iota(size_t{0}, blocks.size()))
    {
        auto const& block = blocks[blockIdx];
        auto const blockFile = std::filesystem::weakly_canonical(block.sourceRange.start.filePath);
        auto const blockStart = block.sourceRange.start.line;
        auto const blockEnd = block.sourceRange.end.line;

        for (auto const& cfc : canonicalDiff)
        {
            if (cfc.canonicalPath != blockFile)
                continue;

            for (auto const& range : *cfc.changedRanges)
            {
                if (Overlaps(range, blockStart, blockEnd))
                {
                    changedIndices.insert(blockIdx);
                    goto nextBlock; // NOLINT -- double break
                }
            }
        }
    nextBlock:;
    }

    return changedIndices;
}

auto DiffFilter::FilterCloneGroups(std::vector<CloneGroup> const& groups,
                                   std::unordered_set<size_t> const& changedBlocks) -> std::vector<CloneGroup>
{
    std::vector<CloneGroup> filtered;
    for (auto const& group : groups)
    {
        auto const hasChanged = std::ranges::any_of(group.blockIndices, [&changedBlocks](size_t idx)
                                                    { return changedBlocks.contains(idx); });
        if (hasChanged)
            filtered.push_back(group);
    }
    return filtered;
}

auto DiffFilter::FilterIntraResults(std::vector<IntraCloneResult> const& results,
                                    std::unordered_set<size_t> const& changedBlocks) -> std::vector<IntraCloneResult>
{
    std::vector<IntraCloneResult> filtered;
    for (auto const& result : results)
    {
        if (changedBlocks.contains(result.blockIndex))
            filtered.push_back(result);
    }
    return filtered;
}

} // namespace codedup
