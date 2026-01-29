// SPDX-License-Identifier: Apache-2.0
#include <codedup/Tokenizer.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
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
    {"alignas", TokenType::Alignas},
    {"alignof", TokenType::Alignof},
    {"auto", TokenType::Auto},
    {"bool", TokenType::Bool},
    {"break", TokenType::Break},
    {"case", TokenType::Case},
    {"catch", TokenType::Catch},
    {"char", TokenType::Char},
    {"char16_t", TokenType::Char16T},
    {"char32_t", TokenType::Char32T},
    {"char8_t", TokenType::Char8T},
    {"class", TokenType::Class},
    {"co_await", TokenType::CoAwait},
    {"co_return", TokenType::CoReturn},
    {"co_yield", TokenType::CoYield},
    {"concept", TokenType::Concept},
    {"const", TokenType::Const},
    {"consteval", TokenType::Consteval},
    {"constexpr", TokenType::Constexpr},
    {"constinit", TokenType::Constinit},
    {"continue", TokenType::Continue},
    {"decltype", TokenType::Decltype},
    {"default", TokenType::Default},
    {"delete", TokenType::Delete},
    {"do", TokenType::Do},
    {"double", TokenType::Double},
    {"dynamic_cast", TokenType::DynamicCast},
    {"else", TokenType::Else},
    {"enum", TokenType::Enum},
    {"explicit", TokenType::Explicit},
    {"export", TokenType::Export},
    {"extern", TokenType::Extern},
    {"false", TokenType::False},
    {"float", TokenType::Float},
    {"for", TokenType::For},
    {"friend", TokenType::Friend},
    {"goto", TokenType::Goto},
    {"if", TokenType::If},
    {"inline", TokenType::Inline},
    {"int", TokenType::Int},
    {"long", TokenType::Long},
    {"mutable", TokenType::Mutable},
    {"namespace", TokenType::Namespace},
    {"new", TokenType::New},
    {"noexcept", TokenType::Noexcept},
    {"nullptr", TokenType::Nullptr},
    {"operator", TokenType::Operator},
    {"override", TokenType::Override},
    {"private", TokenType::Private},
    {"protected", TokenType::Protected},
    {"public", TokenType::Public},
    {"register", TokenType::Register},
    {"reinterpret_cast", TokenType::ReinterpretCast},
    {"requires", TokenType::Requires},
    {"return", TokenType::Return},
    {"short", TokenType::Short},
    {"signed", TokenType::Signed},
    {"sizeof", TokenType::Sizeof},
    {"static", TokenType::Static},
    {"static_assert", TokenType::StaticAssert},
    {"static_cast", TokenType::StaticCast},
    {"struct", TokenType::Struct},
    {"switch", TokenType::Switch},
    {"template", TokenType::Template},
    {"this", TokenType::This},
    {"thread_local", TokenType::ThreadLocal},
    {"throw", TokenType::Throw},
    {"true", TokenType::True},
    {"try", TokenType::Try},
    {"typedef", TokenType::Typedef},
    {"typeid", TokenType::Typeid},
    {"typename", TokenType::Typename},
    {"union", TokenType::Union},
    {"unsigned", TokenType::Unsigned},
    {"using", TokenType::Using},
    {"virtual", TokenType::Virtual},
    {"void", TokenType::Void},
    {"volatile", TokenType::Volatile},
    {"wchar_t", TokenType::WcharT},
    {"while", TokenType::While},
});

/// @brief Looks up a keyword by text using binary search.
[[nodiscard]] auto lookupKeyword(std::string_view text) -> TokenType
{
    auto const it = std::ranges::lower_bound(keywords, text, {}, &KeywordEntry::text);
    if (it != keywords.end() && it->text == text)
        return it->type;
    return TokenType::Identifier;
}

[[nodiscard]] auto isIdentifierStart(char ch) -> bool
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

[[nodiscard]] auto isIdentifierContinue(char ch) -> bool
{
    return isIdentifierStart(ch) || (ch >= '0' && ch <= '9');
}

[[nodiscard]] auto isDigit(char ch) -> bool
{
    return ch >= '0' && ch <= '9';
}

[[nodiscard]] auto isHexDigit(char ch) -> bool
{
    return isDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

[[nodiscard]] auto isBinDigit(char ch) -> bool
{
    return ch == '0' || ch == '1';
}

[[nodiscard]] auto isOctDigit(char ch) -> bool
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

    [[nodiscard]] auto tokenize() -> std::expected<std::vector<Token>, TokenizerError>
    {
        std::vector<Token> tokens;
        tokens.reserve(_source.size() / 4); // rough estimate

        while (!atEnd())
        {
            skipWhitespace();
            if (atEnd())
                break;

            auto token = nextToken();
            if (!token)
                return std::unexpected(std::move(token.error()));
            tokens.push_back(std::move(*token));
        }

        tokens.push_back(Token{.type = TokenType::EndOfFile, .text = {}, .location = currentLocation()});
        return tokens;
    }

private:
    std::string_view _source;
    std::filesystem::path _filePath;
    size_t _pos = 0;
    uint32_t _line = 1;
    uint32_t _column = 1;

    [[nodiscard]] auto atEnd() const -> bool { return _pos >= _source.size(); }

    [[nodiscard]] auto peek() const -> char
    {
        if (atEnd())
            return '\0';
        return _source[_pos];
    }

    [[nodiscard]] auto peekAt(size_t offset) const -> char
    {
        auto const idx = _pos + offset;
        if (idx >= _source.size())
            return '\0';
        return _source[idx];
    }

    auto advance() -> char
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

    void skipWhitespace()
    {
        while (!atEnd())
        {
            auto const ch = peek();
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                advance();
            else
                break;
        }
    }

    [[nodiscard]] auto currentLocation() const -> SourceLocation
    {
        return {.filePath = _filePath, .line = _line, .column = _column};
    }

    [[nodiscard]] auto nextToken() -> std::expected<Token, TokenizerError>
    {
        auto const loc = currentLocation();
        auto const ch = peek();

        // Preprocessor directives
        if (ch == '#' && (loc.column == 1 || isStartOfLine()))
            return scanPreprocessorDirective(loc);

        // Comments
        if (ch == '/' && peekAt(1) == '/')
            return scanLineComment(loc);
        if (ch == '/' && peekAt(1) == '*')
            return scanBlockComment(loc);

        // String and char literals
        if (ch == '"')
            return scanStringLiteral(loc);
        if (ch == '\'')
            return scanCharLiteral(loc);

        // Raw string literals: R"delim(...)delim"
        if (ch == 'R' && peekAt(1) == '"')
            return scanRawStringLiteral(loc);

        // Identifiers and keywords (also handles prefixed strings like u8"...", L'...')
        if (isIdentifierStart(ch))
        {
            // Check for string literal prefixes: u8, u, U, L
            if ((ch == 'u' || ch == 'U' || ch == 'L') && peekAt(1) == '"')
                return scanStringLiteral(loc, true);
            if ((ch == 'u' || ch == 'U' || ch == 'L') && peekAt(1) == '\'')
                return scanCharLiteral(loc, true);
            if (ch == 'u' && peekAt(1) == '8' && peekAt(2) == '"')
                return scanStringLiteral(loc, true);
            if (ch == 'u' && peekAt(1) == '8' && peekAt(2) == '\'')
                return scanCharLiteral(loc, true);
            // Check for prefixed raw strings: u8R"(...)", uR"(...)", etc.
            if ((ch == 'u' || ch == 'U' || ch == 'L') && peekAt(1) == 'R' && peekAt(2) == '"')
                return scanRawStringLiteral(loc, true);
            if (ch == 'u' && peekAt(1) == '8' && peekAt(2) == 'R' && peekAt(3) == '"')
                return scanRawStringLiteral(loc, true);

            return scanIdentifierOrKeyword(loc);
        }

        // Numeric literals
        if (isDigit(ch))
            return scanNumericLiteral(loc);

        // Dot can start a numeric literal (.5) or be an operator
        if (ch == '.' && isDigit(peekAt(1)))
            return scanNumericLiteral(loc);

        // Operators and punctuation
        return scanOperator(loc);
    }

    [[nodiscard]] auto isStartOfLine() const -> bool
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

    [[nodiscard]] auto scanPreprocessorDirective(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        advance(); // skip '#'

        // Read until end of line, handling backslash continuation
        while (!atEnd())
        {
            auto const c = peek();
            if (c == '\\' && peekAt(1) == '\n')
            {
                advance(); // backslash
                advance(); // newline
                continue;
            }
            if (c == '\n')
                break;
            advance();
        }

        return Token{.type = TokenType::PreprocessorDirective,
                     .text = std::string(_source.substr(start, _pos - start)),
                     .location = loc};
    }

    [[nodiscard]] auto scanLineComment(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        advance(); // first /
        advance(); // second /

        while (!atEnd() && peek() != '\n')
            advance();

        return Token{
            .type = TokenType::LineComment, .text = std::string(_source.substr(start, _pos - start)), .location = loc};
    }

    [[nodiscard]] auto scanBlockComment(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        advance(); // /
        advance(); // *

        while (!atEnd())
        {
            if (peek() == '*' && peekAt(1) == '/')
            {
                advance(); // *
                advance(); // /
                return Token{.type = TokenType::BlockComment,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated block comment", .location = loc});
    }

    [[nodiscard]] auto scanStringLiteral(SourceLocation const& loc, bool hasPrefix = false)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        // Skip prefix (u, U, L, u8)
        if (hasPrefix)
        {
            if (peek() == 'u' && peekAt(1) == '8')
            {
                advance();
                advance();
            }
            else
            {
                advance();
            }
        }

        advance(); // opening quote

        while (!atEnd())
        {
            auto const c = peek();
            if (c == '\\')
            {
                advance(); // backslash
                if (!atEnd())
                    advance(); // escaped char
                continue;
            }
            if (c == '"')
            {
                advance(); // closing quote
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            if (c == '\n')
                break; // Unterminated
            advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated string literal", .location = loc});
    }

    [[nodiscard]] auto scanRawStringLiteral(SourceLocation const& loc, bool hasPrefix = false)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        // Skip prefix
        if (hasPrefix)
        {
            if (peek() == 'u' && peekAt(1) == '8')
            {
                advance();
                advance();
            }
            else
            {
                advance(); // u, U, or L
            }
        }

        advance(); // R
        advance(); // opening "

        // Read delimiter (up to 16 chars before '(')
        std::string delimiter;
        while (!atEnd() && peek() != '(')
        {
            delimiter += advance();
            if (delimiter.size() > 16)
                return std::unexpected(TokenizerError{.message = "Raw string delimiter too long", .location = loc});
        }

        if (atEnd())
            return std::unexpected(TokenizerError{.message = "Unterminated raw string literal", .location = loc});

        advance(); // (

        // Build closing sequence: )delimiter"
        auto const closing = ")" + delimiter + "\"";

        while (!atEnd())
        {
            if (_pos + closing.size() <= _source.size() && _source.substr(_pos, closing.size()) == closing)
            {
                for (size_t i = 0; i < closing.size(); ++i)
                    advance();
                return Token{.type = TokenType::StringLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated raw string literal", .location = loc});
    }

    [[nodiscard]] auto scanCharLiteral(SourceLocation const& loc, bool hasPrefix = false)
        -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        if (hasPrefix)
        {
            if (peek() == 'u' && peekAt(1) == '8')
            {
                advance();
                advance();
            }
            else
            {
                advance();
            }
        }

        advance(); // opening quote

        while (!atEnd())
        {
            auto const c = peek();
            if (c == '\\')
            {
                advance();
                if (!atEnd())
                    advance();
                continue;
            }
            if (c == '\'')
            {
                advance(); // closing quote
                return Token{.type = TokenType::CharLiteral,
                             .text = std::string(_source.substr(start, _pos - start)),
                             .location = loc};
            }
            if (c == '\n')
                break;
            advance();
        }

        return std::unexpected(TokenizerError{.message = "Unterminated character literal", .location = loc});
    }

    [[nodiscard]] auto scanIdentifierOrKeyword(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;
        while (!atEnd() && isIdentifierContinue(peek()))
            advance();

        auto const text = _source.substr(start, _pos - start);
        auto const type = lookupKeyword(text);

        return Token{.type = type, .text = std::string(text), .location = loc};
    }

    [[nodiscard]] auto scanNumericLiteral(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const start = _pos;

        if (peek() == '0' && (peekAt(1) == 'x' || peekAt(1) == 'X'))
        {
            advance(); // 0
            advance(); // x
            while (!atEnd() && (isHexDigit(peek()) || peek() == '\''))
                advance();
        }
        else if (peek() == '0' && (peekAt(1) == 'b' || peekAt(1) == 'B'))
        {
            advance(); // 0
            advance(); // b
            while (!atEnd() && (isBinDigit(peek()) || peek() == '\''))
                advance();
        }
        else if (peek() == '0' && isOctDigit(peekAt(1)))
        {
            advance(); // 0
            while (!atEnd() && (isOctDigit(peek()) || peek() == '\''))
                advance();
        }
        else
        {
            // Decimal integer or floating-point
            while (!atEnd() && (isDigit(peek()) || peek() == '\''))
                advance();

            // Fractional part
            if (peek() == '.' && (isDigit(peekAt(1)) || peekAt(1) == 'e' || peekAt(1) == 'E'))
            {
                advance(); // .
                while (!atEnd() && (isDigit(peek()) || peek() == '\''))
                    advance();
            }
            else if (peek() == '.' && _pos > start && isDigit(_source[start]))
            {
                // Might be a decimal point at end like 1.
                // Only consume if it looks numeric (not member access)
                if (isDigit(peekAt(1)) || peekAt(1) == 'e' || peekAt(1) == 'E' || peekAt(1) == 'f' ||
                    peekAt(1) == 'F' || peekAt(1) == 'l' || peekAt(1) == 'L')
                {
                    advance(); // .
                    while (!atEnd() && (isDigit(peek()) || peek() == '\''))
                        advance();
                }
            }

            // Exponent part
            if (peek() == 'e' || peek() == 'E')
            {
                advance();
                if (peek() == '+' || peek() == '-')
                    advance();
                while (!atEnd() && (isDigit(peek()) || peek() == '\''))
                    advance();
            }
        }

        // Integer/float suffixes: u, U, l, L, ll, LL, ul, UL, ull, ULL, f, F, z, Z
        while (!atEnd() && (peek() == 'u' || peek() == 'U' || peek() == 'l' || peek() == 'L' || peek() == 'f' ||
                            peek() == 'F' || peek() == 'z' || peek() == 'Z'))
        {
            advance();
        }

        return Token{.type = TokenType::NumericLiteral,
                     .text = std::string(_source.substr(start, _pos - start)),
                     .location = loc};
    }

    [[nodiscard]] auto scanOperator(SourceLocation const& loc) -> std::expected<Token, TokenizerError>
    {
        auto const ch = peek();

        // Three-character operators
        if (_pos + 2 < _source.size())
        {
            auto const three = _source.substr(_pos, 3);
            if (three == "<=>")
            {
                advance();
                advance();
                advance();
                return Token{.type = TokenType::Spaceship, .text = "<=>", .location = loc};
            }
            if (three == "<<=")
            {
                advance();
                advance();
                advance();
                return Token{.type = TokenType::LessLessEqual, .text = "<<=", .location = loc};
            }
            if (three == ">>=")
            {
                advance();
                advance();
                advance();
                return Token{.type = TokenType::GreaterGreaterEqual, .text = ">>=", .location = loc};
            }
            if (three == "->*")
            {
                advance();
                advance();
                advance();
                return Token{.type = TokenType::ArrowStar, .text = "->*", .location = loc};
            }
            if (three == "...")
            {
                advance();
                advance();
                advance();
                return Token{.type = TokenType::Ellipsis, .text = "...", .location = loc};
            }
            if (three == "##" && false)
            {
            } // ##  is two chars, handled below
        }

        // Two-character operators
        if (_pos + 1 < _source.size())
        {
            auto const two = _source.substr(_pos, 2);
            if (two == "::")
            {
                advance();
                advance();
                return Token{.type = TokenType::ColonColon, .text = "::", .location = loc};
            }
            if (two == "->")
            {
                advance();
                advance();
                return Token{.type = TokenType::Arrow, .text = "->", .location = loc};
            }
            if (two == ".*")
            {
                advance();
                advance();
                return Token{.type = TokenType::DotStar, .text = ".*", .location = loc};
            }
            if (two == "++")
            {
                advance();
                advance();
                return Token{.type = TokenType::PlusPlus, .text = "++", .location = loc};
            }
            if (two == "--")
            {
                advance();
                advance();
                return Token{.type = TokenType::MinusMinus, .text = "--", .location = loc};
            }
            if (two == "+=")
            {
                advance();
                advance();
                return Token{.type = TokenType::PlusEqual, .text = "+=", .location = loc};
            }
            if (two == "-=")
            {
                advance();
                advance();
                return Token{.type = TokenType::MinusEqual, .text = "-=", .location = loc};
            }
            if (two == "*=")
            {
                advance();
                advance();
                return Token{.type = TokenType::StarEqual, .text = "*=", .location = loc};
            }
            if (two == "/=")
            {
                advance();
                advance();
                return Token{.type = TokenType::SlashEqual, .text = "/=", .location = loc};
            }
            if (two == "%=")
            {
                advance();
                advance();
                return Token{.type = TokenType::PercentEqual, .text = "%=", .location = loc};
            }
            if (two == "&&")
            {
                advance();
                advance();
                return Token{.type = TokenType::AmpAmp, .text = "&&", .location = loc};
            }
            if (two == "||")
            {
                advance();
                advance();
                return Token{.type = TokenType::PipePipe, .text = "||", .location = loc};
            }
            if (two == "&=")
            {
                advance();
                advance();
                return Token{.type = TokenType::AmpEqual, .text = "&=", .location = loc};
            }
            if (two == "|=")
            {
                advance();
                advance();
                return Token{.type = TokenType::PipeEqual, .text = "|=", .location = loc};
            }
            if (two == "^=")
            {
                advance();
                advance();
                return Token{.type = TokenType::CaretEqual, .text = "^=", .location = loc};
            }
            if (two == "<<")
            {
                advance();
                advance();
                return Token{.type = TokenType::LessLess, .text = "<<", .location = loc};
            }
            if (two == ">>")
            {
                advance();
                advance();
                return Token{.type = TokenType::GreaterGreater, .text = ">>", .location = loc};
            }
            if (two == "<=")
            {
                advance();
                advance();
                return Token{.type = TokenType::LessEqual, .text = "<=", .location = loc};
            }
            if (two == ">=")
            {
                advance();
                advance();
                return Token{.type = TokenType::GreaterEqual, .text = ">=", .location = loc};
            }
            if (two == "==")
            {
                advance();
                advance();
                return Token{.type = TokenType::EqualEqual, .text = "==", .location = loc};
            }
            if (two == "!=")
            {
                advance();
                advance();
                return Token{.type = TokenType::ExclaimEqual, .text = "!=", .location = loc};
            }
            if (two == "##")
            {
                advance();
                advance();
                return Token{.type = TokenType::HashHash, .text = "##", .location = loc};
            }
        }

        // Single-character operators
        advance();
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
};

} // namespace

auto Tokenizer::tokenize(std::string_view source, std::filesystem::path const& filePath)
    -> std::expected<std::vector<Token>, TokenizerError>
{
    Scanner scanner(source, filePath);
    return scanner.tokenize();
}

auto Tokenizer::tokenizeFile(std::filesystem::path const& filePath, InputEncoding encoding)
    -> std::expected<std::vector<Token>, TokenizerError>
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return std::unexpected(TokenizerError{.message = "Failed to open file: " + filePath.string(),
                                              .location = {.filePath = filePath, .line = 0, .column = 0}});

    std::ostringstream ss;
    ss << file.rdbuf();
    auto const rawContent = ss.str();

    auto utf8Result = convertToUtf8(rawContent, encoding);
    if (!utf8Result)
        return std::unexpected(TokenizerError{
            .message = std::format("Encoding error in {}: {}", filePath.string(), utf8Result.error().message),
            .location = {.filePath = filePath, .line = 0, .column = 0}});

    auto const& utf8Content = *utf8Result;
    return tokenize(utf8Content, filePath);
}

} // namespace codedup
