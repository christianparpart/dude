// SPDX-License-Identifier: Apache-2.0
#include <dude/LanguageRegistry.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace dude;

TEST_CASE("LanguageRegistry.FindByExtension.Cpp", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const* lang = registry.FindByExtension(".cpp");
    REQUIRE(lang != nullptr);
    CHECK(lang->Name() == "C++");
}

TEST_CASE("LanguageRegistry.FindByExtension.CSharp", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const* lang = registry.FindByExtension(".cs");
    REQUIRE(lang != nullptr);
    CHECK(lang->Name() == "C#");
}

TEST_CASE("LanguageRegistry.FindByExtension.CaseInsensitive", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const* lang = registry.FindByExtension(".CPP");
    REQUIRE(lang != nullptr);
    CHECK(lang->Name() == "C++");
}

TEST_CASE("LanguageRegistry.FindByExtension.Unknown", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const* lang = registry.FindByExtension(".xyz");
    CHECK(lang == nullptr);
}

TEST_CASE("LanguageRegistry.FindByExtension.Python", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const* lang = registry.FindByExtension(".py");
    REQUIRE(lang != nullptr);
    CHECK(lang->Name() == "Python");
}

TEST_CASE("LanguageRegistry.FindByExtension.PythonW", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const* lang = registry.FindByExtension(".pyw");
    REQUIRE(lang != nullptr);
    CHECK(lang->Name() == "Python");
}

TEST_CASE("LanguageRegistry.FindByPath", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();

    auto const* cppLang = registry.FindByPath("src/main.cpp");
    REQUIRE(cppLang != nullptr);
    CHECK(cppLang->Name() == "C++");

    auto const* csLang = registry.FindByPath("src/Program.cs");
    REQUIRE(csLang != nullptr);
    CHECK(csLang->Name() == "C#");

    auto const* pyLang = registry.FindByPath("src/main.py");
    REQUIRE(pyLang != nullptr);
    CHECK(pyLang->Name() == "Python");
}

TEST_CASE("LanguageRegistry.FindByName", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();

    auto const* cppLang = registry.FindByName("C++");
    REQUIRE(cppLang != nullptr);

    auto const* csLang = registry.FindByName("C#");
    REQUIRE(csLang != nullptr);

    auto const* pyLang = registry.FindByName("Python");
    REQUIRE(pyLang != nullptr);

    auto const* unknown = registry.FindByName("Java");
    CHECK(unknown == nullptr);
}

TEST_CASE("LanguageRegistry.AllExtensions", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const exts = registry.AllExtensions();

    // Should have C++ extensions + C# extension + Python extensions
    CHECK(exts.size() >= 10); // 7 C++ + 1 C# + 2 Python
    CHECK(std::ranges::find(exts, ".cpp") != exts.end());
    CHECK(std::ranges::find(exts, ".cs") != exts.end());
    CHECK(std::ranges::find(exts, ".py") != exts.end());
    CHECK(std::ranges::find(exts, ".pyw") != exts.end());
}

TEST_CASE("LanguageRegistry.AllLanguages", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const languages = registry.AllLanguages();

    CHECK(languages.size() >= 3); // C++, C#, and Python
}
