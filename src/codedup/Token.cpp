// SPDX-License-Identifier: Apache-2.0
#include <codedup/Token.hpp>

namespace codedup
{

auto tokenTypeName(TokenType type) -> std::string_view
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
    }
    return "Unknown";
}

auto isKeyword(TokenType type) -> bool
{
    auto const val = static_cast<uint16_t>(type);
    return val >= static_cast<uint16_t>(TokenType::Alignas) && val <= static_cast<uint16_t>(TokenType::While);
}

auto isComment(TokenType type) -> bool
{
    return type == TokenType::LineComment || type == TokenType::BlockComment;
}

auto isLiteral(TokenType type) -> bool
{
    return type == TokenType::NumericLiteral || type == TokenType::StringLiteral || type == TokenType::CharLiteral;
}

auto isOperatorOrPunctuation(TokenType type) -> bool
{
    auto const val = static_cast<uint16_t>(type);
    return val >= static_cast<uint16_t>(TokenType::LeftParen) && val <= static_cast<uint16_t>(TokenType::HashHash);
}

} // namespace codedup
