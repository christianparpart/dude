// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/SourceLocation.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace codedup
{

/// @brief Enumeration of all token types for supported languages.
///
/// The first range covers tokens shared across languages (special tokens, literals,
/// identifiers, comments, preprocessor, C++ keywords, and operators). C#-specific
/// keywords and operators follow after the shared operator/punctuation section.
// NOLINTBEGIN(performance-enum-size)
enum class TokenType : uint16_t
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

    // Keywords shared by C++ and C# (alphabetical)
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

    // Operators and punctuation (shared)
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

    // =====================================================================
    // C#-specific keywords
    // =====================================================================
    CSharp_Abstract,
    CSharp_As,
    CSharp_Async,
    CSharp_Await,
    CSharp_Base,
    CSharp_Byte,
    CSharp_Checked,
    CSharp_Decimal,
    CSharp_Delegate,
    CSharp_Event,
    CSharp_Finally,
    CSharp_Fixed,
    CSharp_Foreach,
    CSharp_In,
    CSharp_Interface,
    CSharp_Internal,
    CSharp_Is,
    CSharp_Lock,
    CSharp_Nameof,
    CSharp_Null,
    CSharp_Object,
    CSharp_Out,
    CSharp_Params,
    CSharp_Partial,
    CSharp_Readonly,
    CSharp_Ref,
    CSharp_Sbyte,
    CSharp_Sealed,
    CSharp_Stackalloc,
    CSharp_String,
    CSharp_Typeof,
    CSharp_Uint,
    CSharp_Ulong,
    CSharp_Unchecked,
    CSharp_Unsafe,
    CSharp_Ushort,
    CSharp_Var,
    CSharp_Where,
    CSharp_Yield,

    // C# contextual keywords
    CSharp_Get,
    CSharp_Set,
    CSharp_Value,
    CSharp_When,
    CSharp_Init,
    CSharp_Record,
    CSharp_With,
    CSharp_And,
    CSharp_Or,
    CSharp_Not,

    // C#-specific operators
    CSharp_NullConditional,      // ?.
    CSharp_NullCoalescing,       // ??
    CSharp_NullCoalescingAssign, // ??=
    CSharp_Lambda,               // =>

    // =====================================================================
    // Python-specific keywords
    // =====================================================================
    Python_And,
    Python_As,
    Python_Assert,
    Python_Async,
    Python_Await,
    Python_Def,
    Python_Del,
    Python_Elif,
    Python_Except,
    Python_Finally,
    Python_From,
    Python_Global,
    Python_Import,
    Python_In,
    Python_Is,
    Python_Lambda,
    Python_None,
    Python_Nonlocal,
    Python_Not,
    Python_Or,
    Python_Pass,
    Python_Raise,
    Python_With,
    Python_Yield,
    Python_Match,
    Python_Case,
    Python_Type,

    // Python-specific operators
    Python_At,            // @
    Python_AtEqual,       // @=
    Python_StarStar,      // **
    Python_StarStarEqual, // **=
    Python_FloorDiv,      // //
    Python_FloorDivEqual, // //=
    Python_Walrus,        // :=
};
// NOLINTEND(performance-enum-size)

/// @brief A single lexical token from source code.
struct Token
{
    TokenType type = TokenType::Invalid; ///< The type of this token.
    std::string text;                    ///< The source text of this token.
    SourceLocation location;             ///< Location in the source file.
};

/// @brief Error information from the tokenizer.
struct TokenizerError
{
    std::string message;                 ///< Description of the error.
    SourceLocation location;             ///< Location where the error occurred.
    std::filesystem::path filePath = {}; ///< File path (stored here since SourceLocation uses file index).
};

/// @brief Returns a human-readable name for the given token type.
[[nodiscard]] CODEDUP_API auto TokenTypeName(TokenType type) -> std::string_view;

/// @brief Returns true if the token type is a C++ keyword.
[[nodiscard]] CODEDUP_API auto IsKeyword(TokenType type) -> bool;

/// @brief Returns true if the token type is a C#-specific keyword.
[[nodiscard]] CODEDUP_API auto IsCSharpKeyword(TokenType type) -> bool;

/// @brief Returns true if the token type is a Python-specific keyword.
[[nodiscard]] CODEDUP_API auto IsPythonKeyword(TokenType type) -> bool;

/// @brief Returns true if the token type is a comment (line or block).
[[nodiscard]] CODEDUP_API auto IsComment(TokenType type) -> bool;

/// @brief Returns true if the token type is a literal (numeric, string, or char).
[[nodiscard]] CODEDUP_API auto IsLiteral(TokenType type) -> bool;

/// @brief Returns true if the token type is an operator or punctuation.
[[nodiscard]] CODEDUP_API auto IsOperatorOrPunctuation(TokenType type) -> bool;

} // namespace codedup
