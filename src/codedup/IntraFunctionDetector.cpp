// SPDX-License-Identifier: Apache-2.0
#include <experimental/simd>

#include <codedup/IntraFunctionDetector.hpp>
#include <codedup/RollingHash.hpp>

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <set>
#include <unordered_map>
#include <vector>

namespace codedup
{

namespace stdx = std::experimental;

namespace
{

/// @brief SIMD type for comparing 8 uint32_t values (NormalizedTokenId) in parallel.
///
/// Uses std::experimental::simd with fixed_size<8> ABI, which maps to 256-bit SIMD
/// registers (AVX2) when available. Each iteration compares 8 token IDs simultaneously,
/// yielding 4-8x speedup over scalar comparison on the match extension hot path.
using SimdU32x8 = stdx::simd<uint32_t, stdx::simd_abi::fixed_size<8>>;

/// @brief SIMD-accelerated forward match extension between two token sequences.
///
/// Compares 8 NormalizedTokenId values per iteration using SIMD, finding the first
/// mismatch position. When a SIMD lane contains a mismatch, `find_first_set` identifies
/// the exact position within the 8-element group.
///
/// @param a Pointer to the start of the first region.
/// @param b Pointer to the start of the second region.
/// @param maxLen Maximum number of elements to compare.
/// @return The number of consecutive matching elements from the start.
[[nodiscard]] auto simdForwardMatch(NormalizedTokenId const* a, NormalizedTokenId const* b, size_t maxLen) -> size_t
{
    size_t offset = 0;

    // SIMD loop: compare 8 uint32_t values per iteration
    while (offset + 8 <= maxLen)
    {
        SimdU32x8 const va(a + offset, stdx::element_aligned);
        SimdU32x8 const vb(b + offset, stdx::element_aligned);
        auto const mismatch = va != vb;
        if (stdx::any_of(mismatch))
        {
            // Found a mismatch in this 8-element group; find the exact lane
            return offset + static_cast<size_t>(stdx::find_first_set(mismatch));
        }
        offset += 8;
    }

    // Scalar tail loop: handle remaining elements (maxLen not divisible by 8)
    while (offset < maxLen && a[offset] == b[offset])
        ++offset;

    return offset;
}

/// @brief SIMD-accelerated backward match extension between two token sequences.
///
/// Similar to simdForwardMatch but scans backward from the given positions. Compares
/// 8 elements per iteration in reverse order, using SIMD to find the first mismatch
/// when scanning toward the beginning of the sequences.
///
/// @param a Pointer to the start of the first region.
/// @param b Pointer to the start of the second region.
/// @param posA Current position in region A (extension starts at posA-1 going backward).
/// @param posB Current position in region B (extension starts at posB-1 going backward).
/// @return The number of consecutive matching elements going backward.
[[nodiscard]] auto simdBackwardMatch(NormalizedTokenId const* a, NormalizedTokenId const* b, size_t posA, size_t posB)
    -> size_t
{
    auto const maxLen = std::min(posA, posB);
    size_t matched = 0;

    // SIMD loop: compare 8 elements at a time going backward.
    // We load 8 elements ending at the current backward scan position.
    while (matched + 8 <= maxLen)
    {
        // Load 8 elements ending before current backward scan position
        auto const offA = posA - matched - 8;
        auto const offB = posB - matched - 8;
        SimdU32x8 const va(a + offA, stdx::element_aligned);
        SimdU32x8 const vb(b + offB, stdx::element_aligned);
        auto const mismatch = va != vb;
        if (stdx::any_of(mismatch))
        {
            // Found a mismatch; count matching elements from the end of this group.
            // find_first_set gives the first mismatching lane from the start (low index),
            // but we're scanning backward, so matching elements from the end of this
            // 8-element group are (7 - last_mismatch_lane). We need the count from
            // the high end, which is 7 - find_last_set(mismatch).
            // Simpler: check each element from the high end.
            for (auto const k : std::views::iota(size_t{0}, size_t{8}))
            {
                auto const idx = 7 - k;
                if (a[offA + idx] != b[offB + idx])
                    return matched + k;
            }
            return matched; // shouldn't reach here
        }
        matched += 8;
    }

    // Scalar tail loop
    while (matched < maxLen && a[posA - matched - 1] == b[posB - matched - 1])
        ++matched;

    return matched;
}

/// @brief Computes the overlap length between two 1D intervals [s1, s1+l1) and [s2, s2+l2).
[[nodiscard]] constexpr auto intervalOverlap(size_t s1, size_t l1, size_t s2, size_t l2) -> size_t
{
    auto const end1 = s1 + l1;
    auto const end2 = s2 + l2;
    if (s1 >= end2 || s2 >= end1)
        return 0;
    return std::min(end1, end2) - std::max(s1, s2);
}

/// @brief Computes positional text similarity for two aligned regions of equal length.
///
/// For SIMD-extended exact matches on structural IDs, the regions are guaranteed to be
/// aligned (same length, same structural token pattern). This function counts the
/// fraction of positions where text-preserving IDs also match, giving an O(n) textual
/// similarity score without needing LCS.
///
/// @param textIds The full text-preserving ID vector for the block.
/// @param startA Start index of region A.
/// @param startB Start index of region B.
/// @param length Length of both regions.
/// @return Fraction of matching positions (0.0 to 1.0), or 1.0 if textIds is empty.
[[nodiscard]] auto positionalTextSimilarity(std::vector<NormalizedTokenId> const& textIds, size_t startA, size_t startB,
                                            size_t length) -> double
{
    if (textIds.empty() || length == 0)
        return 1.0;

    size_t matches = 0;
    for (size_t i = 0; i < length; ++i)
    {
        if (textIds[startA + i] == textIds[startB + i])
            ++matches;
    }
    return static_cast<double>(matches) / static_cast<double>(length);
}

/// @brief Checks whether two intra-clone pairs describe substantially the same duplication.
///
/// Two pairs are considered redundant if both their A regions and B regions overlap
/// by more than 50% of the shorter region in each comparison. This ensures only truly
/// redundant pairs (describing the same underlying clone) are merged, preserving all
/// distinct valid pairs.
[[nodiscard]] auto pairsAreRedundant(IntraClonePair const& p, IntraClonePair const& q) -> bool
{
    // Check region A overlap
    auto const overlapA = intervalOverlap(p.regionA.start, p.regionA.length, q.regionA.start, q.regionA.length);
    auto const minLenA = std::min(p.regionA.length, q.regionA.length);
    if (minLenA == 0 || overlapA * 2 <= minLenA)
        return false;

    // Check region B overlap
    auto const overlapB = intervalOverlap(p.regionB.start, p.regionB.length, q.regionB.start, q.regionB.length);
    auto const minLenB = std::min(p.regionB.length, q.regionB.length);
    if (minLenB == 0 || overlapB * 2 <= minLenB)
        return false;

    return true;
}

} // namespace

auto IntraFunctionDetector::detect(std::vector<CodeBlock> const& blocks) -> std::vector<IntraCloneResult>
{
    std::vector<IntraCloneResult> results;

    for (auto const bi : std::views::iota(size_t{0}, blocks.size()))
    {
        auto pairs = detectInBlock(blocks[bi], bi);
        if (!pairs.empty())
        {
            results.push_back(IntraCloneResult{
                .blockIndex = bi,
                .pairs = std::move(pairs),
            });
        }
    }

    return results;
}

auto IntraFunctionDetector::detectInBlock(CodeBlock const& block, size_t blockIndex) -> std::vector<IntraClonePair>
{
    auto const& ids = block.normalizedIds;

    // Need at least 2x minRegionTokens to have any intra-function clone
    if (ids.size() < _config.minRegionTokens * 2)
        return {};

    // Phase 1: Fingerprint self-join
    auto const fingerprints = computeRollingFingerprints(ids, _config.hashWindowSize);
    if (fingerprints.empty())
        return {};

    // Build inverted index: fingerprint -> [position, ...]
    std::unordered_map<uint64_t, std::vector<size_t>> fpIndex;
    for (auto const pos : std::views::iota(size_t{0}, fingerprints.size()))
        fpIndex[fingerprints[pos]].push_back(pos);

    // Collect candidate position pairs from the inverted index.
    // Each pair of positions sharing a fingerprint is a candidate for match extension.
    std::set<std::pair<size_t, size_t>> candidatePairs;
    for (auto const& [fp, positions] : fpIndex)
    {
        if (positions.size() < 2)
            continue;
        // Skip over-common fingerprints (appears too many times within the block)
        if (positions.size() > 50)
            continue;

        for (auto const i : std::views::iota(size_t{0}, positions.size()))
        {
            for (auto const j : std::views::iota(i + 1, positions.size()))
            {
                // Ensure no overlap of the initial hash window
                if (positions[j] - positions[i] >= _config.hashWindowSize)
                    candidatePairs.emplace(positions[i], positions[j]);
            }
        }
    }

    // Phase 2: Extend matches to maximal regions
    std::vector<IntraClonePair> candidates;

    for (auto const& [posI, posJ] : candidatePairs)
    {
        // Forward extension limit: don't cross posJ from posI, and stay within bounds
        auto const fwdLimit = std::min({ids.size() - posI, ids.size() - posJ, posJ - posI});
        // Extend forward using SIMD-accelerated comparison (8 elements per iteration)
        auto const fwdLen = simdForwardMatch(ids.data() + posI, ids.data() + posJ, fwdLimit);

        // Backward extension: SIMD-accelerated, comparing ids[posI-k-1] vs ids[posJ-k-1]
        auto const bwdLen = simdBackwardMatch(ids.data(), ids.data(), posI, posJ);

        auto const startA = posI - bwdLen;
        auto const startB = posJ - bwdLen;
        auto const regionLen = bwdLen + fwdLen;

        // Filter by minimum region size
        if (regionLen < _config.minRegionTokens)
            continue;

        // Ensure regions don't overlap with each other
        if (startA + regionLen > startB)
            continue;

        // Compute similarity.
        // For SIMD-extended exact matches, structural similarity is 1.0 by construction.
        // When text sensitivity is enabled, blend with positional text similarity (O(n)).
        double similarity = 1.0; // Structural sim is 1.0 for exact matches on normalized IDs

        if (_config.textSensitivity > 0.0 && !block.textPreservingIds.empty())
        {
            auto const textualSim = positionalTextSimilarity(block.textPreservingIds, startA, startB, regionLen);
            similarity = (1.0 - _config.textSensitivity) * 1.0 + _config.textSensitivity * textualSim;
        }

        if (similarity < _config.similarityThreshold)
            continue;

        candidates.push_back(IntraClonePair{
            .blockIndex = blockIndex,
            .regionA = {.start = startA, .length = regionLen},
            .regionB = {.start = startB, .length = regionLen},
            .similarity = similarity,
        });
    }

    if (candidates.empty())
        return {};

    // Phase 3: Deduplicate redundant pairs (non-lossy)
    //
    // Sort by region length (longest first), then by similarity (highest first).
    // For each group of redundant pairs (both A and B regions substantially overlap),
    // keep only the best representative. This preserves all distinct clone pairs.
    std::ranges::sort(candidates,
                      [](auto const& a, auto const& b)
                      {
                          auto const lenA = a.regionA.length + a.regionB.length;
                          auto const lenB = b.regionA.length + b.regionB.length;
                          if (lenA != lenB)
                              return lenA > lenB;
                          return a.similarity > b.similarity;
                      });

    std::vector<IntraClonePair> result;
    result.reserve(candidates.size());

    for (auto const& candidate : candidates)
    {
        bool isRedundant = false;
        for (auto const& kept : result)
        {
            if (pairsAreRedundant(candidate, kept))
            {
                isRedundant = true;
                break;
            }
        }
        if (!isRedundant)
            result.push_back(candidate);
    }

    return result;
}

} // namespace codedup
