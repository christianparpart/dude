// SPDX-License-Identifier: Apache-2.0

#include <mcp/AnalysisSession.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace mcp;

namespace
{

/// @brief Creates a temporary directory with C++ source files for testing.
struct TempTestDir
{
    std::filesystem::path root;

    TempTestDir()
    {
        root = std::filesystem::temp_directory_path() / "mcp_session_test";
        std::filesystem::remove_all(root);
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
