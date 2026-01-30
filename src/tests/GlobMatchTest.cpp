// SPDX-License-Identifier: Apache-2.0
#include <codedup/GlobMatch.hpp>

#include <catch2/catch_test_macros.hpp>

using codedup::GlobMatch;

TEST_CASE("GlobMatch.ExactMatch", "[GlobMatch]")
{
    CHECK(GlobMatch("hello", "hello"));
    CHECK_FALSE(GlobMatch("hello", "world"));
    CHECK_FALSE(GlobMatch("hello", "Hello"));
    CHECK_FALSE(GlobMatch("hello", "hello2"));
    CHECK_FALSE(GlobMatch("hello2", "hello"));
}

TEST_CASE("GlobMatch.EmptyPatternAndText", "[GlobMatch]")
{
    CHECK(GlobMatch("", ""));
    CHECK_FALSE(GlobMatch("", "a"));
    CHECK_FALSE(GlobMatch("a", ""));
}

TEST_CASE("GlobMatch.QuestionMark", "[GlobMatch]")
{
    CHECK(GlobMatch("?", "a"));
    CHECK(GlobMatch("?", "z"));
    CHECK_FALSE(GlobMatch("?", ""));
    CHECK_FALSE(GlobMatch("?", "ab"));
    CHECK(GlobMatch("??", "ab"));
    CHECK_FALSE(GlobMatch("??", "a"));
    CHECK(GlobMatch("h?llo", "hello"));
    CHECK(GlobMatch("h?llo", "hallo"));
    CHECK_FALSE(GlobMatch("h?llo", "hllo"));
}

TEST_CASE("GlobMatch.StarMatchesZeroOrMore", "[GlobMatch]")
{
    CHECK(GlobMatch("*", ""));
    CHECK(GlobMatch("*", "anything"));
    CHECK(GlobMatch("*", "a long string with spaces"));
    CHECK(GlobMatch("hello*", "hello"));
    CHECK(GlobMatch("hello*", "helloworld"));
    CHECK(GlobMatch("*world", "world"));
    CHECK(GlobMatch("*world", "helloworld"));
    CHECK_FALSE(GlobMatch("*world", "worldx"));
}

TEST_CASE("GlobMatch.StarInMiddle", "[GlobMatch]")
{
    CHECK(GlobMatch("h*o", "ho"));
    CHECK(GlobMatch("h*o", "hello"));
    CHECK(GlobMatch("h*o", "heyo"));
    CHECK_FALSE(GlobMatch("h*o", "hey"));
    CHECK(GlobMatch("*.cpp", "main.cpp"));
    CHECK(GlobMatch("*.cpp", ".cpp"));
    CHECK_FALSE(GlobMatch("*.cpp", "main.hpp"));
}

TEST_CASE("GlobMatch.MultipleStars", "[GlobMatch]")
{
    CHECK(GlobMatch("**", ""));
    CHECK(GlobMatch("**", "anything"));
    CHECK(GlobMatch("*.*", "file.txt"));
    CHECK(GlobMatch("*.*", "a.b"));
    CHECK_FALSE(GlobMatch("*.*", "nodot"));
    CHECK(GlobMatch("*Bit*", "BitProbe.cpp"));
    CHECK(GlobMatch("*Bit*", "DlgBitWpkZus.cpp"));
    CHECK_FALSE(GlobMatch("*Bit*", "MainWindow.cpp"));
}

TEST_CASE("GlobMatch.StarAndQuestionMixed", "[GlobMatch]")
{
    CHECK(GlobMatch("*?", "a"));
    CHECK(GlobMatch("*?", "abc"));
    CHECK_FALSE(GlobMatch("*?", ""));
    CHECK(GlobMatch("?*", "a"));
    CHECK(GlobMatch("?*", "abc"));
    CHECK_FALSE(GlobMatch("?*", ""));
    CHECK(GlobMatch("?*?", "ab"));
    CHECK(GlobMatch("?*?", "abc"));
    CHECK_FALSE(GlobMatch("?*?", "a"));
}

TEST_CASE("GlobMatch.ConsecutiveStars", "[GlobMatch]")
{
    CHECK(GlobMatch("***", ""));
    CHECK(GlobMatch("***", "abc"));
    CHECK(GlobMatch("a***b", "ab"));
    CHECK(GlobMatch("a***b", "aXYZb"));
}

TEST_CASE("GlobMatch.TrailingStars", "[GlobMatch]")
{
    CHECK(GlobMatch("abc*", "abc"));
    CHECK(GlobMatch("abc**", "abc"));
    CHECK(GlobMatch("abc***", "abcdef"));
}

TEST_CASE("GlobMatch.PatternLongerThanText", "[GlobMatch]")
{
    CHECK_FALSE(GlobMatch("abcdef", "abc"));
    CHECK_FALSE(GlobMatch("???", "ab"));
    CHECK(GlobMatch("a*f", "af"));
    CHECK_FALSE(GlobMatch("a?f", "af"));
}

TEST_CASE("GlobMatch.SpecialCharactersLiteral", "[GlobMatch]")
{
    CHECK(GlobMatch("file.txt", "file.txt"));
    CHECK(GlobMatch("path/to/file", "path/to/file"));
    CHECK(GlobMatch("hello world", "hello world"));
    CHECK(GlobMatch("a-b_c", "a-b_c"));
    CHECK(GlobMatch("[not-a-class]", "[not-a-class]"));
}

TEST_CASE("GlobMatch.TypicalFilePatterns", "[GlobMatch]")
{
    // Extension matching
    CHECK(GlobMatch("*.cpp", "main.cpp"));
    CHECK(GlobMatch("*.cpp", "foo.bar.cpp"));
    CHECK_FALSE(GlobMatch("*.cpp", "main.hpp"));
    CHECK_FALSE(GlobMatch("*.cpp", "cpp"));

    // Prefix matching
    CHECK(GlobMatch("test_*", "test_main"));
    CHECK(GlobMatch("test_*", "test_"));
    CHECK_FALSE(GlobMatch("test_*", "test"));

    // Contains matching
    CHECK(GlobMatch("*test*", "my_test_file"));
    CHECK(GlobMatch("*test*", "test"));
    CHECK(GlobMatch("*test*", "testing"));
    CHECK_FALSE(GlobMatch("*test*", "tset"));
}

TEST_CASE("GlobMatch.Constexpr", "[GlobMatch]")
{
    // Verify the function is usable in constexpr context
    static_assert(GlobMatch("*.cpp", "main.cpp"));
    static_assert(!GlobMatch("*.hpp", "main.cpp"));
    static_assert(GlobMatch("?", "x"));
    static_assert(GlobMatch("", ""));
    static_assert(!GlobMatch("a", ""));
}
