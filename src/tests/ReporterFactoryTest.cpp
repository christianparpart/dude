// SPDX-License-Identifier: Apache-2.0
#include <dude/ConsoleReporter.hpp>
#include <dude/JsonReporter.hpp>
#include <dude/ReporterFactory.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

TEST_CASE("ParseReporterSpec.EmptyDefaultsToConsole", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("");
    REQUIRE(result.has_value());
    CHECK(result->name == "console");
    CHECK(!result->outputPath.has_value());
}

TEST_CASE("ParseReporterSpec.Console", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("console");
    REQUIRE(result.has_value());
    CHECK(result->name == "console");
    CHECK(!result->outputPath.has_value());
}

TEST_CASE("ParseReporterSpec.Json", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("json");
    REQUIRE(result.has_value());
    CHECK(result->name == "json");
    CHECK(!result->outputPath.has_value());
}

TEST_CASE("ParseReporterSpec.JsonWithFile", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("json:file=report.json");
    REQUIRE(result.has_value());
    CHECK(result->name == "json");
    REQUIRE(result->outputPath.has_value());
    CHECK(result->outputPath.value() == "report.json"); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("ParseReporterSpec.JsonWithEmptyFile", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("json:file=");
    REQUIRE(!result.has_value());
    CHECK(result.error().message.contains("Empty file path"));
}

TEST_CASE("ParseReporterSpec.UnknownReporter", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("xml");
    REQUIRE(!result.has_value());
    CHECK(result.error().message.contains("Unknown reporter"));
}

TEST_CASE("ParseReporterSpec.UnknownJsonOption", "[reporter][factory]")
{
    auto const result = ParseReporterSpec("json:unknown=value");
    REQUIRE(!result.has_value());
    CHECK(result.error().message.contains("Unknown json reporter option"));
}

TEST_CASE("CreateReporter.DefaultCreatesConsole", "[reporter][factory]")
{
    auto const result = CreateReporter("");
    REQUIRE(result.has_value());
    CHECK(dynamic_cast<ConsoleReporter*>(result->get()) != nullptr);
}

TEST_CASE("CreateReporter.ConsoleCreatesConsole", "[reporter][factory]")
{
    auto const result = CreateReporter("console", {.useColor = false});
    REQUIRE(result.has_value());
    CHECK(dynamic_cast<ConsoleReporter*>(result->get()) != nullptr);
}

TEST_CASE("CreateReporter.JsonCreatesJson", "[reporter][factory]")
{
    auto const result = CreateReporter("json");
    REQUIRE(result.has_value());
    CHECK(dynamic_cast<JsonReporter*>(result->get()) != nullptr);
}

TEST_CASE("CreateReporter.JsonFileCreatesJson", "[reporter][factory]")
{
    auto const result = CreateReporter("json:file=out.json");
    REQUIRE(result.has_value());
    CHECK(dynamic_cast<JsonReporter*>(result->get()) != nullptr);
}

TEST_CASE("CreateReporter.UnknownReturnsError", "[reporter][factory]")
{
    auto const result = CreateReporter("xml");
    REQUIRE(!result.has_value());
    CHECK(result.error().message.contains("Unknown reporter"));
}
