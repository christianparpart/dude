// SPDX-License-Identifier: Apache-2.0
#include <codedup/Languages/CppLanguage.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <utility>

namespace codedup
{

namespace
{

// =====================================================================
// Tokenizer internals
// =====================================================================

struct KeywordEntry
{
    std::string_view text;
    TokenType type;
};

// Sorted array of keywords for binary search lookup.
constexpr auto keywords = std::to_array<KeywordEntry>({
    {.text = "alignas", .type = TokenType::Alignas},
    {.text = "alignof", .type = TokenType::Alignof},
    {.text = "auto", .type = TokenType::Auto},
    {.text = "bool", .type = TokenType::Bool},
    {.text = "break", .type = TokenType::Break},
    {.text = "case", .type = TokenType::Case},
    {.text = "catch", .type = TokenType::Catch},
    {.text = "char", .type = TokenType::Char},
    {.text = "char16_t", .type = TokenType::Char16T},
    {.text = "char32_t", .type = TokenType::Char32T},
    {.text = "char8_t", .type = TokenType::Char8T},
    {.text = "class", .type = TokenType::Class},
    {.text = "co_await", .type = TokenType::CoAwait},
    {.text = "co_return", .type = TokenType::CoReturn},
    {.text = "co_yield", .type = TokenType::CoYield},
    {.text = "concept", .type = TokenType::Concept},
    {.text = "const", .type = TokenType::Const},
    {.text = "consteval", .type = TokenType::Consteval},
    {.text = "constexpr", .type = TokenType::Constexpr},
    {.text = "constinit", .type = TokenType::Constinit},
    {.text = "continue", .type = TokenType::Continue},
    {.text = "decltype", .type = TokenType::Decltype},
    {.text = "default", .type = TokenType::Default},
    {.text = "delete", .type = TokenType::Delete},
    {.text = "do", .type = TokenType::Do},
    {.text = "double", .type = TokenType::Double},
    {.text = "dynamic_cast", .type = TokenType::DynamicCast},
    {.text = "else", .type = TokenType::Else},
    {.text = "enum", .type = TokenType::Enum},
    {.text = "explicit", .type = TokenType::Explicit},
    {.text = "export", .type = TokenType::Export},
    {.text = "extern", .type = TokenType::Extern},
    {.text = "false", .type = TokenType::False},
    {.text = "float", .type = TokenType::Float},
    {.text = "for", .type = TokenType::For},
    {.text = "friend", .type = TokenType::Friend},
    {.text = "goto", .type = TokenType::Goto},
    {.text = "if", .type = TokenType::If},
    {.text = "inline", .type = TokenType::Inline},
    {.text = "int", .type = TokenType::Int},
    {.text = "long", .type = TokenType::Long},
    {.text = "mutable", .type = TokenType::Mutable},
    {.text = "namespace", .type = TokenType::Namespace},
    {.text = "new", .type = TokenType::New},
    {.text = "noexcept", .type = TokenType::Noexcept},
    {.text = "nullptr", .type = TokenType::Nullptr},
    {.text = "operator", .type = TokenType::Operator},
    {.text = "override", .type = TokenType::Override},
    {.text = "private", .type = TokenType::Private},
    {.text = "protected", .type = TokenType::Protected},
    {.text = "public", .type = TokenType::Public},
    {.text = "register", .type = TokenType::Register},
    {.text = "reinterpret_cast", .type = TokenType::ReinterpretCast},
    {.text = "requires", .type = TokenType::Requires},
    {.text = "return", .type = TokenType::Return},
    {.text = "short", .type = TokenType::Short},
    {.text = "signed", .type = TokenType::Signed},
    {.text = "sizeof", .type = TokenType::Sizeof},
    {.text = "static", .type = TokenType::Static},
    {.text = "static_assert", .type = TokenType::StaticAssert},
    {.text = "static_cast", .type = TokenType::StaticCast},
    {.text = "struct", .type = TokenType::Struct},
    {.text = "switch", .type = TokenType::Switch},
    {.text = "template", .type = TokenType::Template},
    {.text = "this", .type = TokenType::This},
    {.text = "thread_local", .type = TokenType::ThreadLocal},
    {.text = "throw", .type = TokenType::Throw},
    {.text = "true", .type = TokenType::True},
    {.text = "try", .type = TokenType::Try},
    {.text = "typedef", .type = TokenType::Typedef},
    {.text = "typeid", .type = TokenType::Typeid},
    {.text = "typename", .type = TokenType::Typename},
    {.text = "union", .type = TokenType::Union},
    {.text = "unsigned", .type = TokenType::Unsigned},
    {.text = "using", .type = TokenType::Using},
    {.text = "virtual", .type = TokenType::Virtual},
    {.text = "void", .type = TokenType::Void},
    {.text = "volatile", .type = TokenType::Volatile},
    {.text = "wchar_t", .type = TokenType::WcharT},
    {.text = "while", .type = TokenType::While},
});

/// @brief Entry mapping an operator string to its TokenType.
struct OperatorEntry
{
    std::string_view text;
    TokenType type;
};

/// @brief Sorted array of three-character operators for binary search lookup.
constexpr auto threeCharOperators = std::to_array<OperatorEntry>({
    {.text = "->*", .type = TokenType::ArrowStar},
    {.text = "...", .type = TokenType::Ellipsis},
    {.text = "<<=", .type = TokenType::LessLessEqual},
    {.text = "<=>", .type = TokenType::Spaceship},
    {.text = ">>=", .type = TokenType::GreaterGreaterEqual},
});

/// @brief Sorted array of two-character operators for binary search lookup.
constexpr auto twoCharOperators = std::to_array<OperatorEntry>({
    {.text = "!=", .type = TokenType::ExclaimEqual},   {.text = "##", .type = TokenType::HashHash},
    {.text = "%=", .type = TokenType::PercentEqual},   {.text = "&&", .type = TokenType::AmpAmp},
    {.text = "&=", .type = TokenType::AmpEqual},       {.text = "*=", .type = TokenType::StarEqual},
    {.text = "++", .type = TokenType::PlusPlus},       {.text = "+=", .type = TokenType::PlusEqual},
    {.text = "--", .type = TokenType::MinusMinus},     {.text = "-=", .type = TokenType::MinusEqual},
    {.text = "->", .type = TokenType::Arrow},          {.text = ".*", .type = TokenType::DotStar},
    {.text = "/=", .type = TokenType::SlashEqual},     {.text = "::", .type = TokenType::ColonColon},
    {.text = "<<", .type = TokenType::LessLess},       {.text = "<=", .type = TokenType::LessEqual},
    {.text = "==", .type = TokenType::EqualEqual},     {.text = ">=", .type = TokenType::GreaterEqual},
    {.text = ">>", .type = TokenType::GreaterGreater}, {.text = "^=", .type = TokenType::CaretEqual},
    {.text = "|=", .type = TokenType::PipeEqual},      {.text = "||", .type = TokenType::PipePipe},
});

/// @brief Looks up a keyword by text using binary search.
[[nodiscard]] auto LookupKeyword(std::string_view text) -> TokenType
{
    auto const* const it = std::ranges::lower_bound(keywords, text, {}, &KeywordEntry::text);
    if (it != keywords.end() && it->text == text)
        return it->type;
    return TokenType::Identifier;
}

/// @brief Looks up a multi-character operator by text using binary search.
[[nodiscard]] auto LookupOperator(std::span<const OperatorEntry> table, std::string_view text)
    -> std::optional<TokenType>
{
    auto const it = std::ranges::lower_bound(table, text, {}, &OperatorEntry::text);
    if (it != table.end() && it->text == text)
        return it->type;
    return std::nullopt;
}

[[nodiscard]] auto IsIdentifierStart(char ch) -> bool
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

[[nodiscard]] auto IsIdentifierContinue(char ch) -> bool
{
    return IsIdentifierStart(ch) || (ch >= '0' && ch <= '9');
}

[[nodiscard]] auto IsDigit(char ch) -> bool
{
    return ch >= '0' && ch <= '9';
}

[[nodiscard]] auto IsHexDigit(char ch) -> bool
{
    return IsDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

[[nodiscard]] auto IsBinDigit(char ch) -> bool
{
    return ch == '0' || ch == '1';
}

[[nodiscard]] auto IsOctDigit(char ch) -> bool
{
    return ch >= '0' && ch <= '7';
}

/// @brief Scanner state for the C++ tokenizer.
class Scanner
{
public:
    Scanner(std::string_view source, std::filesystem::path filePath) : _source(source), _filePath(std::move(filePath))
    {
    }

    [[nodiscard]] auto Tokenize() -> std::expected<std::vector<Token>, TokenizerError>
    {
        std::vector<Token> tokens;
        tokens.reserve(_source.size() / 4); // rough estimate

        while (!AtEnd())
        {
            SkipWhitespace();
            if (AtEnd())
                break;

            auto token = NextToken();
            if (!token)
                return std::unexpected(std::move(token.error()));
            tokens.push_back(std::move(*token));
        }

        tokens.push_back(Token{.type = TokenType::EndOfFile, .text = {}, .location = CurrentLocation()});
        return tokens;
    }

private:
    std::string_view _source;
    std::filesystem::path _filePath;
    size_t _pos = 0;
    uint32_t _line = 1;
    uint32_t _column = 1;

    [[nodiscard]] auto AtEnd() const -> bool { return _pos >= _source.size(); }

    [[nodiscard]] auto Peek() const -> char
    {
        if (AtEnd())
            return '\0';
        return _source[_pos];
    }

    [[nodiscard]] auto PeekAt(size_t offset) const -> char
    {
        auto const idx = _pos + offset;
        if (idx >= _source.size())
            return '\0';
        return _source[idx];
    }

    auto Advance() -> char
    {
        auto const ch = _source[_pos++];
        if (ch == '\n')
        {
            ++_line;
            _column = 1;
        }
        else
        {
            ++_column;
        }
        return ch;
    }

    void SkipWhitespace()
    {
        while (!AtEnd())
        {
            auto const ch = Peek();
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                Advance();
            else
                break;
        }
    }

    [[nodiscard]] auto CurrentLocation() const -> SourceLocation
    {
        return {.filePath = _filePath, .line = _line, .column = _column};
    }

    [[nodiscard]] auto NextToken() -> std::expected<Token, TokenizerError>
    {
        auto const loc = CurrentLocation();
        auto const ch = Peek();

        // Preprocessor directives
        if (ch == '#' && (loc.column == 1 || IsStartOfLine()))
            return ScanPreprocessorDirective(loc);

        // Comments
        if (ch == '/' && PeekAt(1) == '/')
            return ScanLineComment(loc);
        if (ch == '/' && PeekAt(1) == '*')
            return ScanBlockComment(loc);

        // String and char literals
        if (ch == '"')
            return ScanStringLiteral(loc);
        if (ch == '\'')
            return ScanCharLiteral(loc);

        // Raw string literals: R"delim(...)delim"
        if (ch == 'R' && PeekAt(1) == '"')
            return ScanRawStringLiteral(loc);

        // Identifiers and keywords (also handles prefixed strings like u8"...", L'...')
        if (IsIdentifierStart(ch))
        {
            // Check for string literal prefixes: u8, u, U, L
            if ((ch == 'u' || ch == 'U' || ch == 'L') && PeekAt(1) == '"')
                return ScanStringLiteral(loc, true);
            if ((ch == 'u' || ch == 'U' || ch == 'L') && PeekAt(1) == '\'')
                return ScanCharLiteral(loc, true);
            if (ch == 'u' && PeekAt(1) == '8' && PeekAt(2) == '"')
                return ScanStringLiteral(loc, true);
            if (ch == 'u' && PeekAt(1) == '8' && PeekAt(2) == '\'')
                return ScanCharLiteral(loc, true);
            // Check for prefixed raw strings: u8R"(...)", uR"(...)", etc.
            if ((ch == 'u' || ch == 'U' || ch == 'L') && PeekAt(1) == 'R' && PeekAt(2) == '"')
                return ScanRawStringLiteral(loc, true);
            if (ch == 'u' && PeekAt(1) == '8' && PeekAt(2) == 'R' && PeekAt(3) == '"')
                return ScanRawStringLiteral(loc, true);

            return ScanIdentifierOrKeyword(loc);
        }

        // Numeric literals
        if (IsDigit(ch))
            return ScanNumericLiteral(loc);

        // Dot can start a numeric literal (.5) or be an operator
        if (ch == '.' && IsDigit(PeekAt(1)))
            return ScanNumericLiteral(loc);

        // Operators and punctuation
        return ScanOperator(loc);
    }

    [[nodiscard]] auto IsStartOfLine() const -> bool
    {
        // Check if only whitespace precedes current position on this line
        for (auto i = _pos; i > 0; --i)
        {
            auto const c = _source[i - 1];
            if (c == '\n')
                return true;
            if (c != ' ' && c != '\t')
                return false;
        }
        return true; // Start of file
    }

    [[nodiscard]] auto ScanPreprocessorDirective(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // skip '#'

        // Read until end of line, handling backslash continuation
        while (!AtEnd())
        {
            auto const c = Peek();
            if (c == '\\' && PeekAt(1) == '\n')
            {
                Advance(); // backslash
                Advance(); // newline
                continue;
            }
            if (c == '\n')
                break;
            Advance();
        }

        return Token{.type = TokenType::PreprocessorDirective,
                     .text = std::string(_source.substr(start, _pos - start)),
                     .location = loc};
    }

    [[nodiscard]] auto ScanLineComment(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // first /
        Advance(); // second /

        while (!AtEnd() && Peek() != '\n')
            Advance();

        return Token{
            .type = TokenType::LineComment, .text = std::string(_source.substr(start, _pos - start)), .location = loc};
    }

    [[nodiscard]] auto ScanBlockComment(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // /
        Advance(); // *

        while (!AtEnd())
        {
            if (Peek() == '*' && PeekAt(1) == '/')
            {
                Advance(); // *
                Advance(); // /
                return Token{.type = TokenType::BlockComment,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated block comment", .location = loc});
    }

    [[nodiscard]] auto ScanStringLiteral(SourceLocation const& loc, bool hasPrefix = false)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        // Skip prefix (u, U, L, u8)
        if (hasPrefix)
        {
            if (Peek() == 'u' && PeekAt(1) == '8')
            {
                Advance();
                Advance();
            }
            else
            {
                Advance();
            }
        }

        Advance(); // opening quote

        while (!AtEnd())
        {
            auto const c = Peek();
            if (c == '\\')
            {
                Advance(); // backslash
                if (!AtEnd())
                    Advance(); // escaped char
                continue;
            }
            if (c == '"')
            {
                Advance(); // closing quote
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            if (c == '\n')
                break; // Unterminated
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated string literal", .location = loc});
    }

    [[nodiscard]] auto ScanRawStringLiteral(SourceLocation const& loc, bool hasPrefix = false)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        // Skip prefix
        if (hasPrefix)
        {
            if (Peek() == 'u' && PeekAt(1) == '8')
            {
                Advance();
                Advance();
            }
            else
            {
                Advance(); // u, U, or L
            }
        }

        Advance(); // R
        Advance(); // opening "

        // Read delimiter (up to 16 chars before '(')
        std::string delimiter;
        while (!AtEnd() && Peek() != '(')
        {
            delimiter += Advance();
            if (delimiter.size() > 16)
                return std::unexpected(TokenizerError{.message = "Raw string delimiter too long", .location = loc});
        }

        if (AtEnd())
            return std::unexpected(TokenizerError{.message = "Unterminated raw string literal", .location = loc});

        Advance(); // (

        // Build closing sequence: )delimiter"
        auto const closing = ")" + delimiter + "\"";

        while (!AtEnd())
        {
            if (_pos + closing.size() <= _source.size() && _source.substr(_pos, closing.size()) == closing)
            {
                for (size_t i = 0; i < closing.size(); ++i)
                    Advance();
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated raw string literal", .location = loc});
    }

    [[nodiscard]] auto ScanCharLiteral(SourceLocation const& loc, bool hasPrefix = false)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        if (hasPrefix)
        {
            if (Peek() == 'u' && PeekAt(1) == '8')
            {
                Advance();
                Advance();
            }
            else
            {
                Advance();
            }
        }

        Advance(); // opening quote

        while (!AtEnd())
        {
            auto const c = Peek();
            if (c == '\\')
            {
                Advance();
                if (!AtEnd())
                    Advance();
                continue;
            }
            if (c == '\'')
            {
                Advance(); // closing quote
                return Token{.type = TokenType::CharLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            if (c == '\n')
                break;
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated character literal", .location = loc});
    }

    [[nodiscard]] auto ScanIdentifierOrKeyword(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        while (!AtEnd() && IsIdentifierContinue(Peek()))
            Advance();

        auto const text = _source.substr(start, _pos - start);
        auto const type = LookupKeyword(text);

        return Token{.type = type, .text = std::string(text), .location = loc};
    }

    void ScanHexDigits()
    {
        Advance(); // 0
        Advance(); // x or X
        while (!AtEnd() && (IsHexDigit(Peek()) || Peek() == '\''))
            Advance();
    }

    void ScanBinaryDigits()
    {
        Advance(); // 0
        Advance(); // b or B
        while (!AtEnd() && (IsBinDigit(Peek()) || Peek() == '\''))
            Advance();
    }

    void ScanOctalDigits()
    {
        Advance(); // 0
        while (!AtEnd() && (IsOctDigit(Peek()) || Peek() == '\''))
            Advance();
    }

    void ScanDecimalWithFractionalAndExponent(size_t start)
    {
        // Decimal integer digits
        while (!AtEnd() && (IsDigit(Peek()) || Peek() == '\''))
            Advance();

        // Fractional part
        if (Peek() == '.' && (IsDigit(PeekAt(1)) || PeekAt(1) == 'e' || PeekAt(1) == 'E'))
        {
            Advance(); // .
            while (!AtEnd() && (IsDigit(Peek()) || Peek() == '\''))
                Advance();
        }
        else if (Peek() == '.' && _pos > start && IsDigit(_source[start]))
        {
            if (IsDigit(PeekAt(1)) || PeekAt(1) == 'e' || PeekAt(1) == 'E' || PeekAt(1) == 'f' || PeekAt(1) == 'F' ||
                PeekAt(1) == 'l' || PeekAt(1) == 'L')
            {
                Advance(); // .
                while (!AtEnd() && (IsDigit(Peek()) || Peek() == '\''))
                    Advance();
            }
        }

        // Exponent part
        if (Peek() == 'e' || Peek() == 'E')
        {
            Advance();
            if (Peek() == '+' || Peek() == '-')
                Advance();
            while (!AtEnd() && (IsDigit(Peek()) || Peek() == '\''))
                Advance();
        }
    }

    void ScanNumericSuffix()
    {
        while (!AtEnd() && (Peek() == 'u' || Peek() == 'U' || Peek() == 'l' || Peek() == 'L' || Peek() == 'f' ||
                            Peek() == 'F' || Peek() == 'z' || Peek() == 'Z'))
        {
            Advance();
        }
    }

    [[nodiscard]] auto ScanNumericLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        if (Peek() == '0' && (PeekAt(1) == 'x' || PeekAt(1) == 'X'))
            ScanHexDigits();
        else if (Peek() == '0' && (PeekAt(1) == 'b' || PeekAt(1) == 'B'))
            ScanBinaryDigits();
        else if (Peek() == '0' && IsOctDigit(PeekAt(1)))
            ScanOctalDigits();
        else
            ScanDecimalWithFractionalAndExponent(start);

        ScanNumericSuffix();

        return Token{.type = TokenType::NumericLiteral,
                     .text = std::string(_source.substr(start, _pos - start)),
                     .location = loc};
    }

    void AdvanceBy(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            Advance();
    }

    [[nodiscard]] auto TryMatchOperator(std::span<const OperatorEntry> table, size_t length, SourceLocation const& loc)
        -> std::optional<Token>
    {
        if (_pos + length - 1 >= _source.size())
            return std::nullopt;

        auto const text = _source.substr(_pos, length);
        auto const type = LookupOperator(table, text);
        if (!type)
            return std::nullopt;

        AdvanceBy(length);
        return Token{.type = *type, .text = std::string(text), .location = loc};
    }

    [[nodiscard]] static auto ScanSingleCharOperator(char ch, SourceLocation const& loc) -> Token
    {
        switch (ch)
        {
            case '(':
                return Token{.type = TokenType::LeftParen, .text = "(", .location = loc};
            case ')':
                return Token{.type = TokenType::RightParen, .text = ")", .location = loc};
            case '[':
                return Token{.type = TokenType::LeftBracket, .text = "[", .location = loc};
            case ']':
                return Token{.type = TokenType::RightBracket, .text = "]", .location = loc};
            case '{':
                return Token{.type = TokenType::LeftBrace, .text = "{", .location = loc};
            case '}':
                return Token{.type = TokenType::RightBrace, .text = "}", .location = loc};
            case ';':
                return Token{.type = TokenType::Semicolon, .text = ";", .location = loc};
            case ':':
                return Token{.type = TokenType::Colon, .text = ":", .location = loc};
            case ',':
                return Token{.type = TokenType::Comma, .text = ",", .location = loc};
            case '.':
                return Token{.type = TokenType::Dot, .text = ".", .location = loc};
            case '~':
                return Token{.type = TokenType::Tilde, .text = "~", .location = loc};
            case '!':
                return Token{.type = TokenType::Exclaim, .text = "!", .location = loc};
            case '+':
                return Token{.type = TokenType::Plus, .text = "+", .location = loc};
            case '-':
                return Token{.type = TokenType::Minus, .text = "-", .location = loc};
            case '*':
                return Token{.type = TokenType::Star, .text = "*", .location = loc};
            case '/':
                return Token{.type = TokenType::Slash, .text = "/", .location = loc};
            case '%':
                return Token{.type = TokenType::Percent, .text = "%", .location = loc};
            case '&':
                return Token{.type = TokenType::Amp, .text = "&", .location = loc};
            case '|':
                return Token{.type = TokenType::Pipe, .text = "|", .location = loc};
            case '^':
                return Token{.type = TokenType::Caret, .text = "^", .location = loc};
            case '<':
                return Token{.type = TokenType::Less, .text = "<", .location = loc};
            case '>':
                return Token{.type = TokenType::Greater, .text = ">", .location = loc};
            case '=':
                return Token{.type = TokenType::Equal, .text = "=", .location = loc};
            case '?':
                return Token{.type = TokenType::Question, .text = "?", .location = loc};
            case '#':
                return Token{.type = TokenType::Hash, .text = "#", .location = loc};
            default:
                return Token{.type = TokenType::Invalid, .text = std::string(1, ch), .location = loc};
        }
    }

    [[nodiscard]] auto ScanOperator(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        // Try three-character operators first
        if (auto token = TryMatchOperator(threeCharOperators, 3, loc))
            return std::move(*token);

        // Try two-character operators
        if (auto token = TryMatchOperator(twoCharOperators, 2, loc))
            return std::move(*token);

        // Single-character operators
        auto const ch = Peek();
        Advance();
        return ScanSingleCharOperator(ch, loc);
    }
};

// =====================================================================
// Block extraction internals
// =====================================================================

/// @brief Result of a backward paren-matching operation.
struct ParenMatchResult
{
    size_t pos;   ///< The position after matching.
    bool success; ///< Whether matching succeeded.
};

[[nodiscard]] auto IsTrailingQualifier(TokenType type) -> bool
{
    return type == TokenType::Const || type == TokenType::Noexcept || type == TokenType::Override ||
           type == TokenType::Volatile || type == TokenType::Amp || type == TokenType::AmpAmp ||
           type == TokenType::Arrow || type == TokenType::Identifier;
}

[[nodiscard]] auto IsSimpleTrailingQualifier(TokenType type) -> bool
{
    return type == TokenType::Const || type == TokenType::Noexcept || type == TokenType::Override ||
           type == TokenType::Volatile || type == TokenType::Amp || type == TokenType::AmpAmp;
}

[[nodiscard]] auto SkipTrailingQualifiers(std::span<Token const> tokens, size_t pos) -> size_t
{
    while (pos > 0)
    {
        auto const type = tokens[pos].type;
        if (!IsTrailingQualifier(type))
            break;

        if (type == TokenType::Identifier && tokens[pos].text != "final" && tokens[pos].text != "noexcept")
        {
            if (pos > 0 && tokens[pos - 1].type == TokenType::Arrow)
            {
                pos -= 2;
                continue;
            }
            break;
        }
        --pos;
    }
    return pos;
}

[[nodiscard]] auto SkipSimpleTrailingQualifiers(std::span<Token const> tokens, size_t pos) -> size_t
{
    while (pos > 0)
    {
        auto const type = tokens[pos].type;
        if (IsSimpleTrailingQualifier(type))
        {
            --pos;
            continue;
        }
        if (type == TokenType::Identifier && (tokens[pos].text == "final" || tokens[pos].text == "noexcept"))
        {
            --pos;
            continue;
        }
        break;
    }
    return pos;
}

[[nodiscard]] auto SkipMatchedDelimiterBackward(std::span<Token const> tokens, size_t scanPos, TokenType openType,
                                                TokenType closeType) -> ParenMatchResult
{
    size_t depth = 1;
    if (scanPos == 0)
        return {.pos = scanPos, .success = false};
    --scanPos;
    while (scanPos > 0 && depth > 0)
    {
        if (tokens[scanPos].type == closeType)
            ++depth;
        else if (tokens[scanPos].type == openType)
            --depth;
        if (depth > 0)
            --scanPos;
    }
    return {.pos = scanPos, .success = depth == 0};
}

[[nodiscard]] auto SkipInitializerList(std::span<Token const> tokens, size_t pos) -> size_t
{
    if (tokens[pos].type == TokenType::RightParen)
        return pos;

    if (pos == 0)
        return pos;

    auto scanPos = pos;
    while (scanPos > 0)
    {
        if (tokens[scanPos].type == TokenType::RightParen)
        {
            auto const [newPos, ok] =
                SkipMatchedDelimiterBackward(tokens, scanPos, TokenType::LeftParen, TokenType::RightParen);
            if (!ok)
                break;
            scanPos = newPos;
            if (scanPos > 0)
                --scanPos;
            continue;
        }
        if (tokens[scanPos].type == TokenType::LeftBrace)
        {
            auto const [newPos, ok] =
                SkipMatchedDelimiterBackward(tokens, scanPos, TokenType::RightBrace, TokenType::LeftBrace);
            if (!ok)
                break;
            scanPos = newPos;
            if (scanPos > 0)
                --scanPos;
            continue;
        }
        if (tokens[scanPos].type == TokenType::Comma || tokens[scanPos].type == TokenType::Identifier ||
            tokens[scanPos].type == TokenType::ColonColon)
        {
            --scanPos;
            continue;
        }
        if (tokens[scanPos].type == TokenType::Colon)
        {
            pos = scanPos;
            if (pos > 0)
                --pos;
            return pos;
        }
        break;
    }
    return pos;
}

[[nodiscard]] auto SkipMatchedParensBackward(std::span<Token const> tokens, size_t pos) -> ParenMatchResult
{
    if (tokens[pos].type != TokenType::RightParen)
        return {.pos = pos, .success = false};

    size_t depth = 1;
    if (pos == 0)
        return {.pos = pos, .success = false};
    --pos;
    while (pos > 0 && depth > 0)
    {
        if (tokens[pos].type == TokenType::RightParen)
            ++depth;
        else if (tokens[pos].type == TokenType::LeftParen)
            --depth;
        if (depth > 0)
            --pos;
    }

    if (depth != 0 || pos == 0)
        return {.pos = pos, .success = false};

    return {.pos = pos, .success = true};
}

[[nodiscard]] auto SkipTemplateParamsBackward(std::span<Token const> tokens, size_t pos) -> size_t
{
    if (tokens[pos].type != TokenType::Greater)
        return pos;

    size_t angleDepth = 1;
    if (pos == 0)
        return pos;
    --pos;
    while (pos > 0 && angleDepth > 0)
    {
        if (tokens[pos].type == TokenType::Greater)
            ++angleDepth;
        else if (tokens[pos].type == TokenType::Less)
            --angleDepth;
        if (angleDepth > 0)
            --pos;
    }
    if (pos > 0)
        --pos;
    return pos;
}

[[nodiscard]] auto ExtractFunctionIdentifier(std::span<Token const> tokens, size_t pos) -> std::string
{
    if (tokens[pos].type == TokenType::Identifier)
    {
        auto name = tokens[pos].text;
        if (pos >= 2 && tokens[pos - 1].type == TokenType::ColonColon && tokens[pos - 2].type == TokenType::Identifier)
        {
            name = tokens[pos - 2].text + "::" + name;
        }
        return name;
    }
    if (tokens[pos].type == TokenType::Operator)
    {
        return "operator";
    }
    if (tokens[pos].type == TokenType::Tilde && pos > 0 && tokens[pos - 1].type == TokenType::Identifier)
    {
        return "~" + tokens[pos - 1].text;
    }
    return {};
}

[[nodiscard]] auto IsAllowedFunctionPrefixKeyword(TokenType type) -> bool
{
    return type == TokenType::Const || type == TokenType::Static || type == TokenType::Inline ||
           type == TokenType::Virtual || type == TokenType::Explicit || type == TokenType::Constexpr ||
           type == TokenType::Consteval || type == TokenType::Auto || type == TokenType::Template ||
           type == TokenType::Typename || type == TokenType::Void || type == TokenType::Int ||
           type == TokenType::Long || type == TokenType::Short || type == TokenType::Unsigned ||
           type == TokenType::Signed || type == TokenType::Char || type == TokenType::Bool ||
           type == TokenType::Float || type == TokenType::Double || type == TokenType::WcharT ||
           type == TokenType::Char8T || type == TokenType::Char16T || type == TokenType::Char32T ||
           type == TokenType::Extern || type == TokenType::Friend || type == TokenType::Noexcept;
}

[[nodiscard]] auto IsNonFunctionKeyword(std::span<Token const> tokens, size_t namePos) -> bool
{
    auto checkPos = namePos;
    while (checkPos > 0)
    {
        --checkPos;
        auto const type = tokens[checkPos].type;
        if (type == TokenType::Namespace)
            return true;
        if (type == TokenType::Class || type == TokenType::Struct || type == TokenType::Enum ||
            type == TokenType::Union)
        {
            if (checkPos + 1 == namePos || checkPos + 2 == namePos)
                return true;
            return false;
        }
        if (type == TokenType::Semicolon || type == TokenType::RightBrace || type == TokenType::LeftBrace)
            return false;
        if (IsKeyword(type) && !IsAllowedFunctionPrefixKeyword(type))
            return false;
    }
    return false;
}

[[nodiscard]] auto FindFunctionName(std::vector<Token> const& tokens, size_t braceIndex) -> std::string
{
    if (braceIndex == 0)
        return {};

    auto const tokenSpan = std::span<Token const>{tokens};

    auto pos = SkipTrailingQualifiers(tokenSpan, braceIndex - 1);
    pos = SkipInitializerList(tokenSpan, pos);
    pos = SkipSimpleTrailingQualifiers(tokenSpan, pos);

    auto const [parenPos, parenSuccess] = SkipMatchedParensBackward(tokenSpan, pos);
    if (!parenSuccess)
        return {};
    pos = parenPos - 1;

    pos = SkipTemplateParamsBackward(tokenSpan, pos);

    auto name = ExtractFunctionIdentifier(tokenSpan, pos);
    if (name.empty())
        return {};

    if (IsNonFunctionKeyword(tokenSpan, pos))
        return {};

    return name;
}

/// @brief Extracts code blocks from tokens, building index maps for normalized tokens.
[[nodiscard]] auto ExtractBlocksImpl(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                     std::vector<NormalizedToken> const& textPreserving,
                                     CodeBlockExtractorConfig const& config) -> std::vector<CodeBlock>
{
    std::vector<CodeBlock> blocks;
    auto const hasTextPreserving = !textPreserving.empty();

    // Build a mapping from original token index to normalized token index.
    std::vector<size_t> origToNormalized(tokens.size(), SIZE_MAX);
    for (auto const ni : std::views::iota(size_t{0}, normalized.size()))
        origToNormalized[normalized[ni].originalIndex] = ni;

    // Build the same mapping for text-preserving normalization.
    std::vector<size_t> origToTextPreserving;
    if (hasTextPreserving)
    {
        origToTextPreserving.assign(tokens.size(), SIZE_MAX);
        for (auto const ni : std::views::iota(size_t{0}, textPreserving.size()))
            origToTextPreserving[textPreserving[ni].originalIndex] = ni;
    }

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type != TokenType::LeftBrace)
            continue;

        auto funcName = FindFunctionName(tokens, i);
        if (funcName.empty())
            continue;

        size_t depth = 1;
        size_t bodyEnd = i + 1;
        while (bodyEnd < tokens.size() && depth > 0)
        {
            if (tokens[bodyEnd].type == TokenType::LeftBrace)
                ++depth;
            else if (tokens[bodyEnd].type == TokenType::RightBrace)
                --depth;
            ++bodyEnd;
        }

        if (depth != 0)
            continue;

        std::vector<NormalizedTokenId> normalizedIds;
        std::vector<NormalizedTokenId> textPreservingIds;
        for (size_t ti = i; ti < bodyEnd; ++ti)
        {
            if (origToNormalized[ti] != SIZE_MAX)
            {
                auto normIdx = origToNormalized[ti];
                if (normIdx < normalized.size())
                    normalizedIds.push_back(normalized[normIdx].id);
            }

            if (hasTextPreserving && origToTextPreserving[ti] != SIZE_MAX)
            {
                auto tpIdx = origToTextPreserving[ti];
                if (tpIdx < textPreserving.size())
                    textPreservingIds.push_back(textPreserving[tpIdx].id);
            }
        }

        if (normalizedIds.size() < config.minTokens)
            continue;

        auto const& startLoc = tokens[i].location;
        auto const& endLoc = tokens[bodyEnd - 1].location;

        blocks.push_back(CodeBlock{
            .name = std::move(funcName),
            .sourceRange = SourceRange{.start = startLoc, .end = endLoc},
            .tokenStart = i,
            .tokenEnd = bodyEnd,
            .normalizedIds = std::move(normalizedIds),
            .textPreservingIds = std::move(textPreservingIds),
        });

        i = bodyEnd - 1;
    }

    return blocks;
}

} // anonymous namespace

// =====================================================================
// CppLanguage public API
// =====================================================================

auto CppLanguage::Name() const -> std::string_view
{
    return "C++";
}

auto CppLanguage::Extensions() const -> std::span<std::string const>
{
    static std::string const exts[] = {".cpp", ".cxx", ".cc", ".c", ".h", ".hpp", ".hxx"};
    return exts;
}

auto CppLanguage::Tokenize(std::string_view source, std::filesystem::path const& filePath) const
    -> std::expected<std::vector<Token>, TokenizerError>
{
    Scanner scanner(source, filePath);
    return scanner.Tokenize();
}

auto CppLanguage::TokenizeFile(std::filesystem::path const& filePath, InputEncoding encoding) const
    -> std::expected<std::vector<Token>, TokenizerError>
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return std::unexpected(TokenizerError{.message = "Failed to open file: " + filePath.string(),
                                              .location = {.filePath = filePath, .line = 0, .column = 0}});

    std::ostringstream ss;
    ss << file.rdbuf();
    auto const rawContent = ss.str();

    auto utf8Result = ConvertToUtf8(rawContent, encoding);
    if (!utf8Result)
        return std::unexpected(TokenizerError{
            .message = std::format("Encoding error in {}: {}", filePath.string(), utf8Result.error().message),
            .location = {.filePath = filePath, .line = 0, .column = 0}});

    auto const& utf8Content = *utf8Result;
    return Tokenize(utf8Content, filePath);
}

auto CppLanguage::ExtractBlocks(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                std::vector<NormalizedToken> const& textPreserving,
                                CodeBlockExtractorConfig const& config) const -> std::vector<CodeBlock>
{
    return ExtractBlocksImpl(tokens, normalized, textPreserving, config);
}

auto CppLanguage::ShouldStripToken(TokenType type) const -> bool
{
    return IsComment(type) || type == TokenType::PreprocessorDirective || type == TokenType::EndOfFile;
}

auto CreateCppLanguage() -> std::shared_ptr<Language>
{
    return std::make_shared<CppLanguage>();
}

} // namespace codedup
