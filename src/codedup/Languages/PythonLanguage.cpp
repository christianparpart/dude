// SPDX-License-Identifier: Apache-2.0
#include <codedup/Languages/PythonLanguage.hpp>
#include <codedup/MappedFile.hpp>
#include <codedup/SimdCharClassifier.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace codedup
{

namespace
{

// =====================================================================
// Tokenizer internals
// =====================================================================

/// @brief Entry mapping a keyword string to its TokenType.
struct KeywordEntry
{
    std::string_view text;
    TokenType type;
};

/// @brief Sorted array of Python keywords for binary search lookup.
///
/// Includes both shared keywords (reusing C++ TokenType values where the keyword
/// text matches exactly) and Python-specific keywords with Python_ prefix.
constexpr auto keywords = std::to_array<KeywordEntry>({
    {.text = "False", .type = TokenType::False},
    {.text = "None", .type = TokenType::Python_None},
    {.text = "True", .type = TokenType::True},
    {.text = "and", .type = TokenType::Python_And},
    {.text = "as", .type = TokenType::Python_As},
    {.text = "assert", .type = TokenType::Python_Assert},
    {.text = "async", .type = TokenType::Python_Async},
    {.text = "await", .type = TokenType::Python_Await},
    {.text = "break", .type = TokenType::Break},
    {.text = "case", .type = TokenType::Python_Case},
    {.text = "class", .type = TokenType::Class},
    {.text = "continue", .type = TokenType::Continue},
    {.text = "def", .type = TokenType::Python_Def},
    {.text = "del", .type = TokenType::Python_Del},
    {.text = "elif", .type = TokenType::Python_Elif},
    {.text = "else", .type = TokenType::Else},
    {.text = "except", .type = TokenType::Python_Except},
    {.text = "finally", .type = TokenType::Python_Finally},
    {.text = "for", .type = TokenType::For},
    {.text = "from", .type = TokenType::Python_From},
    {.text = "global", .type = TokenType::Python_Global},
    {.text = "if", .type = TokenType::If},
    {.text = "import", .type = TokenType::Python_Import},
    {.text = "in", .type = TokenType::Python_In},
    {.text = "is", .type = TokenType::Python_Is},
    {.text = "lambda", .type = TokenType::Python_Lambda},
    {.text = "match", .type = TokenType::Python_Match},
    {.text = "nonlocal", .type = TokenType::Python_Nonlocal},
    {.text = "not", .type = TokenType::Python_Not},
    {.text = "or", .type = TokenType::Python_Or},
    {.text = "pass", .type = TokenType::Python_Pass},
    {.text = "raise", .type = TokenType::Python_Raise},
    {.text = "return", .type = TokenType::Return},
    {.text = "try", .type = TokenType::Try},
    {.text = "type", .type = TokenType::Python_Type},
    {.text = "while", .type = TokenType::While},
    {.text = "with", .type = TokenType::Python_With},
    {.text = "yield", .type = TokenType::Python_Yield},
});

/// @brief Entry mapping an operator string to its TokenType.
struct OperatorEntry
{
    std::string_view text;
    TokenType type;
};

/// @brief Sorted array of three-character operators for binary search lookup.
constexpr auto threeCharOperators = std::to_array<OperatorEntry>({
    {.text = "**=", .type = TokenType::Python_StarStarEqual},
    {.text = "...", .type = TokenType::Ellipsis},
    {.text = "//=", .type = TokenType::Python_FloorDivEqual},
    {.text = "<<=", .type = TokenType::LessLessEqual},
    {.text = ">>=", .type = TokenType::GreaterGreaterEqual},
});

/// @brief Sorted array of two-character operators for binary search lookup.
constexpr auto twoCharOperators = std::to_array<OperatorEntry>({
    {.text = "!=", .type = TokenType::ExclaimEqual},    {.text = "%=", .type = TokenType::PercentEqual},
    {.text = "&=", .type = TokenType::AmpEqual},        {.text = "**", .type = TokenType::Python_StarStar},
    {.text = "*=", .type = TokenType::StarEqual},       {.text = "+=", .type = TokenType::PlusEqual},
    {.text = "-=", .type = TokenType::MinusEqual},      {.text = "->", .type = TokenType::Arrow},
    {.text = "//", .type = TokenType::Python_FloorDiv}, {.text = "/=", .type = TokenType::SlashEqual},
    {.text = ":=", .type = TokenType::Python_Walrus},   {.text = "<<", .type = TokenType::LessLess},
    {.text = "<=", .type = TokenType::LessEqual},       {.text = "==", .type = TokenType::EqualEqual},
    {.text = ">=", .type = TokenType::GreaterEqual},    {.text = ">>", .type = TokenType::GreaterGreater},
    {.text = "@=", .type = TokenType::Python_AtEqual},  {.text = "^=", .type = TokenType::CaretEqual},
    {.text = "|=", .type = TokenType::PipeEqual},
});

/// @brief Looks up a keyword by text using binary search.
/// @param text The identifier text to look up.
/// @return The corresponding TokenType, or TokenType::Identifier if not a keyword.
[[nodiscard]] auto LookupKeyword(std::string_view text) -> TokenType
{
    auto const it = std::ranges::lower_bound(keywords, text, {}, &KeywordEntry::text);
    if (it != keywords.end() && it->text == text)
        return it->type;
    return TokenType::Identifier;
}

/// @brief Looks up a multi-character operator by text using binary search.
/// @param table The operator table to search.
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

/// @brief Returns true if the character can start a Python identifier.
[[nodiscard]] auto IsIdentifierStart(char ch) -> bool
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

/// @brief Returns true if the character is a decimal digit.
[[nodiscard]] auto IsDigit(char ch) -> bool
{
    return ch >= '0' && ch <= '9';
}

/// @brief Returns true if the character is a Python string prefix character.
[[nodiscard]] auto IsStringPrefix(char ch) -> bool
{
    return ch == 'r' || ch == 'R' || ch == 'b' || ch == 'B' || ch == 'f' || ch == 'F' || ch == 'u' || ch == 'U';
}

/// @brief Scanner state for the Python tokenizer.
///
/// Implements a hand-written lexical scanner for Python source code, producing a
/// sequence of Token values. Handles Python-specific constructs including triple-quoted
/// strings, f-strings, raw strings, byte strings, hash comments, floor division,
/// power operator, walrus operator, and decorator syntax.
class Scanner
{
public:
    /// @brief Constructs a scanner for the given source code.
    /// @param source The source text to tokenize.
    /// @param fileIndex The file index for source location tracking.
    Scanner(std::string_view source, uint32_t fileIndex) : _source(source), _fileIndex(fileIndex) {}

    /// @brief Tokenizes the entire source into a vector of tokens.
    /// @return The token vector on success, or a TokenizerError on failure.
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
    uint32_t _fileIndex;
    size_t _pos = 0;
    uint32_t _line = 1;
    uint32_t _column = 1;

    /// @brief Returns true if the scanner has reached the end of input.
    [[nodiscard]] auto AtEnd() const -> bool { return _pos >= _source.size(); }

    /// @brief Returns the current character without advancing.
    [[nodiscard]] auto Peek() const -> char
    {
        if (AtEnd())
            return '\0';
        return _source[_pos];
    }

    /// @brief Returns the character at the given offset from the current position.
    [[nodiscard]] auto PeekAt(size_t offset) const -> char
    {
        auto const idx = _pos + offset;
        if (idx >= _source.size())
            return '\0';
        return _source[idx];
    }

    /// @brief Advances the scanner by one character, tracking line/column.
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

    /// @brief Advances the scanner by a given number of characters, tracking line/column.
    /// @param count The number of characters to advance.
    void AdvanceBy(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
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
        }
    }

    /// @brief Skips whitespace characters (space, tab, carriage return) but NOT newlines.
    ///
    /// Python newlines are significant for indentation tracking. However, for the tokenizer
    /// we skip all whitespace including newlines since we track line/column in SourceLocation.
    void SkipWhitespace()
    {
        auto const count = SimdCharClassifier::ScanWhitespace(_source, _pos);
        AdvanceBy(count);
    }

    /// @brief Returns the current source location.
    [[nodiscard]] auto CurrentLocation() const -> SourceLocation
    {
        return {.fileIndex = _fileIndex, .line = _line, .column = _column};
    }

    /// @brief Dispatches to the appropriate scanning function for the next token.
    [[nodiscard]] auto NextToken() -> std::expected<Token, TokenizerError>
    {
        auto const loc = CurrentLocation();
        auto const ch = Peek();

        // Comments: # line comments only
        if (ch == '#')
            return ScanLineComment(loc);

        // String literals with prefix: r"...", b"...", f"...", u"...", rb"...", br"...", etc.
        if (IsStringPrefix(ch))
        {
            auto prefixLen = ScanStringPrefixLength();
            if (prefixLen > 0)
            {
                auto const quoteChar = PeekAt(prefixLen);
                if (quoteChar == '"' || quoteChar == '\'')
                    return ScanPrefixedString(loc, prefixLen);
            }
            // Not a string prefix, fall through to identifier/keyword
        }

        // Triple-quoted strings: """...""" or '''...'''
        if ((ch == '"' && PeekAt(1) == '"' && PeekAt(2) == '"') ||
            (ch == '\'' && PeekAt(1) == '\'' && PeekAt(2) == '\''))
            return ScanTripleQuotedString(loc);

        // Regular string literals: "..." or '...'
        if (ch == '"' || ch == '\'')
            return ScanStringLiteral(loc);

        // Identifiers and keywords
        if (IsIdentifierStart(ch))
            return ScanIdentifierOrKeyword(loc);

        // Numeric literals
        if (IsDigit(ch))
            return ScanNumericLiteral(loc);

        // Dot can start a numeric literal (.5) or be an operator
        if (ch == '.' && IsDigit(PeekAt(1)))
            return ScanNumericLiteral(loc);

        // Operators and punctuation
        return ScanOperator(loc);
    }

    /// @brief Determines the length of a string prefix (r, b, f, u, rb, br, rf, fr, etc.).
    /// @return The prefix length (0 if not a valid string prefix).
    [[nodiscard]] auto ScanStringPrefixLength() const -> size_t
    {
        auto const c0 = Peek();
        auto const c1 = PeekAt(1);
        auto const c2 = PeekAt(2);

        // Two-character prefixes: rb, br, rf, fr, Rb, bR, etc.
        if ((c0 == 'r' || c0 == 'R') && (c1 == 'b' || c1 == 'B') && (c2 == '"' || c2 == '\''))
            return 2;
        if ((c0 == 'b' || c0 == 'B') && (c1 == 'r' || c1 == 'R') && (c2 == '"' || c2 == '\''))
            return 2;
        if ((c0 == 'r' || c0 == 'R') && (c1 == 'f' || c1 == 'F') && (c2 == '"' || c2 == '\''))
            return 2;
        if ((c0 == 'f' || c0 == 'F') && (c1 == 'r' || c1 == 'R') && (c2 == '"' || c2 == '\''))
            return 2;

        // Single-character prefixes
        if (IsStringPrefix(c0) && (c1 == '"' || c1 == '\''))
            return 1;

        return 0;
    }

    /// @brief Scans a line comment (# ...).
    /// @param loc The source location at the start of the comment.
    /// @return The line comment token.
    [[nodiscard]] auto ScanLineComment(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // #

        while (!AtEnd() && Peek() != '\n')
            Advance();

        return Token{
            .type = TokenType::LineComment, .text = std::string(_source.substr(start, _pos - start)), .location = loc};
    }

    /// @brief Scans a string literal with a prefix (r, b, f, u, rb, br, rf, fr).
    /// @param loc The source location at the start of the literal.
    /// @param prefixLen The number of prefix characters.
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanPrefixedString(SourceLocation const& loc, size_t prefixLen)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        for (size_t i = 0; i < prefixLen; ++i)
            Advance();

        auto const quoteChar = Peek();

        // Check for triple-quoted prefixed string
        if (PeekAt(0) == quoteChar && PeekAt(1) == quoteChar && PeekAt(2) == quoteChar)
        {
            // Triple-quoted prefixed string
            Advance(); // first quote
            Advance(); // second quote
            Advance(); // third quote

            while (!AtEnd())
            {
                if (Peek() == quoteChar && PeekAt(1) == quoteChar && PeekAt(2) == quoteChar)
                {
                    Advance();
                    Advance();
                    Advance();
                    return Token{.type = TokenType::StringLiteral,
                                 .text = std::string(_source.substr(start, _pos - start)),
                                 .location = loc};
                }
                if (Peek() == '\\')
                {
                    Advance();
                    if (!AtEnd())
                        Advance();
                    continue;
                }
                Advance();
            }

            return std::unexpected(
                TokenizerError{.message = "Unterminated triple-quoted string literal", .location = loc});
        }

        // Single-quoted prefixed string
        return ScanStringLiteral(loc, start);
    }

    /// @brief Scans a triple-quoted string literal ("""...""" or '''...''').
    /// @param loc The source location at the start of the literal.
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanTripleQuotedString(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        auto const quoteChar = Peek();
        Advance(); // first quote
        Advance(); // second quote
        Advance(); // third quote

        while (!AtEnd())
        {
            if (Peek() == quoteChar && PeekAt(1) == quoteChar && PeekAt(2) == quoteChar)
            {
                Advance();
                Advance();
                Advance();
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            if (Peek() == '\\')
            {
                Advance();
                if (!AtEnd())
                    Advance();
                continue;
            }
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated triple-quoted string literal", .location = loc});
    }

    /// @brief Scans a regular string literal ("..." or '...') with backslash escapes.
    /// @param loc The source location at the start of the literal.
    /// @param startOverride Optional start position override (for prefixed strings).
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanStringLiteral(SourceLocation const& loc, size_t startOverride = SIZE_MAX)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = (startOverride != SIZE_MAX) ? startOverride : _pos;
        auto const quoteChar = Peek();
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
            if (c == quoteChar)
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

    /// @brief Scans an identifier or keyword.
    /// @param loc The source location at the start of the identifier.
    /// @return The identifier or keyword token.
    [[nodiscard]] auto ScanIdentifierOrKeyword(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        auto const count = SimdCharClassifier::ScanIdentifierContinue(_source, _pos);
        AdvanceBy(count);

        auto const text = _source.substr(start, _pos - start);
        auto const type = LookupKeyword(text);

        return Token{.type = type, .text = std::string(text), .location = loc};
    }

    /// @brief Scans hexadecimal digits (after 0x/0X prefix) with underscore separators.
    void ScanHexDigits()
    {
        Advance(); // 0
        Advance(); // x or X
        auto const count = SimdCharClassifier::ScanHexDigits(_source, _pos, '_');
        AdvanceBy(count);
    }

    /// @brief Scans octal digits (after 0o/0O prefix) with underscore separators.
    void ScanOctalDigits()
    {
        Advance(); // 0
        Advance(); // o or O
        auto const count = SimdCharClassifier::ScanOctalDigits(_source, _pos, '_');
        AdvanceBy(count);
    }

    /// @brief Scans binary digits (after 0b/0B prefix) with underscore separators.
    void ScanBinaryDigits()
    {
        Advance(); // 0
        Advance(); // b or B
        auto const count = SimdCharClassifier::ScanBinaryDigits(_source, _pos, '_');
        AdvanceBy(count);
    }

    /// @brief Scans decimal digits with optional fractional part and exponent.
    void ScanDecimalWithFractionalAndExponent()
    {
        // Decimal integer digits (with _ as digit separator)
        {
            auto const n = SimdCharClassifier::ScanDecimalDigits(_source, _pos, '_');
            AdvanceBy(n);
        }

        // Fractional part
        if (Peek() == '.' && (IsDigit(PeekAt(1)) || PeekAt(1) == 'e' || PeekAt(1) == 'E'))
        {
            Advance(); // .
            {
                auto const n = SimdCharClassifier::ScanDecimalDigits(_source, _pos, '_');
                AdvanceBy(n);
            }
        }

        // Exponent part
        if (Peek() == 'e' || Peek() == 'E')
        {
            Advance();
            if (Peek() == '+' || Peek() == '-')
                Advance();
            {
                auto const n = SimdCharClassifier::ScanDecimalDigits(_source, _pos, '_');
                AdvanceBy(n);
            }
        }

        // Complex suffix (j or J)
        if (Peek() == 'j' || Peek() == 'J')
            Advance();
    }

    /// @brief Scans a numeric literal (integer, floating-point, hex, octal, binary, complex).
    /// @param loc The source location at the start of the literal.
    /// @return The numeric literal token.
    [[nodiscard]] auto ScanNumericLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        if (Peek() == '0' && (PeekAt(1) == 'x' || PeekAt(1) == 'X'))
            ScanHexDigits();
        else if (Peek() == '0' && (PeekAt(1) == 'o' || PeekAt(1) == 'O'))
            ScanOctalDigits();
        else if (Peek() == '0' && (PeekAt(1) == 'b' || PeekAt(1) == 'B'))
            ScanBinaryDigits();
        else
            ScanDecimalWithFractionalAndExponent();

        return Token{.type = TokenType::NumericLiteral,
                     .text = std::string(_source.substr(start, _pos - start)),
                     .location = loc};
    }

    /// @brief Tries to match a multi-character operator from a table.
    /// @param table The operator table to match against.
    /// @param length The expected operator length.
    /// @param loc The source location for the resulting token.
    /// @return The matched operator token, or std::nullopt if no match.
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

    /// @brief Maps a single character to its corresponding operator/punctuation TokenType.
    /// @param ch The character to map.
    /// @param loc The source location for the token.
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
            case '@':
                return Token{.type = TokenType::Python_At, .text = "@", .location = loc};
            default:
                return Token{.type = TokenType::Invalid, .text = std::string(1, ch), .location = loc};
        }
    }

    /// @brief Scans an operator or punctuation token.
    ///
    /// Tries three-character operators first, then two-character, then single-character.
    /// @param loc The source location at the start of the operator.
    /// @return The operator token.
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
// Block extraction internals (indentation-based)
// =====================================================================

/// @brief Computes the leading indentation level of a given line in the source.
/// @param token The token to check.
/// @return The indentation column (1-based).
[[nodiscard]] auto GetLineIndent(Token const& token) -> uint32_t
{
    return token.location.column;
}

/// @brief Finds the colon that ends a Python function signature.
///
/// Scans forward from the given start index, tracking parenthesis depth to handle
/// parameter lists, and returns the index of the colon at depth 0.
/// @param tokens The token span to search.
/// @param start The index to start scanning from.
/// @return The index of the colon, or SIZE_MAX if not found.
[[nodiscard]] auto FindSignatureColon(std::span<Token const> tokens, size_t start) -> size_t
{
    size_t parenDepth = 0;
    for (auto idx = start; idx < tokens.size(); ++idx)
    {
        if (tokens[idx].type == TokenType::LeftParen)
            ++parenDepth;
        else if (tokens[idx].type == TokenType::RightParen && parenDepth > 0)
            --parenDepth;
        else if (tokens[idx].type == TokenType::Colon && parenDepth == 0)
            return idx;
    }
    return SIZE_MAX;
}

/// @brief Finds the first token of a function body by skipping comments/EOF.
/// @param tokens The token span to search.
/// @param start The index to start scanning from.
/// @return The index of the first body token, or SIZE_MAX if none found.
[[nodiscard]] auto FindBodyStart(std::span<Token const> tokens, size_t start) -> size_t
{
    auto idx = start;
    while (idx < tokens.size() &&
           (tokens[idx].type == TokenType::LineComment || tokens[idx].type == TokenType::EndOfFile))
    {
        ++idx;
    }
    if (idx >= tokens.size() || tokens[idx].type == TokenType::EndOfFile)
        return SIZE_MAX;
    return idx;
}

/// @brief Finds the end of a Python function body using indentation tracking.
/// @param tokens The token span to search.
/// @param bodyStart The index of the first body token.
/// @param defColumn The indentation column of the enclosing 'def' keyword.
/// @return The exclusive end index of the body.
[[nodiscard]] auto FindBodyEnd(std::span<Token const> tokens, size_t bodyStart, uint32_t defColumn) -> size_t
{
    auto bodyEnd = bodyStart;
    auto currentLine = tokens[bodyStart].location.line;

    for (size_t j = bodyStart; j < tokens.size(); ++j)
    {
        if (tokens[j].type == TokenType::EndOfFile)
            return j;

        auto const tokenLine = tokens[j].location.line;
        auto const tokenCol = GetLineIndent(tokens[j]);

        if (tokenLine > currentLine && tokenCol <= defColumn)
            return j;

        bodyEnd = j + 1;
        currentLine = tokenLine;
    }
    return bodyEnd;
}

/// @brief Collects normalized token IDs for a range of original tokens.
/// @param origToNorm Mapping from original token index to normalized index.
/// @param normalized The normalized token sequence.
/// @param start Start of the range (inclusive).
/// @param end End of the range (exclusive).
/// @return A vector of normalized token IDs.
[[nodiscard]] auto CollectNormalizedIds(std::span<size_t const> origToNorm, std::span<NormalizedToken const> normalized,
                                        size_t start, size_t end) -> std::vector<NormalizedTokenId>
{
    std::vector<NormalizedTokenId> ids;
    for (size_t ti = start; ti < end; ++ti)
    {
        if (origToNorm[ti] != SIZE_MAX)
        {
            auto const normIdx = origToNorm[ti];
            if (normIdx < normalized.size())
                ids.push_back(normalized[normIdx].id);
        }
    }
    return ids;
}

/// @brief Extracts code blocks from Python tokens using indentation-based block detection.
///
/// Scans for `def` keywords, extracts function names, then determines the function
/// body extent by tracking indentation levels.
/// @param tokens The original token sequence.
/// @param normalized The structural normalized token sequence.
/// @param textPreserving The text-preserving normalized token sequence.
/// @param config Configuration for block extraction.
/// @return A vector of extracted code blocks.
[[nodiscard]] auto ExtractBlocksImpl(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                     std::vector<NormalizedToken> const& textPreserving,
                                     CodeBlockExtractorConfig const& config) -> std::vector<CodeBlock>
{
    std::vector<CodeBlock> blocks;

    // Build a mapping from original token index to normalized token index.
    std::vector<size_t> origToNormalized(tokens.size(), SIZE_MAX);
    for (auto const ni : std::views::iota(size_t{0}, normalized.size()))
        origToNormalized[normalized[ni].originalIndex] = ni;

    // Build the same mapping for text-preserving normalization.
    std::vector<size_t> origToTextPreserving;
    if (!textPreserving.empty())
    {
        origToTextPreserving.assign(tokens.size(), SIZE_MAX);
        for (auto const ni : std::views::iota(size_t{0}, textPreserving.size()))
            origToTextPreserving[textPreserving[ni].originalIndex] = ni;
    }

    auto const tokenSpan = std::span<Token const>{tokens};

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type != TokenType::Python_Def)
            continue;

        // For 'async def', use the async token's column as the base indentation
        auto defColumn = GetLineIndent(tokens[i]);
        if (i > 0 && tokens[i - 1].type == TokenType::Python_Async &&
            tokens[i - 1].location.line == tokens[i].location.line)
        {
            defColumn = GetLineIndent(tokens[i - 1]);
        }

        // Extract function name: the identifier immediately after 'def'
        if (i + 1 >= tokens.size() || tokens[i + 1].type != TokenType::Identifier)
            continue;

        auto funcName = tokens[i + 1].text;

        // Find the colon that ends the signature
        auto const colonIndex = FindSignatureColon(tokenSpan, i + 2);
        if (colonIndex == SIZE_MAX)
            continue;

        // Find the first body token
        auto const bodyStart = FindBodyStart(tokenSpan, colonIndex + 1);
        if (bodyStart == SIZE_MAX)
            continue;

        // Verify the body is indented beyond the def
        if (GetLineIndent(tokens[bodyStart]) <= defColumn)
            continue;

        auto const bodyEnd = FindBodyEnd(tokenSpan, bodyStart, defColumn);

        // Collect normalized IDs
        auto normalizedIds = CollectNormalizedIds(origToNormalized, normalized, bodyStart, bodyEnd);
        auto textPreservingIds = textPreserving.empty()
                                     ? std::vector<NormalizedTokenId>{}
                                     : CollectNormalizedIds(origToTextPreserving, textPreserving, bodyStart, bodyEnd);

        if (normalizedIds.size() < config.minTokens)
            continue;

        auto const& startLoc = tokens[bodyStart].location;
        auto const& endLoc = tokens[bodyEnd > 0 ? bodyEnd - 1 : bodyEnd].location;

        blocks.push_back(CodeBlock{
            .name = std::move(funcName),
            .sourceRange = SourceRange{.start = startLoc, .end = endLoc},
            .tokenStart = bodyStart,
            .tokenEnd = bodyEnd,
            .normalizedIds = std::move(normalizedIds),
            .textPreservingIds = std::move(textPreservingIds),
        });

        i = bodyEnd > 0 ? bodyEnd - 1 : bodyEnd;
    }

    return blocks;
}

} // anonymous namespace

// =====================================================================
// PythonLanguage public API
// =====================================================================

auto PythonLanguage::Name() const -> std::string_view
{
    return "Python";
}

auto PythonLanguage::Extensions() const -> std::span<std::string const>
{
    static std::string const exts[] = {".py", ".pyw"};
    return exts;
}

auto PythonLanguage::Tokenize(std::string_view source, uint32_t fileIndex) const
    -> std::expected<std::vector<Token>, TokenizerError>
{
    Scanner scanner(source, fileIndex);
    return scanner.Tokenize();
}

auto PythonLanguage::TokenizeFile(std::filesystem::path const& filePath, uint32_t fileIndex,
                                  InputEncoding encoding) const -> std::expected<std::vector<Token>, TokenizerError>
{
    auto mappedFile = MappedFile::Open(filePath);
    if (!mappedFile)
        return std::unexpected(TokenizerError{.message = mappedFile.error(),
                                              .location = {.fileIndex = fileIndex, .line = 0, .column = 0},
                                              .filePath = filePath});

    auto const rawContent = mappedFile->View();

    auto utf8Result = ConvertToUtf8(rawContent, encoding);
    if (!utf8Result)
        return std::unexpected(TokenizerError{
            .message = std::format("Encoding error in {}: {}", filePath.string(), utf8Result.error().message),
            .location = {.fileIndex = fileIndex, .line = 0, .column = 0},
            .filePath = filePath});

    auto const& utf8Content = *utf8Result;
    return Tokenize(utf8Content, fileIndex);
}

auto PythonLanguage::ExtractBlocks(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                   std::vector<NormalizedToken> const& textPreserving,
                                   CodeBlockExtractorConfig const& config) const -> std::vector<CodeBlock>
{
    return ExtractBlocksImpl(tokens, normalized, textPreserving, config);
}

auto PythonLanguage::ShouldStripToken(TokenType type) const -> bool
{
    return IsComment(type) || type == TokenType::EndOfFile;
}

auto CreatePythonLanguage() -> std::shared_ptr<Language>
{
    return std::make_shared<PythonLanguage>();
}

} // namespace codedup
