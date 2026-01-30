// SPDX-License-Identifier: Apache-2.0
#include <git/GitDiffParser.hpp>

#include <codedup/DiffRange.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace git;
using namespace codedup;

TEST_CASE("GitDiffParser.ParseSingleFileSingleHunk", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/foo.cpp b/src/foo.cpp
index 1234567..abcdefg 100644
--- a/src/foo.cpp
+++ b/src/foo.cpp
@@ -10,3 +10,5 @@ void foo()
+    int a = 1;
+    int b = 2;
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    REQUIRE(result.size() == 1);
    CHECK(result[0].filePath == "src/foo.cpp");
    REQUIRE(result[0].changedRanges.size() == 1);
    CHECK(result[0].changedRanges[0].startLine == 10);
    CHECK(result[0].changedRanges[0].endLine == 14);
}

TEST_CASE("GitDiffParser.ParseSingleFileSingleLineHunk", "[gitdiff]")
{
    // Single line addition: count is implicit 1.
    auto const* const diff = R"(diff --git a/src/foo.cpp b/src/foo.cpp
index 1234567..abcdefg 100644
--- a/src/foo.cpp
+++ b/src/foo.cpp
@@ -5,0 +6 @@ void foo()
+    int x = 42;
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].changedRanges.size() == 1);
    CHECK(result[0].changedRanges[0].startLine == 6);
    CHECK(result[0].changedRanges[0].endLine == 6);
}

TEST_CASE("GitDiffParser.ParseMultipleHunks", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/foo.cpp b/src/foo.cpp
index 1234567..abcdefg 100644
--- a/src/foo.cpp
+++ b/src/foo.cpp
@@ -10,3 +10,5 @@ void foo()
+    int a = 1;
+    int b = 2;
@@ -50,2 +52,4 @@ void bar()
+    int c = 3;
+    int d = 4;
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].changedRanges.size() == 2);
    CHECK(result[0].changedRanges[0].startLine == 10);
    CHECK(result[0].changedRanges[0].endLine == 14);
    CHECK(result[0].changedRanges[1].startLine == 52);
    CHECK(result[0].changedRanges[1].endLine == 55);
}

TEST_CASE("GitDiffParser.ParseMultipleFiles", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/foo.cpp b/src/foo.cpp
index 1234567..abcdefg 100644
--- a/src/foo.cpp
+++ b/src/foo.cpp
@@ -10,3 +10,5 @@ void foo()
+    int a = 1;
diff --git a/src/bar.cpp b/src/bar.cpp
index 1234567..abcdefg 100644
--- a/src/bar.cpp
+++ b/src/bar.cpp
@@ -20,2 +20,3 @@ void bar()
+    int b = 2;
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    REQUIRE(result.size() == 2);
    CHECK(result[0].filePath == "src/foo.cpp");
    CHECK(result[1].filePath == "src/bar.cpp");
}

TEST_CASE("GitDiffParser.SkipDeletedFile", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/old.cpp b/src/old.cpp
deleted file mode 100644
index 1234567..0000000
--- a/src/old.cpp
+++ /dev/null
@@ -1,10 +0,0 @@
-void old() {}
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    CHECK(result.empty());
}

TEST_CASE("GitDiffParser.SkipBinaryFile", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/data/image.png b/data/image.png
Binary files a/data/image.png and b/data/image.png differ
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    CHECK(result.empty());
}

TEST_CASE("GitDiffParser.NewFile", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/new.cpp b/src/new.cpp
new file mode 100644
index 0000000..abcdefg
--- /dev/null
+++ b/src/new.cpp
@@ -0,0 +1,15 @@
+void newFunc() {
+    int x = 1;
+}
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    REQUIRE(result.size() == 1);
    CHECK(result[0].filePath == "src/new.cpp");
    REQUIRE(result[0].changedRanges.size() == 1);
    CHECK(result[0].changedRanges[0].startLine == 1);
    CHECK(result[0].changedRanges[0].endLine == 15);
}

TEST_CASE("GitDiffParser.FilterByExtension", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/foo.cpp b/src/foo.cpp
index 1234567..abcdefg 100644
--- a/src/foo.cpp
+++ b/src/foo.cpp
@@ -10,3 +10,5 @@ void foo()
+    int a = 1;
diff --git a/README.md b/README.md
index 1234567..abcdefg 100644
--- a/README.md
+++ b/README.md
@@ -1,2 +1,3 @@
+# New heading
diff --git a/src/bar.hpp b/src/bar.hpp
index 1234567..abcdefg 100644
--- a/src/bar.hpp
+++ b/src/bar.hpp
@@ -5,2 +5,3 @@
+    int b;
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff, {".cpp", ".hpp"});
    CHECK(result.size() == 2);
    CHECK(result[0].filePath == "src/foo.cpp");
    CHECK(result[1].filePath == "src/bar.hpp");
}

TEST_CASE("GitDiffParser.PureDeletionHunkSkipped", "[gitdiff]")
{
    // Hunk with +start,0 means 0 added lines — pure deletion on the new side.
    auto const* const diff = R"(diff --git a/src/foo.cpp b/src/foo.cpp
index 1234567..abcdefg 100644
--- a/src/foo.cpp
+++ b/src/foo.cpp
@@ -10,3 +10,0 @@ void foo()
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    // The file appears but with no changed ranges (the hunk was a deletion).
    CHECK(result.empty());
}

TEST_CASE("GitDiffParser.EmptyDiff", "[gitdiff]")
{
    auto const result = GitDiffParser::ParseDiffOutput("");
    CHECK(result.empty());
}

TEST_CASE("GitDiffParser.RenamedFile", "[gitdiff]")
{
    auto const* const diff = R"(diff --git a/src/old.cpp b/src/new.cpp
similarity index 90%
rename from src/old.cpp
rename to src/new.cpp
index 1234567..abcdefg 100644
--- a/src/old.cpp
+++ b/src/new.cpp
@@ -5,2 +5,4 @@ void foo()
+    int a = 1;
+    int b = 2;
)";

    auto const result = GitDiffParser::ParseDiffOutput(diff);
    REQUIRE(result.size() == 1);
    // Should use the new (b/) path.
    CHECK(result[0].filePath == "src/new.cpp");
    REQUIRE(result[0].changedRanges.size() == 1);
    CHECK(result[0].changedRanges[0].startLine == 5);
    CHECK(result[0].changedRanges[0].endLine == 8);
}
