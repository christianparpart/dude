// SPDX-License-Identifier: Apache-2.0
#include <ankerl/unordered_dense.h>

#include <codedup/CloneDetector.hpp>
#include <codedup/RollingHash.hpp>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <numeric>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

namespace codedup
{

namespace
{

/// @brief Hash functor for std::pair<size_t, size_t> using golden-ratio bit mixing.
struct PairHash
{
    auto operator()(std::pair<size_t, size_t> const& p) const noexcept -> size_t
    {
        auto const h1 = std::hash<size_t>{}(p.first);
        auto const h2 = std::hash<size_t>{}(p.second);
        return h1 ^ (h2 * size_t{0x9E3779B97F4A7C15} + (h1 << 6) + (h1 >> 2));
    }
};

/// @brief Union-Find data structure for grouping clone pairs.
class UnionFind
{
public:
    explicit UnionFind(size_t n) : _parent(n), _rank(n, 0) { std::ranges::iota(_parent, size_t{0}); }

    [[nodiscard]] auto Find(size_t x) -> size_t
    {
        if (_parent[x] != x)
            _parent[x] = Find(_parent[x]);
        return _parent[x];
    }

    void Unite(size_t x, size_t y)
    {
        auto const rx = Find(x);
        auto const ry = Find(y);
        if (rx == ry)
            return;
        if (_rank[rx] < _rank[ry])
            _parent[rx] = ry;
        else if (_rank[rx] > _rank[ry])
            _parent[ry] = rx;
        else
        {
            _parent[ry] = rx;
            ++_rank[rx];
        }
    }

private:
    std::vector<size_t> _parent;
    std::vector<size_t> _rank;
};

/// @brief Finds the maximum token ID in a sequence for flat PM table sizing.
/// @param b The token sequence to scan.
/// @return The maximum NormalizedTokenId value found, or 0 if empty.
[[nodiscard]] auto FindMaxId(std::span<NormalizedTokenId const> b) -> NormalizedTokenId
{
    NormalizedTokenId maxId = 0;
    for (auto const id : b)
        maxId = std::max(maxId, id);
    return maxId;
}

/// @brief A candidate pair for Phase 2 similarity computation.
struct CandidatePair
{
    size_t blockA = 0; ///< Index of the first block.
    size_t blockB = 0; ///< Index of the second block.
};

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto CloneDetector::Detect(std::vector<CodeBlock> const& blocks, ProgressCallback const& progressCallback,
                           ProgressCallback const& fingerprintCallback, ProgressCallback const& candidateCallback,
                           ProgressCallback const& collectCallback)
    -> std::vector<CloneGroup>
{
    if (blocks.size() < 2)
        return {};

    // Phase 1: Build inverted index of fingerprints to block indices
    ankerl::unordered_dense::map<uint64_t, std::vector<size_t>> fingerprintIndex;

    for (auto const bi : std::views::iota(size_t{0}, blocks.size()))
    {
        auto const fingerprints = ComputeFingerprints(blocks[bi].normalizedIds);
        ankerl::unordered_dense::set<uint64_t> seen; // Deduplicate within a single block
        for (auto const fp : fingerprints)
        {
            if (seen.insert(fp).second)
                fingerprintIndex[fp].push_back(bi);
        }
        if (fingerprintCallback)
            fingerprintCallback(bi + 1, blocks.size());
    }

    // Find candidate pairs: blocks sharing >= minHashMatches fingerprints
    // Skip over-common fingerprints (appearing in > 50 blocks)
    auto const maxBlocksPerFingerprint = std::max(size_t{50}, blocks.size() / 2);

    // Convert fingerprint index to a vector of block-lists for indexed parallel access.
    // We only need the block lists; the fingerprint hash keys are not used during counting.
    std::vector<std::vector<size_t>> fingerprintBlocks;
    fingerprintBlocks.reserve(fingerprintIndex.size());
    for (auto& [fp, blockList] : fingerprintIndex)
        fingerprintBlocks.push_back(std::move(blockList));
    fingerprintIndex.clear(); // Free map memory early

    auto const fingerprintCount = fingerprintBlocks.size();

    // Parallel candidate gathering with partitioned pair-count maps.
    // Each worker uses numWorkers partition maps instead of a single map.
    // Pairs are routed to partitions via PairHash(key) % numWorkers, so each
    // partition is independently mergeable — enabling parallel merge + collect.
    auto const numWorkers = static_cast<size_t>(std::max(1U, std::thread::hardware_concurrency()));
    exec::static_thread_pool pool(std::max(1U, std::thread::hardware_concurrency()));
    auto sched = pool.get_scheduler();

    using PairCountMap = ankerl::unordered_dense::map<std::pair<size_t, size_t>, size_t, PairHash>;

    // perWorkerPartitions[workerIdx][partitionIdx] — numWorkers × numWorkers small maps
    std::vector<std::vector<PairCountMap>> perWorkerPartitions(numWorkers, std::vector<PairCountMap>(numWorkers));
    std::atomic<size_t> fingerprintProcessed{0};

    auto const gatherRange = [&](std::size_t workerIdx)
    {
        auto& partitions = perWorkerPartitions[workerIdx];

        // Interleaved (striped) assignment: worker i processes fingerprints i, i+N, i+2N, ...
        // This distributes expensive fingerprints (large block lists → O(n²) pairs) evenly
        // across workers, avoiding the tail-end stall of contiguous chunking.
        for (auto i = workerIdx; i < fingerprintCount; i += numWorkers)
        {
            auto const& blockList = fingerprintBlocks[i];
            if (blockList.size() <= maxBlocksPerFingerprint)
            {
                for (auto const ii : std::views::iota(size_t{0}, blockList.size()))
                {
                    for (auto const jj : std::views::iota(ii + 1, blockList.size()))
                    {
                        auto const key =
                            std::pair{std::min(blockList[ii], blockList[jj]), std::max(blockList[ii], blockList[jj])};
                        auto const partIdx = PairHash{}(key) % numWorkers;
                        ++partitions[partIdx][key];
                    }
                }
            }

            if (candidateCallback)
            {
                auto const processed = fingerprintProcessed.fetch_add(1, std::memory_order_relaxed) + 1;
                candidateCallback(processed, fingerprintCount);
            }
        }
    };

    {
        auto work = stdexec::starts_on(
            sched, stdexec::just() | stdexec::bulk(stdexec::par, numWorkers, gatherRange));
        stdexec::sync_wait(work);
    }

    // Parallel merge + collect: each partition is independently merged and filtered.
    // perPartitionCandidates[partIdx] holds the candidates found in that partition.
    std::vector<std::vector<CandidatePair>> perPartitionCandidates(numWorkers);
    std::atomic<size_t> partitionsCompleted{0};

    auto const collectPartition = [&](std::size_t partIdx)
    {
        // Find the worker with the largest contribution for this partition (move it).
        size_t largestWorker = 0;
        for (size_t w = 1; w < numWorkers; ++w)
        {
            if (perWorkerPartitions[w][partIdx].size() > perWorkerPartitions[largestWorker][partIdx].size())
                largestWorker = w;
        }

        PairCountMap merged = std::move(perWorkerPartitions[largestWorker][partIdx]);

        // Merge remaining workers' entries into the moved map.
        for (size_t w = 0; w < numWorkers; ++w)
        {
            if (w == largestWorker)
                continue;
            for (auto& [key, count] : perWorkerPartitions[w][partIdx])
                merged[key] += count;
            // Free memory eagerly.
            perWorkerPartitions[w][partIdx] = PairCountMap{};
        }

        // Filter by thresholds and produce candidates.
        auto& localCandidates = perPartitionCandidates[partIdx];
        for (auto const& [pair, count] : merged)
        {
            if (count < _config.minHashMatches)
                continue;
            if (!LengthsCompatible(blocks[pair.first].normalizedIds.size(),
                                    blocks[pair.second].normalizedIds.size(), _config.similarityThreshold))
                continue;
            localCandidates.push_back(CandidatePair{.blockA = pair.first, .blockB = pair.second});
        }

        if (collectCallback)
        {
            auto const completed = partitionsCompleted.fetch_add(1, std::memory_order_relaxed) + 1;
            collectCallback(completed, numWorkers);
        }
    };

    {
        auto work = stdexec::starts_on(
            sched, stdexec::just() | stdexec::bulk(stdexec::par, numWorkers, collectPartition));
        stdexec::sync_wait(work);
    }

    // Lightweight sequential merge of per-partition candidate vectors.
    std::vector<CandidatePair> candidates;
    {
        size_t totalCandidates = 0;
        for (auto const& pc : perPartitionCandidates)
            totalCandidates += pc.size();
        candidates.reserve(totalCandidates);
        for (auto& pc : perPartitionCandidates)
            candidates.insert(candidates.end(), std::make_move_iterator(pc.begin()),
                              std::make_move_iterator(pc.end()));
    }

    // Parallel similarity computation using stdexec::bulk.
    auto const candidateCount = candidates.size();

    std::vector<std::vector<ClonePair>> perWorkerResults(numWorkers);
    std::atomic<size_t> processedCount{0};

    auto const computeRange = [&](std::size_t workerIdx)
    {
        auto& localPairs = perWorkerResults[workerIdx];

        // Interleaved (striped) assignment for even load distribution across workers.
        for (auto i = workerIdx; i < candidateCount; i += numWorkers)
        {
            auto const& candidate = candidates[i];
            auto const& blockA = blocks[candidate.blockA];
            auto const& blockB = blocks[candidate.blockB];
            auto const similarity =
                ComputeBlendedSimilarity(blockA.normalizedIds, blockB.normalizedIds, blockA.textPreservingIds,
                                         blockB.textPreservingIds, _config.textSensitivity);
            if (similarity >= _config.similarityThreshold)
            {
                localPairs.push_back(ClonePair{
                    .blockA = candidate.blockA,
                    .blockB = candidate.blockB,
                    .similarity = similarity,
                });
            }

            if (progressCallback)
            {
                auto const processed = processedCount.fetch_add(1, std::memory_order_relaxed) + 1;
                progressCallback(processed, candidateCount);
            }
        }
    };

    {
        auto work = stdexec::starts_on(
            sched, stdexec::just() | stdexec::bulk(stdexec::par, numWorkers, computeRange));
        stdexec::sync_wait(work);
    }

    // Merge per-worker results
    std::vector<ClonePair> clonePairs;
    for (auto& workerPairs : perWorkerResults)
    {
        clonePairs.insert(clonePairs.end(), std::make_move_iterator(workerPairs.begin()),
                          std::make_move_iterator(workerPairs.end()));
    }

    if (clonePairs.empty())
        return {};

    // Grouping: Union-Find to form connected components
    UnionFind uf(blocks.size());
    for (auto const& cp : clonePairs)
        uf.Unite(cp.blockA, cp.blockB);

    // Collect groups
    ankerl::unordered_dense::map<size_t, std::vector<size_t>> groupMap;
    for (auto const& cp : clonePairs)
    {
        auto const root = uf.Find(cp.blockA);
        // Collect unique block indices per group
        groupMap[root]; // ensure entry exists
    }

    // Gather all blocks that participate in any clone pair
    ankerl::unordered_dense::set<size_t> participatingBlocks;
    for (auto const& cp : clonePairs)
    {
        participatingBlocks.insert(cp.blockA);
        participatingBlocks.insert(cp.blockB);
    }

    for (auto const block : participatingBlocks)
    {
        auto const root = uf.Find(block);
        groupMap[root].push_back(block);
    }

    // Build CloneGroup results
    std::vector<CloneGroup> groups;
    for (auto& [root, blockIndices] : groupMap)
    {
        // Deduplicate and sort
        std::ranges::sort(blockIndices);
        auto const [eraseBegin, eraseEnd] = std::ranges::unique(blockIndices);
        blockIndices.erase(eraseBegin, eraseEnd);

        if (blockIndices.size() < 2)
            continue;

        // Compute average similarity from pairs within this group
        double totalSim = 0;
        size_t pairCount = 0;
        for (auto const& cp : clonePairs)
        {
            if (uf.Find(cp.blockA) == root)
            {
                totalSim += cp.similarity;
                ++pairCount;
            }
        }

        groups.push_back(CloneGroup{
            .blockIndices = std::move(blockIndices),
            .avgSimilarity = pairCount > 0 ? totalSim / static_cast<double>(pairCount) : 0.0,
        });
    }

    // Sort groups by size (largest first), then by similarity
    std::ranges::sort(groups,
                      [](auto const& a, auto const& b)
                      {
                          if (a.blockIndices.size() != b.blockIndices.size())
                              return a.blockIndices.size() > b.blockIndices.size();
                          return a.avgSimilarity > b.avgSimilarity;
                      });

    return groups;
}

auto CloneDetector::ComputeLcsAlignment(std::span<NormalizedTokenId const> a, std::span<NormalizedTokenId const> b)
    -> LcsAlignment
{
    auto const m = a.size();
    auto const n = b.size();

    LcsAlignment result{
        .matchedA = std::vector<bool>(m, false),
        .matchedB = std::vector<bool>(n, false),
    };

    if (m == 0 || n == 0)
        return result;

    // Build full DP table for backtracking
    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1, 0));

    for (auto const i : std::views::iota(size_t{1}, m + 1))
    {
        for (auto const j : std::views::iota(size_t{1}, n + 1))
        {
            if (a[i - 1] == b[j - 1])
                dp[i][j] = dp[i - 1][j - 1] + 1;
            else
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
        }
    }

    // Backtrack to identify matched positions
    auto i = m;
    auto j = n;
    while (i > 0 && j > 0)
    {
        if (a[i - 1] == b[j - 1])
        {
            result.matchedA[i - 1] = true;
            result.matchedB[j - 1] = true;
            --i;
            --j;
        }
        else if (dp[i - 1][j] >= dp[i][j - 1])
        {
            --i;
        }
        else
        {
            --j;
        }
    }

    return result;
}

auto CloneDetector::ComputeFingerprints(std::vector<NormalizedTokenId> const& ids) const -> std::vector<uint64_t>
{
    return ComputeRollingFingerprints(ids, _config.hashWindowSize);
}

// ============================================================================================
// Bit-Parallel LCS Algorithm (Allison-Dill / Hyyro-Navarro)
// ============================================================================================
//
// Reference:
//   Hyyro, H. & Navarro, G. (2004). "A Practical O(m*log(sigma)*ceil(n/w)) Algorithm for
//   Computing the LCS." Proc. CPM 2004. Also: Allison, L. & Dill, T.I. (1986).
//
// The algorithm computes the LCS length of sequences `a` (length m) and `b` (length n)
// in O(m * ceil(n/w)) time, where w is the machine word size (64 bits).
//
// Key idea: Encode the DP table column-wise into bitvectors. For each row i of the DP table,
// maintain a bitvector M where bit j is set iff DP[i][j] > DP[i][j-1] (i.e., column j
// contributes +1 to the LCS at row i).
//
//   DP Table Mapping (columns = sequence b, rows = sequence a):
//
//     b:  b0  b1  b2  b3  ...  b(n-1)
//   a0  [ .   .   .   .   ...   .   ]
//   a1  [ .   .   .   .   ...   .   ]
//   ...
//   a(m-1)
//
//   M bitvector: bit j is set iff DP[current_row][j] > DP[current_row][j-1]
//   (i.e., the LCS length increased at column j compared to the column before it)
//
// For each element a[i], the update is:
//   1. Look up PM[a[i]] -- a precomputed bitvector where bit j is set iff b[j] == a[i]
//   2. X  = M | PM[a[i]]     -- candidate positions: either already matched or newly matching
//   3. M' = X & ((X - ((M << 1) | 1)) ^ X)
//      - (M << 1) | 1: shifts the match vector to represent "previous column matched"
//      - X - ...: subtraction propagates carries through consecutive set bits, effectively
//        implementing the DP recurrence max(DP[i-1][j], DP[i][j-1]) in parallel
//      - XOR with X isolates the new matches
//      - AND with X keeps only valid positions
//
// At the end, LCS length = popcount(M), since each set bit represents one +1 step in the LCS.
// ============================================================================================

namespace
{

/// @brief 256-bit wide bitvector for bit-parallel LCS on sequences with 65-256 tokens.
///
/// Stores four 64-bit words in little-endian order (word[0] = bits 0-63, word[3] = bits 192-255).
/// Supports the bitwise operations needed by the Hyyro-Navarro LCS algorithm:
/// OR, AND, XOR, subtraction-with-borrow, and shift-left-by-1.
struct BitVector256
{
    std::array<uint64_t, 4> words = {};

    /// @brief Sets bit at position `pos` (0-indexed).
    constexpr void SetBit(size_t pos) { words[pos / 64] |= (uint64_t{1} << (pos % 64)); }

    /// @brief Bitwise OR -- combines candidate positions (M | PM[c]).
    [[nodiscard]] constexpr auto operator|(BitVector256 const& rhs) const -> BitVector256
    {
        return BitVector256{
            .words = {words[0] | rhs.words[0], words[1] | rhs.words[1], words[2] | rhs.words[2],
                      words[3] | rhs.words[3]},
        };
    }

    /// @brief Bitwise AND -- isolates valid match positions (X & carry_result).
    [[nodiscard]] constexpr auto operator&(BitVector256 const& rhs) const -> BitVector256
    {
        return BitVector256{
            .words = {words[0] & rhs.words[0], words[1] & rhs.words[1], words[2] & rhs.words[2],
                      words[3] & rhs.words[3]},
        };
    }

    /// @brief Bitwise XOR -- computes (X - shifted_M) ^ X to find new match positions.
    [[nodiscard]] constexpr auto operator^(BitVector256 const& rhs) const -> BitVector256
    {
        return BitVector256{
            .words = {words[0] ^ rhs.words[0], words[1] ^ rhs.words[1], words[2] ^ rhs.words[2],
                      words[3] ^ rhs.words[3]},
        };
    }

    /// @brief Subtraction with borrow propagation across 64-bit words.
    ///
    /// Implements the carry-propagation subtraction X - ((M << 1) | 1) needed by the
    /// bit-parallel LCS recurrence. Borrow propagates from word[0] through word[3],
    /// mirroring how carry propagates across columns in the DP table.
    [[nodiscard]] constexpr auto operator-(BitVector256 const& rhs) const -> BitVector256
    {
        BitVector256 result;
        uint64_t borrow = 0;
        for (auto const i : std::views::iota(size_t{0}, size_t{4}))
        {
            auto const lhs = words[i];
            auto const sub = rhs.words[i];
            // Subtract rhs word and previous borrow
            result.words[i] = lhs - sub - borrow;
            // Borrow occurs if lhs < sub+borrow (unsigned underflow)
            borrow = (lhs < sub || (borrow != 0 && lhs == sub)) ? uint64_t{1} : uint64_t{0};
        }
        return result;
    }

    /// @brief Shift left by 1 bit with carry propagation across words.
    ///
    /// Implements M << 1 for the LCS recurrence. The shift-out bit from each word
    /// becomes the shift-in bit for the next word, maintaining the logical continuity
    /// of the bitvector across 64-bit boundaries.
    [[nodiscard]] constexpr auto ShiftLeft1() const -> BitVector256
    {
        BitVector256 result;
        uint64_t carry = 0;
        for (auto const i : std::views::iota(size_t{0}, size_t{4}))
        {
            result.words[i] = (words[i] << 1) | carry;
            carry = words[i] >> 63; // high bit becomes carry for next word
        }
        return result;
    }

    /// @brief Population count -- total set bits across all words.
    /// Returns the LCS length after the algorithm completes.
    [[nodiscard]] constexpr auto Popcount() const -> size_t
    {
        size_t count = 0;
        for (auto const w : words)
            count += static_cast<size_t>(std::popcount(w));
        return count;
    }
};

/// @brief Dynamic-width bitvector for bit-parallel LCS on sequences with n > 256 tokens.
///
/// Uses W = ceil(n/64) 64-bit words, achieving O(m * W) time instead of O(m * n) classic DP.
/// For a 500-token sequence: W = 8 words, so 500 * 8 = 4,000 word ops vs 250,000 classic DP ops.
class DynamicBitVector
{
public:
    /// @brief Constructs a zero-initialized bitvector with the given number of 64-bit words.
    /// @param numWords Number of 64-bit words (W = ceil(n/64)).
    explicit DynamicBitVector(size_t numWords) : _words(numWords, 0) {}

    /// @brief Sets bit at position `pos` (0-indexed).
    void SetBit(size_t pos) { _words[pos / 64] |= (uint64_t{1} << (pos % 64)); }

    /// @brief Returns a mutable reference to word at index `w`.
    [[nodiscard]] auto Word(size_t w) -> uint64_t& { return _words[w]; }

    /// @brief Returns a const reference to word at index `w`.
    [[nodiscard]] auto Word(size_t w) const -> uint64_t const& { return _words[w]; }

    /// @brief Returns the number of 64-bit words.
    [[nodiscard]] auto NumWords() const -> size_t { return _words.size(); }

    /// @brief Population count -- total set bits across all words.
    [[nodiscard]] auto Popcount() const -> size_t
    {
        size_t count = 0;
        for (auto const w : _words)
            count += static_cast<size_t>(std::popcount(w));
        return count;
    }

private:
    std::vector<uint64_t> _words;
};

/// @brief Computes LCS length using the bit-parallel algorithm for n <= 64.
///
/// Tier 1: Single uint64_t bitvector. Covers most functions (30-64 tokens).
/// Each iteration processes one element of sequence `a` using 5 bitwise operations.
/// Uses a flat vector indexed by token ID for O(1) PM lookups.
///
/// @param a First sequence (processed element-by-element).
/// @param b Second sequence (encoded into PM bitvectors, must have size <= 64).
/// @return LCS length.
[[nodiscard]] auto LcsLengthBitParallel64(std::span<NormalizedTokenId const> a, std::span<NormalizedTokenId const> b)
    -> size_t
{
    auto const n = b.size();

    // Flat PM table: indexed by token ID for O(1) lookups
    auto const maxId = FindMaxId(b);
    std::vector<uint64_t> pm(static_cast<size_t>(maxId) + 1, 0);
    for (auto const j : std::views::iota(size_t{0}, n))
        pm[b[j]] |= (uint64_t{1} << j);

    uint64_t M = 0; // Match bitvector: bit j set iff DP[i][j] > DP[i][j-1]

    for (auto const& ai : a)
    {
        auto const pmVal = (ai <= maxId) ? pm[ai] : uint64_t{0};

        // X = M | PM[a[i]]: all candidate positions (previously matched or newly matching)
        auto const X = M | pmVal;
        // M' = X & ((X - ((M << 1) | 1)) ^ X): propagate carries to resolve DP recurrence
        M = X & ((X - ((M << 1) | uint64_t{1})) ^ X);
    }

    return static_cast<size_t>(std::popcount(M));
}

/// @brief Computes LCS length using the bit-parallel algorithm for 65 <= n <= 256.
///
/// Tier 2: Uses BitVector256 (4 x uint64_t) with carry propagation across words.
/// Covers virtually all practical function sizes (up to 256 tokens).
/// Uses a flat vector indexed by token ID for O(1) PM lookups.
///
/// @param a First sequence (processed element-by-element).
/// @param b Second sequence (encoded into PM bitvectors, must have size <= 256).
/// @return LCS length.
[[nodiscard]] auto LcsLengthBitParallel256(std::span<NormalizedTokenId const> a, std::span<NormalizedTokenId const> b)
    -> size_t
{
    auto const n = b.size();

    // Flat PM table: indexed by token ID for O(1) lookups
    auto const maxId = FindMaxId(b);
    std::vector<BitVector256> pm(static_cast<size_t>(maxId) + 1);
    for (auto const j : std::views::iota(size_t{0}, n))
        pm[b[j]].SetBit(j);

    BitVector256 M{}; // Match bitvector

    // Unit bitvector with only bit 0 set, used as the "| 1" in (M << 1) | 1
    BitVector256 one{};
    one.words[0] = 1;

    for (auto const& ai : a)
    {
        constexpr BitVector256 zero{};
        auto const& pmVal = (ai <= maxId) ? pm[ai] : zero;

        auto const X = M | pmVal;
        M = X & ((X - (M.ShiftLeft1() | one)) ^ X);
    }

    return M.Popcount();
}

/// @brief Computes LCS length using the dynamic-width bit-parallel algorithm for n > 256.
///
/// Replaces the O(m*n) classic DP fallback with O(m * ceil(n/64)) bit-parallel computation.
/// Uses a flat 2D PM table of size (maxId+1) * W, indexed as pm[tokenId * W + word].
///
/// @param a First sequence (processed element-by-element).
/// @param b Second sequence (encoded into PM bitvectors).
/// @return LCS length.
[[nodiscard]] auto LcsLengthBitParallelDynamic(std::span<NormalizedTokenId const> a,
                                               std::span<NormalizedTokenId const> b) -> size_t
{
    auto const n = b.size();
    auto const W = (n + 63) / 64; // Number of 64-bit words

    // Flat 2D PM table: pm[tokenId * W + word]
    auto const maxId = FindMaxId(b);
    auto const pmTableSize = (static_cast<size_t>(maxId) + 1) * W;
    std::vector<uint64_t> pm(pmTableSize, 0);
    for (auto const j : std::views::iota(size_t{0}, n))
    {
        auto const wordIdx = j / 64;
        auto const bitIdx = j % 64;
        pm[static_cast<size_t>(b[j]) * W + wordIdx] |= (uint64_t{1} << bitIdx);
    }

    // M bitvector: W words, all zero-initialized
    DynamicBitVector M(W);

    for (auto const& ai : a)
    {
        auto const* pmRow = (ai <= maxId) ? &pm[static_cast<size_t>(ai) * W] : nullptr;

        uint64_t carryShift = 1; // For (M << 1) | 1
        uint64_t borrowSub = 0;  // For X - shifted

        for (size_t w = 0; w < W; ++w)
        {
            auto const mw = M.Word(w);
            auto const pmw = pmRow ? pmRow[w] : uint64_t{0};

            auto const X = mw | pmw;

            // (M << 1) | 1: shift left with carry propagation
            auto const shifted = (mw << 1) | carryShift;
            carryShift = mw >> 63;

            // X - shifted: subtraction with borrow propagation
            auto const sub = X - shifted - borrowSub;
            borrowSub = (X < shifted || (borrowSub != 0 && X == shifted)) ? 1ULL : 0ULL;

            M.Word(w) = X & (sub ^ X);
        }
    }

    return M.Popcount();
}

} // anonymous namespace

auto CloneDetector::ComputeSimilarity(std::vector<NormalizedTokenId> const& a, std::vector<NormalizedTokenId> const& b)
    -> double
{
    if (a.empty() || b.empty())
        return 0.0;

    auto const m = a.size();
    auto const n = b.size();

    // Ensure b is the shorter sequence for the bit-parallel algorithm, since the bitvector
    // width is determined by the length of b. Swapping ensures we use the narrowest tier.
    auto const& seqA = (m >= n) ? a : b; // longer sequence (processed row-by-row)
    auto const& seqB = (m >= n) ? b : a; // shorter sequence (encoded as bitvectors)
    auto const shortLen = std::min(m, n);

    // Tiered dispatch based on the shorter sequence length
    size_t lcsLength = 0;
    if (shortLen <= 64)
        lcsLength = LcsLengthBitParallel64(seqA, seqB);
    else if (shortLen <= 256)
        lcsLength = LcsLengthBitParallel256(seqA, seqB);
    else
        lcsLength = LcsLengthBitParallelDynamic(seqA, seqB);

    // Dice coefficient: 2 * |LCS| / (|A| + |B|)
    return 2.0 * static_cast<double>(lcsLength) / static_cast<double>(m + n);
}

auto CloneDetector::ComputeSimilarityClassic(std::vector<NormalizedTokenId> const& a,
                                             std::vector<NormalizedTokenId> const& b) -> double
{
    if (a.empty() || b.empty())
        return 0.0;

    auto const m = a.size();
    auto const n = b.size();

    // LCS using two-row DP (space-efficient)
    std::vector<size_t> prev(n + 1, 0);
    std::vector<size_t> curr(n + 1, 0);

    for (auto const i : std::views::iota(size_t{1}, m + 1))
    {
        for (auto const j : std::views::iota(size_t{1}, n + 1))
        {
            if (a[i - 1] == b[j - 1])
                curr[j] = prev[j - 1] + 1;
            else
                curr[j] = std::max(prev[j], curr[j - 1]);
        }
        std::swap(prev, curr);
        std::ranges::fill(curr, size_t{0});
    }

    auto const lcsLength = prev[n];

    // Dice coefficient: 2 * |LCS| / (|A| + |B|)
    return 2.0 * static_cast<double>(lcsLength) / static_cast<double>(m + n);
}

auto CloneDetector::ComputeBlendedSimilarity(std::vector<NormalizedTokenId> const& structuralA,
                                             std::vector<NormalizedTokenId> const& structuralB,
                                             std::vector<NormalizedTokenId> const& textPreservingA,
                                             std::vector<NormalizedTokenId> const& textPreservingB,
                                             double textSensitivity) -> double
{
    auto const structuralSim = ComputeSimilarity(structuralA, structuralB);

    // Short-circuit: no text sensitivity or text-preserving IDs not available
    if (textSensitivity <= 0.0 || textPreservingA.empty() || textPreservingB.empty())
        return structuralSim;

    auto const textualSim = ComputeSimilarity(textPreservingA, textPreservingB);
    return (1.0 - textSensitivity) * structuralSim + textSensitivity * textualSim;
}

} // namespace codedup
