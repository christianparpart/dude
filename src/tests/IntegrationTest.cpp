// SPDX-License-Identifier: Apache-2.0
#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/TokenNormalizer.hpp>
#include <codedup/Tokenizer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace codedup;

namespace
{

/// @brief Runs the full pipeline on source code strings and returns clone groups.
auto RunPipeline(std::vector<std::string> const& sources, double threshold = 0.80, size_t minTokens = 5)
    -> std::pair<std::vector<CodeBlock>, std::vector<CloneGroup>>
{
    TokenNormalizer normalizer;
    CodeBlockExtractor extractor({.minTokens = minTokens});

    std::vector<CodeBlock> allBlocks;

    for (auto const& source : sources)
    {
        auto tokens = Tokenizer::Tokenize(source);
        if (!tokens)
            continue;

        auto normalized = normalizer.Normalize(*tokens);
        auto blocks = extractor.Extract(*tokens, normalized);

        for (auto& block : blocks)
            allBlocks.push_back(std::move(block));
    }

    CloneDetector detector({.similarityThreshold = threshold, .minTokens = minTokens});
    auto groups = detector.Detect(allBlocks);

    return {std::move(allBlocks), std::move(groups)};
}

} // namespace

TEST_CASE("Integration.DuplicateFunctions", "[integration]")
{
    std::vector<std::string> sources = {
        R"cpp(
void processData(int input) {
    int result = input * 2;
    if (result > 100) {
        result = result - 50;
    }
    int final_val = result + 10;
    return;
}
)cpp",
        R"cpp(
void handleValue(int value) {
    int output = value * 2;
    if (output > 100) {
        output = output - 50;
    }
    int end_val = output + 10;
    return;
}
)cpp",
    };

    auto [blocks, groups] = RunPipeline(sources);

    REQUIRE(!blocks.empty());
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].blockIndices.size() == 2);
    CHECK_THAT(groups[0].avgSimilarity, Catch::Matchers::WithinAbs(1.0, 0.01));
}

TEST_CASE("Integration.NoDuplicates", "[integration]")
{
    std::vector<std::string> sources = {
        R"cpp(
void sort(int* arr, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (arr[j] < arr[i]) {
                int tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}
)cpp",
        R"cpp(
int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0;
    int b = 1;
    for (int i = 2; i <= n; ++i) {
        int c = a + b;
        a = b;
        b = c;
    }
    return b;
}
)cpp",
    };

    auto [blocks, groups] = RunPipeline(sources, 0.90);

    // These are structurally different functions — no clones expected
    CHECK(groups.empty());
}

TEST_CASE("Integration.MultipleCloneGroups", "[integration]")
{
    std::vector<std::string> sources = {
        // Clone group 1: processA and processB are clones
        R"cpp(
void processA(int x) {
    int a = x + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    return;
}
)cpp",
        R"cpp(
void processB(int y) {
    int p = y + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    return;
}
)cpp",
        // Clone group 2: calculateA and calculateB are clones (different from group 1)
        R"cpp(
double calculateA(double val) {
    if (val > 0) {
        val = val * val;
    }
    double result = val + 3.14;
    double adjusted = result / 2.0;
    return;
}
)cpp",
        R"cpp(
double calculateB(double num) {
    if (num > 0) {
        num = num * num;
    }
    double output = num + 3.14;
    double final_val = output / 2.0;
    return;
}
)cpp",
    };

    auto [blocks, groups] = RunPipeline(sources);
    CHECK(!groups.empty()); // At least one clone group
}

TEST_CASE("Integration.EndToEndPipeline", "[integration]")
{
    // Verify the full pipeline produces reasonable output for a known duplicate
    auto const* const source1 = R"cpp(
int add(int a, int b) {
    int result = a + b;
    if (result < 0) {
        result = 0;
    }
    return result;
}
)cpp";

    auto const* const source2 = R"cpp(
int sum(int x, int y) {
    int value = x + y;
    if (value < 0) {
        value = 0;
    }
    return value;
}
)cpp";

    auto tokens1 = Tokenizer::Tokenize(source1);
    auto tokens2 = Tokenizer::Tokenize(source2);
    REQUIRE(tokens1.has_value());
    REQUIRE(tokens2.has_value());

    TokenNormalizer n1;
    TokenNormalizer n2;
    auto norm1 = n1.Normalize(*tokens1);
    auto norm2 = n2.Normalize(*tokens2);

    // The normalized sequences should be identical (Type-2 clone)
    // Extract just the IDs and compare
    std::vector<NormalizedTokenId> ids1;
    ids1.reserve(norm1.size());
    std::vector<NormalizedTokenId> ids2;
    ids2.reserve(norm2.size());
    for (auto const& nt : norm1)
        ids1.push_back(nt.id);
    for (auto const& nt : norm2)
        ids2.push_back(nt.id);

    CHECK(ids1 == ids2);
}

// ============================================================================================
// Text Sensitivity Integration Tests
// ============================================================================================

namespace
{

/// @brief Runs the full pipeline with text sensitivity support.
auto RunPipelineWithTextSensitivity(std::vector<std::string> const& sources, double threshold = 0.80,
                                    size_t minTokens = 5, double textSensitivity = 0.3)
    -> std::pair<std::vector<CodeBlock>, std::vector<CloneGroup>>
{
    TokenNormalizer normalizer;
    CodeBlockExtractor extractor({.minTokens = minTokens});

    std::vector<CodeBlock> allBlocks;

    for (auto const& source : sources)
    {
        auto tokens = Tokenizer::Tokenize(source);
        if (!tokens)
            continue;

        auto normalized = normalizer.Normalize(*tokens);
        auto textPreserving = normalizer.NormalizeTextPreserving(*tokens);
        auto blocks = extractor.Extract(*tokens, normalized, textPreserving);

        for (auto& block : blocks)
            allBlocks.push_back(std::move(block));
    }

    CloneDetector detector({
        .similarityThreshold = threshold,
        .minTokens = minTokens,
        .textSensitivity = textSensitivity,
    });
    auto groups = detector.Detect(allBlocks);

    return {std::move(allBlocks), std::move(groups)};
}

} // namespace

TEST_CASE("Integration.TextSensitivity.RenamedClonesDropBelowThreshold", "[integration][blended]")
{
    // Two functions with identical structure but different names/variables
    std::vector<std::string> sources = {
        R"cpp(
void processData(int input) {
    int result = input * 2;
    if (result > 100) {
        result = result - 50;
    }
    int final_val = result + 10;
    return;
}
)cpp",
        R"cpp(
void handleValue(int value) {
    int output = value * 2;
    if (output > 100) {
        output = output - 50;
    }
    int end_val = output + 10;
    return;
}
)cpp",
    };

    // Without text sensitivity: clones detected at threshold 0.95
    auto [blocks0, groups0] = RunPipelineWithTextSensitivity(sources, 0.95, 5, 0.0);
    REQUIRE(groups0.size() == 1);
    CHECK_THAT(groups0[0].avgSimilarity, Catch::Matchers::WithinAbs(1.0, 0.01));

    // With text sensitivity 0.3: blended similarity drops below 0.95 for renamed clones
    auto [blocks1, groups1] = RunPipelineWithTextSensitivity(sources, 0.95, 5, 0.3);
    CHECK(groups1.empty());
}

TEST_CASE("Integration.TextSensitivity.IdenticalCodeStillDetected", "[integration][blended]")
{
    // Identical code (exact copies) should still be detected even with text sensitivity
    std::vector<std::string> sources = {
        R"cpp(
void processData(int input) {
    int result = input * 2;
    if (result > 100) {
        result = result - 50;
    }
    int final_val = result + 10;
    return;
}
)cpp",
        R"cpp(
void processData2(int input) {
    int result = input * 2;
    if (result > 100) {
        result = result - 50;
    }
    int final_val = result + 10;
    return;
}
)cpp",
    };

    // With text sensitivity: identical code still passes
    auto [blocks, groups] = RunPipelineWithTextSensitivity(sources, 0.95, 5, 0.5);
    REQUIRE(groups.size() == 1);
}

TEST_CASE("Integration.TextSensitivity.ZeroPreservesOldBehavior", "[integration][blended]")
{
    // textSensitivity=0 should produce the same results as the old pipeline
    std::vector<std::string> sources = {
        R"cpp(
void foo(int x) {
    int a = x + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    return;
}
)cpp",
        R"cpp(
void bar(int y) {
    int p = y + 1;
    int q = p * 2;
    int r = q - 3;
    int s = r + 4;
    return;
}
)cpp",
    };

    auto [blocksOld, groupsOld] = RunPipeline(sources, 0.80, 5);
    auto [blocksNew, groupsNew] = RunPipelineWithTextSensitivity(sources, 0.80, 5, 0.0);

    CHECK(groupsOld.size() == groupsNew.size());
    if (!groupsOld.empty() && !groupsNew.empty())
        CHECK_THAT(groupsOld[0].avgSimilarity, Catch::Matchers::WithinAbs(groupsNew[0].avgSimilarity, 0.01));
}
