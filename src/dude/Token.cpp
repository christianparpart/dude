// SPDX-License-Identifier: Apache-2.0
#include <dude/Token.hpp>

#include <utility>

namespace dude
{

auto TokenTypeName(TokenType type) -> std::string_view
{
    switch (type)
    {
        case TokenType::EndOfFile:
            return "EndOfFile";
        case TokenType::Invalid:
            return "Invalid";
        case TokenType::NumericLiteral:
            return "NumericLiteral";
        case TokenType::StringLiteral:
            return "StringLiteral";
        case TokenType::CharLiteral:
            return "CharLiteral";
        case TokenType::Identifier:
            return "Identifier";
        case TokenType::LineComment:
            return "LineComment";
        case TokenType::BlockComment:
            return "BlockComment";
        case TokenType::PreprocessorDirective:
            return "PreprocessorDirective";
        case TokenType::Alignas:
            return "alignas";
        case TokenType::Alignof:
            return "alignof";
        case TokenType::Auto:
            return "auto";
        case TokenType::Bool:
            return "bool";
        case TokenType::Break:
            return "break";
        case TokenType::Case:
            return "case";
        case TokenType::Catch:
            return "catch";
        case TokenType::Char:
            return "char";
        case TokenType::Char8T:
            return "char8_t";
        case TokenType::Char16T:
            return "char16_t";
        case TokenType::Char32T:
            return "char32_t";
        case TokenType::Class:
            return "class";
        case TokenType::CoAwait:
            return "co_await";
        case TokenType::CoReturn:
            return "co_return";
        case TokenType::CoYield:
            return "co_yield";
        case TokenType::Concept:
            return "concept";
        case TokenType::Const:
            return "const";
        case TokenType::Consteval:
            return "consteval";
        case TokenType::Constexpr:
            return "constexpr";
        case TokenType::Constinit:
            return "constinit";
        case TokenType::Continue:
            return "continue";
        case TokenType::Decltype:
            return "decltype";
        case TokenType::Default:
            return "default";
        case TokenType::Delete:
            return "delete";
        case TokenType::Do:
            return "do";
        case TokenType::Double:
            return "double";
        case TokenType::DynamicCast:
            return "dynamic_cast";
        case TokenType::Else:
            return "else";
        case TokenType::Enum:
            return "enum";
        case TokenType::Explicit:
            return "explicit";
        case TokenType::Export:
            return "export";
        case TokenType::Extern:
            return "extern";
        case TokenType::False:
            return "false";
        case TokenType::Float:
            return "float";
        case TokenType::For:
            return "for";
        case TokenType::Friend:
            return "friend";
        case TokenType::Goto:
            return "goto";
        case TokenType::If:
            return "if";
        case TokenType::Inline:
            return "inline";
        case TokenType::Int:
            return "int";
        case TokenType::Long:
            return "long";
        case TokenType::Mutable:
            return "mutable";
        case TokenType::Namespace:
            return "namespace";
        case TokenType::New:
            return "new";
        case TokenType::Noexcept:
            return "noexcept";
        case TokenType::Nullptr:
            return "nullptr";
        case TokenType::Operator:
            return "operator";
        case TokenType::Override:
            return "override";
        case TokenType::Private:
            return "private";
        case TokenType::Protected:
            return "protected";
        case TokenType::Public:
            return "public";
        case TokenType::Register:
            return "register";
        case TokenType::ReinterpretCast:
            return "reinterpret_cast";
        case TokenType::Requires:
            return "requires";
        case TokenType::Return:
            return "return";
        case TokenType::Short:
            return "short";
        case TokenType::Signed:
            return "signed";
        case TokenType::Sizeof:
            return "sizeof";
        case TokenType::Static:
            return "static";
        case TokenType::StaticAssert:
            return "static_assert";
        case TokenType::StaticCast:
            return "static_cast";
        case TokenType::Struct:
            return "struct";
        case TokenType::Switch:
            return "switch";
        case TokenType::Template:
            return "template";
        case TokenType::This:
            return "this";
        case TokenType::ThreadLocal:
            return "thread_local";
        case TokenType::Throw:
            return "throw";
        case TokenType::True:
            return "true";
        case TokenType::Try:
            return "try";
        case TokenType::Typedef:
            return "typedef";
        case TokenType::Typeid:
            return "typeid";
        case TokenType::Typename:
            return "typename";
        case TokenType::Union:
            return "union";
        case TokenType::Unsigned:
            return "unsigned";
        case TokenType::Using:
            return "using";
        case TokenType::Virtual:
            return "virtual";
        case TokenType::Void:
            return "void";
        case TokenType::Volatile:
            return "volatile";
        case TokenType::WcharT:
            return "wchar_t";
        case TokenType::While:
            return "while";
        case TokenType::LeftParen:
            return "(";
        case TokenType::RightParen:
            return ")";
        case TokenType::LeftBracket:
            return "[";
        case TokenType::RightBracket:
            return "]";
        case TokenType::LeftBrace:
            return "{";
        case TokenType::RightBrace:
            return "}";
        case TokenType::Semicolon:
            return ";";
        case TokenType::Colon:
            return ":";
        case TokenType::ColonColon:
            return "::";
        case TokenType::Comma:
            return ",";
        case TokenType::Dot:
            return ".";
        case TokenType::DotStar:
            return ".*";
        case TokenType::Ellipsis:
            return "...";
        case TokenType::Arrow:
            return "->";
        case TokenType::ArrowStar:
            return "->*";
        case TokenType::Tilde:
            return "~";
        case TokenType::Exclaim:
            return "!";
        case TokenType::ExclaimEqual:
            return "!=";
        case TokenType::Plus:
            return "+";
        case TokenType::PlusPlus:
            return "++";
        case TokenType::PlusEqual:
            return "+=";
        case TokenType::Minus:
            return "-";
        case TokenType::MinusMinus:
            return "--";
        case TokenType::MinusEqual:
            return "-=";
        case TokenType::Star:
            return "*";
        case TokenType::StarEqual:
            return "*=";
        case TokenType::Slash:
            return "/";
        case TokenType::SlashEqual:
            return "/=";
        case TokenType::Percent:
            return "%";
        case TokenType::PercentEqual:
            return "%=";
        case TokenType::Amp:
            return "&";
        case TokenType::AmpAmp:
            return "&&";
        case TokenType::AmpEqual:
            return "&=";
        case TokenType::Pipe:
            return "|";
        case TokenType::PipePipe:
            return "||";
        case TokenType::PipeEqual:
            return "|=";
        case TokenType::Caret:
            return "^";
        case TokenType::CaretEqual:
            return "^=";
        case TokenType::Less:
            return "<";
        case TokenType::LessLess:
            return "<<";
        case TokenType::LessLessEqual:
            return "<<=";
        case TokenType::LessEqual:
            return "<=";
        case TokenType::Spaceship:
            return "<=>";
        case TokenType::Greater:
            return ">";
        case TokenType::GreaterGreater:
            return ">>";
        case TokenType::GreaterGreaterEqual:
            return ">>=";
        case TokenType::GreaterEqual:
            return ">=";
        case TokenType::Equal:
            return "=";
        case TokenType::EqualEqual:
            return "==";
        case TokenType::Question:
            return "?";
        case TokenType::Hash:
            return "#";
        case TokenType::HashHash:
            return "##";
        // C#-specific keywords
        case TokenType::CSharp_Abstract:
            return "abstract";
        case TokenType::CSharp_As:
            return "as";
        case TokenType::CSharp_Async:
            return "async";
        case TokenType::CSharp_Await:
            return "await";
        case TokenType::CSharp_Base:
            return "base";
        case TokenType::CSharp_Byte:
            return "byte";
        case TokenType::CSharp_Checked:
            return "checked";
        case TokenType::CSharp_Decimal:
            return "decimal";
        case TokenType::CSharp_Delegate:
            return "delegate";
        case TokenType::CSharp_Event:
            return "event";
        case TokenType::CSharp_Finally:
            return "finally";
        case TokenType::CSharp_Fixed:
            return "fixed";
        case TokenType::CSharp_Foreach:
            return "foreach";
        case TokenType::CSharp_In:
            return "in";
        case TokenType::CSharp_Interface:
            return "interface";
        case TokenType::CSharp_Internal:
            return "internal";
        case TokenType::CSharp_Is:
            return "is";
        case TokenType::CSharp_Lock:
            return "lock";
        case TokenType::CSharp_Nameof:
            return "nameof";
        case TokenType::CSharp_Null:
            return "null";
        case TokenType::CSharp_Object:
            return "object";
        case TokenType::CSharp_Out:
            return "out";
        case TokenType::CSharp_Params:
            return "params";
        case TokenType::CSharp_Partial:
            return "partial";
        case TokenType::CSharp_Readonly:
            return "readonly";
        case TokenType::CSharp_Ref:
            return "ref";
        case TokenType::CSharp_Sbyte:
            return "sbyte";
        case TokenType::CSharp_Sealed:
            return "sealed";
        case TokenType::CSharp_Stackalloc:
            return "stackalloc";
        case TokenType::CSharp_String:
            return "string";
        case TokenType::CSharp_Typeof:
            return "typeof";
        case TokenType::CSharp_Uint:
            return "uint";
        case TokenType::CSharp_Ulong:
            return "ulong";
        case TokenType::CSharp_Unchecked:
            return "unchecked";
        case TokenType::CSharp_Unsafe:
            return "unsafe";
        case TokenType::CSharp_Ushort:
            return "ushort";
        case TokenType::CSharp_Var:
            return "var";
        case TokenType::CSharp_Where:
            return "where";
        case TokenType::CSharp_Yield:
            return "yield";
        // C# contextual keywords
        case TokenType::CSharp_Get:
            return "get";
        case TokenType::CSharp_Set:
            return "set";
        case TokenType::CSharp_Value:
            return "value";
        case TokenType::CSharp_When:
            return "when";
        case TokenType::CSharp_Init:
            return "init";
        case TokenType::CSharp_Record:
            return "record";
        case TokenType::CSharp_With:
            return "with";
        case TokenType::CSharp_And:
            return "and";
        case TokenType::CSharp_Or:
            return "or";
        case TokenType::CSharp_Not:
            return "not";
        // C#-specific operators
        case TokenType::CSharp_NullConditional:
            return "?.";
        case TokenType::CSharp_NullCoalescing:
            return "??";
        case TokenType::CSharp_NullCoalescingAssign:
            return "\?\?="; // ??=
        case TokenType::CSharp_Lambda:
            return "=>";
        // Python-specific keywords
        case TokenType::Python_And:
            return "and";
        case TokenType::Python_As:
            return "as";
        case TokenType::Python_Assert:
            return "assert";
        case TokenType::Python_Async:
            return "async";
        case TokenType::Python_Await:
            return "await";
        case TokenType::Python_Def:
            return "def";
        case TokenType::Python_Del:
            return "del";
        case TokenType::Python_Elif:
            return "elif";
        case TokenType::Python_Except:
            return "except";
        case TokenType::Python_Finally:
            return "finally";
        case TokenType::Python_From:
            return "from";
        case TokenType::Python_Global:
            return "global";
        case TokenType::Python_Import:
            return "import";
        case TokenType::Python_In:
            return "in";
        case TokenType::Python_Is:
            return "is";
        case TokenType::Python_Lambda:
            return "lambda";
        case TokenType::Python_None:
            return "None";
        case TokenType::Python_Nonlocal:
            return "nonlocal";
        case TokenType::Python_Not:
            return "not";
        case TokenType::Python_Or:
            return "or";
        case TokenType::Python_Pass:
            return "pass";
        case TokenType::Python_Raise:
            return "raise";
        case TokenType::Python_With:
            return "with";
        case TokenType::Python_Yield:
            return "yield";
        case TokenType::Python_Match:
            return "match";
        case TokenType::Python_Case:
            return "case";
        case TokenType::Python_Type:
            return "type";
        // Python-specific operators
        case TokenType::Python_At:
            return "@";
        case TokenType::Python_AtEqual:
            return "@=";
        case TokenType::Python_StarStar:
            return "**";
        case TokenType::Python_StarStarEqual:
            return "**=";
        case TokenType::Python_FloorDiv:
            return "//";
        case TokenType::Python_FloorDivEqual:
            return "//=";
        case TokenType::Python_Walrus:
            return ":=";
    }
    // All TokenType enum values are exhaustively handled above.
    std::unreachable();
}

auto IsKeyword(TokenType type) -> bool
{
    auto const val = static_cast<uint16_t>(type);
    return (val >= static_cast<uint16_t>(TokenType::Alignas) && val <= static_cast<uint16_t>(TokenType::While)) ||
           IsCSharpKeyword(type) || IsPythonKeyword(type);
}

auto IsCSharpKeyword(TokenType type) -> bool
{
    auto const val = static_cast<uint16_t>(type);
    return val >= static_cast<uint16_t>(TokenType::CSharp_Abstract) &&
           val <= static_cast<uint16_t>(TokenType::CSharp_Not);
}

auto IsPythonKeyword(TokenType type) -> bool
{
    auto const val = static_cast<uint16_t>(type);
    return val >= static_cast<uint16_t>(TokenType::Python_And) && val <= static_cast<uint16_t>(TokenType::Python_Type);
}

auto IsComment(TokenType type) -> bool
{
    return type == TokenType::LineComment || type == TokenType::BlockComment;
}

auto IsLiteral(TokenType type) -> bool
{
    return type == TokenType::NumericLiteral || type == TokenType::StringLiteral || type == TokenType::CharLiteral;
}

auto IsOperatorOrPunctuation(TokenType type) -> bool
{
    auto const val = static_cast<uint16_t>(type);
    return (val >= static_cast<uint16_t>(TokenType::LeftParen) && val <= static_cast<uint16_t>(TokenType::HashHash)) ||
           (val >= static_cast<uint16_t>(TokenType::CSharp_NullConditional) &&
            val <= static_cast<uint16_t>(TokenType::CSharp_Lambda)) ||
           (val >= static_cast<uint16_t>(TokenType::Python_At) &&
            val <= static_cast<uint16_t>(TokenType::Python_Walrus));
}

} // namespace dude
