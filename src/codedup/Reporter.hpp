// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/IntraFunctionDetector.hpp>
#include <codedup/SyntaxHighlighter.hpp>
#include <codedup/Token.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace codedup
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
    [[nodiscard]] constexpr auto total() const -> Duration
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
    std::optional<PerformanceTiming> timing = std::nullopt; ///< Optional performance timing data.
};

/// @brief Configuration for report output.
struct ReporterConfig
{
    bool useColor = true;                ///< Whether to use ANSI color codes.
    bool showSourceCode = true;          ///< Whether to show syntax-highlighted source snippets.
    bool highlightDifferences = true;    ///< Whether to background-highlight differing tokens between clones.
    ColorTheme theme = ColorTheme::Auto; ///< Color theme to use.
};

/// @brief Formats and outputs clone detection results.
///
/// For each clone group, prints a header with group number, block count, and
/// average similarity, followed by details for each block including file path,
/// line range, function name, and optionally syntax-highlighted source code.
class CODEDUP_API Reporter
{
public:
    explicit Reporter(ReporterConfig config = {}) : _config(config) {}

    /// @brief Reports clone groups with details for each block.
    /// @param out Output string to append to.
    /// @param groups The clone groups to report.
    /// @param blocks The code blocks referenced by the groups.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    void report(std::string& out, std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                std::vector<std::vector<Token>> const& allTokens, std::vector<size_t> const& blockToFileIndex) const;

    /// @brief Reports intra-function clone results.
    /// @param out Output string to append to.
    /// @param results The intra-function clone results to report.
    /// @param blocks The code blocks referenced by the results.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    void reportIntraClones(std::string& out, std::vector<IntraCloneResult> const& results,
                           std::vector<CodeBlock> const& blocks, std::vector<std::vector<Token>> const& allTokens,
                           std::vector<size_t> const& blockToFileIndex) const;

    /// @brief Appends a summary of the scan results, optionally including performance timing.
    /// @param out Output string to append to.
    /// @param summary The summary data to report.
    void reportSummary(std::string& out, SummaryData const& summary) const;

private:
    ReporterConfig _config;

    /// @brief Appends a syntax-highlighted source snippet for a code block.
    /// @param highlightTokens Optional set of original token indices to background-highlight as differing.
    void printSourceSnippet(std::string& out, std::vector<Token> const& tokens, size_t tokenStart, size_t tokenEnd,
                            std::unordered_set<size_t> const& highlightTokens = {}) const;
};

} // namespace codedup
