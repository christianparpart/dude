// SPDX-License-Identifier: Apache-2.0

#include <tests/TempTestDir.hpp>

#include <dude/BaselineStore.hpp>
#include <dude/SourceLocation.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct BlockParams
{
    uint32_t fileIndex;
    uint32_t startLine;
    uint32_t endLine;
};

auto MakeBlock(std::string name, BlockParams params) -> dude::CodeBlock
{
    dude::CodeBlock block;
    block.name = std::move(name);
    block.sourceRange.start.fileIndex = params.fileIndex;
    block.sourceRange.start.line = params.startLine;
    block.sourceRange.start.column = 1;
    block.sourceRange.end.fileIndex = params.fileIndex;
    block.sourceRange.end.line = params.endLine;
    block.sourceRange.end.column = 1;
    block.normalizedIds = {1000, 42, 1000};
    return block;
}

} // namespace

TEST_CASE("BaselineStore.SaveLoadRoundTrip", "[BaselineStore]")
{
    test_utils::TempTestDir tmp("dude_baseline_test");

    auto const projectRoot = std::filesystem::path("/project");
    auto const files = std::vector<std::filesystem::path>{"/project/src/foo.cpp", "/project/src/bar.cpp"};

    auto const blocks = std::vector{
        MakeBlock("processData", {.fileIndex = 0, .startLine = 10, .endLine = 25}),
        MakeBlock("handleValue", {.fileIndex = 1, .startLine = 5, .endLine = 20}),
    };

    std::vector<dude::CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.95},
    };

    dude::BaselineStore store(tmp.Path());
    auto const saveResult = store.Save("v1", groups, {}, blocks, files, projectRoot);
    REQUIRE(saveResult.has_value());
    CHECK(store.Exists("v1"));

    auto const loadResult = store.Load("v1");
    REQUIRE(loadResult.has_value());
    CHECK(loadResult->name == "v1");
    REQUIRE(loadResult->cloneGroups.size() == 1);
    REQUIRE(loadResult->cloneGroups[0].members.size() == 2);
    CHECK(loadResult->cloneGroups[0].avgSimilarity == 0.95);
}

TEST_CASE("BaselineStore.BuildCloneIdentities", "[BaselineStore]")
{
    auto const projectRoot = std::filesystem::path("/project");
    auto const files = std::vector<std::filesystem::path>{"/project/src/a.cpp", "/project/src/b.cpp"};

    auto const blocks = std::vector{
        MakeBlock("funcA", {.fileIndex = 0, .startLine = 10, .endLine = 20}),
        MakeBlock("funcB", {.fileIndex = 1, .startLine = 30, .endLine = 40}),
    };

    std::vector<dude::CloneGroup> groups = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.92},
    };

    auto const identities = dude::BaselineStore::BuildCloneIdentities(groups, blocks, files, projectRoot);
    REQUIRE(identities.size() == 1);
    REQUIRE(identities[0].members.size() == 2);
    CHECK(identities[0].avgSimilarity == 0.92);

    // Members should be sorted
    CHECK(identities[0].members[0].filePath <= identities[0].members[1].filePath);
}

TEST_CASE("BaselineStore.FindNewCloneGroups", "[BaselineStore]")
{
    auto const projectRoot = std::filesystem::path("/project");
    auto const files = std::vector<std::filesystem::path>{"/project/a.cpp", "/project/b.cpp", "/project/c.cpp"};

    auto const blocks = std::vector{
        MakeBlock("funcA", {.fileIndex = 0, .startLine = 10, .endLine = 20}),
        MakeBlock("funcB", {.fileIndex = 1, .startLine = 30, .endLine = 40}),
        MakeBlock("funcC", {.fileIndex = 2, .startLine = 50, .endLine = 60}),
    };

    // Baseline has group (A, B)
    dude::Baseline baseline;
    baseline.cloneGroups.push_back(dude::CloneIdentity{
        .members =
            {
                dude::CloneGroupMember{.filePath = "a.cpp", .functionName = "funcA", .startLine = 10, .endLine = 20},
                dude::CloneGroupMember{.filePath = "b.cpp", .functionName = "funcB", .startLine = 30, .endLine = 40},
            },
        .avgSimilarity = 0.92,
    });

    // Current has groups (A, B) and (B, C)
    std::vector<dude::CloneGroup> current = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.92},
        {.blockIndices = {1, 2}, .avgSimilarity = 0.88},
    };

    auto const newGroups = dude::BaselineStore::FindNewCloneGroups(current, baseline, blocks, files, projectRoot);

    // Only (B, C) should be new
    REQUIRE(newGroups.size() == 1);
    CHECK(newGroups[0].avgSimilarity == 0.88);
}

TEST_CASE("BaselineStore.FindNewCloneGroupsAllNew", "[BaselineStore]")
{
    auto const projectRoot = std::filesystem::path("/project");
    auto const files = std::vector<std::filesystem::path>{"/project/a.cpp", "/project/b.cpp"};

    auto const blocks = std::vector{
        MakeBlock("funcA", {.fileIndex = 0, .startLine = 10, .endLine = 20}),
        MakeBlock("funcB", {.fileIndex = 1, .startLine = 30, .endLine = 40}),
    };

    dude::Baseline baseline; // Empty baseline

    std::vector<dude::CloneGroup> current = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.92},
    };

    auto const newGroups = dude::BaselineStore::FindNewCloneGroups(current, baseline, blocks, files, projectRoot);
    CHECK(newGroups.size() == 1);
}

TEST_CASE("BaselineStore.FindNewCloneGroupsNoneNew", "[BaselineStore]")
{
    auto const projectRoot = std::filesystem::path("/project");
    auto const files = std::vector<std::filesystem::path>{"/project/a.cpp", "/project/b.cpp"};

    auto const blocks = std::vector{
        MakeBlock("funcA", {.fileIndex = 0, .startLine = 10, .endLine = 20}),
        MakeBlock("funcB", {.fileIndex = 1, .startLine = 30, .endLine = 40}),
    };

    dude::Baseline baseline;
    baseline.cloneGroups.push_back(dude::CloneIdentity{
        .members =
            {
                dude::CloneGroupMember{.filePath = "a.cpp", .functionName = "funcA", .startLine = 10, .endLine = 20},
                dude::CloneGroupMember{.filePath = "b.cpp", .functionName = "funcB", .startLine = 30, .endLine = 40},
            },
        .avgSimilarity = 0.92,
    });

    std::vector<dude::CloneGroup> current = {
        {.blockIndices = {0, 1}, .avgSimilarity = 0.92},
    };

    auto const newGroups = dude::BaselineStore::FindNewCloneGroups(current, baseline, blocks, files, projectRoot);
    CHECK(newGroups.empty());
}

TEST_CASE("BaselineStore.NonexistentBaseline", "[BaselineStore]")
{
    test_utils::TempTestDir tmp("dude_baseline_test");
    dude::BaselineStore store(tmp.Path());
    CHECK_FALSE(store.Exists("nonexistent"));
    auto const result = store.Load("nonexistent");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("BaselineStore.List", "[BaselineStore]")
{
    test_utils::TempTestDir tmp("dude_baseline_test");
    auto const projectRoot = std::filesystem::path("/project");
    auto const files = std::vector<std::filesystem::path>{"/project/a.cpp"};
    auto const blocks = std::vector{MakeBlock("func", {.fileIndex = 0, .startLine = 1, .endLine = 10})};

    dude::BaselineStore store(tmp.Path());
    REQUIRE(store.Save("beta", {}, {}, blocks, files, projectRoot).has_value());
    REQUIRE(store.Save("alpha", {}, {}, blocks, files, projectRoot).has_value());

    auto const names = store.List();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "alpha");
    CHECK(names[1] == "beta");
}
