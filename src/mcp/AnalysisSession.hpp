// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/AnalysisScope.hpp>
#include <dude/CloneDetector.hpp>
#include <dude/CodeBlock.hpp>
#include <dude/Encoding.hpp>
#include <dude/IntraFunctionDetector.hpp>
#include <dude/Reporter.hpp>
#include <dude/Token.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace mcp
{

/// @brief Configuration for a code duplication analysis run.
struct AnalysisConfig
{
    std::filesystem::path directory;                          ///< Directory to scan.
    double threshold = 0.80;                                  ///< Similarity threshold.
    size_t minTokens = 30;                                    ///< Minimum block size in tokens.
    double textSensitivity = 0.3;                             ///< Text sensitivity blend factor.
    dude::AnalysisScope scope = dude::AnalysisScope::All;     ///< Analysis scope bitmask.
    std::vector<std::string> globPatterns;                    ///< Filename glob patterns (empty = defaults).
    dude::InputEncoding encoding = dude::InputEncoding::Auto; ///< Input encoding.
};

/// @brief Error from an analysis operation.
struct AnalysisError
{
    std::string message; ///< Human-readable error description.
};

/// @brief Caches a full analysis pipeline run for interactive MCP access.
///
/// Wraps the scan → tokenize → normalize → extract → detect pipeline,
/// storing all intermediate results for subsequent tool queries.
class AnalysisSession
{
public:
    /// @brief Runs a full analysis pipeline on the given directory.
    /// @param config Analysis configuration.
    /// @return void on success, or an AnalysisError on failure.
    auto Analyze(AnalysisConfig const& config) -> std::expected<void, AnalysisError>;

    /// @brief Reconfigures detection parameters and re-runs detection on cached data.
    ///
    /// Reuses cached files and tokens but re-runs block extraction and detection
    /// since minTokens, threshold, or scope may have changed.
    ///
    /// @param threshold New similarity threshold.
    /// @param minTokens New minimum block size.
    /// @param textSensitivity New text sensitivity.
    /// @param scope New analysis scope.
    /// @return void on success, or an AnalysisError on failure.
    auto Reconfigure(double threshold, size_t minTokens, double textSensitivity, dude::AnalysisScope scope)
        -> std::expected<void, AnalysisError>;

    /// @brief Returns whether analysis results are available.
    [[nodiscard]] auto HasResults() const -> bool { return _hasResults; }

    /// @brief Returns the current analysis configuration.
    [[nodiscard]] auto Config() const -> AnalysisConfig const& { return _config; }

    /// @brief Returns the scanned file paths.
    [[nodiscard]] auto Files() const -> std::vector<std::filesystem::path> const& { return _files; }

    /// @brief Returns the token vectors for all files.
    [[nodiscard]] auto AllTokens() const -> std::vector<std::vector<dude::Token>> const& { return _allTokens; }

    /// @brief Returns all extracted code blocks.
    [[nodiscard]] auto AllBlocks() const -> std::vector<dude::CodeBlock> const& { return _allBlocks; }

    /// @brief Returns the detected inter-function clone groups.
    [[nodiscard]] auto CloneGroups() const -> std::vector<dude::CloneGroup> const& { return _groups; }

    /// @brief Returns the detected intra-function clone results.
    [[nodiscard]] auto IntraResults() const -> std::vector<dude::IntraCloneResult> const& { return _intraResults; }

    /// @brief Returns the block-to-file-index mapping.
    [[nodiscard]] auto BlockToFileIndex() const -> std::vector<size_t> const& { return _blockToFileIndex; }

    /// @brief Returns the performance timing data.
    [[nodiscard]] auto Timing() const -> dude::PerformanceTiming const& { return _timing; }

    /// @brief Reads the source code for a code block from disk.
    /// @param blockIndex Index of the block in AllBlocks().
    /// @return The source code string, or an error.
    [[nodiscard]] auto ReadBlockSource(size_t blockIndex) const -> std::expected<std::string, AnalysisError>;

private:
    bool _hasResults = false;
    AnalysisConfig _config;
    std::vector<std::filesystem::path> _files;
    std::vector<std::vector<dude::Token>> _allTokens;
    std::vector<dude::Language const*> _fileLanguages;
    std::vector<dude::CodeBlock> _allBlocks;
    std::vector<size_t> _blockToFileIndex;
    std::vector<dude::CloneGroup> _groups;
    std::vector<dude::IntraCloneResult> _intraResults;
    dude::PerformanceTiming _timing{};

    void RunBlockExtractionAndDetection();
};

} // namespace mcp
