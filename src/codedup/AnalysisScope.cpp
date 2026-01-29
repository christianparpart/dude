// SPDX-License-Identifier: Apache-2.0

#include <codedup/AnalysisScope.hpp>

#include <algorithm>
#include <format>
#include <string>

namespace codedup
{

namespace
{

/// @brief Trims leading and trailing whitespace from a string view.
[[nodiscard]] constexpr auto Trim(std::string_view sv) -> std::string_view
{
    auto const start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
        return {};
    auto const end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

/// @brief Converts a string view to lowercase in-place (ASCII only).
[[nodiscard]] auto ToLower(std::string_view sv) -> std::string
{
    std::string result(sv);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

} // namespace

auto ParseAnalysisScope(std::string_view input) -> std::expected<AnalysisScope, AnalysisScopeError>
{
    auto result = AnalysisScope::None;
    auto const trimmed = Trim(input);

    if (trimmed.empty())
        return std::unexpected(AnalysisScopeError{.message = "Empty scope string"});

    size_t start = 0;
    while (start <= trimmed.size())
    {
        auto const end = trimmed.find(',', start);
        auto const tokenSv = Trim(trimmed.substr(start, end == std::string_view::npos ? end : end - start));
        auto const token = ToLower(tokenSv);

        if (token.empty())
        {
            start = (end == std::string_view::npos) ? trimmed.size() + 1 : end + 1;
            continue;
        }

        if (token == "all")
            result |= AnalysisScope::All;
        else if (token == "none")
        { /* no-op, None is 0 */
        }
        else if (token == "inter-file")
            result |= AnalysisScope::InterFile;
        else if (token == "intra-file")
            result |= AnalysisScope::IntraFile;
        else if (token == "inter-function")
            result |= AnalysisScope::InterFunction;
        else if (token == "intra-function")
            result |= AnalysisScope::IntraFunction;
        else
            return std::unexpected(AnalysisScopeError{.message = std::format("Unknown scope: '{}'", tokenSv)});

        start = (end == std::string_view::npos) ? trimmed.size() + 1 : end + 1;
    }

    return result;
}

auto FormatAnalysisScope(AnalysisScope scope) -> std::string
{
    if (scope == AnalysisScope::All)
        return "all";
    if (scope == AnalysisScope::None)
        return "none";

    std::string result;
    auto const append = [&](std::string_view name)
    {
        if (!result.empty())
            result += ", ";
        result += name;
    };

    if (HasScope(scope, AnalysisScope::InterFile))
        append("inter-file");
    if (HasScope(scope, AnalysisScope::IntraFile))
        append("intra-file");
    if (HasScope(scope, AnalysisScope::IntraFunction))
        append("intra-function");

    return result;
}

} // namespace codedup
