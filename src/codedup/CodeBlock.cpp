// SPDX-License-Identifier: Apache-2.0
#include <codedup/CodeBlock.hpp>

#include <algorithm>
#include <ranges>
#include <span>
#include <string>

namespace codedup
{

namespace
{

/// @brief Result of a backward paren-matching operation.
struct ParenMatchResult
{
    size_t pos;   ///< The position after matching.
    bool success; ///< Whether matching succeeded.
};

/// @brief Checks whether the given token type is a trailing qualifier keyword.
/// @param type The token type to check.
/// @return True if the type is const, noexcept, override, volatile, &, &&, or ->.
[[nodiscard]] auto IsTrailingQualifier(TokenType type) -> bool
{
    return type == TokenType::Const || type == TokenType::Noexcept || type == TokenType::Override ||
           type == TokenType::Volatile || type == TokenType::Amp || type == TokenType::AmpAmp ||
           type == TokenType::Arrow || type == TokenType::Identifier;
}

/// @brief Checks whether the given token type is a simple trailing qualifier (no Arrow/Identifier).
/// @param type The token type to check.
/// @return True if the type is const, noexcept, override, volatile, &, or &&.
[[nodiscard]] auto IsSimpleTrailingQualifier(TokenType type) -> bool
{
    return type == TokenType::Const || type == TokenType::Noexcept || type == TokenType::Override ||
           type == TokenType::Volatile || type == TokenType::Amp || type == TokenType::AmpAmp;
}

/// @brief Skips trailing qualifiers (const, noexcept, override, final, volatile, &, &&, -> returnType)
///        going backward from the given position.
/// @param tokens The token sequence.
/// @param pos The starting position (inclusive) to scan backward from.
/// @return The new position after skipping trailing qualifiers.
[[nodiscard]] auto SkipTrailingQualifiers(std::span<Token const> tokens, size_t pos) -> size_t
{
    while (pos > 0)
    {
        auto const type = tokens[pos].type;
        if (!IsTrailingQualifier(type))
            break;

        if (type == TokenType::Identifier && tokens[pos].text != "final" && tokens[pos].text != "noexcept")
        {
            // Could be a return type of trailing return - check if preceded by ->
            if (pos > 0 && tokens[pos - 1].type == TokenType::Arrow)
            {
                pos -= 2; // skip identifier and ->
                continue;
            }
            break; // This is likely the function name or part of something else
        }
        --pos;
    }
    return pos;
}

/// @brief Skips simple trailing qualifiers (const, noexcept, override, final, volatile, &, &&)
///        going backward from the given position.
///
/// Unlike SkipTrailingQualifiers, this variant does not handle Arrow or trailing return types.
/// It is used after skipping the initializer list.
/// @param tokens The token sequence.
/// @param pos The starting position (inclusive) to scan backward from.
/// @return The new position after skipping simple trailing qualifiers.
[[nodiscard]] auto SkipSimpleTrailingQualifiers(std::span<Token const> tokens, size_t pos) -> size_t
{
    while (pos > 0)
    {
        auto const type = tokens[pos].type;
        if (IsSimpleTrailingQualifier(type))
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
    return pos;
}

/// @brief Skips a matched pair of delimiters backward (e.g., parentheses or braces).
///
/// Starting at scanPos (which should be a closing delimiter), this scans backward
/// to find the matching opening delimiter. Returns the position of the opening
/// delimiter on success.
/// @param tokens The token sequence.
/// @param scanPos The position of the closing delimiter.
/// @param openType The opening delimiter token type (e.g., LeftParen, RightBrace).
/// @param closeType The closing delimiter token type (e.g., RightParen, LeftBrace).
/// @return A ParenMatchResult with the new position and success flag.
[[nodiscard]] auto SkipMatchedDelimiterBackward(std::span<Token const> tokens, size_t scanPos, TokenType openType,
                                                TokenType closeType) -> ParenMatchResult
{
    size_t depth = 1;
    if (scanPos == 0)
        return {.pos = scanPos, .success = false};
    --scanPos;
    while (scanPos > 0 && depth > 0)
    {
        if (tokens[scanPos].type == closeType)
            ++depth;
        else if (tokens[scanPos].type == openType)
            --depth;
        if (depth > 0)
            --scanPos;
    }
    return {.pos = scanPos, .success = depth == 0};
}

/// @brief Skips a constructor initializer list going backward from the given position.
///
/// Handles patterns like `: member(args), member(args)` and braced initializers.
/// If no initializer list is found, returns the original position unchanged.
/// @param tokens The token sequence.
/// @param pos The starting position (inclusive) to scan backward from.
/// @return The new position after skipping the initializer list.
[[nodiscard]] auto SkipInitializerList(std::span<Token const> tokens, size_t pos) -> size_t
{
    if (tokens[pos].type == TokenType::RightParen)
        return pos; // Could be parameter list directly, don't skip

    if (pos == 0)
        return pos;

    auto scanPos = pos;
    while (scanPos > 0)
    {
        if (tokens[scanPos].type == TokenType::RightParen)
        {
            auto const [newPos, ok] =
                SkipMatchedDelimiterBackward(tokens, scanPos, TokenType::LeftParen, TokenType::RightParen);
            if (!ok)
                break;
            scanPos = newPos;
            if (scanPos > 0)
                --scanPos;
            continue;
        }
        if (tokens[scanPos].type == TokenType::LeftBrace)
        {
            auto const [newPos, ok] =
                SkipMatchedDelimiterBackward(tokens, scanPos, TokenType::RightBrace, TokenType::LeftBrace);
            if (!ok)
                break;
            scanPos = newPos;
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
            return pos;
        }
        break;
    }
    return pos;
}

/// @brief Matches parentheses going backward from the given position.
///
/// Expects tokens[pos] to be a RightParen. Scans backward to find the matching LeftParen.
/// @param tokens The token sequence.
/// @param pos The position of the closing ')'.
/// @return A ParenMatchResult with the position of the opening '(' and success flag.
[[nodiscard]] auto SkipMatchedParensBackward(std::span<Token const> tokens, size_t pos) -> ParenMatchResult
{
    if (tokens[pos].type != TokenType::RightParen)
        return {.pos = pos, .success = false};

    size_t depth = 1;
    if (pos == 0)
        return {.pos = pos, .success = false};
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
        return {.pos = pos, .success = false};

    return {.pos = pos, .success = true};
}

/// @brief Skips template angle brackets going backward from the given position.
///
/// If tokens[pos] is a '>', this scans backward to find the matching '<' and
/// returns the position before it. If tokens[pos] is not '>', returns pos unchanged.
/// @param tokens The token sequence.
/// @param pos The starting position.
/// @return The new position after skipping template parameters, or the original position.
[[nodiscard]] auto SkipTemplateParamsBackward(std::span<Token const> tokens, size_t pos) -> size_t
{
    if (tokens[pos].type != TokenType::Greater)
        return pos;

    size_t angleDepth = 1;
    if (pos == 0)
        return pos;
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
    return pos;
}

/// @brief Extracts the function name (or operator/destructor name) at the given position.
///
/// Handles qualified names (Class::method), operator overloads, and destructors (~ClassName).
/// @param tokens The token sequence.
/// @param pos The position of the expected function name token.
/// @return The extracted function name, or an empty string if none found.
[[nodiscard]] auto ExtractFunctionIdentifier(std::span<Token const> tokens, size_t pos) -> std::string
{
    if (tokens[pos].type == TokenType::Identifier)
    {
        auto name = tokens[pos].text;
        // Check for :: qualification
        if (pos >= 2 && tokens[pos - 1].type == TokenType::ColonColon && tokens[pos - 2].type == TokenType::Identifier)
        {
            name = tokens[pos - 2].text + "::" + name;
        }
        return name;
    }
    if (tokens[pos].type == TokenType::Operator)
    {
        // operator overload: operator+, operator==, etc.
        return "operator";
    }
    if (tokens[pos].type == TokenType::Tilde && pos > 0 && tokens[pos - 1].type == TokenType::Identifier)
    {
        // Destructor: ~ClassName
        return "~" + tokens[pos - 1].text;
    }
    return {};
}

/// @brief Checks whether a keyword allowed as a function return-type qualifier or type specifier.
/// @param type The token type to check.
/// @return True if the keyword is allowed before a function definition.
[[nodiscard]] auto IsAllowedFunctionPrefixKeyword(TokenType type) -> bool
{
    return type == TokenType::Const || type == TokenType::Static || type == TokenType::Inline ||
           type == TokenType::Virtual || type == TokenType::Explicit || type == TokenType::Constexpr ||
           type == TokenType::Consteval || type == TokenType::Auto || type == TokenType::Template ||
           type == TokenType::Typename || type == TokenType::Void || type == TokenType::Int ||
           type == TokenType::Long || type == TokenType::Short || type == TokenType::Unsigned ||
           type == TokenType::Signed || type == TokenType::Char || type == TokenType::Bool ||
           type == TokenType::Float || type == TokenType::Double || type == TokenType::WcharT ||
           type == TokenType::Char8T || type == TokenType::Char16T || type == TokenType::Char32T ||
           type == TokenType::Extern || type == TokenType::Friend || type == TokenType::Noexcept;
}

/// @brief Checks if looking further back from the name position reveals a non-function keyword
///        (namespace, class, struct, enum, union), indicating this brace block is not a function body.
/// @param tokens The token sequence.
/// @param namePos The position of the function name token.
/// @return True if a non-function keyword is found that disqualifies this as a function.
[[nodiscard]] auto IsNonFunctionKeyword(std::span<Token const> tokens, size_t namePos) -> bool
{
    auto checkPos = namePos;
    while (checkPos > 0)
    {
        --checkPos;
        auto const type = tokens[checkPos].type;
        if (type == TokenType::Namespace)
            return true;
        if (type == TokenType::Class || type == TokenType::Struct || type == TokenType::Enum ||
            type == TokenType::Union)
        {
            // If the class/struct keyword is immediately (or almost) before the name,
            // this is a class body, not a function body
            if (checkPos + 1 == namePos || checkPos + 2 == namePos)
                return true;
            return false;
        }
        // Stop looking if we hit something definitively non-qualifier
        if (type == TokenType::Semicolon || type == TokenType::RightBrace || type == TokenType::LeftBrace)
            return false;
        if (IsKeyword(type) && !IsAllowedFunctionPrefixKeyword(type))
            return false;
    }
    return false;
}

} // anonymous namespace

auto CodeBlockExtractor::Extract(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized) const
    -> std::vector<CodeBlock>
{
    // Delegate to the 3-parameter overload with an empty text-preserving vector.
    return Extract(tokens, normalized, {});
}

auto CodeBlockExtractor::Extract(std::vector<Token> const& tokens, std::vector<NormalizedToken> const& normalized,
                                 std::vector<NormalizedToken> const& textPreserving) const -> std::vector<CodeBlock>
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
        auto funcName = FindFunctionName(tokens, i);
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

auto CodeBlockExtractor::FindFunctionName(std::vector<Token> const& tokens, size_t braceIndex) -> std::string
{
    if (braceIndex == 0)
        return {};

    auto const tokenSpan = std::span<Token const>{tokens};

    // Step 1: Skip trailing qualifiers (const, noexcept, override, final, volatile, &, &&, -> returnType)
    auto pos = SkipTrailingQualifiers(tokenSpan, braceIndex - 1);

    // Step 2: Skip constructor initializer list (: member(args), member(args))
    pos = SkipInitializerList(tokenSpan, pos);

    // Step 3: Skip simple trailing qualifiers again (after skipping init list)
    pos = SkipSimpleTrailingQualifiers(tokenSpan, pos);

    // Step 4: Match the parameter list parentheses backward
    auto const [parenPos, parenSuccess] = SkipMatchedParensBackward(tokenSpan, pos);
    if (!parenSuccess)
        return {};
    pos = parenPos - 1;

    // Step 5: Skip template parameters if present
    pos = SkipTemplateParamsBackward(tokenSpan, pos);

    // Step 6: Extract the function identifier (qualified name, operator, destructor)
    auto name = ExtractFunctionIdentifier(tokenSpan, pos);
    if (name.empty())
        return {};

    // Step 7: Verify this is not a namespace/class/struct/enum/union body
    if (IsNonFunctionKeyword(tokenSpan, pos))
        return {};

    return name;
}

} // namespace codedup
