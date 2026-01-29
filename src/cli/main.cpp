// SPDX-License-Identifier: Apache-2.0

#include "GitDiffParser.hpp"
#include "GitFileFilter.hpp"

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
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
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
    bool respectGitignore = true;                                   ///< Respect .gitignore when scanning files.
    bool showHelp = false;                                          ///< Show help text.
    bool showVersion = false;                                       ///< Show version.
    std::string diffBase;                                           ///< Git ref to diff against (enables diff mode).
};

void PrintUsage(FILE* out)
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
                 "  --gitignore                 Respect .gitignore when scanning (default)\n"
                 "  --no-gitignore              Include gitignored files in analysis\n"
                 "  --intra                     Enable intra-function clone detection (default)\n"
                 "  --no-intra                  Disable intra-function clone detection\n"
                 "  -v, --verbose               Show progress during scanning\n"
                 "  -h, --help                  Show help\n"
                 "  --version                   Show version");
}

// ---------------------------------------------------------------------------
// ParseArgs helper functions
// ---------------------------------------------------------------------------

/// @brief Parses a double-valued option from the command-line argument list.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @param name Display name of the option for error messages (e.g. "--threshold").
/// @param min Minimum allowed value (inclusive).
/// @param max Maximum allowed value (inclusive).
/// @param rangeError Error message to use when the value is out of range.
/// @return The parsed double value, or an error string.
auto ParseDoubleOption(int argc, char* argv[], int& i, std::string_view name, double min, double max,
                       std::string_view rangeError) -> std::expected<double, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::format("Missing value for {}", name));
    auto const value = std::stod(argv[i]);
    if (value < min || value > max)
        return std::unexpected(std::string(rangeError));
    return value;
}

/// @brief Parses a size_t-valued option from the command-line argument list.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @param name Display name of the option for error messages.
/// @return The parsed size_t value, or an error string.
auto ParseSizeOption(int argc, char* argv[], int& i, std::string_view name) -> std::expected<size_t, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::format("Missing value for {}", name));
    return static_cast<size_t>(std::stoul(argv[i]));
}

/// @brief Parses a string-valued option from the command-line argument list.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @param name Display name of the option for error messages.
/// @return The parsed string value, or an error string.
auto ParseStringOption(int argc, char* argv[], int& i, std::string_view name) -> std::expected<std::string, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::format("Missing value for {}", name));
    return std::string(argv[i]);
}

/// @brief Parses the --theme option value into a ColorTheme enum.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @return The parsed ColorTheme value, or an error string.
auto ParseThemeOption(int argc, char* argv[], int& i) -> std::expected<codedup::ColorTheme, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::string("Missing value for --theme"));
    auto const val = std::string_view(argv[i]);
    if (val == "dark")
        return codedup::ColorTheme::Dark;
    if (val == "light")
        return codedup::ColorTheme::Light;
    if (val == "auto")
        return codedup::ColorTheme::Auto;
    return std::unexpected(std::format("Unknown theme: {}", val));
}

/// @brief Parses a comma-separated list of file extensions from the command-line.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @return A vector of extension strings (each prefixed with '.'), or an error string.
auto ParseExtensionsOption(int argc, char* argv[], int& i) -> std::expected<std::vector<std::string>, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::string("Missing value for --extensions"));
    auto const list = std::string(argv[i]);
    std::vector<std::string> extensions;
    size_t start = 0;
    while (start < list.size())
    {
        auto const end = list.find(',', start);
        auto ext = list.substr(start, end == std::string::npos ? end : end - start);
        if (!ext.empty() && ext[0] != '.')
            ext.insert(0, ".");
        extensions.push_back(std::move(ext));
        start = (end == std::string::npos) ? list.size() : end + 1;
    }
    return extensions;
}

/// @brief Parses the --encoding option value into an InputEncoding enum.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @return The parsed InputEncoding value, or an error string.
auto ParseEncodingOption(int argc, char* argv[], int& i) -> std::expected<codedup::InputEncoding, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::string("Missing value for --encoding"));
    auto const encResult = codedup::ParseEncodingName(argv[i]);
    if (!encResult)
        return std::unexpected(encResult.error().message);
    return *encResult;
}

/// @brief Processes a single command-line argument, updating the options struct.
///
/// Returns std::nullopt on success (continue parsing), or a CliOptions / error
/// to return immediately from ParseArgs.
///
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (may be advanced for options with values).
/// @param opts The options struct to populate.
/// @return std::nullopt to continue parsing, or a final result to return.
auto ProcessArg(int argc, char* argv[], int& i, CliOptions& opts)
    -> std::optional<std::expected<CliOptions, std::string>>
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
        return ParseDoubleOption(argc, argv, i, "--threshold", 0.0, 1.0, "Threshold must be between 0.0 and 1.0")
            .transform(
                [&](double v) -> CliOptions
                {
                    opts.threshold = v;
                    return opts;
                });
    if (arg == "-m" || arg == "--min-tokens")
        return ParseSizeOption(argc, argv, i, "--min-tokens")
            .transform(
                [&](size_t v) -> CliOptions
                {
                    opts.minTokens = v;
                    return opts;
                });
    if (arg == "--text-sensitivity")
        return ParseDoubleOption(argc, argv, i, "--text-sensitivity", 0.0, 1.0,
                                 "Text sensitivity must be between 0.0 and 1.0")
            .transform(
                [&](double v) -> CliOptions
                {
                    opts.textSensitivity = v;
                    return opts;
                });
    if (arg == "--diff-base")
        return ParseStringOption(argc, argv, i, "--diff-base")
            .transform(
                [&](std::string v) -> CliOptions
                {
                    opts.diffBase = std::move(v);
                    return opts;
                });
    if (arg == "--no-color")
    {
        opts.useColor = false;
        return std::nullopt;
    }
    if (arg == "--no-source")
    {
        opts.showSource = false;
        return std::nullopt;
    }
    if (arg == "--theme")
        return ParseThemeOption(argc, argv, i)
            .transform(
                [&](codedup::ColorTheme v) -> CliOptions
                {
                    opts.theme = v;
                    return opts;
                });
    if (arg == "-e" || arg == "--extensions")
        return ParseExtensionsOption(argc, argv, i)
            .transform(
                [&](std::vector<std::string> v) -> CliOptions
                {
                    opts.extensions = std::move(v);
                    return opts;
                });
    if (arg == "--encoding")
        return ParseEncodingOption(argc, argv, i)
            .transform(
                [&](codedup::InputEncoding v) -> CliOptions
                {
                    opts.encoding = v;
                    return opts;
                });
    if (arg == "--gitignore")
    {
        opts.respectGitignore = true;
        return std::nullopt;
    }
    if (arg == "--no-gitignore")
    {
        opts.respectGitignore = false;
        return std::nullopt;
    }
    if (arg == "--intra")
    {
        opts.detectIntraClones = true;
        return std::nullopt;
    }
    if (arg == "--no-intra")
    {
        opts.detectIntraClones = false;
        return std::nullopt;
    }
    if (arg == "-v" || arg == "--verbose")
    {
        opts.verbose = true;
        return std::nullopt;
    }
    if (arg.starts_with("-"))
        return std::expected<CliOptions, std::string>(std::unexpected(std::format("Unknown option: {}", arg)));

    // Positional argument: directory
    if (!opts.directory.empty())
        return std::expected<CliOptions, std::string>(std::unexpected(std::string("Multiple directories specified")));
    opts.directory = arg;
    return std::nullopt;
}

/// @brief Parses command-line arguments into a CliOptions struct.
/// @param argc Argument count from main().
/// @param argv Argument vector from main().
/// @return Parsed options on success, or an error string.
auto ParseArgs(int argc, char* argv[]) -> std::expected<CliOptions, std::string>
{
    CliOptions opts;

    for (int i = 1; i < argc; ++i)
    {
        auto result = ProcessArg(argc, argv, i, opts);
        if (result.has_value())
        {
            // For value-bearing options that succeeded, we just continue parsing.
            // Only return early for errors, --help, or --version.
            if (!result->has_value())
                return std::unexpected(std::move(result->error()));
            if (opts.showHelp || opts.showVersion)
                return opts;
        }
    }

    if (!opts.showHelp && !opts.showVersion && opts.directory.empty())
        return std::unexpected("No directory specified");

    return opts;
}

// ---------------------------------------------------------------------------
// main() pipeline stage helpers
// ---------------------------------------------------------------------------

/// @brief Runs git diff setup when diff mode is active (step 0).
///
/// Executes git diff against the specified base ref and parses the output
/// into structured diff data. Prints progress and results to stderr.
///
/// @param opts The parsed CLI options.
/// @return The parsed diff result on success, or an exit code on failure.
///         Returns an empty DiffResult if diff mode is not active.
auto RunDiffSetup(CliOptions const& opts) -> std::expected<codedup::DiffResult, int>
{
    if (opts.diffBase.empty())
        return codedup::DiffResult{};

    if (opts.verbose)
        std::println(stderr, "Running git diff against {}...", opts.diffBase);

    auto const projectRoot = std::filesystem::weakly_canonical(opts.directory);
    auto const diffOutput = cli::GitDiffParser::RunGitDiff(projectRoot, opts.diffBase);
    if (!diffOutput)
    {
        std::println(stderr, "Error: {}", diffOutput.error().message);
        return std::unexpected(2);
    }

    auto const& extensions = opts.extensions.empty() ? codedup::FileScanner::DefaultExtensions() : opts.extensions;
    auto diffResult = cli::GitDiffParser::ParseDiffOutput(*diffOutput, extensions);

    if (diffResult.empty())
    {
        std::println("No C++ files changed relative to {}.", opts.diffBase);
        return std::unexpected(0);
    }

    std::println(stderr, "Checking for duplication in changes relative to `{}`...", opts.diffBase);

    if (opts.verbose)
    {
        for (auto const& fc : diffResult)
            std::println(stderr, "  Changed: {} ({} hunks)", fc.filePath.string(), fc.changedRanges.size());
    }

    return diffResult;
}

/// @brief Scans the directory for source files (step 1).
///
/// Uses the extensions from CLI options or the default set.
/// Records the scanning duration into the provided timing struct.
///
/// @param opts The parsed CLI options.
/// @param timing Performance timing struct to record scan duration.
/// @return A vector of source file paths on success, or an exit code on failure.
auto ScanFiles(CliOptions const& opts, codedup::PerformanceTiming& timing)
    -> std::expected<std::vector<std::filesystem::path>, int>
{
    using Clock = std::chrono::steady_clock;

    if (opts.verbose)
        std::println(stderr, "Scanning directory: {}", opts.directory.string());

    auto const scanStart = Clock::now();
    auto const& extensions = opts.extensions.empty() ? codedup::FileScanner::DefaultExtensions() : opts.extensions;

    // Build an optional gitignore-aware filter.
    auto const gitFilter =
        opts.respectGitignore ? cli::GitFileFilter::CreateFilter(opts.directory, opts.verbose) : std::nullopt;

    auto const filesResult = codedup::FileScanner::Scan(opts.directory, extensions, gitFilter);
    timing.scanning = Clock::now() - scanStart;
    if (!filesResult)
    {
        std::println(stderr, "Error: {}", filesResult.error().message);
        return std::unexpected(2);
    }

    if (opts.verbose)
        std::println(stderr, "Found {} source files", filesResult->size());

    return *filesResult;
}

/// @brief Tokenizes all source files (step 2).
///
/// Each file is tokenized independently. Files that fail to tokenize produce
/// a warning on stderr and contribute an empty token vector.
///
/// @param files The source file paths to tokenize.
/// @param opts The parsed CLI options (for encoding and verbosity).
/// @param timing Performance timing struct to record tokenization duration.
/// @return A vector of token vectors, one per file (in the same order as files).
auto TokenizeFiles(std::vector<std::filesystem::path> const& files, CliOptions const& opts,
                   codedup::PerformanceTiming& timing) -> std::vector<std::vector<codedup::Token>>
{
    using Clock = std::chrono::steady_clock;

    auto const tokenizeStart = Clock::now();
    std::vector<std::vector<codedup::Token>> allTokens;
    allTokens.reserve(files.size());

    for (auto const& file : files)
    {
        if (opts.verbose)
            std::println(stderr, "Tokenizing: {}", file.string());

        auto tokensResult = codedup::Tokenizer::TokenizeFile(file, opts.encoding);
        if (!tokensResult)
        {
            std::println(stderr, "Warning: Failed to tokenize {}: {}", file.string(), tokensResult.error().message);
            allTokens.emplace_back(); // Empty tokens for failed files
            continue;
        }
        allTokens.push_back(std::move(*tokensResult));
    }
    timing.tokenizing = Clock::now() - tokenizeStart;

    return allTokens;
}

/// @brief Normalizes tokens and extracts code blocks (step 3).
///
/// For each non-empty token vector, normalizes the tokens structurally (and
/// optionally text-preserving) and extracts function-level code blocks.
///
/// @param allTokens Token vectors for all files.
/// @param files Source file paths (for verbose output).
/// @param opts The parsed CLI options (for minTokens, textSensitivity, verbosity).
/// @param timing Performance timing struct to record normalization duration.
/// @return A tuple of (all extracted code blocks, block-to-file-index mapping).
auto ExtractBlocks(std::vector<std::vector<codedup::Token>> const& allTokens,
                   std::vector<std::filesystem::path> const& files, CliOptions const& opts,
                   codedup::PerformanceTiming& timing)
    -> std::tuple<std::vector<codedup::CodeBlock>, std::vector<size_t>>
{
    using Clock = std::chrono::steady_clock;

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

        auto normalized = normalizer.Normalize(allTokens[fi]);
        auto textPreserving = useTextSensitivity ? normalizer.NormalizeTextPreserving(allTokens[fi])
                                                 : std::vector<codedup::NormalizedToken>{};
        auto blocks = extractor.Extract(allTokens[fi], normalized, textPreserving);

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

    return {std::move(allBlocks), std::move(blockToFileIndex)};
}

} // namespace

int main(int argc, char* argv[])
{
    auto const optsResult = ParseArgs(argc, argv);
    if (!optsResult)
    {
        std::println(stderr, "Error: {}\n", optsResult.error());
        PrintUsage(stderr);
        return 2;
    }

    auto const& opts = *optsResult;

    if (opts.showHelp)
    {
        PrintUsage(stdout);
        return 0;
    }

    if (opts.showVersion)
    {
        std::println("codedupdetector {}", versionString);
        return 0;
    }

    auto const diffMode = !opts.diffBase.empty();

    // Step 0: Parse git diff if in diff mode.
    auto const diffSetupResult = RunDiffSetup(opts);
    if (!diffSetupResult)
        return diffSetupResult.error();
    auto const& diffResult = *diffSetupResult;

    codedup::PerformanceTiming timing;

    // Step 1: Scan files
    auto const filesResult = ScanFiles(opts, timing);
    if (!filesResult)
        return filesResult.error();
    auto const& files = *filesResult;

    // Step 2: Tokenize all files
    auto allTokens = TokenizeFiles(files, opts, timing);

    // Step 3: Normalize and extract blocks
    auto [allBlocks, blockToFileIndex] = ExtractBlocks(allTokens, files, opts, timing);

    // Step 4: Detect clones
    using Clock = std::chrono::steady_clock;

    auto const detectStart = Clock::now();
    codedup::CloneDetector detector({
        .similarityThreshold = opts.threshold,
        .minTokens = opts.minTokens,
        .textSensitivity = opts.textSensitivity,
    });

    auto groups = detector.Detect(allBlocks);
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

        intraResults = intraDetector.Detect(allBlocks);
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
        auto const changedBlocks = codedup::DiffFilter::FindChangedBlocks(allBlocks, diffResult, projectRoot);

        if (opts.verbose)
            std::println(stderr, "Found {} code blocks overlapping with changed lines", changedBlocks.size());

        groups = codedup::DiffFilter::FilterCloneGroups(groups, changedBlocks);
        intraResults = codedup::DiffFilter::FilterIntraResults(intraResults, changedBlocks);
    }

    // Step 5: Report results
    codedup::Reporter reporter({
        .useColor = opts.useColor,
        .showSourceCode = opts.showSource,
        .theme = opts.theme,
    });

    std::string output;
    reporter.Report(output, groups, allBlocks, allTokens, blockToFileIndex);

    if (!intraResults.empty())
        reporter.ReportIntraClones(output, intraResults, allBlocks, allTokens, blockToFileIndex);

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

    reporter.ReportSummary(output, {
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
