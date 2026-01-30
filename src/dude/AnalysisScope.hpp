// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace dude
{

/// @brief Bitmask enum controlling which duplication scopes are analyzed.
///
/// Scopes can be combined using bitwise OR. The three inter-function sub-scopes
/// (InterFile, IntraFile) compose into InterFunction = InterFile | IntraFile.
enum class AnalysisScope : uint8_t
{
    None = 0,                                    ///< No analysis.
    InterFile = 1,                               ///< Inter-function clones across different files.
    IntraFile = 2,                               ///< Inter-function clones within the same file.
    IntraFunction = 4,                           ///< Clones within a single function body.
    InterFunction = InterFile | IntraFile,       ///< All inter-function clones (= InterFile + IntraFile).
    All = InterFile | IntraFile | IntraFunction, ///< Everything (default).
};

/// @brief Bitwise OR for AnalysisScope values.
[[nodiscard]] constexpr auto operator|(AnalysisScope lhs, AnalysisScope rhs) -> AnalysisScope
{
    return static_cast<AnalysisScope>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

/// @brief Bitwise AND for AnalysisScope values.
[[nodiscard]] constexpr auto operator&(AnalysisScope lhs, AnalysisScope rhs) -> AnalysisScope
{
    return static_cast<AnalysisScope>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

/// @brief Bitwise OR assignment for AnalysisScope values.
constexpr auto operator|=(AnalysisScope& lhs, AnalysisScope rhs) -> AnalysisScope&
{
    lhs = lhs | rhs;
    return lhs;
}

/// @brief Checks whether a specific flag is set in the given scope.
/// @param scope The scope bitmask to test.
/// @param flag The flag to check for.
/// @return True if the flag is set.
[[nodiscard]] constexpr auto HasScope(AnalysisScope scope, AnalysisScope flag) -> bool
{
    return (scope & flag) == flag;
}

/// @brief Checks whether any inter-function scope (InterFile or IntraFile) is active.
/// @param scope The scope bitmask to test.
/// @return True if at least one inter-function sub-scope is set.
[[nodiscard]] constexpr auto HasInterFunctionScope(AnalysisScope scope) -> bool
{
    return (scope & AnalysisScope::InterFunction) != AnalysisScope::None;
}

/// @brief Error information from parsing an analysis scope string.
struct AnalysisScopeError
{
    std::string message; ///< Human-readable error message.
};

/// @brief Parses a comma-separated scope string into an AnalysisScope bitmask.
///
/// Valid tokens (case-insensitive): inter-file, intra-file, inter-function,
/// intra-function, all, none. Whitespace around tokens is trimmed.
///
/// @param input The comma-separated scope string.
/// @return The combined AnalysisScope on success, or an error.
[[nodiscard]] DUDE_API auto ParseAnalysisScope(std::string_view input)
    -> std::expected<AnalysisScope, AnalysisScopeError>;

/// @brief Formats an AnalysisScope bitmask as a human-readable string.
///
/// Returns "all" for All, "none" for None, or a comma-separated list of active
/// scope names for partial bitmasks.
///
/// @param scope The scope bitmask to format.
/// @return Human-readable representation of the scope.
[[nodiscard]] DUDE_API auto FormatAnalysisScope(AnalysisScope scope) -> std::string;

} // namespace dude
