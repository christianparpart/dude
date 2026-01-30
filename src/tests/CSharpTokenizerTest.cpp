// SPDX-License-Identifier: Apache-2.0
#include <codedup/Languages/CSharpLanguage.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace codedup;

TEST_CASE("CSharpTokenizer.EmptyInput", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    CHECK(result->front().type == TokenType::EndOfFile);
}

TEST_CASE("CSharpTokenizer.SharedKeywords", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("if else for while return class struct void const int");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    REQUIRE(tokens.size() >= 11);
    CHECK(tokens[0].type == TokenType::If);
    CHECK(tokens[1].type == TokenType::Else);
    CHECK(tokens[2].type == TokenType::For);
    CHECK(tokens[3].type == TokenType::While);
    CHECK(tokens[4].type == TokenType::Return);
    CHECK(tokens[5].type == TokenType::Class);
    CHECK(tokens[6].type == TokenType::Struct);
    CHECK(tokens[7].type == TokenType::Void);
    CHECK(tokens[8].type == TokenType::Const);
    CHECK(tokens[9].type == TokenType::Int);
}

TEST_CASE("CSharpTokenizer.CSharpSpecificKeywords", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("async await var foreach interface delegate event sealed");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    CHECK(tokens[0].type == TokenType::CSharp_Async);
    CHECK(tokens[1].type == TokenType::CSharp_Await);
    CHECK(tokens[2].type == TokenType::CSharp_Var);
    CHECK(tokens[3].type == TokenType::CSharp_Foreach);
    CHECK(tokens[4].type == TokenType::CSharp_Interface);
    CHECK(tokens[5].type == TokenType::CSharp_Delegate);
    CHECK(tokens[6].type == TokenType::CSharp_Event);
    CHECK(tokens[7].type == TokenType::CSharp_Sealed);
}

TEST_CASE("CSharpTokenizer.ContextualKeywords", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("get set value when init record with and or not");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    CHECK(tokens[0].type == TokenType::CSharp_Get);
    CHECK(tokens[1].type == TokenType::CSharp_Set);
    CHECK(tokens[2].type == TokenType::CSharp_Value);
    CHECK(tokens[3].type == TokenType::CSharp_When);
    CHECK(tokens[4].type == TokenType::CSharp_Init);
    CHECK(tokens[5].type == TokenType::CSharp_Record);
    CHECK(tokens[6].type == TokenType::CSharp_With);
    CHECK(tokens[7].type == TokenType::CSharp_And);
    CHECK(tokens[8].type == TokenType::CSharp_Or);
    CHECK(tokens[9].type == TokenType::CSharp_Not);
}

TEST_CASE("CSharpTokenizer.Identifiers", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("foo _bar baz123");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 3; ++i)
        CHECK((*result)[i].type == TokenType::Identifier);
    CHECK((*result)[0].text == "foo");
    CHECK((*result)[1].text == "_bar");
    CHECK((*result)[2].text == "baz123");
}

TEST_CASE("CSharpTokenizer.RegularString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"("hello" "world\n" "escaped\"quote")");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[0].text == "\"hello\"");
    CHECK((*result)[1].type == TokenType::StringLiteral);
    CHECK((*result)[2].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.VerbatimString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs(@"verbatim ""escaped"" string")cs");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.InterpolatedString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"($"Hello {name}")");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.NumericLiterals", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("42 3.14 0xFF 0b1010 1_000_000 2.5f 3.14m");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 7; ++i)
    {
        CAPTURE(i);
        CHECK((*result)[i].type == TokenType::NumericLiteral);
    }
    CHECK((*result)[0].text == "42");
    CHECK((*result)[1].text == "3.14");
    CHECK((*result)[2].text == "0xFF");
    CHECK((*result)[3].text == "0b1010");
    CHECK((*result)[4].text == "1_000_000");
}

TEST_CASE("CSharpTokenizer.NullConditionalOperator", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("a?.b");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::CSharp_NullConditional);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("CSharpTokenizer.NullCoalescingOperator", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("a ?? b");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::CSharp_NullCoalescing);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("CSharpTokenizer.NullCoalescingAssignment", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("a \?\?= b"); // ??=
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::CSharp_NullCoalescingAssign);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("CSharpTokenizer.LambdaOperator", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("x => x + 1");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::CSharp_Lambda);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("CSharpTokenizer.LineComment", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("x // comment\ny");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::LineComment);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("CSharpTokenizer.XmlDocComment", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("/// <summary>doc</summary>\nvoid foo();");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::LineComment);
}

TEST_CASE("CSharpTokenizer.BlockComment", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("x /* block\ncomment */ y");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::BlockComment);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("CSharpTokenizer.PreprocessorDirective", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("#region MyRegion\nint x;\n#endregion");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::PreprocessorDirective);
}

TEST_CASE("CSharpTokenizer.FullMethod", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs(
public void ProcessData(int input) {
    var result = input * 2;
    if (result > 100) {
        result = result - 50;
    }
}
)cs");
    REQUIRE(result.has_value());

    auto const& t = *result;
    size_t i = 0;
    CHECK(t[i++].type == TokenType::Public);
    CHECK(t[i++].type == TokenType::Void);
    CHECK(t[i++].type == TokenType::Identifier); // ProcessData
    CHECK(t[i++].type == TokenType::LeftParen);
    CHECK(t[i++].type == TokenType::Int);
    CHECK(t[i++].type == TokenType::Identifier); // input
    CHECK(t[i++].type == TokenType::RightParen);
    CHECK(t[i++].type == TokenType::LeftBrace);
}

TEST_CASE("CSharpTokenizer.UnterminatedString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("\"unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("CSharpTokenizer.UnterminatedBlockComment", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("/* unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("CSharpTokenizer.CharLiteral", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("'a' '\\n' '\\0'");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::CharLiteral);
    CHECK((*result)[0].text == "'a'");
    CHECK((*result)[1].type == TokenType::CharLiteral);
    CHECK((*result)[2].type == TokenType::CharLiteral);
}

TEST_CASE("CSharpTokenizer.SharedOperators", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("+ - * / % = == != < > <= >= && || ! & | ^ ~");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::Plus);
    CHECK(t[1].type == TokenType::Minus);
    CHECK(t[2].type == TokenType::Star);
    CHECK(t[3].type == TokenType::Slash);
    CHECK(t[4].type == TokenType::Percent);
    CHECK(t[5].type == TokenType::Equal);
    CHECK(t[6].type == TokenType::EqualEqual);
    CHECK(t[7].type == TokenType::ExclaimEqual);
    CHECK(t[8].type == TokenType::Less);
    CHECK(t[9].type == TokenType::Greater);
    CHECK(t[10].type == TokenType::LessEqual);
    CHECK(t[11].type == TokenType::GreaterEqual);
    CHECK(t[12].type == TokenType::AmpAmp);
    CHECK(t[13].type == TokenType::PipePipe);
    CHECK(t[14].type == TokenType::Exclaim);
    CHECK(t[15].type == TokenType::Amp);
    CHECK(t[16].type == TokenType::Pipe);
    CHECK(t[17].type == TokenType::Caret);
    CHECK(t[18].type == TokenType::Tilde);
}

TEST_CASE("CSharpTokenizer.VerbatimStringDoubledQuotes", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs(@"doubled ""quotes"" inside")cs");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[0].text.find("doubled") != std::string::npos);
}

TEST_CASE("CSharpTokenizer.UnterminatedVerbatimString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs(@"unterminated)cs");
    CHECK(!result.has_value());
}

TEST_CASE("CSharpTokenizer.InterpolatedVerbatimString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs($@"multi {x}")cs");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.VerbatimInterpolatedString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs(@$"multi {x}")cs");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.InterpolatedEscapedBraces", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"($"{{literal}}")");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.UnterminatedInterpolatedString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("$\"unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("CSharpTokenizer.RawStringLiteral", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs("""raw string""")cs");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.RawStringMultiline", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("\"\"\"multi\nline\nraw\"\"\"");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.RawStringFourQuotes", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs(""""has "quotes" inside"""")cs");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("CSharpTokenizer.UnterminatedRawString", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(R"cs("""unterminated)cs");
    CHECK(!result.has_value());
}

TEST_CASE("CSharpTokenizer.NumericSuffixes", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("42UL 3.14M 100D 0XAB 0B1010 1.5e10");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 6; ++i)
    {
        CAPTURE(i);
        CHECK((*result)[i].type == TokenType::NumericLiteral);
    }
    CHECK((*result)[0].text == "42UL");
    CHECK((*result)[1].text == "3.14M");
    CHECK((*result)[2].text == "100D");
}

TEST_CASE("CSharpTokenizer.VerbatimIdentifier", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("@if");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[0].text == "@if");
}

TEST_CASE("CSharpTokenizer.DotStartedNumeric", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize(".5");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::NumericLiteral);
}

TEST_CASE("CSharpTokenizer.AllCSharpKeywords", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("checked decimal event finally fixed in internal is lock nameof "
                                            "object out params partial readonly ref sbyte stackalloc typeof "
                                            "uint ulong unchecked unsafe ushort where value when init");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::CSharp_Checked);
    CHECK(t[1].type == TokenType::CSharp_Decimal);
    CHECK(t[2].type == TokenType::CSharp_Event);
    CHECK(t[3].type == TokenType::CSharp_Finally);
    CHECK(t[4].type == TokenType::CSharp_Fixed);
    CHECK(t[5].type == TokenType::CSharp_In);
    CHECK(t[6].type == TokenType::CSharp_Internal);
    CHECK(t[7].type == TokenType::CSharp_Is);
    CHECK(t[8].type == TokenType::CSharp_Lock);
    CHECK(t[9].type == TokenType::CSharp_Nameof);
    CHECK(t[10].type == TokenType::CSharp_Object);
    CHECK(t[11].type == TokenType::CSharp_Out);
    CHECK(t[12].type == TokenType::CSharp_Params);
    CHECK(t[13].type == TokenType::CSharp_Partial);
    CHECK(t[14].type == TokenType::CSharp_Readonly);
    CHECK(t[15].type == TokenType::CSharp_Ref);
    CHECK(t[16].type == TokenType::CSharp_Sbyte);
    CHECK(t[17].type == TokenType::CSharp_Stackalloc);
    CHECK(t[18].type == TokenType::CSharp_Typeof);
    CHECK(t[19].type == TokenType::CSharp_Uint);
    CHECK(t[20].type == TokenType::CSharp_Ulong);
    CHECK(t[21].type == TokenType::CSharp_Unchecked);
    CHECK(t[22].type == TokenType::CSharp_Unsafe);
    CHECK(t[23].type == TokenType::CSharp_Ushort);
    CHECK(t[24].type == TokenType::CSharp_Where);
    CHECK(t[25].type == TokenType::CSharp_Value);
    CHECK(t[26].type == TokenType::CSharp_When);
    CHECK(t[27].type == TokenType::CSharp_Init);
}

TEST_CASE("CSharpTokenizer.UnterminatedCharLiteral", "[csharp][tokenizer]")
{
    auto result = CSharpLanguage{}.Tokenize("'unterminated");
    CHECK(!result.has_value());
}
