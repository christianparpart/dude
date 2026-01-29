// SPDX-License-Identifier: Apache-2.0
#include <codedup/TokenNormalizer.hpp>

#include <ranges>

namespace codedup
{

auto TokenNormalizer::normalize(std::vector<Token> const& tokens) -> std::vector<NormalizedToken>
{
    std::vector<NormalizedToken> result;
    result.reserve(tokens.size());

    for (auto const i : std::views::iota(size_t{0}, tokens.size()))
    {
        auto const& token = tokens[i];

        // Strip comments and preprocessor directives
        if (isComment(token.type) || token.type == TokenType::PreprocessorDirective ||
            token.type == TokenType::EndOfFile)
        {
            continue;
        }

        auto const id = assignId(token.type);
        result.push_back(NormalizedToken{.id = id, .originalIndex = i});
    }

    return result;
}

auto TokenNormalizer::normalizeTextPreserving(std::vector<Token> const& tokens) -> std::vector<NormalizedToken>
{
    std::vector<NormalizedToken> result;
    result.reserve(tokens.size());

    for (auto const i : std::views::iota(size_t{0}, tokens.size()))
    {
        auto const& token = tokens[i];

        // Strip comments and preprocessor directives (same as structural mode)
        if (isComment(token.type) || token.type == TokenType::PreprocessorDirective ||
            token.type == TokenType::EndOfFile)
        {
            continue;
        }

        auto const id = assignTextPreservingId(token);
        result.push_back(NormalizedToken{.id = id, .originalIndex = i});
    }

    return result;
}

auto TokenNormalizer::assignTextPreservingId(Token const& token) -> NormalizedTokenId
{
    // Keywords and operators get the same deterministic IDs as structural mode
    if (token.type != TokenType::Identifier && !isLiteral(token.type))
        return static_cast<NormalizedTokenId>(token.type);

    // For identifiers and literals, assign a unique ID per unique text
    auto const [it, inserted] =
        _textPreservingDictionary.nameToId.try_emplace(token.text, _textPreservingDictionary.nextId);
    if (inserted)
    {
        _textPreservingDictionary.idToName[_textPreservingDictionary.nextId] = token.text;
        ++_textPreservingDictionary.nextId;
    }
    return it->second;
}

auto TokenNormalizer::assignId(TokenType type) -> NormalizedTokenId
{
    // Generic IDs for literals and identifiers
    switch (type)
    {
        case TokenType::Identifier:
            return static_cast<NormalizedTokenId>(GenericId::Identifier);
        case TokenType::NumericLiteral:
            return static_cast<NormalizedTokenId>(GenericId::NumericLiteral);
        case TokenType::StringLiteral:
            return static_cast<NormalizedTokenId>(GenericId::StringLiteral);
        case TokenType::CharLiteral:
            return static_cast<NormalizedTokenId>(GenericId::CharLiteral);
        default:
            break;
    }

    // For keywords and operators, use a deterministic ID based on the enum value.
    // This guarantees each keyword/operator gets a unique, stable ID.
    return static_cast<NormalizedTokenId>(type);
}

} // namespace codedup
