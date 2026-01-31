// SPDX-License-Identifier: Apache-2.0

#include <mcp/AnalysisSession.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>

using namespace mcp;

namespace
{

/// @brief Creates a unique temporary directory for testing (safe for concurrent CI).
struct TempTestDir
{
    std::filesystem::path root;

    TempTestDir()
    {
        static auto const seed = std::random_device{}();
        static std::atomic<unsigned> counter{0};
        root =
            std::filesystem::temp_directory_path() / std::format("mcp_session_test_{}_{}", seed, counter.fetch_add(1));
        std::filesystem::create_directories(root);
    }

    TempTestDir(TempTestDir const&) = delete;
    TempTestDir(TempTestDir&&) = delete;
    auto operator=(TempTestDir const&) -> TempTestDir& = delete;
    auto operator=(TempTestDir&&) -> TempTestDir& = delete;

    void WriteFile(std::string const& name, std::string const& content) const
    {
        std::ofstream out(root / name);
        out << content;
    }

    ~TempTestDir() { std::filesystem::remove_all(root); }
};

auto constexpr kDuplicateSource = R"(
void functionA(int x) {
    int result = 0;
    for (int i = 0; i < x; ++i) {
        result += i * 2;
        if (result > 100) {
            result = 100;
        }
    }
    return;
}

void functionB(int y) {
    int result = 0;
    for (int i = 0; i < y; ++i) {
        result += i * 2;
        if (result > 100) {
            result = 100;
        }
    }
    return;
}
)";

} // namespace

// ---------------------------------------------------------------------------
// AnalysisSession tests
// ---------------------------------------------------------------------------

TEST_CASE("AnalysisSession.InitiallyNoResults", "[mcp][session]")
{
    AnalysisSession session;
    CHECK_FALSE(session.HasResults());
}

TEST_CASE("AnalysisSession.AnalyzeNonExistentDirectory", "[mcp][session]")
{
    AnalysisSession session;
    AnalysisConfig config;
    config.directory = "/nonexistent/path/xyzzy";
    auto const result = session.Analyze(config);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("AnalysisSession.AnalyzeValidDirectory", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    auto const result = session.Analyze(config);
    REQUIRE(result.has_value());
    CHECK(session.HasResults());
    CHECK(session.Files().size() == 1);
    CHECK(session.AllBlocks().size() >= 2);
}

TEST_CASE("AnalysisSession.DetectsClones", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("dup.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    config.threshold = 0.70;
    config.minTokens = 10;
    config.textSensitivity = 0.0;
    auto const result = session.Analyze(config);
    REQUIRE(result.has_value());
    CHECK_FALSE(session.CloneGroups().empty());
}

TEST_CASE("AnalysisSession.ReconfigureWithoutAnalysis", "[mcp][session]")
{
    AnalysisSession session;
    auto const result = session.Reconfigure(0.90, 20, 0.0, dude::AnalysisScope::All);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("AnalysisSession.ReconfigureAfterAnalysis", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    config.threshold = 0.70;
    config.minTokens = 10;
    REQUIRE(session.Analyze(config).has_value());

    auto const originalGroups = session.CloneGroups().size();
    auto const result = session.Reconfigure(0.99, 10, 0.0, dude::AnalysisScope::All);
    REQUIRE(result.has_value());
    // With a very high threshold, fewer or equal groups should remain
    CHECK(session.CloneGroups().size() <= originalGroups);
}

TEST_CASE("AnalysisSession.ReadBlockSource", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("src.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    config.minTokens = 10;
    REQUIRE(session.Analyze(config).has_value());
    REQUIRE(!session.AllBlocks().empty());

    auto const source = session.ReadBlockSource(0);
    REQUIRE(source.has_value());
    CHECK(!source->empty());
}

TEST_CASE("AnalysisSession.ReadBlockSourceOutOfRange", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("src.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    REQUIRE(session.Analyze(config).has_value());

    auto const source = session.ReadBlockSource(99999);
    CHECK_FALSE(source.has_value());
}

TEST_CASE("AnalysisSession.TimingIsPopulated", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    REQUIRE(session.Analyze(config).has_value());

    auto const& timing = session.Timing();
    CHECK(timing.scanning.count() > 0);
    CHECK(timing.tokenizing.count() > 0);
    CHECK(timing.Total().count() > 0);
}

// ---------------------------------------------------------------------------
// Coverage: accessor methods (Config, AllTokens, IntraResults, BlockToFileIndex)
// ---------------------------------------------------------------------------

TEST_CASE("AnalysisSession.Accessors", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    REQUIRE(session.Analyze(config).has_value());

    // Exercise all accessor methods
    auto const& cfg = session.Config();
    CHECK(cfg.directory == dir.root);

    auto const& allTokens = session.AllTokens();
    CHECK_FALSE(allTokens.empty());

    auto const& intraResults = session.IntraResults();
    // IntraResults may or may not be empty depending on the source
    (void)intraResults;

    auto const& blockToFileIndex = session.BlockToFileIndex();
    CHECK_FALSE(blockToFileIndex.empty());
}

// ---------------------------------------------------------------------------
// Coverage: glob patterns in Analyze config
// ---------------------------------------------------------------------------

TEST_CASE("AnalysisSession.AnalyzeWithGlobPatterns", "[mcp][session]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);
    dir.WriteFile("test.py", "def foo():\n    pass\n");

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    config.globPatterns = {"*.cpp"};
    REQUIRE(session.Analyze(config).has_value());
    CHECK(session.HasResults());
}

// ---------------------------------------------------------------------------
// Coverage: Reconfigure without prior analysis should fail
// ---------------------------------------------------------------------------

TEST_CASE("AnalysisSession.ReconfigureWithoutAnalysisDefaults", "[mcp][session]")
{
    AnalysisSession session;
    auto result = session.Reconfigure(0.8, 30, 0.3, dude::AnalysisScope::All);
    CHECK_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Coverage: clone group sorting (lines 158-166) and intra-result sorting (lines 183-193)
// ---------------------------------------------------------------------------

TEST_CASE("AnalysisSession.AnalyzeWithMultipleGroupsAndIntra", "[mcp][session]")
{
    TempTestDir dir;
    // Create a file with multiple function pairs that form different clone groups,
    // and a function with internal duplication for intra-function detection.
    dir.WriteFile("multi.cpp", R"(
void funcA(int x) {
    int a = x + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
}
void funcB(int y) {
    int a = y + 1;
    int b = a * 2;
    int c = b - 3;
    int d = c + 4;
    int e = d * 5;
}
void funcC(int z) {
    int p = z * 10;
    int q = p + 20;
    int r = q - 30;
}
void funcD(int w) {
    int p = w * 10;
    int q = p + 20;
    int r = q - 30;
}
void bigFunc(int n) {
    int a1 = n + 1;
    int b1 = a1 * 2;
    int c1 = b1 - 3;
    int d1 = c1 + 4;
    int e1 = d1 * 5;
    int f1 = e1 - 6;
    int a2 = n + 1;
    int b2 = a2 * 2;
    int c2 = b2 - 3;
    int d2 = c2 + 4;
    int e2 = d2 * 5;
    int f2 = e2 - 6;
}
)");

    AnalysisSession session;
    AnalysisConfig config;
    config.directory = dir.root;
    config.minTokens = 5;
    config.threshold = 0.70;
    config.scope = dude::AnalysisScope::All;
    REQUIRE(session.Analyze(config).has_value());

    // Should have multiple clone groups (triggers sorting lambda)
    CHECK(session.CloneGroups().size() >= 1);
    // The groups should be sorted by token count descending
    auto const& groups = session.CloneGroups();
    for (size_t i = 1; i < groups.size(); ++i)
    {
        auto const& blocks = session.AllBlocks();
        auto tokensA =
            blocks[groups[i - 1].blockIndices.front()].tokenEnd - blocks[groups[i - 1].blockIndices.front()].tokenStart;
        auto tokensB =
            blocks[groups[i].blockIndices.front()].tokenEnd - blocks[groups[i].blockIndices.front()].tokenStart;
        CHECK(tokensA >= tokensB);
    }
}
