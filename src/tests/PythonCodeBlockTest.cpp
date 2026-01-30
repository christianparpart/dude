// SPDX-License-Identifier: Apache-2.0
#include <dude/CodeBlock.hpp>
#include <dude/Languages/PythonLanguage.hpp>
#include <dude/TokenNormalizer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

namespace
{

auto ExtractBlocks(std::string_view source, size_t minTokens = 5) -> std::vector<CodeBlock>
{
    PythonLanguage const python;
    auto tokens = python.Tokenize(source);
    if (!tokens)
        return {};

    TokenNormalizer normalizer;
    auto normalized = normalizer.Normalize(*tokens, &python);

    return python.ExtractBlocks(*tokens, normalized, {}, {.minTokens = minTokens});
}

} // namespace

TEST_CASE("PythonCodeBlock.SimpleFunction", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
def process(x):
    result = x * 2
    if result > 100:
        result = result - 50
    return result
)py");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "process");
}

TEST_CASE("PythonCodeBlock.MultipleFunctions", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
def foo(x):
    a = x + 1
    b = a + 2
    return b

def bar(y):
    a = y + 1
    b = a + 2
    return b
)py");

    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].name == "foo");
    CHECK(blocks[1].name == "bar");
}

TEST_CASE("PythonCodeBlock.AsyncFunction", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
async def load_data(url):
    data = await fetch(url)
    if data is not None:
        process(data)
    result = transform(data)
    return result
)py");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "load_data");
}

TEST_CASE("PythonCodeBlock.NestedFunction", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
def outer(x):
    a = x + 1
    def inner(y):
        b = y + 2
        c = b + 3
        return c
    result = inner(a)
    return result
)py");

    // The outer function is extracted first; the inner may also be extracted
    REQUIRE(!blocks.empty());
    CHECK(blocks[0].name == "outer");
    if (blocks.size() >= 2)
        CHECK(blocks[1].name == "inner");
}

TEST_CASE("PythonCodeBlock.MethodInsideClass", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
class MyClass:
    def method(self):
        a = self.x + 1
        b = a + 2
        c = b + 3
        return c
)py");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "method");
}

TEST_CASE("PythonCodeBlock.DecoratedFunction", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
@decorator
def decorated(x):
    a = x + 1
    b = a + 2
    c = b + 3
    return c
)py");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "decorated");
}

TEST_CASE("PythonCodeBlock.MinTokenFiltering", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
def tiny():
    pass
)py",
                                100);
    CHECK(blocks.empty());
}

TEST_CASE("PythonCodeBlock.NormalizedIds", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
def foo(x):
    a = x + 1
    b = a + 2
    c = b + 3
    return c
)py");

    REQUIRE(blocks.size() == 1);
    CHECK(!blocks[0].normalizedIds.empty());
}

TEST_CASE("PythonCodeBlock.FunctionWithReturnAnnotation", "[python][codeblock]")
{
    auto blocks = ExtractBlocks(R"py(
def compute(x: int, y: int) -> int:
    a = x + y
    b = a * 2
    c = b - 1
    return c
)py");

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].name == "compute");
}

TEST_CASE("PythonCodeBlock.RejectsClassBody", "[python][codeblock]")
{
    // Class bodies without methods should not produce blocks
    auto blocks = ExtractBlocks(R"py(
class MyClass:
    x = 1
    y = 2
    z = 3
)py");

    CHECK(blocks.empty());
}
