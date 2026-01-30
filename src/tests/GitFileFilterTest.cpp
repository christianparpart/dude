// SPDX-License-Identifier: Apache-2.0
#include <git/GitFileFilter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace git;

namespace
{

/// @brief Returns the project root directory by navigating up from this test file's location.
auto ProjectRoot() -> std::filesystem::path
{
    // __FILE__ is src/tests/GitFileFilterTest.cpp, so go up two levels.
    return std::filesystem::weakly_canonical(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path());
}

} // namespace

TEST_CASE("GitFileFilter.FindGitRootOnProjectDirectory", "[gitfilefilter]")
{
    auto const projectDir = ProjectRoot();
    auto const result = GitFileFilter::FindGitRoot(projectDir);
    REQUIRE(result.has_value());
    CHECK(std::filesystem::exists(*result / ".git"));
}

TEST_CASE("GitFileFilter.FindGitRootOnTmp", "[gitfilefilter]")
{
    // /tmp is typically not a git repository.
    auto const result = GitFileFilter::FindGitRoot("/tmp");
    CHECK(!result.has_value());
}

TEST_CASE("GitFileFilter.CreateFilterOnTmp", "[gitfilefilter]")
{
    // CreateFilter should return nullopt for a non-git directory.
    auto const filter = GitFileFilter::CreateFilter("/tmp");
    CHECK(!filter.has_value());
}

TEST_CASE("GitFileFilter.CreateFilterOnProjectDirectory", "[gitfilefilter]")
{
    auto const filter = GitFileFilter::CreateFilter(ProjectRoot());
    REQUIRE(filter.has_value());
}

TEST_CASE("GitFileFilter.QueryNonIgnoredFilesReturnsNonEmpty", "[gitfilefilter]")
{
    auto const projectDir = ProjectRoot();
    auto const gitRoot = GitFileFilter::FindGitRoot(projectDir);
    REQUIRE(gitRoot.has_value());

    auto const files = GitFileFilter::QueryNonIgnoredFiles(*gitRoot, projectDir);
    REQUIRE(files.has_value());
    CHECK(!files->empty());
}

TEST_CASE("GitFileFilter.FilterAcceptsTrackedSourceFiles", "[gitfilefilter]")
{
    auto const projectDir = ProjectRoot();
    auto const filter = GitFileFilter::CreateFilter(projectDir);
    REQUIRE(filter.has_value());

    // The main.cpp source file should be accepted by the filter.
    auto const mainCpp = std::filesystem::weakly_canonical(projectDir / "src" / "cli" / "main.cpp");
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- REQUIRE above ensures has_value
    CHECK((*filter)(mainCpp));
}

TEST_CASE("GitFileFilter.FilterRejectsBuildArtifacts", "[gitfilefilter]")
{
    auto const projectDir = ProjectRoot();
    auto const filter = GitFileFilter::CreateFilter(projectDir);
    REQUIRE(filter.has_value());

    // A path that doesn't exist in git should be rejected.
    auto const fakePath = std::filesystem::weakly_canonical(projectDir / "build" / "fake_artifact.cpp");
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- REQUIRE above ensures has_value
    CHECK(!(*filter)(fakePath));
}
