// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/SourceLocation.hpp>
#include <codedup/Token.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace codedup
{

/// @brief Represents an extracted code block (typically a function body).
struct CodeBlock
{
    std::string name;                                 ///< Function/method name (best effort)
    SourceRange sourceRange;                          ///< Source range of the entire block
    size_t tokenStart = 0;                            ///< Start index in the original token vector
    size_t tokenEnd = 0;                              ///< End index (exclusive) in the original token vector
    std::vector<NormalizedTokenId> normalizedIds;     ///< Normalized token ID sequence for this block
    std::vector<NormalizedTokenId> textPreservingIds; ///< Text-preserving normalized IDs (empty if not computed)
};

/// @brief Configuration for block extraction.
struct CodeBlockExtractorConfig
{
    size_t minTokens = 30; ///< Minimum number of tokens for a block to be included.
};

/// @brief Extracts function-level code blocks from tokenized source files.
class CODEDUP_API CodeBlockExtractor
{
public:
    using Config = CodeBlockExtractorConfig;

    explicit CodeBlockExtractor(Config config = {}) : _config(config) {}

    /// @brief Extracts code blocks from tokens and their normalized forms.
    /// @param tokens The original token sequence.
    /// @param normalized The normalized token sequence (structural).
    /// @return A vector of extracted code blocks.
    [[nodiscard]] auto Extract(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized) const
        -> std::vector<CodeBlock>;

    /// @brief Extracts code blocks with both structural and text-preserving normalization.
    /// @param tokens The original token sequence.
    /// @param normalized The structural normalized token sequence.
    /// @param textPreserving The text-preserving normalized token sequence.
    /// @return A vector of extracted code blocks with both ID vectors populated.
    [[nodiscard]] auto Extract(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                               std::vector<NormalizedToken> const& textPreserving) const -> std::vector<CodeBlock>;

private:
    Config _config;

    /// @brief Attempts to determine the function name preceding a brace-delimited block.
    [[nodiscard]] static auto FindFunctionName(std::vector<Token> const& tokens, size_t braceIndex) -> std::string;
};

} // namespace codedup
