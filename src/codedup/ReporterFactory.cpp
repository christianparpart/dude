// SPDX-License-Identifier: Apache-2.0
#include <codedup/ConsoleReporter.hpp>
#include <codedup/JsonReporter.hpp>
#include <codedup/ReporterFactory.hpp>

#include <format>
#include <memory>

namespace codedup
{

auto ParseReporterSpec(std::string_view spec) -> std::expected<ReporterSpec, ReporterError>
{
    if (spec.empty() || spec == "console")
        return ReporterSpec{.name = "console", .outputPath = std::nullopt};

    if (spec == "json")
        return ReporterSpec{.name = "json", .outputPath = std::nullopt};

    if (spec.starts_with("json:"))
    {
        auto const options = spec.substr(5);
        if (options.starts_with("file="))
        {
            auto const path = options.substr(5);
            if (path.empty())
                return std::unexpected(ReporterError{.message = "Empty file path in reporter spec"});
            return ReporterSpec{.name = "json", .outputPath = std::string(path)};
        }
        return std::unexpected(ReporterError{.message = std::format("Unknown json reporter option: {}", options)});
    }

    return std::unexpected(ReporterError{.message = std::format("Unknown reporter: {}", spec)});
}

auto CreateReporter(std::string_view spec, ReporterConfig const& consoleConfig)
    -> std::expected<std::unique_ptr<Reporter>, ReporterError>
{
    return ParseReporterSpec(spec).transform(
        [&](ReporterSpec const& parsed) -> std::unique_ptr<Reporter>
        {
            if (parsed.name == "json")
                return std::make_unique<JsonReporter>();
            return std::make_unique<ConsoleReporter>(consoleConfig);
        });
}

} // namespace codedup
