// SPDX-License-Identifier: Apache-2.0

#include <codedup/HelpFormatter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using namespace codedup;

// ---------------------------------------------------------------------------
// ClassifyLine tests
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.ClassifyLine.Empty", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("", jsonDepth, inContinuation, true) == HelpLineType::Empty);
    CHECK(HelpFormatter::ClassifyLine("   ", jsonDepth, inContinuation, true) == HelpLineType::Empty);
}

TEST_CASE("HelpFormatter.ClassifyLine.TitleUnderline", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("==================================", jsonDepth, inContinuation, true) ==
          HelpLineType::TitleUnderline);
}

TEST_CASE("HelpFormatter.ClassifyLine.SectionUnderline", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("-----------", jsonDepth, inContinuation, true) ==
          HelpLineType::SectionUnderline);
}

TEST_CASE("HelpFormatter.ClassifyLine.SectionHeader", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("Basic Usage", jsonDepth, inContinuation, true) == HelpLineType::SectionHeader);
}

TEST_CASE("HelpFormatter.ClassifyLine.ShellComment", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("  # Scan a directory with default settings", jsonDepth, inContinuation, true) ==
          HelpLineType::ShellComment);
}

TEST_CASE("HelpFormatter.ClassifyLine.ShellCommand", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("  codedupdetector /path/to/project", jsonDepth, inContinuation, true) ==
          HelpLineType::ShellCommand);
}

TEST_CASE("HelpFormatter.ClassifyLine.ShellContinuation", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;

    // First line ends with backslash
    auto type =
        HelpFormatter::ClassifyLine("  codedupdetector --diff-base origin/main \\", jsonDepth, inContinuation, true);
    CHECK(type == HelpLineType::ShellCommand);
    CHECK(inContinuation == true);

    // Next line is a continuation
    type = HelpFormatter::ClassifyLine("      --no-source /path/to/project", jsonDepth, inContinuation, true);
    CHECK(type == HelpLineType::ShellContinuation);
    CHECK(inContinuation == false); // No trailing backslash
}

TEST_CASE("HelpFormatter.ClassifyLine.JsonLine", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;

    // Start of JSON block
    auto type = HelpFormatter::ClassifyLine("  {", jsonDepth, inContinuation, true);
    CHECK(type == HelpLineType::JsonLine);
    CHECK(jsonDepth == 1);

    // Inside JSON block
    type = HelpFormatter::ClassifyLine("    \"mcpServers\": {", jsonDepth, inContinuation, true);
    CHECK(type == HelpLineType::JsonLine);
    CHECK(jsonDepth == 2);

    // Closing brace
    type = HelpFormatter::ClassifyLine("    }", jsonDepth, inContinuation, true);
    CHECK(type == HelpLineType::JsonLine);
    CHECK(jsonDepth == 1);

    // Final closing brace
    type = HelpFormatter::ClassifyLine("  }", jsonDepth, inContinuation, true);
    CHECK(type == HelpLineType::JsonLine);
    CHECK(jsonDepth == 0);
}

TEST_CASE("HelpFormatter.ClassifyLine.UsageLine", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("Usage: codedupdetector [OPTIONS] <directory>", jsonDepth, inContinuation,
                                      false) == HelpLineType::UsageLine);
}

TEST_CASE("HelpFormatter.ClassifyLine.OptionsLabel", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("Options:", jsonDepth, inContinuation, false) == HelpLineType::OptionsLabel);
}

TEST_CASE("HelpFormatter.ClassifyLine.OptionLine", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    CHECK(HelpFormatter::ClassifyLine("  -t, --threshold <N>         Similarity threshold 0.0-1.0 (default: 0.80)",
                                      jsonDepth, inContinuation, false) == HelpLineType::OptionLine);
}

TEST_CASE("HelpFormatter.ClassifyLine.NotExamplesMode", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;
    // In help mode, shell comments are not classified as such
    CHECK(HelpFormatter::ClassifyLine("  # This is some text", jsonDepth, inContinuation, false) ==
          HelpLineType::PlainText);
}

// ---------------------------------------------------------------------------
// FormatHelp / FormatExamples: no-color mode
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.FormatHelp.NoColor", "[help-formatter]")
{
    auto const input = std::string_view("Usage: codedupdetector [OPTIONS]\n\nOptions:\n  -t, --threshold <N>  desc");
    auto const result = HelpFormatter::FormatHelp(input, false, ColorTheme::Dark);
    CHECK(result == input);
}

TEST_CASE("HelpFormatter.FormatExamples.NoColor", "[help-formatter]")
{
    auto const input = std::string_view("Title\n=====\n  # Comment\n  codedupdetector /path");
    auto const result = HelpFormatter::FormatExamples(input, false, ColorTheme::Dark);
    CHECK(result == input);
}

// ---------------------------------------------------------------------------
// FormatHelp / FormatExamples: color mode
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.FormatHelp.Color", "[help-formatter]")
{
    auto const input = std::string_view("Usage: codedupdetector [OPTIONS]\n\nOptions:\n  -t, --threshold <N>  desc");
    auto const result = HelpFormatter::FormatHelp(input, true, ColorTheme::Dark);
    CHECK(result != input);
    CHECK(result.contains("\033[")); // Contains ANSI escape
    CHECK(result.contains("Usage:"));
    CHECK(result.contains("Options:"));
}

TEST_CASE("HelpFormatter.FormatExamples.Color", "[help-formatter]")
{
    auto const input =
        std::string_view("Title\n=====\n\nSection\n-------\n  # Comment\n  codedupdetector /path/to/project");
    auto const result = HelpFormatter::FormatExamples(input, true, ColorTheme::Dark);
    CHECK(result != input);
    CHECK(result.contains("\033["));   // Contains ANSI escape
    CHECK(result.contains("Title"));   // Title text preserved
    CHECK(result.contains("Section")); // Section header preserved
    CHECK(result.contains("Comment")); // Comment text preserved
}

// ---------------------------------------------------------------------------
// HighlightShellLine tests
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.HighlightShellLine.CommandName", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightShellLine("  codedupdetector /path/to/project", colors);
    CHECK(result.contains("\033[1m"));                                               // Bold
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.keywords.r,      //
                                      colors.keywords.g, colors.keywords.b)));       // Keywords color
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.identifiers.r,   //
                                      colors.identifiers.g, colors.identifiers.b))); // Path color
    CHECK(result.contains("/path/to/project"));                                      // Path text
}

TEST_CASE("HelpFormatter.HighlightShellLine.Flags", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightShellLine("  codedupdetector -t 0.95 /path", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.preprocessor.r,    //
                                      colors.preprocessor.g, colors.preprocessor.b))); // Flag color
    CHECK(result.contains("-t"));
}

TEST_CASE("HelpFormatter.HighlightShellLine.Numbers", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightShellLine("  codedupdetector -t 0.95 /path", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.numbers.r, //
                                      colors.numbers.g, colors.numbers.b)));   // Number color
    CHECK(result.contains("0.95"));
}

TEST_CASE("HelpFormatter.HighlightShellLine.QuotedStrings", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightShellLine("  codedupdetector -g '*Controller*' /path", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.strings.r, //
                                      colors.strings.g, colors.strings.b)));   // String color
    CHECK(result.contains("'*Controller*'"));
}

TEST_CASE("HelpFormatter.HighlightShellLine.Continuation", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightShellLine("  codedupdetector --no-color \\", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.operators.r, //
                                      colors.operators.g, colors.operators.b))); // Operator color
    CHECK(result.contains("\\"));
}

// ---------------------------------------------------------------------------
// HighlightJsonLine tests
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.HighlightJsonLine.Key", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightJsonLine("    \"mcpServers\": {", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.keywords.r, //
                                      colors.keywords.g, colors.keywords.b)));  // Key color
    CHECK(result.contains("\"mcpServers\""));
}

TEST_CASE("HelpFormatter.HighlightJsonLine.StringValue", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightJsonLine(R"(        "type": "stdio",)", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.strings.r, //
                                      colors.strings.g, colors.strings.b)));   // String color
    CHECK(result.contains(R"("stdio")"));
}

TEST_CASE("HelpFormatter.HighlightJsonLine.Punctuation", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightJsonLine("  {", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.punctuation.r,   //
                                      colors.punctuation.g, colors.punctuation.b))); // Punctuation color
}

// ---------------------------------------------------------------------------
// HighlightOptionLine tests
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.HighlightOptionLine.Flags", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result =
        HelpFormatter::HighlightOptionLine("  -t, --threshold <N>         Similarity threshold", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.preprocessor.r,    //
                                      colors.preprocessor.g, colors.preprocessor.b))); // Flag color
    CHECK(result.contains("-t"));
    CHECK(result.contains("--threshold"));
}

TEST_CASE("HelpFormatter.HighlightOptionLine.Param", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result =
        HelpFormatter::HighlightOptionLine("  -t, --threshold <N>         Similarity threshold", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.strings.r, //
                                      colors.strings.g, colors.strings.b)));   // Param color
    CHECK(result.contains("<N>"));
}

TEST_CASE("HelpFormatter.HighlightOptionLine.DefaultValue", "[help-formatter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    auto const result = HelpFormatter::HighlightOptionLine(
        "  -t, --threshold <N>         Similarity threshold 0.0-1.0 (default: 0.80)", colors);
    CHECK(result.contains(std::format("\033[38;2;{};{};{}m", colors.numbers.r, //
                                      colors.numbers.g, colors.numbers.b)));   // Number color for default
    CHECK(result.contains("0.80"));
}

// ---------------------------------------------------------------------------
// JSON brace depth tracking across lines
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.JsonDepthTracking", "[help-formatter]")
{
    int jsonDepth = 0;
    bool inContinuation = false;

    // Open outer brace
    (void)HelpFormatter::ClassifyLine("  {", jsonDepth, inContinuation, true);
    CHECK(jsonDepth == 1);

    // Open inner brace
    (void)HelpFormatter::ClassifyLine("    \"servers\": {", jsonDepth, inContinuation, true);
    CHECK(jsonDepth == 2);

    // Open innermost brace
    (void)HelpFormatter::ClassifyLine("      \"tool\": {", jsonDepth, inContinuation, true);
    CHECK(jsonDepth == 3);

    // Close innermost
    (void)HelpFormatter::ClassifyLine("      }", jsonDepth, inContinuation, true);
    CHECK(jsonDepth == 2);

    // Close inner
    (void)HelpFormatter::ClassifyLine("    }", jsonDepth, inContinuation, true);
    CHECK(jsonDepth == 1);

    // Close outer
    (void)HelpFormatter::ClassifyLine("  }", jsonDepth, inContinuation, true);
    CHECK(jsonDepth == 0);
}

// ---------------------------------------------------------------------------
// Light theme produces different colors
// ---------------------------------------------------------------------------

TEST_CASE("HelpFormatter.LightTheme", "[help-formatter]")
{
    auto const darkResult = HelpFormatter::FormatExamples("  # Comment", true, ColorTheme::Dark);
    auto const lightResult = HelpFormatter::FormatExamples("  # Comment", true, ColorTheme::Light);
    CHECK(darkResult != lightResult);
}
