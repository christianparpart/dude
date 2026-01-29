// SPDX-License-Identifier: Apache-2.0

#include "GitDiffParser.hpp"

#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/DiffFilter.hpp>
#include <codedup/Encoding.hpp>
#include <codedup/FileScanner.hpp>
#include <codedup/IntraFunctionDetector.hpp>
#include <codedup/Reporter.hpp>
#include <codedup/Token.hpp>
#include <codedup/TokenNormalizer.hpp>
#include <codedup/Tokenizer.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{

constexpr auto versionString = "0.1.0";

/// @brief Parsed command-line arguments.
struct CliOptions
{
    std::filesystem::path directory;                                ///< Directory to scan.
    double threshold = 0.80;                                        ///< Similarity threshold.
    size_t minTokens = 30;                                          ///< Minimum block size in tokens.
    double textSensitivity = 0.3;                                   ///< Text sensitivity blend factor.
    bool useColor = true;                                           ///< Whether to use ANSI colors.
    bool showSource = true;                                         ///< Whether to show source snippets.
    codedup::ColorTheme theme = codedup::ColorTheme::Auto;          ///< Color theme.
    std::vector<std::string> extensions;                            ///< File extensions to scan.
    codedup::InputEncoding encoding = codedup::InputEncoding::Auto; ///< Input file encoding.
    bool verbose = false;                                           ///< Show progress.
    bool detectIntraClones = true;                                  ///< Detect intra-function clones.
    bool showHelp = false;                                          ///< Show help text.
    bool showVersion = false;                                       ///< Show version.
    std::string diffBase;                                           ///< Git ref to diff against (enables diff mode).
};

void printUsage(FILE* out)
{
    std::println(out,
                 "Usage: codedupdetector [OPTIONS] <directory>\n"
                 "\n"
                 "Options:\n"
                 "  -t, --threshold <N>         Similarity threshold 0.0-1.0 (default: 0.80)\n"
                 "  -m, --min-tokens <N>        Minimum block size in tokens (default: 30)\n"
                 "  --text-sensitivity <N>      Text sensitivity blend factor 0.0-1.0 (default: 0.3)\n"
                 "  --diff-base <ref>           Git ref to diff against (enables diff mode for CI)\n"
                 "  --no-color                  Disable ANSI color output\n"
                 "  --no-source                 Don't print source code snippets\n"
                 "  --theme <dark|light|auto>   Color theme (default: auto)\n"
                 "  -e, --extensions <list>     Comma-separated extensions (default: .cpp,.cxx,.cc,.c,.h,.hpp,.hxx)\n"
                 "  --encoding <enc>            Input encoding: auto, utf8, windows-1252 (default: auto)\n"
                 "  --intra                     Enable intra-function clone detection (default)\n"
                 "  --no-intra                  Disable intra-function clone detection\n"
                 "  -v, --verbose               Show progress during scanning\n"
                 "  -h, --help                  Show help\n"
                 "  --version                   Show version");
}

auto parseArgs(int argc, char* argv[]) -> std::expected<CliOptions, std::string>
{
    CliOptions opts;

    for (int i = 1; i < argc; ++i)
    {
        auto const arg = std::string_view(argv[i]);

        if (arg == "-h" || arg == "--help")
        {
            opts.showHelp = true;
            return opts;
        }
        if (arg == "--version")
        {
            opts.showVersion = true;
            return opts;
        }
        if (arg == "-t" || arg == "--threshold")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --threshold");
            opts.threshold = std::stod(argv[i]);
            if (opts.threshold < 0.0 || opts.threshold > 1.0)
                return std::unexpected("Threshold must be between 0.0 and 1.0");
            continue;
        }
        if (arg == "-m" || arg == "--min-tokens")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --min-tokens");
            opts.minTokens = static_cast<size_t>(std::stoul(argv[i]));
            continue;
        }
        if (arg == "--text-sensitivity")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --text-sensitivity");
            opts.textSensitivity = std::stod(argv[i]);
            if (opts.textSensitivity < 0.0 || opts.textSensitivity > 1.0)
                return std::unexpected("Text sensitivity must be between 0.0 and 1.0");
            continue;
        }
        if (arg == "--diff-base")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --diff-base");
            opts.diffBase = argv[i];
            continue;
        }
        if (arg == "--no-color")
        {
            opts.useColor = false;
            continue;
        }
        if (arg == "--no-source")
        {
            opts.showSource = false;
            continue;
        }
        if (arg == "--theme")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --theme");
            auto const val = std::string_view(argv[i]);
            if (val == "dark")
                opts.theme = codedup::ColorTheme::Dark;
            else if (val == "light")
                opts.theme = codedup::ColorTheme::Light;
            else if (val == "auto")
                opts.theme = codedup::ColorTheme::Auto;
            else
                return std::unexpected(std::format("Unknown theme: {}", val));
            continue;
        }
        if (arg == "-e" || arg == "--extensions")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --extensions");
            // Parse comma-separated list
            auto const list = std::string(argv[i]);
            size_t start = 0;
            while (start < list.size())
            {
                auto const end = list.find(',', start);
                auto ext = list.substr(start, end == std::string::npos ? end : end - start);
                if (!ext.empty() && ext[0] != '.')
                    ext = "." + ext;
                opts.extensions.push_back(std::move(ext));
                start = (end == std::string::npos) ? list.size() : end + 1;
            }
            continue;
        }
        if (arg == "--encoding")
        {
            if (++i >= argc)
                return std::unexpected("Missing value for --encoding");
            auto const encResult = codedup::parseEncodingName(argv[i]);
            if (!encResult)
                return std::unexpected(encResult.error().message);
            opts.encoding = *encResult;
            continue;
        }
        if (arg == "--intra")
        {
            opts.detectIntraClones = true;
            continue;
        }
        if (arg == "--no-intra")
        {
            opts.detectIntraClones = false;
            continue;
        }
        if (arg == "-v" || arg == "--verbose")
        {
            opts.verbose = true;
            continue;
        }
        if (arg.starts_with("-"))
        {
            return std::unexpected(std::format("Unknown option: {}", arg));
        }

        // Positional argument: directory
        if (!opts.directory.empty())
            return std::unexpected("Multiple directories specified");
        opts.directory = arg;
    }

    if (!opts.showHelp && !opts.showVersion && opts.directory.empty())
        return std::unexpected("No directory specified");

    return opts;
}

} // namespace

int main(int argc, char* argv[])
{
    auto const optsResult = parseArgs(argc, argv);
    if (!optsResult)
    {
        std::println(stderr, "Error: {}\n", optsResult.error());
        printUsage(stderr);
        return 2;
    }

    auto const& opts = *optsResult;

    if (opts.showHelp)
    {
        printUsage(stdout);
        return 0;
    }

    if (opts.showVersion)
    {
        std::println("codedupdetector {}", versionString);
        return 0;
    }

    auto const diffMode = !opts.diffBase.empty();

    // Step 0: Parse git diff if in diff mode.
    codedup::DiffResult diffResult;
    if (diffMode)
    {
        if (opts.verbose)
            std::println(stderr, "Running git diff against {}...", opts.diffBase);

        auto const projectRoot = std::filesystem::weakly_canonical(opts.directory);
        auto const diffOutput = cli::GitDiffParser::runGitDiff(projectRoot, opts.diffBase);
        if (!diffOutput)
        {
            std::println(stderr, "Error: {}", diffOutput.error().message);
            return 2;
        }

        auto const& extensions = opts.extensions.empty() ? codedup::FileScanner::defaultExtensions() : opts.extensions;
        diffResult = cli::GitDiffParser::parseDiffOutput(*diffOutput, extensions);

        if (diffResult.empty())
        {
            std::println("No C++ files changed relative to {}.", opts.diffBase);
            return 0;
        }

        std::println(stderr, "Checking for duplication in changes relative to `{}`...", opts.diffBase);

        if (opts.verbose)
        {
            for (auto const& fc : diffResult)
                std::println(stderr, "  Changed: {} ({} hunks)", fc.filePath.string(), fc.changedRanges.size());
        }
    }

    codedup::PerformanceTiming timing;
    using Clock = std::chrono::steady_clock;

    // Step 1: Scan files
    if (opts.verbose)
        std::println(stderr, "Scanning directory: {}", opts.directory.string());

    auto const scanStart = Clock::now();
    auto const& extensions = opts.extensions.empty() ? codedup::FileScanner::defaultExtensions() : opts.extensions;

    auto const filesResult = codedup::FileScanner::scan(opts.directory, extensions);
    timing.scanning = Clock::now() - scanStart;
    if (!filesResult)
    {
        std::println(stderr, "Error: {}", filesResult.error().message);
        return 2;
    }

    auto const& files = *filesResult;
    if (opts.verbose)
        std::println(stderr, "Found {} source files", files.size());

    // Step 2: Tokenize all files
    auto const tokenizeStart = Clock::now();
    std::vector<std::vector<codedup::Token>> allTokens;
    allTokens.reserve(files.size());

    for (auto const& file : files)
    {
        if (opts.verbose)
            std::println(stderr, "Tokenizing: {}", file.string());

        auto tokensResult = codedup::Tokenizer::tokenizeFile(file, opts.encoding);
        if (!tokensResult)
        {
            std::println(stderr, "Warning: Failed to tokenize {}: {}", file.string(), tokensResult.error().message);
            allTokens.emplace_back(); // Empty tokens for failed files
            continue;
        }
        allTokens.push_back(std::move(*tokensResult));
    }
    timing.tokenizing = Clock::now() - tokenizeStart;

    // Step 3: Normalize and extract blocks
    auto const normalizeStart = Clock::now();
    codedup::TokenNormalizer normalizer;
    codedup::CodeBlockExtractor extractor({.minTokens = opts.minTokens});

    std::vector<codedup::CodeBlock> allBlocks;
    std::vector<size_t> blockToFileIndex;

    auto const useTextSensitivity = opts.textSensitivity > 0.0;

    for (auto const fi : std::views::iota(size_t{0}, allTokens.size()))
    {
        if (allTokens[fi].empty())
            continue;

        auto normalized = normalizer.normalize(allTokens[fi]);
        auto textPreserving = useTextSensitivity ? normalizer.normalizeTextPreserving(allTokens[fi])
                                                 : std::vector<codedup::NormalizedToken>{};
        auto blocks = extractor.extract(allTokens[fi], normalized, textPreserving);

        if (opts.verbose && !blocks.empty())
            std::println(stderr, "  {} blocks from {}", blocks.size(), files[fi].string());

        for (auto& block : blocks)
        {
            blockToFileIndex.push_back(fi);
            allBlocks.push_back(std::move(block));
        }
    }
    timing.normalizing = Clock::now() - normalizeStart;

    if (opts.verbose)
        std::println(stderr, "Extracted {} code blocks total", allBlocks.size());

    // Step 4: Detect clones
    auto const detectStart = Clock::now();
    codedup::CloneDetector detector({
        .similarityThreshold = opts.threshold,
        .minTokens = opts.minTokens,
        .textSensitivity = opts.textSensitivity,
    });

    auto groups = detector.detect(allBlocks);
    timing.cloneDetection = Clock::now() - detectStart;

    std::ranges::sort(groups,
                      [&allBlocks](auto const& a, auto const& b)
                      {
                          auto const tokensA =
                              allBlocks[a.blockIndices.front()].tokenEnd - allBlocks[a.blockIndices.front()].tokenStart;
                          auto const tokensB =
                              allBlocks[b.blockIndices.front()].tokenEnd - allBlocks[b.blockIndices.front()].tokenStart;
                          return tokensA > tokensB;
                      });

    // Step 4b: Detect intra-function clones
    std::vector<codedup::IntraCloneResult> intraResults;
    if (opts.detectIntraClones)
    {
        if (opts.verbose)
            std::println(stderr, "Detecting intra-function clones...");

        auto const intraStart = Clock::now();
        codedup::IntraFunctionDetector intraDetector({
            .minRegionTokens = opts.minTokens,
            .similarityThreshold = opts.threshold,
            .textSensitivity = opts.textSensitivity,
        });

        intraResults = intraDetector.detect(allBlocks);
        timing.intraDetection = Clock::now() - intraStart;

        std::ranges::sort(intraResults,
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

        if (opts.verbose)
        {
            size_t totalPairs = 0;
            for (auto const& r : intraResults)
                totalPairs += r.pairs.size();
            std::println(stderr, "Found {} intra-function clone pairs in {} blocks", totalPairs, intraResults.size());
        }
    }

    // Step 4c: Filter results if in diff mode.
    if (diffMode)
    {
        auto const projectRoot = std::filesystem::weakly_canonical(opts.directory);
        auto const changedBlocks = codedup::DiffFilter::findChangedBlocks(allBlocks, diffResult, projectRoot);

        if (opts.verbose)
            std::println(stderr, "Found {} code blocks overlapping with changed lines", changedBlocks.size());

        groups = codedup::DiffFilter::filterCloneGroups(groups, changedBlocks);
        intraResults = codedup::DiffFilter::filterIntraResults(intraResults, changedBlocks);
    }

    // Step 5: Report results
    codedup::Reporter reporter({
        .useColor = opts.useColor,
        .showSourceCode = opts.showSource,
        .theme = opts.theme,
    });

    std::string output;
    reporter.report(output, groups, allBlocks, allTokens, blockToFileIndex);

    if (!intraResults.empty())
        reporter.reportIntraClones(output, intraResults, allBlocks, allTokens, blockToFileIndex);

    size_t totalIntraPairs = 0;
    for (auto const& r : intraResults)
        totalIntraPairs += r.pairs.size();

    size_t totalDuplicatedLines = 0;
    size_t totalFunctions = 0;
    for (auto const& group : groups)
    {
        for (auto const blockIdx : group.blockIndices)
        {
            auto const& range = allBlocks[blockIdx].sourceRange;
            totalDuplicatedLines += range.end.line - range.start.line + 1;
            ++totalFunctions;
        }
    }

    auto const totalIntraFunctions = intraResults.size();

    reporter.reportSummary(output, {
                                       .totalFiles = files.size(),
                                       .totalBlocks = allBlocks.size(),
                                       .totalGroups = groups.size(),
                                       .totalIntraPairs = totalIntraPairs,
                                       .totalDuplicatedLines = totalDuplicatedLines,
                                       .totalFunctions = totalFunctions,
                                       .totalIntraFunctions = totalIntraFunctions,
                                       .timing = timing,
                                   });

    std::cout << output;

    // Exit code: 0 if no clones, 1 if clones found
    return (groups.empty() && intraResults.empty()) ? 0 : 1;
}
