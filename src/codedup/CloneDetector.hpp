// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/ProgressCallback.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace codedup
{

/// @brief A pair of code blocks detected as clones with their similarity score.
struct ClonePair
{
    size_t blockA = 0;     ///< Index of the first block in the block vector.
    size_t blockB = 0;     ///< Index of the second block in the block vector.
    double similarity = 0; ///< Similarity score (0.0 to 1.0, Dice coefficient).
};

/// @brief A group of code blocks that are all clones of each other.
struct CloneGroup
{
    std::vector<size_t> blockIndices; ///< Indices of the blocks in this clone group.
    double avgSimilarity = 0;         ///< Average pairwise similarity within the group.
};

/// @brief Result of LCS alignment between two normalized token sequences.
///
/// For each position in sequence A and B, indicates whether that position
/// participates in the Longest Common Subsequence. Positions not in the LCS
/// represent tokens that differ between the two sequences.
struct LcsAlignment
{
    std::vector<bool> matchedA; ///< matchedA[i] = true if a[i] is part of the LCS.
    std::vector<bool> matchedB; ///< matchedB[i] = true if b[i] is part of the LCS.
};

/// @brief Configuration for the clone detection algorithm.
struct CloneDetectorConfig
{
    double similarityThreshold = 0.80; ///< Minimum similarity for a pair to be reported.
    size_t minTokens = 30;             ///< Minimum block size in tokens.
    size_t hashWindowSize = 10;        ///< Sliding window size for fingerprinting.
    size_t minHashMatches = 3;         ///< Minimum shared fingerprints for candidacy.
    double textSensitivity = 0.0;      ///< Blend factor for text-preserving similarity (0.0-1.0).
                                       ///< 0.0 = pure structural (default), 1.0 = pure textual.
};

/// @brief Token frequency histogram for a code block.
///
/// Stores the count of each NormalizedTokenId in a block's token sequence.
/// Used by the bag-of-tokens Dice pre-filter to cheaply upper-bound LCS similarity.
struct BlockHistogram
{
    std::vector<uint32_t> counts; ///< counts[tokenId] = number of occurrences.
};

/// @brief Detects code clones using fingerprint-based filtering and LCS similarity.
///
/// Phase 1: Rabin-Karp fingerprinting with a sliding window to build an inverted
/// index of hash values to blocks, identifying candidate pairs.
///
/// Phase 2: Longest Common Subsequence (LCS) computation on candidate pairs with
/// Dice coefficient similarity scoring.
///
/// Grouping: Union-Find to merge clone pairs into connected components.
class CODEDUP_API CloneDetector
{
public:
    explicit CloneDetector(CloneDetectorConfig config = {}) : _config(config) {}

    /// @brief Detects clone groups among the given code blocks.
    /// @param blocks The code blocks to analyze.
    /// @param progressCallback Optional callback for reporting Phase 2 (similarity) progress.
    /// @param fingerprintCallback Optional callback for reporting Phase 1 (fingerprinting) progress.
    /// @param candidateCallback Optional callback for reporting candidate pair counting progress.
    /// @param collectCallback Optional callback for reporting merge and candidate collection progress.
    /// @return A vector of clone groups, each containing block indices and similarity.
    [[nodiscard]] auto Detect(std::vector<CodeBlock> const& blocks, ProgressCallback const& progressCallback = {},
                              ProgressCallback const& fingerprintCallback = {},
                              ProgressCallback const& candidateCallback = {},
                              ProgressCallback const& collectCallback = {}) -> std::vector<CloneGroup>;

    /// @brief Computes LCS-based Dice coefficient similarity between two ID sequences.
    ///
    /// Uses bit-parallel LCS (Allison-Dill / Hyyro-Navarro algorithm) for sequences where
    /// the shorter sequence has <= 256 tokens, falling back to the classic two-row DP otherwise.
    /// @param a First normalized token ID sequence.
    /// @param b Second normalized token ID sequence.
    /// @return Dice coefficient similarity (0.0 to 1.0).
    [[nodiscard]] static auto ComputeSimilarity(std::vector<NormalizedTokenId> const& a,
                                                std::vector<NormalizedTokenId> const& b) -> double;

    /// @brief Computes blended similarity using structural and text-preserving ID sequences.
    ///
    /// finalSim = (1 - textSensitivity) * structuralSim + textSensitivity * textualSim.
    /// When textSensitivity is 0 or text-preserving IDs are empty, returns pure structural similarity.
    /// @param structuralA Structural normalized IDs for block A.
    /// @param structuralB Structural normalized IDs for block B.
    /// @param textPreservingA Text-preserving IDs for block A (may be empty).
    /// @param textPreservingB Text-preserving IDs for block B (may be empty).
    /// @param textSensitivity Blend factor (0.0-1.0).
    /// @return Blended similarity (0.0 to 1.0).
    [[nodiscard]] static auto ComputeBlendedSimilarity(std::vector<NormalizedTokenId> const& structuralA,
                                                       std::vector<NormalizedTokenId> const& structuralB,
                                                       std::vector<NormalizedTokenId> const& textPreservingA,
                                                       std::vector<NormalizedTokenId> const& textPreservingB,
                                                       double textSensitivity) -> double;

    /// @brief Classic two-row DP implementation of LCS similarity (reference implementation).
    ///
    /// Kept for correctness testing against the optimized bit-parallel implementation.
    /// @param a First normalized token ID sequence.
    /// @param b Second normalized token ID sequence.
    /// @return Dice coefficient similarity (0.0 to 1.0).
    [[nodiscard]] static auto ComputeSimilarityClassic(std::vector<NormalizedTokenId> const& a,
                                                       std::vector<NormalizedTokenId> const& b) -> double;

    /// @brief Computes LCS alignment between two normalized token sequences.
    ///
    /// Uses classic O(m*n) DP with backtracking to identify which positions in each
    /// sequence participate in the LCS. Intended for report-time use only (not hot path).
    /// @param a First normalized token ID sequence.
    /// @param b Second normalized token ID sequence.
    /// @return LcsAlignment indicating matched positions in both sequences.
    [[nodiscard]] static auto ComputeLcsAlignment(std::span<NormalizedTokenId const> a,
                                                  std::span<NormalizedTokenId const> b) -> LcsAlignment;

    /// @brief Computes LCS-based Dice coefficient similarity with early termination.
    ///
    /// Like ComputeSimilarity, but aborts early if the threshold cannot be reached,
    /// returning 0.0. This avoids wasting work on pairs that will be filtered out.
    /// @param a First normalized token ID sequence.
    /// @param b Second normalized token ID sequence.
    /// @param threshold Minimum similarity threshold for early termination.
    /// @return Dice coefficient similarity (0.0 to 1.0), or 0.0 if threshold unreachable.
    [[nodiscard]] static auto ComputeSimilarityWithThreshold(std::vector<NormalizedTokenId> const& a,
                                                             std::vector<NormalizedTokenId> const& b,
                                                             double threshold) -> double;

    /// @brief Computes blended similarity with early termination support.
    ///
    /// Like ComputeBlendedSimilarity, but uses threshold-aware LCS to abort early
    /// when the threshold cannot be reached.
    [[nodiscard]] static auto ComputeBlendedSimilarityWithThreshold(
        std::vector<NormalizedTokenId> const& structuralA, std::vector<NormalizedTokenId> const& structuralB,
        std::vector<NormalizedTokenId> const& textPreservingA, std::vector<NormalizedTokenId> const& textPreservingB,
        double textSensitivity, double threshold) -> double;

    /// @brief Bag-of-tokens Dice pre-filter.
    ///
    /// Computes an upper bound on Dice similarity using token frequency histograms
    /// (multiset intersection). Since bag intersection >= LCS length is always true,
    /// if bag_dice < threshold, the true Dice similarity also < threshold.
    /// Cost: O(V) where V is the vocabulary size (~1003), much cheaper than LCS.
    ///
    /// @param histA Histogram of the first block.
    /// @param histB Histogram of the second block.
    /// @param lenA Length of the first block's token sequence.
    /// @param lenB Length of the second block's token sequence.
    /// @param threshold Minimum similarity threshold.
    /// @return True if the pair could possibly meet the threshold.
    [[nodiscard]] static auto BagDiceCompatible(BlockHistogram const& histA, BlockHistogram const& histB, size_t lenA,
                                                size_t lenB, double threshold) -> bool;

    /// @brief O(1) length-ratio pre-filter for clone candidate pairs.
    ///
    /// Computes the upper bound of the Dice coefficient based on sequence lengths alone:
    /// maxDice = 2 * min(lenA, lenB) / (lenA + lenB). If this upper bound is below the
    /// similarity threshold, the pair cannot possibly pass and LCS computation is skipped.
    ///
    /// @param lenA Length of the first sequence.
    /// @param lenB Length of the second sequence.
    /// @param threshold Minimum similarity threshold.
    /// @return True if the pair could possibly meet the threshold.
    [[nodiscard]] static constexpr auto LengthsCompatible(size_t lenA, size_t lenB, double threshold) -> bool
    {
        if (lenA == 0 || lenB == 0)
            return false;
        auto const maxDice = 2.0 * static_cast<double>(std::min(lenA, lenB)) / static_cast<double>(lenA + lenB);
        return maxDice >= threshold;
    }

private:
    CloneDetectorConfig _config;

    /// @brief Computes a rolling hash fingerprint for a window of token IDs.
    [[nodiscard]] auto ComputeFingerprints(std::vector<NormalizedTokenId> const& ids) const -> std::vector<uint64_t>;
};

} // namespace codedup
