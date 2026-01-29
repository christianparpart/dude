// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/Token.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace codedup
{

/// @brief Normalized token ID used for clone detection.
using NormalizedTokenId = uint32_t;

/// @brief A normalized token with its ID and a back-reference to the original token index.
struct NormalizedToken
{
    NormalizedTokenId id = 0; ///< The normalized token ID.
    size_t originalIndex = 0; ///< Index into the original token vector.
};

/// @brief Generic placeholder IDs used during normalization.
enum class GenericId : uint16_t
{
    Identifier = 1000,     ///< Generic ID for all identifiers.
    NumericLiteral = 1001, ///< Generic ID for all numeric literals.
    StringLiteral = 1002,  ///< Generic ID for all string literals.
    CharLiteral = 1003,    ///< Generic ID for all character literals.
};

/// @brief Bijective mapping between token descriptions and their normalized IDs.
struct TokenDictionary
{
    std::unordered_map<NormalizedTokenId, std::string> idToName; ///< Mapping from ID to human-readable name.
    std::unordered_map<std::string, NormalizedTokenId> nameToId; ///< Mapping from name to normalized ID.
    NormalizedTokenId nextId = 2000;                             ///< Next available ID (IDs below 2000 are reserved).
};

/// @brief Normalizes tokens into integer sequences for Type-2 clone detection.
///
/// Normalization rules:
/// - Comments and preprocessor directives are stripped.
/// - Each keyword and operator maps to a unique fixed ID.
/// - All identifiers map to a single generic ID.
/// - All numeric, string, and char literals map to their respective generic IDs.
class CODEDUP_API TokenNormalizer
{
public:
    /// @brief Normalizes a sequence of tokens (structural mode).
    ///
    /// All identifiers collapse to a single generic ID, and all literals of each kind
    /// collapse to their respective generic IDs. This produces identical sequences for
    /// Type-2 (renamed) clones.
    /// @param tokens The original token sequence.
    /// @return A vector of normalized tokens (comments and preprocessor stripped).
    [[nodiscard]] auto Normalize(std::vector<Token> const& tokens) -> std::vector<NormalizedToken>;

    /// @brief Normalizes a sequence of tokens (text-preserving mode).
    ///
    /// Keywords and operators get the same IDs as structural mode, but each unique
    /// identifier or literal text receives its own distinct ID. This allows computing
    /// a textual similarity score that distinguishes renamed identifiers.
    /// @param tokens The original token sequence.
    /// @return A vector of normalized tokens with text-preserving IDs.
    [[nodiscard]] auto NormalizeTextPreserving(std::vector<Token> const& tokens) -> std::vector<NormalizedToken>;

    /// @brief Returns the token dictionary built during normalization.
    [[nodiscard]] auto Dictionary() const -> TokenDictionary const& { return _dictionary; }

private:
    TokenDictionary _dictionary;
    TokenDictionary _textPreservingDictionary;

    [[nodiscard]] static auto AssignId(TokenType type) -> NormalizedTokenId;
    [[nodiscard]] auto AssignTextPreservingId(Token const& token) -> NormalizedTokenId;
};

} // namespace codedup
