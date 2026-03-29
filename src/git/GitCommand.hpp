// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace git
{

/// @brief Error information from a git subprocess invocation.
struct GitCommandError
{
    std::string message; ///< Description of the error.
};

/// @brief Result of a successful git command execution.
struct GitCommandResult
{
    std::string output; ///< Captured stdout (trimmed of trailing newlines).
};

/// @brief Executes a shell command via popen, captures stdout, and returns the trimmed output.
///
/// This is the single abstraction for all git subprocess calls. Handles:
/// - popen/pclose lifecycle
/// - Error reporting with captured stderr (when redirected via 2>&1)
/// - Trailing newline/whitespace trimming
///
/// @param command The shell command to execute.
/// @param errorContext Human-readable context for error messages (e.g., "git diff").
/// @return The trimmed command output, or an error.
[[nodiscard]] auto RunCommand(std::string const& command, std::string_view errorContext)
    -> std::expected<GitCommandResult, GitCommandError>;

/// @brief Executes a shell command and returns raw binary output without trimming.
///
/// Used for commands that produce NUL-delimited or binary output (e.g., git ls-files -z).
///
/// @param command The shell command to execute.
/// @param errorContext Human-readable context for error messages.
/// @return The raw command output, or an error.
[[nodiscard]] auto RunCommandRaw(std::string const& command, std::string_view errorContext)
    -> std::expected<std::string, GitCommandError>;

} // namespace git
