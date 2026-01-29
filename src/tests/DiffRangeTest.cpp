// SPDX-License-Identifier: Apache-2.0
#include <codedup/DiffRange.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace codedup;

TEST_CASE("DiffRange.OverlapsExactMatch", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 10, .endLine = 20};
    CHECK(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.OverlapsPartialStart", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 10, .endLine = 20};
    CHECK(Overlaps(changed, 5, 15));
}

TEST_CASE("DiffRange.OverlapsPartialEnd", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 10, .endLine = 20};
    CHECK(Overlaps(changed, 15, 25));
}

TEST_CASE("DiffRange.OverlapsContained", "[diffrange]")
{
    // Changed range is fully contained within the block.
    auto const changed = LineRange{.startLine = 12, .endLine = 18};
    CHECK(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.OverlapsContaining", "[diffrange]")
{
    // Changed range fully contains the block.
    auto const changed = LineRange{.startLine = 5, .endLine = 25};
    CHECK(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.OverlapsAdjacentStart", "[diffrange]")
{
    // Ranges touch at a single line.
    auto const changed = LineRange{.startLine = 10, .endLine = 10};
    CHECK(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.OverlapsAdjacentEnd", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 20, .endLine = 20};
    CHECK(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.NoOverlapBefore", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 1, .endLine = 9};
    CHECK_FALSE(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.NoOverlapAfter", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 21, .endLine = 30};
    CHECK_FALSE(Overlaps(changed, 10, 20));
}

TEST_CASE("DiffRange.SingleLineOverlap", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 15, .endLine = 15};
    CHECK(Overlaps(changed, 15, 15));
}

TEST_CASE("DiffRange.SingleLineNoOverlap", "[diffrange]")
{
    auto const changed = LineRange{.startLine = 14, .endLine = 14};
    CHECK_FALSE(Overlaps(changed, 15, 15));
}

TEST_CASE("DiffRange.FileHasChangesAtMatch", "[diffrange]")
{
    auto const changes = FileChanges{
        .filePath = "test.cpp",
        .changedRanges = {{.startLine = 5, .endLine = 10}, {.startLine = 50, .endLine = 60}},
    };

    // Block overlaps second range.
    CHECK(FileHasChangesAt(changes, 55, 70));
    // Block overlaps first range.
    CHECK(FileHasChangesAt(changes, 1, 7));
}

TEST_CASE("DiffRange.FileHasChangesAtNoMatch", "[diffrange]")
{
    auto const changes = FileChanges{
        .filePath = "test.cpp",
        .changedRanges = {{.startLine = 5, .endLine = 10}, {.startLine = 50, .endLine = 60}},
    };

    // Block is between the two ranges.
    CHECK_FALSE(FileHasChangesAt(changes, 20, 40));
}

TEST_CASE("DiffRange.FileHasChangesAtEmptyRanges", "[diffrange]")
{
    auto const changes = FileChanges{
        .filePath = "test.cpp",
        .changedRanges = {},
    };
    CHECK_FALSE(FileHasChangesAt(changes, 1, 100));
}
