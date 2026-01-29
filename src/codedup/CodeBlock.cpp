// SPDX-License-Identifier: Apache-2.0
#include <codedup/CodeBlock.hpp>

#include <algorithm>
#include <ranges>

namespace codedup
{

auto CodeBlockExtractor::extract(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized)
    -> std::vector<CodeBlock>
{
    // Delegate to the 3-parameter overload with an empty text-preserving vector.
    return extract(tokens, normalized, {});
}

auto CodeBlockExtractor::extract(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                 std::vector<NormalizedToken> const& textPreserving) -> std::vector<CodeBlock>
{
    std::vector<CodeBlock> blocks;
    auto const hasTextPreserving = !textPreserving.empty();

    // Build a mapping from original token index to normalized token index.
    // This allows us to find normalized tokens for a given range of original tokens.
    std::vector<size_t> origToNormalized(tokens.size(), SIZE_MAX);
    for (auto const ni : std::views::iota(size_t{0}, normalized.size()))
        origToNormalized[normalized[ni].originalIndex] = ni;

    // Build the same mapping for text-preserving normalization.
    std::vector<size_t> origToTextPreserving;
    if (hasTextPreserving)
    {
        origToTextPreserving.assign(tokens.size(), SIZE_MAX);
        for (auto const ni : std::views::iota(size_t{0}, textPreserving.size()))
            origToTextPreserving[textPreserving[ni].originalIndex] = ni;
    }

    // Scan for function definitions by looking for patterns ending with '{'.
    // We look for: [qualifiers] identifier ( ... ) [qualifiers] { body }
    // The heuristic: find '{' that is preceded (possibly with qualifiers) by ')'.
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type != TokenType::LeftBrace)
            continue;

        // Check if this brace looks like the start of a function body.
        // Look backwards for a ')' possibly separated by qualifiers like const, noexcept, override, etc.
        auto funcName = findFunctionName(tokens, i);
        if (funcName.empty())
            continue; // Not a function body

        // Find matching closing brace
        size_t depth = 1;
        size_t bodyEnd = i + 1;
        while (bodyEnd < tokens.size() && depth > 0)
        {
            if (tokens[bodyEnd].type == TokenType::LeftBrace)
                ++depth;
            else if (tokens[bodyEnd].type == TokenType::RightBrace)
                --depth;
            ++bodyEnd;
        }

        if (depth != 0)
            continue; // Unmatched braces

        // Collect normalized IDs for the block (from '{' to '}' inclusive)
        std::vector<NormalizedTokenId> normalizedIds;
        std::vector<NormalizedTokenId> textPreservingIds;
        for (size_t ti = i; ti < bodyEnd; ++ti)
        {
            if (origToNormalized[ti] != SIZE_MAX)
            {
                auto normIdx = origToNormalized[ti];
                if (normIdx < normalized.size())
                    normalizedIds.push_back(normalized[normIdx].id);
            }

            if (hasTextPreserving && origToTextPreserving[ti] != SIZE_MAX)
            {
                auto tpIdx = origToTextPreserving[ti];
                if (tpIdx < textPreserving.size())
                    textPreservingIds.push_back(textPreserving[tpIdx].id);
            }
        }

        // Apply minimum token filter
        if (normalizedIds.size() < _config.minTokens)
            continue;

        auto const& startLoc = tokens[i].location;
        auto const& endLoc = tokens[bodyEnd - 1].location;

        blocks.push_back(CodeBlock{
            .name = std::move(funcName),
            .sourceRange = SourceRange{.start = startLoc, .end = endLoc},
            .tokenStart = i,
            .tokenEnd = bodyEnd,
            .normalizedIds = std::move(normalizedIds),
            .textPreservingIds = std::move(textPreservingIds),
        });

        // Skip past this block
        i = bodyEnd - 1;
    }

    return blocks;
}

auto CodeBlockExtractor::findFunctionName(std::vector<Token> const& tokens, size_t braceIndex) -> std::string
{
    if (braceIndex == 0)
        return {};

    // Look backwards from the '{', skipping:
    // - Trailing qualifiers: const, noexcept, override, final, volatile, &, &&, ->returnType
    // - Initializer list: : base(args), member(args)
    // - The parameter list: (...)
    // Then we expect to find an identifier (the function name).

    auto pos = braceIndex - 1;

    // Skip trailing qualifiers and attributes
    while (pos > 0)
    {
        auto const type = tokens[pos].type;
        if (type == TokenType::Const || type == TokenType::Noexcept || type == TokenType::Override ||
            type == TokenType::Volatile || type == TokenType::Amp || type == TokenType::AmpAmp ||
            type == TokenType::Arrow || type == TokenType::Identifier)
        {
            // "final" is a context-sensitive keyword, represented as an identifier
            if (type == TokenType::Identifier && tokens[pos].text != "final" && tokens[pos].text != "noexcept")
            {
                // Could be a return type of trailing return - check if preceded by ->
                if (pos > 0 && tokens[pos - 1].type == TokenType::Arrow)
                {
                    --pos; // skip identifier
                    --pos; // skip ->
                    continue;
                }
                break; // This is likely the function name or part of something else
            }
            --pos;
            continue;
        }
        break;
    }

    // Check for constructor initializer lists: `: member(args), member(args)`
    // We need to skip back past these to find the closing ')' of the parameter list
    if (tokens[pos].type == TokenType::RightParen)
    {
        // This could be part of an initializer list member(args)
        // or the actual function parameter list.
        // Let's check - skip this paren group first.
    }
    else if (pos > 0)
    {
        // Try to look for initializer list pattern
        // If the current token is part of an init list, we'd see: ) : member(...), ...
        // But let's just check if we see a ':' pattern for init lists
        auto scanPos = pos;
        // Skip back through potential initializer list
        while (scanPos > 0)
        {
            if (tokens[scanPos].type == TokenType::RightParen)
            {
                // Skip paren group
                size_t depth = 1;
                if (scanPos == 0)
                    break;
                --scanPos;
                while (scanPos > 0 && depth > 0)
                {
                    if (tokens[scanPos].type == TokenType::RightParen)
                        ++depth;
                    else if (tokens[scanPos].type == TokenType::LeftParen)
                        --depth;
                    if (depth > 0)
                        --scanPos;
                }
                if (depth != 0)
                    break;
                // Before '(' should be a member/base name
                if (scanPos > 0)
                    --scanPos;
                continue;
            }
            if (tokens[scanPos].type == TokenType::LeftBrace)
            {
                // Skip brace group (for braced init)
                size_t depth = 1;
                if (scanPos == 0)
                    break;
                --scanPos;
                while (scanPos > 0 && depth > 0)
                {
                    if (tokens[scanPos].type == TokenType::LeftBrace)
                        ++depth;
                    else if (tokens[scanPos].type == TokenType::RightBrace)
                        --depth;
                    if (depth > 0)
                        --scanPos;
                }
                if (scanPos > 0)
                    --scanPos;
                continue;
            }
            if (tokens[scanPos].type == TokenType::Comma || tokens[scanPos].type == TokenType::Identifier ||
                tokens[scanPos].type == TokenType::ColonColon)
            {
                --scanPos;
                continue;
            }
            if (tokens[scanPos].type == TokenType::Colon)
            {
                // Found the ':' starting an initializer list
                pos = scanPos;
                if (pos > 0)
                    --pos;
                break;
            }
            break;
        }
    }

    // Now skip trailing qualifiers again (after skipping init list)
    while (pos > 0)
    {
        auto const type = tokens[pos].type;
        if (type == TokenType::Const || type == TokenType::Noexcept || type == TokenType::Override ||
            type == TokenType::Volatile || type == TokenType::Amp || type == TokenType::AmpAmp)
        {
            --pos;
            continue;
        }
        if (type == TokenType::Identifier && (tokens[pos].text == "final" || tokens[pos].text == "noexcept"))
        {
            --pos;
            continue;
        }
        break;
    }

    // Now we expect ')' for the end of the parameter list
    if (tokens[pos].type != TokenType::RightParen)
        return {};

    // Skip the parameter list (matching parens)
    size_t depth = 1;
    if (pos == 0)
        return {};
    --pos;
    while (pos > 0 && depth > 0)
    {
        if (tokens[pos].type == TokenType::RightParen)
            ++depth;
        else if (tokens[pos].type == TokenType::LeftParen)
            --depth;
        if (depth > 0)
            --pos;
    }

    if (depth != 0 || pos == 0)
        return {};

    // Now look back for the function name
    --pos;

    // Skip template parameters if present
    if (tokens[pos].type == TokenType::Greater)
    {
        size_t angleDepth = 1;
        if (pos == 0)
            return {};
        --pos;
        while (pos > 0 && angleDepth > 0)
        {
            if (tokens[pos].type == TokenType::Greater)
                ++angleDepth;
            else if (tokens[pos].type == TokenType::Less)
                --angleDepth;
            if (angleDepth > 0)
                --pos;
        }
        if (pos > 0)
            --pos;
    }

    // The function name should be an identifier
    // Handle qualified names like Class::method
    std::string name;
    if (tokens[pos].type == TokenType::Identifier)
    {
        name = tokens[pos].text;
        // Check for :: qualification
        if (pos >= 2 && tokens[pos - 1].type == TokenType::ColonColon && tokens[pos - 2].type == TokenType::Identifier)
        {
            name = tokens[pos - 2].text + "::" + name;
        }
    }
    else if (tokens[pos].type == TokenType::Operator)
    {
        // operator overload: operator+, operator==, etc.
        name = "operator";
    }
    else if (tokens[pos].type == TokenType::Tilde && pos > 0 && tokens[pos - 1].type == TokenType::Identifier)
    {
        // Destructor: ~ClassName
        name = "~" + tokens[pos - 1].text;
    }

    // Don't extract namespace blocks, control flow blocks, etc.
    // Only function-like blocks (must have a name)
    if (name.empty())
        return {};

    // Verify it's not a namespace, class, struct, enum body
    // Look further back for these keywords
    auto checkPos = pos;
    while (checkPos > 0)
    {
        --checkPos;
        auto const type = tokens[checkPos].type;
        if (type == TokenType::Namespace)
            return {};
        if (type == TokenType::Class || type == TokenType::Struct || type == TokenType::Enum ||
            type == TokenType::Union)
        {
            // If the class/struct keyword is immediately (or almost) before the name,
            // this is a class body, not a function body
            if (checkPos + 1 == pos || checkPos + 2 == pos)
                return {};
            break;
        }
        // Stop looking if we hit something definitively non-qualifier
        if (type == TokenType::Semicolon || type == TokenType::RightBrace || type == TokenType::LeftBrace)
            break;
        if (isKeyword(type) && type != TokenType::Const && type != TokenType::Static && type != TokenType::Inline &&
            type != TokenType::Virtual && type != TokenType::Explicit && type != TokenType::Constexpr &&
            type != TokenType::Consteval && type != TokenType::Auto && type != TokenType::Template &&
            type != TokenType::Typename && type != TokenType::Void && type != TokenType::Int &&
            type != TokenType::Long && type != TokenType::Short && type != TokenType::Unsigned &&
            type != TokenType::Signed && type != TokenType::Char && type != TokenType::Bool &&
            type != TokenType::Float && type != TokenType::Double && type != TokenType::WcharT &&
            type != TokenType::Char8T && type != TokenType::Char16T && type != TokenType::Char32T &&
            type != TokenType::Extern && type != TokenType::Friend && type != TokenType::Noexcept)
        {
            break;
        }
    }

    return name;
}

} // namespace codedup
