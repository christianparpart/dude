// SPDX-License-Identifier: Apache-2.0

#include "GitDiffParser.hpp"
#include "GitFileFilter.hpp"
#include <exec/static_thread_pool.hpp>
#include <fnmatch.h>
#include <mcp/AnalysisSession.hpp>
#include <mcp/McpTooling.hpp>
#include <mcpprotocol/McpServer.hpp>
#include <stdexec/execution.hpp>

#include <codedup/AnalysisScope.hpp>
#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/DiffFilter.hpp>
#include <codedup/Encoding.hpp>
#include <codedup/FileScanner.hpp>
#include <codedup/HelpFormatter.hpp>
#include <codedup/IntraFunctionDetector.hpp>
#include <codedup/Language.hpp>
#include <codedup/LanguageRegistry.hpp>
#include <codedup/ProgressBar.hpp>
#include <codedup/Reporter.hpp>
#include <codedup/ReporterFactory.hpp>
#include <codedup/ScopeFilter.hpp>
#include <codedup/SimdCharClassifier.hpp>
#include <codedup/Token.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <vector>

// x86 CPUID for runtime SIMD detection
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define CODEDUP_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace
{

constexpr auto versionString = CODEDUPDETECTOR_VERSION;

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
    std::vector<std::string> globPatterns;                          ///< Filename glob patterns to include.
    codedup::InputEncoding encoding = codedup::InputEncoding::Auto; ///< Input file encoding.
    bool verbose = false;                                           ///< Show progress.
    codedup::AnalysisScope scope = codedup::AnalysisScope::All;     ///< Analysis scope bitmask.
    bool respectGitignore = true;                                   ///< Respect .gitignore when scanning files.
    bool showHelp = false;                                          ///< Show help text.
    bool showVersion = false;                                       ///< Show version.
    bool showExamples = false;                                      ///< Show usage examples.
    bool showInfo = false;                                          ///< Show system capabilities info.
    bool mcpMode = false;                                           ///< Run as MCP server.
    std::string diffBase;                                           ///< Git ref to diff against (enables diff mode).
    std::string reporterSpec; ///< Reporter spec (e.g. "console", "json", "json:file=out.json").
};

void PrintUsage(FILE* out, bool useColor, codedup::ColorTheme theme)
{
    static constexpr auto helpText =
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
        "  -g, --glob <pattern>        Filename glob filter (may be repeated, e.g., -g '*.cpp' -g '*Ctrl*')\n"
        "  --encoding <enc>            Input encoding: auto, utf8, windows-1252 (default: auto)\n"
        "  -s, --scope <scopes>        Comma-separated analysis scopes (default: all)\n"
        "                              Valid: inter-file, intra-file, inter-function,\n"
        "                                     intra-function, all\n"
        "  --reporter <spec>           Output reporter: console (default), json, json:file=<path>\n"
        "  --gitignore                 Respect .gitignore when scanning (default)\n"
        "  --no-gitignore              Include gitignored files in analysis\n"
        "  -v, --verbose               Show progress during scanning\n"
        "  --mcp                       Run as MCP server (JSON-RPC over stdio)\n"
        "  -h, --help                  Show help\n"
        "  --version                   Show version\n"
        "  --show-examples             Show usage examples\n"
        "  --info                      Show system capabilities (threads, SIMD)";
    auto const formatted = codedup::HelpFormatter::FormatHelp(helpText, useColor, theme);
    std::print(out, "{}\n", formatted);
}

/// @brief Prints categorized usage examples to stdout.
void PrintExamples(bool useColor, codedup::ColorTheme theme)
{
    static constexpr auto examplesText = "Usage Examples for codedupdetector\n"
                                         "==================================\n"
                                         "\n"
                                         "Basic Usage\n"
                                         "-----------\n"
                                         "  # Scan a directory with default settings\n"
                                         "  codedupdetector /path/to/project\n"
                                         "\n"
                                         "  # Scan with verbose output\n"
                                         "  codedupdetector -v /path/to/project\n"
                                         "\n"
                                         "Threshold & Sensitivity Tuning\n"
                                         "------------------------------\n"
                                         "  # High threshold: only near-identical clones\n"
                                         "  codedupdetector -t 0.95 /path/to/project\n"
                                         "\n"
                                         "  # Low threshold: find loose similarities\n"
                                         "  codedupdetector -t 0.60 /path/to/project\n"
                                         "\n"
                                         "  # Stricter identifier matching (higher text sensitivity)\n"
                                         "  codedupdetector --text-sensitivity 0.7 /path/to/project\n"
                                         "\n"
                                         "  # Ignore identifier names entirely (structural only)\n"
                                         "  codedupdetector --text-sensitivity 0.0 /path/to/project\n"
                                         "\n"
                                         "Scope Control\n"
                                         "-------------\n"
                                         "  # Only detect inter-file clones\n"
                                         "  codedupdetector -s inter-file /path/to/project\n"
                                         "\n"
                                         "  # Only detect intra-function copy-paste\n"
                                         "  codedupdetector -s intra-function /path/to/project\n"
                                         "\n"
                                         "  # Combine scopes\n"
                                         "  codedupdetector -s inter-file,intra-function /path/to/project\n"
                                         "\n"
                                         "File Filtering\n"
                                         "--------------\n"
                                         "  # Scan only C++ headers and source files\n"
                                         "  codedupdetector -g '*.hpp' -g '*.cpp' /path/to/project\n"
                                         "\n"
                                         "  # Scan only C# files\n"
                                         "  codedupdetector -g '*.cs' /path/to/project\n"
                                         "\n"
                                         "  # Glob-based filename filter\n"
                                         "  codedupdetector -g '*Controller*' /path/to/project\n"
                                         "\n"
                                         "  # Multiple glob patterns\n"
                                         "  codedupdetector -g '*Controller*' -g '*Service*' /path/to/project\n"
                                         "\n"
                                         "  # Include gitignored files in analysis\n"
                                         "  codedupdetector --no-gitignore /path/to/project\n"
                                         "\n"
                                         "Output Control\n"
                                         "--------------\n"
                                         "  # Disable colors (useful for piping or CI logs)\n"
                                         "  codedupdetector --no-color /path/to/project\n"
                                         "\n"
                                         "  # Suppress source code snippets\n"
                                         "  codedupdetector --no-source /path/to/project\n"
                                         "\n"
                                         "  # Machine-readable output (no color, no source)\n"
                                         "  codedupdetector --no-color --no-source /path/to/project\n"
                                         "\n"
                                         "  # Set color theme explicitly\n"
                                         "  codedupdetector --theme dark /path/to/project\n"
                                         "\n"
                                         "CI / Git Integration\n"
                                         "--------------------\n"
                                         "  # Diff mode: only check changed code vs. a branch\n"
                                         "  codedupdetector --diff-base origin/master /path/to/project\n"
                                         "\n"
                                         "  # Diff mode with strict threshold for CI gates\n"
                                         "  codedupdetector --diff-base origin/master -t 0.90 /path/to/project\n"
                                         "\n"
                                         "Combining Options\n"
                                         "-----------------\n"
                                         "  # Full CI pipeline: diff mode, strict threshold, machine-readable,\n"
                                         "  # inter-file scope only\n"
                                         "  codedupdetector --diff-base origin/main -t 0.90 --no-color \\\n"
                                         "      --no-source -s inter-file /path/to/project\n"
                                         "\n"
                                         "MCP Server Mode\n"
                                         "---------------\n"
                                         "  # Start the MCP server for use with AI coding assistants\n"
                                         "  codedupdetector --mcp\n"
                                         "\n"
                                         "  # Claude Code: add to .mcp.json in project root\n"
                                         "  {\n"
                                         "    \"mcpServers\": {\n"
                                         "      \"codedupdetector\": {\n"
                                         "        \"type\": \"stdio\",\n"
                                         "        \"command\": \"/path/to/codedupdetector\",\n"
                                         "        \"args\": [\"--mcp\"]\n"
                                         "      }\n"
                                         "    }\n"
                                         "  }\n"
                                         "\n"
                                         "  # Gemini CLI / Antigravity IDE: add to mcp_config.json\n"
                                         "  {\n"
                                         "    \"mcpServers\": {\n"
                                         "      \"codedupdetector\": {\n"
                                         "        \"command\": \"/path/to/codedupdetector\",\n"
                                         "        \"args\": [\"--mcp\"]\n"
                                         "      }\n"
                                         "    }\n"
                                         "  }";
    auto const formatted = codedup::HelpFormatter::FormatExamples(examplesText, useColor, theme);
    std::print("{}\n", formatted);
}

#if defined(CODEDUP_X86)
/// @brief Queries x86 CPUID leaf/sub-leaf, returns {eax, ebx, ecx, edx}.
auto Cpuid(int leaf, int subleaf = 0) -> std::array<unsigned, 4>
{
    std::array<unsigned, 4> regs{};
#if defined(_MSC_VER)
    int buf[4];
    __cpuidex(buf, leaf, subleaf);
    regs = {static_cast<unsigned>(buf[0]), static_cast<unsigned>(buf[1]), static_cast<unsigned>(buf[2]),
            static_cast<unsigned>(buf[3])};
#else
    __cpuid_count(leaf, subleaf, regs[0], regs[1], regs[2], regs[3]);
#endif
    return regs;
}
#endif

/// @brief Prints system capabilities relevant to this tool.
void PrintInfo()
{
    std::println("System Capabilities");
    std::println("===================");
    std::println("  Threads (hardware concurrency): {}", std::thread::hardware_concurrency());

#if defined(CODEDUP_X86)
    auto const [eax1, ebx1, ecx1, edx1] = Cpuid(1);
    auto const [eax7, ebx7, ecx7, edx7] = Cpuid(7, 0);
    std::println("  CPU SIMD support:");
    std::println("    SSE2:    {}", (edx1 >> 26) & 1 ? "yes" : "no");
    std::println("    SSE3:    {}", (ecx1 >> 0) & 1 ? "yes" : "no");
    std::println("    SSSE3:   {}", (ecx1 >> 9) & 1 ? "yes" : "no");
    std::println("    SSE4.1:  {}", (ecx1 >> 19) & 1 ? "yes" : "no");
    std::println("    SSE4.2:  {}", (ecx1 >> 20) & 1 ? "yes" : "no");
    std::println("    AVX:     {}", (ecx1 >> 28) & 1 ? "yes" : "no");
    std::println("    AVX2:    {}", (ebx7 >> 5) & 1 ? "yes" : "no");
    std::println("    AVX-512: {}", (ebx7 >> 16) & 1 ? "yes" : "no");
#endif

#if CODEDUP_HAS_SIMD
    namespace stdx = std::experimental;
    using SimdU8 = stdx::native_simd<uint8_t>;
    using SimdU32 = stdx::native_simd<uint32_t>;
    std::println("  Binary SIMD (compiled):");
    std::println("    Vector width (uint8):  {} elements ({} bits)", SimdU8::size(), SimdU8::size() * 8);
    std::println("    Vector width (uint32): {} elements ({} bits)", SimdU32::size(), SimdU32::size() * 32);
#else
    std::println("  Binary SIMD:             not available");
#endif
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
    if (arg == "--show-examples")
    {
        opts.showExamples = true;
        return opts;
    }
    if (arg == "--info")
    {
        opts.showInfo = true;
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
    if (arg == "-g" || arg == "--glob")
        return ParseStringOption(argc, argv, i, "--glob")
            .transform(
                [&](std::string v) -> CliOptions
                {
                    opts.globPatterns.push_back(std::move(v));
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
    if (arg == "--reporter")
        return ParseStringOption(argc, argv, i, "--reporter")
            .transform(
                [&](std::string v) -> CliOptions
                {
                    opts.reporterSpec = std::move(v);
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
    if (arg == "-s" || arg == "--scope")
        return ParseStringOption(argc, argv, i, "--scope")
            .and_then(
                [&](std::string const& v) -> std::expected<CliOptions, std::string>
                {
                    auto const scopeResult = codedup::ParseAnalysisScope(v);
                    if (!scopeResult)
                        return std::unexpected(scopeResult.error().message);
                    opts.scope = *scopeResult;
                    return opts;
                });
    if (arg == "--mcp")
    {
        opts.mcpMode = true;
        return opts;
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
            if (opts.showHelp || opts.showVersion || opts.showExamples || opts.showInfo || opts.mcpMode)
                return opts;
        }
    }

    if (!opts.showHelp && !opts.showVersion && !opts.showExamples && !opts.showInfo && !opts.mcpMode &&
        opts.directory.empty())
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

    auto const extensions =
        opts.globPatterns.empty() ? codedup::FileScanner::DefaultExtensions() : std::vector<std::string>{};
    auto diffResult = cli::GitDiffParser::ParseDiffOutput(*diffOutput, extensions);

    // Post-filter by glob patterns when active
    if (!opts.globPatterns.empty())
    {
        std::erase_if(diffResult,
                      [&](auto const& fc)
                      {
                          auto const filename = fc.filePath.filename().string();
                          return !std::ranges::any_of(opts.globPatterns, [&filename](std::string const& pattern)
                                                      { return fnmatch(pattern.c_str(), filename.c_str(), 0) == 0; });
                      });
    }

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
    // When glob patterns are active, skip extension filtering (pass empty → accept all)
    auto const extensions =
        opts.globPatterns.empty() ? codedup::FileScanner::DefaultExtensions() : std::vector<std::string>{};

    // Build an optional gitignore-aware filter.
    auto const gitFilter =
        opts.respectGitignore ? cli::GitFileFilter::CreateFilter(opts.directory, opts.verbose) : std::nullopt;

    // Build an optional glob-based filename filter.
    auto const globFilter =
        opts.globPatterns.empty()
            ? std::optional<codedup::FileFilter>(std::nullopt)
            : std::optional<codedup::FileFilter>(
                  [patterns = opts.globPatterns](std::filesystem::path const& path) -> bool
                  {
                      auto const filename = path.filename().string();
                      return std::ranges::any_of(patterns, [&filename](std::string const& pattern)
                                                 { return fnmatch(pattern.c_str(), filename.c_str(), 0) == 0; });
                  });

    // Compose all filters into a single predicate.
    auto const composedFilter = (gitFilter || globFilter)
                                    ? std::optional<codedup::FileFilter>(
                                          [gitFilter, globFilter](std::filesystem::path const& path) -> bool
                                          {
                                              if (gitFilter && !(*gitFilter)(path))
                                                  return false;
                                              if (globFilter && !(*globFilter)(path))
                                                  return false;
                                              return true;
                                          })
                                    : std::optional<codedup::FileFilter>(std::nullopt);

    auto const filesResult = codedup::FileScanner::Scan(opts.directory, extensions, composedFilter);
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
/// Each file is tokenized using the appropriate language implementation based on
/// file extension. Files with unrecognized extensions or tokenization failures
/// produce a warning on stderr and contribute an empty token vector.
///
/// @param files The source file paths to tokenize.
/// @param opts The parsed CLI options (for encoding and verbosity).
/// @param timing Performance timing struct to record tokenization duration.
/// @return A pair of (token vectors, language pointers), one per file (in the same order as files).
auto TokenizeFiles(std::vector<std::filesystem::path> const& files, CliOptions const& opts,
                   codedup::PerformanceTiming& timing, codedup::ProgressBar* progressBar)
    -> std::pair<std::vector<std::vector<codedup::Token>>, std::vector<codedup::Language const*>>
{
    using Clock = std::chrono::steady_clock;

    auto const tokenizeStart = Clock::now();
    auto const numFiles = files.size();

    std::vector<std::vector<codedup::Token>> allTokens(numFiles);
    std::vector<codedup::Language const*> fileLanguages(numFiles, nullptr);

    auto const& registry = codedup::LanguageRegistry::Instance();

    // Pre-resolve languages (lightweight, sequential)
    for (size_t fi = 0; fi < numFiles; ++fi)
        fileLanguages[fi] = registry.FindByPath(files[fi]);

    // Parallel tokenization using stdexec::bulk
    {
        exec::static_thread_pool pool(std::thread::hardware_concurrency());
        auto sched = pool.get_scheduler();

        auto work = stdexec::starts_on(
            sched, stdexec::just() | stdexec::bulk(stdexec::par, numFiles,
                                                   [&](std::size_t fi)
                                                   {
                                                       auto const* language = fileLanguages[fi];
                                                       if (!language)
                                                           return;
                                                       auto const fileIndex = static_cast<uint32_t>(fi);
                                                       auto result =
                                                           language->TokenizeFile(files[fi], fileIndex, opts.encoding);
                                                       if (result)
                                                           allTokens[fi] = std::move(*result);
                                                       if (progressBar)
                                                           progressBar->Tick();
                                                   }));
        stdexec::sync_wait(work);
    }

    // Sequential verbose output and error reporting
    if (opts.verbose)
    {
        for (size_t fi = 0; fi < numFiles; ++fi)
        {
            auto const logMsg = [&](std::string const& msg)
            {
                if (progressBar && progressBar->IsActive())
                    progressBar->Log(msg);
                else
                    std::println(stderr, "{}", msg);
            };

            if (!fileLanguages[fi])
                logMsg(std::format("Warning: No language support for {}", files[fi].string()));
            else if (allTokens[fi].empty())
                logMsg(std::format("Warning: Failed to tokenize {}", files[fi].string()));
            else
                logMsg(std::format("Tokenized ({}): {}", fileLanguages[fi]->Name(), files[fi].string()));
        }
    }

    timing.tokenizing = Clock::now() - tokenizeStart;

    return {std::move(allTokens), std::move(fileLanguages)};
}

/// @brief Normalizes tokens and extracts code blocks (step 3).
///
/// For each non-empty token vector, normalizes the tokens structurally (and
/// optionally text-preserving) using language-aware stripping, and extracts
/// function-level code blocks via the language's block extractor.
///
/// @param allTokens Token vectors for all files.
/// @param fileLanguages Language pointers for each file (may be nullptr).
/// @param files Source file paths (for verbose output).
/// @param opts The parsed CLI options (for minTokens, textSensitivity, verbosity).
/// @param timing Performance timing struct to record normalization duration.
/// @return A tuple of (all extracted code blocks, block-to-file-index mapping).
auto ExtractBlocks(std::vector<std::vector<codedup::Token>> const& allTokens,
                   std::vector<codedup::Language const*> const& fileLanguages,
                   std::vector<std::filesystem::path> const& files, CliOptions const& opts,
                   codedup::PerformanceTiming& timing, codedup::ProgressBar* progressBar)
    -> std::tuple<std::vector<codedup::CodeBlock>, std::vector<size_t>>
{
    using Clock = std::chrono::steady_clock;

    auto const normalizeStart = Clock::now();
    codedup::TokenNormalizer normalizer;
    codedup::CodeBlockExtractorConfig const config{.minTokens = opts.minTokens};

    std::vector<codedup::CodeBlock> allBlocks;
    std::vector<size_t> blockToFileIndex;

    auto const useTextSensitivity = opts.textSensitivity > 0.0;

    for (auto const fi : std::views::iota(size_t{0}, allTokens.size()))
    {
        if (allTokens[fi].empty())
            continue;

        auto const* language = fileLanguages[fi];
        if (!language)
            continue;

        auto normalized = normalizer.Normalize(allTokens[fi], language);
        auto textPreserving = useTextSensitivity ? normalizer.NormalizeTextPreserving(allTokens[fi], language)
                                                 : std::vector<codedup::NormalizedToken>{};
        auto blocks = language->ExtractBlocks(allTokens[fi], normalized, textPreserving, config);

        if (opts.verbose && !blocks.empty())
        {
            auto const msg = std::format("  {} blocks from {}", blocks.size(), files[fi].string());
            if (progressBar && progressBar->IsActive())
                progressBar->Log(msg);
            else
                std::println(stderr, "{}", msg);
        }

        for (auto& block : blocks)
        {
            blockToFileIndex.push_back(fi);
            allBlocks.push_back(std::move(block));
        }

        if (progressBar)
            progressBar->Tick();
    }
    timing.normalizing = Clock::now() - normalizeStart;

    if (opts.verbose)
        std::println(stderr, "Extracted {} code blocks total", allBlocks.size());

    return {std::move(allBlocks), std::move(blockToFileIndex)};
}

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char* argv[])
{
    auto const optsResult = ParseArgs(argc, argv);
    if (!optsResult)
    {
        std::println(stderr, "Error: {}\n", optsResult.error());
        PrintUsage(stderr, false, codedup::ColorTheme::Auto);
        return 2;
    }

    auto const& opts = *optsResult;

    if (opts.showHelp)
    {
        PrintUsage(stdout, opts.useColor, opts.theme);
        return 0;
    }

    if (opts.showVersion)
    {
        std::println("codedupdetector {}", versionString);
        return 0;
    }

    if (opts.showExamples)
    {
        PrintExamples(opts.useColor, opts.theme);
        return 0;
    }

    if (opts.showInfo)
    {
        PrintInfo();
        return 0;
    }

    if (opts.mcpMode)
    {
        mcp::AnalysisSession session;
        mcpprotocol::McpServer server({
            .name = "codedupdetector",
            .version = versionString,
            .title = "CodeDupDetector",
            .description = "Code duplication detection and analysis tool",
            .websiteUrl = {},
        });
        mcp::RegisterCodeDupTools(server, session);
        return server.Run();
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

    // Step 2: Tokenize all files (language-aware)
    auto tokenBar = opts.verbose ? std::make_optional<codedup::ProgressBar>("Tokenizing", files.size()) : std::nullopt;
    if (tokenBar)
        tokenBar->Start();
    auto [allTokens, fileLanguages] = TokenizeFiles(files, opts, timing, tokenBar ? &*tokenBar : nullptr);
    if (tokenBar)
        tokenBar->Finish();

    // Step 3: Normalize and extract blocks (language-aware)
    auto extractBar =
        opts.verbose ? std::make_optional<codedup::ProgressBar>("Extracting", files.size()) : std::nullopt;
    if (extractBar)
        extractBar->Start();
    auto [allBlocks, blockToFileIndex] =
        ExtractBlocks(allTokens, fileLanguages, files, opts, timing, extractBar ? &*extractBar : nullptr);
    if (extractBar)
        extractBar->Finish();

    // Step 4: Detect clones
    using Clock = std::chrono::steady_clock;

    std::vector<codedup::CloneGroup> groups;
    if (codedup::HasInterFunctionScope(opts.scope))
    {
        auto const detectStart = Clock::now();
        codedup::CloneDetector detector({
            .similarityThreshold = opts.threshold,
            .minTokens = opts.minTokens,
            .textSensitivity = opts.textSensitivity,
        });

        auto detectBar = opts.verbose ? std::make_optional<codedup::ProgressBar>("Detecting", size_t{0}) : std::nullopt;
        if (detectBar)
            detectBar->Start();
        groups =
            detector.Detect(allBlocks, detectBar ? detectBar->MakeAbsoluteCallback() : codedup::ProgressCallback{});
        if (detectBar)
            detectBar->Finish();
        timing.cloneDetection = Clock::now() - detectStart;

        // Apply scope-based filtering (inter-file vs intra-file).
        groups = codedup::ScopeFilter::FilterCloneGroups(groups, blockToFileIndex, opts.scope);

        std::ranges::sort(groups,
                          [&allBlocks](auto const& a, auto const& b)
                          {
                              auto const tokensA = allBlocks[a.blockIndices.front()].tokenEnd -
                                                   allBlocks[a.blockIndices.front()].tokenStart;
                              auto const tokensB = allBlocks[b.blockIndices.front()].tokenEnd -
                                                   allBlocks[b.blockIndices.front()].tokenStart;
                              return tokensA > tokensB;
                          });
    }

    // Step 4b: Detect intra-function clones
    std::vector<codedup::IntraCloneResult> intraResults;
    if (codedup::HasScope(opts.scope, codedup::AnalysisScope::IntraFunction))
    {
        if (opts.verbose)
            std::println(stderr, "Detecting intra-function clones...");

        auto const intraStart = Clock::now();
        codedup::IntraFunctionDetector intraDetector({
            .minRegionTokens = opts.minTokens,
            .similarityThreshold = opts.threshold,
            .textSensitivity = opts.textSensitivity,
        });

        auto intraBar =
            opts.verbose ? std::make_optional<codedup::ProgressBar>("Intra-detect", allBlocks.size()) : std::nullopt;
        if (intraBar)
            intraBar->Start();
        intraResults =
            intraDetector.Detect(allBlocks, intraBar ? intraBar->MakeAbsoluteCallback() : codedup::ProgressCallback{});
        if (intraBar)
            intraBar->Finish();
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
        auto const changedBlocks = codedup::DiffFilter::FindChangedBlocks(allBlocks, diffResult, projectRoot, files);

        if (opts.verbose)
            std::println(stderr, "Found {} code blocks overlapping with changed lines", changedBlocks.size());

        groups = codedup::DiffFilter::FilterCloneGroups(groups, changedBlocks);
        intraResults = codedup::DiffFilter::FilterIntraResults(intraResults, changedBlocks);
    }

    // Step 4d: Release token vectors for non-participating files to reduce peak memory.
    {
        std::unordered_set<size_t> participatingFiles;
        for (auto const& group : groups)
            for (auto const idx : group.blockIndices)
                participatingFiles.insert(blockToFileIndex[idx]);
        for (auto const& result : intraResults)
            participatingFiles.insert(blockToFileIndex[result.blockIndex]);

        for (size_t fi = 0; fi < allTokens.size(); ++fi)
        {
            if (!participatingFiles.contains(fi))
            {
                allTokens[fi].clear();
                allTokens[fi].shrink_to_fit();
            }
        }

        if (opts.verbose)
        {
            auto const released = allTokens.size() - participatingFiles.size();
            std::println(stderr, "Released token vectors for {} non-participating files", released);
        }
    }

    // Step 5: Report results
    codedup::ReporterConfig const consoleConfig{
        .useColor = opts.useColor,
        .showSourceCode = opts.showSource,
        .theme = opts.theme,
    };

    auto reporterResult = codedup::CreateReporter(opts.reporterSpec, consoleConfig);
    if (!reporterResult)
    {
        std::println(stderr, "Error: {}", reporterResult.error().message);
        return 2;
    }
    auto const& reporter = *reporterResult;

    auto const specResult = codedup::ParseReporterSpec(opts.reporterSpec);

    reporter->Report(groups, allBlocks, allTokens, blockToFileIndex, files);

    if (!intraResults.empty())
        reporter->ReportIntraClones(intraResults, allBlocks, allTokens, blockToFileIndex, files);

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

    reporter->ReportSummary({
        .totalFiles = files.size(),
        .totalBlocks = allBlocks.size(),
        .totalGroups = groups.size(),
        .totalIntraPairs = totalIntraPairs,
        .totalDuplicatedLines = totalDuplicatedLines,
        .totalFunctions = totalFunctions,
        .totalIntraFunctions = totalIntraFunctions,
        .timing = timing,
        .activeScope = opts.scope,
    });

    // Write output to file or stdout
    if (specResult && specResult->outputPath)
    {
        std::ofstream file(*specResult->outputPath);
        if (!file)
        {
            std::println(stderr, "Error: Cannot open output file: {}", *specResult->outputPath);
            return 2;
        }
        reporter->WriteTo(file);
    }
    else
    {
        reporter->WriteTo(std::cout);
    }

    // Exit code: 0 if no clones, 1 if clones found
    return (groups.empty() && intraResults.empty()) ? 0 : 1;
}
