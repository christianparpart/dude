// SPDX-License-Identifier: Apache-2.0
#include <codedup/FileScanner.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace codedup;

namespace
{

/// @brief Helper to create a temporary directory with test files.
class TempDir
{
public:
    TempDir() : _path(std::filesystem::temp_directory_path() / "codedup_test")
    {
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

    CHECK(result->size() == 2); // Only .cpp and .hpp
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
