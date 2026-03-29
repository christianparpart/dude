// SPDX-License-Identifier: Apache-2.0

#include <nlohmann/json.hpp>
#include <tests/TempTestDir.hpp>

#include <dude/BlockCache.hpp>
#include <dude/ContentHash.hpp>
#include <dude/SourceLocation.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <fstream>
#include <span>

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

} // namespace

TEST_CASE("BlockCache.RoundTrip", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    auto const hash = dude::ComputeContentHash("test file content");
    auto blocks = std::vector{MakeTestBlock("foo", 10, 20, {1000, 42, 1000, 43})};

    {
        dude::BlockCache cache(cachePath);
        cache.Store(hash, "C++", 300, 0.3, blocks);
        REQUIRE(cache.Size() == 1);
        auto const saveResult = cache.Save();
        REQUIRE(saveResult.has_value());
    }

    {
        dude::BlockCache cache(cachePath);
        auto const loadResult = cache.Load();
        REQUIRE(loadResult.has_value());
        REQUIRE(cache.Size() == 1);

        auto result = cache.Lookup(hash, "C++", 300, 0.3);
        REQUIRE(result.has_value());
        auto const cachedBlocks = result.value_or(std::span<dude::CodeBlock const>{});
        REQUIRE(cachedBlocks.size() == 1);
        CHECK(cachedBlocks[0].name == "foo");
        CHECK(cachedBlocks[0].sourceRange.start.line == 10);
        CHECK(cachedBlocks[0].sourceRange.end.line == 20);
        CHECK(cachedBlocks[0].normalizedIds == std::vector<dude::NormalizedTokenId>{1000, 42, 1000, 43});
    }
}

TEST_CASE("BlockCache.CacheMissDifferentHash", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    dude::BlockCache cache(tmp.Path() / "blocks.json");

    auto const hash = dude::ComputeContentHash("content A");
    cache.Store(hash, "C++", 300, 0.3, {MakeTestBlock("foo", 1, 10, {1000})});

    auto const otherHash = dude::ComputeContentHash("content B");
    CHECK_FALSE(cache.Lookup(otherHash, "C++", 300, 0.3).has_value());
}

TEST_CASE("BlockCache.CacheMissDifferentParams", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    dude::BlockCache cache(tmp.Path() / "blocks.json");

    auto const hash = dude::ComputeContentHash("content");
    cache.Store(hash, "C++", 300, 0.3, {MakeTestBlock("foo", 1, 10, {1000})});

    CHECK_FALSE(cache.Lookup(hash, "C++", 100, 0.3).has_value());
    CHECK_FALSE(cache.Lookup(hash, "C++", 300, 0.5).has_value());
    CHECK_FALSE(cache.Lookup(hash, "Python", 300, 0.3).has_value());
}

TEST_CASE("BlockCache.EmptyCacheLoad", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    dude::BlockCache cache(tmp.Path() / "nonexistent" / "blocks.json");
    auto const result = cache.Load();
    CHECK(result.has_value());
    CHECK(cache.Size() == 0);
}

TEST_CASE("BlockCache.CorruptFile", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    {
        std::ofstream file(cachePath);
        file << "this is not valid JSON{{{";
    }

    dude::BlockCache cache(cachePath);
    auto const result = cache.Load();
    CHECK_FALSE(result.has_value());
}

TEST_CASE("BlockCache.VersionMismatch", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    {
        std::ofstream file(cachePath);
        file << R"({"version": 999, "entries": {}})";
    }

    dude::BlockCache cache(cachePath);
    auto const result = cache.Load();
    CHECK(result.has_value());
    CHECK(cache.Size() == 0);
}

TEST_CASE("BlockCache.Clear", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    dude::BlockCache cache(tmp.Path() / "blocks.json");

    cache.Store("hash1", "C++", 300, 0.3, {MakeTestBlock("foo", 1, 10, {1000})});
    REQUIRE(cache.Size() == 1);
    cache.Clear();
    CHECK(cache.Size() == 0);
}

TEST_CASE("BlockCache.FileIndexIsSentinel", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";
    dude::BlockCache cache(cachePath);

    auto block = MakeTestBlock("bar", 5, 15, {42, 43, 44});
    block.sourceRange.start.fileIndex = 7;
    cache.Store("hash", "C++", 300, 0.3, {block});
    REQUIRE(cache.Save().has_value());

    dude::BlockCache cache2(cachePath);
    REQUIRE(cache2.Load().has_value());
    auto result = cache2.Lookup("hash", "C++", 300, 0.3);
    REQUIRE(result.has_value());
    auto const loadedBlocks = result.value_or(std::span<dude::CodeBlock const>{});
    REQUIRE(loadedBlocks.size() == 1);
    CHECK(loadedBlocks[0].sourceRange.start.fileIndex == dude::NoFileIndex);
    CHECK(loadedBlocks[0].sourceRange.end.fileIndex == dude::NoFileIndex);
}

TEST_CASE("BlockCache.EvictionByAge", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    // Store two entries and save.
    {
        dude::BlockCache cache(cachePath);
        cache.Store("fresh", "C++", 300, 0.3, {MakeTestBlock("freshFunc", 1, 10, {1000})});
        cache.Store("stale", "C++", 300, 0.3, {MakeTestBlock("staleFunc", 20, 30, {1001})});
        REQUIRE(cache.Save().has_value());
    }

    // Manually patch the cache file to make the "stale" entry old.
    {
        std::ifstream in(cachePath);
        nlohmann::json root;
        in >> root;
        in.close();

        // Set the stale entry's lastAccessed to 5 weeks ago.
        auto const fiveWeeksAgo =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count() -
            (5 * 7 * 24 * 3600);
        for (auto const& [key, value] : root["entries"].items())
        {
            if (key.starts_with("stale:"))
                root["entries"][key]["lastAccessed"] = fiveWeeksAgo;
        }

        std::ofstream out(cachePath);
        out << root.dump(2);
    }

    // Load with default maxAge (4 weeks) -- stale entry should be evicted.
    {
        dude::BlockCache cache(cachePath);
        REQUIRE(cache.Load().has_value());
        CHECK(cache.Size() == 1); // Only "fresh" survives
        CHECK(cache.Lookup("fresh", "C++", 300, 0.3).has_value());
        CHECK_FALSE(cache.Lookup("stale", "C++", 300, 0.3).has_value());
    }
}

TEST_CASE("BlockCache.NoEvictionWithLargeMaxAge", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    {
        dude::BlockCache cache(cachePath);
        cache.Store("entry", "C++", 300, 0.3, {MakeTestBlock("func", 1, 10, {1000})});
        REQUIRE(cache.Save().has_value());
    }

    // Load with a very large maxAge -- nothing should be evicted.
    {
        dude::BlockCache cache(cachePath, std::chrono::seconds{365 * 24 * 3600});
        REQUIRE(cache.Load().has_value());
        CHECK(cache.Size() == 1);
    }
}

TEST_CASE("BlockCache.TextPreservingIdsRoundTrip", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    auto block = MakeTestBlock("func", 1, 20, {1000, 42, 1000});
    block.textPreservingIds = {2001, 2002, 2003};

    {
        dude::BlockCache cache(cachePath);
        cache.Store("hash", "C++", 300, 0.3, {block});
        REQUIRE(cache.Save().has_value());
    }

    {
        dude::BlockCache cache(cachePath);
        REQUIRE(cache.Load().has_value());
        auto result = cache.Lookup("hash", "C++", 300, 0.3);
        REQUIRE(result.has_value());
        auto const loaded = result.value_or(std::span<dude::CodeBlock const>{});
        REQUIRE(loaded.size() == 1);
        CHECK(loaded[0].textPreservingIds == std::vector<dude::NormalizedTokenId>{2001, 2002, 2003});
    }
}

TEST_CASE("BlockCache.SaveSkipsWhenNotDirty", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    {
        dude::BlockCache cache(cachePath);
        cache.Store("hash", "C++", 300, 0.3, {MakeTestBlock("f", 1, 10, {42})});
        REQUIRE(cache.Save().has_value());
    }

    // Load and save again without any Store — should succeed immediately (no-op).
    {
        dude::BlockCache cache(cachePath);
        REQUIRE(cache.Load().has_value());
        // Lookup within the same epoch-day does NOT mark dirty.
        auto const hit = cache.Lookup("hash", "C++", 300, 0.3);
        CHECK(hit.has_value());
        REQUIRE(cache.Save().has_value()); // Should be a no-op since epoch-day didn't change
    }
}

TEST_CASE("BlockCache.MultipleBlocksPerEntry", "[BlockCache]")
{
    test_utils::TempTestDir tmp("dude_cache_test");
    auto const cachePath = tmp.Path() / "blocks.json";

    auto blocks = std::vector{
        MakeTestBlock("funcA", 1, 10, {1000, 42}),
        MakeTestBlock("funcB", 20, 30, {1001, 43}),
        MakeTestBlock("funcC", 40, 50, {1002, 44}),
    };

    {
        dude::BlockCache cache(cachePath);
        cache.Store("hash", "C++", 300, 0.3, blocks);
        REQUIRE(cache.Save().has_value());
    }

    {
        dude::BlockCache cache(cachePath);
        REQUIRE(cache.Load().has_value());
        auto result = cache.Lookup("hash", "C++", 300, 0.3);
        REQUIRE(result.has_value());
        auto const loaded = result.value_or(std::span<dude::CodeBlock const>{});
        REQUIRE(loaded.size() == 3);
        CHECK(loaded[0].name == "funcA");
        CHECK(loaded[1].name == "funcB");
        CHECK(loaded[2].name == "funcC");
    }
}
