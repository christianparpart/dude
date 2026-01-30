// SPDX-License-Identifier: Apache-2.0
#include <codedup/Token.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace codedup;

// ---------------------------------------------------------------------------
// TokenTypeName
// ---------------------------------------------------------------------------

TEST_CASE("TokenTypeName.Special", "[Token]")
{
    CHECK(TokenTypeName(TokenType::EndOfFile) == "EndOfFile");
    CHECK(TokenTypeName(TokenType::Invalid) == "Invalid");
}

TEST_CASE("TokenTypeName.Literals", "[Token]")
{
    CHECK(TokenTypeName(TokenType::NumericLiteral) == "NumericLiteral");
    CHECK(TokenTypeName(TokenType::StringLiteral) == "StringLiteral");
    CHECK(TokenTypeName(TokenType::CharLiteral) == "CharLiteral");
}

TEST_CASE("TokenTypeName.Identifier", "[Token]")
{
    CHECK(TokenTypeName(TokenType::Identifier) == "Identifier");
}

TEST_CASE("TokenTypeName.Comments", "[Token]")
{
    CHECK(TokenTypeName(TokenType::LineComment) == "LineComment");
    CHECK(TokenTypeName(TokenType::BlockComment) == "BlockComment");
}

TEST_CASE("TokenTypeName.Preprocessor", "[Token]")
{
    CHECK(TokenTypeName(TokenType::PreprocessorDirective) == "PreprocessorDirective");
}

TEST_CASE("TokenTypeName.CppKeywords", "[Token]")
{
    CHECK(TokenTypeName(TokenType::Alignas) == "alignas");
    CHECK(TokenTypeName(TokenType::Alignof) == "alignof");
    CHECK(TokenTypeName(TokenType::Auto) == "auto");
    CHECK(TokenTypeName(TokenType::Bool) == "bool");
    CHECK(TokenTypeName(TokenType::Break) == "break");
    CHECK(TokenTypeName(TokenType::Case) == "case");
    CHECK(TokenTypeName(TokenType::Catch) == "catch");
    CHECK(TokenTypeName(TokenType::Char) == "char");
    CHECK(TokenTypeName(TokenType::Char8T) == "char8_t");
    CHECK(TokenTypeName(TokenType::Char16T) == "char16_t");
    CHECK(TokenTypeName(TokenType::Char32T) == "char32_t");
    CHECK(TokenTypeName(TokenType::Class) == "class");
    CHECK(TokenTypeName(TokenType::CoAwait) == "co_await");
    CHECK(TokenTypeName(TokenType::CoReturn) == "co_return");
    CHECK(TokenTypeName(TokenType::CoYield) == "co_yield");
    CHECK(TokenTypeName(TokenType::Concept) == "concept");
    CHECK(TokenTypeName(TokenType::Const) == "const");
    CHECK(TokenTypeName(TokenType::Consteval) == "consteval");
    CHECK(TokenTypeName(TokenType::Constexpr) == "constexpr");
    CHECK(TokenTypeName(TokenType::Constinit) == "constinit");
    CHECK(TokenTypeName(TokenType::Continue) == "continue");
    CHECK(TokenTypeName(TokenType::Decltype) == "decltype");
    CHECK(TokenTypeName(TokenType::Default) == "default");
    CHECK(TokenTypeName(TokenType::Delete) == "delete");
    CHECK(TokenTypeName(TokenType::Do) == "do");
    CHECK(TokenTypeName(TokenType::Double) == "double");
    CHECK(TokenTypeName(TokenType::DynamicCast) == "dynamic_cast");
    CHECK(TokenTypeName(TokenType::Else) == "else");
    CHECK(TokenTypeName(TokenType::Enum) == "enum");
    CHECK(TokenTypeName(TokenType::Explicit) == "explicit");
    CHECK(TokenTypeName(TokenType::Export) == "export");
    CHECK(TokenTypeName(TokenType::Extern) == "extern");
    CHECK(TokenTypeName(TokenType::False) == "false");
    CHECK(TokenTypeName(TokenType::Float) == "float");
    CHECK(TokenTypeName(TokenType::For) == "for");
    CHECK(TokenTypeName(TokenType::Friend) == "friend");
    CHECK(TokenTypeName(TokenType::Goto) == "goto");
    CHECK(TokenTypeName(TokenType::If) == "if");
    CHECK(TokenTypeName(TokenType::Inline) == "inline");
    CHECK(TokenTypeName(TokenType::Int) == "int");
    CHECK(TokenTypeName(TokenType::Long) == "long");
    CHECK(TokenTypeName(TokenType::Mutable) == "mutable");
    CHECK(TokenTypeName(TokenType::Namespace) == "namespace");
    CHECK(TokenTypeName(TokenType::New) == "new");
    CHECK(TokenTypeName(TokenType::Noexcept) == "noexcept");
    CHECK(TokenTypeName(TokenType::Nullptr) == "nullptr");
    CHECK(TokenTypeName(TokenType::Operator) == "operator");
    CHECK(TokenTypeName(TokenType::Override) == "override");
    CHECK(TokenTypeName(TokenType::Private) == "private");
    CHECK(TokenTypeName(TokenType::Protected) == "protected");
    CHECK(TokenTypeName(TokenType::Public) == "public");
    CHECK(TokenTypeName(TokenType::Register) == "register");
    CHECK(TokenTypeName(TokenType::ReinterpretCast) == "reinterpret_cast");
    CHECK(TokenTypeName(TokenType::Requires) == "requires");
    CHECK(TokenTypeName(TokenType::Return) == "return");
    CHECK(TokenTypeName(TokenType::Short) == "short");
    CHECK(TokenTypeName(TokenType::Signed) == "signed");
    CHECK(TokenTypeName(TokenType::Sizeof) == "sizeof");
    CHECK(TokenTypeName(TokenType::Static) == "static");
    CHECK(TokenTypeName(TokenType::StaticAssert) == "static_assert");
    CHECK(TokenTypeName(TokenType::StaticCast) == "static_cast");
    CHECK(TokenTypeName(TokenType::Struct) == "struct");
    CHECK(TokenTypeName(TokenType::Switch) == "switch");
    CHECK(TokenTypeName(TokenType::Template) == "template");
    CHECK(TokenTypeName(TokenType::This) == "this");
    CHECK(TokenTypeName(TokenType::ThreadLocal) == "thread_local");
    CHECK(TokenTypeName(TokenType::Throw) == "throw");
    CHECK(TokenTypeName(TokenType::True) == "true");
    CHECK(TokenTypeName(TokenType::Try) == "try");
    CHECK(TokenTypeName(TokenType::Typedef) == "typedef");
    CHECK(TokenTypeName(TokenType::Typeid) == "typeid");
    CHECK(TokenTypeName(TokenType::Typename) == "typename");
    CHECK(TokenTypeName(TokenType::Union) == "union");
    CHECK(TokenTypeName(TokenType::Unsigned) == "unsigned");
    CHECK(TokenTypeName(TokenType::Using) == "using");
    CHECK(TokenTypeName(TokenType::Virtual) == "virtual");
    CHECK(TokenTypeName(TokenType::Void) == "void");
    CHECK(TokenTypeName(TokenType::Volatile) == "volatile");
    CHECK(TokenTypeName(TokenType::WcharT) == "wchar_t");
    CHECK(TokenTypeName(TokenType::While) == "while");
}

TEST_CASE("TokenTypeName.SharedOperators", "[Token]")
{
    CHECK(TokenTypeName(TokenType::LeftParen) == "(");
    CHECK(TokenTypeName(TokenType::RightParen) == ")");
    CHECK(TokenTypeName(TokenType::LeftBracket) == "[");
    CHECK(TokenTypeName(TokenType::RightBracket) == "]");
    CHECK(TokenTypeName(TokenType::LeftBrace) == "{");
    CHECK(TokenTypeName(TokenType::RightBrace) == "}");
    CHECK(TokenTypeName(TokenType::Semicolon) == ";");
    CHECK(TokenTypeName(TokenType::Colon) == ":");
    CHECK(TokenTypeName(TokenType::ColonColon) == "::");
    CHECK(TokenTypeName(TokenType::Comma) == ",");
    CHECK(TokenTypeName(TokenType::Dot) == ".");
    CHECK(TokenTypeName(TokenType::DotStar) == ".*");
    CHECK(TokenTypeName(TokenType::Ellipsis) == "...");
    CHECK(TokenTypeName(TokenType::Arrow) == "->");
    CHECK(TokenTypeName(TokenType::ArrowStar) == "->*");
    CHECK(TokenTypeName(TokenType::Tilde) == "~");
    CHECK(TokenTypeName(TokenType::Exclaim) == "!");
    CHECK(TokenTypeName(TokenType::ExclaimEqual) == "!=");
    CHECK(TokenTypeName(TokenType::Plus) == "+");
    CHECK(TokenTypeName(TokenType::PlusPlus) == "++");
    CHECK(TokenTypeName(TokenType::PlusEqual) == "+=");
    CHECK(TokenTypeName(TokenType::Minus) == "-");
    CHECK(TokenTypeName(TokenType::MinusMinus) == "--");
    CHECK(TokenTypeName(TokenType::MinusEqual) == "-=");
    CHECK(TokenTypeName(TokenType::Star) == "*");
    CHECK(TokenTypeName(TokenType::StarEqual) == "*=");
    CHECK(TokenTypeName(TokenType::Slash) == "/");
    CHECK(TokenTypeName(TokenType::SlashEqual) == "/=");
    CHECK(TokenTypeName(TokenType::Percent) == "%");
    CHECK(TokenTypeName(TokenType::PercentEqual) == "%=");
    CHECK(TokenTypeName(TokenType::Amp) == "&");
    CHECK(TokenTypeName(TokenType::AmpAmp) == "&&");
    CHECK(TokenTypeName(TokenType::AmpEqual) == "&=");
    CHECK(TokenTypeName(TokenType::Pipe) == "|");
    CHECK(TokenTypeName(TokenType::PipePipe) == "||");
    CHECK(TokenTypeName(TokenType::PipeEqual) == "|=");
    CHECK(TokenTypeName(TokenType::Caret) == "^");
    CHECK(TokenTypeName(TokenType::CaretEqual) == "^=");
    CHECK(TokenTypeName(TokenType::Less) == "<");
    CHECK(TokenTypeName(TokenType::LessLess) == "<<");
    CHECK(TokenTypeName(TokenType::LessLessEqual) == "<<=");
    CHECK(TokenTypeName(TokenType::LessEqual) == "<=");
    CHECK(TokenTypeName(TokenType::Greater) == ">");
    CHECK(TokenTypeName(TokenType::GreaterGreater) == ">>");
    CHECK(TokenTypeName(TokenType::GreaterGreaterEqual) == ">>=");
    CHECK(TokenTypeName(TokenType::GreaterEqual) == ">=");
    CHECK(TokenTypeName(TokenType::Equal) == "=");
    CHECK(TokenTypeName(TokenType::EqualEqual) == "==");
    CHECK(TokenTypeName(TokenType::Question) == "?");
    CHECK(TokenTypeName(TokenType::Hash) == "#");
    CHECK(TokenTypeName(TokenType::HashHash) == "##");
    CHECK(TokenTypeName(TokenType::Spaceship) == "<=>");
}

TEST_CASE("TokenTypeName.CSharpKeywords", "[Token]")
{
    CHECK(TokenTypeName(TokenType::CSharp_Abstract) == "abstract");
    CHECK(TokenTypeName(TokenType::CSharp_As) == "as");
    CHECK(TokenTypeName(TokenType::CSharp_Async) == "async");
    CHECK(TokenTypeName(TokenType::CSharp_Await) == "await");
    CHECK(TokenTypeName(TokenType::CSharp_Base) == "base");
    CHECK(TokenTypeName(TokenType::CSharp_Byte) == "byte");
    CHECK(TokenTypeName(TokenType::CSharp_Checked) == "checked");
    CHECK(TokenTypeName(TokenType::CSharp_Decimal) == "decimal");
    CHECK(TokenTypeName(TokenType::CSharp_Delegate) == "delegate");
    CHECK(TokenTypeName(TokenType::CSharp_Event) == "event");
    CHECK(TokenTypeName(TokenType::CSharp_Finally) == "finally");
    CHECK(TokenTypeName(TokenType::CSharp_Fixed) == "fixed");
    CHECK(TokenTypeName(TokenType::CSharp_Foreach) == "foreach");
    CHECK(TokenTypeName(TokenType::CSharp_In) == "in");
    CHECK(TokenTypeName(TokenType::CSharp_Interface) == "interface");
    CHECK(TokenTypeName(TokenType::CSharp_Internal) == "internal");
    CHECK(TokenTypeName(TokenType::CSharp_Is) == "is");
    CHECK(TokenTypeName(TokenType::CSharp_Lock) == "lock");
    CHECK(TokenTypeName(TokenType::CSharp_Nameof) == "nameof");
    CHECK(TokenTypeName(TokenType::CSharp_Null) == "null");
    CHECK(TokenTypeName(TokenType::CSharp_Object) == "object");
    CHECK(TokenTypeName(TokenType::CSharp_Out) == "out");
    CHECK(TokenTypeName(TokenType::CSharp_Params) == "params");
    CHECK(TokenTypeName(TokenType::CSharp_Partial) == "partial");
    CHECK(TokenTypeName(TokenType::CSharp_Readonly) == "readonly");
    CHECK(TokenTypeName(TokenType::CSharp_Ref) == "ref");
    CHECK(TokenTypeName(TokenType::CSharp_Sbyte) == "sbyte");
    CHECK(TokenTypeName(TokenType::CSharp_Sealed) == "sealed");
    CHECK(TokenTypeName(TokenType::CSharp_Stackalloc) == "stackalloc");
    CHECK(TokenTypeName(TokenType::CSharp_String) == "string");
    CHECK(TokenTypeName(TokenType::CSharp_Typeof) == "typeof");
    CHECK(TokenTypeName(TokenType::CSharp_Uint) == "uint");
    CHECK(TokenTypeName(TokenType::CSharp_Ulong) == "ulong");
    CHECK(TokenTypeName(TokenType::CSharp_Unchecked) == "unchecked");
    CHECK(TokenTypeName(TokenType::CSharp_Unsafe) == "unsafe");
    CHECK(TokenTypeName(TokenType::CSharp_Ushort) == "ushort");
    CHECK(TokenTypeName(TokenType::CSharp_Var) == "var");
    CHECK(TokenTypeName(TokenType::CSharp_Where) == "where");
    CHECK(TokenTypeName(TokenType::CSharp_Yield) == "yield");
    CHECK(TokenTypeName(TokenType::CSharp_Get) == "get");
    CHECK(TokenTypeName(TokenType::CSharp_Set) == "set");
    CHECK(TokenTypeName(TokenType::CSharp_Value) == "value");
    CHECK(TokenTypeName(TokenType::CSharp_When) == "when");
    CHECK(TokenTypeName(TokenType::CSharp_Init) == "init");
    CHECK(TokenTypeName(TokenType::CSharp_Record) == "record");
    CHECK(TokenTypeName(TokenType::CSharp_With) == "with");
    CHECK(TokenTypeName(TokenType::CSharp_And) == "and");
    CHECK(TokenTypeName(TokenType::CSharp_Or) == "or");
    CHECK(TokenTypeName(TokenType::CSharp_Not) == "not");
}

TEST_CASE("TokenTypeName.CSharpOperators", "[Token]")
{
    CHECK(TokenTypeName(TokenType::CSharp_NullConditional) == "?.");
    CHECK(TokenTypeName(TokenType::CSharp_NullCoalescing) == "??");
    CHECK(TokenTypeName(TokenType::CSharp_NullCoalescingAssign) == "\?\?=");
    CHECK(TokenTypeName(TokenType::CSharp_Lambda) == "=>");
}

TEST_CASE("TokenTypeName.PythonKeywords", "[Token]")
{
    CHECK(TokenTypeName(TokenType::Python_And) == "and");
    CHECK(TokenTypeName(TokenType::Python_As) == "as");
    CHECK(TokenTypeName(TokenType::Python_Assert) == "assert");
    CHECK(TokenTypeName(TokenType::Python_Async) == "async");
    CHECK(TokenTypeName(TokenType::Python_Await) == "await");
    CHECK(TokenTypeName(TokenType::Python_Def) == "def");
    CHECK(TokenTypeName(TokenType::Python_Del) == "del");
    CHECK(TokenTypeName(TokenType::Python_Elif) == "elif");
    CHECK(TokenTypeName(TokenType::Python_Except) == "except");
    CHECK(TokenTypeName(TokenType::Python_Finally) == "finally");
    CHECK(TokenTypeName(TokenType::Python_From) == "from");
    CHECK(TokenTypeName(TokenType::Python_Global) == "global");
    CHECK(TokenTypeName(TokenType::Python_Import) == "import");
    CHECK(TokenTypeName(TokenType::Python_In) == "in");
    CHECK(TokenTypeName(TokenType::Python_Is) == "is");
    CHECK(TokenTypeName(TokenType::Python_Lambda) == "lambda");
    CHECK(TokenTypeName(TokenType::Python_None) == "None");
    CHECK(TokenTypeName(TokenType::Python_Nonlocal) == "nonlocal");
    CHECK(TokenTypeName(TokenType::Python_Not) == "not");
    CHECK(TokenTypeName(TokenType::Python_Or) == "or");
    CHECK(TokenTypeName(TokenType::Python_Pass) == "pass");
    CHECK(TokenTypeName(TokenType::Python_Raise) == "raise");
    CHECK(TokenTypeName(TokenType::Python_With) == "with");
    CHECK(TokenTypeName(TokenType::Python_Yield) == "yield");
    CHECK(TokenTypeName(TokenType::Python_Match) == "match");
    CHECK(TokenTypeName(TokenType::Python_Case) == "case");
    CHECK(TokenTypeName(TokenType::Python_Type) == "type");
}

TEST_CASE("TokenTypeName.PythonOperators", "[Token]")
{
    CHECK(TokenTypeName(TokenType::Python_At) == "@");
    CHECK(TokenTypeName(TokenType::Python_AtEqual) == "@=");
    CHECK(TokenTypeName(TokenType::Python_StarStar) == "**");
    CHECK(TokenTypeName(TokenType::Python_StarStarEqual) == "**=");
    CHECK(TokenTypeName(TokenType::Python_FloorDiv) == "//");
    CHECK(TokenTypeName(TokenType::Python_FloorDivEqual) == "//=");
    CHECK(TokenTypeName(TokenType::Python_Walrus) == ":=");
}

// ---------------------------------------------------------------------------
// IsKeyword
// ---------------------------------------------------------------------------

TEST_CASE("IsKeyword.CppKeywordsReturnTrue", "[Token]")
{
    CHECK(IsKeyword(TokenType::Alignas));
    CHECK(IsKeyword(TokenType::Auto));
    CHECK(IsKeyword(TokenType::Bool));
    CHECK(IsKeyword(TokenType::If));
    CHECK(IsKeyword(TokenType::Return));
    CHECK(IsKeyword(TokenType::While));
    CHECK(IsKeyword(TokenType::Constexpr));
    CHECK(IsKeyword(TokenType::Namespace));
    CHECK(IsKeyword(TokenType::Virtual));
}

TEST_CASE("IsKeyword.CSharpKeywordsReturnTrue", "[Token]")
{
    CHECK(IsKeyword(TokenType::CSharp_Abstract));
    CHECK(IsKeyword(TokenType::CSharp_Foreach));
    CHECK(IsKeyword(TokenType::CSharp_Not));
}

TEST_CASE("IsKeyword.PythonKeywordsReturnTrue", "[Token]")
{
    CHECK(IsKeyword(TokenType::Python_And));
    CHECK(IsKeyword(TokenType::Python_Def));
    CHECK(IsKeyword(TokenType::Python_Type));
}

TEST_CASE("IsKeyword.NonKeywordsReturnFalse", "[Token]")
{
    CHECK_FALSE(IsKeyword(TokenType::EndOfFile));
    CHECK_FALSE(IsKeyword(TokenType::Invalid));
    CHECK_FALSE(IsKeyword(TokenType::NumericLiteral));
    CHECK_FALSE(IsKeyword(TokenType::StringLiteral));
    CHECK_FALSE(IsKeyword(TokenType::Identifier));
    CHECK_FALSE(IsKeyword(TokenType::LineComment));
    CHECK_FALSE(IsKeyword(TokenType::LeftParen));
    CHECK_FALSE(IsKeyword(TokenType::Plus));
    CHECK_FALSE(IsKeyword(TokenType::CSharp_NullConditional));
    CHECK_FALSE(IsKeyword(TokenType::Python_At));
}

// ---------------------------------------------------------------------------
// IsCSharpKeyword
// ---------------------------------------------------------------------------

TEST_CASE("IsCSharpKeyword.CSharpKeywordsReturnTrue", "[Token]")
{
    CHECK(IsCSharpKeyword(TokenType::CSharp_Abstract));
    CHECK(IsCSharpKeyword(TokenType::CSharp_As));
    CHECK(IsCSharpKeyword(TokenType::CSharp_Foreach));
    CHECK(IsCSharpKeyword(TokenType::CSharp_Var));
    CHECK(IsCSharpKeyword(TokenType::CSharp_Yield));
    CHECK(IsCSharpKeyword(TokenType::CSharp_Get));
    CHECK(IsCSharpKeyword(TokenType::CSharp_Set));
    CHECK(IsCSharpKeyword(TokenType::CSharp_And));
    CHECK(IsCSharpKeyword(TokenType::CSharp_Not));
}

TEST_CASE("IsCSharpKeyword.NonCSharpReturnFalse", "[Token]")
{
    CHECK_FALSE(IsCSharpKeyword(TokenType::If));
    CHECK_FALSE(IsCSharpKeyword(TokenType::Auto));
    CHECK_FALSE(IsCSharpKeyword(TokenType::Python_Def));
    CHECK_FALSE(IsCSharpKeyword(TokenType::LeftParen));
    CHECK_FALSE(IsCSharpKeyword(TokenType::Identifier));
    CHECK_FALSE(IsCSharpKeyword(TokenType::CSharp_NullConditional));
}

// ---------------------------------------------------------------------------
// IsPythonKeyword
// ---------------------------------------------------------------------------

TEST_CASE("IsPythonKeyword.PythonKeywordsReturnTrue", "[Token]")
{
    CHECK(IsPythonKeyword(TokenType::Python_And));
    CHECK(IsPythonKeyword(TokenType::Python_As));
    CHECK(IsPythonKeyword(TokenType::Python_Def));
    CHECK(IsPythonKeyword(TokenType::Python_Lambda));
    CHECK(IsPythonKeyword(TokenType::Python_None));
    CHECK(IsPythonKeyword(TokenType::Python_Yield));
    CHECK(IsPythonKeyword(TokenType::Python_Match));
    CHECK(IsPythonKeyword(TokenType::Python_Type));
}

TEST_CASE("IsPythonKeyword.NonPythonReturnFalse", "[Token]")
{
    CHECK_FALSE(IsPythonKeyword(TokenType::If));
    CHECK_FALSE(IsPythonKeyword(TokenType::Auto));
    CHECK_FALSE(IsPythonKeyword(TokenType::CSharp_Abstract));
    CHECK_FALSE(IsPythonKeyword(TokenType::LeftParen));
    CHECK_FALSE(IsPythonKeyword(TokenType::Identifier));
    CHECK_FALSE(IsPythonKeyword(TokenType::Python_At));
}

// ---------------------------------------------------------------------------
// IsComment
// ---------------------------------------------------------------------------

TEST_CASE("IsComment.CommentsReturnTrue", "[Token]")
{
    CHECK(IsComment(TokenType::LineComment));
    CHECK(IsComment(TokenType::BlockComment));
}

TEST_CASE("IsComment.NonCommentsReturnFalse", "[Token]")
{
    CHECK_FALSE(IsComment(TokenType::EndOfFile));
    CHECK_FALSE(IsComment(TokenType::Identifier));
    CHECK_FALSE(IsComment(TokenType::StringLiteral));
    CHECK_FALSE(IsComment(TokenType::If));
    CHECK_FALSE(IsComment(TokenType::LeftParen));
    CHECK_FALSE(IsComment(TokenType::PreprocessorDirective));
}

// ---------------------------------------------------------------------------
// IsLiteral
// ---------------------------------------------------------------------------

TEST_CASE("IsLiteral.LiteralsReturnTrue", "[Token]")
{
    CHECK(IsLiteral(TokenType::NumericLiteral));
    CHECK(IsLiteral(TokenType::StringLiteral));
    CHECK(IsLiteral(TokenType::CharLiteral));
}

TEST_CASE("IsLiteral.NonLiteralsReturnFalse", "[Token]")
{
    CHECK_FALSE(IsLiteral(TokenType::EndOfFile));
    CHECK_FALSE(IsLiteral(TokenType::Identifier));
    CHECK_FALSE(IsLiteral(TokenType::LineComment));
    CHECK_FALSE(IsLiteral(TokenType::If));
    CHECK_FALSE(IsLiteral(TokenType::LeftParen));
    CHECK_FALSE(IsLiteral(TokenType::PreprocessorDirective));
}

// ---------------------------------------------------------------------------
// IsOperatorOrPunctuation
// ---------------------------------------------------------------------------

TEST_CASE("IsOperatorOrPunctuation.SharedOperatorsReturnTrue", "[Token]")
{
    CHECK(IsOperatorOrPunctuation(TokenType::LeftParen));
    CHECK(IsOperatorOrPunctuation(TokenType::RightBrace));
    CHECK(IsOperatorOrPunctuation(TokenType::Plus));
    CHECK(IsOperatorOrPunctuation(TokenType::Minus));
    CHECK(IsOperatorOrPunctuation(TokenType::Star));
    CHECK(IsOperatorOrPunctuation(TokenType::EqualEqual));
    CHECK(IsOperatorOrPunctuation(TokenType::Spaceship));
    CHECK(IsOperatorOrPunctuation(TokenType::Hash));
    CHECK(IsOperatorOrPunctuation(TokenType::HashHash));
}

TEST_CASE("IsOperatorOrPunctuation.CSharpOperatorsReturnTrue", "[Token]")
{
    CHECK(IsOperatorOrPunctuation(TokenType::CSharp_NullConditional));
    CHECK(IsOperatorOrPunctuation(TokenType::CSharp_NullCoalescing));
    CHECK(IsOperatorOrPunctuation(TokenType::CSharp_NullCoalescingAssign));
    CHECK(IsOperatorOrPunctuation(TokenType::CSharp_Lambda));
}

TEST_CASE("IsOperatorOrPunctuation.PythonOperatorsReturnTrue", "[Token]")
{
    CHECK(IsOperatorOrPunctuation(TokenType::Python_At));
    CHECK(IsOperatorOrPunctuation(TokenType::Python_AtEqual));
    CHECK(IsOperatorOrPunctuation(TokenType::Python_StarStar));
    CHECK(IsOperatorOrPunctuation(TokenType::Python_StarStarEqual));
    CHECK(IsOperatorOrPunctuation(TokenType::Python_FloorDiv));
    CHECK(IsOperatorOrPunctuation(TokenType::Python_FloorDivEqual));
    CHECK(IsOperatorOrPunctuation(TokenType::Python_Walrus));
}

TEST_CASE("IsOperatorOrPunctuation.NonOperatorsReturnFalse", "[Token]")
{
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::EndOfFile));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::Identifier));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::NumericLiteral));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::StringLiteral));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::If));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::LineComment));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::CSharp_Abstract));
    CHECK_FALSE(IsOperatorOrPunctuation(TokenType::Python_Def));
}
