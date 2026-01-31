// SPDX-License-Identifier: Apache-2.0
#include <dude/FileScanner.hpp>
#include <dude/GlobMatch.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <ranges>
#include <string>
#include <vector>

using namespace dude;

namespace
{

/// @brief Helper to create a temporary directory with test files.
class TempDir
{
public:
    TempDir()
    {
        static auto const seed = std::random_device{}();
        static std::atomic<unsigned> counter{0};
        _path = std::filesystem::temp_directory_path() / std::format("dude_test_{}_{}", seed, counter.fetch_add(1));
        std::filesystem::create_directories(_path);
    }

    ~TempDir() { std::filesystem::remove_all(_path); }

    TempDir(TempDir const&) = delete;
    TempDir& operator=(TempDir const&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    void CreateFile(std::filesystem::path const& relativePath) const
    {
        auto const fullPath = _path / relativePath;
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << "// test file\n";
    }

    [[nodiscard]] auto Path() const -> std::filesystem::path const& { return _path; }

private:
    std::filesystem::path _path;
};

} // namespace

TEST_CASE("FileScanner.ExtensionFiltering", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("test.cpp");
    dir.CreateFile("test.hpp");
    dir.CreateFile("test.txt");
    dir.CreateFile("test.py");

    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());

    CHECK(result->size() == 3); // .cpp, .hpp, and .py
}

TEST_CASE("FileScanner.RecursiveScanning", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("a.cpp");
    dir.CreateFile("sub/b.cpp");
    dir.CreateFile("sub/deep/c.hpp");

    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());

    CHECK(result->size() == 3);
}

TEST_CASE("FileScanner.CustomExtensions", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("test.cpp");
    dir.CreateFile("test.py");
    dir.CreateFile("test.rs");

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
    TempDir dir;
    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("FileScanner.SortedResults", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("c.cpp");
    dir.CreateFile("a.cpp");
    dir.CreateFile("b.cpp");

    auto result = FileScanner::Scan(dir.Path());
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);

    CHECK(std::is_sorted(result->begin(), result->end()));
}

TEST_CASE("FileScanner.WithFilter", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("keep.cpp");
    dir.CreateFile("skip.cpp");
    dir.CreateFile("also_keep.cpp");

    // Filter that rejects files containing "skip" in the filename.
    auto const filter =
        dude::FileFilter([](std::filesystem::path const& path) { return !path.filename().string().contains("skip"); });

    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.WithNulloptFilter", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("a.cpp");
    dir.CreateFile("b.cpp");

    // Passing std::nullopt should include all files (same as no filter).
    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), std::nullopt);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.WithGlobFilter", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("BitProbe.cpp");
    dir.CreateFile("DlgBitWpkZus.cpp");
    dir.CreateFile("MainWindow.cpp");
    dir.CreateFile("Helper.hpp");

    // Glob filter that matches filenames containing "Bit".
    auto const filter = dude::FileFilter([](std::filesystem::path const& path) -> bool
                                         { return dude::GlobMatch("*Bit*", path.filename().string()); });

    auto result = FileScanner::Scan(dir.Path(), FileScanner::DefaultExtensions(), filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.EmptyExtensionsAcceptsAll", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("test.cpp");
    dir.CreateFile("test.txt");
    dir.CreateFile("test.unknown");

    auto result = FileScanner::Scan(dir.Path(), {});
    REQUIRE(result.has_value());
    CHECK(result->size() == 3);
}

TEST_CASE("FileScanner.EmptyExtensionsWithGlobFilter", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("Controller.cpp");
    dir.CreateFile("Controller.hpp");
    dir.CreateFile("Main.cpp");
    dir.CreateFile("notes.txt");

    auto const filter = dude::FileFilter([](std::filesystem::path const& path) -> bool
                                         { return dude::GlobMatch("*.cpp", path.filename().string()); });

    auto result = FileScanner::Scan(dir.Path(), {}, filter);
    REQUIRE(result.has_value());
    CHECK(result->size() == 2); // Controller.cpp and Main.cpp
}

TEST_CASE("FileScanner.WithMultipleGlobPatterns", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("BitProbe.cpp");
    dir.CreateFile("ProbeTest.hpp");
    dir.CreateFile("MainWindow.cpp");

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

// ---------------------------------------------------------------------------
// Coverage: error path when path is a regular file (not a directory)
// ---------------------------------------------------------------------------

TEST_CASE("FileScanner.ScanRegularFile", "[scanner]")
{
    TempDir dir;
    dir.CreateFile("foo.cpp");
    auto const filePath = dir.Path() / "foo.cpp";

    auto result = FileScanner::Scan(filePath, {});
    CHECK_FALSE(result.has_value());
}
