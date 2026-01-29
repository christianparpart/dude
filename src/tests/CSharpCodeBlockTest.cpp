// SPDX-License-Identifier: Apache-2.0
#include <codedup/CodeBlock.hpp>
#include <codedup/Languages/CSharpLanguage.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace codedup;

namespace
{

auto ExtractBlocks(std::string_view source, size_t minTokens = 5) -> std::vector<CodeBlock>
{
    CSharpLanguage const csharp;
    auto tokens = csharp.Tokenize(source);
    if (!tokens)
        return {};

    TokenNormalizer normalizer;
    auto normalized = normalizer.Normalize(*tokens, &csharp);

    return csharp.ExtractBlocks(*tokens, normalized, {}, {.minTokens = minTokens});
}

} // namespace

TEST_CASE("CSharpCodeBlock.SimpleMethod", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public void Process(int x) {
    if (x > 0) {
        return;
    }
    var y = x + 1;
    var z = y * 2;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "Process");
}

TEST_CASE("CSharpCodeBlock.MultipleMethods", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public void Foo(int x) {
    var a = x + 1;
    var b = a + 2;
    var c = b + 3;
}

public void Bar(int y) {
    var a = y + 1;
    var b = a + 2;
    var c = b + 3;
}
)cs");

    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].name == "Foo");
    CHECK(blocks[1].name == "Bar");
}

TEST_CASE("CSharpCodeBlock.AsyncMethod", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public async void LoadData(string url) {
    var data = await FetchAsync(url);
    if (data != null) {
        Process(data);
    }
    var result = Transform(data);
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "LoadData");
}

TEST_CASE("CSharpCodeBlock.StaticMethod", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public static int Calculate(int x) {
    var a = x + 1;
    var b = a * 2;
    var c = b - 3;
    return c;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "Calculate");
}

TEST_CASE("CSharpCodeBlock.RejectsClassBody", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
class MyClass {
    int x;
    int y;
    int z;
}
)cs");

    CHECK(blocks.empty());
}

TEST_CASE("CSharpCodeBlock.RejectsNamespaceBody", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
namespace MyApp {
    class Inner {
        int field;
    }
}
)cs");

    CHECK(blocks.empty());
}

TEST_CASE("CSharpCodeBlock.RejectsInterfaceBody", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
interface IMyInterface {
    void DoWork();
    int Count();
    string Name();
}
)cs");

    CHECK(blocks.empty());
}

TEST_CASE("CSharpCodeBlock.MinTokenFiltering", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks("public void Tiny() { return; }", 100);
    CHECK(blocks.empty());
}

TEST_CASE("CSharpCodeBlock.NormalizedIds", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public void Foo(int x) {
    var a = x + 1;
    var b = a + 2;
    var c = b + 3;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(!blocks[0].normalizedIds.empty());
}
