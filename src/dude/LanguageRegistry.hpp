// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>
#include <dude/Language.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dude
{

/// @brief Singleton registry that maps file extensions to Language implementations.
///
/// Auto-registers built-in languages (C++, C#) on first access. Provides lookup
/// by extension, file path, or language name.
class DUDE_API LanguageRegistry
{
public:
    /// @brief Returns the singleton instance, auto-registering built-in languages on first call.
    [[nodiscard]] static auto Instance() -> LanguageRegistry&;

    /// @brief Registers a language implementation.
    /// @param language Shared pointer to the language implementation.
    void Register(std::shared_ptr<Language> language);

    /// @brief Finds a language by file extension (e.g., ".cpp", ".cs").
    /// @param extension The file extension including the leading dot.
    /// @return Pointer to the matching language, or nullptr if not found.
    [[nodiscard]] auto FindByExtension(std::string_view extension) const -> Language const*;

    /// @brief Finds a language by file path (using the path's extension).
    /// @param filePath The file path to look up.
    /// @return Pointer to the matching language, or nullptr if not found.
    [[nodiscard]] auto FindByPath(std::filesystem::path const& filePath) const -> Language const*;

    /// @brief Finds a language by name (e.g., "C++", "C#").
    /// @param name The language name to look up.
    /// @return Pointer to the matching language, or nullptr if not found.
    [[nodiscard]] auto FindByName(std::string_view name) const -> Language const*;

    /// @brief Returns a flattened list of all registered file extensions.
    [[nodiscard]] auto AllExtensions() const -> std::vector<std::string>;

    /// @brief Returns all registered language implementations.
    [[nodiscard]] auto AllLanguages() const -> std::vector<Language const*>;

private:
    LanguageRegistry() = default;

    std::vector<std::shared_ptr<Language>> _languages;
};

} // namespace dude
