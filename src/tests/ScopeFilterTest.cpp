// SPDX-License-Identifier: Apache-2.0

#include <codedup/AnalysisScope.hpp>
#include <codedup/CloneDetector.hpp>
#include <codedup/ScopeFilter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace codedup;

// ---------------------------------------------------------------------------
// Helper: block-to-file index for tests
// ---------------------------------------------------------------------------

// Blocks 0,1 in file 0; blocks 2,3 in file 1; block 4 in file 2.
static auto const TestBlockToFileIndex = std::vector<size_t>{0, 0, 1, 1, 2};

// ---------------------------------------------------------------------------
// InterFunction scope (both InterFile + IntraFile) passes through unchanged
// ---------------------------------------------------------------------------

TEST_CASE("ScopeFilter.InterFunction.PassesThrough", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 2}, .avgSimilarity = 0.90},
        {.blockIndices = {1, 3}, .avgSimilarity = 0.85},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::InterFunction);
    CHECK(filtered.size() == 2);
}

TEST_CASE("ScopeFilter.All.PassesThrough", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 2}, .avgSimilarity = 0.90},
    };

    // All includes InterFunction, so should pass through.
    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::All);
    CHECK(filtered.size() == 1);
}

// ---------------------------------------------------------------------------
// InterFile keeps cross-file groups, drops same-file groups
// ---------------------------------------------------------------------------

TEST_CASE("ScopeFilter.InterFile.KeepsCrossFile", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 2}, .avgSimilarity = 0.90}, // file 0 + file 1 = cross-file
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::InterFile);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0].blockIndices == std::vector<size_t>{0, 2});
}

TEST_CASE("ScopeFilter.InterFile.DropsSameFile", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.95}, // both in file 0 = same-file
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::InterFile);
    CHECK(filtered.empty());
}

TEST_CASE("ScopeFilter.InterFile.MixedGroups", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.95},    // same-file (dropped)
        {.blockIndices = {0, 2, 4}, .avgSimilarity = 0.85}, // cross-file (kept)
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::InterFile);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0].blockIndices == std::vector<size_t>{0, 2, 4});
}

// ---------------------------------------------------------------------------
// IntraFile splits groups by file, drops files with < 2 blocks
// ---------------------------------------------------------------------------

TEST_CASE("ScopeFilter.IntraFile.SplitsGroupByFile", "[scopefilter]")
{
    // Group with blocks from file 0 (0,1) and file 1 (2,3).
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1, 2, 3}, .avgSimilarity = 0.90},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::IntraFile);
    // Should produce two sub-groups: {0,1} from file 0 and {2,3} from file 1.
    REQUIRE(filtered.size() == 2);

    // Check that both sub-groups have 2 blocks each.
    for (auto const& g : filtered)
        CHECK(g.blockIndices.size() == 2);
}

TEST_CASE("ScopeFilter.IntraFile.DropsFilesWithSingleBlock", "[scopefilter]")
{
    // Group: block 0 (file 0), block 2 (file 1), block 4 (file 2) -- all different files.
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 2, 4}, .avgSimilarity = 0.85},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::IntraFile);
    CHECK(filtered.empty());
}

TEST_CASE("ScopeFilter.IntraFile.PreservesSimilarity", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.92},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::IntraFile);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0].avgSimilarity == 0.92);
}

// ---------------------------------------------------------------------------
// None scope returns empty
// ---------------------------------------------------------------------------

TEST_CASE("ScopeFilter.None.ReturnsEmpty", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 2}, .avgSimilarity = 0.90},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::None);
    CHECK(filtered.empty());
}

TEST_CASE("ScopeFilter.IntraFunctionOnly.ReturnsEmpty", "[scopefilter]")
{
    // IntraFunction alone has no inter-function bits set.
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 2}, .avgSimilarity = 0.90},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::IntraFunction);
    CHECK(filtered.empty());
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("ScopeFilter.EmptyInput", "[scopefilter]")
{
    std::vector<CloneGroup> groups;
    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::InterFile);
    CHECK(filtered.empty());
}

TEST_CASE("ScopeFilter.SingleBlockGroup", "[scopefilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0}, .avgSimilarity = 1.0},
    };

    auto const filtered = ScopeFilter::FilterCloneGroups(groups, TestBlockToFileIndex, AnalysisScope::InterFile);
    CHECK(filtered.empty());
}
