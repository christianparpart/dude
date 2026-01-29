// SPDX-License-Identifier: Apache-2.0
#include <codedup/Tokenizer.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <utility>

namespace codedup
{

namespace
{

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
/// @param table The sorted array of operator entries to search.
/// @param text The operator text to look up.
/// @return The corresponding TokenType, or std::nullopt if not found.
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

/// @brief Scanner state for the tokenizer.
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

    /// @brief Scans hexadecimal digit sequence after the '0x' or '0X' prefix.
    ///
    /// Consumes the '0' and 'x'/'X' prefix, then all hex digits and digit separators.
    void ScanHexDigits()
    {
        Advance(); // 0
        Advance(); // x or X
        while (!AtEnd() && (IsHexDigit(Peek()) || Peek() == '\''))
            Advance();
    }

    /// @brief Scans binary digit sequence after the '0b' or '0B' prefix.
    ///
    /// Consumes the '0' and 'b'/'B' prefix, then all binary digits and digit separators.
    void ScanBinaryDigits()
    {
        Advance(); // 0
        Advance(); // b or B
        while (!AtEnd() && (IsBinDigit(Peek()) || Peek() == '\''))
            Advance();
    }

    /// @brief Scans octal digit sequence after the leading '0'.
    ///
    /// Consumes the leading '0', then all octal digits and digit separators.
    void ScanOctalDigits()
    {
        Advance(); // 0
        while (!AtEnd() && (IsOctDigit(Peek()) || Peek() == '\''))
            Advance();
    }

    /// @brief Scans a decimal number including optional fractional and exponent parts.
    /// @param start The starting position of the numeric literal (used to validate fractional detection).
    ///
    /// Consumes decimal digits with digit separators, an optional fractional part (dot followed by digits),
    /// and an optional exponent part (e/E with optional sign and digits).
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
            // Might be a decimal point at end like 1.
            // Only consume if it looks numeric (not member access)
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

    /// @brief Scans integer and floating-point type suffixes.
    ///
    /// Consumes suffix characters: u, U, l, L, ll, LL, ul, UL, ull, ULL, f, F, z, Z.
    void ScanNumericSuffix()
    {
        while (!AtEnd() && (Peek() == 'u' || Peek() == 'U' || Peek() == 'l' || Peek() == 'L' || Peek() == 'f' ||
                            Peek() == 'F' || Peek() == 'z' || Peek() == 'Z'))
        {
            Advance();
        }
    }

    /// @brief Scans a complete numeric literal (integer or floating-point).
    /// @param loc The source location where the literal starts.
    /// @return The numeric literal token, or an error if the literal is malformed.
    ///
    /// Handles hexadecimal (0x), binary (0b), octal (0), and decimal literals,
    /// including fractional parts, exponents, and type suffixes.
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

    /// @brief Advances the scanner position by the given number of characters.
    /// @param count The number of characters to advance.
    void AdvanceBy(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            Advance();
    }

    /// @brief Attempts to match and consume a multi-character operator from a lookup table.
    /// @param table The sorted array of operator entries to search.
    /// @param length The number of characters to match (must be 2 or 3).
    /// @param loc The source location for the resulting token.
    /// @return The operator token if matched, or std::nullopt if no match was found.
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

    /// @brief Scans a single-character operator or punctuation token.
    /// @param ch The character that was already peeked at and advanced past.
    /// @param loc The source location for the resulting token.
    /// @return The single-character operator token.
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

    /// @brief Scans an operator or punctuation token.
    /// @param loc The source location where the operator starts.
    /// @return The operator token, or an error if the character is unrecognized.
    ///
    /// Tries three-character operators first, then two-character operators (both via
    /// binary search in sorted lookup tables), and finally falls back to single-character
    /// operator matching via a switch statement.
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

} // namespace

auto Tokenizer::Tokenize(std::string_view source, std::filesystem::path const& filePath)
    -> std::expected<std::vector<Token>, TokenizerError>
{
    Scanner scanner(source, filePath);
    return scanner.Tokenize();
}

auto Tokenizer::TokenizeFile(std::filesystem::path const& filePath, InputEncoding encoding)
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

} // namespace codedup
