// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/AnalysisScope.hpp>
#include <dude/Api.hpp>
#include <dude/CloneDetector.hpp>
#include <dude/CodeBlock.hpp>
#include <dude/IntraFunctionDetector.hpp>
#include <dude/SyntaxHighlighter.hpp>
#include <dude/Token.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <vector>

namespace dude
{

/// @brief Performance timing data for each phase of the clone detection pipeline.
struct PerformanceTiming
{
    using Duration = std::chrono::steady_clock::duration;

    Duration scanning{};       ///< Time spent scanning the filesystem for source files.
    Duration tokenizing{};     ///< Time spent tokenizing all source files.
    Duration normalizing{};    ///< Time spent normalizing tokens and extracting code blocks.
    Duration cloneDetection{}; ///< Time spent detecting inter-function clones.
    Duration intraDetection{}; ///< Time spent detecting intra-function clones.

    /// @brief Returns the total wall-clock time across all phases.
    [[nodiscard]] constexpr auto Total() const -> Duration
    {
        return scanning + tokenizing + normalizing + cloneDetection + intraDetection;
    }
};

/// @brief Data for the summary report.
struct SummaryData
{
    size_t totalFiles = 0;           ///< Number of files scanned.
    size_t totalBlocks = 0;          ///< Number of code blocks extracted.
    size_t totalGroups = 0;          ///< Number of clone groups found.
    size_t totalIntraPairs = 0;      ///< Number of intra-function clone pairs found.
    size_t totalDuplicatedLines = 0; ///< Total source lines involved in inter-function clone groups.
    size_t totalFunctions = 0;       ///< Number of functions involved in inter-function clone groups.
    size_t totalIntraFunctions = 0;  ///< Number of functions containing intra-function clones.
    std::optional<PerformanceTiming> timing = std::nullopt;  ///< Optional performance timing data.
    std::optional<AnalysisScope> activeScope = std::nullopt; ///< Active scope (displayed when not All).
};

/// @brief Configuration for report output.
struct ReporterConfig
{
    bool useColor = true;                ///< Whether to use ANSI color codes.
    bool showSourceCode = true;          ///< Whether to show syntax-highlighted source snippets.
    bool highlightDifferences = true;    ///< Whether to background-highlight differing tokens between clones.
    ColorTheme theme = ColorTheme::Auto; ///< Color theme to use.
};

/// @brief Abstract interface for formatting and outputting clone detection results.
///
/// Each reporter implementation accumulates output internally and provides
/// Render() and WriteTo() for retrieving the final result.
/// Concrete implementations include ConsoleReporter (human-readable text)
/// and JsonReporter (structured JSON output).
class DUDE_API Reporter
{
public:
    virtual ~Reporter() = default;
    Reporter() = default;
    Reporter(Reporter const&) = delete;
    Reporter(Reporter&&) = delete;
    auto operator=(Reporter const&) -> Reporter& = delete;
    auto operator=(Reporter&&) -> Reporter& = delete;

    /// @brief Reports clone groups with details for each block.
    /// @param groups The clone groups to report.
    /// @param blocks The code blocks referenced by the groups.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    /// @param files The file path vector for resolving file indices to paths.
    virtual void Report(std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                        std::vector<std::vector<Token>> const& allTokens, std::vector<size_t> const& blockToFileIndex,
                        std::span<std::filesystem::path const> files) = 0;

    /// @brief Reports intra-function clone results.
    /// @param results The intra-function clone results to report.
    /// @param blocks The code blocks referenced by the results.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    /// @param files The file path vector for resolving file indices to paths.
    virtual void ReportIntraClones(std::vector<IntraCloneResult> const& results, std::vector<CodeBlock> const& blocks,
                                   std::vector<std::vector<Token>> const& allTokens,
                                   std::vector<size_t> const& blockToFileIndex,
                                   std::span<std::filesystem::path const> files) = 0;

    /// @brief Appends a summary of the scan results, optionally including performance timing.
    /// @param summary The summary data to report.
    virtual void ReportSummary(SummaryData const& summary) = 0;

    /// @brief Returns the accumulated output as a string.
    [[nodiscard]] virtual auto Render() const -> std::string = 0;

    /// @brief Writes the accumulated output to the given stream.
    /// @param out The output stream to write to.
    virtual void WriteTo(std::ostream& out) const = 0;
};

} // namespace dude
