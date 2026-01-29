// SPDX-License-Identifier: Apache-2.0
#include <codedup/CloneDetector.hpp>
#include <codedup/CodeBlock.hpp>
#include <codedup/DiffFilter.hpp>
#include <codedup/DiffRange.hpp>
#include <codedup/IntraFunctionDetector.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace codedup;

namespace
{

/// @brief Creates a minimal CodeBlock with the given file index and line range.
auto MakeTestBlock(uint32_t fileIndex, uint32_t startLine, uint32_t endLine, std::string const& name = "test")
    -> CodeBlock
{
    return CodeBlock{
        .name = name,
        .sourceRange = {.start = {.fileIndex = fileIndex, .line = startLine},
                        .end = {.fileIndex = fileIndex, .line = endLine}},
        .tokenStart = 0,
        .tokenEnd = 10,
        .normalizedIds = {},
        .textPreservingIds = {},
    };
}

} // namespace

TEST_CASE("DiffFilter.FindChangedBlocks.BasicOverlap", "[difffilter]")
{
    auto const projectRoot = std::filesystem::path("/project");
    std::vector<std::filesystem::path> const files = {
        "/project/src/foo.cpp",
        "/project/src/bar.cpp",
        "/project/src/baz.cpp",
    };
    std::vector<CodeBlock> blocks = {
        MakeTestBlock(0, 10, 30, "foo"),
        MakeTestBlock(1, 5, 15, "bar"),
        MakeTestBlock(2, 100, 200, "baz"),
    };

    DiffResult diff = {
        {.filePath = "src/foo.cpp", .changedRanges = {{.startLine = 20, .endLine = 25}}},
    };

    auto const changed = DiffFilter::FindChangedBlocks(blocks, diff, projectRoot, files);
    CHECK(changed.size() == 1);
    CHECK(changed.contains(0)); // foo overlaps
}

TEST_CASE("DiffFilter.FindChangedBlocks.MultipleFiles", "[difffilter]")
{
    auto const projectRoot = std::filesystem::path("/project");
    std::vector<std::filesystem::path> const files = {
        "/project/src/a.cpp",
        "/project/src/b.cpp",
        "/project/src/c.cpp",
    };
    std::vector<CodeBlock> blocks = {
        MakeTestBlock(0, 10, 30, "a"),
        MakeTestBlock(1, 5, 15, "b"),
        MakeTestBlock(2, 100, 200, "c"),
    };

    DiffResult diff = {
        {.filePath = "src/a.cpp", .changedRanges = {{.startLine = 25, .endLine = 35}}},
        {.filePath = "src/b.cpp", .changedRanges = {{.startLine = 10, .endLine = 12}}},
    };

    auto const changed = DiffFilter::FindChangedBlocks(blocks, diff, projectRoot, files);
    CHECK(changed.size() == 2);
    CHECK(changed.contains(0));
    CHECK(changed.contains(1));
    CHECK_FALSE(changed.contains(2));
}

TEST_CASE("DiffFilter.FindChangedBlocks.NoneChanged", "[difffilter]")
{
    auto const projectRoot = std::filesystem::path("/project");
    std::vector<std::filesystem::path> const files = {
        "/project/src/foo.cpp",
    };
    std::vector<CodeBlock> blocks = {
        MakeTestBlock(0, 10, 30, "foo"),
    };

    DiffResult diff = {
        {.filePath = "src/foo.cpp", .changedRanges = {{.startLine = 50, .endLine = 60}}},
    };

    auto const changed = DiffFilter::FindChangedBlocks(blocks, diff, projectRoot, files);
    CHECK(changed.empty());
}

TEST_CASE("DiffFilter.FindChangedBlocks.AllChanged", "[difffilter]")
{
    auto const projectRoot = std::filesystem::path("/project");
    std::vector<std::filesystem::path> const files = {
        "/project/src/foo.cpp",
    };
    std::vector<CodeBlock> blocks = {
        MakeTestBlock(0, 1, 100, "foo"),
        MakeTestBlock(0, 200, 300, "bar"),
    };

    DiffResult diff = {
        {.filePath = "src/foo.cpp",
         .changedRanges = {{.startLine = 50, .endLine = 55}, {.startLine = 250, .endLine = 260}}},
    };

    auto const changed = DiffFilter::FindChangedBlocks(blocks, diff, projectRoot, files);
    CHECK(changed.size() == 2);
}

TEST_CASE("DiffFilter.FilterCloneGroups.KeepGroupWithChangedBlock", "[difffilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1, 2}, .avgSimilarity = 0.95},
        {.blockIndices = {3, 4}, .avgSimilarity = 0.90},
    };

    std::unordered_set<size_t> changedBlocks = {1}; // Only block 1 is changed.

    auto const filtered = DiffFilter::FilterCloneGroups(groups, changedBlocks);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0].blockIndices == std::vector<size_t>{0, 1, 2});
}

TEST_CASE("DiffFilter.FilterCloneGroups.RemoveGroupWithNoChangedBlocks", "[difffilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.90},
    };

    std::unordered_set<size_t> changedBlocks = {5}; // No match.

    auto const filtered = DiffFilter::FilterCloneGroups(groups, changedBlocks);
    CHECK(filtered.empty());
}

TEST_CASE("DiffFilter.FilterCloneGroups.AllGroupsKept", "[difffilter]")
{
    std::vector<CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.95},
        {.blockIndices = {2, 3}, .avgSimilarity = 0.90},
    };

    std::unordered_set<size_t> changedBlocks = {0, 3};

    auto const filtered = DiffFilter::FilterCloneGroups(groups, changedBlocks);
    CHECK(filtered.size() == 2);
}

TEST_CASE("DiffFilter.FilterIntraResults.KeepChangedBlock", "[difffilter]")
{
    std::vector<IntraCloneResult> results = {
        {.blockIndex = 0, .pairs = {{.blockIndex = 0, .regionA = {}, .regionB = {}, .similarity = 0.9}}},
        {.blockIndex = 1, .pairs = {{.blockIndex = 1, .regionA = {}, .regionB = {}, .similarity = 0.9}}},
        {.blockIndex = 2, .pairs = {{.blockIndex = 2, .regionA = {}, .regionB = {}, .similarity = 0.9}}},
    };

    std::unordered_set<size_t> changedBlocks = {1};

    auto const filtered = DiffFilter::FilterIntraResults(results, changedBlocks);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0].blockIndex == 1);
}

TEST_CASE("DiffFilter.FilterIntraResults.NoneChanged", "[difffilter]")
{
    std::vector<IntraCloneResult> results = {
        {.blockIndex = 0, .pairs = {{.blockIndex = 0, .regionA = {}, .regionB = {}, .similarity = 0.9}}},
    };

    std::unordered_set<size_t> changedBlocks = {};

    auto const filtered = DiffFilter::FilterIntraResults(results, changedBlocks);
    CHECK(filtered.empty());
}
