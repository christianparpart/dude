// SPDX-License-Identifier: Apache-2.0
#include <codedup/CloneDetector.hpp>
#include <codedup/Reporter.hpp>

#include <format>
#include <iterator>
#include <ranges>
#include <unordered_set>

namespace codedup
{

namespace
{

/// @brief Builds a mapping from normalized-ID position to original token index.
///
/// Iterates through original tokens in [tokenStart, tokenEnd) and collects indices
/// of tokens that survive normalization (i.e., not comments, preprocessor, or EOF).
/// The resulting vector has the same ordering and size as the normalized ID sequence.
/// @param tokens All tokens from the file.
/// @param tokenStart Start index (inclusive) in the token vector.
/// @param tokenEnd End index (exclusive) in the token vector.
/// @return Vector where element i maps normalized position i to the original token index.
auto buildNormToOrigMap(std::vector<Token> const& tokens, size_t tokenStart, size_t tokenEnd) -> std::vector<size_t>
{
    std::vector<size_t> map;
    for (auto const i : std::views::iota(tokenStart, std::min(tokenEnd, tokens.size())))
    {
        if (!isComment(tokens[i].type) && tokens[i].type != TokenType::PreprocessorDirective &&
            tokens[i].type != TokenType::EndOfFile)
            map.push_back(i);
    }
    return map;
}

/// @brief Computes original token indices that should be background-highlighted as differing.
///
/// Aligns the normalized ID sequences of the given block and a reference block using LCS,
/// then maps unmatched positions in the block back to original token indices.
/// @param block The code block whose tokens to highlight.
/// @param referenceBlock The reference block to compare against.
/// @param tokens All tokens from the block's file.
/// @return Set of original token indices that differ from the reference.
auto computeHighlightSet(CodeBlock const& block, CodeBlock const& referenceBlock, std::vector<Token> const& tokens)
    -> std::unordered_set<size_t>
{
    // Choose text-preserving IDs if available, else structural
    auto const& idsA =
        referenceBlock.textPreservingIds.empty() ? referenceBlock.normalizedIds : referenceBlock.textPreservingIds;
    auto const& idsB = block.textPreservingIds.empty() ? block.normalizedIds : block.textPreservingIds;

    auto const alignment = CloneDetector::computeLcsAlignment(idsA, idsB);
    auto const normToOrig = buildNormToOrigMap(tokens, block.tokenStart, block.tokenEnd);

    std::unordered_set<size_t> highlighted;
    for (auto const i : std::views::iota(size_t{0}, std::min(alignment.matchedB.size(), normToOrig.size())))
    {
        if (!alignment.matchedB[i])
            highlighted.insert(normToOrig[i]);
    }
    return highlighted;
}

/// @brief Computes highlight set for the reference block (compared against the second block).
auto computeHighlightSetForReference(CodeBlock const& referenceBlock, CodeBlock const& secondBlock,
                                     std::vector<Token> const& tokens) -> std::unordered_set<size_t>
{
    auto const& idsA =
        referenceBlock.textPreservingIds.empty() ? referenceBlock.normalizedIds : referenceBlock.textPreservingIds;
    auto const& idsB =
        secondBlock.textPreservingIds.empty() ? secondBlock.normalizedIds : secondBlock.textPreservingIds;

    auto const alignment = CloneDetector::computeLcsAlignment(idsA, idsB);
    auto const normToOrig = buildNormToOrigMap(tokens, referenceBlock.tokenStart, referenceBlock.tokenEnd);

    std::unordered_set<size_t> highlighted;
    for (auto const i : std::views::iota(size_t{0}, std::min(alignment.matchedA.size(), normToOrig.size())))
    {
        if (!alignment.matchedA[i])
            highlighted.insert(normToOrig[i]);
    }
    return highlighted;
}

} // namespace

void Reporter::report(std::string& out, std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                      std::vector<std::vector<Token>> const& allTokens,
                      std::vector<size_t> const& blockToFileIndex) const
{
    auto inserter = std::back_inserter(out);

    for (auto const gi : std::views::iota(size_t{0}, groups.size()))
    {
        auto const& group = groups[gi];

        // Group header
        if (_config.useColor)
            out += "\033[1;33m"; // Bold yellow
        std::format_to(inserter, "Clone Group #{}, {} blocks, avg similarity {:.0f}%", gi + 1,
                       group.blockIndices.size(), group.avgSimilarity * 100.0);
        if (_config.useColor)
            out += ansiReset;
        out += '\n';

        // Block details
        for (auto const bi : std::views::iota(size_t{0}, group.blockIndices.size()))
        {
            auto const blockIdx = group.blockIndices[bi];
            auto const& block = blocks[blockIdx];
            auto const& range = block.sourceRange;

            if (_config.useColor)
                out += "\033[1;36m"; // Bold cyan
            std::format_to(inserter, "  {} [{}:{}-{}:{}] {}", range.start.filePath.string(), range.start.line,
                           range.start.column, range.end.line, range.end.column, block.name);
            if (_config.useColor)
                out += ansiReset;
            out += '\n';

            if (_config.showSourceCode && blockIdx < blockToFileIndex.size())
            {
                auto const fileIdx = blockToFileIndex[blockIdx];
                if (fileIdx < allTokens.size())
                {
                    std::unordered_set<size_t> highlightTokens;
                    if (_config.highlightDifferences && _config.useColor && group.blockIndices.size() >= 2)
                    {
                        if (bi == 0)
                        {
                            // Reference block: compare against second block
                            auto const secondIdx = group.blockIndices[1];
                            auto const secondFileIdx =
                                secondIdx < blockToFileIndex.size() ? blockToFileIndex[secondIdx] : size_t{0};
                            if (secondFileIdx < allTokens.size())
                                highlightTokens =
                                    computeHighlightSetForReference(block, blocks[secondIdx], allTokens[fileIdx]);
                        }
                        else
                        {
                            // Compare against the reference (first) block
                            auto const refIdx = group.blockIndices[0];
                            auto const refFileIdx =
                                refIdx < blockToFileIndex.size() ? blockToFileIndex[refIdx] : size_t{0};
                            if (refFileIdx < allTokens.size())
                                highlightTokens = computeHighlightSet(block, blocks[refIdx], allTokens[fileIdx]);
                        }
                    }
                    printSourceSnippet(out, allTokens[fileIdx], block.tokenStart, block.tokenEnd, highlightTokens);
                    out += '\n';
                }
            }
        }

        out += '\n';
    }
}

void Reporter::reportIntraClones(std::string& out, std::vector<IntraCloneResult> const& results,
                                 std::vector<CodeBlock> const& blocks, std::vector<std::vector<Token>> const& allTokens,
                                 std::vector<size_t> const& blockToFileIndex) const
{
    auto inserter = std::back_inserter(out);

    for (auto const& result : results)
    {
        auto const& block = blocks[result.blockIndex];
        auto const& range = block.sourceRange;

        // Block header
        if (_config.useColor)
            out += "\033[1;35m"; // Bold magenta
        std::format_to(inserter, "Intra-function clones in {} [{}:{}-{}:{}] {} ({} pairs)",
                       range.start.filePath.string(), range.start.line, range.start.column, range.end.line,
                       range.end.column, block.name, result.pairs.size());
        if (_config.useColor)
            out += ansiReset;
        out += '\n';

        for (auto const pi : std::views::iota(size_t{0}, result.pairs.size()))
        {
            auto const& pair = result.pairs[pi];

            if (_config.useColor)
                out += "\033[1;33m"; // Bold yellow
            std::format_to(inserter, "  Pair #{} (similarity {:.0f}%):", pi + 1, pair.similarity * 100.0);
            if (_config.useColor)
                out += ansiReset;
            out += '\n';

            // Compute line ranges for each region from token positions
            auto const fileIdx =
                result.blockIndex < blockToFileIndex.size() ? blockToFileIndex[result.blockIndex] : size_t{0};
            auto const hasTokens = fileIdx < allTokens.size() && !allTokens[fileIdx].empty();

            // Compute highlight sets for both intra-clone regions
            std::unordered_set<size_t> highlightA;
            std::unordered_set<size_t> highlightB;
            if (_config.highlightDifferences && _config.useColor && hasTokens)
            {
                // Extract normalized sub-sequences for each region
                auto const& ids = block.textPreservingIds.empty() ? block.normalizedIds : block.textPreservingIds;
                auto const regionAEnd = pair.regionA.start + pair.regionA.length;
                auto const regionBEnd = pair.regionB.start + pair.regionB.length;
                if (regionAEnd <= ids.size() && regionBEnd <= ids.size())
                {
                    auto const subA =
                        std::span<NormalizedTokenId const>(ids).subspan(pair.regionA.start, pair.regionA.length);
                    auto const subB =
                        std::span<NormalizedTokenId const>(ids).subspan(pair.regionB.start, pair.regionB.length);
                    auto const alignment = CloneDetector::computeLcsAlignment(subA, subB);

                    auto const normToOrigA = buildNormToOrigMap(
                        allTokens[fileIdx], block.tokenStart + pair.regionA.start, block.tokenStart + regionAEnd);
                    auto const normToOrigB = buildNormToOrigMap(
                        allTokens[fileIdx], block.tokenStart + pair.regionB.start, block.tokenStart + regionBEnd);

                    for (auto const i :
                         std::views::iota(size_t{0}, std::min(alignment.matchedA.size(), normToOrigA.size())))
                    {
                        if (!alignment.matchedA[i])
                            highlightA.insert(normToOrigA[i]);
                    }
                    for (auto const i :
                         std::views::iota(size_t{0}, std::min(alignment.matchedB.size(), normToOrigB.size())))
                    {
                        if (!alignment.matchedB[i])
                            highlightB.insert(normToOrigB[i]);
                    }
                }
            }

            // Region A
            auto const tokenStartA = block.tokenStart + pair.regionA.start;
            auto const tokenEndA = block.tokenStart + pair.regionA.start + pair.regionA.length;
            if (hasTokens && tokenStartA < allTokens[fileIdx].size() && tokenEndA <= allTokens[fileIdx].size())
            {
                auto const lineStartA = allTokens[fileIdx][tokenStartA].location.line;
                auto const lineEndA = allTokens[fileIdx][tokenEndA - 1].location.line;
                if (_config.useColor)
                    out += "\033[1;36m"; // Bold cyan
                std::format_to(inserter, "    Region A: lines {}-{} ({} tokens)", lineStartA, lineEndA,
                               pair.regionA.length);
                if (_config.useColor)
                    out += ansiReset;
                out += '\n';

                if (_config.showSourceCode)
                {
                    printSourceSnippet(out, allTokens[fileIdx], tokenStartA, tokenEndA, highlightA);
                    out += '\n';
                }
            }
            else
            {
                std::format_to(inserter, "    Region A: token offset {}-{} ({} tokens)\n", pair.regionA.start,
                               pair.regionA.start + pair.regionA.length, pair.regionA.length);
            }

            // Region B
            auto const tokenStartB = block.tokenStart + pair.regionB.start;
            auto const tokenEndB = block.tokenStart + pair.regionB.start + pair.regionB.length;
            if (hasTokens && tokenStartB < allTokens[fileIdx].size() && tokenEndB <= allTokens[fileIdx].size())
            {
                auto const lineStartB = allTokens[fileIdx][tokenStartB].location.line;
                auto const lineEndB = allTokens[fileIdx][tokenEndB - 1].location.line;
                if (_config.useColor)
                    out += "\033[1;36m"; // Bold cyan
                std::format_to(inserter, "    Region B: lines {}-{} ({} tokens)", lineStartB, lineEndB,
                               pair.regionB.length);
                if (_config.useColor)
                    out += ansiReset;
                out += '\n';

                if (_config.showSourceCode)
                {
                    printSourceSnippet(out, allTokens[fileIdx], tokenStartB, tokenEndB, highlightB);
                    out += '\n';
                }
            }
            else
            {
                std::format_to(inserter, "    Region B: token offset {}-{} ({} tokens)\n", pair.regionB.start,
                               pair.regionB.start + pair.regionB.length, pair.regionB.length);
            }
        }

        out += '\n';
    }
}

namespace
{

/// @brief Formats a duration as a human-readable string with appropriate units.
///
/// Selects the most readable unit: microseconds for < 1ms, milliseconds for < 1s,
/// seconds with one decimal for >= 1s.
[[nodiscard]] auto formatDuration(PerformanceTiming::Duration d) -> std::string
{
    auto const us = std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    if (us < 1000)
        return std::format("{} us", us);
    if (us < 1'000'000)
        return std::format("{:.1f} ms", static_cast<double>(us) / 1'000.0);
    return std::format("{:.1f} s", static_cast<double>(us) / 1'000'000.0);
}

} // namespace

void Reporter::reportSummary(std::string& out, SummaryData const& summary) const
{
    auto inserter = std::back_inserter(out);

    if (_config.useColor)
        out += "\033[1m"; // Bold
    if (summary.totalIntraPairs > 0)
        std::format_to(inserter,
                       "Summary: {} files scanned, {} blocks extracted, {} clone groups found, {} intra-function "
                       "clone pairs found",
                       summary.totalFiles, summary.totalBlocks, summary.totalGroups, summary.totalIntraPairs);
    else
        std::format_to(inserter, "Summary: {} files scanned, {} blocks extracted, {} clone groups found",
                       summary.totalFiles, summary.totalBlocks, summary.totalGroups);
    if (_config.useColor)
        out += ansiReset;
    out += '\n';

    if (summary.totalDuplicatedLines > 0 || summary.totalFunctions > 0 || summary.totalIntraFunctions > 0)
    {
        if (_config.useColor)
            out += "\033[1m"; // Bold
        out += "Duplications:";
        if (_config.useColor)
            out += ansiReset;
        std::format_to(inserter, " {} duplicated lines, {} functions in clone groups", summary.totalDuplicatedLines,
                       summary.totalFunctions);
        if (summary.totalIntraFunctions > 0)
            std::format_to(inserter, ", {} functions with internal clones", summary.totalIntraFunctions);
        out += '\n';
    }

    if (summary.timing)
    {
        if (_config.useColor)
            out += "\033[1m"; // Bold
        out += "Timing:";
        if (_config.useColor)
            out += ansiReset;
        std::format_to(inserter, " scanning {}, tokenizing {}, normalizing {}, clone detection {}",
                       formatDuration(summary.timing->scanning), formatDuration(summary.timing->tokenizing),
                       formatDuration(summary.timing->normalizing), formatDuration(summary.timing->cloneDetection));
        if (summary.timing->intraDetection > PerformanceTiming::Duration::zero())
            std::format_to(inserter, ", intra-function detection {}", formatDuration(summary.timing->intraDetection));
        std::format_to(inserter, ", total {}", formatDuration(summary.timing->total()));
        out += '\n';
    }
}

void Reporter::printSourceSnippet(std::string& out, std::vector<Token> const& tokens, size_t tokenStart,
                                  size_t tokenEnd, std::unordered_set<size_t> const& highlightTokens) const
{
    // Reconstruct source lines from token positions, preserving original indentation and spacing

    if (tokenStart >= tokens.size() || tokenEnd > tokens.size())
        return;

    auto inserter = std::back_inserter(out);
    auto const startLine = tokens[tokenStart].location.line;
    auto const endLine = tokens[std::min(tokenEnd, tokens.size()) - 1].location.line;

    // Determine line number width for formatting
    auto const lineNumWidth = std::format("{}", endLine).size();

    uint32_t currentLine = startLine;
    uint32_t currentColumn = 1;
    bool lineStarted = false;

    for (size_t i = tokenStart; i < tokenEnd && i < tokens.size(); ++i)
    {
        auto const& token = tokens[i];

        // Handle line breaks
        while (currentLine < token.location.line)
        {
            if (lineStarted)
                out += '\n';
            ++currentLine;
            lineStarted = false;
        }

        if (!lineStarted)
        {
            // Print line number
            if (_config.useColor)
                out += "\033[38;5;240m"; // Gray for line numbers
            std::format_to(inserter, "    {:>{}} | ", currentLine, lineNumWidth);
            if (_config.useColor)
                out += ansiReset;
            lineStarted = true;
            currentColumn = 1;
        }

        // Pad with spaces to match original column position
        if (token.location.column > currentColumn)
            out.append(token.location.column - currentColumn, ' ');

        // Print token with color (and optional diff background highlight)
        if (_config.useColor)
        {
            if (highlightTokens.contains(i))
                out += diffHighlightBg(_config.theme);
            out += colorForTokenType(token.type, _config.theme);
        }
        out += token.text;
        if (_config.useColor)
            out += ansiReset;

        currentColumn = token.location.column + static_cast<uint32_t>(token.text.size());
    }

    if (lineStarted)
        out += '\n';
}

} // namespace codedup
