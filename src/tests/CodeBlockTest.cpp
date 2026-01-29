// SPDX-License-Identifier: Apache-2.0
#include <codedup/CodeBlock.hpp>
#include <codedup/TokenNormalizer.hpp>
#include <codedup/Tokenizer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace codedup;

namespace
{

auto ExtractBlocks(std::string_view source, size_t minTokens = 5) -> std::vector<CodeBlock>
{
    auto tokens = Tokenizer::Tokenize(source);
    if (!tokens)
        return {};

    TokenNormalizer normalizer;
    auto normalized = normalizer.Normalize(*tokens);

    CodeBlockExtractor extractor({.minTokens = minTokens});
    return extractor.Extract(*tokens, normalized);
}

} // namespace

TEST_CASE("CodeBlock.SimpleFunction", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void foo(int x) {
    if (x > 0) {
        return;
    }
    int y = x + 1;
    int z = y * 2;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "foo");
}

TEST_CASE("CodeBlock.MultipleFunctions", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void foo(int x) {
    int a = x + 1;
    int b = a + 2;
    int c = b + 3;
}

void bar(int y) {
    int a = y + 1;
    int b = a + 2;
    int c = b + 3;
}
)cpp");

    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].name == "foo");
    CHECK(blocks[1].name == "bar");
}

TEST_CASE("CodeBlock.NestedBraces", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void foo() {
    if (true) {
        for (int i = 0; i < 10; ++i) {
            int x = i;
        }
    }
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "foo");
}

TEST_CASE("CodeBlock.MinTokenFiltering", "[codeblock]")
{
    // With a high minTokens, short functions should be filtered out
    auto blocks = ExtractBlocks("void tiny() { return; }", 100);
    CHECK(blocks.empty());
}

TEST_CASE("CodeBlock.QualifiedName", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void MyClass::method(int x) {
    int a = x + 1;
    int b = a * 2;
    int c = b - 3;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "MyClass::method");
}

TEST_CASE("CodeBlock.NormalizedIds", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void foo(int x) {
    int a = x + 1;
    int b = a + 2;
    int c = b + 3;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(!blocks[0].normalizedIds.empty());
}
