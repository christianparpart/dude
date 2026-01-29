// SPDX-License-Identifier: Apache-2.0
#include <codedup/LanguageRegistry.hpp>

#include <algorithm>

// Forward declarations for built-in languages.
namespace codedup
{
auto CreateCppLanguage() -> std::shared_ptr<Language>;
auto CreateCSharpLanguage() -> std::shared_ptr<Language>;
auto CreatePythonLanguage() -> std::shared_ptr<Language>;
} // namespace codedup

namespace codedup
{

auto LanguageRegistry::Instance() -> LanguageRegistry&
{
    static LanguageRegistry registry = []()
    {
        LanguageRegistry r;
        r.Register(CreateCppLanguage());
        r.Register(CreateCSharpLanguage());
        r.Register(CreatePythonLanguage());
        return r;
    }();
    return registry;
}

void LanguageRegistry::Register(std::shared_ptr<Language> language)
{
    _languages.push_back(std::move(language));
}

auto LanguageRegistry::FindByExtension(std::string_view extension) const -> Language const*
{
    auto lowerExt = std::string(extension);
    std::ranges::transform(lowerExt, lowerExt.begin(), [](unsigned char c) { return std::tolower(c); });

    for (auto const& lang : _languages)
    {
        for (auto const& ext : lang->Extensions())
        {
            auto lowerLangExt = ext;
            std::ranges::transform(lowerLangExt, lowerLangExt.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lowerExt == lowerLangExt)
                return lang.get();
        }
    }
    return nullptr;
}

auto LanguageRegistry::FindByPath(std::filesystem::path const& filePath) const -> Language const*
{
    return FindByExtension(filePath.extension().string());
}

auto LanguageRegistry::FindByName(std::string_view name) const -> Language const*
{
    for (auto const& lang : _languages)
    {
        if (lang->Name() == name)
            return lang.get();
    }
    return nullptr;
}

auto LanguageRegistry::AllExtensions() const -> std::vector<std::string>
{
    std::vector<std::string> result;
    for (auto const& lang : _languages)
    {
        for (auto const& ext : lang->Extensions())
            result.push_back(ext);
    }
    return result;
}

auto LanguageRegistry::AllLanguages() const -> std::vector<Language const*>
{
    std::vector<Language const*> result;
    result.reserve(_languages.size());
    for (auto const& lang : _languages)
        result.push_back(lang.get());
    return result;
}

} // namespace codedup
