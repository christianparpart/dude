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

    void createFile(std::filesystem::path const& relativePath) const
    {
        auto const fullPath = _path / relativePath;
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << "// test file\n";
    }

    [[nodiscard]] auto path() const -> std::filesystem::path const& { return _path; }

private:
    std::filesystem::path _path;
};

} // namespace

TEST_CASE("FileScanner.ExtensionFiltering", "[scanner]")
{
    TempDir dir;
    dir.createFile("test.cpp");
    dir.createFile("test.hpp");
    dir.createFile("test.txt");
    dir.createFile("test.py");

    auto result = FileScanner::scan(dir.path());
    REQUIRE(result.has_value());

    CHECK(result->size() == 2); // Only .cpp and .hpp
}

TEST_CASE("FileScanner.RecursiveScanning", "[scanner]")
{
    TempDir dir;
    dir.createFile("a.cpp");
    dir.createFile("sub/b.cpp");
    dir.createFile("sub/deep/c.hpp");

    auto result = FileScanner::scan(dir.path());
    REQUIRE(result.has_value());

    CHECK(result->size() == 3);
}

TEST_CASE("FileScanner.CustomExtensions", "[scanner]")
{
    TempDir dir;
    dir.createFile("test.cpp");
    dir.createFile("test.py");
    dir.createFile("test.rs");

    auto result = FileScanner::scan(dir.path(), {".py", ".rs"});
    REQUIRE(result.has_value());

    CHECK(result->size() == 2);
}

TEST_CASE("FileScanner.NonExistentDirectory", "[scanner]")
{
    auto result = FileScanner::scan("/nonexistent/path/that/does/not/exist");
    CHECK(!result.has_value());
}

TEST_CASE("FileScanner.EmptyDirectory", "[scanner]")
{
    TempDir dir;
    auto result = FileScanner::scan(dir.path());
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("FileScanner.SortedResults", "[scanner]")
{
    TempDir dir;
    dir.createFile("c.cpp");
    dir.createFile("a.cpp");
    dir.createFile("b.cpp");

    auto result = FileScanner::scan(dir.path());
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);

    CHECK(std::is_sorted(result->begin(), result->end()));
}
