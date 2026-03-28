// SPDX-License-Identifier: Apache-2.0

#include <git/GitDiffParser.hpp>
#include <git/GitFileFilter.hpp>
#include <mcp/AnalysisSession.hpp>
#include <mcp/McpTooling.hpp>
#include <mcpprotocol/McpServer.hpp>

#include <dude/AnalysisScope.hpp>
#include <dude/BaselineStore.hpp>
#include <dude/BlockCache.hpp>
#include <dude/CloneDetector.hpp>
#include <dude/CodeBlock.hpp>
#include <dude/ContentHash.hpp>
#include <dude/DiffFilter.hpp>
#include <dude/Encoding.hpp>
#include <dude/FileScanner.hpp>
#include <dude/GlobMatch.hpp>
#include <dude/HelpFormatter.hpp>
#include <dude/IntraFunctionDetector.hpp>
#include <dude/Language.hpp>
#include <dude/LanguageRegistry.hpp>
#include <dude/MappedFile.hpp>
#include <dude/ProgressBar.hpp>
#include <dude/Reporter.hpp>
#include <dude/ReporterFactory.hpp>
#include <dude/ScopeFilter.hpp>
#include <dude/SimdCharClassifier.hpp>
#include <dude/Token.hpp>
#include <dude/TokenNormalizer.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <vector>

// x86 CPUID for runtime SIMD detection
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define DUDE_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace
{

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

/// @brief Global flag set by the signal handler to request graceful shutdown.
std::atomic<bool> gInterrupted{false};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/// @brief Signal handler for SIGINT and SIGTERM.
///
/// Sets the global interrupted flag so pipeline phases can check for cancellation
/// between stages. Only performs an atomic store, which is safe in signal context.
void SignalHandler(int /*signum*/)
{
    gInterrupted.store(true, std::memory_order_relaxed);
}

/// @brief Installs signal handlers for graceful shutdown on SIGINT and SIGTERM.
void InstallSignalHandlers()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
}

/// @brief Checks if the process has been interrupted and exits cleanly if so.
/// @return Exit code 2 if interrupted, or std::nullopt to continue.
auto CheckInterrupted() -> std::optional<int>
{
    if (gInterrupted.load(std::memory_order_relaxed))
    {
        std::println(stderr, "\nInterrupted.");
        return 2;
    }
    return std::nullopt;
}

constexpr auto versionString = DUDE_VERSION;

/// @brief Parsed command-line arguments.
struct CliOptions
{
    std::filesystem::path directory;                 ///< Directory to scan.
    double threshold = 0.90;                         ///< Similarity threshold.
    size_t minTokens = 300;                          ///< Minimum block size in tokens.
    size_t limit = 0;                                ///< Limit output to top N findings (0 = unlimited).
    double textSensitivity = 0.3;                    ///< Text sensitivity blend factor.
    bool useColor = true;                            ///< Whether to use ANSI colors.
    bool showSource = true;                          ///< Whether to show source snippets.
    dude::ColorTheme theme = dude::ColorTheme::Auto; ///< Color theme.
    std::vector<std::string> globPatterns;           ///< Filename glob patterns to include.
    std::vector<std::string> excludePatterns;        ///< Glob patterns to exclude (matched against relative path).
    dude::InputEncoding encoding = dude::InputEncoding::Auto; ///< Input file encoding.
    bool verbose = false;                                     ///< Show verbose diagnostics.
    bool showProgress = false;                                ///< Show progress bars.
    dude::AnalysisScope scope = dude::AnalysisScope::All;     ///< Analysis scope bitmask.
    bool respectGitignore = true;                             ///< Respect .gitignore when scanning files.
    bool showHelp = false;                                    ///< Show help text.
    bool showVersion = false;                                 ///< Show version.
    bool showExamples = false;                                ///< Show usage examples.
    bool showInfo = false;                                    ///< Show system capabilities info.
    bool mcpMode = false;                                     ///< Run as MCP server.
    std::string diffBase;                                     ///< Git ref to diff against (enables diff mode).
    std::vector<std::string> diffCommits;                     ///< Commit SHAs to diff (enables commit-diff mode).
    std::string reporterSpec;    ///< Reporter spec (e.g. "console", "json", "json:file=out.json").
    bool enableCache = true;     ///< Enable block extraction cache.
    std::string saveBaseline;    ///< Save results as this baseline name (empty = disabled).
    std::string compareBaseline; ///< Compare against this baseline (empty = disabled).
};

void PrintUsage(FILE* out, bool useColor, dude::ColorTheme theme)
{
    static constexpr auto helpText =
        "Usage: dude [OPTIONS] <directory>\n"
        "\n"
        "Options:\n"
        "  -t, --threshold <N>         Similarity threshold 0.0-1.0 (default: 0.90)\n"
        "  -m, --min-tokens <N>        Minimum block size in tokens (default: 300)\n"
        "  -l, --limit <N>             Limit output to top N findings per category (default: unlimited)\n"
        "  --text-sensitivity <N>      Text sensitivity blend factor 0.0-1.0 (default: 0.3)\n"
        "  --diff-base <ref>           Git ref to diff against (enables diff mode for CI)\n"
        "  --diff-commits <sha,...>    Comma-separated commit SHAs (enables commit-diff mode)\n"
        "  --no-color                  Disable ANSI color output\n"
        "  --no-source                 Don't print source code snippets\n"
        "  --theme <dark|light|auto>   Color theme (default: auto)\n"
        "  -g, --glob <pattern>        Filename glob filter (may be repeated, e.g., -g '*.cpp' -g '*Ctrl*')\n"
        "  -x, --exclude <pattern>     Exclude files matching glob pattern against relative path\n"
        "                              (may be repeated, e.g., -x '*_test*' -x 'test/*')\n"
        "  --encoding <enc>            Input encoding: auto, utf8, windows-1252 (default: auto)\n"
        "  -s, --scope <scopes>        Comma-separated analysis scopes (default: all)\n"
        "                              Valid: inter-file, intra-file, inter-function,\n"
        "                                     intra-function, all\n"
        "  --reporter <spec>           Output reporter: console (default), json, json:file=<path>\n"
        "  --gitignore                 Respect .gitignore when scanning (default)\n"
        "  --no-gitignore              Include gitignored files in analysis\n"
        "  -p, --progress              Show progress bars during analysis\n"
        "  -v, --verbose               Show verbose diagnostics during scanning\n"
        "  --mcp                       Run as MCP server (JSON-RPC over stdio)\n"
        "  --cache                     Enable block extraction cache (default)\n"
        "  --no-cache                  Disable block extraction cache\n"
        "  --save-baseline <name>      Save analysis results as a named baseline\n"
        "  --baseline <name>           Compare against a saved baseline, show only new clones\n"
        "  -h, --help                  Show help\n"
        "  --version                   Show version\n"
        "  --show-examples             Show usage examples\n"
        "  --info                      Show system capabilities (threads, SIMD)";
    auto const formatted = dude::HelpFormatter::FormatHelp(helpText, useColor, theme);
    std::print(out, "{}\n", formatted);
}

/// @brief Prints categorized usage examples to stdout.
void PrintExamples(bool useColor, dude::ColorTheme theme)
{
    static constexpr auto examplesText = "Usage Examples for dude\n"
                                         "=======================\n"
                                         "\n"
                                         "Basic Usage\n"
                                         "-----------\n"
                                         "  # Scan a directory with default settings\n"
                                         "  dude /path/to/project\n"
                                         "\n"
                                         "  # Scan with progress bars\n"
                                         "  dude -p /path/to/project\n"
                                         "\n"
                                         "  # Scan with verbose output\n"
                                         "  dude -v /path/to/project\n"
                                         "\n"
                                         "Threshold & Sensitivity Tuning\n"
                                         "------------------------------\n"
                                         "  # High threshold: only near-identical clones\n"
                                         "  dude -t 0.95 /path/to/project\n"
                                         "\n"
                                         "  # Low threshold: find loose similarities\n"
                                         "  dude -t 0.60 /path/to/project\n"
                                         "\n"
                                         "  # Stricter identifier matching (higher text sensitivity)\n"
                                         "  dude --text-sensitivity 0.7 /path/to/project\n"
                                         "\n"
                                         "  # Ignore identifier names entirely (structural only)\n"
                                         "  dude --text-sensitivity 0.0 /path/to/project\n"
                                         "\n"
                                         "Scope Control\n"
                                         "-------------\n"
                                         "  # Only detect inter-file clones\n"
                                         "  dude -s inter-file /path/to/project\n"
                                         "\n"
                                         "  # Only detect intra-function copy-paste\n"
                                         "  dude -s intra-function /path/to/project\n"
                                         "\n"
                                         "  # Combine scopes\n"
                                         "  dude -s inter-file,intra-function /path/to/project\n"
                                         "\n"
                                         "File Filtering\n"
                                         "--------------\n"
                                         "  # Scan only C++ headers and source files\n"
                                         "  dude -g '*.hpp' -g '*.cpp' /path/to/project\n"
                                         "\n"
                                         "  # Scan only C# files\n"
                                         "  dude -g '*.cs' /path/to/project\n"
                                         "\n"
                                         "  # Glob-based filename filter\n"
                                         "  dude -g '*Controller*' /path/to/project\n"
                                         "\n"
                                         "  # Multiple glob patterns\n"
                                         "  dude -g '*Controller*' -g '*Service*' /path/to/project\n"
                                         "\n"
                                         "  # Include gitignored files in analysis\n"
                                         "  dude --no-gitignore /path/to/project\n"
                                         "\n"
                                         "Output Control\n"
                                         "--------------\n"
                                         "  # Disable colors (useful for piping or CI logs)\n"
                                         "  dude --no-color /path/to/project\n"
                                         "\n"
                                         "  # Suppress source code snippets\n"
                                         "  dude --no-source /path/to/project\n"
                                         "\n"
                                         "  # Machine-readable output (no color, no source)\n"
                                         "  dude --no-color --no-source /path/to/project\n"
                                         "\n"
                                         "  # Set color theme explicitly\n"
                                         "  dude --theme dark /path/to/project\n"
                                         "\n"
                                         "CI / Git Integration\n"
                                         "--------------------\n"
                                         "  # Diff mode: only check changed code vs. a branch\n"
                                         "  dude --diff-base origin/master /path/to/project\n"
                                         "\n"
                                         "  # Diff mode with strict threshold for CI gates\n"
                                         "  dude --diff-base origin/master -t 0.90 /path/to/project\n"
                                         "\n"
                                         "  # Check duplicates introduced by specific commits\n"
                                         "  dude --diff-commits abc123,def456 /path/to/project\n"
                                         "\n"
                                         "Combining Options\n"
                                         "-----------------\n"
                                         "  # Full CI pipeline: diff mode, strict threshold, machine-readable,\n"
                                         "  # inter-file scope only\n"
                                         "  dude --diff-base origin/main -t 0.90 --no-color \\\n"
                                         "      --no-source -s inter-file /path/to/project\n"
                                         "\n"
                                         "Caching & Baselines\n"
                                         "-------------------\n"
                                         "  # Run with block cache (default, speeds up repeat runs)\n"
                                         "  dude /path/to/project\n"
                                         "\n"
                                         "  # Disable caching for a fresh analysis\n"
                                         "  dude --no-cache /path/to/project\n"
                                         "\n"
                                         "  # Save current results as a named baseline\n"
                                         "  dude --save-baseline v1.0 /path/to/project\n"
                                         "\n"
                                         "  # Show only new clones compared to a baseline\n"
                                         "  dude --baseline v1.0 /path/to/project\n"
                                         "\n"
                                         "MCP Server Mode\n"
                                         "---------------\n"
                                         "  # Start the MCP server for use with AI coding assistants\n"
                                         "  dude --mcp\n"
                                         "\n"
                                         "  # Claude Code: add to .mcp.json in project root\n"
                                         "  {\n"
                                         "    \"mcpServers\": {\n"
                                         "      \"dude\": {\n"
                                         "        \"type\": \"stdio\",\n"
                                         "        \"command\": \"/path/to/dude\",\n"
                                         "        \"args\": [\"--mcp\"]\n"
                                         "      }\n"
                                         "    }\n"
                                         "  }\n"
                                         "\n"
                                         "  # Gemini CLI / Antigravity IDE: add to mcp_config.json\n"
                                         "  {\n"
                                         "    \"mcpServers\": {\n"
                                         "      \"dude\": {\n"
                                         "        \"command\": \"/path/to/dude\",\n"
                                         "        \"args\": [\"--mcp\"]\n"
                                         "      }\n"
                                         "    }\n"
                                         "  }";
    auto const formatted = dude::HelpFormatter::FormatExamples(examplesText, useColor, theme);
    std::print("{}\n", formatted);
}

#if defined(DUDE_X86)
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

#if defined(DUDE_X86)
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

#if DUDE_HAS_SIMD
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
auto ParseThemeOption(int argc, char* argv[], int& i) -> std::expected<dude::ColorTheme, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::string("Missing value for --theme"));
    auto const val = std::string_view(argv[i]);
    if (val == "dark")
        return dude::ColorTheme::Dark;
    if (val == "light")
        return dude::ColorTheme::Light;
    if (val == "auto")
        return dude::ColorTheme::Auto;
    return std::unexpected(std::format("Unknown theme: {}", val));
}

/// @brief Parses the --encoding option value into an InputEncoding enum.
/// @param argc Total argument count.
/// @param argv Argument vector.
/// @param i Current argument index (advanced past the value on success).
/// @return The parsed InputEncoding value, or an error string.
auto ParseEncodingOption(int argc, char* argv[], int& i) -> std::expected<dude::InputEncoding, std::string>
{
    if (++i >= argc)
        return std::unexpected(std::string("Missing value for --encoding"));
    auto const encResult = dude::ParseEncodingName(argv[i]);
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
    if (arg == "-l" || arg == "--limit")
        return ParseSizeOption(argc, argv, i, "--limit")
            .transform(
                [&](size_t v) -> CliOptions
                {
                    opts.limit = v;
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
    if (arg == "--diff-commits")
        return ParseStringOption(argc, argv, i, "--diff-commits")
            .transform(
                [&](std::string const& v) -> CliOptions
                {
                    std::istringstream stream(v);
                    std::string sha;
                    while (std::getline(stream, sha, ','))
                    {
                        if (!sha.empty())
                            opts.diffCommits.push_back(sha);
                    }
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
                [&](dude::ColorTheme v) -> CliOptions
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
    if (arg == "-x" || arg == "--exclude")
        return ParseStringOption(argc, argv, i, "--exclude")
            .transform(
                [&](std::string v) -> CliOptions
                {
                    opts.excludePatterns.push_back(std::move(v));
                    return opts;
                });
    if (arg == "--encoding")
        return ParseEncodingOption(argc, argv, i)
            .transform(
                [&](dude::InputEncoding v) -> CliOptions
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
                    auto const scopeResult = dude::ParseAnalysisScope(v);
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
    if (arg == "--cache")
    {
        opts.enableCache = true;
        return std::nullopt;
    }
    if (arg == "--no-cache")
    {
        opts.enableCache = false;
        return std::nullopt;
    }
    if (arg == "--save-baseline")
        return ParseStringOption(argc, argv, i, "--save-baseline")
            .transform(
                [&](std::string v) -> CliOptions
                {
                    opts.saveBaseline = std::move(v);
                    return opts;
                });
    if (arg == "--baseline")
        return ParseStringOption(argc, argv, i, "--baseline")
            .transform(
                [&](std::string v) -> CliOptions
                {
                    opts.compareBaseline = std::move(v);
                    return opts;
                });
    if (arg == "-p" || arg == "--progress")
    {
        opts.showProgress = true;
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
            if (opts.showHelp || opts.showVersion || opts.showExamples || opts.showInfo || opts.mcpMode)
                return opts;
        }
    }

    if (!opts.showHelp && !opts.showVersion && !opts.showExamples && !opts.showInfo && !opts.mcpMode &&
        opts.directory.empty())
        return std::unexpected("No directory specified");

    if (!opts.diffBase.empty() && !opts.diffCommits.empty())
        return std::unexpected("--diff-base and --diff-commits are mutually exclusive");

    return opts;
}

// ---------------------------------------------------------------------------
// main() pipeline stage helpers
// ---------------------------------------------------------------------------

/// @brief Runs git diff setup when diff mode is active (step 0).
///
/// Executes git diff against the specified base ref (or commit SHAs) and parses
/// the output into structured diff data. Prints progress and results to stderr.
///
/// @param opts The parsed CLI options.
/// @return The parsed diff result on success, or an exit code on failure.
///         Returns an empty DiffResult if diff mode is not active.
auto RunDiffSetup(CliOptions const& opts) -> std::expected<dude::DiffResult, int>
{
    if (opts.diffBase.empty() && opts.diffCommits.empty())
        return dude::DiffResult{};

    auto const projectRoot = std::filesystem::weakly_canonical(opts.directory);

    // Obtain raw diff output from either --diff-base or --diff-commits.
    std::expected<std::string, git::GitDiffError> diffOutput;
    if (!opts.diffCommits.empty())
    {
        if (opts.verbose)
            std::println(stderr, "Running git show for {} commits...", opts.diffCommits.size());
        diffOutput = git::GitDiffParser::RunGitShow(projectRoot, opts.diffCommits);
    }
    else
    {
        if (opts.verbose)
            std::println(stderr, "Running git diff against {}...", opts.diffBase);
        diffOutput = git::GitDiffParser::RunGitDiff(projectRoot, opts.diffBase);
    }

    if (!diffOutput)
    {
        std::println(stderr, "Error: {}", diffOutput.error().message);
        return std::unexpected(2);
    }

    auto const extensions =
        opts.globPatterns.empty() ? dude::FileScanner::DefaultExtensions() : std::vector<std::string>{};
    auto diffResult = git::GitDiffParser::ParseDiffOutput(*diffOutput, extensions);

    // Post-filter by glob patterns when active
    if (!opts.globPatterns.empty())
    {
        std::erase_if(diffResult,
                      [&](auto const& fc)
                      {
                          auto const filename = fc.filePath.filename().string();
                          return !std::ranges::any_of(opts.globPatterns, [&filename](std::string const& pattern)
                                                      { return dude::GlobMatch(pattern, filename); });
                      });
    }

    // Post-filter by exclude patterns (matched against relative path)
    if (!opts.excludePatterns.empty())
    {
        std::erase_if(diffResult,
                      [&](auto const& fc)
                      {
                          auto const relative = fc.filePath.string();
                          return std::ranges::any_of(opts.excludePatterns, [&relative](std::string const& pattern)
                                                     { return dude::GlobMatch(pattern, relative); });
                      });
    }

    if (diffResult.empty())
    {
        if (!opts.diffCommits.empty())
            std::println("No matching files changed in the specified commits.");
        else
            std::println("No C++ files changed relative to {}.", opts.diffBase);
        return std::unexpected(0);
    }

    if (!opts.diffCommits.empty())
        std::println(stderr, "Checking for duplication in changes from {} commits...", opts.diffCommits.size());
    else
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
auto ScanFiles(CliOptions const& opts, dude::PerformanceTiming& timing)
    -> std::expected<std::vector<std::filesystem::path>, int>
{
    using Clock = std::chrono::steady_clock;

    if (opts.verbose)
        std::println(stderr, "Scanning directory: {}", opts.directory.string());

    auto const scanStart = Clock::now();
    // When glob patterns are active, skip extension filtering (pass empty → accept all)
    auto const extensions =
        opts.globPatterns.empty() ? dude::FileScanner::DefaultExtensions() : std::vector<std::string>{};

    // Build an optional gitignore-aware filter.
    auto const gitFilter =
        opts.respectGitignore ? git::GitFileFilter::CreateFilter(opts.directory, opts.verbose) : std::nullopt;

    // Build an optional glob-based filename filter.
    auto const globFilter = opts.globPatterns.empty()
                                ? std::optional<dude::FileFilter>(std::nullopt)
                                : std::optional<dude::FileFilter>(
                                      [patterns = opts.globPatterns](std::filesystem::path const& path) -> bool
                                      {
                                          auto const filename = path.filename().string();
                                          return std::ranges::any_of(patterns, [&filename](std::string const& pattern)
                                                                     { return dude::GlobMatch(pattern, filename); });
                                      });

    // Build an optional exclude filter (matches against relative path from scan directory).
    auto const canonicalDir = std::filesystem::weakly_canonical(opts.directory);
    auto const excludeFilter =
        opts.excludePatterns.empty()
            ? std::optional<dude::FileFilter>(std::nullopt)
            : std::optional<dude::FileFilter>(
                  [patterns = opts.excludePatterns, canonicalDir](std::filesystem::path const& path) -> bool
                  {
                      auto const relative = std::filesystem::relative(path, canonicalDir).string();
                      return !std::ranges::any_of(patterns, [&relative](std::string const& pattern)
                                                  { return dude::GlobMatch(pattern, relative); });
                  });

    // Compose all filters into a single predicate.
    auto const composedFilter =
        (gitFilter || globFilter || excludeFilter)
            ? std::optional<dude::FileFilter>(
                  [gitFilter, globFilter, excludeFilter](std::filesystem::path const& path) -> bool
                  {
                      if (gitFilter && !(*gitFilter)(path))
                          return false;
                      if (globFilter && !(*globFilter)(path))
                          return false;
                      if (excludeFilter && !(*excludeFilter)(path))
                          return false;
                      return true;
                  })
            : std::optional<dude::FileFilter>(std::nullopt);

    auto const filesResult = dude::FileScanner::Scan(opts.directory, extensions, composedFilter);
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

/// @brief Tries to load cached blocks for a file, patching fileIndex values.
/// @return true if blocks were loaded from cache, false otherwise.
auto TryLoadCachedBlocks(dude::BlockCache& cache, std::filesystem::path const& filePath, dude::Language const& language,
                         CliOptions const& opts, uint32_t fileIndex, std::vector<dude::CodeBlock>& allBlocks,
                         std::vector<size_t>& blockToFileIndex) -> bool
{
    auto const mappedFile = dude::MappedFile::Open(filePath);
    if (!mappedFile)
        return false;

    auto const contentHash = dude::ComputeContentHash(mappedFile->View());
    auto const cached = cache.Lookup(contentHash, language.Name(), opts.minTokens, opts.textSensitivity);
    if (!cached)
        return false;

    auto const fi = static_cast<size_t>(fileIndex);
    for (auto block : *cached)
    {
        block.sourceRange.start.fileIndex = fileIndex;
        block.sourceRange.end.fileIndex = fileIndex;
        blockToFileIndex.push_back(fi);
        allBlocks.push_back(std::move(block));
    }
    return true;
}

/// @brief Stores extracted blocks in the cache.
void StoreBlocksInCache(dude::BlockCache& cache, std::filesystem::path const& filePath, dude::Language const& language,
                        CliOptions const& opts, std::vector<dude::CodeBlock> const& blocks)
{
    auto const mappedFile = dude::MappedFile::Open(filePath);
    if (mappedFile)
        cache.Store(dude::ComputeContentHash(mappedFile->View()), language.Name(), opts.minTokens, opts.textSensitivity,
                    blocks);
}

/// @brief Tokenizes all source files and extracts code blocks in a streaming per-file pipeline (steps 2+3).
///
/// Each file is tokenized, normalized, and block-extracted in a single pass.
/// Tokens are released after each file, keeping peak memory proportional to
/// the largest single file rather than the entire codebase.
///
/// When a block cache is provided, unchanged files (by content hash) are loaded
/// from cache instead of being re-tokenized, significantly speeding up repeat runs.
///
/// @param files The source file paths to process.
/// @param opts The parsed CLI options (for encoding, minTokens, textSensitivity, verbosity).
/// @param timing Performance timing struct to record tokenization and normalization duration.
/// @param progressBar Optional progress bar to report per-file progress.
/// @param cache Optional block cache for skipping tokenization of unchanged files.
/// @return A tuple of (all extracted code blocks, block-to-file-index mapping, language pointers per file).
auto TokenizeAndExtractBlocks(std::vector<std::filesystem::path> const& files, CliOptions const& opts,
                              dude::PerformanceTiming& timing, dude::ProgressBar* progressBar, dude::BlockCache* cache)
    -> std::tuple<std::vector<dude::CodeBlock>, std::vector<size_t>, std::vector<dude::Language const*>>
{
    using Clock = std::chrono::steady_clock;

    auto const startTime = Clock::now();
    auto const numFiles = files.size();

    auto const& registry = dude::LanguageRegistry::Instance();
    std::vector<dude::Language const*> fileLanguages(numFiles, nullptr);
    for (size_t fi = 0; fi < numFiles; ++fi)
        fileLanguages[fi] = registry.FindByPath(files[fi]);

    dude::TokenNormalizer normalizer;
    dude::CodeBlockExtractorConfig const blockConfig{.minTokens = opts.minTokens};
    auto const useTextSensitivity = opts.textSensitivity > 0.0;

    std::vector<dude::CodeBlock> allBlocks;
    std::vector<size_t> blockToFileIndex;
    size_t cacheHits = 0;

    auto const logVerbose = [&](std::string const& msg)
    {
        if (!opts.verbose)
            return;
        if (progressBar && progressBar->IsActive())
            progressBar->Log(msg);
        else
            std::println(stderr, "{}", msg);
    };

    for (auto const fi : std::views::iota(size_t{0}, numFiles))
    {
        auto const* language = fileLanguages[fi];
        if (!language)
        {
            logVerbose(std::format("Warning: No language support for {}", files[fi].string()));
            if (progressBar)
                progressBar->Tick();
            continue;
        }

        auto const fileIndex = static_cast<uint32_t>(fi);

        // Try loading from cache first.
        if (cache && TryLoadCachedBlocks(*cache, files[fi], *language, opts, fileIndex, allBlocks, blockToFileIndex))
        {
            ++cacheHits;
            logVerbose(std::format("Cache hit ({}): {}", language->Name(), files[fi].string()));
            if (progressBar)
                progressBar->Tick();
            continue;
        }

        // Tokenize this single file
        auto tokensResult = language->TokenizeFile(files[fi], fileIndex, opts.encoding);
        if (!tokensResult || tokensResult->empty())
        {
            logVerbose(std::format("Warning: Failed to tokenize {}", files[fi].string()));
            if (progressBar)
                progressBar->Tick();
            continue;
        }

        auto& tokens = *tokensResult;
        logVerbose(std::format("Tokenized ({}): {}", language->Name(), files[fi].string()));

        // Normalize and extract blocks, then release tokens
        auto normalized = normalizer.Normalize(tokens, language);
        auto textPreserving = useTextSensitivity ? normalizer.NormalizeTextPreserving(tokens, language)
                                                 : std::vector<dude::NormalizedToken>{};
        auto blocks = language->ExtractBlocks(tokens, normalized, textPreserving, blockConfig);

        if (!blocks.empty())
            logVerbose(std::format("  {} blocks from {}", blocks.size(), files[fi].string()));

        // Store in cache before moving blocks.
        if (cache)
            StoreBlocksInCache(*cache, files[fi], *language, opts, blocks);

        for (auto& block : blocks)
        {
            blockToFileIndex.push_back(fi);
            allBlocks.push_back(std::move(block));
        }

        // tokens released here when tokensResult goes out of scope at end of iteration

        if (progressBar)
            progressBar->Tick();
    }

    timing.tokenizing = Clock::now() - startTime;
    logVerbose(std::format("Extracted {} code blocks total ({} from cache)", allBlocks.size(), cacheHits));

    return {std::move(allBlocks), std::move(blockToFileIndex), std::move(fileLanguages)};
}

/// @brief Re-tokenizes only the files that participate in detected clone results.
///
/// After clone detection, only a subset of files appear in the results.
/// This function re-tokenizes only those files from disk for use by reporters
/// that need access to original token text (e.g., syntax highlighting).
///
/// @param groups The detected clone groups.
/// @param intraResults The detected intra-function clone results.
/// @param blockToFileIndex Mapping from block index to file index.
/// @param files The source file paths.
/// @param fileLanguages Language pointers per file.
/// @param encoding Input encoding setting.
/// @return Sparse token vector (only participating file indices are populated).
auto RetokenizeParticipatingFiles(std::vector<dude::CloneGroup> const& groups,
                                  std::vector<dude::IntraCloneResult> const& intraResults,
                                  std::vector<size_t> const& blockToFileIndex,
                                  std::vector<std::filesystem::path> const& files,
                                  std::vector<dude::Language const*> const& fileLanguages, dude::InputEncoding encoding)
    -> std::vector<std::vector<dude::Token>>
{
    // Identify files that appear in results
    std::unordered_set<size_t> participatingFiles;
    for (auto const& group : groups)
        for (auto const idx : group.blockIndices)
            participatingFiles.insert(blockToFileIndex[idx]);
    for (auto const& result : intraResults)
        participatingFiles.insert(blockToFileIndex[result.blockIndex]);

    // Re-tokenize only those files
    std::vector<std::vector<dude::Token>> allTokens(files.size());
    for (auto const fi : participatingFiles)
    {
        auto const* language = fileLanguages[fi];
        if (!language)
            continue;
        auto const fileIndex = static_cast<uint32_t>(fi);
        auto tokensResult = language->TokenizeFile(files[fi], fileIndex, encoding);
        if (tokensResult)
            allTokens[fi] = std::move(*tokensResult);
    }

    return allTokens;
}

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char* argv[])
{
    auto const optsResult = ParseArgs(argc, argv);
    if (!optsResult)
    {
        std::println(stderr, "Error: {}\n", optsResult.error());
        PrintUsage(stderr, false, dude::ColorTheme::Auto);
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
        std::println("dude {}", versionString);
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
            .name = "dude",
            .version = versionString,
            .title = "Code Duplication Analysis Tool",
            .description = "Code duplication detection and analysis tool",
            .websiteUrl = {},
        });
        mcp::RegisterDudeTools(server, session);
        return server.Run();
    }

    InstallSignalHandlers();

    auto const diffMode = !opts.diffBase.empty() || !opts.diffCommits.empty();

    // Step 0: Parse git diff if in diff mode.
    auto const diffSetupResult = RunDiffSetup(opts);
    if (!diffSetupResult)
        return diffSetupResult.error();
    auto const& diffResult = *diffSetupResult;

    if (auto const interrupted = CheckInterrupted())
        return *interrupted;

    dude::PerformanceTiming timing;

    // Step 1: Scan files
    auto const filesResult = ScanFiles(opts, timing);
    if (!filesResult)
        return filesResult.error();
    auto const& files = *filesResult;

    if (auto const interrupted = CheckInterrupted())
        return *interrupted;

    // Compute total progress stages based on active scope.
    size_t totalStages = 1; // Processing (tokenize + extract, fused)
    if (dude::HasInterFunctionScope(opts.scope))
        totalStages += 4; // Fingerprinting, Gather Candidates, Collecting, Detecting
    if (dude::HasScope(opts.scope, dude::AnalysisScope::IntraFunction))
        totalStages += 1; // Intra-detect
    size_t currentStage = 0;

    // Set up block cache if enabled.
    auto const projectRoot = std::filesystem::weakly_canonical(opts.directory);
    auto const cachePath = projectRoot / ".dude-cache" / "blocks.json";
    std::optional<dude::BlockCache> blockCache;
    if (opts.enableCache)
    {
        blockCache.emplace(cachePath);
        auto const loadResult = blockCache->Load();
        if (!loadResult && opts.verbose)
            std::println(stderr, "Warning: {}", loadResult.error().message);
        else if (opts.verbose)
            std::println(stderr, "Loaded block cache ({} entries)", blockCache->Size());
    }

    // Steps 2+3: Streaming tokenize + normalize + extract blocks (per file)
    auto processBar = opts.showProgress ? std::make_optional<dude::ProgressBar>("Processing", files.size(), stderr,
                                                                                false, ++currentStage, totalStages)
                                        : std::nullopt;
    if (processBar)
        processBar->Start();
    auto [allBlocks, blockToFileIndex, fileLanguages] = TokenizeAndExtractBlocks(
        files, opts, timing, processBar ? &*processBar : nullptr, blockCache ? &*blockCache : nullptr);
    if (processBar)
        processBar->Finish(false);

    // Save updated block cache.
    if (blockCache)
    {
        auto const saveResult = blockCache->Save();
        if (!saveResult && opts.verbose)
            std::println(stderr, "Warning: Failed to save block cache: {}", saveResult.error().message);
        else if (opts.verbose)
            std::println(stderr, "Saved block cache ({} entries)", blockCache->Size());
    }

    if (auto const interrupted = CheckInterrupted())
        return *interrupted;

    // Step 4: Detect clones
    using Clock = std::chrono::steady_clock;

    std::vector<dude::CloneGroup> groups;
    if (dude::HasInterFunctionScope(opts.scope))
    {
        auto const detectStart = Clock::now();
        dude::CloneDetector detector({
            .similarityThreshold = opts.threshold,
            .minTokens = opts.minTokens,
            .textSensitivity = opts.textSensitivity,
        });

        auto fingerprintBar = opts.showProgress
                                  ? std::make_optional<dude::ProgressBar>("Fingerprinting", allBlocks.size(), stderr,
                                                                          false, ++currentStage, totalStages)
                                  : std::nullopt;
        if (fingerprintBar)
            fingerprintBar->Start();

        auto candidateBar = opts.showProgress
                                ? std::make_optional<dude::ProgressBar>("Gather Candidates", size_t{0}, stderr, false,
                                                                        ++currentStage, totalStages)
                                : std::nullopt;

        auto collectBar = opts.showProgress ? std::make_optional<dude::ProgressBar>("Collecting", size_t{0}, stderr,
                                                                                    false, ++currentStage, totalStages)
                                            : std::nullopt;

        auto detectBar = opts.showProgress ? std::make_optional<dude::ProgressBar>("Detecting", size_t{0}, stderr,
                                                                                   false, ++currentStage, totalStages)
                                           : std::nullopt;

        groups = detector.Detect(
            allBlocks, detectBar ? detectBar->MakeAbsoluteCallback() : dude::ProgressCallback{},
            [&](size_t current, size_t total)
            {
                if (fingerprintBar)
                    fingerprintBar->Update(current, total);
                if (current >= total)
                {
                    if (fingerprintBar)
                    {
                        fingerprintBar->Finish(false);
                        fingerprintBar.reset();
                    }
                    if (candidateBar)
                        candidateBar->Start();
                }
            },
            [&](size_t current, size_t total)
            {
                if (candidateBar)
                    candidateBar->Update(current, total);
                if (current >= total)
                {
                    if (candidateBar)
                    {
                        candidateBar->Finish(false);
                        candidateBar.reset();
                    }
                    if (collectBar)
                        collectBar->Start();
                }
            },
            [&](size_t current, size_t total)
            {
                if (collectBar)
                    collectBar->Update(current, total);
                if (current >= total)
                {
                    if (collectBar)
                    {
                        collectBar->Finish(false);
                        collectBar.reset();
                    }
                    if (detectBar)
                        detectBar->Start();
                }
            });

        if (fingerprintBar)
            fingerprintBar->Finish(false); // safety: edge case with < 2 blocks
        if (candidateBar)
            candidateBar->Finish(false); // safety: no fingerprints edge case
        if (collectBar)
            collectBar->Finish(false); // safety: no candidates edge case
        if (detectBar)
            detectBar->Finish(false);
        timing.cloneDetection = Clock::now() - detectStart;

        // Apply scope-based filtering (inter-file vs intra-file).
        groups = dude::ScopeFilter::FilterCloneGroups(groups, blockToFileIndex, opts.scope);

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

    if (auto const interrupted = CheckInterrupted())
        return *interrupted;

    // Step 4b: Detect intra-function clones
    std::vector<dude::IntraCloneResult> intraResults;
    if (dude::HasScope(opts.scope, dude::AnalysisScope::IntraFunction))
    {
        if (opts.verbose)
            std::println(stderr, "Detecting intra-function clones...");

        auto const intraStart = Clock::now();
        dude::IntraFunctionDetector intraDetector({
            .minRegionTokens = opts.minTokens,
            .similarityThreshold = opts.threshold,
            .textSensitivity = opts.textSensitivity,
        });

        auto intraBar = opts.showProgress
                            ? std::make_optional<dude::ProgressBar>("Intra-detect", allBlocks.size(), stderr, false,
                                                                    ++currentStage, totalStages)
                            : std::nullopt;
        if (intraBar)
            intraBar->Start();
        intraResults =
            intraDetector.Detect(allBlocks, intraBar ? intraBar->MakeAbsoluteCallback() : dude::ProgressCallback{});
        if (intraBar)
            intraBar->Finish(false);
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

    if (auto const interrupted = CheckInterrupted())
        return *interrupted;

    // Step 4c: Filter results if in diff mode.
    if (diffMode)
    {
        auto const changedBlocks = dude::DiffFilter::FindChangedBlocks(allBlocks, diffResult, projectRoot, files);

        if (opts.verbose)
            std::println(stderr, "Found {} code blocks overlapping with changed lines", changedBlocks.size());

        groups = dude::DiffFilter::FilterCloneGroups(groups, changedBlocks);
        intraResults = dude::DiffFilter::FilterIntraResults(intraResults, changedBlocks);
    }

    // Step 4d: Save baseline if requested.
    if (!opts.saveBaseline.empty())
    {
        auto baselineName = opts.saveBaseline;
        if (baselineName == "auto")
        {
            auto const headSha = git::GitDiffParser::GetHeadSha(projectRoot);
            baselineName = headSha.value_or("unknown");
        }

        dude::BaselineStore store(projectRoot / ".dude-cache" / "baselines");
        auto const saveResult = store.Save(baselineName, groups, intraResults, allBlocks, files, projectRoot);
        if (saveResult)
            std::println(stderr, "Saved baseline '{}'", baselineName);
        else
            std::println(stderr, "Warning: Failed to save baseline: {}", saveResult.error().message);
    }

    // Step 4e: Filter to new-only if baseline comparison requested.
    if (!opts.compareBaseline.empty())
    {
        dude::BaselineStore store(projectRoot / ".dude-cache" / "baselines");
        auto const baseline = store.Load(opts.compareBaseline);
        if (baseline)
        {
            groups = dude::BaselineStore::FindNewCloneGroups(groups, *baseline, allBlocks, files, projectRoot);
            intraResults =
                dude::BaselineStore::FindNewIntraClones(intraResults, *baseline, allBlocks, files, projectRoot);

            if (opts.verbose)
            {
                std::println(stderr, "Filtered against baseline '{}': {} new clone groups, {} new intra-clone results",
                             opts.compareBaseline, groups.size(), intraResults.size());
            }
        }
        else
        {
            std::println(stderr, "Warning: Baseline '{}' not found, showing all results", opts.compareBaseline);
        }
    }

    // Step 4f: Apply --limit to truncate results to top N per category.
    if (opts.limit > 0)
    {
        if (groups.size() > opts.limit)
            groups.resize(opts.limit);
        if (intraResults.size() > opts.limit)
            intraResults.resize(opts.limit);
    }

    // Step 5: Re-tokenize participating files for reporter output and report results.
    auto const allTokens =
        RetokenizeParticipatingFiles(groups, intraResults, blockToFileIndex, files, fileLanguages, opts.encoding);

    if (opts.verbose)
    {
        size_t participatingCount = 0;
        for (auto const& tokens : allTokens)
            if (!tokens.empty())
                ++participatingCount;
        std::println(stderr, "Re-tokenized {} participating files for reporting", participatingCount);
    }
    dude::ReporterConfig const consoleConfig{
        .useColor = opts.useColor,
        .showSourceCode = opts.showSource,
        .theme = opts.theme,
    };

    auto reporterResult = dude::CreateReporter(opts.reporterSpec, consoleConfig);
    if (!reporterResult)
    {
        std::println(stderr, "Error: {}", reporterResult.error().message);
        return 2;
    }
    auto const& reporter = *reporterResult;

    auto const specResult = dude::ParseReporterSpec(opts.reporterSpec);

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
