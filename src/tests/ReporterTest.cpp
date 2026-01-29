// SPDX-License-Identifier: Apache-2.0
#include <codedup/IntraFunctionDetector.hpp>
#include <codedup/Reporter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace codedup;

TEST_CASE("Reporter.SummaryNoColor", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3});

    CHECK(out.contains("10 files"));
    CHECK(out.contains("50 blocks"));
    CHECK(out.contains("3 clone groups"));
}

TEST_CASE("Reporter.SummaryWithColor", "[reporter]")
{
    Reporter reporter({.useColor = true});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 5, .totalBlocks = 20, .totalGroups = 1});

    CHECK(out.contains("\033[")); // Contains ANSI escape
    CHECK(out.contains("5 files"));
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
    CHECK(out.contains("| " + std::string(4, ' ') + "if"));
    // Line 2 should have 8 leading spaces for indentation (column 9 means 8 spaces before "return")
    CHECK(out.contains("| " + std::string(8, ' ') + "return"));
    // Line 3 should have 4 leading spaces for indentation (column 5 means 4 spaces before "}")
    CHECK(out.contains("| " + std::string(4, ' ') + "}"));
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
    CHECK(out.contains("a  +  b"));
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

    CHECK(out.contains("Clone Group #1"));
    CHECK(out.contains("testFunc"));
    CHECK(out.contains("95%"));
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

    CHECK(out.contains("100 files"));
    CHECK(out.contains("500 blocks"));
    CHECK(out.contains("12 clone groups"));
    CHECK(out.contains("5 intra-function"));
    CHECK(out.contains("Timing:"));
    CHECK(out.contains("scanning"));
    CHECK(out.contains("tokenizing"));
    CHECK(out.contains("clone detection"));
    CHECK(out.contains("intra-function detection"));
    CHECK(out.contains("total"));
}

TEST_CASE("Reporter.SummaryWithoutTiming", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(out, {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3});

    CHECK(out.contains("10 files"));
    // No timing line should appear
    CHECK(!out.contains("Timing:"));
    // No duplications line when all counts are zero
    CHECK(!out.contains("Duplications:"));
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

    CHECK(out.contains("Duplications:"));
    CHECK(out.contains("120 duplicated lines"));
    CHECK(out.contains("6 functions in clone groups"));
    CHECK(out.contains("2 functions with internal clones"));
}

TEST_CASE("Reporter.SummaryDuplicationsNoIntra", "[reporter]")
{
    Reporter reporter({.useColor = false});
    std::string out;
    reporter.ReportSummary(
        out, {.totalFiles = 10, .totalBlocks = 50, .totalGroups = 3, .totalDuplicatedLines = 80, .totalFunctions = 4});

    CHECK(out.contains("Duplications:"));
    CHECK(out.contains("80 duplicated lines"));
    CHECK(out.contains("4 functions in clone groups"));
    // No intra-function text when count is zero
    CHECK(!out.contains("internal clones"));
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
    CHECK(out.contains("\033[48;2;"));
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
    CHECK(!out.contains("\033["));
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
    CHECK(out.contains("\033[38;2;"));
    CHECK(!out.contains("\033[48;2;"));
}

// ============================================================================================
// Intra-Clone Highlighting Tests (coordinate space fix)
// ============================================================================================

TEST_CASE("Reporter.IntraCloneRegionLines.WithComments", "[reporter][intra]")
{
    // Verifies that intra-clone region line numbers are correct when comments
    // cause normalized indices to diverge from original token indices.
    //
    // Original tokens (indices 0..11):
    //   0: int       line 1   (normalized idx 0)
    //   1: // cmt    line 1   (skipped - comment)
    //   2: a         line 2   (normalized idx 1)
    //   3: =         line 2   (normalized idx 2)
    //   4: 1         line 2   (normalized idx 3)
    //   5: ;         line 2   (normalized idx 4)
    //   6: int       line 3   (normalized idx 5)
    //   7: // cmt2   line 3   (skipped - comment)
    //   8: b         line 4   (normalized idx 6)
    //   9: =         line 4   (normalized idx 7)
    //  10: 2         line 4   (normalized idx 8)
    //  11: ;         line 4   (normalized idx 9)
    //
    // Normalized IDs: [int, a, =, 1, ;, int, b, =, 2, ;]  (10 entries)
    // Region A: normalized [0..5) -> "int a = 1 ;" -> original tokens [0,2,3,4,5] -> lines 1-2
    // Region B: normalized [5..10) -> "int b = 2 ;" -> original tokens [6,8,9,10,11] -> lines 3-4
    //
    // Without the fix, block.tokenStart + region.start would use wrong original indices.

    Reporter reporter({.useColor = false, .showSourceCode = false});

    std::vector<Token> tokens = {
        {TokenType::Int, "int", {.filePath = "test.cpp", .line = 1, .column = 1}},
        {TokenType::LineComment, "// cmt", {.filePath = "test.cpp", .line = 1, .column = 5}},
        {TokenType::Identifier, "a", {.filePath = "test.cpp", .line = 2, .column = 1}},
        {TokenType::Equal, "=", {.filePath = "test.cpp", .line = 2, .column = 3}},
        {TokenType::NumericLiteral, "1", {.filePath = "test.cpp", .line = 2, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "test.cpp", .line = 2, .column = 6}},
        {TokenType::Int, "int", {.filePath = "test.cpp", .line = 3, .column = 1}},
        {TokenType::LineComment, "// cmt2", {.filePath = "test.cpp", .line = 3, .column = 5}},
        {TokenType::Identifier, "b", {.filePath = "test.cpp", .line = 4, .column = 1}},
        {TokenType::Equal, "=", {.filePath = "test.cpp", .line = 4, .column = 3}},
        {TokenType::NumericLiteral, "2", {.filePath = "test.cpp", .line = 4, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "test.cpp", .line = 4, .column = 6}},
    };

    CodeBlock block{
        .name = "testFunc",
        .sourceRange = {.start = {.filePath = "test.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "test.cpp", .line = 4, .column = 7}},
        .tokenStart = 0,
        .tokenEnd = 12,
        // Normalized IDs skip comments: tokens 0,2,3,4,5,6,8,9,10,11
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

    std::string out;
    reporter.ReportIntraClones(out, {result}, {block}, allTokens, blockToFile);

    // Region A should span lines 1-2 (original tokens 0 and 5)
    CHECK(out.contains("Region A: lines 1-2"));
    // Region B should span lines 3-4 (original tokens 6 and 11)
    CHECK(out.contains("Region B: lines 3-4"));
}

TEST_CASE("Reporter.IntraCloneHighlighting.WithComments", "[reporter][intra][highlight]")
{
    // Verifies that intra-clone highlight positions are correct when comments
    // cause normalized indices to diverge from original token indices.
    //
    // Same token layout as the region lines test above.
    // The two regions differ in identifier and literal tokens:
    //   Region A normalized: [int, a, =, 1, ;]  (indices 0..4)
    //   Region B normalized: [int, b, =, 2, ;]  (indices 5..9)
    // LCS matches: int, =, ;  (positions 0,2,4 in A; 0,2,4 in B)
    // Unmatched A: positions 1,3 -> normalized idx 1,3 -> original tokens 2,4
    // Unmatched B: positions 1,3 -> normalized idx 6,8 -> original tokens 8,10
    //
    // Without the fix, the highlights would use wrong original token indices because
    // the old code added block.tokenStart + region.start (mixing coordinate spaces).

    Reporter reporter(
        {.useColor = true, .showSourceCode = true, .highlightDifferences = true, .theme = ColorTheme::Dark});

    std::vector<Token> tokens = {
        {TokenType::Int, "int", {.filePath = "test.cpp", .line = 1, .column = 1}},
        {TokenType::LineComment, "// cmt", {.filePath = "test.cpp", .line = 1, .column = 5}},
        {TokenType::Identifier, "a", {.filePath = "test.cpp", .line = 2, .column = 1}},
        {TokenType::Equal, "=", {.filePath = "test.cpp", .line = 2, .column = 3}},
        {TokenType::NumericLiteral, "1", {.filePath = "test.cpp", .line = 2, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "test.cpp", .line = 2, .column = 6}},
        {TokenType::Int, "int", {.filePath = "test.cpp", .line = 3, .column = 1}},
        {TokenType::LineComment, "// cmt2", {.filePath = "test.cpp", .line = 3, .column = 5}},
        {TokenType::Identifier, "b", {.filePath = "test.cpp", .line = 4, .column = 1}},
        {TokenType::Equal, "=", {.filePath = "test.cpp", .line = 4, .column = 3}},
        {TokenType::NumericLiteral, "2", {.filePath = "test.cpp", .line = 4, .column = 5}},
        {TokenType::Semicolon, ";", {.filePath = "test.cpp", .line = 4, .column = 6}},
    };

    CodeBlock block{
        .name = "testFunc",
        .sourceRange = {.start = {.filePath = "test.cpp", .line = 1, .column = 1},
                        .end = {.filePath = "test.cpp", .line = 4, .column = 7}},
        .tokenStart = 0,
        .tokenEnd = 12,
        // Normalized IDs skip comments: tokens 0,2,3,4,5,6,8,9,10,11
        .normalizedIds = {57, 1000, 166, 1001, 125, 57, 1002, 166, 1003, 125},
        // Text-preserving IDs: identifiers/literals differ between regions
        .textPreservingIds = {57, 2000, 166, 2002, 125, 57, 2001, 166, 2003, 125},
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

    std::string out;
    reporter.ReportIntraClones(out, {result}, {block}, allTokens, blockToFile);

    // With highlighting enabled, background highlight ANSI code should appear for differing tokens.
    // The differing tokens are "a" (original idx 2), "1" (original idx 4),
    //                          "b" (original idx 8), "2" (original idx 10).
    // Background truecolor ANSI should be present.
    CHECK(out.contains("\033[48;2;"));

    // The output should contain lines 1-2 for region A and lines 3-4 for region B
    CHECK(out.contains("lines 1-2"));
    CHECK(out.contains("lines 3-4"));

    // The tokens "a" and "b" should appear in the source output
    CHECK(out.contains('a'));
    CHECK(out.contains('b'));
}
