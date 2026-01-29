// SPDX-License-Identifier: Apache-2.0
#include <codedup/AnalysisScope.hpp>
#include <codedup/JsonReporter.hpp>

#include <chrono>
#include <filesystem>
#include <ranges>
#include <span>
#include <vector>

namespace codedup
{

namespace
{

/// @brief Builds a mapping from normalized-ID position to original token index.
///
/// Iterates through original tokens in [tokenStart, tokenEnd) and collects indices
/// of tokens that survive normalization (i.e., not comments, preprocessor, or EOF).
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

/// @brief Formats a duration in microseconds as a human-readable string.
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

void JsonReporter::Report(std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                          [[maybe_unused]] std::vector<std::vector<Token>> const& allTokens,
                          [[maybe_unused]] std::vector<size_t> const& blockToFileIndex,
                          std::span<std::filesystem::path const> files)
{
    for (auto const gi : std::views::iota(size_t{0}, groups.size()))
    {
        auto const& group = groups[gi];

        nlohmann::json groupJson;
        groupJson["groupIndex"] = gi + 1;
        groupJson["blockCount"] = group.blockIndices.size();
        groupJson["avgSimilarity"] = group.avgSimilarity;

        auto blocksJson = nlohmann::json::array();
        for (auto const blockIdx : group.blockIndices)
        {
            auto const& block = blocks[blockIdx];
            auto const& range = block.sourceRange;

            nlohmann::json blockJson;
            blockJson["name"] = block.name;
            blockJson["filePath"] =
                range.start.fileIndex < files.size() ? files[range.start.fileIndex].string() : std::string("<unknown>");
            blockJson["startLine"] = range.start.line;
            blockJson["startColumn"] = range.start.column;
            blockJson["endLine"] = range.end.line;
            blockJson["endColumn"] = range.end.column;

            blocksJson.push_back(std::move(blockJson));
        }

        groupJson["blocks"] = std::move(blocksJson);
        _cloneGroups.push_back(std::move(groupJson));
    }
}

void JsonReporter::ReportIntraClones(std::vector<IntraCloneResult> const& results, std::vector<CodeBlock> const& blocks,
                                     std::vector<std::vector<Token>> const& allTokens,
                                     std::vector<size_t> const& blockToFileIndex,
                                     std::span<std::filesystem::path const> files)
{
    for (auto const& result : results)
    {
        auto const& block = blocks[result.blockIndex];
        auto const& range = block.sourceRange;

        auto const fileIdx =
            result.blockIndex < blockToFileIndex.size() ? blockToFileIndex[result.blockIndex] : size_t{0};
        auto const hasTokens = fileIdx < allTokens.size() && !allTokens[fileIdx].empty();
        auto const normToOrig = hasTokens ? BuildNormToOrigMap(allTokens[fileIdx], block.tokenStart, block.tokenEnd)
                                          : std::vector<size_t>{};

        nlohmann::json resultJson;
        resultJson["blockIndex"] = result.blockIndex;
        resultJson["name"] = block.name;
        resultJson["filePath"] =
            range.start.fileIndex < files.size() ? files[range.start.fileIndex].string() : std::string("<unknown>");
        resultJson["startLine"] = range.start.line;
        resultJson["endLine"] = range.end.line;

        auto pairsJson = nlohmann::json::array();
        for (auto const& pair : result.pairs)
        {
            nlohmann::json pairJson;
            pairJson["similarity"] = pair.similarity;

            auto makeRegionJson = [&](IntraCloneRegion const& region) -> nlohmann::json
            {
                nlohmann::json rj;
                rj["tokenOffset"] = region.start;
                rj["tokenCount"] = region.length;

                if (region.start + region.length <= normToOrig.size())
                {
                    auto const tokenStart = normToOrig[region.start];
                    auto const tokenEnd = normToOrig[region.start + region.length - 1] + 1;
                    auto const& tokens = allTokens[fileIdx];

                    if (tokenStart < tokens.size() && tokenEnd <= tokens.size())
                    {
                        rj["startLine"] = tokens[tokenStart].location.line;
                        rj["endLine"] = tokens[tokenEnd - 1].location.line;
                    }
                }
                return rj;
            };

            pairJson["regionA"] = makeRegionJson(pair.regionA);
            pairJson["regionB"] = makeRegionJson(pair.regionB);

            pairsJson.push_back(std::move(pairJson));
        }

        resultJson["pairs"] = std::move(pairsJson);
        _intraClones.push_back(std::move(resultJson));
    }
}

void JsonReporter::ReportSummary(SummaryData const& summary)
{
    _summary["totalFiles"] = summary.totalFiles;
    _summary["totalBlocks"] = summary.totalBlocks;
    _summary["totalGroups"] = summary.totalGroups;
    _summary["totalIntraPairs"] = summary.totalIntraPairs;
    _summary["totalDuplicatedLines"] = summary.totalDuplicatedLines;
    _summary["totalFunctions"] = summary.totalFunctions;
    _summary["totalIntraFunctions"] = summary.totalIntraFunctions;

    if (summary.timing)
    {
        nlohmann::json timingJson;
        timingJson["scanning"] = FormatDuration(summary.timing->scanning);
        timingJson["tokenizing"] = FormatDuration(summary.timing->tokenizing);
        timingJson["normalizing"] = FormatDuration(summary.timing->normalizing);
        timingJson["cloneDetection"] = FormatDuration(summary.timing->cloneDetection);
        if (summary.timing->intraDetection > PerformanceTiming::Duration::zero())
            timingJson["intraDetection"] = FormatDuration(summary.timing->intraDetection);
        timingJson["total"] = FormatDuration(summary.timing->Total());
        _summary["timing"] = std::move(timingJson);
    }

    if (summary.activeScope && *summary.activeScope != AnalysisScope::All)
        _summary["scope"] = FormatAnalysisScope(*summary.activeScope);
}

auto JsonReporter::Render() const -> std::string
{
    nlohmann::json root;
    root["cloneGroups"] = _cloneGroups;
    root["intraClones"] = _intraClones;
    root["summary"] = _summary;
    return root.dump(2) + '\n';
}

void JsonReporter::WriteTo(std::ostream& out) const
{
    out << Render();
}

} // namespace codedup
