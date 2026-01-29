// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/Encoding.hpp>
#include <codedup/Token.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace codedup
{

/// @brief Abstract base class for language-specific tokenization and block extraction.
///
/// Each language implementation encapsulates its own tokenizer scanner and block
/// extraction heuristics. The LanguageRegistry maps file extensions to Language
/// implementations at runtime.
class CODEDUP_API Language
{
public:
    virtual ~Language() = default;
    Language() = default;
    Language(Language const&) = delete;
    Language(Language&&) = delete;
    auto operator=(Language const&) -> Language& = delete;
    auto operator=(Language&&) -> Language& = delete;

    /// @brief Returns the display name of the language (e.g., "C++", "C#").
    [[nodiscard]] virtual auto Name() const -> std::string_view = 0;

    /// @brief Returns the set of file extensions handled by this language.
    [[nodiscard]] virtual auto Extensions() const -> std::span<std::string const> = 0;

    /// @brief Tokenizes source code from a string.
    /// @param source The source code to tokenize.
    /// @param fileIndex Index into the global file path vector for source location tracking.
    /// @return A vector of tokens on success, or a TokenizerError on failure.
    [[nodiscard]] virtual auto Tokenize(std::string_view source, uint32_t fileIndex = NoFileIndex) const
        -> std::expected<std::vector<Token>, TokenizerError> = 0;

    /// @brief Tokenizes a source file, handling encoding detection/conversion.
    /// @param filePath Path to the source file.
    /// @param fileIndex Index into the global file path vector.
    /// @param encoding The input encoding to use (default: auto-detect).
    /// @return A vector of tokens on success, or a TokenizerError on failure.
    [[nodiscard]] virtual auto TokenizeFile(std::filesystem::path const& filePath, uint32_t fileIndex,
                                            InputEncoding encoding = InputEncoding::Auto) const
        -> std::expected<std::vector<Token>, TokenizerError> = 0;

    /// @brief Extracts function-level code blocks from tokenized source.
    /// @param tokens The original token sequence.
    /// @param normalized The structural normalized token sequence.
    /// @param textPreserving The text-preserving normalized token sequence.
    /// @param config Configuration for block extraction.
    /// @return A vector of extracted code blocks.
    [[nodiscard]] virtual auto
    ExtractBlocks(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                  std::vector<NormalizedToken> const& textPreserving, CodeBlockExtractorConfig const& config) const
        -> std::vector<CodeBlock> = 0;

    /// @brief Returns true if the given token type should be stripped during normalization.
    /// @param type The token type to check.
    /// @return True if the token should be excluded from normalized output.
    [[nodiscard]] virtual auto ShouldStripToken(TokenType type) const -> bool = 0;
};

} // namespace codedup
