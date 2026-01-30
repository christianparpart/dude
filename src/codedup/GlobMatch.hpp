// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string_view>

namespace codedup
{

/// @brief Simple glob pattern matcher supporting '*' and '?' wildcards.
///
/// '*' matches zero or more characters, '?' matches exactly one character.
/// This provides a portable replacement for POSIX fnmatch().
///
/// @param pattern The glob pattern.
/// @param text The text to match against.
/// @return true if the text matches the pattern.
[[nodiscard]] constexpr auto GlobMatch(std::string_view pattern, std::string_view text) -> bool
{
    size_t pi = 0;
    size_t ti = 0;
    size_t starPi = std::string_view::npos;
    size_t starTi = 0;

    while (ti < text.size())
    {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti]))
        {
            ++pi;
            ++ti;
        }
        else if (pi < pattern.size() && pattern[pi] == '*')
        {
            starPi = pi;
            starTi = ti;
            ++pi;
        }
        else if (starPi != std::string_view::npos)
        {
            pi = starPi + 1;
            ++starTi;
            ti = starTi;
        }
        else
        {
            return false;
        }
    }

    while (pi < pattern.size() && pattern[pi] == '*')
        ++pi;

    return pi == pattern.size();
}

} // namespace codedup
