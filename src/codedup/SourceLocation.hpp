// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <limits>

namespace codedup
{

/// @brief Sentinel value indicating no file index has been assigned.
inline constexpr uint32_t NoFileIndex = std::numeric_limits<uint32_t>::max();

/// @brief Represents a position within a source file.
///
/// Uses a compact uint32_t file index instead of a full filesystem path to reduce
/// per-token memory overhead. The file index references into the file path vector
/// maintained by the analysis pipeline. For tokenizer errors (before a file index
/// is assigned), NoFileIndex is used and the path is stored in the error struct.
struct SourceLocation
{
    uint32_t fileIndex = NoFileIndex; ///< Index into the global file path vector.
    uint32_t line = 1;                ///< 1-based line number.
    uint32_t column = 1;              ///< 1-based column number.
};

/// @brief Represents a range within a source file (start to end inclusive).
struct SourceRange
{
    SourceLocation start; ///< Start of the range.
    SourceLocation end;   ///< End of the range (inclusive).
};

} // namespace codedup
