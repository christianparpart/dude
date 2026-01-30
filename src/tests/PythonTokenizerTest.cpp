// SPDX-License-Identifier: Apache-2.0
#include <codedup/Languages/PythonLanguage.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace codedup;

TEST_CASE("PythonTokenizer.EmptyInput", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    CHECK(result->front().type == TokenType::EndOfFile);
}

TEST_CASE("PythonTokenizer.SharedKeywords", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("if else for while return class break continue try");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    REQUIRE(tokens.size() >= 10);
    CHECK(tokens[0].type == TokenType::If);
    CHECK(tokens[1].type == TokenType::Else);
    CHECK(tokens[2].type == TokenType::For);
    CHECK(tokens[3].type == TokenType::While);
    CHECK(tokens[4].type == TokenType::Return);
    CHECK(tokens[5].type == TokenType::Class);
    CHECK(tokens[6].type == TokenType::Break);
    CHECK(tokens[7].type == TokenType::Continue);
    CHECK(tokens[8].type == TokenType::Try);
}

TEST_CASE("PythonTokenizer.PythonSpecificKeywords", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("def import from async await yield with assert lambda");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    CHECK(tokens[0].type == TokenType::Python_Def);
    CHECK(tokens[1].type == TokenType::Python_Import);
    CHECK(tokens[2].type == TokenType::Python_From);
    CHECK(tokens[3].type == TokenType::Python_Async);
    CHECK(tokens[4].type == TokenType::Python_Await);
    CHECK(tokens[5].type == TokenType::Python_Yield);
    CHECK(tokens[6].type == TokenType::Python_With);
    CHECK(tokens[7].type == TokenType::Python_Assert);
    CHECK(tokens[8].type == TokenType::Python_Lambda);
}

TEST_CASE("PythonTokenizer.MorePythonKeywords", "[python][tokenizer]")
{
    auto result =
        PythonLanguage{}.Tokenize("nonlocal global elif except raise pass del in is and or not None match case");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    CHECK(tokens[0].type == TokenType::Python_Nonlocal);
    CHECK(tokens[1].type == TokenType::Python_Global);
    CHECK(tokens[2].type == TokenType::Python_Elif);
    CHECK(tokens[3].type == TokenType::Python_Except);
    CHECK(tokens[4].type == TokenType::Python_Raise);
    CHECK(tokens[5].type == TokenType::Python_Pass);
    CHECK(tokens[6].type == TokenType::Python_Del);
    CHECK(tokens[7].type == TokenType::Python_In);
    CHECK(tokens[8].type == TokenType::Python_Is);
    CHECK(tokens[9].type == TokenType::Python_And);
    CHECK(tokens[10].type == TokenType::Python_Or);
    CHECK(tokens[11].type == TokenType::Python_Not);
    CHECK(tokens[12].type == TokenType::Python_None);
    CHECK(tokens[13].type == TokenType::Python_Match);
    CHECK(tokens[14].type == TokenType::Python_Case);
}

TEST_CASE("PythonTokenizer.TrueFalse", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("True False");
    REQUIRE(result.has_value());

    auto const& tokens = *result;
    CHECK(tokens[0].type == TokenType::True);
    CHECK(tokens[0].text == "True");
    CHECK(tokens[1].type == TokenType::False);
    CHECK(tokens[1].text == "False");
}

TEST_CASE("PythonTokenizer.Identifiers", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("foo _bar baz123 __init__");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 4; ++i)
        CHECK((*result)[i].type == TokenType::Identifier);
    CHECK((*result)[0].text == "foo");
    CHECK((*result)[1].text == "_bar");
    CHECK((*result)[2].text == "baz123");
    CHECK((*result)[3].text == "__init__");
}

TEST_CASE("PythonTokenizer.DoubleQuoteString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"("hello" "world\n" "escaped\"quote")");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[0].text == "\"hello\"");
    CHECK((*result)[1].type == TokenType::StringLiteral);
    CHECK((*result)[2].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.SingleQuoteString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("'hello' 'world'");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[0].text == "'hello'");
    CHECK((*result)[1].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.TripleQuotedString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"py("""triple
quoted
string""")py");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.TripleSingleQuotedString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("'''triple\nsingle\nquoted'''");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.RawString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"(r"raw\nstring" r'another\tone')");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[1].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.FString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"(f"hello {name}")");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.ByteString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"(b"byte string" b'another')");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[1].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.CombinedPrefixString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"(rb"raw byte" br"byte raw")");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[1].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.NumericLiterals", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("42 3.14 0xFF 0o17 0b1010 1_000_000");
    REQUIRE(result.has_value());

    for (size_t i = 0; i < 6; ++i)
    {
        CAPTURE(i);
        CHECK((*result)[i].type == TokenType::NumericLiteral);
    }
    CHECK((*result)[0].text == "42");
    CHECK((*result)[1].text == "3.14");
    CHECK((*result)[2].text == "0xFF");
    CHECK((*result)[3].text == "0o17");
    CHECK((*result)[4].text == "0b1010");
    CHECK((*result)[5].text == "1_000_000");
}

TEST_CASE("PythonTokenizer.ComplexLiteral", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("3j 3.14j");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::NumericLiteral);
    CHECK((*result)[0].text == "3j");
    CHECK((*result)[1].type == TokenType::NumericLiteral);
    CHECK((*result)[1].text == "3.14j");
}

TEST_CASE("PythonTokenizer.LineComment", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("x # comment\ny");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Identifier);
    CHECK((*result)[1].type == TokenType::LineComment);
    CHECK((*result)[1].text == "# comment");
    CHECK((*result)[2].type == TokenType::Identifier);
}

TEST_CASE("PythonTokenizer.PythonOperators", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("** // @ := @= **= //=");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::Python_StarStar);
    CHECK(t[1].type == TokenType::Python_FloorDiv);
    CHECK(t[2].type == TokenType::Python_At);
    CHECK(t[3].type == TokenType::Python_Walrus);
    CHECK(t[4].type == TokenType::Python_AtEqual);
    CHECK(t[5].type == TokenType::Python_StarStarEqual);
    CHECK(t[6].type == TokenType::Python_FloorDivEqual);
}

TEST_CASE("PythonTokenizer.SharedOperators", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("+ - * / % = == != < > <= >= & | ^ ~");
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
    CHECK(t[12].type == TokenType::Amp);
    CHECK(t[13].type == TokenType::Pipe);
    CHECK(t[14].type == TokenType::Caret);
    CHECK(t[15].type == TokenType::Tilde);
}

TEST_CASE("PythonTokenizer.ArrowOperator", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("-> ...");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Arrow);
    CHECK((*result)[1].type == TokenType::Ellipsis);
}

TEST_CASE("PythonTokenizer.FullFunction", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"py(
def process_data(input):
    result = input * 2
    if result > 100:
        result = result - 50
    return result
)py");
    REQUIRE(result.has_value());

    auto const& t = *result;
    size_t i = 0;
    CHECK(t[i++].type == TokenType::Python_Def);
    CHECK(t[i++].type == TokenType::Identifier); // process_data
    CHECK(t[i++].type == TokenType::LeftParen);
    CHECK(t[i++].type == TokenType::Identifier); // input
    CHECK(t[i++].type == TokenType::RightParen);
    CHECK(t[i++].type == TokenType::Colon);
}

TEST_CASE("PythonTokenizer.UnterminatedString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("\"unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("PythonTokenizer.UnterminatedTripleQuotedString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"("""unterminated)");
    CHECK(!result.has_value());
}

TEST_CASE("PythonTokenizer.AssignmentOperators", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("+= -= *= /= %= &= |= ^= <<= >>=");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::PlusEqual);
    CHECK(t[1].type == TokenType::MinusEqual);
    CHECK(t[2].type == TokenType::StarEqual);
    CHECK(t[3].type == TokenType::SlashEqual);
    CHECK(t[4].type == TokenType::PercentEqual);
    CHECK(t[5].type == TokenType::AmpEqual);
    CHECK(t[6].type == TokenType::PipeEqual);
    CHECK(t[7].type == TokenType::CaretEqual);
    CHECK(t[8].type == TokenType::LessLessEqual);
    CHECK(t[9].type == TokenType::GreaterGreaterEqual);
}

TEST_CASE("PythonTokenizer.Punctuation", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("( ) [ ] { } : , . ;");
    REQUIRE(result.has_value());

    auto const& t = *result;
    CHECK(t[0].type == TokenType::LeftParen);
    CHECK(t[1].type == TokenType::RightParen);
    CHECK(t[2].type == TokenType::LeftBracket);
    CHECK(t[3].type == TokenType::RightBracket);
    CHECK(t[4].type == TokenType::LeftBrace);
    CHECK(t[5].type == TokenType::RightBrace);
    CHECK(t[6].type == TokenType::Colon);
    CHECK(t[7].type == TokenType::Comma);
    CHECK(t[8].type == TokenType::Dot);
    CHECK(t[9].type == TokenType::Semicolon);
}

TEST_CASE("PythonTokenizer.PrefixedTripleQuotedString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"py(r"""raw triple
quoted""")py");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.CombinedPrefixRfFr", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"py(rf"hello {x}" fr"hello {x}")py");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
    CHECK((*result)[1].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.ByteTripleQuotedString", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("b\"\"\"bytes\ncontent\"\"\"");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.FStringTripleQuoted", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("f\"\"\"formatted\n{value}\"\"\"");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.RawTripleSingleQuoted", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("r'''raw\nsingle'''");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::StringLiteral);
}

TEST_CASE("PythonTokenizer.UppercaseOctal", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("0O17");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::NumericLiteral);
    CHECK((*result)[0].text == "0O17");
}

TEST_CASE("PythonTokenizer.UppercaseComplex", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("3J");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::NumericLiteral);
    CHECK((*result)[0].text == "3J");
}

TEST_CASE("PythonTokenizer.ExponentWithSign", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("1.5e+10 2e-3");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::NumericLiteral);
    CHECK((*result)[0].text == "1.5e+10");
    CHECK((*result)[1].type == TokenType::NumericLiteral);
    CHECK((*result)[1].text == "2e-3");
}

TEST_CASE("PythonTokenizer.TypeKeyword", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("type Point = tuple");
    REQUIRE(result.has_value());

    CHECK((*result)[0].type == TokenType::Python_Type);
}

TEST_CASE("PythonTokenizer.UnterminatedSingleQuote", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize("'unterminated");
    CHECK(!result.has_value());
}

TEST_CASE("PythonTokenizer.UnterminatedPrefixedTripleQuoted", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(R"(r"""unterminated)");
    CHECK(!result.has_value());
}

TEST_CASE("PythonTokenizer.DotStartedNumeric", "[python][tokenizer]")
{
    auto result = PythonLanguage{}.Tokenize(".5");
    REQUIRE(result.has_value());
    CHECK((*result)[0].type == TokenType::NumericLiteral);
}
