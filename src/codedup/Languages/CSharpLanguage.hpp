// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Language.hpp>

#include <memory>

namespace codedup
{

/// @brief C# language implementation for tokenization and block extraction.
///
/// Encapsulates the C# tokenizer (keywords, verbatim strings, interpolated strings,
/// null-conditional/coalescing operators, digit separators with underscores) and
/// the backward-scan method/property extraction heuristic for block extraction.
class CODEDUP_API CSharpLanguage final : public Language
{
public:
    [[nodiscard]] auto Name() const -> std::string_view override;
    [[nodiscard]] auto Extensions() const -> std::span<std::string const> override;

    [[nodiscard]] auto Tokenize(std::string_view source, std::filesystem::path const& filePath = {}) const
        -> std::expected<std::vector<Token>, TokenizerError> override;

    [[nodiscard]] auto TokenizeFile(std::filesystem::path const& filePath,
                                    InputEncoding encoding = InputEncoding::Auto) const
        -> std::expected<std::vector<Token>, TokenizerError> override;

    [[nodiscard]] auto ExtractBlocks(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                     std::vector<NormalizedToken> const& textPreserving,
                                     CodeBlockExtractorConfig const& config) const -> std::vector<CodeBlock> override;

    [[nodiscard]] auto ShouldStripToken(TokenType type) const -> bool override;
};

/// @brief Factory function for creating a CSharpLanguage instance.
[[nodiscard]] auto CreateCSharpLanguage() -> std::shared_ptr<Language>;

} // namespace codedup
