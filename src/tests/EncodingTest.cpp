// SPDX-License-Identifier: Apache-2.0
#include <codedup/Encoding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using namespace codedup;

// ---- Detection tests ----

TEST_CASE("Encoding.DetectPureAscii", "[encoding]")
{
    auto const result = detectEncoding("Hello, world!\n");
    CHECK(result == InputEncoding::Utf8);
}

TEST_CASE("Encoding.DetectEmpty", "[encoding]")
{
    auto const result = detectEncoding("");
    CHECK(result == InputEncoding::Utf8);
}

TEST_CASE("Encoding.DetectUtf8Bom", "[encoding]")
{
    auto const input = std::string("\xEF\xBB\xBF"
                                   "Hello");
    auto const result = detectEncoding(input);
    CHECK(result == InputEncoding::Utf8);
}

TEST_CASE("Encoding.DetectValidMultiByteUtf8", "[encoding]")
{
    // UTF-8 encoded "Héllo" — é is 0xC3 0xA9
    auto const input = std::string("H\xC3\xA9llo");
    auto const result = detectEncoding(input);
    CHECK(result == InputEncoding::Utf8);
}

TEST_CASE("Encoding.DetectWindows1252Bytes", "[encoding]")
{
    // 0x93 is a left double quotation mark in Windows-1252, invalid as UTF-8 lead byte
    auto const input = std::string("Hello \x93world\x94");
    auto const result = detectEncoding(input);
    CHECK(result == InputEncoding::Windows1252);
}

TEST_CASE("Encoding.Detect0x80To0x9FRange", "[encoding]")
{
    // Byte 0x80 (Euro sign in Windows-1252) is invalid UTF-8
    auto const input = std::string("Price: \x80"
                                   "100");
    auto const result = detectEncoding(input);
    CHECK(result == InputEncoding::Windows1252);
}

// ---- Conversion tests ----

TEST_CASE("Encoding.ConvertUtf8Passthrough", "[encoding]")
{
    auto const input = std::string("Hello, UTF-8 world!\n");
    auto const result = convertToUtf8(input, InputEncoding::Utf8);
    REQUIRE(result.has_value());
    CHECK(*result == input);
}

TEST_CASE("Encoding.ConvertUtf8BomStripping", "[encoding]")
{
    auto const input = std::string("\xEF\xBB\xBF"
                                   "Hello");
    auto const result = convertToUtf8(input, InputEncoding::Utf8);
    REQUIRE(result.has_value());
    CHECK(*result == "Hello");
}

TEST_CASE("Encoding.ConvertUtf8BomStrippingAuto", "[encoding]")
{
    auto const input = std::string("\xEF\xBB\xBF"
                                   "Hello");
    auto const result = convertToUtf8(input, InputEncoding::Auto);
    REQUIRE(result.has_value());
    CHECK(*result == "Hello");
}

TEST_CASE("Encoding.ConvertWindows1252AsciiPassthrough", "[encoding]")
{
    auto const input = std::string("Hello, world!");
    auto const result = convertToUtf8(input, InputEncoding::Windows1252);
    REQUIRE(result.has_value());
    CHECK(*result == "Hello, world!");
}

TEST_CASE("Encoding.ConvertWindows1252LatinSupplement", "[encoding]")
{
    // 0xA9 = copyright sign (U+00A9), 0xE9 = é (U+00E9)
    auto const input = std::string("\xA9\xE9");
    auto const result = convertToUtf8(input, InputEncoding::Windows1252);
    REQUIRE(result.has_value());
    // U+00A9 = 0xC2 0xA9, U+00E9 = 0xC3 0xA9
    CHECK(*result == std::string("\xC2\xA9\xC3\xA9"));
}

TEST_CASE("Encoding.ConvertWindows1252EuroSign", "[encoding]")
{
    // 0x80 in Windows-1252 = Euro sign (U+20AC)
    auto const input = std::string("\x80");
    auto const result = convertToUtf8(input, InputEncoding::Windows1252);
    REQUIRE(result.has_value());
    // U+20AC = 0xE2 0x82 0xAC
    CHECK(*result == std::string("\xE2\x82\xAC"));
}

TEST_CASE("Encoding.ConvertWindows1252UndefinedBytes", "[encoding]")
{
    // 0x81 is undefined in Windows-1252 → maps to U+FFFD
    auto const input = std::string("\x81");
    auto const result = convertToUtf8(input, InputEncoding::Windows1252);
    REQUIRE(result.has_value());
    // U+FFFD = 0xEF 0xBF 0xBD
    CHECK(*result == std::string("\xEF\xBF\xBD"));
}

TEST_CASE("Encoding.ConvertWindows1252FullRangeValidation", "[encoding]")
{
    // Convert all bytes 0x80–0xFF and verify result is valid UTF-8
    std::string input;
    for (int i = 0x80; i <= 0xFF; ++i)
        input += static_cast<char>(i);

    auto const result = convertToUtf8(input, InputEncoding::Windows1252);
    REQUIRE(result.has_value());

    // The result should be non-empty and longer than input (multi-byte encodings)
    CHECK(!result->empty());
    CHECK(result->size() > input.size());

    // Re-detect the output as UTF-8
    CHECK(detectEncoding(*result) == InputEncoding::Utf8);
}

TEST_CASE("Encoding.ConvertAutoDetectsWindows1252", "[encoding]")
{
    // 0x93/0x94 = smart quotes in Windows-1252, invalid UTF-8
    auto const input = std::string("\x93Hello\x94");
    auto const result = convertToUtf8(input, InputEncoding::Auto);
    REQUIRE(result.has_value());
    // Should have converted smart quotes to UTF-8
    CHECK(detectEncoding(*result) == InputEncoding::Utf8);
}

// ---- parseEncodingName tests ----

TEST_CASE("Encoding.ParseEncodingNameAuto", "[encoding]")
{
    auto const result = parseEncodingName("auto");
    REQUIRE(result.has_value());
    CHECK(*result == InputEncoding::Auto);
}

TEST_CASE("Encoding.ParseEncodingNameUtf8Variants", "[encoding]")
{
    CHECK(parseEncodingName("utf8").value() == InputEncoding::Utf8);
    CHECK(parseEncodingName("utf-8").value() == InputEncoding::Utf8);
    CHECK(parseEncodingName("UTF8").value() == InputEncoding::Utf8);
    CHECK(parseEncodingName("UTF-8").value() == InputEncoding::Utf8);
}

TEST_CASE("Encoding.ParseEncodingNameWindows1252Variants", "[encoding]")
{
    CHECK(parseEncodingName("windows-1252").value() == InputEncoding::Windows1252);
    CHECK(parseEncodingName("windows1252").value() == InputEncoding::Windows1252);
    CHECK(parseEncodingName("cp1252").value() == InputEncoding::Windows1252);
    CHECK(parseEncodingName("WINDOWS-1252").value() == InputEncoding::Windows1252);
    CHECK(parseEncodingName("CP1252").value() == InputEncoding::Windows1252);
}

TEST_CASE("Encoding.ParseEncodingNameUnknown", "[encoding]")
{
    auto const result = parseEncodingName("latin-1");
    REQUIRE(!result.has_value());
    CHECK(result.error().message.find("Unknown encoding") != std::string::npos);
}
