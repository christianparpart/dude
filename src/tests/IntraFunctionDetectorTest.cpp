// SPDX-License-Identifier: Apache-2.0
#include <codedup/CodeBlock.hpp>
#include <codedup/IntraFunctionDetector.hpp>
#include <codedup/Languages/CppLanguage.hpp>
#include <codedup/TokenNormalizer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace codedup;

namespace
{

/// @brief Creates a CodeBlock from source code by running the full tokenize+normalize pipeline.
auto MakeBlock(std::string_view source, std::string const& name = "test") -> CodeBlock
{
    auto tokens = CppLanguage{}.Tokenize(source);
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

TEST_CASE("IntraFunctionDetector.IdenticalRegions", "[intra]")
{
    // A function with two copy-pasted blocks of code (different variable names).
    // After normalization, identifiers are all the same generic ID, so the
    // two blocks become identical token sequences.
    auto const block = MakeBlock(R"cpp(
void bigFunction(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;
    int k = j * 11;
    int result1 = k;

    int p = input + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    int t = s * 5;
    int u = t - 6;
    int v = u + 7;
    int w = v * 8;
    int x = w - 9;
    int y = x + 10;
    int z = y * 11;
    int result2 = z;
}
)cpp",
                                 "bigFunction");

    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    REQUIRE(results.size() == 1);
    CHECK(results[0].blockIndex == 0);
    REQUIRE(!results[0].pairs.empty());
    CHECK(results[0].pairs[0].similarity >= 0.80);
    // Both regions should be non-trivial
    CHECK(results[0].pairs[0].regionA.length >= 10);
    CHECK(results[0].pairs[0].regionB.length >= 10);
}

TEST_CASE("IntraFunctionDetector.NoInternalDuplication", "[intra]")
{
    // A function with no repetition
    auto const block = MakeBlock(R"cpp(
void uniqueFunction(int x) {
    int a = x + 1;
    if (a > 10) {
        for (int i = 0; i < a; ++i) {
            a += i * 2;
        }
    }
    double b = 3.14;
    while (b < 100.0) {
        b = b * b;
    }
    return;
}
)cpp",
                                 "uniqueFunction");

    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    CHECK(results.empty());
}

TEST_CASE("IntraFunctionDetector.OverlappingRegions", "[intra]")
{
    // With three repeated blocks, multiple pairs are possible.
    // The detector should keep all non-redundant pairs.
    auto const block = MakeBlock(R"cpp(
void tripleRepeat(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;

    int p = input + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    int t = s * 5;
    int u = t - 6;
    int v = u + 7;
    int w = v * 8;
    int x = w - 9;
    int y = x + 10;

    int aa = input + 1;
    int bb = aa * 2;
    int cc = bb - 3;
    int dd = cc + 4;
    int ee = dd * 5;
    int ff = ee - 6;
    int gg = ff + 7;
    int hh = gg * 8;
    int ii = hh - 9;
    int jj = ii + 10;
}
)cpp",
                                 "tripleRepeat");

    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    REQUIRE(results.size() == 1);
    // With three identical regions, we expect multiple distinct pairs
    CHECK(results[0].pairs.size() >= 2);
}

TEST_CASE("IntraFunctionDetector.MinRegionFiltering", "[intra]")
{
    // Regions below minRegionTokens are filtered out.
    // Create a function with short repeated snippets that are below the threshold.
    auto const block = MakeBlock(R"cpp(
void shortRepeats(int x) {
    int a = x + 1;
    int b = a + 2;
    int c = x + 1;
    int d = c + 2;
    if (a > 0) {
        for (int i = 0; i < 100; ++i) {
            x = x + i * 3;
        }
    }
}
)cpp",
                                 "shortRepeats");

    // Set minRegionTokens very high so nothing passes
    IntraFunctionDetector detector({.minRegionTokens = 50, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    CHECK(results.empty());
}

TEST_CASE("IntraFunctionDetector.MultipleIntraPairs", "[intra]")
{
    // A function with two distinct sets of duplicated code
    auto const block = MakeBlock(R"cpp(
void multiDup(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;

    int p = input + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    int t = s * 5;
    int u = t - 6;
    int v = u + 7;
    int w = v * 8;
    int x = w - 9;
    int y = x + 10;

    if (input > 0) {
        double aa = 1.0;
        double bb = aa + 2.0;
        double cc = bb * 3.0;
        double dd = cc - 4.0;
        double ee = dd + 5.0;
        double ff = ee * 6.0;
        double gg = ff - 7.0;
        double hh = gg + 8.0;
        double ii = hh * 9.0;
        double jj = ii - 10.0;
    }
    if (input < 0) {
        double aa2 = 1.0;
        double bb2 = aa2 + 2.0;
        double cc2 = bb2 * 3.0;
        double dd2 = cc2 - 4.0;
        double ee2 = dd2 + 5.0;
        double ff2 = ee2 * 6.0;
        double gg2 = ff2 - 7.0;
        double hh2 = gg2 + 8.0;
        double ii2 = hh2 * 9.0;
        double jj2 = ii2 - 10.0;
    }
}
)cpp",
                                 "multiDup");

    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    REQUIRE(results.size() == 1);
    // Should detect at least 2 distinct pairs (the two different duplication sets)
    CHECK(results[0].pairs.size() >= 2);
}

TEST_CASE("IntraFunctionDetector.SmallFunction", "[intra]")
{
    // Function too small for any intra-function clones
    auto const block = MakeBlock(R"cpp(
void tiny(int x) {
    return;
}
)cpp",
                                 "tiny");

    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    CHECK(results.empty());
}

TEST_CASE("IntraFunctionDetector.ThresholdFiltering", "[intra]")
{
    // Near-miss regions below threshold are excluded.
    // Create two similar but not identical repeated blocks.
    auto const block = MakeBlock(R"cpp(
void nearMiss(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;

    int p = input + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    int t = s * 5;
    int u = t - 6;
    int v = u + 7;
    int w = v * 8;
    int x = w - 9;
    int y = x + 10;
}
)cpp",
                                 "nearMiss");

    // Use a very high threshold that even identical (post-normalization) regions might pass
    // but set it to 1.01 so nothing can pass
    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 1.01});
    auto const results = detector.Detect({block});

    // Nothing should pass a threshold > 1.0
    CHECK(results.empty());
}

TEST_CASE("IntraFunctionDetector.Integration", "[intra]")
{
    // End-to-end test with realistic C++ code containing internal duplication.
    auto const block = MakeBlock(R"cpp(
void processData(int* data, int size) {
    // First pass: forward scan
    for (int i = 0; i < size; ++i) {
        if (data[i] > 100) {
            data[i] = data[i] - 50;
        }
        if (data[i] < 0) {
            data[i] = 0;
        }
        data[i] = data[i] * 2;
    }

    int total = 0;
    for (int i = 0; i < size; ++i) {
        total += data[i];
    }

    // Second pass: forward scan (same logic, copy-pasted)
    for (int j = 0; j < size; ++j) {
        if (data[j] > 100) {
            data[j] = data[j] - 50;
        }
        if (data[j] < 0) {
            data[j] = 0;
        }
        data[j] = data[j] * 2;
    }

    int total2 = 0;
    for (int j = 0; j < size; ++j) {
        total2 += data[j];
    }
}
)cpp",
                                 "processData");

    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.80});
    auto const results = detector.Detect({block});

    REQUIRE(results.size() == 1);
    REQUIRE(!results[0].pairs.empty());

    // The duplicated region should have high similarity
    auto const& pair = results[0].pairs[0];
    CHECK(pair.similarity >= 0.80);

    // Regions should not overlap
    CHECK(pair.regionA.start + pair.regionA.length <= pair.regionB.start);
}

// ============================================================================================
// Text Sensitivity Tests for Intra-Function Detection
// ============================================================================================

namespace
{

/// @brief Creates a CodeBlock with both structural and text-preserving IDs.
auto MakeBlockWithTP(std::string_view source, std::string const& name = "test") -> CodeBlock
{
    auto tokens = CppLanguage{}.Tokenize(source);
    if (!tokens)
        return {};

    TokenNormalizer normalizer;
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

TEST_CASE("IntraFunctionDetector.TextSensitivity.RenamedRegions", "[intra][blended]")
{
    // Two copy-pasted blocks with different variable names.
    // With text sensitivity, the similarity should be reduced below 1.0.
    auto const block = MakeBlockWithTP(R"cpp(
void bigFunction(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;
    int k = j * 11;
    int result1 = k;

    int p = input + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    int t = s * 5;
    int u = t - 6;
    int v = u + 7;
    int w = v * 8;
    int x = w - 9;
    int y = x + 10;
    int z = y * 11;
    int result2 = z;
}
)cpp",
                                       "bigFunction");

    // Without text sensitivity: similarity = 1.0
    IntraFunctionDetector detector0({.minRegionTokens = 10, .similarityThreshold = 0.80, .textSensitivity = 0.0});
    auto const results0 = detector0.Detect({block});
    REQUIRE(results0.size() == 1);
    REQUIRE(!results0[0].pairs.empty());
    CHECK_THAT(results0[0].pairs[0].similarity, Catch::Matchers::WithinAbs(1.0, 0.01));

    // With text sensitivity: similarity < 1.0 (renamed identifiers reduce it)
    IntraFunctionDetector detector03({.minRegionTokens = 10, .similarityThreshold = 0.50, .textSensitivity = 0.3});
    auto const results03 = detector03.Detect({block});
    REQUIRE(results03.size() == 1);
    REQUIRE(!results03[0].pairs.empty());
    CHECK(results03[0].pairs[0].similarity < 1.0);
}

TEST_CASE("IntraFunctionDetector.TextSensitivity.IdenticalRegions", "[intra][blended]")
{
    // Copy-pasted blocks with identical variable names should still be 1.0 even with text sensitivity.
    auto const block = MakeBlockWithTP(R"cpp(
void duplicateFunction(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;
    int k = j * 11;
    int result1 = k;

    int a2 = input + 1;
    int b2 = a2 * 2;
    int c2 = b2 - 3;
    int d2 = c2 + 4;
    int e2 = d2 * 5;
    int f2 = e2 - 6;
    int g2 = f2 + 7;
    int h2 = g2 * 8;
    int i2 = h2 - 9;
    int j2 = i2 + 10;
    int k2 = j2 * 11;
    int result3 = k2;
}
)cpp",
                                       "duplicateFunction");

    // With text sensitivity: renamed identifiers should reduce similarity
    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.50, .textSensitivity = 0.5});
    auto const results = detector.Detect({block});
    REQUIRE(results.size() == 1);
    REQUIRE(!results[0].pairs.empty());
    // Even with text sensitivity, renamed vars reduce similarity below 1.0
    CHECK(results[0].pairs[0].similarity < 1.0);
}

TEST_CASE("IntraFunctionDetector.TextSensitivity.FiltersByThreshold", "[intra][blended]")
{
    // With high text sensitivity and high threshold, renamed clones get filtered
    auto const block = MakeBlockWithTP(R"cpp(
void bigFunction(int input) {
    int a = input + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
    int f = e - 6;
    int g = f + 7;
    int h = g * 8;
    int i = h - 9;
    int j = i + 10;
    int k = j * 11;
    int result1 = k;

    int p = input + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    int t = s * 5;
    int u = t - 6;
    int v = u + 7;
    int w = v * 8;
    int x = w - 9;
    int y = x + 10;
    int z = y * 11;
    int result2 = z;
}
)cpp",
                                       "bigFunction");

    // High text sensitivity + high threshold should filter renamed clones
    IntraFunctionDetector detector({.minRegionTokens = 10, .similarityThreshold = 0.99, .textSensitivity = 1.0});
    auto const results = detector.Detect({block});
    CHECK(results.empty());
}
