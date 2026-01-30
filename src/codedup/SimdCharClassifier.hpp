// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#if __has_include(<experimental/simd>) && defined(__SSE2__)
#define CODEDUP_HAS_SIMD 1
#include <experimental/simd>
#else
#define CODEDUP_HAS_SIMD 0
#endif

namespace codedup
{

/// @brief SIMD-accelerated character classification for tokenizer hot paths.
///
/// Provides batch character scanning using native SIMD vectors. Each function
/// scans forward from a given position and returns the number of consecutive
/// characters matching a particular character class.
///
/// Uses std::experimental::native_simd<uint8_t> for optimal width selection:
/// 16 elements on SSE2, 32 on AVX2, 64 on AVX-512 with -march=native.
///
/// Gracefully falls back to scalar loops when SIMD is not available.
class SimdCharClassifier
{
public:
    /// @brief Scans forward counting consecutive identifier-continue characters [a-zA-Z0-9_].
    /// @param source The source string view.
    /// @param pos Starting position in the source.
    /// @return Number of consecutive identifier-continue characters from pos.
    [[nodiscard]] static auto ScanIdentifierContinue(std::string_view source, size_t pos) noexcept -> size_t;

    /// @brief Scans forward counting consecutive hex digit characters [0-9a-fA-F] or digit separator.
    /// @param source The source string view.
    /// @param pos Starting position in the source.
    /// @param separator Digit separator character ('\'' for C++, '_' for C#/Python).
    /// @return Number of consecutive hex digit or separator characters from pos.
    [[nodiscard]] static auto ScanHexDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t;

    /// @brief Scans forward counting consecutive binary digit characters [01] or digit separator.
    /// @param source The source string view.
    /// @param pos Starting position in the source.
    /// @param separator Digit separator character ('\'' for C++, '_' for C#/Python).
    /// @return Number of consecutive binary digit or separator characters from pos.
    [[nodiscard]] static auto ScanBinaryDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t;

    /// @brief Scans forward counting consecutive octal digit characters [0-7] or digit separator.
    /// @param source The source string view.
    /// @param pos Starting position in the source.
    /// @param separator Digit separator character ('\'' for C++, '_' for C#/Python).
    /// @return Number of consecutive octal digit or separator characters from pos.
    [[nodiscard]] static auto ScanOctalDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t;

    /// @brief Scans forward counting consecutive whitespace characters [ \t\r\n].
    /// @param source The source string view.
    /// @param pos Starting position in the source.
    /// @return Number of consecutive whitespace characters from pos.
    [[nodiscard]] static auto ScanWhitespace(std::string_view source, size_t pos) noexcept -> size_t;

    /// @brief Scans forward counting consecutive decimal digit characters [0-9] or digit separator.
    /// @param source The source string view.
    /// @param pos Starting position in the source.
    /// @param separator Digit separator character ('\'' for C++, '_' for C#/Python).
    /// @return Number of consecutive decimal digit or separator characters from pos.
    [[nodiscard]] static auto ScanDecimalDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t;

private:
#if CODEDUP_HAS_SIMD
    using SimdU8 = std::experimental::native_simd<uint8_t>;
    static constexpr auto kSimdWidth = SimdU8::size();

    /// @brief Internal SIMD scan loop: finds the first non-matching character using a classification predicate.
    ///
    /// @tparam Classify A callable taking SimdU8 and returning a SIMD mask of matching elements.
    /// @param data Pointer to the byte data.
    /// @param length Number of bytes to scan.
    /// @return Number of consecutive matching characters from the start.
    template <typename Classify>
    [[nodiscard]] static auto SimdScan(char const* data, size_t length, Classify classify) noexcept -> size_t;
#endif
};

// ============================================================================
// Inline implementation
// ============================================================================

#if CODEDUP_HAS_SIMD

namespace stdx = std::experimental;

template <typename Classify>
auto SimdCharClassifier::SimdScan(char const* data, size_t length, Classify classify) noexcept -> size_t
{
    size_t offset = 0;

    // SIMD loop: classify kSimdWidth characters per iteration
    while (offset + kSimdWidth <= length)
    {
        // Load raw bytes and reinterpret as uint8_t
        SimdU8 const chars([&](auto i) { return static_cast<uint8_t>(data[offset + i]); });
        auto const valid = classify(chars);
        if (!stdx::all_of(valid))
        {
            // Found a non-matching character; find the exact position
            return offset + static_cast<size_t>(stdx::find_first_set(!valid));
        }
        offset += kSimdWidth;
    }

    return offset; // Return where SIMD stopped; caller handles scalar tail
}

#endif // CODEDUP_HAS_SIMD

inline auto SimdCharClassifier::ScanIdentifierContinue(std::string_view source, size_t pos) noexcept -> size_t
{
    if (pos >= source.size())
        return 0;

    auto const* data = source.data() + pos;
    auto const length = source.size() - pos;

    auto const scalarCheck = [](char ch) -> bool
    { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'; };

    size_t offset = 0;

#if CODEDUP_HAS_SIMD
    offset = SimdScan(data, length,
                      [](SimdU8 const& chars) -> decltype(chars == chars)
                      {
                          auto const u = chars;
                          return (u >= uint8_t('a') && u <= uint8_t('z')) || (u >= uint8_t('A') && u <= uint8_t('Z')) ||
                                 (u >= uint8_t('0') && u <= uint8_t('9')) || u == uint8_t('_');
                      });
#endif

    // Scalar tail
    while (offset < length && scalarCheck(data[offset]))
        ++offset;

    return offset;
}

inline auto SimdCharClassifier::ScanHexDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t
{
    if (pos >= source.size())
        return 0;

    auto const* data = source.data() + pos;
    auto const length = source.size() - pos;
    auto const scalarCheck = [separator](char ch) -> bool
    { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || ch == separator; };

    size_t offset = 0;

#if CODEDUP_HAS_SIMD
    auto const sep = static_cast<uint8_t>(separator);
    offset = SimdScan(data, length,
                      [sep](SimdU8 const& chars) -> decltype(chars == chars)
                      {
                          auto const u = chars;
                          return (u >= uint8_t('0') && u <= uint8_t('9')) || (u >= uint8_t('a') && u <= uint8_t('f')) ||
                                 (u >= uint8_t('A') && u <= uint8_t('F')) || u == sep;
                      });
#endif

    while (offset < length && scalarCheck(data[offset]))
        ++offset;

    return offset;
}

inline auto SimdCharClassifier::ScanBinaryDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t
{
    if (pos >= source.size())
        return 0;

    auto const* data = source.data() + pos;
    auto const length = source.size() - pos;
    auto const scalarCheck = [separator](char ch) -> bool { return ch == '0' || ch == '1' || ch == separator; };

    size_t offset = 0;

#if CODEDUP_HAS_SIMD
    auto const sep = static_cast<uint8_t>(separator);
    offset = SimdScan(data, length,
                      [sep](SimdU8 const& chars) -> decltype(chars == chars)
                      {
                          auto const u = chars;
                          return u == uint8_t('0') || u == uint8_t('1') || u == sep;
                      });
#endif

    while (offset < length && scalarCheck(data[offset]))
        ++offset;

    return offset;
}

inline auto SimdCharClassifier::ScanOctalDigits(std::string_view source, size_t pos, char separator) noexcept -> size_t
{
    if (pos >= source.size())
        return 0;

    auto const* data = source.data() + pos;
    auto const length = source.size() - pos;
    auto const scalarCheck = [separator](char ch) -> bool { return (ch >= '0' && ch <= '7') || ch == separator; };

    size_t offset = 0;

#if CODEDUP_HAS_SIMD
    auto const sep = static_cast<uint8_t>(separator);
    offset = SimdScan(data, length,
                      [sep](SimdU8 const& chars) -> decltype(chars == chars)
                      {
                          auto const u = chars;
                          return (u >= uint8_t('0') && u <= uint8_t('7')) || u == sep;
                      });
#endif

    while (offset < length && scalarCheck(data[offset]))
        ++offset;

    return offset;
}

inline auto SimdCharClassifier::ScanWhitespace(std::string_view source, size_t pos) noexcept -> size_t
{
    if (pos >= source.size())
        return 0;

    auto const* data = source.data() + pos;
    auto const length = source.size() - pos;

    auto const scalarCheck = [](char ch) -> bool { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };

    size_t offset = 0;

#if CODEDUP_HAS_SIMD
    offset = SimdScan(data, length,
                      [](SimdU8 const& chars) -> decltype(chars == chars)
                      {
                          auto const u = chars;
                          return u == uint8_t(' ') || u == uint8_t('\t') || u == uint8_t('\r') || u == uint8_t('\n');
                      });
#endif

    while (offset < length && scalarCheck(data[offset]))
        ++offset;

    return offset;
}

inline auto SimdCharClassifier::ScanDecimalDigits(std::string_view source, size_t pos, char separator) noexcept
    -> size_t
{
    if (pos >= source.size())
        return 0;

    auto const* data = source.data() + pos;
    auto const length = source.size() - pos;
    auto const scalarCheck = [separator](char ch) -> bool { return (ch >= '0' && ch <= '9') || ch == separator; };

    size_t offset = 0;

#if CODEDUP_HAS_SIMD
    auto const sep = static_cast<uint8_t>(separator);
    offset = SimdScan(data, length,
                      [sep](SimdU8 const& chars) -> decltype(chars == chars)
                      {
                          auto const u = chars;
                          return (u >= uint8_t('0') && u <= uint8_t('9')) || u == sep;
                      });
#endif

    while (offset < length && scalarCheck(data[offset]))
        ++offset;

    return offset;
}

} // namespace codedup
