// SPDX-License-Identifier: Apache-2.0
#include <dude/CodeBlock.hpp>
#include <dude/Languages/CSharpLanguage.hpp>
#include <dude/TokenNormalizer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

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

TEST_CASE("CSharpCodeBlock.GenericMethod", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public void Process<T>(T x) {
    var a = x.ToString();
    var b = a.Length;
    var c = b + 1;
    var d = c * 2;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "Process");
}

TEST_CASE("CSharpCodeBlock.WhereClause", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public void Foo<T>() where T : class {
    var a = default(T);
    var b = a != null;
    var c = b ? 1 : 0;
    var d = c + 1;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "Foo");
}

TEST_CASE("CSharpCodeBlock.ConstructorChainingBase", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public MyClass(int x) : base(x) {
    var a = x + 1;
    var b = a * 2;
    var c = b - 3;
    var d = c + 4;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "MyClass");
}

TEST_CASE("CSharpCodeBlock.ConstructorChainingThis", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
public MyClass() : this(0) {
    var a = 1;
    var b = a + 2;
    var c = b * 3;
    var d = c - 4;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "MyClass");
}

TEST_CASE("CSharpCodeBlock.PropertyAccessor", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
get {
    var a = _value;
    var b = a + 1;
    var c = b * 2;
    return c;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "get");
}

TEST_CASE("CSharpCodeBlock.ExplicitInterfaceImpl", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
void IFoo.Bar(int x) {
    var a = x + 1;
    var b = a * 2;
    var c = b - 3;
    var d = c + 4;
}
)cs");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "IFoo.Bar");
}

TEST_CASE("CSharpCodeBlock.RejectsEnumBody", "[csharp][codeblock]")
{
    auto blocks = ExtractBlocks(R"cs(
enum Color {
    Red,
    Green,
    Blue
}
)cs");

    CHECK(blocks.empty());
}
