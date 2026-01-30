// SPDX-License-Identifier: Apache-2.0
#include <dude/Languages/CSharpLanguage.hpp>
#include <dude/MappedFile.hpp>
#include <dude/SimdCharClassifier.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace dude
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

/// @brief Sorted array of C# keywords for binary search lookup.
///
/// Includes both shared keywords (reusing C++ TokenType values where the keyword
/// text matches exactly) and C#-specific keywords with CSharp_ prefix.
constexpr auto keywords = std::to_array<KeywordEntry>({
    {.text = "abstract", .type = TokenType::CSharp_Abstract},
    {.text = "and", .type = TokenType::CSharp_And},
    {.text = "as", .type = TokenType::CSharp_As},
    {.text = "async", .type = TokenType::CSharp_Async},
    {.text = "await", .type = TokenType::CSharp_Await},
    {.text = "base", .type = TokenType::CSharp_Base},
    {.text = "bool", .type = TokenType::Bool},
    {.text = "break", .type = TokenType::Break},
    {.text = "byte", .type = TokenType::CSharp_Byte},
    {.text = "case", .type = TokenType::Case},
    {.text = "catch", .type = TokenType::Catch},
    {.text = "char", .type = TokenType::Char},
    {.text = "checked", .type = TokenType::CSharp_Checked},
    {.text = "class", .type = TokenType::Class},
    {.text = "const", .type = TokenType::Const},
    {.text = "continue", .type = TokenType::Continue},
    {.text = "decimal", .type = TokenType::CSharp_Decimal},
    {.text = "default", .type = TokenType::Default},
    {.text = "delegate", .type = TokenType::CSharp_Delegate},
    {.text = "do", .type = TokenType::Do},
    {.text = "double", .type = TokenType::Double},
    {.text = "else", .type = TokenType::Else},
    {.text = "enum", .type = TokenType::Enum},
    {.text = "event", .type = TokenType::CSharp_Event},
    {.text = "explicit", .type = TokenType::Explicit},
    {.text = "extern", .type = TokenType::Extern},
    {.text = "false", .type = TokenType::False},
    {.text = "finally", .type = TokenType::CSharp_Finally},
    {.text = "fixed", .type = TokenType::CSharp_Fixed},
    {.text = "float", .type = TokenType::Float},
    {.text = "for", .type = TokenType::For},
    {.text = "foreach", .type = TokenType::CSharp_Foreach},
    {.text = "get", .type = TokenType::CSharp_Get},
    {.text = "goto", .type = TokenType::Goto},
    {.text = "if", .type = TokenType::If},
    {.text = "in", .type = TokenType::CSharp_In},
    {.text = "init", .type = TokenType::CSharp_Init},
    {.text = "int", .type = TokenType::Int},
    {.text = "interface", .type = TokenType::CSharp_Interface},
    {.text = "internal", .type = TokenType::CSharp_Internal},
    {.text = "is", .type = TokenType::CSharp_Is},
    {.text = "lock", .type = TokenType::CSharp_Lock},
    {.text = "long", .type = TokenType::Long},
    {.text = "nameof", .type = TokenType::CSharp_Nameof},
    {.text = "namespace", .type = TokenType::Namespace},
    {.text = "new", .type = TokenType::New},
    {.text = "not", .type = TokenType::CSharp_Not},
    {.text = "null", .type = TokenType::CSharp_Null},
    {.text = "object", .type = TokenType::CSharp_Object},
    {.text = "operator", .type = TokenType::Operator},
    {.text = "or", .type = TokenType::CSharp_Or},
    {.text = "out", .type = TokenType::CSharp_Out},
    {.text = "override", .type = TokenType::Override},
    {.text = "params", .type = TokenType::CSharp_Params},
    {.text = "partial", .type = TokenType::CSharp_Partial},
    {.text = "private", .type = TokenType::Private},
    {.text = "protected", .type = TokenType::Protected},
    {.text = "public", .type = TokenType::Public},
    {.text = "readonly", .type = TokenType::CSharp_Readonly},
    {.text = "record", .type = TokenType::CSharp_Record},
    {.text = "ref", .type = TokenType::CSharp_Ref},
    {.text = "return", .type = TokenType::Return},
    {.text = "sbyte", .type = TokenType::CSharp_Sbyte},
    {.text = "sealed", .type = TokenType::CSharp_Sealed},
    {.text = "set", .type = TokenType::CSharp_Set},
    {.text = "short", .type = TokenType::Short},
    {.text = "sizeof", .type = TokenType::Sizeof},
    {.text = "stackalloc", .type = TokenType::CSharp_Stackalloc},
    {.text = "static", .type = TokenType::Static},
    {.text = "string", .type = TokenType::CSharp_String},
    {.text = "struct", .type = TokenType::Struct},
    {.text = "switch", .type = TokenType::Switch},
    {.text = "this", .type = TokenType::This},
    {.text = "throw", .type = TokenType::Throw},
    {.text = "true", .type = TokenType::True},
    {.text = "try", .type = TokenType::Try},
    {.text = "typedef", .type = TokenType::Typedef},
    {.text = "typeof", .type = TokenType::CSharp_Typeof},
    {.text = "uint", .type = TokenType::CSharp_Uint},
    {.text = "ulong", .type = TokenType::CSharp_Ulong},
    {.text = "unchecked", .type = TokenType::CSharp_Unchecked},
    {.text = "unsafe", .type = TokenType::CSharp_Unsafe},
    {.text = "ushort", .type = TokenType::CSharp_Ushort},
    {.text = "using", .type = TokenType::Using},
    {.text = "value", .type = TokenType::CSharp_Value},
    {.text = "var", .type = TokenType::CSharp_Var},
    {.text = "virtual", .type = TokenType::Virtual},
    {.text = "void", .type = TokenType::Void},
    {.text = "volatile", .type = TokenType::Volatile},
    {.text = "when", .type = TokenType::CSharp_When},
    {.text = "where", .type = TokenType::CSharp_Where},
    {.text = "while", .type = TokenType::While},
    {.text = "with", .type = TokenType::CSharp_With},
    {.text = "yield", .type = TokenType::CSharp_Yield},
});

/// @brief Entry mapping an operator string to its TokenType.
struct OperatorEntry
{
    std::string_view text;
    TokenType type;
};

/// @brief Sorted array of three-character operators for binary search lookup.
///
/// C# does not have ->*, <=>, or .* but does have ??= and <<=, >>=.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtrigraphs"
#endif
constexpr auto threeCharOperators = std::to_array<OperatorEntry>({
    {.text = "...", .type = TokenType::Ellipsis},
    {.text = "<<=", .type = TokenType::LessLessEqual},
    {.text = "??=", .type = TokenType::CSharp_NullCoalescingAssign},
    {.text = ">>=", .type = TokenType::GreaterGreaterEqual},
});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/// @brief Sorted array of two-character operators for binary search lookup.
///
/// C# uses ?. (null-conditional), ?? (null-coalescing), => (lambda) in addition
/// to standard operators. Does not include ::, ->, ->*, .*, or <=>.
constexpr auto twoCharOperators = std::to_array<OperatorEntry>({
    {.text = "!=", .type = TokenType::ExclaimEqual},
    {.text = "%=", .type = TokenType::PercentEqual},
    {.text = "&&", .type = TokenType::AmpAmp},
    {.text = "&=", .type = TokenType::AmpEqual},
    {.text = "*=", .type = TokenType::StarEqual},
    {.text = "++", .type = TokenType::PlusPlus},
    {.text = "+=", .type = TokenType::PlusEqual},
    {.text = "--", .type = TokenType::MinusMinus},
    {.text = "-=", .type = TokenType::MinusEqual},
    {.text = "/=", .type = TokenType::SlashEqual},
    {.text = "<<", .type = TokenType::LessLess},
    {.text = "<=", .type = TokenType::LessEqual},
    {.text = "==", .type = TokenType::EqualEqual},
    {.text = "=>", .type = TokenType::CSharp_Lambda},
    {.text = ">=", .type = TokenType::GreaterEqual},
    {.text = ">>", .type = TokenType::GreaterGreater},
    {.text = "?.", .type = TokenType::CSharp_NullConditional},
    {.text = "??", .type = TokenType::CSharp_NullCoalescing},
    {.text = "^=", .type = TokenType::CaretEqual},
    {.text = "|=", .type = TokenType::PipeEqual},
    {.text = "||", .type = TokenType::PipePipe},
});

/// @brief Looks up a keyword by text using binary search.
/// @param text The identifier text to look up.
/// @return The corresponding TokenType, or TokenType::Identifier if not a keyword.
[[nodiscard]] auto LookupKeyword(std::string_view text) -> TokenType
{
    auto const* const it = std::ranges::lower_bound(keywords, text, {}, &KeywordEntry::text);
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

/// @brief Returns true if the character can start a C# identifier.
[[nodiscard]] auto IsIdentifierStart(char ch) -> bool
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

/// @brief Returns true if the character can continue a C# identifier.
[[nodiscard]] auto IsIdentifierContinue(char ch) -> bool
{
    return IsIdentifierStart(ch) || (ch >= '0' && ch <= '9');
}

/// @brief Returns true if the character is a decimal digit.
[[nodiscard]] auto IsDigit(char ch) -> bool
{
    return ch >= '0' && ch <= '9';
}

/// @brief Returns true if the character is a valid C# numeric suffix character.
[[nodiscard]] auto IsNumericSuffix(char ch) -> bool
{
    return ch == 'u' || ch == 'U' || ch == 'l' || ch == 'L' || ch == 'f' || ch == 'F' || ch == 'd' || ch == 'D' ||
           ch == 'm' || ch == 'M';
}

/// @brief Scanner state for the C# tokenizer.
///
/// Implements a hand-written lexical scanner for C# source code, producing a
/// sequence of Token values. Handles C#-specific constructs including verbatim
/// strings (@"..."), interpolated strings ($"..."), raw string literals ("""..."""),
/// null-conditional (?.), null-coalescing (??/??=), lambda (=>) operators,
/// and numeric literals with underscore digit separators.
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

    /// @brief Skips whitespace characters (space, tab, carriage return, newline).
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

        // Preprocessor directives: #region, #if, #else, #endif, #define, #pragma, etc.
        if (ch == '#' && (loc.column == 1 || IsStartOfLine()))
            return ScanPreprocessorDirective(loc);

        // Comments: //, /* */, and XML doc ///
        if (ch == '/' && PeekAt(1) == '/')
            return ScanLineComment(loc);
        if (ch == '/' && PeekAt(1) == '*')
            return ScanBlockComment(loc);

        // Verbatim string literal: @"..."
        if (ch == '@' && PeekAt(1) == '"')
            return ScanVerbatimStringLiteral(loc);

        // Interpolated verbatim string literal: $@"..." or @$"..."
        if (ch == '$' && PeekAt(1) == '@' && PeekAt(2) == '"')
            return ScanVerbatimStringLiteral(loc, 2);
        if (ch == '@' && PeekAt(1) == '$' && PeekAt(2) == '"')
            return ScanVerbatimStringLiteral(loc, 2);

        // Interpolated string literal: $"..."
        if (ch == '$' && PeekAt(1) == '"')
            return ScanInterpolatedStringLiteral(loc);

        // Raw string literals: """..."""
        if (ch == '"' && PeekAt(1) == '"' && PeekAt(2) == '"')
            return ScanRawStringLiteral(loc);

        // Regular string literals: "..."
        if (ch == '"')
            return ScanStringLiteral(loc);

        // Character literals: '...'
        if (ch == '\'')
            return ScanCharLiteral(loc);

        // Identifiers and keywords (@ prefix for verbatim identifiers)
        if (IsIdentifierStart(ch))
            return ScanIdentifierOrKeyword(loc);
        if (ch == '@' && PeekAt(1) != '"' && IsIdentifierStart(PeekAt(1)))
            return ScanVerbatimIdentifier(loc);

        // Numeric literals
        if (IsDigit(ch))
            return ScanNumericLiteral(loc);

        // Dot can start a numeric literal (.5) or be an operator
        if (ch == '.' && IsDigit(PeekAt(1)))
            return ScanNumericLiteral(loc);

        // Operators and punctuation
        return ScanOperator(loc);
    }

    /// @brief Returns true if the current position is at the start of a line (only whitespace before it).
    [[nodiscard]] auto IsStartOfLine() const -> bool
    {
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

    /// @brief Scans a preprocessor directive (#region, #if, #else, #endif, #define, #pragma, etc.).
    /// @param loc The source location at the start of the directive.
    /// @return The preprocessor directive token.
    [[nodiscard]] auto ScanPreprocessorDirective(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // skip '#'

        // Read until end of line (C# preprocessor directives do not support backslash continuation)
        while (!AtEnd())
        {
            auto const c = Peek();
            if (c == '\n')
                break;
            Advance();
        }

        return Token{.type = TokenType::PreprocessorDirective,
                     .text = std::string(_source.substr(start, _pos - start)),
                     .location = loc};
    }

    /// @brief Scans a line comment (// or /// XML doc comment).
    /// @param loc The source location at the start of the comment.
    /// @return The line comment token.
    [[nodiscard]] auto ScanLineComment(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // first /
        Advance(); // second /

        // Consume the rest of the line (includes /// XML doc comments)
        while (!AtEnd() && Peek() != '\n')
            Advance();

        return Token{
            .type = TokenType::LineComment, .text = std::string(_source.substr(start, _pos - start)), .location = loc};
    }

    /// @brief Scans a block comment (/* ... */).
    /// @param loc The source location at the start of the comment.
    /// @return The block comment token, or an error if unterminated.
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

    /// @brief Scans a regular string literal ("...") with backslash escapes.
    /// @param loc The source location at the start of the literal.
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanStringLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // opening "

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
                Advance(); // closing "
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

    /// @brief Scans a verbatim string literal (@"..." with "" for escaped quotes).
    /// @param loc The source location at the start of the literal.
    /// @param prefixLen Number of prefix characters to skip before the opening quote (1 for @", 2 for $@" or @$").
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanVerbatimStringLiteral(SourceLocation const& loc, size_t prefixLen = 1)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        // Skip prefix characters (@ or $@ or @$)
        for (size_t i = 0; i < prefixLen; ++i)
            Advance();
        Advance(); // opening "

        while (!AtEnd())
        {
            auto const c = Peek();
            if (c == '"')
            {
                Advance(); // consume "
                // Doubled quote "" is an escape for a literal quote
                if (!AtEnd() && Peek() == '"')
                {
                    Advance(); // consume second "
                    continue;
                }
                // Single " ends the verbatim string
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated verbatim string literal", .location = loc});
    }

    /// @brief Scans an interpolated string literal ($"..."), tokenizing just the outer string.
    /// @param loc The source location at the start of the literal.
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanInterpolatedStringLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // $
        Advance(); // opening "

        // For clone detection purposes, we tokenize the entire interpolated string as one token.
        // We track brace depth to handle embedded expressions like $"value is {expr}".
        size_t braceDepth = 0;

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
            if (c == '{')
            {
                if (PeekAt(1) == '{')
                {
                    // Escaped brace {{ is a literal {
                    Advance();
                    Advance();
                    continue;
                }
                ++braceDepth;
                Advance();
                continue;
            }
            if (c == '}')
            {
                if (PeekAt(1) == '}')
                {
                    // Escaped brace }} is a literal }
                    Advance();
                    Advance();
                    continue;
                }
                if (braceDepth > 0)
                    --braceDepth;
                Advance();
                continue;
            }
            if (c == '"' && braceDepth == 0)
            {
                Advance(); // closing "
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            if (c == '\n' && braceDepth == 0)
                break; // Unterminated
            Advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated interpolated string literal", .location = loc});
    }

    /// @brief Scans a C# raw string literal ("""...""" with optional more quotes).
    ///
    /// Raw string literals are delimited by three or more quote characters.
    /// The closing delimiter must have the same number of quotes as the opening.
    /// @param loc The source location at the start of the literal.
    /// @return The string literal token, or an error if unterminated.
    [[nodiscard]] auto ScanRawStringLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        // Count opening quotes (at least 3)
        size_t quoteCount = 0;
        while (!AtEnd() && Peek() == '"')
        {
            ++quoteCount;
            Advance();
        }

        // Find the closing sequence: the same number of consecutive quotes
        size_t consecutiveQuotes = 0;

        while (!AtEnd())
        {
            if (Peek() == '"')
            {
                ++consecutiveQuotes;
                Advance();
                if (consecutiveQuotes == quoteCount)
                {
                    return Token{.type = TokenType::StringLiteral,
                                 .text = std::string(_source.substr(start, _pos - start)),
                                 .location = loc};
                }
            }
            else
            {
                consecutiveQuotes = 0;
                Advance();
            }
        }

        return std::unexpected(TokenizerError{.message = "Unterminated raw string literal", .location = loc});
    }

    /// @brief Scans a character literal ('x', '\n', etc.).
    /// @param loc The source location at the start of the literal.
    /// @return The character literal token, or an error if unterminated.
    [[nodiscard]] auto ScanCharLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // opening '

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
            if (c == '\'')
            {
                Advance(); // closing '
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

    /// @brief Scans a verbatim identifier (@identifier, allowing keywords as identifiers).
    /// @param loc The source location at the start of the verbatim identifier.
    /// @return The identifier token (always TokenType::Identifier, never a keyword).
    [[nodiscard]] auto ScanVerbatimIdentifier(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        Advance(); // skip @

        while (!AtEnd() && IsIdentifierContinue(Peek()))
            Advance();

        auto const text = _source.substr(start, _pos - start);
        // Verbatim identifiers are always identifiers, never keywords
        return Token{.type = TokenType::Identifier, .text = std::string(text), .location = loc};
    }

    /// @brief Scans hexadecimal digits (after 0x/0X prefix) with underscore separators.
    void ScanHexDigits()
    {
        Advance(); // 0
        Advance(); // x or X
        auto const count = SimdCharClassifier::ScanHexDigits(_source, _pos, '_');
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
    /// @param start The starting position of the numeric literal for context.
    void ScanDecimalWithFractionalAndExponent(size_t start)
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
        else if (Peek() == '.' && _pos > start && IsDigit(_source[start]))
        {
            if (IsDigit(PeekAt(1)) || PeekAt(1) == 'e' || PeekAt(1) == 'E' || PeekAt(1) == 'f' || PeekAt(1) == 'F' ||
                PeekAt(1) == 'd' || PeekAt(1) == 'D' || PeekAt(1) == 'm' || PeekAt(1) == 'M')
            {
                Advance(); // .
                {
                    auto const n = SimdCharClassifier::ScanDecimalDigits(_source, _pos, '_');
                    AdvanceBy(n);
                }
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
    }

    /// @brief Scans numeric suffixes (m/M, d/D, f/F, u/U, l/L, and combinations like UL, LU).
    void ScanNumericSuffix()
    {
        while (!AtEnd() && IsNumericSuffix(Peek()))
            Advance();
    }

    /// @brief Scans a numeric literal (integer, floating-point, hex, binary).
    /// @param loc The source location at the start of the literal.
    /// @return The numeric literal token.
    [[nodiscard]] auto ScanNumericLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        if (Peek() == '0' && (PeekAt(1) == 'x' || PeekAt(1) == 'X'))
            ScanHexDigits();
        else if (Peek() == '0' && (PeekAt(1) == 'b' || PeekAt(1) == 'B'))
            ScanBinaryDigits();
        else
            ScanDecimalWithFractionalAndExponent(start);

        ScanNumericSuffix();

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
            case '?':
                return Token{.type = TokenType::Question, .text = "?", .location = loc};
            case '#':
                return Token{.type = TokenType::Hash, .text = "#", .location = loc};
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
// Block extraction internals
// =====================================================================

/// @brief Result of a backward delimiter-matching operation.
struct ParenMatchResult
{
    size_t pos;   ///< The position after matching.
    bool success; ///< Whether matching succeeded.
};

/// @brief Matches a paired delimiter backward through the token stream.
/// @param tokens The token span to search.
/// @param scanPos The starting position (at the closing delimiter).
/// @param openType The opening delimiter token type.
/// @param closeType The closing delimiter token type.
/// @return The position of the opening delimiter and whether the match succeeded.
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

/// @brief Skips a where clause backward from the given position.
///
/// C# generic constraints look like: where T : IComparable, ICloneable
/// This scans backward past such clauses to find the actual method signature.
/// @param tokens The token span to search.
/// @param pos The starting position (before the opening brace).
/// @return The position before the where clause, or unchanged if none found.
[[nodiscard]] auto SkipWhereClausesBackward(std::span<Token const> tokens, size_t pos) -> size_t
{
    // Walk backward looking for 'where' keywords followed by constraint clauses.
    // A where clause looks like: where T : constraint1, constraint2
    // There can be multiple where clauses chained.
    auto scanPos = pos;

    while (scanPos > 0)
    {
        // Look for a 'where' keyword further back
        auto wherePos = scanPos;
        bool foundWhere = false;

        // Scan backward past identifiers, commas, colons, dots, less/greater (generics in constraints)
        while (wherePos > 0)
        {
            auto const type = tokens[wherePos].type;

            if (type == TokenType::CSharp_Where)
            {
                foundWhere = true;
                break;
            }

            // Tokens that can appear in a where constraint clause
            if (type == TokenType::Identifier || type == TokenType::Comma || type == TokenType::Colon ||
                type == TokenType::Dot || type == TokenType::New || type == TokenType::Class ||
                type == TokenType::Struct || type == TokenType::CSharp_Null || type == TokenType::CSharp_Unchecked ||
                type == TokenType::LeftParen || type == TokenType::RightParen)
            {
                --wherePos;
                continue;
            }

            // Skip generic parameters in constraints like IComparable<T>
            if (type == TokenType::Greater)
            {
                size_t angleDepth = 1;
                if (wherePos == 0)
                    break;
                --wherePos;
                while (wherePos > 0 && angleDepth > 0)
                {
                    if (tokens[wherePos].type == TokenType::Greater)
                        ++angleDepth;
                    else if (tokens[wherePos].type == TokenType::Less)
                        --angleDepth;
                    if (angleDepth > 0)
                        --wherePos;
                }
                if (angleDepth != 0)
                    break;
                if (wherePos > 0)
                    --wherePos;
                continue;
            }

            break;
        }

        if (!foundWhere)
            break;

        // Found a where keyword, skip past it
        scanPos = wherePos;
        if (scanPos > 0)
            --scanPos;
        else
            break;
    }

    return scanPos;
}

/// @brief Skips constructor chaining (: base(...) or : this(...)) backward.
///
/// In C#, constructors can chain to base or this constructors:
///   public MyClass(int x) : base(x) { }
///   public MyClass() : this(0) { }
/// @param tokens The token span to search.
/// @param pos The starting position.
/// @return The position before the constructor chaining clause, or unchanged.
[[nodiscard]] auto SkipConstructorChainingBackward(std::span<Token const> tokens, size_t pos) -> size_t
{
    // Pattern: : base(...) or : this(...)
    // At pos, we might see RightParen from the base/this call
    if (tokens[pos].type != TokenType::RightParen)
        return pos;

    auto const [parenPos, parenOk] =
        SkipMatchedDelimiterBackward(tokens, pos, TokenType::LeftParen, TokenType::RightParen);
    if (!parenOk || parenPos == 0)
        return pos;

    // Check that before the parens we have base or this
    auto const keywordPos = parenPos - 1;
    if (tokens[keywordPos].type != TokenType::CSharp_Base && tokens[keywordPos].type != TokenType::This)
        return pos;

    // Check for the colon before base/this
    if (keywordPos == 0)
        return pos;
    auto const colonPos = keywordPos - 1;
    if (tokens[colonPos].type != TokenType::Colon)
        return pos;

    // Successfully skipped constructor chaining
    if (colonPos > 0)
        return colonPos - 1;
    return 0;
}

/// @brief Matches parentheses backward from a closing paren.
/// @param tokens The token span to search.
/// @param pos The position of the closing parenthesis.
/// @return The position of the opening parenthesis and whether the match succeeded.
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

/// @brief Skips generic type parameters (<T, U>) backward from a closing angle bracket.
/// @param tokens The token span to search.
/// @param pos The current position (expected to be at '>').
/// @return The position before the generic parameters, or unchanged if no match.
[[nodiscard]] auto SkipGenericParamsBackward(std::span<Token const> tokens, size_t pos) -> size_t
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
    if (angleDepth != 0)
        return pos;
    if (pos > 0)
        --pos;
    return pos;
}

/// @brief Extracts the function/method identifier from the token at the given position.
/// @param tokens The token span to search.
/// @param pos The position expected to contain the identifier.
/// @return The function name string, or empty if no valid identifier found.
[[nodiscard]] auto ExtractFunctionIdentifier(std::span<Token const> tokens, size_t pos) -> std::string
{
    if (tokens[pos].type == TokenType::Identifier)
    {
        auto name = tokens[pos].text;
        // C# uses dot for namespace access, but method names are simple identifiers.
        // Check for ClassName.MethodName patterns (explicit interface implementation).
        if (pos >= 2 && tokens[pos - 1].type == TokenType::Dot && tokens[pos - 2].type == TokenType::Identifier)
        {
            name = tokens[pos - 2].text + "." + name;
        }
        return name;
    }
    if (tokens[pos].type == TokenType::Operator)
    {
        return "operator";
    }
    return {};
}

/// @brief Returns true if the keyword at namePos indicates a non-function block.
///
/// Rejects blocks that are class, struct, enum, namespace, or interface bodies.
/// @param tokens The token span to search.
/// @param namePos The position of the name token.
/// @return True if the block should be rejected (not a function/method).
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
            type == TokenType::CSharp_Interface)
        {
            // If the keyword is immediately or nearly immediately before the name, it's a type definition
            if (checkPos + 1 == namePos || checkPos + 2 == namePos)
                return true;
            return false;
        }
        if (type == TokenType::Semicolon || type == TokenType::RightBrace || type == TokenType::LeftBrace)
            return false;
    }
    return false;
}

/// @brief Returns true if the token at the given position is a property accessor keyword (get/set/init).
/// @param tokens The token span to examine.
/// @param pos The position to check.
/// @return True if this is a property accessor that should be included.
[[nodiscard]] auto IsPropertyAccessor(std::span<Token const> tokens, size_t pos) -> bool
{
    auto const type = tokens[pos].type;
    return type == TokenType::CSharp_Get || type == TokenType::CSharp_Set || type == TokenType::CSharp_Init;
}

/// @brief Finds the function/method name for a block starting at the given brace index.
///
/// Performs a backward scan from the opening brace '{' to determine the method name.
/// The heuristic:
///   1. Skip where T : constraint clauses
///   2. Skip constructor chaining : base(args) / : this(args)
///   3. Match parameter list (...) backward
///   4. Skip generic parameters <...> backward
///   5. Extract method/property identifier
///   6. Reject class, struct, enum, namespace, interface bodies
///   7. Include property accessors with bodies (get/set blocks with logic)
/// @param tokens The full token vector.
/// @param braceIndex The index of the opening brace.
/// @return The method name, or empty string if the block is not a function/method.
[[nodiscard]] auto FindFunctionName(std::vector<Token> const& tokens, size_t braceIndex) -> std::string
{
    if (braceIndex == 0)
        return {};

    auto const tokenSpan = std::span<Token const>{tokens};

    auto pos = braceIndex - 1;

    // Step 7: Check for property accessor (get { ... }, set { ... }, init { ... })
    if (IsPropertyAccessor(tokenSpan, pos))
    {
        // This is a property accessor body. Include it as a named block.
        return tokens[pos].text;
    }

    // Step 1: Skip where constraint clauses
    pos = SkipWhereClausesBackward(tokenSpan, pos);

    // Step 2: Skip constructor chaining (: base(...) or : this(...))
    pos = SkipConstructorChainingBackward(tokenSpan, pos);

    // Step 3: Match parameter list (...) backward
    auto const [parenPos, parenSuccess] = SkipMatchedParensBackward(tokenSpan, pos);
    if (!parenSuccess)
        return {};
    pos = parenPos - 1;

    // Step 4: Skip generic parameters <...> backward
    pos = SkipGenericParamsBackward(tokenSpan, pos);

    // Step 5: Extract method/property identifier
    auto name = ExtractFunctionIdentifier(tokenSpan, pos);
    if (name.empty())
        return {};

    // Step 6: Reject class, struct, enum, namespace, interface bodies
    if (IsNonFunctionKeyword(tokenSpan, pos))
        return {};

    return name;
}

/// @brief Extracts code blocks from C# tokens, building index maps for normalized tokens.
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
// CSharpLanguage public API
// =====================================================================

auto CSharpLanguage::Name() const -> std::string_view
{
    return "C#";
}

auto CSharpLanguage::Extensions() const -> std::span<std::string const>
{
    static std::string const exts[] = {".cs"};
    return exts;
}

auto CSharpLanguage::Tokenize(std::string_view source, uint32_t fileIndex) const
    -> std::expected<std::vector<Token>, TokenizerError>
{
    Scanner scanner(source, fileIndex);
    return scanner.Tokenize();
}

auto CSharpLanguage::TokenizeFile(std::filesystem::path const& filePath, uint32_t fileIndex,
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

auto CSharpLanguage::ExtractBlocks(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                   std::vector<NormalizedToken> const& textPreserving,
                                   CodeBlockExtractorConfig const& config) const -> std::vector<CodeBlock>
{
    return ExtractBlocksImpl(tokens, normalized, textPreserving, config);
}

auto CSharpLanguage::ShouldStripToken(TokenType type) const -> bool
{
    return IsComment(type) || type == TokenType::PreprocessorDirective || type == TokenType::EndOfFile;
}

auto CreateCSharpLanguage() -> std::shared_ptr<Language>
{
    return std::make_shared<CSharpLanguage>();
}

} // namespace dude
