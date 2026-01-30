// SPDX-License-Identifier: Apache-2.0

#include <dude/AnalysisScope.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace dude;

// ---------------------------------------------------------------------------
// ParseAnalysisScope tests
// ---------------------------------------------------------------------------

TEST_CASE("ParseAnalysisScope.SingleToken.All", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("all");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::All);
}

TEST_CASE("ParseAnalysisScope.SingleToken.None", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("none");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::None);
}

TEST_CASE("ParseAnalysisScope.SingleToken.InterFile", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("inter-file");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::InterFile);
}

TEST_CASE("ParseAnalysisScope.SingleToken.IntraFile", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("intra-file");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::IntraFile);
}

TEST_CASE("ParseAnalysisScope.SingleToken.InterFunction", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("inter-function");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::InterFunction);
}

TEST_CASE("ParseAnalysisScope.SingleToken.IntraFunction", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("intra-function");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::IntraFunction);
}

TEST_CASE("ParseAnalysisScope.CommaSeparated", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("inter-file,intra-function");
    REQUIRE(result.has_value());
    CHECK(HasScope(*result, AnalysisScope::InterFile));
    CHECK(HasScope(*result, AnalysisScope::IntraFunction));
    CHECK_FALSE(HasScope(*result, AnalysisScope::IntraFile));
}

TEST_CASE("ParseAnalysisScope.WhitespaceTrimming", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("  inter-file , intra-function  ");
    REQUIRE(result.has_value());
    CHECK(HasScope(*result, AnalysisScope::InterFile));
    CHECK(HasScope(*result, AnalysisScope::IntraFunction));
}

TEST_CASE("ParseAnalysisScope.CaseInsensitive", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("Inter-File,INTRA-FUNCTION");
    REQUIRE(result.has_value());
    CHECK(HasScope(*result, AnalysisScope::InterFile));
    CHECK(HasScope(*result, AnalysisScope::IntraFunction));
}

TEST_CASE("ParseAnalysisScope.UnknownToken", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("bogus");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().message.find("bogus") != std::string::npos);
}

TEST_CASE("ParseAnalysisScope.EmptyString", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ParseAnalysisScope.Duplicates", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("inter-file,inter-file");
    REQUIRE(result.has_value());
    CHECK(*result == AnalysisScope::InterFile);
}

TEST_CASE("ParseAnalysisScope.InterFunctionExpands", "[analysisscope]")
{
    auto const result = ParseAnalysisScope("inter-function");
    REQUIRE(result.has_value());
    CHECK(HasScope(*result, AnalysisScope::InterFile));
    CHECK(HasScope(*result, AnalysisScope::IntraFile));
}

// ---------------------------------------------------------------------------
// FormatAnalysisScope tests
// ---------------------------------------------------------------------------

TEST_CASE("FormatAnalysisScope.All", "[analysisscope]")
{
    CHECK(FormatAnalysisScope(AnalysisScope::All) == "all");
}

TEST_CASE("FormatAnalysisScope.None", "[analysisscope]")
{
    CHECK(FormatAnalysisScope(AnalysisScope::None) == "none");
}

TEST_CASE("FormatAnalysisScope.SingleFlag", "[analysisscope]")
{
    CHECK(FormatAnalysisScope(AnalysisScope::InterFile) == "inter-file");
    CHECK(FormatAnalysisScope(AnalysisScope::IntraFile) == "intra-file");
    CHECK(FormatAnalysisScope(AnalysisScope::IntraFunction) == "intra-function");
}

TEST_CASE("FormatAnalysisScope.MultipleFlags", "[analysisscope]")
{
    auto const scope = AnalysisScope::InterFile | AnalysisScope::IntraFunction;
    auto const formatted = FormatAnalysisScope(scope);
    CHECK(formatted == "inter-file, intra-function");
}

TEST_CASE("FormatAnalysisScope.InterFunction", "[analysisscope]")
{
    CHECK(FormatAnalysisScope(AnalysisScope::InterFunction) == "inter-file, intra-file");
}

// ---------------------------------------------------------------------------
// Predicate tests
// ---------------------------------------------------------------------------

TEST_CASE("HasScope.ChecksFlag", "[analysisscope]")
{
    CHECK(HasScope(AnalysisScope::All, AnalysisScope::InterFile));
    CHECK(HasScope(AnalysisScope::All, AnalysisScope::IntraFile));
    CHECK(HasScope(AnalysisScope::All, AnalysisScope::IntraFunction));
    CHECK_FALSE(HasScope(AnalysisScope::InterFile, AnalysisScope::IntraFile));
    CHECK_FALSE(HasScope(AnalysisScope::None, AnalysisScope::InterFile));
}

TEST_CASE("HasInterFunctionScope.DetectsAnyInterFunction", "[analysisscope]")
{
    CHECK(HasInterFunctionScope(AnalysisScope::All));
    CHECK(HasInterFunctionScope(AnalysisScope::InterFile));
    CHECK(HasInterFunctionScope(AnalysisScope::IntraFile));
    CHECK(HasInterFunctionScope(AnalysisScope::InterFunction));
    CHECK_FALSE(HasInterFunctionScope(AnalysisScope::IntraFunction));
    CHECK_FALSE(HasInterFunctionScope(AnalysisScope::None));
}
