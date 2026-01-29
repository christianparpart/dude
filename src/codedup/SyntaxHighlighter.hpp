// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Token.hpp>

#include <cstdint>
#include <format>
#include <string>

namespace codedup
{

/// @brief Color theme selection for terminal output.
enum class ColorTheme
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
[[nodiscard]] inline auto themeColors(ColorTheme theme) -> ThemeColors
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
            .keywords = {86, 156, 214},      // #569CD6 — bright blue
            .identifiers = {156, 220, 254},  // #9CDCFE — light cyan
            .strings = {206, 145, 120},      // #CE9178 — warm orange
            .chars = {206, 145, 120},        // #CE9178 — warm orange
            .numbers = {181, 206, 168},      // #B5CEA8 — light green
            .comments = {106, 153, 85},      // #6A9955 — muted green
            .preprocessor = {197, 134, 192}, // #C586C0 — purple
            .operators = {212, 212, 212},    // #D4D4D4 — light gray
            .punctuation = {212, 212, 212},  // #D4D4D4 — light gray
            .defaultText = {212, 212, 212},  // #D4D4D4 — light gray
        };
    }

    // Light theme
    return ThemeColors{
        .keywords = {0, 0, 255},       // #0000FF — dark blue
        .identifiers = {0, 0, 0},      // #000000 — black
        .strings = {163, 21, 21},      // #A31515 — dark red
        .chars = {163, 21, 21},        // #A31515 — dark red
        .numbers = {9, 134, 88},       // #098658 — dark magenta/green
        .comments = {0, 128, 0},       // #008000 — dark green
        .preprocessor = {175, 0, 219}, // #AF00DB — dark purple
        .operators = {0, 0, 0},        // #000000 — black
        .punctuation = {0, 0, 0},      // #000000 — black
        .defaultText = {0, 0, 0},      // #000000 — black
    };
}

/// @brief Returns the ANSI truecolor escape sequence for the given token type and theme.
[[nodiscard]] inline auto colorForTokenType(TokenType type, ColorTheme theme) -> std::string
{
    auto const colors = themeColors(theme);

    auto const colorToAnsi = [](RGB const& c) -> std::string
    { return std::format("\033[38;2;{};{};{}m", c.r, c.g, c.b); };

    if (isKeyword(type))
        return colorToAnsi(colors.keywords);
    if (type == TokenType::Identifier)
        return colorToAnsi(colors.identifiers);
    if (type == TokenType::StringLiteral)
        return colorToAnsi(colors.strings);
    if (type == TokenType::CharLiteral)
        return colorToAnsi(colors.chars);
    if (type == TokenType::NumericLiteral)
        return colorToAnsi(colors.numbers);
    if (isComment(type))
        return colorToAnsi(colors.comments);
    if (type == TokenType::PreprocessorDirective)
        return colorToAnsi(colors.preprocessor);
    if (isOperatorOrPunctuation(type))
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
[[nodiscard]] inline auto diffHighlightBg(ColorTheme theme) -> std::string
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
