// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>
#include <dude/CodeBlock.hpp>
#include <dude/ProgressCallback.hpp>

#include <cstddef>
#include <vector>

namespace dude
{

/// @brief A region within a code block's normalized token sequence.
struct IntraCloneRegion
{
    size_t start = 0;  ///< Start index in the block's normalizedIds.
    size_t length = 0; ///< Number of tokens in the region.
};

/// @brief A pair of duplicated regions within a single code block.
struct IntraClonePair
{
    size_t blockIndex = 0;    ///< Index of the block in the global block vector.
    IntraCloneRegion regionA; ///< First duplicated region.
    IntraCloneRegion regionB; ///< Second duplicated region.
    double similarity = 0;    ///< Dice coefficient similarity between the two regions.
};

/// @brief All intra-function clone results for one block.
struct IntraCloneResult
{
    size_t blockIndex = 0;             ///< Index of the block.
    std::vector<IntraClonePair> pairs; ///< Detected intra-function clone pairs.
};

/// @brief Configuration for intra-function clone detection.
struct IntraCloneDetectorConfig
{
    size_t minRegionTokens = 20;       ///< Minimum region size in tokens to report.
    double similarityThreshold = 0.80; ///< Minimum similarity for a pair to be reported.
    size_t hashWindowSize = 10;        ///< Sliding window size for fingerprinting.
    double textSensitivity = 0.0;      ///< Blend factor for text-preserving similarity (0.0-1.0).
                                       ///< 0.0 = pure structural (default), 1.0 = pure textual.
};

/// @brief Detects duplicated regions within individual functions (intra-function clones).
///
/// For each code block, computes rolling hash fingerprints over the normalized token
/// sequence and performs a self-join to find candidate duplicate regions. Candidates are
/// extended to maximal matches, similarity-filtered, and deduplicated.
class DUDE_API IntraFunctionDetector
{
public:
    explicit IntraFunctionDetector(IntraCloneDetectorConfig config = {}) : _config(config) {}

    /// @brief Detects intra-function clones in all blocks.
    /// @param blocks The code blocks to analyze.
    /// @param progressCallback Optional callback for reporting progress.
    /// @return A vector of results, one per block that contains intra-function clones.
    [[nodiscard]] auto Detect(std::vector<CodeBlock> const& blocks, ProgressCallback const& progressCallback = {})
        -> std::vector<IntraCloneResult>;

private:
    IntraCloneDetectorConfig _config;

    /// @brief Detects clones within a single block.
    /// @param block The code block to analyze.
    /// @param blockIndex The index of this block in the global block vector.
    /// @return A vector of intra-function clone pairs found within this block.
    [[nodiscard]] auto DetectInBlock(CodeBlock const& block, size_t blockIndex) const -> std::vector<IntraClonePair>;
};

} // namespace dude
