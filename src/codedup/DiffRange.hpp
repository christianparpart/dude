// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace codedup
{

/// @brief Represents a contiguous range of 1-based, inclusive source lines.
struct LineRange
{
    uint32_t startLine = 0; ///< First line of the range (1-based).
    uint32_t endLine = 0;   ///< Last line of the range (1-based, inclusive).
};

/// @brief Represents a set of changed line ranges within a single file.
struct FileChanges
{
    std::filesystem::path filePath;       ///< Path to the changed file.
    std::vector<LineRange> changedRanges; ///< Changed/added line ranges.
};

/// @brief Result of parsing a git diff: a list of per-file changes.
using DiffResult = std::vector<FileChanges>;

/// @brief Tests whether a changed line range overlaps with a code block's source range.
/// @param changed The changed line range from the diff.
/// @param blockStart Start line of the code block (1-based, inclusive).
/// @param blockEnd End line of the code block (1-based, inclusive).
/// @return True if the ranges overlap.
[[nodiscard]] constexpr auto Overlaps(LineRange const& changed, uint32_t blockStart, uint32_t blockEnd) -> bool
{
    return changed.startLine <= blockEnd && changed.endLine >= blockStart;
}

/// @brief Tests whether any changed range in a file overlaps with a given source line range.
/// @param changes The file's set of changed ranges.
/// @param startLine Start line of the code block (1-based, inclusive).
/// @param endLine End line of the code block (1-based, inclusive).
/// @return True if any changed range overlaps with [startLine, endLine].
[[nodiscard]] inline auto FileHasChangesAt(FileChanges const& changes, uint32_t startLine, uint32_t endLine) -> bool
{
    return std::ranges::any_of(changes.changedRanges,
                               [&](auto const& range) { return Overlaps(range, startLine, endLine); });
}

} // namespace codedup
