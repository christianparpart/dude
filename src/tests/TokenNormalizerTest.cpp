// SPDX-License-Identifier: Apache-2.0
#include <dude/Languages/CppLanguage.hpp>
#include <dude/TokenNormalizer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

TEST_CASE("TokenNormalizer.IdentifierNormalization", "[normalizer]")
{
    // Different identifier names should produce the same normalized ID
    CppLanguage const cpp;
    auto tokens1 = cpp.Tokenize("int foo = 42;");
    auto tokens2 = cpp.Tokenize("int bar = 42;");
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer norm1;
    TokenNormalizer norm2;
    auto const normalized1 = norm1.Normalize(*tokens1);
    auto const normalized2 = norm2.Normalize(*tokens2);

    REQUIRE(normalized1.size() == normalized2.size());

    for (size_t i = 0; i < normalized1.size(); ++i)
        CHECK(normalized1[i].id == normalized2[i].id);
}

TEST_CASE("TokenNormalizer.KeywordsAreUnique", "[normalizer]")
{
    auto tokens = CppLanguage{}.Tokenize("if else for while return");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const normalized = normalizer.Normalize(*tokens);

    // Each keyword should have a different ID
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        for (size_t j = i + 1; j < normalized.size(); ++j)
        {
            CHECK(normalized[i].id != normalized[j].id);
        }
    }
}

TEST_CASE("TokenNormalizer.LiteralNormalization", "[normalizer]")
{
    // Different numeric literal values should produce the same normalized ID
    CppLanguage const cpp;
    auto tokens1 = cpp.Tokenize("42");
    auto tokens2 = cpp.Tokenize("99");
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer n1;
    TokenNormalizer n2;
    auto const norm1 = n1.Normalize(*tokens1);
    auto const norm2 = n2.Normalize(*tokens2);

    REQUIRE(norm1.size() == 1);
    REQUIRE(norm2.size() == 1);
    CHECK(norm1[0].id == norm2[0].id);
    CHECK(norm1[0].id == static_cast<NormalizedTokenId>(GenericId::NumericLiteral));
}

TEST_CASE("TokenNormalizer.StringLiteralNormalization", "[normalizer]")
{
    CppLanguage const cpp;
    auto tokens1 = cpp.Tokenize(R"("hello")");
    auto tokens2 = cpp.Tokenize(R"("world")");
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer n1;
    TokenNormalizer n2;
    auto const norm1 = n1.Normalize(*tokens1);
    auto const norm2 = n2.Normalize(*tokens2);

    REQUIRE(norm1.size() == 1);
    CHECK(norm1[0].id == norm2[0].id);
    CHECK(norm1[0].id == static_cast<NormalizedTokenId>(GenericId::StringLiteral));
}

TEST_CASE("TokenNormalizer.CommentStripping", "[normalizer]")
{
    auto tokens = CppLanguage{}.Tokenize("int x; // comment\nint y; /* block */");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const normalized = normalizer.Normalize(*tokens);

    // Comments should be stripped; only int x ; int y ;
    for (auto const& nt : normalized)
    {
        auto const& orig = (*tokens)[nt.originalIndex];
        CHECK(!IsComment(orig.type));
    }
}

TEST_CASE("TokenNormalizer.PreprocessorStripping", "[normalizer]")
{
    auto tokens = CppLanguage{}.Tokenize("#include <vector>\nint x;");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const normalized = normalizer.Normalize(*tokens);

    for (auto const& nt : normalized)
    {
        auto const& orig = (*tokens)[nt.originalIndex];
        CHECK(orig.type != TokenType::PreprocessorDirective);
    }
}

TEST_CASE("TokenNormalizer.StructuralEquivalence", "[normalizer]")
{
    // Two structurally identical functions with different names should produce identical normalized sequences
    CppLanguage const cpp;
    auto tokens1 = cpp.Tokenize("void foo(int x) { return x + 1; }");
    auto tokens2 = cpp.Tokenize("void bar(int y) { return y + 1; }");
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer n1;
    TokenNormalizer n2;
    auto const norm1 = n1.Normalize(*tokens1);
    auto const norm2 = n2.Normalize(*tokens2);

    REQUIRE(norm1.size() == norm2.size());
    for (size_t i = 0; i < norm1.size(); ++i)
        CHECK(norm1[i].id == norm2[i].id);
}

TEST_CASE("TokenNormalizer.BackReference", "[normalizer]")
{
    auto tokens = CppLanguage{}.Tokenize("int x = 5;");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const normalized = normalizer.Normalize(*tokens);

    // Each normalized token should reference a valid original token
    for (auto const& nt : normalized)
    {
        REQUIRE(nt.originalIndex < tokens->size());
    }
}

// ============================================================================================
// Text-Preserving Normalization Tests
// ============================================================================================

TEST_CASE("TokenNormalizer.TextPreserving.DifferentIdentifiersGetDifferentIds", "[normalizer]")
{
    // In text-preserving mode, "foo" and "bar" should get different IDs
    auto tokens = CppLanguage{}.Tokenize("int foo = bar;");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const tp = normalizer.NormalizeTextPreserving(*tokens);

    // Tokens: int(keyword), foo(ident), =(op), bar(ident), ;(op)
    // Find the two identifier tokens
    NormalizedTokenId fooId = 0;
    NormalizedTokenId barId = 0;
    for (auto const& nt : tp)
    {
        auto const& orig = (*tokens)[nt.originalIndex];
        if (orig.text == "foo")
            fooId = nt.id;
        else if (orig.text == "bar")
            barId = nt.id;
    }
    CHECK(fooId != 0);
    CHECK(barId != 0);
    CHECK(fooId != barId);
}

TEST_CASE("TokenNormalizer.TextPreserving.SameIdentifierGetsSameId", "[normalizer]")
{
    // In text-preserving mode, two occurrences of "foo" should get the same ID
    auto tokens = CppLanguage{}.Tokenize("int foo = foo;");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const tp = normalizer.NormalizeTextPreserving(*tokens);

    std::vector<NormalizedTokenId> fooIds;
    for (auto const& nt : tp)
    {
        auto const& orig = (*tokens)[nt.originalIndex];
        if (orig.text == "foo")
            fooIds.push_back(nt.id);
    }
    REQUIRE(fooIds.size() == 2);
    CHECK(fooIds[0] == fooIds[1]);
}

TEST_CASE("TokenNormalizer.TextPreserving.KeywordsMatchStructuralIds", "[normalizer]")
{
    // Keywords should get the same IDs in both modes
    auto tokens = CppLanguage{}.Tokenize("if else for while return");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const structural = normalizer.Normalize(*tokens);
    auto const textPreserving = normalizer.NormalizeTextPreserving(*tokens);

    REQUIRE(structural.size() == textPreserving.size());
    for (size_t i = 0; i < structural.size(); ++i)
        CHECK(structural[i].id == textPreserving[i].id);
}

TEST_CASE("TokenNormalizer.TextPreserving.DifferentLiteralsGetDifferentIds", "[normalizer]")
{
    // Different numeric literal values should get different text-preserving IDs
    auto tokens = CppLanguage{}.Tokenize("42 + 99");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const tp = normalizer.NormalizeTextPreserving(*tokens);

    // Find the two literal tokens
    std::vector<NormalizedTokenId> litIds;
    for (auto const& nt : tp)
    {
        auto const& orig = (*tokens)[nt.originalIndex];
        if (orig.type == TokenType::NumericLiteral)
            litIds.push_back(nt.id);
    }
    REQUIRE(litIds.size() == 2);
    CHECK(litIds[0] != litIds[1]);
}

TEST_CASE("TokenNormalizer.TextPreserving.RenamedFunctionsDiffer", "[normalizer]")
{
    // Structurally identical functions with different names should produce
    // identical structural IDs but different text-preserving IDs.
    // Both files must be processed by the same normalizer (as in production usage)
    // so that different identifier texts get different IDs from the shared dictionary.
    CppLanguage const cpp;
    auto tokens1 = cpp.Tokenize("void foo(int x) { return x + 1; }");
    auto tokens2 = cpp.Tokenize("void bar(int y) { return y + 1; }");
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer normalizer;
    auto const struct1 = normalizer.Normalize(*tokens1);
    auto const struct2 = normalizer.Normalize(*tokens2);
    auto const tp1 = normalizer.NormalizeTextPreserving(*tokens1);
    auto const tp2 = normalizer.NormalizeTextPreserving(*tokens2);

    // Structural: identical
    REQUIRE(struct1.size() == struct2.size());
    for (size_t i = 0; i < struct1.size(); ++i)
        CHECK(struct1[i].id == struct2[i].id);

    // Text-preserving: should differ at identifier positions (foo != bar, x != y)
    REQUIRE(tp1.size() == tp2.size());
    bool anyDifferent = false;
    for (size_t i = 0; i < tp1.size(); ++i)
    {
        if (tp1[i].id != tp2[i].id)
            anyDifferent = true;
    }
    CHECK(anyDifferent);
}

// ---------------------------------------------------------------------------
// Coverage: CharLiteral normalization (TokenNormalizer.cpp L86-87)
// ---------------------------------------------------------------------------

TEST_CASE("TokenNormalizer.CharLiteralNormalization", "[normalizer]")
{
    CppLanguage const cpp;
    auto tokens1 = cpp.Tokenize("'a'");
    auto tokens2 = cpp.Tokenize("'z'");
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer n1;
    TokenNormalizer n2;
    auto const norm1 = n1.Normalize(*tokens1);
    auto const norm2 = n2.Normalize(*tokens2);

    REQUIRE(norm1.size() == 1);
    REQUIRE(norm2.size() == 1);
    CHECK(norm1[0].id == norm2[0].id);
    CHECK(norm1[0].id == static_cast<NormalizedTokenId>(GenericId::CharLiteral));
}

// ---------------------------------------------------------------------------
// Coverage: Dictionary() accessor (TokenNormalizer.hpp L78)
// ---------------------------------------------------------------------------

TEST_CASE("TokenNormalizer.DictionaryAccessor", "[normalizer]")
{
    auto tokens = CppLanguage{}.Tokenize("int x = 5;");
    REQUIRE(tokens.has_value());

    TokenNormalizer normalizer;
    auto const normalized = normalizer.Normalize(*tokens);
    CHECK(!normalized.empty());

    auto const& dict = normalizer.Dictionary();
    // The structural dictionary does not populate idToName (deterministic IDs from enum),
    // but the accessor should be callable and return a valid reference.
    CHECK(dict.nextId == 2000);
}
