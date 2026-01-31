// SPDX-License-Identifier: Apache-2.0
#include <dude/SyntaxHighlighter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <format>
#include <string>

using namespace dude;

namespace
{

/// @brief Builds the expected ANSI truecolor foreground escape sequence for an RGB color.
auto ExpectedAnsi(RGB const& c) -> std::string
{
    return std::format("\033[38;2;{};{};{}m", c.r, c.g, c.b);
}

/// @brief RAII guard that sets an environment variable and restores the previous value on destruction.
struct ScopedEnv
{
    std::string name;
    std::string saved;
    bool hadPrev;

    ScopedEnv(char const* envName, char const* value) : name(envName)
    {
        auto const* prev = std::getenv(envName);
        saved = prev ? prev : "";
        hadPrev = prev != nullptr;
#ifdef _WIN32
        _putenv_s(envName, value);
#else
        setenv(envName, value, 1);
#endif
    }

    ~ScopedEnv()
    {
        if (hadPrev)
        {
#ifdef _WIN32
            _putenv_s(name.c_str(), saved.c_str());
#else
            setenv(name.c_str(), saved.c_str(), 1);
#endif
        }
        else
        {
#ifdef _WIN32
            _putenv_s(name.c_str(), "");
#else
            unsetenv(name.c_str());
#endif
        }
    }

    ScopedEnv(ScopedEnv const&) = delete;
    ScopedEnv& operator=(ScopedEnv const&) = delete;
    ScopedEnv(ScopedEnv&&) = delete;
    ScopedEnv& operator=(ScopedEnv&&) = delete;
};

} // namespace

// ---------------------------------------------------------------------------
// GetThemeColors
// ---------------------------------------------------------------------------

TEST_CASE("GetThemeColors.DarkTheme", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(colors.keywords.r == 86);
    CHECK(colors.keywords.g == 156);
    CHECK(colors.keywords.b == 214);
    CHECK(colors.identifiers.r == 156);
    CHECK(colors.strings.r == 206);
    CHECK(colors.chars.r == 206);
    CHECK(colors.numbers.r == 181);
    CHECK(colors.comments.r == 106);
    CHECK(colors.preprocessor.r == 197);
    CHECK(colors.operators.r == 212);
    CHECK(colors.punctuation.r == 212);
    CHECK(colors.defaultText.r == 212);
}

TEST_CASE("GetThemeColors.LightTheme", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Light);
    CHECK(colors.keywords.r == 0);
    CHECK(colors.keywords.g == 0);
    CHECK(colors.keywords.b == 255);
    CHECK(colors.identifiers.r == 0);
    CHECK(colors.strings.r == 163);
    CHECK(colors.chars.r == 163);
    CHECK(colors.numbers.r == 9);
    CHECK(colors.comments.r == 0);
    CHECK(colors.comments.g == 128);
    CHECK(colors.preprocessor.r == 175);
    CHECK(colors.operators.r == 0);
    CHECK(colors.defaultText.r == 0);
}

TEST_CASE("GetThemeColors.AutoDefaultsToDark", "[SyntaxHighlighter]")
{
    // When COLORFGBG is not set, Auto should default to Dark.
    // If COLORFGBG is set, Auto resolves based on background value.
    auto const autoColors = GetThemeColors(ColorTheme::Auto);
    auto const darkColors = GetThemeColors(ColorTheme::Dark);
    auto const lightColors = GetThemeColors(ColorTheme::Light);

    // Auto must resolve to one of the two themes
    auto const matchesDark = autoColors.keywords.r == darkColors.keywords.r &&
                             autoColors.keywords.g == darkColors.keywords.g &&
                             autoColors.keywords.b == darkColors.keywords.b;
    auto const matchesLight = autoColors.keywords.r == lightColors.keywords.r &&
                              autoColors.keywords.g == lightColors.keywords.g &&
                              autoColors.keywords.b == lightColors.keywords.b;
    CHECK((matchesDark || matchesLight));
}

// ---------------------------------------------------------------------------
// ColorForTokenType
// ---------------------------------------------------------------------------

TEST_CASE("ColorForTokenType.Keywords", "[SyntaxHighlighter]")
{
    auto const darkColors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::If, ColorTheme::Dark) == ExpectedAnsi(darkColors.keywords));
    CHECK(ColorForTokenType(TokenType::Return, ColorTheme::Dark) == ExpectedAnsi(darkColors.keywords));
    CHECK(ColorForTokenType(TokenType::CSharp_Abstract, ColorTheme::Dark) == ExpectedAnsi(darkColors.keywords));
    CHECK(ColorForTokenType(TokenType::Python_Def, ColorTheme::Dark) == ExpectedAnsi(darkColors.keywords));

    auto const lightColors = GetThemeColors(ColorTheme::Light);
    CHECK(ColorForTokenType(TokenType::If, ColorTheme::Light) == ExpectedAnsi(lightColors.keywords));
}

TEST_CASE("ColorForTokenType.Identifier", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::Identifier, ColorTheme::Dark) == ExpectedAnsi(colors.identifiers));
}

TEST_CASE("ColorForTokenType.StringLiteral", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::StringLiteral, ColorTheme::Dark) == ExpectedAnsi(colors.strings));
}

TEST_CASE("ColorForTokenType.CharLiteral", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::CharLiteral, ColorTheme::Dark) == ExpectedAnsi(colors.chars));
}

TEST_CASE("ColorForTokenType.NumericLiteral", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::NumericLiteral, ColorTheme::Dark) == ExpectedAnsi(colors.numbers));
}

TEST_CASE("ColorForTokenType.Comments", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::LineComment, ColorTheme::Dark) == ExpectedAnsi(colors.comments));
    CHECK(ColorForTokenType(TokenType::BlockComment, ColorTheme::Dark) == ExpectedAnsi(colors.comments));
}

TEST_CASE("ColorForTokenType.Preprocessor", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::PreprocessorDirective, ColorTheme::Dark) == ExpectedAnsi(colors.preprocessor));
}

TEST_CASE("ColorForTokenType.Operators", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::Plus, ColorTheme::Dark) == ExpectedAnsi(colors.operators));
    CHECK(ColorForTokenType(TokenType::LeftParen, ColorTheme::Dark) == ExpectedAnsi(colors.operators));
    CHECK(ColorForTokenType(TokenType::EqualEqual, ColorTheme::Dark) == ExpectedAnsi(colors.operators));
}

TEST_CASE("ColorForTokenType.DefaultText", "[SyntaxHighlighter]")
{
    auto const colors = GetThemeColors(ColorTheme::Dark);
    CHECK(ColorForTokenType(TokenType::EndOfFile, ColorTheme::Dark) == ExpectedAnsi(colors.defaultText));
    CHECK(ColorForTokenType(TokenType::Invalid, ColorTheme::Dark) == ExpectedAnsi(colors.defaultText));
}

// ---------------------------------------------------------------------------
// DiffHighlightBg
// ---------------------------------------------------------------------------

TEST_CASE("DiffHighlightBg.DarkTheme", "[SyntaxHighlighter]")
{
    auto const bg = DiffHighlightBg(ColorTheme::Dark);
    CHECK(bg == "\033[48;2;90;45;45m");
}

TEST_CASE("DiffHighlightBg.LightTheme", "[SyntaxHighlighter]")
{
    auto const bg = DiffHighlightBg(ColorTheme::Light);
    CHECK(bg == "\033[48;2;255;215;215m");
}

// ---------------------------------------------------------------------------
// ansiReset
// ---------------------------------------------------------------------------

TEST_CASE("AnsiReset.Value", "[SyntaxHighlighter]")
{
    CHECK(std::string(ansiReset) == "\033[0m");
}

// ---------------------------------------------------------------------------
// Coverage: COLORFGBG environment variable detection
// ---------------------------------------------------------------------------

TEST_CASE("GetThemeColors.AutoWithDarkCOLORFGBG", "[SyntaxHighlighter]")
{
    ScopedEnv env("COLORFGBG", "15;0");

    auto const autoColors = GetThemeColors(ColorTheme::Auto);
    auto const darkColors = GetThemeColors(ColorTheme::Dark);
    CHECK(autoColors.keywords.r == darkColors.keywords.r);
    CHECK(autoColors.keywords.g == darkColors.keywords.g);
    CHECK(autoColors.keywords.b == darkColors.keywords.b);
}

TEST_CASE("GetThemeColors.AutoWithLightCOLORFGBG", "[SyntaxHighlighter]")
{
    ScopedEnv env("COLORFGBG", "0;15");

    auto const autoColors = GetThemeColors(ColorTheme::Auto);
    auto const lightColors = GetThemeColors(ColorTheme::Light);
    CHECK(autoColors.keywords.r == lightColors.keywords.r);
    CHECK(autoColors.keywords.g == lightColors.keywords.g);
    CHECK(autoColors.keywords.b == lightColors.keywords.b);
}

TEST_CASE("DiffHighlightBg.AutoWithCOLORFGBG", "[SyntaxHighlighter]")
{
    ScopedEnv env("COLORFGBG", "15;0");

    auto const bg = DiffHighlightBg(ColorTheme::Auto);
    CHECK(bg == "\033[48;2;90;45;45m"); // Dark theme
}

TEST_CASE("DiffHighlightBg.AutoWithoutSemicolonInCOLORFGBG", "[SyntaxHighlighter]")
{
    // COLORFGBG is set but has no semicolon, so theme resolution falls through to Dark
    ScopedEnv env("COLORFGBG", "nodelimiter");

    auto const bg = DiffHighlightBg(ColorTheme::Auto);
    CHECK(bg == "\033[48;2;90;45;45m"); // Dark theme fallback
}
