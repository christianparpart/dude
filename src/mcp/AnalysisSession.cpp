// SPDX-License-Identifier: Apache-2.0
#include <mcp/AnalysisSession.hpp>

#include <codedup/FileScanner.hpp>
#include <codedup/Language.hpp>
#include <codedup/LanguageRegistry.hpp>
#include <codedup/ScopeFilter.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <ranges>
#include <sstream>

namespace mcp
{

auto AnalysisSession::Analyze(AnalysisConfig const& config) -> std::expected<void, AnalysisError>
{
    using Clock = std::chrono::steady_clock;

    _config = config;
    _timing = {};
    _hasResults = false;

    // Step 1: Scan files
    auto const scanStart = Clock::now();
    auto const& extensions = config.extensions.empty() ? codedup::FileScanner::DefaultExtensions() : config.extensions;
    auto const filesResult = codedup::FileScanner::Scan(config.directory, extensions);
    _timing.scanning = Clock::now() - scanStart;

    if (!filesResult)
        return std::unexpected(AnalysisError{.message = filesResult.error().message});

    _files = *filesResult;

    // Step 2: Tokenize all files
    auto const tokenizeStart = Clock::now();
    _allTokens.clear();
    _fileLanguages.clear();
    _allTokens.reserve(_files.size());
    _fileLanguages.reserve(_files.size());

    auto const& registry = codedup::LanguageRegistry::Instance();
    for (auto const& file : _files)
    {
        auto const* language = registry.FindByPath(file);
        if (!language)
        {
            _allTokens.emplace_back();
            _fileLanguages.push_back(nullptr);
            continue;
        }

        auto const fileIndex = static_cast<uint32_t>(_allTokens.size());
        auto tokensResult = language->TokenizeFile(file, fileIndex, config.encoding);
        if (!tokensResult)
        {
            _allTokens.emplace_back();
            _fileLanguages.push_back(language);
            continue;
        }
        _allTokens.push_back(std::move(*tokensResult));
        _fileLanguages.push_back(language);
    }
    _timing.tokenizing = Clock::now() - tokenizeStart;

    // Steps 3-4: Extract blocks and detect clones
    RunBlockExtractionAndDetection();

    _hasResults = true;
    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto AnalysisSession::Reconfigure(double threshold, size_t minTokens, double textSensitivity,
                                  codedup::AnalysisScope scope) -> std::expected<void, AnalysisError>
{
    if (!_hasResults)
        return std::unexpected(AnalysisError{.message = "No analysis results available. Run analyze_directory first."});

    _config.threshold = threshold;
    _config.minTokens = minTokens;
    _config.textSensitivity = textSensitivity;
    _config.scope = scope;

    RunBlockExtractionAndDetection();

    return {};
}

void AnalysisSession::RunBlockExtractionAndDetection()
{
    using Clock = std::chrono::steady_clock;

    // Step 3: Normalize and extract blocks
    auto const normalizeStart = Clock::now();
    codedup::TokenNormalizer normalizer;
    codedup::CodeBlockExtractorConfig const blockConfig{.minTokens = _config.minTokens};
    auto const useTextSensitivity = _config.textSensitivity > 0.0;

    _allBlocks.clear();
    _blockToFileIndex.clear();

    for (auto const fi : std::views::iota(size_t{0}, _allTokens.size()))
    {
        if (_allTokens[fi].empty())
            continue;

        auto const* language = _fileLanguages[fi];
        if (!language)
            continue;

        auto normalized = normalizer.Normalize(_allTokens[fi], language);
        auto textPreserving = useTextSensitivity ? normalizer.NormalizeTextPreserving(_allTokens[fi], language)
                                                 : std::vector<codedup::NormalizedToken>{};
        auto blocks = language->ExtractBlocks(_allTokens[fi], normalized, textPreserving, blockConfig);

        for (auto& block : blocks)
        {
            _blockToFileIndex.push_back(fi);
            _allBlocks.push_back(std::move(block));
        }
    }
    _timing.normalizing = Clock::now() - normalizeStart;

    // Step 4: Detect inter-function clones
    _groups.clear();
    if (codedup::HasInterFunctionScope(_config.scope))
    {
        auto const detectStart = Clock::now();
        codedup::CloneDetector detector({
            .similarityThreshold = _config.threshold,
            .minTokens = _config.minTokens,
            .textSensitivity = _config.textSensitivity,
        });

        _groups = detector.Detect(_allBlocks);
        _timing.cloneDetection = Clock::now() - detectStart;

        _groups = codedup::ScopeFilter::FilterCloneGroups(_groups, _blockToFileIndex, _config.scope);

        std::ranges::sort(_groups,
                          [this](auto const& a, auto const& b)
                          {
                              auto const tokensA = _allBlocks[a.blockIndices.front()].tokenEnd -
                                                   _allBlocks[a.blockIndices.front()].tokenStart;
                              auto const tokensB = _allBlocks[b.blockIndices.front()].tokenEnd -
                                                   _allBlocks[b.blockIndices.front()].tokenStart;
                              return tokensA > tokensB;
                          });
    }

    // Step 4b: Detect intra-function clones
    _intraResults.clear();
    if (codedup::HasScope(_config.scope, codedup::AnalysisScope::IntraFunction))
    {
        auto const intraStart = Clock::now();
        codedup::IntraFunctionDetector intraDetector({
            .minRegionTokens = _config.minTokens,
            .similarityThreshold = _config.threshold,
            .textSensitivity = _config.textSensitivity,
        });

        _intraResults = intraDetector.Detect(_allBlocks);
        _timing.intraDetection = Clock::now() - intraStart;

        std::ranges::sort(_intraResults,
                          [](auto const& a, auto const& b)
                          {
                              auto maxLen = [](auto const& result)
                              {
                                  return std::ranges::fold_left(
                                      result.pairs, size_t{0}, [](size_t m, auto const& p)
                                      { return std::max(m, std::max(p.regionA.length, p.regionB.length)); });
                              };
                              return maxLen(a) > maxLen(b);
                          });
    }
}

auto AnalysisSession::ReadBlockSource(size_t blockIndex) const -> std::expected<std::string, AnalysisError>
{
    if (blockIndex >= _allBlocks.size())
        return std::unexpected(AnalysisError{
            .message = std::format("Block index {} out of range (total: {})", blockIndex, _allBlocks.size())});

    auto const& block = _allBlocks[blockIndex];
    auto const fileIdx = block.sourceRange.start.fileIndex;
    if (fileIdx >= _files.size())
        return std::unexpected(
            AnalysisError{.message = std::format("File index {} out of range (total: {})", fileIdx, _files.size())});
    auto const& filePath = _files[fileIdx];
    auto const startLine = block.sourceRange.start.line;
    auto const endLine = block.sourceRange.end.line;

    std::ifstream file(filePath);
    if (!file.is_open())
        return std::unexpected(AnalysisError{.message = std::format("Failed to open file: {}", filePath.string())});

    std::string source;
    std::string line;
    uint32_t lineNum = 0;
    while (std::getline(file, line))
    {
        ++lineNum;
        if (lineNum >= startLine && lineNum <= endLine)
        {
            if (!source.empty())
                source += '\n';
            source += line;
        }
        if (lineNum > endLine)
            break;
    }

    return source;
}

} // namespace mcp
