// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/AnalysisScope.hpp>
#include <dude/Api.hpp>
#include <dude/CloneDetector.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace dude
{

/// @brief Filters clone groups based on the active analysis scope.
///
/// This class provides post-hoc filtering of inter-function clone groups
/// to restrict results to the requested scope (inter-file, intra-file, or both).
class DUDE_API ScopeFilter
{
public:
    /// @brief Filters clone groups according to the active analysis scope.
    ///
    /// - Both InterFile + IntraFile (= InterFunction): returns groups unchanged.
    /// - InterFile only: keeps groups where blocks span >= 2 distinct files.
    /// - IntraFile only: splits each group by file, emitting sub-groups with >= 2 blocks per file.
    /// - Neither set: returns empty.
    ///
    /// @param groups The detected clone groups.
    /// @param blockToFileIndex Mapping from block index to file index.
    /// @param scope The active analysis scope bitmask.
    /// @return Filtered (and possibly split) clone groups.
    [[nodiscard]] static auto FilterCloneGroups(std::vector<CloneGroup> const& groups,
                                                std::span<size_t const> blockToFileIndex, AnalysisScope scope)
        -> std::vector<CloneGroup>;
};

} // namespace dude
