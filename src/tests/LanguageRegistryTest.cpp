// SPDX-License-Identifier: Apache-2.0
#include <codedup/LanguageRegistry.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace codedup;

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

TEST_CASE("LanguageRegistry.FindByPath", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();

    auto const* cppLang = registry.FindByPath("src/main.cpp");
    REQUIRE(cppLang != nullptr);
    CHECK(cppLang->Name() == "C++");

    auto const* csLang = registry.FindByPath("src/Program.cs");
    REQUIRE(csLang != nullptr);
    CHECK(csLang->Name() == "C#");
}

TEST_CASE("LanguageRegistry.FindByName", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();

    auto const* cppLang = registry.FindByName("C++");
    REQUIRE(cppLang != nullptr);

    auto const* csLang = registry.FindByName("C#");
    REQUIRE(csLang != nullptr);

    auto const* unknown = registry.FindByName("Java");
    CHECK(unknown == nullptr);
}

TEST_CASE("LanguageRegistry.AllExtensions", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const exts = registry.AllExtensions();

    // Should have C++ extensions + C# extension
    CHECK(exts.size() >= 8); // 7 C++ + 1 C#
    CHECK(std::ranges::find(exts, ".cpp") != exts.end());
    CHECK(std::ranges::find(exts, ".cs") != exts.end());
}

TEST_CASE("LanguageRegistry.AllLanguages", "[registry]")
{
    auto const& registry = LanguageRegistry::Instance();
    auto const languages = registry.AllLanguages();

    CHECK(languages.size() >= 2); // C++ and C#
}
