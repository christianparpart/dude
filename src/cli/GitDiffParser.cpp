// SPDX-License-Identifier: Apache-2.0

#include "GitDiffParser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <format>
#include <ranges>
#include <sstream>
#include <string_view>

namespace cli
{

auto GitDiffParser::runGitDiff(std::filesystem::path const& projectRoot, std::string const& baseRef)
    -> std::expected<std::string, GitDiffError>
{
    // Build the command: git -C <dir> diff --no-color -U0 <baseRef>...HEAD
    auto const command = std::format("git -C {} diff --no-color -U0 {}...HEAD 2>&1", projectRoot.string(), baseRef);

    // NOLINTNEXTLINE(cert-env33-c) -- popen is intentional for git subprocess communication
    auto* pipe = popen(command.c_str(), "r");
    if (!pipe)
        return std::unexpected(GitDiffError{.message = "Failed to execute git diff (is git on PATH?)"});

    std::string output;
    std::array<char, 4096> buffer{};
    while (auto* result = fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
        output += result;

    auto const status = pclose(pipe);
    if (status != 0)
    {
        // If git produced output, use it as the error message (trimmed).
        if (!output.empty())
        {
            // Trim trailing newlines.
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
                output.pop_back();
            return std::unexpected(GitDiffError{.message = std::format("git diff failed: {}", output)});
        }
        return std::unexpected(GitDiffError{.message = std::format("git diff exited with status {}", status)});
    }

    return output;
}

namespace
{

/// @brief Checks whether a file path matches any of the given extensions (case-insensitive).
auto matchesExtension(std::filesystem::path const& filePath, std::vector<std::string> const& extensions) -> bool
{
    if (extensions.empty())
        return true;

    auto ext = filePath.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return std::tolower(c); });

    return std::ranges::any_of(extensions,
                               [&ext](std::string const& allowed)
                               {
                                   auto lower = allowed;
                                   std::ranges::transform(lower, lower.begin(),
                                                          [](unsigned char c) { return std::tolower(c); });
                                   return ext == lower;
                               });
}

/// @brief Parses a hunk header like "@@ -a,b +c,d @@" and extracts the new-file line range.
///
/// Formats handled:
///   @@ -a,b +c,d @@ ...    → LineRange{c, c+d-1}
///   @@ -a,b +c @@           → LineRange{c, c}      (single line, count=1 implied)
///   @@ -a +c,d @@           → (old side single line)
///   @@ -a,b +c,0 @@        → skipped (pure deletion in new file)
auto parseHunkHeader(std::string_view line) -> std::optional<codedup::LineRange>
{
    // Find the "+start" or "+start,count" portion.
    auto const plusPos = line.find('+', 3); // Skip past "@@ -"
    if (plusPos == std::string_view::npos)
        return std::nullopt;

    auto const afterPlus = line.substr(plusPos + 1);
    auto const spacePos = afterPlus.find(' ');
    auto const rangeStr = afterPlus.substr(0, spacePos);

    uint32_t start = 0;
    uint32_t count = 1; // Default count is 1 (single-line change).

    auto const commaPos = rangeStr.find(',');
    if (commaPos != std::string_view::npos)
    {
        auto const startStr = rangeStr.substr(0, commaPos);
        auto const countStr = rangeStr.substr(commaPos + 1);

        auto [ptr1, ec1] = std::from_chars(startStr.data(), startStr.data() + startStr.size(), start);
        if (ec1 != std::errc{})
            return std::nullopt;

        auto [ptr2, ec2] = std::from_chars(countStr.data(), countStr.data() + countStr.size(), count);
        if (ec2 != std::errc{})
            return std::nullopt;
    }
    else
    {
        auto [ptr, ec] = std::from_chars(rangeStr.data(), rangeStr.data() + rangeStr.size(), start);
        if (ec != std::errc{})
            return std::nullopt;
    }

    // count=0 means pure deletion on new side — no added lines.
    if (count == 0)
        return std::nullopt;

    return codedup::LineRange{.startLine = start, .endLine = start + count - 1};
}

} // namespace

auto GitDiffParser::parseDiffOutput(std::string const& diffOutput, std::vector<std::string> const& extensions)
    -> codedup::DiffResult
{
    codedup::DiffResult result;
    std::filesystem::path currentFile;
    bool skipCurrentFile = false;

    std::istringstream stream(diffOutput);
    std::string lineBuffer;

    while (std::getline(stream, lineBuffer))
    {
        auto const line = std::string_view(lineBuffer);

        // Detect diff --git header: "diff --git a/path b/path"
        if (line.starts_with("diff --git "))
        {
            // Extract the b/ path (new file path).
            auto const bPos = line.rfind(" b/");
            if (bPos == std::string_view::npos)
            {
                skipCurrentFile = true;
                continue;
            }

            currentFile = std::filesystem::path(std::string(line.substr(bPos + 3)));
            skipCurrentFile = false;

            // Filter by extension.
            if (!matchesExtension(currentFile, extensions))
            {
                skipCurrentFile = true;
                continue;
            }

            continue;
        }

        // Skip deleted files: new file is /dev/null
        if (line.starts_with("+++ /dev/null"))
        {
            skipCurrentFile = true;
            continue;
        }

        // Skip binary files
        if (line.starts_with("Binary files "))
        {
            skipCurrentFile = true;
            continue;
        }

        if (skipCurrentFile)
            continue;

        // Parse hunk headers: @@ ... @@
        if (line.starts_with("@@"))
        {
            auto const hunkRange = parseHunkHeader(line);
            if (!hunkRange)
                continue;

            // Find or create FileChanges entry for the current file.
            auto it =
                std::ranges::find_if(result, [&currentFile](auto const& fc) { return fc.filePath == currentFile; });
            if (it == result.end())
            {
                result.push_back({.filePath = currentFile, .changedRanges = {}});
                it = result.end() - 1;
            }

            it->changedRanges.push_back(*hunkRange);
        }
    }

    return result;
}

} // namespace cli
