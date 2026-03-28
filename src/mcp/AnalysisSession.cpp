// SPDX-License-Identifier: Apache-2.0
#include <mcp/AnalysisSession.hpp>

#include <dude/FileScanner.hpp>
#include <dude/GlobMatch.hpp>
#include <dude/Language.hpp>
#include <dude/LanguageRegistry.hpp>
#include <dude/ScopeFilter.hpp>
#include <dude/TokenNormalizer.hpp>

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
    auto const extensions =
        config.globPatterns.empty() ? dude::FileScanner::DefaultExtensions() : std::vector<std::string>{};

    auto const globFilter = config.globPatterns.empty()
                                ? std::optional<dude::FileFilter>(std::nullopt)
                                : std::optional<dude::FileFilter>(
                                      [&patterns = config.globPatterns](std::filesystem::path const& path) -> bool
                                      {
                                          auto const filename = path.filename().string();
                                          return std::ranges::any_of(patterns, [&filename](std::string const& pattern)
                                                                     { return dude::GlobMatch(pattern, filename); });
                                      });

    auto const canonicalDir = std::filesystem::weakly_canonical(config.directory);
    auto const excludeFilter =
        config.excludePatterns.empty()
            ? std::optional<dude::FileFilter>(std::nullopt)
            : std::optional<dude::FileFilter>(
                  [&patterns = config.excludePatterns, canonicalDir](std::filesystem::path const& path) -> bool
                  {
                      auto const relative = std::filesystem::relative(path, canonicalDir).string();
                      return !std::ranges::any_of(patterns, [&relative](std::string const& pattern)
                                                  { return dude::GlobMatch(pattern, relative); });
                  });

    auto const composedFilter = (globFilter || excludeFilter)
                                    ? std::optional<dude::FileFilter>(
                                          [globFilter, excludeFilter](std::filesystem::path const& path) -> bool
                                          {
                                              if (globFilter && !(*globFilter)(path))
                                                  return false;
                                              if (excludeFilter && !(*excludeFilter)(path))
                                                  return false;
                                              return true;
                                          })
                                    : std::optional<dude::FileFilter>(std::nullopt);

    auto const filesResult = dude::FileScanner::Scan(config.directory, extensions, composedFilter);
    _timing.scanning = Clock::now() - scanStart;

    if (!filesResult)
        return std::unexpected(AnalysisError{.message = filesResult.error().message});

    _files = *filesResult;

    // Step 2: Resolve languages for each file
    _fileLanguages.clear();
    _fileLanguages.reserve(_files.size());

    auto const& registry = dude::LanguageRegistry::Instance();
    for (auto const& file : _files)
        _fileLanguages.push_back(registry.FindByPath(file));

    // Step 3: Streaming tokenize + normalize + extract blocks (per file)
    RunBlockExtraction();

    // Step 4: Detect clones
    RunDetection();

    _hasResults = true;
    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto AnalysisSession::Reconfigure(double threshold, size_t minTokens, double textSensitivity, dude::AnalysisScope scope)
    -> std::expected<void, AnalysisError>
{
    if (!_hasResults)
        return std::unexpected(AnalysisError{.message = "No analysis results available. Run analyze_directory first."});

    _config.threshold = threshold;
    _config.minTokens = minTokens;
    _config.textSensitivity = textSensitivity;
    _config.scope = scope;

    // Re-tokenize from disk and re-extract blocks with new parameters
    RunBlockExtraction();
    RunDetection();

    return {};
}

void AnalysisSession::RunBlockExtraction()
{
    using Clock = std::chrono::steady_clock;

    auto const tokenizeStart = Clock::now();
    dude::TokenNormalizer normalizer;
    dude::CodeBlockExtractorConfig const blockConfig{.minTokens = _config.minTokens};
    auto const useTextSensitivity = _config.textSensitivity > 0.0;

    _allBlocks.clear();
    _blockToFileIndex.clear();

    for (auto const fi : std::views::iota(size_t{0}, _files.size()))
    {
        auto const* language = _fileLanguages[fi];
        if (!language)
            continue;

        // Tokenize this single file
        auto const fileIndex = static_cast<uint32_t>(fi);
        auto tokensResult = language->TokenizeFile(_files[fi], fileIndex, _config.encoding);
        if (!tokensResult || tokensResult->empty())
            continue;

        auto& tokens = *tokensResult;

        // Normalize and extract blocks from this file's tokens
        auto normalized = normalizer.Normalize(tokens, language);
        auto textPreserving = useTextSensitivity ? normalizer.NormalizeTextPreserving(tokens, language)
                                                 : std::vector<dude::NormalizedToken>{};
        auto blocks = language->ExtractBlocks(tokens, normalized, textPreserving, blockConfig);

        for (auto& block : blocks)
        {
            _blockToFileIndex.push_back(fi);
            _allBlocks.push_back(std::move(block));
        }

        // tokens is released here when tokensResult goes out of scope at end of iteration
    }
    _timing.tokenizing = Clock::now() - tokenizeStart;
}

void AnalysisSession::RunDetection()
{
    using Clock = std::chrono::steady_clock;

    // Detect inter-function clones
    _groups.clear();
    if (dude::HasInterFunctionScope(_config.scope))
    {
        auto const detectStart = Clock::now();
        dude::CloneDetector detector({
            .similarityThreshold = _config.threshold,
            .minTokens = _config.minTokens,
            .textSensitivity = _config.textSensitivity,
        });

        _groups = detector.Detect(_allBlocks);
        _timing.cloneDetection = Clock::now() - detectStart;

        _groups = dude::ScopeFilter::FilterCloneGroups(_groups, _blockToFileIndex, _config.scope);

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

    // Detect intra-function clones
    _intraResults.clear();
    if (dude::HasScope(_config.scope, dude::AnalysisScope::IntraFunction))
    {
        auto const intraStart = Clock::now();
        dude::IntraFunctionDetector intraDetector({
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
