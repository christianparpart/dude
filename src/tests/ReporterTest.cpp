// SPDX-License-Identifier: Apache-2.0
#include <codedup/Reporter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace codedup;

TEST_CASE("Reporter.SummaryNoColor", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3});

    CHECK(out.find("10 files") != std::string::npos);
    CHECK(out.find("50 blocks") != std::string::npos);
    CHECK(out.find("3 clone groups") != std::string::npos);
}

TEST_CASE("Reporter.SummaryWithColor", "[reporter]")
{
    Reporter reporter({.useColor = true});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 5, .totalBlocks = 20, .totalGroups = 1});

    CHECK(out.find("\033[") != std::string::npos); // Contains ANSI escape
    CHECK(out.find("5 files") != std::string::npos);
}

TEST_CASE("Reporter.EmptyGroups", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;

    std::vector<CloneGroup> groups;
    std::vector<CodeBlock> blocks;
    std::vector<std::vector<Token>> allTokens;
    std::vector<size_t> blockToFile;

    reporter.Report(out, groups, blocks, allTokens, blockToFile);
    CHECK(out.empty());
}

TEST_CASE("Reporter.IndentationPreservation", "[reporter]")
{
    // Simulate tokens for:
    //     if (x > 0) {
    //         return;
    //     }
    Reporter reporter({.useColor = false, .showSourceCode = true});

    std::vector<Token> tokens = {
        {TokenType::If, "if", {.filePath = "test.cpp", .line = 1, .column = 5}},
        {TokenType::LeftParen, "(", {.filePath = "test.cpp", .line = 1, .column = 8}},
        {TokenType::Identifier, "x", {.filePath = "test.cpp", .line = 1, .column = 9}},
        {TokenType::Greater, ">", {.filePath = "test.cpp", .line = 1, .column = 11}},
        {TokenType::NumericLiteral, "0", {.filePath = "test.cpp", .line = 1, .column = 13}},
        {TokenType::RightParen, ")", {.filePath = "test.cpp", .line = 1, .column = 14}},
        {TokenType::LeftBrace, "{", {.filePath = "test.cpp", .line = 1, .column = 16}},
        {TokenType::Return, "return", {.filePath = "test.cpp", .line = 2, .column = 9}},
        {TokenType::Semicolon, ";", {.filePath = "test.cpp", .line = 2, .column = 15}},
        {TokenType::RightBrace, "}", {.filePath = "test.cpp", .line = 3, .column = 5}},
    };

    CodeBlock block;
    block.name = "testFunc";
    block.sourceRange = {.start = {.filePath = "test.cpp", .line = 1, .column = 5},
                         .end = {.filePath = "test.cpp", .line = 3, .column = 6}};
    block.tokenStart = 0;
    block.tokenEnd = 10;
    block.normalizedIds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    CodeBlock block2 = block;
    block2.name = "testFunc2";

    CloneGroup group;
    group.blockIndices = {0, 1};
    group.avgSimilarity = 1.0;

    std::vector<std::vector<Token>> allTokens = {tokens};
    std::vector<size_t> blockToFile = {0, 0};

    std::string out;
    reporter.Report(out, {group}, {block, block2}, allTokens, blockToFile);

    // Line 1 should have 4 leading spaces for indentation (column 5 means 4 spaces before "if")
    CHECK(out.find("| " + std::string(4, ' ') + "if") != std::string::npos);
    // Line 2 should have 8 leading spaces for indentation (column 9 means 8 spaces before "return")
    CHECK(out.find("| " + std::string(8, ' ') + "return") != std::string::npos);
    // Line 3 should have 4 leading spaces for indentation (column 5 means 4 spaces before "}")
    CHECK(out.find("| " + std::string(4, ' ') + "}") != std::string::npos);
}

TEST_CASE("Reporter.InterTokenSpacingPreservation", "[reporter]")
{
    // Simulate tokens for: a  +  b  (extra spaces around +)
    // a at column 1, + at column 4, b at column 7
    Reporter reporter({.useColor = false, .showSourceCode = true});

    std::vector<Token> tokens = {
        {TokenType::Identifier, "a", {.filePath = "test.cpp", .line = 1, .column = 1}},
        {TokenType::Plus, "+", {.filePath = "test.cpp", .line = 1, .column = 4}},
        {TokenType::Identifier, "b", {.filePath = "test.cpp", .line = 1, .column = 7}},
    };

    CodeBlock block;
    block.name = "testFunc";
    block.sourceRange = {.start = {.filePath = "test.cpp", .line = 1, .column = 1},
                         .end = {.filePath = "test.cpp", .line = 1, .column = 8}};
    block.tokenStart = 0;
    block.tokenEnd = 3;
    block.normalizedIds = {1, 2, 3};

    CodeBlock block2 = block;
    block2.name = "testFunc2";

    CloneGroup group;
    group.blockIndices = {0, 1};
    group.avgSimilarity = 1.0;

    std::vector<std::vector<Token>> allTokens = {tokens};
    std::vector<size_t> blockToFile = {0, 0};

    std::string out;
    reporter.Report(out, {group}, {block, block2}, allTokens, blockToFile);

    // Should preserve "a  +  b" spacing (2 spaces before +, 2 spaces before b), not "a + b"
    CHECK(out.find("a  +  b") != std::string::npos);
}

TEST_CASE("Reporter.NoSourceOutput", "[reporter]")
{
    Reporter reporter({.useColor = false, .showSourceCode = false});
    std::string out;

    CodeBlock block;
    block.name = "testFunc";
    block.sourceRange = {.start = {.filePath = "test.cpp", .line = 1, .column = 1},
                         .end = {.filePath = "test.cpp", .line = 5, .column = 1}};
    block.normalizedIds = {1, 2, 3};

    CodeBlock block2;
    block2.name = "testFunc2";
    block2.sourceRange = {.start = {.filePath = "test2.cpp", .line = 1, .column = 1},
                          .end = {.filePath = "test2.cpp", .line = 5, .column = 1}};
    block2.normalizedIds = {1, 2, 3};

    CloneGroup group;
    group.blockIndices = {0, 1};
    group.avgSimilarity = 0.95;

    std::vector<std::vector<Token>> allTokens;
    std::vector<size_t> blockToFile;

    reporter.Report(out, {group}, {block, block2}, allTokens, blockToFile);

    CHECK(out.find("Clone Group #1") != std::string::npos);
    CHECK(out.find("testFunc") != std::string::npos);
    CHECK(out.find("95%") != std::string::npos);
}

TEST_CASE("Reporter.SummaryWithTiming", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;

    PerformanceTiming timing;
    timing.scanning = std::chrono::milliseconds(15);
    timing.tokenizing = std::chrono::milliseconds(120);
    timing.normalizing = std::chrono::milliseconds(45);
    timing.cloneDetection = std::chrono::milliseconds(890);
    timing.intraDetection = std::chrono::milliseconds(230);

    reporter.ReportSummary(
        out, {.totalFiles = 100, .totalBlocks = 500, .totalGroups = 12, .totalIntraPairs = 5, .timing = timing});

    CHECK(out.find("100 files") != std::string::npos);
    CHECK(out.find("500 blocks") != std::string::npos);
    CHECK(out.find("12 clone groups") != std::string::npos);
    CHECK(out.find("5 intra-function") != std::string::npos);
    CHECK(out.find("Timing:") != std::string::npos);
    CHECK(out.find("scanning") != std::string::npos);
    CHECK(out.find("tokenizing") != std::string::npos);
    CHECK(out.find("clone detection") != std::string::npos);
    CHECK(out.find("intra-function detection") != std::string::npos);
    CHECK(out.find("total") != std::string::npos);
}

TEST_CASE("Reporter.SummaryWithoutTiming", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3});

    CHECK(out.find("10 files") != std::string::npos);
    // No timing line should appear
    CHECK(out.find("Timing:") == std::string::npos);
    // No duplications line when all counts are zero
    CHECK(out.find("Duplications:") == std::string::npos);
}

TEST_CASE("Reporter.SummaryDuplications", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 10,
                                 .totalBlocks = 50,
                                 .totalGroups = 3,
                                 .totalIntraPairs = 2,
                                 .totalDuplicatedLines = 120,
                                 .totalFunctions = 6,
                                 .totalIntraFunctions = 2});

    CHECK(out.find("Duplications:") != std::string::npos);
    CHECK(out.find("120 duplicated lines") != std::string::npos);
    CHECK(out.find("6 functions in clone groups") != std::string::npos);
    CHECK(out.find("2 functions with internal clones") != std::string::npos);
}

TEST_CASE("Reporter.SummaryDuplicationsNoIntra", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(
        out, {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3, .totalDuplicatedLines = 80, .totalFunctions = 4});

    CHECK(out.find("Duplications:") != std::string::npos);
    CHECK(out.find("80 duplicated lines") != std::string::npos);
    CHECK(out.find("4 functions in clone groups") != std::string::npos);
    // No intra-function text when count is zero
    CHECK(out.find("internal clones") == std::string::npos);
}

// ============================================================================================
// Diff Highlighting Tests
// ============================================================================================

TEST_CASE("Reporter.DiffHighlighting.ColoredOutput", "[reporter][highlight]")
{
    // Two blocks with one differing token: different identifiers at position 2
    Reporter reporter(
        {.useColor = true, .showSourceCode = true, .highlightDifferences = true, .theme = ColorTheme::Dark});

    // Block A: "int foo = 0;"  -> tokens: int, foo, =, 0, ;
    // Block B: "int bar = 0;"  -> tokens: int, bar, =, 0, ;
    // Text-preserving IDs differ at position 1 (foo vs bar)
    std::vector<Token> tokensA = {
        {TokenType::Int, "int", {.filePath = "a.cpp", .line = 1, .column = 1}},
        {TokenType::Identifier, "foo", {.filePath = "a.cpp", .line = 1, .column = 5}},
        {TokenType::Equal, "=", {.filePath = "a.cpp", .line = 1, .column = 9}},
        {TokenType::NumericLiteral, "0", {.filePath = "a.cpp", .line = 1, .column = 11}},
        {TokenType::Semicolon, ";", {.filePath = "a.cpp", .line = 1, .column = 12}},
    };
    std::vector<Token> tokensB = {
        {TokenType::Int, "int", {.filePath = "b.cpp", .line = 1, .column = 1}},
        {TokenType::Identifier, "bar", {.filePath = "b.cpp", .line = 1, .column = 5}},
        {TokenType::Equal, "=", {.filePath = "b.cpp", .line = 1, .column = 9}},
        {TokenType::NumericLiteral, "0", {.filePath = "b.cpp", .line = 1, .column = 11}},
        {TokenType::Semicolon, ";", {.filePath = "b.cpp", .line = 1, .column = 12}},
    };

    // Structural IDs: int=57, Identifier=1000, Equal=166, NumericLiteral=1001, Semicolon=125
    // Text-preserving: int=57, "foo"=2000, "bar"=2001, =166, 0=2002, ;=125
    CodeBlock blockA{
        .name = "funcA",
        .sourceRange = {.start = {.filePath = "a.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "a.cpp", .line = 1, .column = 13}},
        .tokenStart = 0,
        .tokenEnd = 5,
        .normalizedIds = {57, 1000, 166, 1001, 125},
        .textPreservingIds = {57, 2000, 166, 2002, 125},
    };
    CodeBlock blockB{
        .name = "funcB",
        .sourceRange = {.start = {.filePath = "b.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "b.cpp", .line = 1, .column = 13}},
        .tokenStart = 0,
        .tokenEnd = 5,
        .normalizedIds = {57, 1000, 166, 1001, 125},
        .textPreservingIds = {57, 2001, 166, 2002, 125},
    };

    CloneGroup group{.blockIndices = {0, 1}, .avgSimilarity = 0.90};

    std::vector<std::vector<Token>> allTokens = {tokensA, tokensB};
    std::vector<size_t> blockToFile = {0, 1};

    std::string out;
    reporter.Report(out, {group}, {blockA, blockB}, allTokens, blockToFile);

    // Background highlight ANSI code should appear (48;2; is background truecolor)
    CHECK(out.find("\033[48;2;") != std::string::npos);
}

TEST_CASE("Reporter.DiffHighlighting.NoColorDisablesHighlight", "[reporter][highlight]")
{
    // With useColor = false, no ANSI codes at all
    Reporter reporter({.useColor = false, .showSourceCode = true, .highlightDifferences = true});

    std::vector<Token> tokensA = {
        {TokenType::Int, "int", {.filePath = "a.cpp", .line = 1, .column = 1}},
        {TokenType::Identifier, "foo", {.filePath = "a.cpp", .line = 1, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "a.cpp", .line = 1, .column = 8}},
    };
    std::vector<Token> tokensB = {
        {TokenType::Int, "int", {.filePath = "b.cpp", .line = 1, .column = 1}},
        {TokenType::Identifier, "bar", {.filePath = "b.cpp", .line = 1, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "b.cpp", .line = 1, .column = 8}},
    };

    CodeBlock blockA{
        .name = "funcA",
        .sourceRange = {.start = {.filePath = "a.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "a.cpp", .line = 1, .column = 9}},
        .tokenStart = 0,
        .tokenEnd = 3,
        .normalizedIds = {57, 1000, 125},
        .textPreservingIds = {57, 2000, 125},
    };
    CodeBlock blockB{
        .name = "funcB",
        .sourceRange = {.start = {.filePath = "b.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "b.cpp", .line = 1, .column = 9}},
        .tokenStart = 0,
        .tokenEnd = 3,
        .normalizedIds = {57, 1000, 125},
        .textPreservingIds = {57, 2001, 125},
    };

    CloneGroup group{.blockIndices = {0, 1}, .avgSimilarity = 0.90};

    std::vector<std::vector<Token>> allTokens = {tokensA, tokensB};
    std::vector<size_t> blockToFile = {0, 1};

    std::string out;
    reporter.Report(out, {group}, {blockA, blockB}, allTokens, blockToFile);

    // No ANSI codes at all
    CHECK(out.find("\033[") == std::string::npos);
}

TEST_CASE("Reporter.DiffHighlighting.DisabledHighlightDifferences", "[reporter][highlight]")
{
    // With highlightDifferences = false and useColor = true, no background highlighting
    Reporter reporter(
        {.useColor = true, .showSourceCode = true, .highlightDifferences = false, .theme = ColorTheme::Dark});

    std::vector<Token> tokensA = {
        {TokenType::Int, "int", {.filePath = "a.cpp", .line = 1, .column = 1}},
        {TokenType::Identifier, "foo", {.filePath = "a.cpp", .line = 1, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "a.cpp", .line = 1, .column = 8}},
    };
    std::vector<Token> tokensB = {
        {TokenType::Int, "int", {.filePath = "b.cpp", .line = 1, .column = 1}},
        {TokenType::Identifier, "bar", {.filePath = "b.cpp", .line = 1, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "b.cpp", .line = 1, .column = 8}},
    };

    CodeBlock blockA{
        .name = "funcA",
        .sourceRange = {.start = {.filePath = "a.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "a.cpp", .line = 1, .column = 9}},
        .tokenStart = 0,
        .tokenEnd = 3,
        .normalizedIds = {57, 1000, 125},
        .textPreservingIds = {57, 2000, 125},
    };
    CodeBlock blockB{
        .name = "funcB",
        .sourceRange = {.start = {.filePath = "b.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "b.cpp", .line = 1, .column = 9}},
        .tokenStart = 0,
        .tokenEnd = 3,
        .normalizedIds = {57, 1000, 125},
        .textPreservingIds = {57, 2001, 125},
    };

    CloneGroup group{.blockIndices = {0, 1}, .avgSimilarity = 0.90};

    std::vector<std::vector<Token>> allTokens = {tokensA, tokensB};
    std::vector<size_t> blockToFile = {0, 1};

    std::string out;
    reporter.Report(out, {group}, {blockA, blockB}, allTokens, blockToFile);

    // Should have foreground ANSI codes (38;2;) but no background codes (48;2;)
    CHECK(out.find("\033[38;2;") != std::string::npos);
    CHECK(out.find("\033[48;2;") == std::string::npos);
}
