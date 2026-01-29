// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace codedup
{

/// @brief Supported input file encodings.
enum class InputEncoding : uint8_t
{
    Auto,        ///< Auto-detect encoding (BOM check, UTF-8 validation, fallback to Windows-1252).
    Utf8,        ///< UTF-8 encoding.
    Windows1252, ///< Windows-1252 (CP1252) encoding.
};

/// @brief Error information from encoding operations.
struct EncodingError
{
    std::string message; ///< Description of the error.
};

/// @brief Detects the encoding of a byte sequence.
///
/// Checks for UTF-8 BOM, validates UTF-8, and falls back to Windows-1252.
/// @param data The raw byte data to inspect.
/// @return The detected encoding (never returns Auto).
[[nodiscard]] CODEDUP_API auto detectEncoding(std::string_view data) -> InputEncoding;

/// @brief Converts raw bytes to UTF-8 using the specified encoding.
///
/// When encoding is Auto, detects the encoding first. Strips UTF-8 BOM if present.
/// @param data The raw byte data to convert.
/// @param encoding The encoding to use (or Auto for detection).
/// @return The UTF-8 converted string, or an error.
[[nodiscard]] CODEDUP_API auto convertToUtf8(std::string_view data, InputEncoding encoding)
    -> std::expected<std::string, EncodingError>;

/// @brief Parses an encoding name string into an InputEncoding value.
///
/// Accepts (case-insensitive): "auto", "utf8", "utf-8", "windows-1252", "windows1252", "cp1252".
/// @param name The encoding name to parse.
/// @return The parsed encoding value, or an error for unrecognized names.
[[nodiscard]] CODEDUP_API auto parseEncodingName(std::string_view name) -> std::expected<InputEncoding, EncodingError>;

} // namespace codedup
