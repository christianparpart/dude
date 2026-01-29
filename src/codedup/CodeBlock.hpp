// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/SourceLocation.hpp>
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

} // namespace codedup
