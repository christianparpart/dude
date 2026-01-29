// SPDX-License-Identifier: Apache-2.0
#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/TokenNormalizer.hpp>
#include <codedup/Tokenizer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <random>

using namespace codedup;

namespace
{

auto MakeBlock(std::string_view source, std::string const& name = "test") -> CodeBlock
{
    auto tokens = Tokenizer::Tokenize(source);
    if (!tokens)
        return {};

    TokenNormalizer normalizer;
    auto normalized = normalizer.Normalize(*tokens);

    std::vector<NormalizedTokenId> ids;
    ids.reserve(normalized.size());
    for (auto const& nt : normalized)
        ids.push_back(nt.id);

    return CodeBlock{
        .name = name,
        .sourceRange = {},
        .tokenStart = 0,
        .tokenEnd = tokens->size(),
        .normalizedIds = std::move(ids),
        .textPreservingIds = {},
    };
}

} // namespace

TEST_CASE("CloneDetector.IdenticalBlocks", "[detector]")
{
    auto const block1 = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const block2 = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo2");

    CloneDetector detector({.similarityThreshold = 0.80, .minTokens = 5});
    auto const groups = detector.Detect({block1, block2});

    REQUIRE(groups.size() == 1);
    CHECK_THAT(groups[0].avgSimilarity, Catch::Matchers::WithinAbs(1.0, 0.01));
}

TEST_CASE("CloneDetector.RenamedIdentifiers", "[detector]")
{
    // Same structure, different names = Type-2 clone
    auto const block1 = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const block2 = MakeBlock("void bar(int y) { int p = y + 1; int q = p * 2; int r = q - 3; return; }", "bar");

    CloneDetector detector({.similarityThreshold = 0.80, .minTokens = 5});
    auto const groups = detector.Detect({block1, block2});

    REQUIRE(groups.size() == 1);
    CHECK_THAT(groups[0].avgSimilarity, Catch::Matchers::WithinAbs(1.0, 0.01));
}

TEST_CASE("CloneDetector.DifferentBlocks", "[detector]")
{
    auto const block1 = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; return; }", "foo");
    auto const block2 =
        MakeBlock("void bar(double y) { if (y > 0) { for (int i = 0; i < 10; ++i) { y += i; } } }", "bar");

    CloneDetector detector({.similarityThreshold = 0.80, .minTokens = 5});
    auto const groups = detector.Detect({block1, block2});

    CHECK(groups.empty());
}

TEST_CASE("CloneDetector.ThreeWayGrouping", "[detector]")
{
    auto const block1 = MakeBlock("void f1(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "f1");
    auto const block2 = MakeBlock("void f2(int y) { int p = y + 1; int q = p * 2; int r = q - 3; return; }", "f2");
    auto const block3 = MakeBlock("void f3(int z) { int m = z + 1; int n = m * 2; int o = n - 3; return; }", "f3");

    CloneDetector detector({.similarityThreshold = 0.80, .minTokens = 5});
    auto const groups = detector.Detect({block1, block2, block3});

    REQUIRE(groups.size() == 1);
    CHECK(groups[0].blockIndices.size() == 3);
}

TEST_CASE("CloneDetector.ThresholdFiltering", "[detector]")
{
    // Partially similar blocks should be filtered by high threshold
    auto const block1 = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; return; }", "foo");
    auto const block2 =
        MakeBlock("void bar(int y) { int a = y + 1; if (a > 0) { return; } int b = a * 3; int c = b + 4; }", "bar");

    CloneDetector detector({.similarityThreshold = 0.99, .minTokens = 5});
    auto const groups = detector.Detect({block1, block2});

    CHECK(groups.empty());
}

TEST_CASE("CloneDetector.EmptyInput", "[detector]")
{
    CloneDetector detector;
    auto const groups = detector.Detect({});
    CHECK(groups.empty());
}

TEST_CASE("CloneDetector.SingleBlock", "[detector]")
{
    auto const block = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; return; }", "foo");

    CloneDetector detector;
    auto const groups = detector.Detect({block});
    CHECK(groups.empty());
}

// ============================================================================================
// Bit-Parallel LCS Correctness Tests
// ============================================================================================
// These tests verify that ComputeSimilarity (bit-parallel) matches ComputeSimilarityClassic
// (two-row DP) at boundary sizes that exercise different tiers of the implementation:
//   - n <= 64:  single uint64_t bitvector (Tier 1)
//   - 65-256:   BitVector256 with carry propagation (Tier 2)
//   - n > 256:  falls back to classic DP
// ============================================================================================

namespace
{

/// @brief Generates a sequence of NormalizedTokenId with values cycling through [1, alphabetSize].
auto MakeSequence(size_t length, uint32_t alphabetSize = 10) -> std::vector<NormalizedTokenId>
{
    std::vector<NormalizedTokenId> seq(length);
    for (size_t i = 0; i < length; ++i)
        seq[i] = static_cast<NormalizedTokenId>((i % alphabetSize) + 1);
    return seq;
}

/// @brief Generates a shuffled sequence of NormalizedTokenId using a deterministic PRNG.
auto MakeShuffledSequence(size_t length, uint32_t alphabetSize = 10, uint32_t seed = 42)
    -> std::vector<NormalizedTokenId>
{
    auto seq = MakeSequence(length, alphabetSize);
    std::mt19937 rng(seed);
    std::shuffle(seq.begin(), seq.end(), rng);
    return seq;
}

} // namespace

TEST_CASE("BitParallelLCS.EmptySequences", "[lcs]")
{
    std::vector<NormalizedTokenId> const empty;
    std::vector<NormalizedTokenId> const nonEmpty = {1, 2, 3};

    CHECK(CloneDetector::ComputeSimilarity(empty, empty) == 0.0);
    CHECK(CloneDetector::ComputeSimilarity(empty, nonEmpty) == 0.0);
    CHECK(CloneDetector::ComputeSimilarity(nonEmpty, empty) == 0.0);

    CHECK(CloneDetector::ComputeSimilarityClassic(empty, empty) == 0.0);
    CHECK(CloneDetector::ComputeSimilarityClassic(empty, nonEmpty) == 0.0);
    CHECK(CloneDetector::ComputeSimilarityClassic(nonEmpty, empty) == 0.0);
}

TEST_CASE("BitParallelLCS.SingleElement", "[lcs]")
{
    std::vector<NormalizedTokenId> const a = {1};
    std::vector<NormalizedTokenId> const b = {1};
    std::vector<NormalizedTokenId> const c = {2};

    auto const simAB = CloneDetector::ComputeSimilarity(a, b);
    auto const simABClassic = CloneDetector::ComputeSimilarityClassic(a, b);
    CHECK_THAT(simAB, Catch::Matchers::WithinAbs(simABClassic, 1e-10));
    CHECK_THAT(simAB, Catch::Matchers::WithinAbs(1.0, 1e-10));

    auto const simAC = CloneDetector::ComputeSimilarity(a, c);
    auto const simACClassic = CloneDetector::ComputeSimilarityClassic(a, c);
    CHECK_THAT(simAC, Catch::Matchers::WithinAbs(simACClassic, 1e-10));
    CHECK_THAT(simAC, Catch::Matchers::WithinAbs(0.0, 1e-10));
}

TEST_CASE("BitParallelLCS.IdenticalSequences", "[lcs]")
{
    // Test at boundary sizes: 1, 63, 64, 65, 127, 128, 256, 257
    for (auto const n :
         {size_t{1}, size_t{63}, size_t{64}, size_t{65}, size_t{127}, size_t{128}, size_t{256}, size_t{257}})
    {
        CAPTURE(n);
        auto const seq = MakeSequence(n);
        auto const sim = CloneDetector::ComputeSimilarity(seq, seq);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(seq, seq);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(1.0, 1e-10));
    }
}

TEST_CASE("BitParallelLCS.CompletelyDifferent", "[lcs]")
{
    // Sequences with no common elements
    for (auto const n : {size_t{1}, size_t{63}, size_t{64}, size_t{65}, size_t{128}, size_t{256}, size_t{257}})
    {
        CAPTURE(n);
        // a = [1, 2, ..., n], b = [n+1, n+2, ..., 2n] -- no overlap
        std::vector<NormalizedTokenId> a(n);
        std::vector<NormalizedTokenId> b(n);
        for (size_t i = 0; i < n; ++i)
        {
            a[i] = static_cast<NormalizedTokenId>(i + 1);
            b[i] = static_cast<NormalizedTokenId>(i + n + 1);
        }
        auto const sim = CloneDetector::ComputeSimilarity(a, b);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(a, b);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(0.0, 1e-10));
    }
}

TEST_CASE("BitParallelLCS.PartialOverlap", "[lcs]")
{
    // Test partial similarity at boundary sizes
    for (auto const n :
         {size_t{10}, size_t{63}, size_t{64}, size_t{65}, size_t{127}, size_t{128}, size_t{256}, size_t{257}})
    {
        CAPTURE(n);
        auto const a = MakeShuffledSequence(n, 10, 42);
        auto const b = MakeShuffledSequence(n, 10, 99);
        auto const sim = CloneDetector::ComputeSimilarity(a, b);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(a, b);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
    }
}

TEST_CASE("BitParallelLCS.AsymmetricLengths", "[lcs]")
{
    // Test with different-length sequences to verify proper swapping (shorter = bitvector)
    for (auto const [m, n] :
         {std::pair{size_t{10}, size_t{64}}, std::pair{size_t{64}, size_t{10}}, std::pair{size_t{100}, size_t{256}},
          std::pair{size_t{256}, size_t{100}}, std::pair{size_t{64}, size_t{65}}, std::pair{size_t{300}, size_t{50}}})
    {
        CAPTURE(m, n);
        auto const a = MakeShuffledSequence(m, 15, 42);
        auto const b = MakeShuffledSequence(n, 15, 99);
        auto const sim = CloneDetector::ComputeSimilarity(a, b);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(a, b);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
    }
}

TEST_CASE("BitParallelLCS.SubsequenceAtBoundary", "[lcs]")
{
    // a is a prefix of b -- LCS = a
    for (auto const n : {size_t{32}, size_t{64}, size_t{128}, size_t{256}})
    {
        CAPTURE(n);
        auto const full = MakeSequence(n * 2, 20);
        std::vector<NormalizedTokenId> const prefix(full.begin(), full.begin() + static_cast<ptrdiff_t>(n));
        auto const sim = CloneDetector::ComputeSimilarity(prefix, full);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(prefix, full);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
    }
}

// ============================================================================================
// Length Ratio Pre-filter Tests
// ============================================================================================

TEST_CASE("CloneDetector.LengthRatioPrefilter", "[detector]")
{
    // At threshold=0.95, blocks must be within ~5.13% of each other
    // maxDice = 2 * min(a,b) / (a+b)

    SECTION("Equal lengths always compatible")
    {
        CHECK(CloneDetector::LengthsCompatible(100, 100, 0.95));
        CHECK(CloneDetector::LengthsCompatible(100, 100, 1.00));
        CHECK(CloneDetector::LengthsCompatible(1, 1, 0.95));
    }

    SECTION("Zero-length sequences are never compatible")
    {
        CHECK_FALSE(CloneDetector::LengthsCompatible(0, 0, 0.80));
        CHECK_FALSE(CloneDetector::LengthsCompatible(0, 100, 0.80));
        CHECK_FALSE(CloneDetector::LengthsCompatible(100, 0, 0.80));
    }

    SECTION("Threshold 0.95 boundary values")
    {
        // 2 * 95 / (95 + 100) = 190 / 195 = 0.97435... >= 0.95 -> compatible
        CHECK(CloneDetector::LengthsCompatible(100, 95, 0.95));
        CHECK(CloneDetector::LengthsCompatible(95, 100, 0.95));

        // 2 * 90 / (90 + 100) = 180 / 190 = 0.94736... < 0.95 -> not compatible
        CHECK_FALSE(CloneDetector::LengthsCompatible(100, 90, 0.95));
        CHECK_FALSE(CloneDetector::LengthsCompatible(90, 100, 0.95));
    }

    SECTION("Threshold 0.80 allows wider length differences")
    {
        // 2 * 80 / (80 + 100) = 160 / 180 = 0.888... >= 0.80 -> compatible
        CHECK(CloneDetector::LengthsCompatible(100, 80, 0.80));

        // 2 * 60 / (60 + 100) = 120 / 160 = 0.75 < 0.80 -> not compatible
        CHECK_FALSE(CloneDetector::LengthsCompatible(100, 60, 0.80));
    }

    SECTION("Symmetry")
    {
        CHECK(CloneDetector::LengthsCompatible(200, 190, 0.95) == CloneDetector::LengthsCompatible(190, 200, 0.95));
        CHECK(CloneDetector::LengthsCompatible(500, 300, 0.80) == CloneDetector::LengthsCompatible(300, 500, 0.80));
    }
}

// ============================================================================================
// Dynamic-Width Bit-Parallel LCS Tests
// ============================================================================================

TEST_CASE("BitParallelLCS.DynamicWidth", "[lcs]")
{
    // Verify that ComputeSimilarity matches ComputeSimilarityClassic for sizes > 256,
    // which exercise the new dynamic-width bit-parallel path.
    for (auto const n : {size_t{300}, size_t{500}, size_t{1000}})
    {
        CAPTURE(n);
        auto const a = MakeShuffledSequence(n, 15, 42);
        auto const b = MakeShuffledSequence(n, 15, 99);
        auto const sim = CloneDetector::ComputeSimilarity(a, b);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(a, b);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
    }
}

TEST_CASE("BitParallelLCS.DynamicWidthIdentical", "[lcs]")
{
    // Identical sequences > 256 must produce similarity = 1.0
    for (auto const n : {size_t{300}, size_t{500}, size_t{1000}})
    {
        CAPTURE(n);
        auto const seq = MakeSequence(n);
        auto const sim = CloneDetector::ComputeSimilarity(seq, seq);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(1.0, 1e-10));
    }
}

TEST_CASE("BitParallelLCS.DynamicWidthAsymmetric", "[lcs]")
{
    // Asymmetric lengths where the shorter sequence > 256
    for (auto const [m, n] : {std::pair{size_t{300}, size_t{500}}, std::pair{size_t{500}, size_t{300}},
                              std::pair{size_t{400}, size_t{1000}}})
    {
        CAPTURE(m, n);
        auto const a = MakeShuffledSequence(m, 15, 42);
        auto const b = MakeShuffledSequence(n, 15, 99);
        auto const sim = CloneDetector::ComputeSimilarity(a, b);
        auto const simClassic = CloneDetector::ComputeSimilarityClassic(a, b);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(simClassic, 1e-10));
    }
}

// ============================================================================================
// Multi-threaded Phase 2 Consistency Test
// ============================================================================================

TEST_CASE("CloneDetector.MultithreadedConsistency", "[detector]")
{
    // Create 20 identical blocks -- they should all land in one clone group
    std::vector<CodeBlock> blocks;
    blocks.reserve(20);
    for (size_t i = 0; i < 20; ++i)
    {
        auto block =
            MakeBlock("void func(int x) { int a = x + 1; int b = a * 2; int c = b - 3; int d = c + 4; return; }",
                      "func" + std::to_string(i));
        blocks.push_back(std::move(block));
    }

    CloneDetector detector({.similarityThreshold = 0.80, .minTokens = 5});
    auto const groups = detector.Detect(blocks);

    REQUIRE(groups.size() == 1);
    CHECK(groups[0].blockIndices.size() == 20);
    CHECK(groups[0].avgSimilarity >= 0.99);
}

// ============================================================================================
// Multi-threaded Work Distribution Edge Case Tests
// ============================================================================================

TEST_CASE("CloneDetector.MultithreadedWorkDistributionEdgeCase", "[detector]")
{
    // Regression test: when candidateCount is slightly above 100 (the multithreading threshold)
    // and threadCount > candidateCount / chunkSize, the last threads would compute
    // start = threadIdx * chunkSize > candidateCount, causing start > end in std::views::iota.
    // This crashed with SIGABRT (debug) or SIGSEGV (release).
    //
    // We generate enough similar blocks to produce ~100-200 candidate pairs, which triggers
    // the multi-threaded path with some threads having empty work ranges.

    std::vector<CodeBlock> blocks;
    // 16 blocks with very similar structure produces C(16,2) = 120 candidate pairs
    for (size_t i = 0; i < 16; ++i)
    {
        auto block = MakeBlock("void func(int x) { int a = x + 1; int b = a * 2; int c = b - 3; int d = c + 4; "
                               "int e = d * 5; int f = e - 6; return; }",
                               "func" + std::to_string(i));
        blocks.push_back(std::move(block));
    }

    // This must not crash -- the fix clamps start to candidateCount so empty ranges are safe
    CloneDetector detector({.similarityThreshold = 0.80, .minTokens = 5});
    auto const groups = detector.Detect(blocks);

    // All 16 blocks are identical clones, so they should form one group
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].blockIndices.size() == 16);
}

// ============================================================================================
// Blended Similarity Tests
// ============================================================================================

namespace
{

/// @brief Creates a CodeBlock with both structural and text-preserving IDs.
///
/// A shared normalizer must be used across blocks being compared, so that different
/// identifier/literal texts get different IDs from the same dictionary.
auto MakeBlockWithTextPreserving(TokenNormalizer& normalizer, std::string_view source, std::string const& name = "test")
    -> CodeBlock
{
    auto tokens = Tokenizer::Tokenize(source);
    if (!tokens)
        return {};

    auto normalized = normalizer.Normalize(*tokens);
    auto textPreserving = normalizer.NormalizeTextPreserving(*tokens);

    std::vector<NormalizedTokenId> ids;
    ids.reserve(normalized.size());
    for (auto const& nt : normalized)
        ids.push_back(nt.id);

    std::vector<NormalizedTokenId> tpIds;
    tpIds.reserve(textPreserving.size());
    for (auto const& nt : textPreserving)
        tpIds.push_back(nt.id);

    return CodeBlock{
        .name = name,
        .sourceRange = {},
        .tokenStart = 0,
        .tokenEnd = tokens->size(),
        .normalizedIds = std::move(ids),
        .textPreservingIds = std::move(tpIds),
    };
}

} // namespace

TEST_CASE("CloneDetector.BlendedSimilarity.ZeroTextSensitivity", "[detector][blended]")
{
    // With textSensitivity = 0, blended similarity equals structural similarity
    TokenNormalizer normalizer;
    auto const a = MakeBlockWithTextPreserving(
        normalizer, "void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const b = MakeBlockWithTextPreserving(
        normalizer, "void bar(int y) { int p = y + 1; int q = p * 2; int r = q - 3; return; }", "bar");

    auto const structuralOnly = CloneDetector::ComputeBlendedSimilarity(a.normalizedIds, b.normalizedIds,
                                                                        a.textPreservingIds, b.textPreservingIds, 0.0);
    auto const pureStructural = CloneDetector::ComputeSimilarity(a.normalizedIds, b.normalizedIds);
    CHECK_THAT(structuralOnly, Catch::Matchers::WithinAbs(pureStructural, 1e-10));
}

TEST_CASE("CloneDetector.BlendedSimilarity.IdenticalCode", "[detector][blended]")
{
    // Identical code should give 1.0 regardless of text sensitivity
    TokenNormalizer normalizer;
    auto const a = MakeBlockWithTextPreserving(
        normalizer, "void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const b = MakeBlockWithTextPreserving(
        normalizer, "void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo2");

    for (auto const sensitivity : {0.0, 0.3, 0.5, 1.0})
    {
        CAPTURE(sensitivity);
        auto const sim = CloneDetector::ComputeBlendedSimilarity(a.normalizedIds, b.normalizedIds, a.textPreservingIds,
                                                                 b.textPreservingIds, sensitivity);
        CHECK_THAT(sim, Catch::Matchers::WithinAbs(1.0, 0.01));
    }
}

TEST_CASE("CloneDetector.BlendedSimilarity.RenamedClones", "[detector][blended]")
{
    // Renamed clones should have lower blended similarity with text sensitivity > 0
    TokenNormalizer normalizer;
    auto const a = MakeBlockWithTextPreserving(
        normalizer, "void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const b = MakeBlockWithTextPreserving(
        normalizer, "void bar(int y) { int p = y + 1; int q = p * 2; int r = q - 3; return; }", "bar");

    auto const sim0 = CloneDetector::ComputeBlendedSimilarity(a.normalizedIds, b.normalizedIds, a.textPreservingIds,
                                                              b.textPreservingIds, 0.0);
    auto const sim03 = CloneDetector::ComputeBlendedSimilarity(a.normalizedIds, b.normalizedIds, a.textPreservingIds,
                                                               b.textPreservingIds, 0.3);
    auto const sim10 = CloneDetector::ComputeBlendedSimilarity(a.normalizedIds, b.normalizedIds, a.textPreservingIds,
                                                               b.textPreservingIds, 1.0);

    // structural sim = 1.0, textual sim < 1.0 for renamed code
    CHECK_THAT(sim0, Catch::Matchers::WithinAbs(1.0, 0.01));
    CHECK(sim03 < sim0);  // Blended should be lower
    CHECK(sim10 < sim03); // More text sensitivity = even lower
    CHECK(sim10 < 1.0);   // Textual-only should be < 1.0
}

TEST_CASE("CloneDetector.BlendedSimilarity.EmptyTextPreserving", "[detector][blended]")
{
    // When text-preserving IDs are empty, falls back to structural similarity
    auto const a = MakeBlock("void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const b = MakeBlock("void bar(int y) { int p = y + 1; int q = p * 2; int r = q - 3; return; }", "bar");

    auto const sim = CloneDetector::ComputeBlendedSimilarity(a.normalizedIds, b.normalizedIds, a.textPreservingIds,
                                                             b.textPreservingIds, 0.5);
    auto const pureStructural = CloneDetector::ComputeSimilarity(a.normalizedIds, b.normalizedIds);
    CHECK_THAT(sim, Catch::Matchers::WithinAbs(pureStructural, 1e-10));
}

TEST_CASE("CloneDetector.BlendedSimilarity.DetectWithTextSensitivity", "[detector][blended]")
{
    // Renamed clones detected at textSensitivity=0 should be filtered at high sensitivity + threshold
    TokenNormalizer normalizer;
    auto const a = MakeBlockWithTextPreserving(
        normalizer, "void foo(int x) { int a = x + 1; int b = a * 2; int c = b - 3; return; }", "foo");
    auto const b = MakeBlockWithTextPreserving(
        normalizer, "void bar(int y) { int p = y + 1; int q = p * 2; int r = q - 3; return; }", "bar");

    // Without text sensitivity: clones detected
    CloneDetector detector0({.similarityThreshold = 0.95, .minTokens = 5, .textSensitivity = 0.0});
    auto const groups0 = detector0.Detect({a, b});
    CHECK(groups0.size() == 1);

    // With high text sensitivity: renamed clones filtered out
    CloneDetector detector1({.similarityThreshold = 0.95, .minTokens = 5, .textSensitivity = 0.5});
    auto const groups1 = detector1.Detect({a, b});
    CHECK(groups1.empty());
}

// ============================================================================================
// LCS Alignment Tests
// ============================================================================================

TEST_CASE("LcsAlignment.IdenticalSequences", "[lcs][alignment]")
{
    std::vector<NormalizedTokenId> const seq = {1, 2, 3, 4, 5};
    auto const alignment = CloneDetector::ComputeLcsAlignment(seq, seq);

    REQUIRE(alignment.matchedA.size() == 5);
    REQUIRE(alignment.matchedB.size() == 5);
    for (size_t i = 0; i < 5; ++i)
    {
        CHECK(alignment.matchedA[i]);
        CHECK(alignment.matchedB[i]);
    }
}

TEST_CASE("LcsAlignment.CompletelyDifferent", "[lcs][alignment]")
{
    std::vector<NormalizedTokenId> const a = {1, 2, 3};
    std::vector<NormalizedTokenId> const b = {4, 5, 6};
    auto const alignment = CloneDetector::ComputeLcsAlignment(a, b);

    REQUIRE(alignment.matchedA.size() == 3);
    REQUIRE(alignment.matchedB.size() == 3);
    for (size_t i = 0; i < 3; ++i)
    {
        CHECK_FALSE(alignment.matchedA[i]);
        CHECK_FALSE(alignment.matchedB[i]);
    }
}

TEST_CASE("LcsAlignment.PartialOverlap", "[lcs][alignment]")
{
    // LCS of {1,2,3,4} and {1,3,5,4} is {1,3,4} (length 3)
    std::vector<NormalizedTokenId> const a = {1, 2, 3, 4};
    std::vector<NormalizedTokenId> const b = {1, 3, 5, 4};
    auto const alignment = CloneDetector::ComputeLcsAlignment(a, b);

    REQUIRE(alignment.matchedA.size() == 4);
    REQUIRE(alignment.matchedB.size() == 4);

    // In A: 1 matched, 2 NOT matched, 3 matched, 4 matched
    CHECK(alignment.matchedA[0]);       // 1
    CHECK_FALSE(alignment.matchedA[1]); // 2
    CHECK(alignment.matchedA[2]);       // 3
    CHECK(alignment.matchedA[3]);       // 4

    // In B: 1 matched, 3 matched, 5 NOT matched, 4 matched
    CHECK(alignment.matchedB[0]);       // 1
    CHECK(alignment.matchedB[1]);       // 3
    CHECK_FALSE(alignment.matchedB[2]); // 5
    CHECK(alignment.matchedB[3]);       // 4
}

TEST_CASE("LcsAlignment.EmptySequences", "[lcs][alignment]")
{
    std::vector<NormalizedTokenId> const empty;
    std::vector<NormalizedTokenId> const nonEmpty = {1, 2, 3};

    auto const both = CloneDetector::ComputeLcsAlignment(empty, empty);
    CHECK(both.matchedA.empty());
    CHECK(both.matchedB.empty());

    auto const aEmpty = CloneDetector::ComputeLcsAlignment(empty, nonEmpty);
    CHECK(aEmpty.matchedA.empty());
    REQUIRE(aEmpty.matchedB.size() == 3);
    CHECK_FALSE(aEmpty.matchedB[0]);
    CHECK_FALSE(aEmpty.matchedB[1]);
    CHECK_FALSE(aEmpty.matchedB[2]);

    auto const bEmpty = CloneDetector::ComputeLcsAlignment(nonEmpty, empty);
    REQUIRE(bEmpty.matchedA.size() == 3);
    CHECK_FALSE(bEmpty.matchedA[0]);
    CHECK_FALSE(bEmpty.matchedA[1]);
    CHECK_FALSE(bEmpty.matchedA[2]);
    CHECK(bEmpty.matchedB.empty());
}

TEST_CASE("LcsAlignment.SingleElementMatch", "[lcs][alignment]")
{
    std::vector<NormalizedTokenId> const a = {42};
    std::vector<NormalizedTokenId> const b = {42};
    auto const alignment = CloneDetector::ComputeLcsAlignment(a, b);

    REQUIRE(alignment.matchedA.size() == 1);
    REQUIRE(alignment.matchedB.size() == 1);
    CHECK(alignment.matchedA[0]);
    CHECK(alignment.matchedB[0]);
}

TEST_CASE("LcsAlignment.SingleElementNoMatch", "[lcs][alignment]")
{
    std::vector<NormalizedTokenId> const a = {1};
    std::vector<NormalizedTokenId> const b = {2};
    auto const alignment = CloneDetector::ComputeLcsAlignment(a, b);

    REQUIRE(alignment.matchedA.size() == 1);
    REQUIRE(alignment.matchedB.size() == 1);
    CHECK_FALSE(alignment.matchedA[0]);
    CHECK_FALSE(alignment.matchedB[0]);
}

TEST_CASE("LcsAlignment.AsymmetricLengths", "[lcs][alignment]")
{
    // a = {1, 2, 3}, b = {1, 2, 3, 4, 5}
    // LCS = {1, 2, 3} (length 3). All of a is matched, only first 3 of b matched.
    std::vector<NormalizedTokenId> const a = {1, 2, 3};
    std::vector<NormalizedTokenId> const b = {1, 2, 3, 4, 5};
    auto const alignment = CloneDetector::ComputeLcsAlignment(a, b);

    REQUIRE(alignment.matchedA.size() == 3);
    REQUIRE(alignment.matchedB.size() == 5);

    CHECK(alignment.matchedA[0]);
    CHECK(alignment.matchedA[1]);
    CHECK(alignment.matchedA[2]);

    CHECK(alignment.matchedB[0]);
    CHECK(alignment.matchedB[1]);
    CHECK(alignment.matchedB[2]);
    CHECK_FALSE(alignment.matchedB[3]);
    CHECK_FALSE(alignment.matchedB[4]);
}
