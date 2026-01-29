// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/Reporter.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace codedup
{

/// @brief Parsed reporter specification from the --reporter CLI flag.
struct ReporterSpec
{
    std::string name;                      ///< Reporter name ("console" or "json").
    std::optional<std::string> outputPath; ///< Optional output file path (e.g. from "json:file=report.json").
};

/// @brief Error returned when reporter creation fails.
struct ReporterError
{
    std::string message; ///< Human-readable error description.
};

/// @brief Parses a reporter specification string.
///
/// Accepted formats:
/// - "" or "console" -> ConsoleReporter
/// - "json" -> JsonReporter (stdout)
/// - "json:file=path" -> JsonReporter (write to file)
///
/// @param spec The reporter specification string from --reporter.
/// @return Parsed ReporterSpec on success, or ReporterError on failure.
[[nodiscard]] CODEDUP_API auto ParseReporterSpec(std::string_view spec) -> std::expected<ReporterSpec, ReporterError>;

/// @brief Creates a reporter from a specification string and console config.
///
/// The consoleConfig is used only when creating a ConsoleReporter.
///
/// @param spec The reporter specification string from --reporter (empty means "console").
/// @param consoleConfig Configuration for the ConsoleReporter (color, source, theme).
/// @return A unique_ptr to the created Reporter, or ReporterError on failure.
[[nodiscard]] CODEDUP_API auto CreateReporter(std::string_view spec, ReporterConfig const& consoleConfig = {})
    -> std::expected<std::unique_ptr<Reporter>, ReporterError>;

} // namespace codedup
