// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <filesystem>

namespace codedup
{

/// @brief Represents a position within a source file.
struct SourceLocation
{
    std::filesystem::path filePath; ///< Path to the source file.
    uint32_t line = 1;              ///< 1-based line number.
    uint32_t column = 1;            ///< 1-based column number.
};

/// @brief Represents a range within a source file (start to end inclusive).
struct SourceRange
{
    SourceLocation start; ///< Start of the range.
    SourceLocation end;   ///< End of the range (inclusive).
};

} // namespace codedup
