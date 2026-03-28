// SPDX-License-Identifier: Apache-2.0

#include <dude/BlockCache.hpp>
#include <dude/ContentHash.hpp>
#include <dude/SourceLocation.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace
{

auto MakeTestBlock(std::string name, uint32_t startLine, uint32_t endLine,
                   std::vector<dude::NormalizedTokenId> normalizedIds) -> dude::CodeBlock
{
    dude::CodeBlock block;
    block.name = std::move(name);
    block.sourceRange.start.fileIndex = dude::NoFileIndex;
    block.sourceRange.start.line = startLine;
    block.sourceRange.start.column = 1;
    block.sourceRange.end.fileIndex = dude::NoFileIndex;
    block.sourceRange.end.line = endLine;
    block.sourceRange.end.column = 1;
    block.tokenStart = 0;
    block.tokenEnd = normalizedIds.size();
    block.normalizedIds = std::move(normalizedIds);
    return block;
}

// NOLINTBEGIN(cppcoreguidelines-special-member-functions)
struct TempCacheFile
{
    std::filesystem::path path;

    TempCacheFile() : path(std::filesystem::temp_directory_path() / "dude-test-cache" / "blocks.json") {}

    ~TempCacheFile()
    {
        std::error_code ec;
        std::filesystem::remove_all(path.parent_path(), ec);
    }
};
// NOLINTEND(cppcoreguidelines-special-member-functions)

} // namespace

TEST_CASE("BlockCache.RoundTrip", "[BlockCache]")
{
    TempCacheFile tmp;

    auto const hash = dude::ComputeContentHash("test file content");
    auto blocks = std::vector{MakeTestBlock("foo", 10, 20, {1000, 42, 1000, 43})};

    {
        dude::BlockCache cache(tmp.path);
        cache.Store(hash, "C++", 300, 0.3, blocks);
        REQUIRE(cache.Size() == 1);
        auto const saveResult = cache.Save();
        REQUIRE(saveResult.has_value());
    }

    {
        dude::BlockCache cache(tmp.path);
        auto const loadResult = cache.Load();
        REQUIRE(loadResult.has_value());
        REQUIRE(cache.Size() == 1);

        auto result = cache.Lookup(hash, "C++", 300, 0.3);
        REQUIRE(result.has_value());
        auto const cachedBlocks = result.value_or(std::vector<dude::CodeBlock>{});
        REQUIRE(cachedBlocks.size() == 1);
        CHECK(cachedBlocks[0].name == "foo");
        CHECK(cachedBlocks[0].sourceRange.start.line == 10);
        CHECK(cachedBlocks[0].sourceRange.end.line == 20);
        CHECK(cachedBlocks[0].normalizedIds == std::vector<dude::NormalizedTokenId>{1000, 42, 1000, 43});
    }
}

TEST_CASE("BlockCache.CacheMissDifferentHash", "[BlockCache]")
{
    TempCacheFile tmp;
    dude::BlockCache cache(tmp.path);

    auto const hash = dude::ComputeContentHash("content A");
    cache.Store(hash, "C++", 300, 0.3, {MakeTestBlock("foo", 1, 10, {1000})});

    auto const otherHash = dude::ComputeContentHash("content B");
    CHECK_FALSE(cache.Lookup(otherHash, "C++", 300, 0.3).has_value());
}

TEST_CASE("BlockCache.CacheMissDifferentParams", "[BlockCache]")
{
    TempCacheFile tmp;
    dude::BlockCache cache(tmp.path);

    auto const hash = dude::ComputeContentHash("content");
    cache.Store(hash, "C++", 300, 0.3, {MakeTestBlock("foo", 1, 10, {1000})});

    // Different minTokens
    CHECK_FALSE(cache.Lookup(hash, "C++", 100, 0.3).has_value());
    // Different textSensitivity
    CHECK_FALSE(cache.Lookup(hash, "C++", 300, 0.5).has_value());
    // Different language
    CHECK_FALSE(cache.Lookup(hash, "Python", 300, 0.3).has_value());
}

TEST_CASE("BlockCache.EmptyCacheLoad", "[BlockCache]")
{
    auto const path = std::filesystem::temp_directory_path() / "dude-test-nonexistent" / "blocks.json";
    dude::BlockCache cache(path);
    auto const result = cache.Load();
    CHECK(result.has_value());
    CHECK(cache.Size() == 0);
}

TEST_CASE("BlockCache.CorruptFile", "[BlockCache]")
{
    TempCacheFile tmp;

    std::filesystem::create_directories(tmp.path.parent_path());
    {
        std::ofstream file(tmp.path);
        file << "this is not valid JSON{{{";
    }

    dude::BlockCache cache(tmp.path);
    auto const result = cache.Load();
    CHECK_FALSE(result.has_value());
}

TEST_CASE("BlockCache.VersionMismatch", "[BlockCache]")
{
    TempCacheFile tmp;

    std::filesystem::create_directories(tmp.path.parent_path());
    {
        std::ofstream file(tmp.path);
        file << R"({"version": 999, "entries": {}})";
    }

    dude::BlockCache cache(tmp.path);
    auto const result = cache.Load();
    CHECK(result.has_value());
    CHECK(cache.Size() == 0);
}

TEST_CASE("BlockCache.Clear", "[BlockCache]")
{
    TempCacheFile tmp;
    dude::BlockCache cache(tmp.path);

    cache.Store("hash1", "C++", 300, 0.3, {MakeTestBlock("foo", 1, 10, {1000})});
    REQUIRE(cache.Size() == 1);
    cache.Clear();
    CHECK(cache.Size() == 0);
}

TEST_CASE("BlockCache.FileIndexIsSentinel", "[BlockCache]")
{
    TempCacheFile tmp;
    dude::BlockCache cache(tmp.path);

    auto block = MakeTestBlock("bar", 5, 15, {42, 43, 44});
    block.sourceRange.start.fileIndex = 7; // Simulate a non-sentinel value
    cache.Store("hash", "C++", 300, 0.3, {block});
    REQUIRE(cache.Save().has_value());

    dude::BlockCache cache2(tmp.path);
    REQUIRE(cache2.Load().has_value());
    auto result = cache2.Lookup("hash", "C++", 300, 0.3);
    REQUIRE(result.has_value());
    auto const loadedBlocks = result.value_or(std::vector<dude::CodeBlock>{});
    REQUIRE(loadedBlocks.size() == 1);
    // After load, fileIndex should always be NoFileIndex
    CHECK(loadedBlocks[0].sourceRange.start.fileIndex == dude::NoFileIndex);
    CHECK(loadedBlocks[0].sourceRange.end.fileIndex == dude::NoFileIndex);
}
