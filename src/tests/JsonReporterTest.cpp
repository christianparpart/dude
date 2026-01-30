// SPDX-License-Identifier: Apache-2.0
#include <nlohmann/json.hpp>

#include <dude/IntraFunctionDetector.hpp>
#include <dude/JsonReporter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>

using namespace dude;

TEST_CASE("JsonReporter.EmptyOutput", "[reporter][json]")
{
    JsonReporter reporter;
    reporter.ReportSummary({.totalFiles = 0, .totalBlocks = 0, .totalGroups = 0});

    auto const output = reporter.Render();
    auto const json = nlohmann::json::parse(output);

    CHECK(json.contains("cloneGroups"));
    CHECK(json.contains("intraClones"));
    CHECK(json.contains("summary"));
    CHECK(json["cloneGroups"].empty());
    CHECK(json["intraClones"].empty());
    CHECK(json["summary"]["totalFiles"] == 0);
}

TEST_CASE("JsonReporter.SummaryFields", "[reporter][json]")
{
    JsonReporter reporter;
    reporter.ReportSummary({.totalFiles = 10,
                            .totalBlocks = 50,
                            .totalGroups = 3,
                            .totalIntraPairs = 2,
                            .totalDuplicatedLines = 120,
                            .totalFunctions = 6,
                            .totalIntraFunctions = 2});

    auto const output = reporter.Render();
    auto const json = nlohmann::json::parse(output);
    auto const& summary = json["summary"];

    CHECK(summary["totalFiles"] == 10);
    CHECK(summary["totalBlocks"] == 50);
    CHECK(summary["totalGroups"] == 3);
    CHECK(summary["totalIntraPairs"] == 2);
    CHECK(summary["totalDuplicatedLines"] == 120);
    CHECK(summary["totalFunctions"] == 6);
    CHECK(summary["totalIntraFunctions"] == 2);
}

TEST_CASE("JsonReporter.SummaryWithTiming", "[reporter][json]")
{
    JsonReporter reporter;

    PerformanceTiming timing;
    timing.scanning = std::chrono::milliseconds(15);
    timing.tokenizing = std::chrono::milliseconds(120);
    timing.normalizing = std::chrono::milliseconds(45);
    timing.cloneDetection = std::chrono::milliseconds(890);

    reporter.ReportSummary({.totalFiles = 100, .totalBlocks = 500, .totalGroups = 12, .timing = timing});

    auto const output = reporter.Render();
    auto const json = nlohmann::json::parse(output);
    auto const& summary = json["summary"];

    CHECK(summary.contains("timing"));
    CHECK(summary["timing"].contains("scanning"));
    CHECK(summary["timing"].contains("tokenizing"));
    CHECK(summary["timing"].contains("normalizing"));
    CHECK(summary["timing"].contains("cloneDetection"));
    CHECK(summary["timing"].contains("total"));
}

TEST_CASE("JsonReporter.SummaryWithScope", "[reporter][json]")
{
    JsonReporter reporter;
    reporter.ReportSummary(
        {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3, .activeScope = AnalysisScope::InterFile});

    auto const output = reporter.Render();
    auto const json = nlohmann::json::parse(output);

    CHECK(json["summary"].contains("scope"));
}

TEST_CASE("JsonReporter.CloneGroups", "[reporter][json]")
{
    JsonReporter reporter;

    std::vector<std::filesystem::path> files = {"a.cpp", "b.cpp"};

    CodeBlock blockA{
        .name = "funcA",
        .sourceRange = {.start = {.fileIndex = 0, .line = 1, .column = 1},
                        .end = {.fileIndex = 0, .line = 10, .column = 1}},
        .tokenStart = 0,
        .tokenEnd = 5,
        .normalizedIds = {1, 2, 3, 4, 5},
        .textPreservingIds = {},
    };
    CodeBlock blockB{
        .name = "funcB",
        .sourceRange = {.start = {.fileIndex = 1, .line = 5, .column = 1},
                        .end = {.fileIndex = 1, .line = 15, .column = 1}},
        .tokenStart = 0,
        .tokenEnd = 5,
        .normalizedIds = {1, 2, 3, 4, 5},
        .textPreservingIds = {},
    };

    CloneGroup group{.blockIndices = {0, 1}, .avgSimilarity = 0.95};

    std::vector<std::vector<Token>> allTokens;
    std::vector<size_t> blockToFile;

    reporter.Report({group}, {blockA, blockB}, allTokens, blockToFile, files);
    reporter.ReportSummary({.totalFiles = 2, .totalBlocks = 2, .totalGroups = 1});

    auto const output = reporter.Render();
    auto const json = nlohmann::json::parse(output);

    REQUIRE(json["cloneGroups"].size() == 1);
    auto const& g = json["cloneGroups"][0];
    CHECK(g["groupIndex"] == 1);
    CHECK(g["blockCount"] == 2);
    CHECK(g["avgSimilarity"].get<double>() > 0.94);

    REQUIRE(g["blocks"].size() == 2);
    CHECK(g["blocks"][0]["name"] == "funcA");
    CHECK(g["blocks"][0]["filePath"] == "a.cpp");
    CHECK(g["blocks"][0]["startLine"] == 1);
    CHECK(g["blocks"][0]["endLine"] == 10);
    CHECK(g["blocks"][1]["name"] == "funcB");
    CHECK(g["blocks"][1]["filePath"] == "b.cpp");
}

TEST_CASE("JsonReporter.IntraClones", "[reporter][json]")
{
    JsonReporter reporter;

    std::vector<std::filesystem::path> files = {"test.cpp"};

    std::vector<Token> tokens = {
        {TokenType::Int, "int", {.fileIndex = 0, .line = 1, .column = 1}},
        {TokenType::Identifier, "a", {.fileIndex = 0, .line = 2, .column = 1}},
        {TokenType::Equal, "=", {.fileIndex = 0, .line = 2, .column = 3}},
        {TokenType::NumericLiteral, "1", {.fileIndex = 0, .line = 2, .column = 5}},
        {TokenType::Semicolon, ";", {.fileIndex = 0, .line = 2, .column = 6}},
        {TokenType::Int, "int", {.fileIndex = 0, .line = 3, .column = 1}},
        {TokenType::Identifier, "b", {.fileIndex = 0, .line = 4, .column = 1}},
        {TokenType::Equal, "=", {.fileIndex = 0, .line = 4, .column = 3}},
        {TokenType::NumericLiteral, "2", {.fileIndex = 0, .line = 4, .column = 5}},
        {TokenType::Semicolon, ";", {.fileIndex = 0, .line = 4, .column = 6}},
    };

    CodeBlock block{
        .name = "testFunc",
        .sourceRange = {.start = {.fileIndex = 0, .line = 1, .column = 1},
                        .end = {.fileIndex = 0, .line = 4, .column = 7}},
        .tokenStart = 0,
        .tokenEnd = 10,
        .normalizedIds = {57, 1000, 166, 1001, 125, 57, 1002, 166, 1003, 125},
        .textPreservingIds = {},
    };

    IntraCloneResult result{
        .blockIndex = 0,
        .pairs = {IntraClonePair{
            .blockIndex = 0,
            .regionA = {.start = 0, .length = 5},
            .regionB = {.start = 5, .length = 5},
            .similarity = 0.80,
        }},
    };

    std::vector<std::vector<Token>> allTokens = {tokens};
    std::vector<size_t> blockToFile = {0};

    reporter.ReportIntraClones({result}, {block}, allTokens, blockToFile, files);
    reporter.ReportSummary({.totalFiles = 1, .totalBlocks = 1, .totalGroups = 0, .totalIntraPairs = 1});

    auto const output = reporter.Render();
    auto const json = nlohmann::json::parse(output);

    REQUIRE(json["intraClones"].size() == 1);
    auto const& ic = json["intraClones"][0];
    CHECK(ic["name"] == "testFunc");
    CHECK(ic["filePath"] == "test.cpp");

    REQUIRE(ic["pairs"].size() == 1);
    auto const& pair = ic["pairs"][0];
    CHECK(pair["similarity"].get<double>() > 0.79);
    CHECK(pair["regionA"].contains("startLine"));
    CHECK(pair["regionA"].contains("endLine"));
    CHECK(pair["regionB"].contains("startLine"));
    CHECK(pair["regionB"].contains("endLine"));
}

TEST_CASE("JsonReporter.ValidJson", "[reporter][json]")
{
    // Ensure output is always valid JSON even with empty data
    JsonReporter reporter;

    reporter.Report({}, {}, {}, {}, {});
    reporter.ReportIntraClones({}, {}, {}, {}, {});
    reporter.ReportSummary({});

    auto const output = reporter.Render();
    CHECK_NOTHROW(nlohmann::json::parse(output));
}

TEST_CASE("JsonReporter.WriteTo", "[reporter][json]")
{
    JsonReporter reporter;
    reporter.ReportSummary({.totalFiles = 5});

    std::ostringstream oss;
    reporter.WriteTo(oss);

    auto const json = nlohmann::json::parse(oss.str());
    CHECK(json["summary"]["totalFiles"] == 5);
}
