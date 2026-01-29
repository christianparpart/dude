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
auto BuildNormToOrigMap(std::vector<Token> const& tokens, size_t tokenStart, size_t tokenEnd) -> std::vector<size_t>
{
    std::vector<size_t> map;
    for (auto const i : std::views::iota(tokenStart, std::min(tokenEnd, tokens.size())))
    {
        if (!IsComment(tokens[i].type) && tokens[i].type != TokenType::PreprocessorDirective &&
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
auto ComputeHighlightSet(CodeBlock const& block, CodeBlock const& referenceBlock, std::vector<Token> const& tokens)
    -> std::unordered_set<size_t>
{
    // Choose text-preserving IDs if available, else structural
    auto const& idsA =
        referenceBlock.textPreservingIds.empty() ? referenceBlock.normalizedIds : referenceBlock.textPreservingIds;
    auto const& idsB = block.textPreservingIds.empty() ? block.normalizedIds : block.textPreservingIds;

    auto const alignment = CloneDetector::ComputeLcsAlignment(idsA, idsB);
    auto const normToOrig = BuildNormToOrigMap(tokens, block.tokenStart, block.tokenEnd);

    std::unordered_set<size_t> highlighted;
    for (auto const i : std::views::iota(size_t{0}, std::min(alignment.matchedB.size(), normToOrig.size())))
    {
        if (!alignment.matchedB[i])
            highlighted.insert(normToOrig[i]);
    }
    return highlighted;
}

/// @brief Computes highlight set for the reference block (compared against the second block).
auto ComputeHighlightSetForReference(CodeBlock const& referenceBlock, CodeBlock const& secondBlock,
                                     std::vector<Token> const& tokens) -> std::unordered_set<size_t>
{
    auto const& idsA =
        referenceBlock.textPreservingIds.empty() ? referenceBlock.normalizedIds : referenceBlock.textPreservingIds;
    auto const& idsB =
        secondBlock.textPreservingIds.empty() ? secondBlock.normalizedIds : secondBlock.textPreservingIds;

    auto const alignment = CloneDetector::ComputeLcsAlignment(idsA, idsB);
    auto const normToOrig = BuildNormToOrigMap(tokens, referenceBlock.tokenStart, referenceBlock.tokenEnd);

    std::unordered_set<size_t> highlighted;
    for (auto const i : std::views::iota(size_t{0}, std::min(alignment.matchedA.size(), normToOrig.size())))
    {
        if (!alignment.matchedA[i])
            highlighted.insert(normToOrig[i]);
    }
    return highlighted;
}

/// @brief Formats a clone group header line with optional ANSI coloring.
///
/// Writes the group number, block count, and average similarity percentage.
/// @param out Output string to append to.
/// @param groupIndex Zero-based index of the group.
/// @param group The clone group to format.
/// @param config Reporter configuration for color settings.
void ReportGroupHeader(std::string& out, size_t groupIndex, CloneGroup const& group, ReporterConfig const& config)
{
    auto inserter = std::back_inserter(out);
    if (config.useColor)
        out += "\033[1;33m"; // Bold yellow
    std::format_to(inserter, "Clone Group #{}, {} blocks, avg similarity {:.0f}%", groupIndex + 1,
                   group.blockIndices.size(), group.avgSimilarity * 100.0);
    if (config.useColor)
        out += ansiReset;
    out += '\n';
}

/// @brief Formats a single block's location line with optional ANSI coloring.
///
/// Writes the file path, line/column range, and function name for a code block.
/// @param out Output string to append to.
/// @param block The code block whose location to format.
/// @param range The source range of the block.
/// @param config Reporter configuration for color settings.
void ReportBlockEntry(std::string& out, CodeBlock const& block, SourceRange const& range, ReporterConfig const& config)
{
    auto inserter = std::back_inserter(out);
    if (config.useColor)
        out += "\033[1;36m"; // Bold cyan
    std::format_to(inserter, "  {} [{}:{}-{}:{}] {}", range.start.filePath.string(), range.start.line,
                   range.start.column, range.end.line, range.end.column, block.name);
    if (config.useColor)
        out += ansiReset;
    out += '\n';
}

/// @brief Computes the set of token indices to highlight as differing for a block in a clone group.
///
/// For the first block (reference), compares against the second block; for all other blocks,
/// compares against the first (reference) block. Returns an empty set when highlighting is
/// disabled or the group has fewer than two blocks.
/// @param group The clone group containing the block.
/// @param blockIndexInGroup Zero-based index of the block within the group.
/// @param blocks All code blocks.
/// @param allTokens All tokens from all files.
/// @param blockToFileIndex Mapping from block index to file index in allTokens.
/// @param config Reporter configuration for color and highlighting settings.
/// @return Set of original token indices that should be background-highlighted.
auto ComputeBlockHighlights(CloneGroup const& group, size_t blockIndexInGroup, std::vector<CodeBlock> const& blocks,
                            std::vector<std::vector<Token>> const& allTokens,
                            std::vector<size_t> const& blockToFileIndex, ReporterConfig const& config)
    -> std::unordered_set<size_t>
{
    auto const blockIdx = group.blockIndices[blockIndexInGroup];
    auto const& block = blocks[blockIdx];

    if (!config.highlightDifferences || !config.useColor || group.blockIndices.size() < 2)
        return {};
    if (blockIdx >= blockToFileIndex.size())
        return {};

    auto const fileIdx = blockToFileIndex[blockIdx];
    if (fileIdx >= allTokens.size())
        return {};

    if (blockIndexInGroup == 0)
    {
        // Reference block: compare against second block
        auto const secondIdx = group.blockIndices[1];
        auto const secondFileIdx = secondIdx < blockToFileIndex.size() ? blockToFileIndex[secondIdx] : size_t{0};
        if (secondFileIdx < allTokens.size())
            return ComputeHighlightSetForReference(block, blocks[secondIdx], allTokens[fileIdx]);
    }
    else
    {
        // Compare against the reference (first) block
        auto const refIdx = group.blockIndices[0];
        auto const refFileIdx = refIdx < blockToFileIndex.size() ? blockToFileIndex[refIdx] : size_t{0};
        if (refFileIdx < allTokens.size())
            return ComputeHighlightSet(block, blocks[refIdx], allTokens[fileIdx]);
    }
    return {};
}

/// @brief Computes highlight sets for both regions of an intra-clone pair.
///
/// Extracts normalized sub-sequences for each region, performs LCS alignment, and maps
/// unmatched positions back to original token indices. Returns empty sets when the regions
/// exceed the available token IDs.
/// @param pair The intra-clone pair containing region offsets and lengths.
/// @param block The code block containing both regions.
/// @param allTokens All tokens from the block's file.
/// @param fileIdx Index into allTokens for the block's file.
/// @return Pair of highlight sets: first for region A, second for region B.
auto ComputeIntraHighlights(IntraClonePair const& pair, CodeBlock const& block,
                            std::vector<std::vector<Token>> const& allTokens, size_t fileIdx)
    -> std::pair<std::unordered_set<size_t>, std::unordered_set<size_t>>
{
    auto const& ids = block.textPreservingIds.empty() ? block.normalizedIds : block.textPreservingIds;
    auto const regionAEnd = pair.regionA.start + pair.regionA.length;
    auto const regionBEnd = pair.regionB.start + pair.regionB.length;

    if (regionAEnd > ids.size() || regionBEnd > ids.size())
        return {};

    auto const subA = std::span<NormalizedTokenId const>(ids).subspan(pair.regionA.start, pair.regionA.length);
    auto const subB = std::span<NormalizedTokenId const>(ids).subspan(pair.regionB.start, pair.regionB.length);
    auto const alignment = CloneDetector::ComputeLcsAlignment(subA, subB);

    auto const normToOrigA =
        BuildNormToOrigMap(allTokens[fileIdx], block.tokenStart + pair.regionA.start, block.tokenStart + regionAEnd);
    auto const normToOrigB =
        BuildNormToOrigMap(allTokens[fileIdx], block.tokenStart + pair.regionB.start, block.tokenStart + regionBEnd);

    std::unordered_set<size_t> highlightA;
    for (auto const i : std::views::iota(size_t{0}, std::min(alignment.matchedA.size(), normToOrigA.size())))
    {
        if (!alignment.matchedA[i])
            highlightA.insert(normToOrigA[i]);
    }

    std::unordered_set<size_t> highlightB;
    for (auto const i : std::views::iota(size_t{0}, std::min(alignment.matchedB.size(), normToOrigB.size())))
    {
        if (!alignment.matchedB[i])
            highlightB.insert(normToOrigB[i]);
    }

    return {std::move(highlightA), std::move(highlightB)};
}

/// @brief Formats one region (A or B) of an intra-clone pair, including header and optional source snippet.
///
/// When token information is available, prints the line range and token count with optional ANSI
/// coloring, followed by a syntax-highlighted source snippet if enabled. When token information is
/// unavailable, falls back to printing raw token offsets.
/// @param out Output string to append to.
/// @param label Region label (e.g. "A" or "B").
/// @param region The intra-clone region describing the token offset and length.
/// @param block The code block containing the region.
/// @param tokens All tokens from the block's file.
/// @param highlights Set of original token indices to background-highlight.
/// @param config Reporter configuration for color and source display settings.
/// @param printSnippet Callback to print a source snippet (the Reporter's PrintSourceSnippet member).
void ReportIntraRegion(std::string& out, std::string_view label, IntraCloneRegion const& region, CodeBlock const& block,
                       std::vector<Token> const& tokens, std::unordered_set<size_t> const& highlights,
                       ReporterConfig const& config, auto const& printSnippet)
{
    auto inserter = std::back_inserter(out);
    auto const tokenStart = block.tokenStart + region.start;
    auto const tokenEnd = block.tokenStart + region.start + region.length;

    if (tokenStart < tokens.size() && tokenEnd <= tokens.size())
    {
        auto const lineStart = tokens[tokenStart].location.line;
        auto const lineEnd = tokens[tokenEnd - 1].location.line;
        if (config.useColor)
            out += "\033[1;36m"; // Bold cyan
        std::format_to(inserter, "    Region {}: lines {}-{} ({} tokens)", label, lineStart, lineEnd, region.length);
        if (config.useColor)
            out += ansiReset;
        out += '\n';

        if (config.showSourceCode)
        {
            printSnippet(out, tokens, tokenStart, tokenEnd, highlights);
            out += '\n';
        }
    }
    else
    {
        std::format_to(inserter, "    Region {}: token offset {}-{} ({} tokens)\n", label, region.start,
                       region.start + region.length, region.length);
    }
}

} // namespace

void Reporter::Report(std::string& out, std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                      std::vector<std::vector<Token>> const& allTokens,
                      std::vector<size_t> const& blockToFileIndex) const
{
    for (auto const gi : std::views::iota(size_t{0}, groups.size()))
    {
        auto const& group = groups[gi];

        ReportGroupHeader(out, gi, group, _config);

        for (auto const bi : std::views::iota(size_t{0}, group.blockIndices.size()))
        {
            auto const blockIdx = group.blockIndices[bi];
            auto const& block = blocks[blockIdx];

            ReportBlockEntry(out, block, block.sourceRange, _config);

            if (_config.showSourceCode && blockIdx < blockToFileIndex.size())
            {
                auto const fileIdx = blockToFileIndex[blockIdx];
                if (fileIdx < allTokens.size())
                {
                    auto const highlightTokens =
                        ComputeBlockHighlights(group, bi, blocks, allTokens, blockToFileIndex, _config);
                    PrintSourceSnippet(out, allTokens[fileIdx], block.tokenStart, block.tokenEnd, highlightTokens);
                    out += '\n';
                }
            }
        }

        out += '\n';
    }
}

void Reporter::ReportIntraClones(std::string& out, std::vector<IntraCloneResult> const& results,
                                 std::vector<CodeBlock> const& blocks, std::vector<std::vector<Token>> const& allTokens,
                                 std::vector<size_t> const& blockToFileIndex) const
{
    auto inserter = std::back_inserter(out);

    auto const printSnippet = [this](std::string& snippetOut, std::vector<Token> const& tokens, size_t tokenStart,
                                     size_t tokenEnd, std::unordered_set<size_t> const& highlights)
    { PrintSourceSnippet(snippetOut, tokens, tokenStart, tokenEnd, highlights); };

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

        auto const fileIdx =
            result.blockIndex < blockToFileIndex.size() ? blockToFileIndex[result.blockIndex] : size_t{0};
        auto const hasTokens = fileIdx < allTokens.size() && !allTokens[fileIdx].empty();

        for (auto const pi : std::views::iota(size_t{0}, result.pairs.size()))
        {
            auto const& pair = result.pairs[pi];

            if (_config.useColor)
                out += "\033[1;33m"; // Bold yellow
            std::format_to(inserter, "  Pair #{} (similarity {:.0f}%):", pi + 1, pair.similarity * 100.0);
            if (_config.useColor)
                out += ansiReset;
            out += '\n';

            // Compute highlight sets for both intra-clone regions
            auto [highlightA, highlightB] = (_config.highlightDifferences && _config.useColor && hasTokens)
                                                ? ComputeIntraHighlights(pair, block, allTokens, fileIdx)
                                                : std::pair<std::unordered_set<size_t>, std::unordered_set<size_t>>{};

            if (hasTokens)
            {
                ReportIntraRegion(out, "A", pair.regionA, block, allTokens[fileIdx], highlightA, _config, printSnippet);
                ReportIntraRegion(out, "B", pair.regionB, block, allTokens[fileIdx], highlightB, _config, printSnippet);
            }
            else
            {
                std::format_to(inserter, "    Region A: token offset {}-{} ({} tokens)\n", pair.regionA.start,
                               pair.regionA.start + pair.regionA.length, pair.regionA.length);
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
[[nodiscard]] auto FormatDuration(PerformanceTiming::Duration d) -> std::string
{
    auto const us = std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    if (us < 1000)
        return std::format("{} us", us);
    if (us < 1'000'000)
        return std::format("{:.1f} ms", static_cast<double>(us) / 1'000.0);
    return std::format("{:.1f} s", static_cast<double>(us) / 1'000'000.0);
}

} // namespace

void Reporter::ReportSummary(std::string& out, SummaryData const& summary) const
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
                       FormatDuration(summary.timing->scanning), FormatDuration(summary.timing->tokenizing),
                       FormatDuration(summary.timing->normalizing), FormatDuration(summary.timing->cloneDetection));
        if (summary.timing->intraDetection > PerformanceTiming::Duration::zero())
            std::format_to(inserter, ", intra-function detection {}", FormatDuration(summary.timing->intraDetection));
        std::format_to(inserter, ", total {}", FormatDuration(summary.timing->Total()));
        out += '\n';
    }
}

void Reporter::PrintSourceSnippet(std::string& out, std::vector<Token> const& tokens, size_t tokenStart,
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
                out += DiffHighlightBg(_config.theme);
            out += ColorForTokenType(token.type, _config.theme);
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
