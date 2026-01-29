// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <nlohmann/json.hpp>

#include <codedup/Api.hpp>
#include <codedup/Reporter.hpp>

#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <vector>

namespace codedup
{

/// @brief JSON reporter that formats clone detection results as structured JSON.
///
/// Accumulates clone groups, intra-function clones, and summary data internally,
/// then renders them as a single JSON document with `cloneGroups`, `intraClones`,
/// and `summary` top-level keys.
class CODEDUP_API JsonReporter final : public Reporter
{
public:
    JsonReporter() = default;
    ~JsonReporter() override = default;
    JsonReporter(JsonReporter const&) = delete;
    JsonReporter(JsonReporter&&) = delete;
    auto operator=(JsonReporter const&) -> JsonReporter& = delete;
    auto operator=(JsonReporter&&) -> JsonReporter& = delete;

    /// @brief Reports clone groups as structured JSON.
    /// @param groups The clone groups to report.
    /// @param blocks The code blocks referenced by the groups.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    /// @param files The file path vector for resolving file indices to paths.
    void Report(std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                std::vector<std::vector<Token>> const& allTokens, std::vector<size_t> const& blockToFileIndex,
                std::span<std::filesystem::path const> files) override;

    /// @brief Reports intra-function clone results as structured JSON.
    /// @param results The intra-function clone results to report.
    /// @param blocks The code blocks referenced by the results.
    /// @param allTokens All tokens from all files (indexed by block's token range).
    /// @param blockToFileIndex Mapping from block index to file index in allTokens.
    /// @param files The file path vector for resolving file indices to paths.
    void ReportIntraClones(std::vector<IntraCloneResult> const& results, std::vector<CodeBlock> const& blocks,
                           std::vector<std::vector<Token>> const& allTokens,
                           std::vector<size_t> const& blockToFileIndex,
                           std::span<std::filesystem::path const> files) override;

    /// @brief Reports summary data as structured JSON.
    /// @param summary The summary data to report.
    void ReportSummary(SummaryData const& summary) override;

    /// @brief Renders the accumulated JSON as an indented string.
    [[nodiscard]] auto Render() const -> std::string override;

    /// @brief Writes the accumulated JSON to the given stream.
    /// @param out The output stream to write to.
    void WriteTo(std::ostream& out) const override;

private:
    nlohmann::json _cloneGroups = nlohmann::json::array();
    nlohmann::json _intraClones = nlohmann::json::array();
    nlohmann::json _summary = nlohmann::json::object();
};

} // namespace codedup
