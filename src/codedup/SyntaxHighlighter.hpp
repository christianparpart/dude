// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Token.hpp>

#include <cstdint>
#include <format>
#include <string>

namespace codedup
{

/// @brief Color theme selection for terminal output.
enum class ColorTheme : uint8_t
{
    Dark,  ///< Optimized for dark terminal backgrounds.
    Light, ///< Optimized for light terminal backgrounds.
    Auto,  ///< Auto-detect based on COLORFGBG environment variable.
};

/// @brief RGB color triple.
struct RGB
{
    uint8_t r = 0; ///< Red component (0-255).
    uint8_t g = 0; ///< Green component (0-255).
    uint8_t b = 0; ///< Blue component (0-255).
};

/// @brief Color scheme for syntax highlighting with truecolor (24-bit) ANSI output.
struct ThemeColors
{
    RGB keywords;     ///< Color for C++ keywords.
    RGB identifiers;  ///< Color for identifiers.
    RGB strings;      ///< Color for string literals.
    RGB chars;        ///< Color for character literals.
    RGB numbers;      ///< Color for numeric literals.
    RGB comments;     ///< Color for comments.
    RGB preprocessor; ///< Color for preprocessor directives.
    RGB operators;    ///< Color for operators.
    RGB punctuation;  ///< Color for punctuation (braces, parens, etc.).
    RGB defaultText;  ///< Color for default/unclassified text.
};

/// @brief Returns the color scheme for the given theme.
[[nodiscard]] inline auto GetThemeColors(ColorTheme theme) -> ThemeColors
{
    auto const resolvedTheme = [&]() -> ColorTheme
    {
        if (theme != ColorTheme::Auto)
            return theme;
        // Auto-detect: check COLORFGBG environment variable
        // Format: "fg;bg" where bg < 8 typically means dark background
        if (auto const* env = std::getenv("COLORFGBG"))
        {
            auto const s = std::string(env);
            auto const pos = s.rfind(';');
            if (pos != std::string::npos)
            {
                auto const bg = std::stoi(s.substr(pos + 1));
                return bg < 8 ? ColorTheme::Dark : ColorTheme::Light;
            }
        }
        return ColorTheme::Dark; // Default to dark
    }();

    if (resolvedTheme == ColorTheme::Dark)
    {
        return ThemeColors{
            .keywords = {.r = 86, .g = 156, .b = 214},      // #569CD6 — bright blue
            .identifiers = {.r = 156, .g = 220, .b = 254},  // #9CDCFE — light cyan
            .strings = {.r = 206, .g = 145, .b = 120},      // #CE9178 — warm orange
            .chars = {.r = 206, .g = 145, .b = 120},        // #CE9178 — warm orange
            .numbers = {.r = 181, .g = 206, .b = 168},      // #B5CEA8 — light green
            .comments = {.r = 106, .g = 153, .b = 85},      // #6A9955 — muted green
            .preprocessor = {.r = 197, .g = 134, .b = 192}, // #C586C0 — purple
            .operators = {.r = 212, .g = 212, .b = 212},    // #D4D4D4 — light gray
            .punctuation = {.r = 212, .g = 212, .b = 212},  // #D4D4D4 — light gray
            .defaultText = {.r = 212, .g = 212, .b = 212},  // #D4D4D4 — light gray
        };
    }

    // Light theme
    return ThemeColors{
        .keywords = {.r = 0, .g = 0, .b = 255},       // #0000FF — dark blue
        .identifiers = {.r = 0, .g = 0, .b = 0},      // #000000 — black
        .strings = {.r = 163, .g = 21, .b = 21},      // #A31515 — dark red
        .chars = {.r = 163, .g = 21, .b = 21},        // #A31515 — dark red
        .numbers = {.r = 9, .g = 134, .b = 88},       // #098658 — dark magenta/green
        .comments = {.r = 0, .g = 128, .b = 0},       // #008000 — dark green
        .preprocessor = {.r = 175, .g = 0, .b = 219}, // #AF00DB — dark purple
        .operators = {.r = 0, .g = 0, .b = 0},        // #000000 — black
        .punctuation = {.r = 0, .g = 0, .b = 0},      // #000000 — black
        .defaultText = {.r = 0, .g = 0, .b = 0},      // #000000 — black
    };
}

/// @brief Returns the ANSI truecolor escape sequence for the given token type and theme.
[[nodiscard]] inline auto ColorForTokenType(TokenType type, ColorTheme theme) -> std::string
{
    auto const colors = GetThemeColors(theme);

    auto const colorToAnsi = [](RGB const& c) -> std::string
    { return std::format("\033[38;2;{};{};{}m", c.r, c.g, c.b); };

    if (IsKeyword(type))
        return colorToAnsi(colors.keywords);
    if (type == TokenType::Identifier)
        return colorToAnsi(colors.identifiers);
    if (type == TokenType::StringLiteral)
        return colorToAnsi(colors.strings);
    if (type == TokenType::CharLiteral)
        return colorToAnsi(colors.chars);
    if (type == TokenType::NumericLiteral)
        return colorToAnsi(colors.numbers);
    if (IsComment(type))
        return colorToAnsi(colors.comments);
    if (type == TokenType::PreprocessorDirective)
        return colorToAnsi(colors.preprocessor);
    if (IsOperatorOrPunctuation(type))
        return colorToAnsi(colors.operators);

    return colorToAnsi(colors.defaultText);
}

/// @brief Returns ANSI background color for highlighting differing tokens.
///
/// Uses a subtle warm/reddish background on dark themes and a subtle pink
/// background on light themes. The foreground syntax color is preserved;
/// the reset sequence at the end of each token clears both fg and bg.
/// @param theme The active color theme.
/// @return ANSI escape sequence for the background color.
[[nodiscard]] inline auto DiffHighlightBg(ColorTheme theme) -> std::string
{
    auto const resolvedTheme = [&]() -> ColorTheme
    {
        if (theme != ColorTheme::Auto)
            return theme;
        if (auto const* env = std::getenv("COLORFGBG"))
        {
            auto const s = std::string(env);
            auto const pos = s.rfind(';');
            if (pos != std::string::npos)
            {
                auto const bg = std::stoi(s.substr(pos + 1));
                return bg < 8 ? ColorTheme::Dark : ColorTheme::Light;
            }
        }
        return ColorTheme::Dark;
    }();

    if (resolvedTheme == ColorTheme::Dark)
        return "\033[48;2;90;45;45m"; // Dark reddish background
    return "\033[48;2;255;215;215m";  // Light pink background
}

/// @brief ANSI reset escape sequence.
constexpr auto ansiReset = "\033[0m";

} // namespace codedup
