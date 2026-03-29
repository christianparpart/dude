// SPDX-License-Identifier: Apache-2.0
#include <tests/TempTestDir.hpp>

#include <dude/FileScanner.hpp>
#include <dude/GlobMatch.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

using namespace dude;
using test_utils::TempTestDir;

TEST_CASE("FileScanner.ExtensionFiltering", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("test.cpp");
    dir.WriteFile("test.hpp");
    dir.WriteFile("test.txt");
    dir.WriteFile("test.py");

    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());

    CHECK(result->size() == 3); // .cpp, .hpp, and .py
}

TEST_CASE("FileScanner.RecursiveScanning", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("a.cpp");
    dir.WriteFile("sub/b.cpp");
    dir.WriteFile("sub/deep/c.hpp");

    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());

    CHECK(result->size() == 3);
}

TEST_CASE("FileScanner.CustomExtensions", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("test.cpp");
    dir.WriteFile("test.py");
    dir.WriteFile("test.rs");

    auto result = FileScanner::Scan(dir.Path(), {".py", ".rs"});
    REQUIRE(result.has_value());

    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.NonExistentDirectory", "[scanner]")
{
    auto result = FileScanner::Scan("/nonexistent/path/that/does/not/exist");
    CHECK(!result.has_value());
}

TEST_CASE("FileScanner.EmptyDirectory", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("FileScanner.SortedResults", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("c.cpp");
    dir.WriteFile("a.cpp");
    dir.WriteFile("b.cpp");

    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);

    CHECK(std::is_sorted(result->begin(), result->end()));
}

TEST_CASE("FileScanner.WithFilter", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("keep.cpp");
    dir.WriteFile("skip.cpp");
    dir.WriteFile("also_keep.cpp");

    // Filter that rejects files containing "skip" in the filename.
    auto const filter =
        dude::FileFilter([](std::filesystem::path const& path) { return !path.filename().string().contains("skip"); });

    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.WithNulloptFilter", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("a.cpp");
    dir.WriteFile("b.cpp");

    // Passing std::nullopt should include all files (same as no filter).
    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), std::nullopt);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.WithGlobFilter", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("BitProbe.cpp");
    dir.WriteFile("DlgBitWpkZus.cpp");
    dir.WriteFile("MainWindow.cpp");
    dir.WriteFile("Helper.hpp");

    // Glob filter that matches filenames containing "Bit".
    auto const filter = dude::FileFilter([](std::filesystem::path const& path) -> bool
                                         { return dude::GlobMatch("*Bit*", path.filename().string()); });

    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.EmptyExtensionsAcceptsAll", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("test.cpp");
    dir.WriteFile("test.txt");
    dir.WriteFile("test.unknown");

    auto result = FileScanner::Scan(dir.Path(), {});
    REQUIRE(result.has_value());
    CHECK(result->size() == 3);
}

TEST_CASE("FileScanner.EmptyExtensionsWithGlobFilter", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("Controller.cpp");
    dir.WriteFile("Controller.hpp");
    dir.WriteFile("Main.cpp");
    dir.WriteFile("notes.txt");

    auto const filter = dude::FileFilter([](std::filesystem::path const& path) -> bool
                                         { return dude::GlobMatch("*.cpp", path.filename().string()); });

    auto result = FileScanner::Scan(dir.Path(), {}, filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2); // Controller.cpp and Main.cpp
}

TEST_CASE("FileScanner.WithMultipleGlobPatterns", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("BitProbe.cpp");
    dir.WriteFile("ProbeTest.hpp");
    dir.WriteFile("MainWindow.cpp");

    // Glob filter with OR semantics: matches "*Bit*" or "*Probe*".
    auto const patterns = std::vector<std::string>{"*Bit*", "*Probe*"};
    auto const filter = dude::FileFilter(
        [patterns](std::filesystem::path const& path) -> bool
        {
            auto const filename = path.filename().string();
            return std::ranges::any_of(patterns, [&filename](std::string const& pattern)
                                       { return dude::GlobMatch(pattern, filename); });
        });

    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.WithExcludeFilter", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("main.cpp");
    dir.WriteFile("main_test.cpp");
    dir.WriteFile("helper.cpp");
    dir.WriteFile("helper_test.cpp");

    // Exclude filter that rejects filenames matching "*_test*".
    auto const filter = dude::FileFilter([](std::filesystem::path const& path) -> bool
                                         { return !dude::GlobMatch("*_test*", path.filename().string()); });

    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2); // main.cpp and helper.cpp
}

TEST_CASE("FileScanner.WithGlobAndExcludeFilter", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("main.cpp");
    dir.WriteFile("main_test.cpp");
    dir.WriteFile("helper.hpp");

    // Include *.cpp, then exclude *_test*
    auto const filter = dude::FileFilter(
        [](std::filesystem::path const& path) -> bool
        {
            auto const filename = path.filename().string();
            if (!dude::GlobMatch("*.cpp", filename))
                return false;
            if (dude::GlobMatch("*_test*", filename))
                return false;
            return true;
        });

    auto result = FileScanner::Scan(dir.Path(), {}, filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 1); // main.cpp only
}

// ---------------------------------------------------------------------------
// Coverage: error path when path is a regular file (not a directory)
// ---------------------------------------------------------------------------

TEST_CASE("FileScanner.ScanRegularFile", "[scanner]")
{
    TempTestDir dir("dude_scanner_test");
    dir.WriteFile("foo.cpp");
    auto const filePath = dir.Path() / "foo.cpp";

    auto result = FileScanner::Scan(filePath, {});
    CHECK_FALSE(result.has_value());
}
