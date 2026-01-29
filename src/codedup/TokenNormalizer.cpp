// SPDX-License-Identifier: Apache-2.0
#include <codedup/Language.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <ranges>

namespace codedup
{

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto TokenNormalizer::Normalize(std::vector<Token> const& tokens, Language const* language)
    -> std::vector<NormalizedToken>
{
    std::vector<NormalizedToken> result;
    result.reserve(tokens.size());

    for (auto const i : std::views::iota(size_t{0}, tokens.size()))
    {
        auto const& token = tokens[i];

        // Strip tokens based on language or default behavior
        if (language ? language->ShouldStripToken(token.type) : DefaultShouldStrip(token.type))
            continue;

        auto const id = AssignId(token.type);
        result.push_back(NormalizedToken{.id = id, .originalIndex = i});
    }

    return result;
}

auto TokenNormalizer::NormalizeTextPreserving(std::vector<Token> const& tokens, Language const* language)
    -> std::vector<NormalizedToken>
{
    std::vector<NormalizedToken> result;
    result.reserve(tokens.size());

    for (auto const i : std::views::iota(size_t{0}, tokens.size()))
    {
        auto const& token = tokens[i];

        // Strip tokens based on language or default behavior
        if (language ? language->ShouldStripToken(token.type) : DefaultShouldStrip(token.type))
            continue;

        auto const id = AssignTextPreservingId(token);
        result.push_back(NormalizedToken{.id = id, .originalIndex = i});
    }

    return result;
}

auto TokenNormalizer::DefaultShouldStrip(TokenType type) -> bool
{
    return IsComment(type) || type == TokenType::PreprocessorDirective || type == TokenType::EndOfFile;
}

auto TokenNormalizer::AssignTextPreservingId(Token const& token) -> NormalizedTokenId
{
    // Keywords and operators get the same deterministic IDs as structural mode
    if (token.type != TokenType::Identifier && !IsLiteral(token.type))
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

auto TokenNormalizer::AssignId(TokenType type) -> NormalizedTokenId
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
