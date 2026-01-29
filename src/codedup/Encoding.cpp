// SPDX-License-Identifier: Apache-2.0
#include <codedup/Encoding.hpp>

#include <algorithm>
#include <array>
#include <format>

namespace codedup
{

namespace
{

/// @brief Unicode code points for Windows-1252 bytes 0x80–0x9F.
///
/// Undefined positions (0x81, 0x8D, 0x8F, 0x90, 0x9D) map to U+FFFD (replacement character).
// clang-format off
constexpr auto windows1252Table = std::to_array<char32_t>({
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 0x80-0x87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD, // 0x88-0x8F
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 0x90-0x97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178, // 0x98-0x9F
});
// clang-format on

/// @brief Appends a Unicode code point as 1–4 UTF-8 bytes.
/// @param out The string to append to.
/// @param cp The Unicode code point to encode.
void AppendUtf8(std::string& out, char32_t cp)
{
    if (cp <= 0x7F)
    {
        out += static_cast<char>(cp);
    }
    else if (cp <= 0x7FF)
    {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF)
    {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0x10FFFF)
    {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// @brief Checks whether the data starts with a UTF-8 BOM (0xEF 0xBB 0xBF).
/// @param data The byte sequence to check.
/// @return True if a UTF-8 BOM is present.
[[nodiscard]] auto HasUtf8Bom(std::string_view data) -> bool
{
    return data.size() >= 3 && static_cast<uint8_t>(data[0]) == 0xEF && static_cast<uint8_t>(data[1]) == 0xBB &&
           static_cast<uint8_t>(data[2]) == 0xBF;
}

/// @brief Validates whether the data is strictly valid UTF-8.
///
/// Rejects overlong encodings, surrogate code points (U+D800–U+DFFF),
/// and code points above U+10FFFF.
/// @param data The byte sequence to validate.
/// @return True if the data is valid UTF-8.
[[nodiscard]] auto IsValidUtf8(std::string_view data) -> bool
{
    auto const size = data.size();
    size_t i = 0;
    while (i < size)
    {
        auto const b0 = static_cast<uint8_t>(data[i]);
        if (b0 <= 0x7F)
        {
            ++i;
            continue;
        }

        size_t seqLen = 0;
        char32_t cp = 0;
        char32_t minCp = 0;

        if ((b0 & 0xE0) == 0xC0)
        {
            seqLen = 2;
            cp = b0 & 0x1F;
            minCp = 0x80;
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            seqLen = 3;
            cp = b0 & 0x0F;
            minCp = 0x800;
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            seqLen = 4;
            cp = b0 & 0x07;
            minCp = 0x10000;
        }
        else
        {
            return false; // Invalid lead byte
        }

        if (i + seqLen > size)
            return false; // Truncated sequence

        for (size_t j = 1; j < seqLen; ++j)
        {
            auto const bj = static_cast<uint8_t>(data[i + j]);
            if ((bj & 0xC0) != 0x80)
                return false; // Invalid continuation byte
            cp = (cp << 6) | (bj & 0x3F);
        }

        // Reject overlong encodings
        if (cp < minCp)
            return false;

        // Reject surrogates (U+D800–U+DFFF)
        if (cp >= 0xD800 && cp <= 0xDFFF)
            return false;

        // Reject code points above U+10FFFF
        if (cp > 0x10FFFF)
            return false;

        i += seqLen;
    }
    return true;
}

/// @brief Converts Windows-1252 encoded bytes to UTF-8.
///
/// ASCII bytes (0x00–0x7F) pass through unchanged.
/// Bytes 0x80–0x9F are mapped via the Windows-1252 table to their Unicode equivalents.
/// Bytes 0xA0–0xFF map to U+00A0–U+00FF (Latin-1 Supplement).
/// @param data The Windows-1252 byte sequence.
/// @return The UTF-8 converted string.
[[nodiscard]] auto ConvertWindows1252ToUtf8(std::string_view data) -> std::string
{
    std::string result;
    result.reserve(data.size() + data.size() / 4); // Slight overallocation for multi-byte sequences

    for (auto const ch : data)
    {
        auto const byte = static_cast<uint8_t>(ch);
        if (byte <= 0x7F)
        {
            result += ch;
        }
        else if (byte >= 0x80 && byte <= 0x9F)
        {
            AppendUtf8(result, windows1252Table[byte - 0x80]);
        }
        else
        {
            // 0xA0–0xFF map directly to U+00A0–U+00FF
            AppendUtf8(result, static_cast<char32_t>(byte));
        }
    }

    return result;
}

/// @brief Converts a character to lowercase (ASCII only).
/// @param ch The character to convert.
/// @return The lowercase version of the character.
[[nodiscard]] constexpr auto ToLowerAscii(char ch) -> char
{
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<char>(ch + ('a' - 'A'));
    return ch;
}

/// @brief Case-insensitive string comparison for ASCII strings.
/// @param a First string.
/// @param b Second string.
/// @return True if the strings are equal ignoring case.
[[nodiscard]] auto EqualsIgnoreCase(std::string_view a, std::string_view b) -> bool
{
    if (a.size() != b.size())
        return false;
    return std::ranges::equal(a, b, [](char ca, char cb) { return ToLowerAscii(ca) == ToLowerAscii(cb); });
}

} // namespace

auto DetectEncoding(std::string_view data) -> InputEncoding
{
    if (HasUtf8Bom(data))
        return InputEncoding::Utf8;

    if (IsValidUtf8(data))
        return InputEncoding::Utf8;

    return InputEncoding::Windows1252;
}

auto ConvertToUtf8(std::string_view data, InputEncoding encoding) -> std::expected<std::string, EncodingError>
{
    auto const resolved = (encoding == InputEncoding::Auto) ? DetectEncoding(data) : encoding;

    switch (resolved)
    {
        case InputEncoding::Utf8:
        {
            // Strip BOM if present
            auto const start = HasUtf8Bom(data) ? size_t{3} : size_t{0};
            return std::string(data.substr(start));
        }
        case InputEncoding::Windows1252:
            return ConvertWindows1252ToUtf8(data);
        case InputEncoding::Auto:
            break; // Unreachable after resolution
    }

    return std::unexpected(EncodingError{.message = "Unexpected encoding value"});
}

auto ParseEncodingName(std::string_view name) -> std::expected<InputEncoding, EncodingError>
{
    if (EqualsIgnoreCase(name, "auto"))
        return InputEncoding::Auto;
    if (EqualsIgnoreCase(name, "utf8") || EqualsIgnoreCase(name, "utf-8"))
        return InputEncoding::Utf8;
    if (EqualsIgnoreCase(name, "windows-1252") || EqualsIgnoreCase(name, "windows1252") ||
        EqualsIgnoreCase(name, "cp1252"))
        return InputEncoding::Windows1252;

    return std::unexpected(EncodingError{.message = std::format("Unknown encoding: {}", name)});
}

} // namespace codedup
