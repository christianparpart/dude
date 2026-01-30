// SPDX-License-Identifier: Apache-2.0
#include <dude/MappedFile.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace dude;

namespace
{

/// @brief RAII helper that creates a temporary file and removes it on destruction.
struct TempFile
{
    std::filesystem::path path;

    explicit TempFile(std::string const& content)
    {
        path = std::filesystem::temp_directory_path() / "dude_mapped_file_test.tmp";
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
    }

    ~TempFile() { std::filesystem::remove(path); }

    TempFile(TempFile const&) = delete;
    TempFile& operator=(TempFile const&) = delete;
    TempFile(TempFile&&) = delete;
    TempFile& operator=(TempFile&&) = delete;
};

/// @brief RAII helper that creates an empty temporary file and removes it on destruction.
struct EmptyTempFile
{
    std::filesystem::path path;

    EmptyTempFile()
    {
        path = std::filesystem::temp_directory_path() / "dude_mapped_file_empty_test.tmp";
        std::ofstream ofs(path, std::ios::binary);
        // write nothing
    }

    ~EmptyTempFile() { std::filesystem::remove(path); }

    EmptyTempFile(EmptyTempFile const&) = delete;
    EmptyTempFile& operator=(EmptyTempFile const&) = delete;
    EmptyTempFile(EmptyTempFile&&) = delete;
    EmptyTempFile& operator=(EmptyTempFile&&) = delete;
};

} // namespace

TEST_CASE("MappedFile.OpenValidFile", "[MappedFile]")
{
    auto const content = std::string("Hello, memory-mapped world!");
    TempFile tmp(content);

    auto result = MappedFile::Open(tmp.path);
    REQUIRE(result.has_value());

    auto& mapped = *result;
    CHECK(mapped.IsValid());
    CHECK(mapped.Size() == content.size());
    CHECK(mapped.View() == content);
}

TEST_CASE("MappedFile.OpenEmptyFile", "[MappedFile]")
{
    EmptyTempFile tmp;

    auto result = MappedFile::Open(tmp.path);
    REQUIRE(result.has_value());

    auto& mapped = *result;
    // Empty file: valid mapping but zero size
    CHECK(mapped.Size() == 0);
    CHECK(mapped.View().empty());
}

TEST_CASE("MappedFile.OpenNonexistentFile", "[MappedFile]")
{
    auto result = MappedFile::Open("/tmp/dude_nonexistent_file_that_does_not_exist.xyz");
    REQUIRE_FALSE(result.has_value());
    CHECK_FALSE(result.error().empty());
}

TEST_CASE("MappedFile.MoveConstruction", "[MappedFile]")
{
    auto const content = std::string("move-construct test data");
    TempFile tmp(content);

    auto result = MappedFile::Open(tmp.path);
    REQUIRE(result.has_value());

    auto source = std::move(*result);
    CHECK(source.IsValid());
    CHECK(source.Size() == content.size());
    CHECK(source.View() == content);

    // Move construct into destination
    MappedFile destination(std::move(source));
    CHECK(destination.IsValid());
    CHECK(destination.Size() == content.size());
    CHECK(destination.View() == content);

    // Source should be invalidated
    CHECK_FALSE(source.IsValid()); // NOLINT(bugprone-use-after-move)
    CHECK(source.Size() == 0);
}

TEST_CASE("MappedFile.MoveAssignment", "[MappedFile]")
{
    auto const content1 = std::string("first file content");
    auto const content2 = std::string("second file content");
    TempFile tmp1(content1);
    TempFile tmp2(content2);

    auto result1 = MappedFile::Open(tmp1.path);
    auto result2 = MappedFile::Open(tmp2.path);
    REQUIRE(result1.has_value());
    REQUIRE(result2.has_value());

    auto mapped1 = std::move(*result1);
    auto mapped2 = std::move(*result2);

    // Move assign mapped1 = mapped2
    mapped1 = std::move(mapped2);
    CHECK(mapped1.IsValid());
    CHECK(mapped1.Size() == content2.size());
    CHECK(mapped1.View() == content2);

    // Source should be invalidated
    CHECK_FALSE(mapped2.IsValid()); // NOLINT(bugprone-use-after-move)
    CHECK(mapped2.Size() == 0);
}

TEST_CASE("MappedFile.MoveSelfAssignment", "[MappedFile]")
{
    auto const content = std::string("self-assignment data");
    TempFile tmp(content);

    auto result = MappedFile::Open(tmp.path);
    REQUIRE(result.has_value());

    auto mapped = std::move(*result);
    CHECK(mapped.IsValid());

    // Self-move-assignment should be a no-op
    auto* ptr = &mapped;
    *ptr = std::move(mapped);               // NOLINT(bugprone-use-after-move,clang-diagnostic-self-move)
    CHECK(mapped.IsValid());                // NOLINT(bugprone-use-after-move)
    CHECK(mapped.Size() == content.size()); // NOLINT(bugprone-use-after-move)
    CHECK(mapped.View() == content);        // NOLINT(bugprone-use-after-move)
}
