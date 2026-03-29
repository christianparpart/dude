// SPDX-License-Identifier: Apache-2.0

#include "GitCommand.hpp"

#include <array>
#include <cstdio>
#include <format>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace git
{

auto RunCommand(std::string const& command, std::string_view errorContext)
    -> std::expected<GitCommandResult, GitCommandError>
{
    // NOLINTNEXTLINE(cert-env33-c) -- popen is intentional for git subprocess communication
    auto* pipe = popen(command.c_str(), "r");
    if (!pipe)
        return std::unexpected(
            GitCommandError{.message = std::format("Failed to execute {} (is git on PATH?)", errorContext)});

    std::string output;
    std::array<char, 4096> buffer{};
    while (auto* result = fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
        output += result;

    auto const status = pclose(pipe);
    if (status != 0)
    {
        // Trim output for error message.
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
            output.pop_back();
        if (!output.empty())
            return std::unexpected(GitCommandError{.message = std::format("{} failed: {}", errorContext, output)});
        return std::unexpected(
            GitCommandError{.message = std::format("{} exited with status {}", errorContext, status)});
    }

    // Trim trailing whitespace.
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
        output.pop_back();

    return GitCommandResult{.output = std::move(output)};
}

auto RunCommandRaw(std::string const& command, std::string_view errorContext)
    -> std::expected<std::string, GitCommandError>
{
    // NOLINTNEXTLINE(cert-env33-c) -- popen is intentional for git subprocess communication
    auto* pipe = popen(command.c_str(), "r");
    if (!pipe)
        return std::unexpected(
            GitCommandError{.message = std::format("Failed to execute {} (is git on PATH?)", errorContext)});

    std::string output;
    std::array<char, 8192> buffer{};
    while (auto const bytesRead = fread(buffer.data(), 1, buffer.size(), pipe))
        output.append(buffer.data(), bytesRead);

    auto const status = pclose(pipe);
    if (status != 0)
        return std::unexpected(
            GitCommandError{.message = std::format("{} failed with status {}", errorContext, status)});

    return output;
}

} // namespace git
