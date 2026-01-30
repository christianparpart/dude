// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/SyntaxHighlighter.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace dude
{

/// @brief Classification of a single line in help or example output.
enum class HelpLineType : uint8_t
{
    Empty,             ///< Blank line.
    Title,             ///< Top-level title (e.g. "Usage Examples for ...").
    TitleUnderline,    ///< Title underline (===...).
    SectionHeader,     ///< Section header (e.g. "Basic Usage").
    SectionUnderline,  ///< Section underline (---...).
    UsageLine,         ///< Usage synopsis line (e.g. "Usage: dude ...").
    OptionsLabel,      ///< The "Options:" label line.
    OptionLine,        ///< An option definition line (e.g. "  -t, --threshold <N>  ...").
    ShellComment,      ///< Shell comment line (leading #).
    ShellCommand,      ///< Shell command line.
    ShellContinuation, ///< Continuation of a shell command (after backslash).
    JsonLine,          ///< Line inside a JSON block.
    PlainText,         ///< Any other text.
};

/// @brief Formats --help and --show-examples output with optional ANSI color highlighting.
///
/// Processes plain-text help strings at runtime, classifying each line and applying
/// syntax highlighting using VT escape sequences. Reuses ThemeColors from SyntaxHighlighter.
class HelpFormatter
{
public:
    /// @brief Formats --help output text with optional color.
    /// @param text The raw help text.
    /// @param useColor Whether to apply ANSI color codes.
    /// @param theme The color theme to use.
    /// @return The formatted text (with or without ANSI escapes).
    [[nodiscard]] static auto FormatHelp(std::string_view text, bool useColor, ColorTheme theme) -> std::string;

    /// @brief Formats --show-examples output text with optional color.
    /// @param text The raw examples text.
    /// @param useColor Whether to apply ANSI color codes.
    /// @param theme The color theme to use.
    /// @return The formatted text (with or without ANSI escapes).
    [[nodiscard]] static auto FormatExamples(std::string_view text, bool useColor, ColorTheme theme) -> std::string;

    /// @brief Classifies a single line of help/example output.
    /// @param line The line text (without trailing newline).
    /// @param jsonDepth Current JSON brace nesting depth (mutated on { and }).
    /// @param inContinuation Whether the previous line ended with backslash continuation.
    /// @param isExamplesMode Whether we are in examples mode (enables shell/JSON classification).
    /// @return The classified line type.
    [[nodiscard]] static auto ClassifyLine(std::string_view line, int& jsonDepth, bool& inContinuation,
                                           bool isExamplesMode) -> HelpLineType;

    /// @brief Applies syntax highlighting to a shell command line.
    /// @param line The shell command line text.
    /// @param colors The theme color palette.
    /// @return The highlighted line with ANSI escape sequences.
    [[nodiscard]] static auto HighlightShellLine(std::string_view line, ThemeColors const& colors) -> std::string;

    /// @brief Applies syntax highlighting to a JSON line.
    /// @param line The JSON line text.
    /// @param colors The theme color palette.
    /// @return The highlighted line with ANSI escape sequences.
    [[nodiscard]] static auto HighlightJsonLine(std::string_view line, ThemeColors const& colors) -> std::string;

    /// @brief Applies syntax highlighting to an option definition line.
    /// @param line The option line text (e.g. "  -t, --threshold <N>  Description").
    /// @param colors The theme color palette.
    /// @return The highlighted line with ANSI escape sequences.
    [[nodiscard]] static auto HighlightOptionLine(std::string_view line, ThemeColors const& colors) -> std::string;
};

} // namespace dude
