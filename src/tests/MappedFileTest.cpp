// SPDX-License-Identifier: Apache-2.0
#include <tests/TempTestDir.hpp>

#include <dude/MappedFile.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace dude;

TEST_CASE("MappedFile.OpenValidFile", "[MappedFile]")
{
    auto const content = std::string("Hello, memory-mapped world!");
    test_utils::TempTestDir tmp("dude_mapped_file_test");
    tmp.WriteFile("test.dat", content);
    auto const path = tmp.Path() / "test.dat";

    auto result = MappedFile::Open(path);
    REQUIRE(result.has_value());

    auto& mapped = *result;
    CHECK(mapped.IsValid());
    CHECK(mapped.Size() == content.size());
    CHECK(mapped.View() == content);
}

TEST_CASE("MappedFile.OpenEmptyFile", "[MappedFile]")
{
    test_utils::TempTestDir tmp("dude_mapped_file_test");
    tmp.WriteFile("empty.dat", "");
    auto const path = tmp.Path() / "empty.dat";

    auto result = MappedFile::Open(path);
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
    test_utils::TempTestDir tmp("dude_mapped_file_test");
    tmp.WriteFile("test.dat", content);
    auto const path = tmp.Path() / "test.dat";

    auto result = MappedFile::Open(path);
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
    test_utils::TempTestDir tmp("dude_mapped_file_test");
    tmp.WriteFile("file1.dat", content1);
    tmp.WriteFile("file2.dat", content2);

    auto result1 = MappedFile::Open(tmp.Path() / "file1.dat");
    auto result2 = MappedFile::Open(tmp.Path() / "file2.dat");
    REQUIRE(result1.has_value());
    REQUIRE(result2.has_value());

    auto mapped1 = std::move(*result1);
    auto mapped2 = std::move(*result2);

    // Move assign mapped1 = mapped2
    mapped1 = std::move(mapped2);
    CHECK(mapped1.IsValid());
    CHECK(mapped1.Size() == content2.size());
    CHECK(mapped1.View() == content2);
}
