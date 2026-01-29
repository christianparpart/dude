// SPDX-License-Identifier: Apache-2.0

#include <codedup/ScopeFilter.hpp>

#include <algorithm>
#include <ranges>
#include <unordered_map>

namespace codedup
{

auto ScopeFilter::FilterCloneGroups(std::vector<CloneGroup> const& groups, std::span<size_t const> blockToFileIndex,
                                    AnalysisScope scope) -> std::vector<CloneGroup>
{
    auto const hasInterFile = HasScope(scope, AnalysisScope::InterFile);
    auto const hasIntraFile = HasScope(scope, AnalysisScope::IntraFile);

    // Both set (= InterFunction): pass through unchanged.
    if (hasInterFile && hasIntraFile)
        return groups;

    // Neither set: no inter-function results wanted.
    if (!hasInterFile && !hasIntraFile)
        return {};

    std::vector<CloneGroup> result;

    if (hasInterFile)
    {
        // Keep groups where blocks span >= 2 distinct files.
        for (auto const& group : groups)
        {
            if (group.blockIndices.size() < 2)
                continue;

            auto const firstFile =
                group.blockIndices.front() < blockToFileIndex.size() ? blockToFileIndex[group.blockIndices.front()] : 0;
            auto const crossFile =
                std::ranges::any_of(group.blockIndices, [&](size_t idx)
                                    { return idx < blockToFileIndex.size() && blockToFileIndex[idx] != firstFile; });
            if (crossFile)
                result.push_back(group);
        }
    }
    else
    {
        // IntraFile only: split each group by file, emit sub-groups with >= 2 blocks per file.
        for (auto const& group : groups)
        {
            // Bucket block indices by their file index.
            std::unordered_map<size_t, std::vector<size_t>> byFile;
            for (auto const blockIdx : group.blockIndices)
            {
                auto const fileIdx = blockIdx < blockToFileIndex.size() ? blockToFileIndex[blockIdx] : 0;
                byFile[fileIdx].push_back(blockIdx);
            }

            for (auto& [fileIdx, indices] : byFile)
            {
                if (indices.size() >= 2)
                    result.push_back(
                        CloneGroup{.blockIndices = std::move(indices), .avgSimilarity = group.avgSimilarity});
            }
        }
    }

    return result;
}

} // namespace codedup
