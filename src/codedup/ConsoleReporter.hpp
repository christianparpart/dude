// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/Reporter.hpp>

#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace codedup
{

/// @brief Console reporter that formats clone detection results as human-readable text.
///
/// Produces ANSI-colored (optional) textual output with clone group headers,
/// block locations, syntax-highlighted source snippets, and a summary section.
/// This is the default reporter used by the CLI.
class CODEDUP_API ConsoleReporter final : public Reporter
{
public:
    /// @brief Constructs a ConsoleReporter with the given configuration.
    /// @param config Reporter configuration for color, source display, and theme settings.
    explicit ConsoleReporter(ReporterConfig config = {}) : _config(config) {}

    ~ConsoleReporter() override = default;
    ConsoleReporter(ConsoleReporter const&) = delete;
    ConsoleReporter(ConsoleReporter&&) = delete;
    auto operator=(ConsoleReporter const&) -> ConsoleReporter& = delete;
    auto operator=(ConsoleReporter&&) -> ConsoleReporter& = delete;

    /// @brief Reports clone groups with details for each block.
    /// @param groups The clone groups to report.
    /// @param blocks The code blocks referenced by the groups.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    /// @param files The file path vector for resolving file indices to paths.
    void Report(std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                std::vector<std::vector<Token>> const& allTokens, std::vector<size_t> const& blockToFileIndex,
                std::span<std::filesystem::path const> files) override;

    /// @brief Reports intra-function clone results.
    /// @param results The intra-function clone results to report.
    /// @param blocks The code blocks referenced by the results.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    /// @param files The file path vector for resolving file indices to paths.
    void ReportIntraClones(std::vector<IntraCloneResult> const& results, std::vector<CodeBlock> const& blocks,
                           std::vector<std::vector<Token>> const& allTokens,
                           std::vector<size_t> const& blockToFileIndex,
                           std::span<std::filesystem::path const> files) override;

    /// @brief Appends a summary of the scan results, optionally including performance timing.
    /// @param summary The summary data to report.
    void ReportSummary(SummaryData const& summary) override;

    /// @brief Returns the accumulated output as a string.
    [[nodiscard]] auto Render() const -> std::string override;

    /// @brief Writes the accumulated output to the given stream.
    /// @param out The output stream to write to.
    void WriteTo(std::ostream& out) const override;

private:
    ReporterConfig _config;
    std::string _output;

    /// @brief Appends a syntax-highlighted source snippet for a code block.
    /// @param highlightTokens Optional set of original token indices to background-highlight as differing.
    void PrintSourceSnippet(std::string& out, std::vector<Token> const& tokens, size_t tokenStart, size_t tokenEnd,
                            std::unordered_set<size_t> const& highlightTokens = {}) const;
};

} // namespace codedup
