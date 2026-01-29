// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/SourceLocation.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace codedup
{

/// @brief Enumeration of all C++ token types.
enum class TokenType : uint8_t
{
    // Special
    EndOfFile = 0,
    Invalid,

    // Literals
    NumericLiteral,
    StringLiteral,
    CharLiteral,

    // Identifiers
    Identifier,

    // Comments
    LineComment,
    BlockComment,

    // Preprocessor
    PreprocessorDirective,

    // Keywords (alphabetical)
    Alignas,
    Alignof,
    Auto,
    Bool,
    Break,
    Case,
    Catch,
    Char,
    Char8T,
    Char16T,
    Char32T,
    Class,
    CoAwait,
    CoReturn,
    CoYield,
    Concept,
    Const,
    Consteval,
    Constexpr,
    Constinit,
    Continue,
    Decltype,
    Default,
    Delete,
    Do,
    Double,
    DynamicCast,
    Else,
    Enum,
    Explicit,
    Export,
    Extern,
    False,
    Float,
    For,
    Friend,
    Goto,
    If,
    Inline,
    Int,
    Long,
    Mutable,
    Namespace,
    New,
    Noexcept,
    Nullptr,
    Operator,
    Override,
    Private,
    Protected,
    Public,
    Register,
    ReinterpretCast,
    Requires,
    Return,
    Short,
    Signed,
    Sizeof,
    Static,
    StaticAssert,
    StaticCast,
    Struct,
    Switch,
    Template,
    This,
    ThreadLocal,
    Throw,
    True,
    Try,
    Typedef,
    Typeid,
    Typename,
    Union,
    Unsigned,
    Using,
    Virtual,
    Void,
    Volatile,
    WcharT,
    While,

    // Operators and punctuation
    LeftParen,           // (
    RightParen,          // )
    LeftBracket,         // [
    RightBracket,        // ]
    LeftBrace,           // {
    RightBrace,          // }
    Semicolon,           // ;
    Colon,               // :
    ColonColon,          // ::
    Comma,               // ,
    Dot,                 // .
    DotStar,             // .*
    Ellipsis,            // ...
    Arrow,               // ->
    ArrowStar,           // ->*
    Tilde,               // ~
    Exclaim,             // !
    ExclaimEqual,        // !=
    Plus,                // +
    PlusPlus,            // ++
    PlusEqual,           // +=
    Minus,               // -
    MinusMinus,          // --
    MinusEqual,          // -=
    Star,                // *
    StarEqual,           // *=
    Slash,               // /
    SlashEqual,          // /=
    Percent,             // %
    PercentEqual,        // %=
    Amp,                 // &
    AmpAmp,              // &&
    AmpEqual,            // &=
    Pipe,                // |
    PipePipe,            // ||
    PipeEqual,           // |=
    Caret,               // ^
    CaretEqual,          // ^=
    Less,                // <
    LessLess,            // <<
    LessLessEqual,       // <<=
    LessEqual,           // <=
    Spaceship,           // <=>
    Greater,             // >
    GreaterGreater,      // >>
    GreaterGreaterEqual, // >>=
    GreaterEqual,        // >=
    Equal,               // =
    EqualEqual,          // ==
    Question,            // ?
    Hash,                // #
    HashHash,            // ##
};

/// @brief A single lexical token from C++ source code.
struct Token
{
    TokenType type = TokenType::Invalid; ///< The type of this token.
    std::string text;                    ///< The source text of this token.
    SourceLocation location;             ///< Location in the source file.
};

/// @brief Returns a human-readable name for the given token type.
[[nodiscard]] CODEDUP_API auto TokenTypeName(TokenType type) -> std::string_view;

/// @brief Returns true if the token type is a C++ keyword.
[[nodiscard]] CODEDUP_API auto IsKeyword(TokenType type) -> bool;

/// @brief Returns true if the token type is a comment (line or block).
[[nodiscard]] CODEDUP_API auto IsComment(TokenType type) -> bool;

/// @brief Returns true if the token type is a literal (numeric, string, or char).
[[nodiscard]] CODEDUP_API auto IsLiteral(TokenType type) -> bool;

/// @brief Returns true if the token type is an operator or punctuation.
[[nodiscard]] CODEDUP_API auto IsOperatorOrPunctuation(TokenType type) -> bool;

} // namespace codedup
