// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <tests/TempTestDir.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

namespace test_utils
{

/// @brief RAII guard for a temporary git repository used by tests.
///
/// Creates a unique temporary directory, initializes a git repository in it with a
/// deterministic identity, and removes the whole tree on destruction.
class TempGitRepo
{
public:
    /// @brief Creates and initializes the temporary repository.
    /// @param prefix Name prefix for the temporary directory.
    explicit TempGitRepo(std::string_view prefix = "dude_git_test") : _root(UniqueTempPath(prefix))
    {
        std::filesystem::create_directories(_root);
        RunGit("init");
        RunGit("config user.email test@test.com");
        RunGit("config user.name Test");
    }

    ~TempGitRepo()
    {
        std::error_code ec;
        std::filesystem::remove_all(_root, ec);
    }

    TempGitRepo(TempGitRepo const&) = delete;
    TempGitRepo(TempGitRepo&&) = delete;
    auto operator=(TempGitRepo const&) -> TempGitRepo& = delete;
    auto operator=(TempGitRepo&&) -> TempGitRepo& = delete;

    /// @brief Returns the repository root directory.
    [[nodiscard]] auto Root() const -> std::filesystem::path const& { return _root; }

    /// @brief Writes a file inside the repository, creating parent directories as needed.
    /// @param relativePath Path relative to the repository root.
    /// @param content File content to write.
    void WriteFile(std::filesystem::path const& relativePath, std::string_view content) const
    {
        auto const fullPath = _root / relativePath;
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream out(fullPath);
        out << content;
    }

    /// @brief Runs a git command inside the repository and requires it to succeed.
    /// @param gitArgs The git arguments, without the leading `git`.
    void RunGit(std::string const& gitArgs) const
    {
        auto const command = std::format("git -C {} {}", _root.string(), gitArgs);
        // NOLINTNEXTLINE(cert-env33-c) -- std::system is intentional for test setup
        auto const status = std::system(command.c_str());
        REQUIRE(status == 0);
    }

    /// @brief Stages everything and creates a commit.
    /// @param message The commit message.
    void Commit(std::string const& message) const
    {
        RunGit("add -A");
        RunGit(std::format("commit -m \"{}\"", message));
    }

    /// @brief Returns the current HEAD commit SHA.
    [[nodiscard]] auto GetHeadSha() const -> std::string
    {
        auto const shaFile = _root / "head_sha.tmp";
        auto const command = std::format("git -C {} rev-parse HEAD > \"{}\"", _root.string(), shaFile.string());
        // NOLINTNEXTLINE(cert-env33-c) -- std::system is intentional for test setup
        auto const status = std::system(command.c_str());
        REQUIRE(status == 0);

        std::string sha;
        {
            std::ifstream in(shaFile);
            std::getline(in, sha);
        } // Close the file handle before removing (required on Windows).
        std::filesystem::remove(shaFile);

        while (!sha.empty() && (sha.back() == '\n' || sha.back() == '\r'))
            sha.pop_back();
        return sha;
    }

private:
    std::filesystem::path _root;
};

} // namespace test_utils
