// SPDX-License-Identifier: Apache-2.0

#include <mcpprotocol/McpProtocol.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace mcpprotocol;

// ---------------------------------------------------------------------------
// BuildInitializeResult tests
// ---------------------------------------------------------------------------

TEST_CASE("BuildInitializeResult.ContainsProtocolVersion", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}}, true, true);
    CHECK(result["protocolVersion"] == kProtocolVersion);
}

TEST_CASE("BuildInitializeResult.ContainsServerInfo", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "myserver", .version = "2.0", .title = "My Server", .description = "A test server", .websiteUrl = {}},
        false, false);
    CHECK(result["serverInfo"]["name"] == "myserver");
    CHECK(result["serverInfo"]["version"] == "2.0");
    CHECK(result["serverInfo"]["title"] == "My Server");
    CHECK(result["serverInfo"]["description"] == "A test server");
    CHECK_FALSE(result["serverInfo"].contains("websiteUrl"));
}

TEST_CASE("BuildInitializeResult.ServerInfoOmitsEmptyOptionalFields", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "s", .version = "1", .title = {}, .description = {}, .websiteUrl = {}}, false, false);
    CHECK_FALSE(result["serverInfo"].contains("title"));
    CHECK_FALSE(result["serverInfo"].contains("description"));
    CHECK_FALSE(result["serverInfo"].contains("websiteUrl"));
}

TEST_CASE("BuildInitializeResult.CapabilitiesWithTools", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "s", .version = "1", .title = {}, .description = {}, .websiteUrl = {}}, true, false);
    CHECK(result["capabilities"].contains("tools"));
    CHECK_FALSE(result["capabilities"].contains("prompts"));
}

TEST_CASE("BuildInitializeResult.CapabilitiesWithPrompts", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "s", .version = "1", .title = {}, .description = {}, .websiteUrl = {}}, false, true);
    CHECK_FALSE(result["capabilities"].contains("tools"));
    CHECK(result["capabilities"].contains("prompts"));
}

TEST_CASE("BuildInitializeResult.CapabilitiesWithBoth", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "s", .version = "1", .title = {}, .description = {}, .websiteUrl = {}}, true, true);
    CHECK(result["capabilities"].contains("tools"));
    CHECK(result["capabilities"].contains("prompts"));
}

TEST_CASE("BuildInitializeResult.EmptyCapabilities", "[mcp][protocol]")
{
    auto const result = BuildInitializeResult(
        {.name = "s", .version = "1", .title = {}, .description = {}, .websiteUrl = {}}, false, false);
    CHECK(result["capabilities"].empty());
}

// ---------------------------------------------------------------------------
// BuildToolsListResult tests
// ---------------------------------------------------------------------------

TEST_CASE("BuildToolsListResult.EmptyList", "[mcp][protocol]")
{
    auto const result = BuildToolsListResult({});
    CHECK(result["tools"].is_array());
    CHECK(result["tools"].empty());
}

TEST_CASE("BuildToolsListResult.SingleTool", "[mcp][protocol]")
{
    std::vector<ToolDescriptor> tools{
        {.name = "my_tool",
         .title = "My Tool",
         .description = "Does things",
         .inputSchema = {{"type", "object"}},
         .outputSchema = nullptr,
         .annotations =
             {.readOnlyHint = true, .destructiveHint = false, .idempotentHint = true, .openWorldHint = false}},
    };
    auto const result = BuildToolsListResult(tools);
    REQUIRE(result["tools"].size() == 1);
    CHECK(result["tools"][0]["name"] == "my_tool");
    CHECK(result["tools"][0]["title"] == "My Tool");
    CHECK(result["tools"][0]["description"] == "Does things");
    CHECK(result["tools"][0]["inputSchema"]["type"] == "object");
    CHECK_FALSE(result["tools"][0].contains("outputSchema"));
    CHECK(result["tools"][0]["annotations"]["readOnlyHint"] == true);
    CHECK(result["tools"][0]["annotations"]["destructiveHint"] == false);
    CHECK(result["tools"][0]["annotations"]["idempotentHint"] == true);
    CHECK(result["tools"][0]["annotations"]["openWorldHint"] == false);
}

// ---------------------------------------------------------------------------
// BuildPromptsListResult tests
// ---------------------------------------------------------------------------

TEST_CASE("BuildPromptsListResult.EmptyList", "[mcp][protocol]")
{
    auto const result = BuildPromptsListResult({});
    CHECK(result["prompts"].is_array());
    CHECK(result["prompts"].empty());
}

TEST_CASE("BuildPromptsListResult.SinglePrompt", "[mcp][protocol]")
{
    std::vector<PromptDescriptor> prompts{
        {.name = "my_prompt",
         .title = "My Prompt",
         .description = "A prompt",
         .arguments = nlohmann::json::array({{{"name", "arg1"}}})},
    };
    auto const result = BuildPromptsListResult(prompts);
    REQUIRE(result["prompts"].size() == 1);
    CHECK(result["prompts"][0]["name"] == "my_prompt");
    CHECK(result["prompts"][0]["title"] == "My Prompt");
    CHECK(result["prompts"][0]["arguments"].size() == 1);
}

// ---------------------------------------------------------------------------
// BuildToolResult tests
// ---------------------------------------------------------------------------

TEST_CASE("BuildToolResult.TextContent", "[mcp][protocol]")
{
    auto const result = BuildToolResult("hello world");
    REQUIRE(result["content"].is_array());
    REQUIRE(result["content"].size() == 1);
    CHECK(result["content"][0]["type"] == "text");
    CHECK(result["content"][0]["text"] == "hello world");
    CHECK_FALSE(result.contains("isError"));
}

TEST_CASE("BuildToolResult.ErrorContent", "[mcp][protocol]")
{
    auto const result = BuildToolResult("something failed", true);
    CHECK(result["isError"] == true);
    CHECK(result["content"][0]["text"] == "something failed");
}

TEST_CASE("BuildToolResultJson.SerializesJson", "[mcp][protocol]")
{
    auto const data = nlohmann::json{{"key", "value"}};
    auto const result = BuildToolResultJson(data);
    auto const text = result["content"][0]["text"].get<std::string>();
    auto const parsed = nlohmann::json::parse(text);
    CHECK(parsed["key"] == "value");
}
