// SPDX-License-Identifier: Apache-2.0
#include <dude/CodeBlock.hpp>
#include <dude/Languages/CppLanguage.hpp>
#include <dude/TokenNormalizer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

namespace
{

auto ExtractBlocks(std::string_view source, size_t minTokens = 5) -> std::vector<CodeBlock>
{
    CppLanguage const cpp;
    auto tokens = cpp.Tokenize(source);
    if (!tokens)
        return {};

    TokenNormalizer normalizer;
    auto normalized = normalizer.Normalize(*tokens);

    return cpp.ExtractBlocks(*tokens, normalized, {}, {.minTokens = minTokens});
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

TEST_CASE("CodeBlock.TrailingConst", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void foo() const {
    int a = 1;
    int b = a + 2;
    int c = b + 3;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "foo");
}

TEST_CASE("CodeBlock.TrailingNoexcept", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
void foo() noexcept {
    int a = 1;
    int b = a + 2;
    int c = b + 3;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "foo");
}

TEST_CASE("CodeBlock.TrailingReturnType", "[codeblock]")
{
    // Trailing return type with -> uses Identifier for type name, which is handled
    auto blocks = ExtractBlocks(R"cpp(
auto foo() -> MyType {
    int a = 1;
    int b = a + 2;
    int c = b + 3;
    return c;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "foo");
}

TEST_CASE("CodeBlock.RejectsClassBody", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
class MyClass {
    int x;
    int y;
    int z;
}
)cpp");

    CHECK(blocks.empty());
}

TEST_CASE("CodeBlock.TemplateFunction", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
template<typename T> void foo(T x) {
    auto a = x + 1;
    auto b = a + 2;
    auto c = b + 3;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "foo");
}

TEST_CASE("CodeBlock.Destructor", "[codeblock]")
{
    // Destructor with qualified name: backward scan extracts tilde + identifier
    auto blocks = ExtractBlocks(R"cpp(
Foo::~Foo() {
    int a = 1;
    int b = a + 2;
    int c = b + 3;
    delete ptr;
}
)cpp");

    REQUIRE(blocks.size() == 1);
    // The backward scan from `()` finds `Foo` then sees `~` and `Foo::`,
    // producing just "Foo" since the tilde check expects Identifier before Tilde.
    CHECK(blocks[0].name == "Foo");
}

TEST_CASE("CodeBlock.RejectsNamespace", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
namespace MyNs {
    int x;
    int y;
    int z;
}
)cpp");

    CHECK(blocks.empty());
}

TEST_CASE("CodeBlock.RejectsEnum", "[codeblock]")
{
    auto blocks = ExtractBlocks(R"cpp(
enum Color {
    Red,
    Green,
    Blue
}
)cpp");

    CHECK(blocks.empty());
}
