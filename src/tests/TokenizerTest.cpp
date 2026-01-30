// SPDX-License-Identifier: Apache-2.0
#include <dude/Languages/CppLanguage.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

TEST_CASE("Tokenizer.EmptyInput", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1); // EndOfFile only
    CHECK(result->front().type == TokenType::EndOfFile);
}

TEST_CASE("Tokenizer.Keywords", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("if else for while return class struct void const int");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    REQUIRE(tokens.size() >= 11); // 10 keywords + EndOfFile

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

TEST_CASE("Tokenizer.CppKeywords", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("constexpr auto template typename namespace co_await co_return co_yield");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    CHECK(tokens[0].type == TokenType::Constexpr);
    CHECK(tokens[1].type == TokenType::Auto);
    CHECK(tokens[2].type == TokenType::Template);
    CHECK(tokens[3].type == TokenType::Typename);
    CHECK(tokens[4].type == TokenType::Namespace);
    CHECK(tokens[5].type == TokenType::CoAwait);
    CHECK(tokens[6].type == TokenType::CoReturn);
    CHECK(tokens[7].type == TokenType::CoYield);
}

TEST_CASE("Tokenizer.Identifiers", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("foo _bar baz123 __internal");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 4; ++i)
    {
        CHECK((*result)[i].type == TokenType::Identifier);
    }
    CHECK((*result)[0].text == "foo");
    CHECK((*result)[1].text == "_bar");
    CHECK((*result)[2].text == "baz123");
    CHECK((*result)[3].text == "__internal");
}

TEST_CASE("Tokenizer.NumericLiterals", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("42 3.14 0xFF 0b1010 0777 1'000'000 1.5e10 2.0f");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 8; ++i)
    {
        CHECK((*result)[i].type == TokenType::NumericLiteral);
    }
    CHECK((*result)[0].text == "42");
    CHECK((*result)[1].text == "3.14");
    CHECK((*result)[2].text == "0xFF");
    CHECK((*result)[3].text == "0b1010");
    CHECK((*result)[4].text == "0777");
    CHECK((*result)[5].text == "1'000'000");
}

TEST_CASE("Tokenizer.StringLiterals", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"("hello" "world\n" "escaped\"quote")");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[0].text == "\"hello\"");
    CHECK((*result)[1].type == TokenType::StringLiteral);
    CHECK((*result)[2].type == TokenType::StringLiteral);
}

TEST_CASE("Tokenizer.RawStringLiteral", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"cpp(R"(raw string)")cpp");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[0].text == R"cpp(R"(raw string)")cpp");
}

TEST_CASE("Tokenizer.RawStringWithDelimiter", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"cpp(R"delim(has "quotes" inside)delim")cpp");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("Tokenizer.CharLiterals", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("'a' '\\n' '\\0'");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::CharLiteral);
    CHECK((*result)[0].text == "'a'");
    CHECK((*result)[1].type == TokenType::CharLiteral);
    CHECK((*result)[2].type == TokenType::CharLiteral);
}

TEST_CASE("Tokenizer.LineComment", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("x // comment\ny");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::LineComment);
    CHECK((*result)[1].text == "// comment");
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("Tokenizer.BlockComment", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("x /* block\ncomment */ y");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::BlockComment);
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("Tokenizer.PreprocessorDirective", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("#include <stdio.h>\nint x;");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::PreprocessorDirective);
    CHECK((*result)[0].text == "#include <stdio.h>");
}

TEST_CASE("Tokenizer.PreprocessorContinuation", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("#define FOO \\\nbar\nint x;");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::PreprocessorDirective);
}

TEST_CASE("Tokenizer.Operators", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("+ - * / % = == != < > <= >= && || ! & | ^ ~ << >> ++ -- += -=");
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
    CHECK(t[19].type == TokenType::LessLess);
    CHECK(t[20].type == TokenType::GreaterGreater);
    CHECK(t[21].type == TokenType::PlusPlus);
    CHECK(t[22].type == TokenType::MinusMinus);
    CHECK(t[23].type == TokenType::PlusEqual);
    CHECK(t[24].type == TokenType::MinusEqual);
}

TEST_CASE("Tokenizer.ThreeCharOperators", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("<=> <<= >>= ->* ...");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Spaceship);
    CHECK((*result)[1].type == TokenType::LessLessEqual);
    CHECK((*result)[2].type == TokenType::GreaterGreaterEqual);
    CHECK((*result)[3].type == TokenType::ArrowStar);
    CHECK((*result)[4].type == TokenType::Ellipsis);
}

TEST_CASE("Tokenizer.Punctuation", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("( ) [ ] { } ; : :: , . -> .* ?");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::LeftParen);
    CHECK(t[1].type == TokenType::RightParen);
    CHECK(t[2].type == TokenType::LeftBracket);
    CHECK(t[3].type == TokenType::RightBracket);
    CHECK(t[4].type == TokenType::LeftBrace);
    CHECK(t[5].type == TokenType::RightBrace);
    CHECK(t[6].type == TokenType::Semicolon);
    CHECK(t[7].type == TokenType::Colon);
    CHECK(t[8].type == TokenType::ColonColon);
    CHECK(t[9].type == TokenType::Comma);
    CHECK(t[10].type == TokenType::Dot);
    CHECK(t[11].type == TokenType::Arrow);
    CHECK(t[12].type == TokenType::DotStar);
    CHECK(t[13].type == TokenType::Question);
}

TEST_CASE("Tokenizer.SourceLocationTracking", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("int\nx;\n  y;", 0);
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].location.line == 1);
    CHECK(t[0].location.column == 1);
    CHECK(t[1].location.line == 2);
    CHECK(t[1].location.column == 1);
    CHECK(t[2].location.line == 2);
    CHECK(t[3].location.line == 3);
    CHECK(t[3].location.column == 3);
}

TEST_CASE("Tokenizer.UnterminatedString", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("\"unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("Tokenizer.UnterminatedBlockComment", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("/* unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("Tokenizer.RealWorldSnippet", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"cpp(
void foo(int x) {
    if (x > 0) {
        return;
    }
}
)cpp");
    REQUIRE(result.has_value());

    auto const& t = *result;
    // void foo ( int x ) { if ( x > 0 ) { return ; } }
    size_t i = 0;
    CHECK(t[i++].type == TokenType::Void);
    CHECK(t[i++].type == TokenType::Identifier); // foo
    CHECK(t[i++].type == TokenType::LeftParen);
    CHECK(t[i++].type == TokenType::Int);
    CHECK(t[i++].type == TokenType::Identifier); // x
    CHECK(t[i++].type == TokenType::RightParen);
    CHECK(t[i++].type == TokenType::LeftBrace);
    CHECK(t[i++].type == TokenType::If);
}

TEST_CASE("Tokenizer.PrefixedRawStrings", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"cpp(u8R"(content)" uR"(content)" LR"(content)")cpp");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[1].type == TokenType::StringLiteral);
    CHECK((*result)[2].type == TokenType::StringLiteral);
}

TEST_CASE("Tokenizer.UnterminatedRawString", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"cpp(R"(unterminated)cpp");
    CHECK(!result.has_value());
}

TEST_CASE("Tokenizer.RawStringDelimiterTooLong", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"(R"12345678901234567(content)12345678901234567")");
    CHECK(!result.has_value());
}

TEST_CASE("Tokenizer.PrefixedStrings", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(R"cpp(u"str" U"str" L"str" u8"str")cpp");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 4; ++i)
    {
        CAPTURE(i);
        CHECK((*result)[i].type == TokenType::StringLiteral);
    }
}

TEST_CASE("Tokenizer.PrefixedChars", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("u'x' U'x' L'x' u8'x'");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 4; ++i)
    {
        CAPTURE(i);
        CHECK((*result)[i].type == TokenType::CharLiteral);
    }
}

TEST_CASE("Tokenizer.NumericEdgeCases", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("0b1010'0101 0xFF'00 1.5e+10 2E-3 42uz 3.14f 2.0L");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 7; ++i)
    {
        CAPTURE(i);
        CHECK((*result)[i].type == TokenType::NumericLiteral);
    }
    CHECK((*result)[0].text == "0b1010'0101");
    CHECK((*result)[1].text == "0xFF'00");
    CHECK((*result)[2].text == "1.5e+10");
    CHECK((*result)[3].text == "2E-3");
    CHECK((*result)[4].text == "42uz");
    CHECK((*result)[5].text == "3.14f");
    CHECK((*result)[6].text == "2.0L");
}

TEST_CASE("Tokenizer.AllCppKeywords", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("alignof char8_t char16_t char32_t concept consteval constinit "
                                         "decltype dynamic_cast explicit export mutable register "
                                         "reinterpret_cast requires short signed static_assert static_cast "
                                         "thread_local typeid");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::Alignof);
    CHECK(t[1].type == TokenType::Char8T);
    CHECK(t[2].type == TokenType::Char16T);
    CHECK(t[3].type == TokenType::Char32T);
    CHECK(t[4].type == TokenType::Concept);
    CHECK(t[5].type == TokenType::Consteval);
    CHECK(t[6].type == TokenType::Constinit);
    CHECK(t[7].type == TokenType::Decltype);
    CHECK(t[8].type == TokenType::DynamicCast);
    CHECK(t[9].type == TokenType::Explicit);
    CHECK(t[10].type == TokenType::Export);
    CHECK(t[11].type == TokenType::Mutable);
    CHECK(t[12].type == TokenType::Register);
    CHECK(t[13].type == TokenType::ReinterpretCast);
    CHECK(t[14].type == TokenType::Requires);
    CHECK(t[15].type == TokenType::Short);
    CHECK(t[16].type == TokenType::Signed);
    CHECK(t[17].type == TokenType::StaticAssert);
    CHECK(t[18].type == TokenType::StaticCast);
    CHECK(t[19].type == TokenType::ThreadLocal);
    CHECK(t[20].type == TokenType::Typeid);
}

TEST_CASE("Tokenizer.CompoundAssignmentOperators", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("*= /= %= &= |= ^=");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::StarEqual);
    CHECK(t[1].type == TokenType::SlashEqual);
    CHECK(t[2].type == TokenType::PercentEqual);
    CHECK(t[3].type == TokenType::AmpEqual);
    CHECK(t[4].type == TokenType::PipeEqual);
    CHECK(t[5].type == TokenType::CaretEqual);
}

TEST_CASE("Tokenizer.DotStartedNumeric", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize(".5");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::NumericLiteral);
    CHECK((*result)[0].text == ".5");
}

TEST_CASE("Tokenizer.UnterminatedCharLiteral", "[tokenizer]")
{
    auto result = CppLanguage{}.Tokenize("'unterminated");
    CHECK(!result.has_value());
}
