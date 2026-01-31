// SPDX-License-Identifier: Apache-2.0

#include <mcpprotocol/McpServer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>

using namespace mcpprotocol;

namespace
{

/// @brief Helper to create a server with test info.
auto MakeTestServer() -> McpServer
{
    return McpServer(
        {.name = "test-server", .version = DUDE_VERSION, .title = {}, .description = {}, .websiteUrl = {}});
}

/// @brief Helper to send a request and get a response via HandleMessage.
auto SendRequest(McpServer& server, nlohmann::json id, std::string method, nlohmann::json params = {})
    -> std::optional<JsonRpcResponse>
{
    JsonRpcRequest req;
    req.id = std::move(id);
    req.method = std::move(method);
    req.params = std::move(params);
    return server.HandleMessage(JsonRpcMessage{req});
}

/// @brief Helper to send a notification via HandleMessage.
void SendNotification(McpServer& server, std::string method, nlohmann::json params = {})
{
    JsonRpcNotification notif;
    notif.method = std::move(method);
    notif.params = std::move(params);
    (void)server.HandleMessage(JsonRpcMessage{notif});
}

/// @brief Initializes a server through the full handshake.
void InitializeServer(McpServer& server)
{
    auto resp = SendRequest(server, 1, "initialize",
                            {{"protocolVersion", kProtocolVersion},
                             {"capabilities", nlohmann::json::object()},
                             {"clientInfo", {{"name", "test-client"}, {"version", "1.0"}}}});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value()); // NOLINT(bugprone-unchecked-optional-access)
    SendNotification(server, "notifications/initialized");
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle tests
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.InitialState", "[mcp][server]")
{
    auto server = MakeTestServer();
    CHECK(server.State() == ServerState::Uninitialized);
}

TEST_CASE("McpServer.InitializeTransitionsToInitializing", "[mcp][server]")
{
    auto server = MakeTestServer();
    auto resp = SendRequest(server, 1, "initialize",
                            {{"protocolVersion", kProtocolVersion},
                             {"capabilities", nlohmann::json::object()},
                             {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());         // NOLINT(bugprone-unchecked-optional-access)
    auto const& result = resp->result.value(); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(result["protocolVersion"] == kProtocolVersion);
    CHECK(result["serverInfo"]["name"] == "test-server");
    CHECK(server.State() == ServerState::Initializing);
}

TEST_CASE("McpServer.InitializedNotificationTransitionsToRunning", "[mcp][server]")
{
    auto server = MakeTestServer();
    SendRequest(server, 1, "initialize",
                {{"protocolVersion", kProtocolVersion},
                 {"capabilities", nlohmann::json::object()},
                 {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}});
    SendNotification(server, "notifications/initialized");
    CHECK(server.State() == ServerState::Running);
}

TEST_CASE("McpServer.DoubleInitializeReturnsError", "[mcp][server]")
{
    auto server = MakeTestServer();
    SendRequest(server, 1, "initialize",
                {{"protocolVersion", kProtocolVersion},
                 {"capabilities", nlohmann::json::object()},
                 {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}});
    auto resp = SendRequest(server, 2, "initialize", nlohmann::json::object());
    REQUIRE(resp.has_value());
    CHECK(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpServer.RequestBeforeInitializeReturnsError", "[mcp][server]")
{
    auto server = MakeTestServer();
    auto resp = SendRequest(server, 1, "tools/list");
    REQUIRE(resp.has_value());
    REQUIRE(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp->error.value().code == static_cast<int32_t>(JsonRpcErrorCode::InvalidRequest));
}

// ---------------------------------------------------------------------------
// Ping tests
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.Ping", "[mcp][server]")
{
    auto server = MakeTestServer();
    InitializeServer(server);
    auto resp = SendRequest(server, 10, "ping");
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());       // NOLINT(bugprone-unchecked-optional-access)
    CHECK(resp->result.value().is_object()); // NOLINT(bugprone-unchecked-optional-access)
}

// ---------------------------------------------------------------------------
// Tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.ToolsList", "[mcp][server]")
{
    auto server = MakeTestServer();
    server.RegisterTool({.name = "my_tool",
                         .title = {},
                         .description = "A test tool",
                         .inputSchema = {{"type", "object"}},
                         .outputSchema = nullptr,
                         .annotations = {}},
                        [](nlohmann::json const&) -> std::expected<nlohmann::json, std::string>
                        { return BuildToolResult("result"); });

    InitializeServer(server);
    auto resp = SendRequest(server, 2, "tools/list");
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());         // NOLINT(bugprone-unchecked-optional-access)
    auto const& result = resp->result.value(); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(result["tools"].size() == 1);
    CHECK(result["tools"][0]["name"] == "my_tool");
}

TEST_CASE("McpServer.ToolsCall.Success", "[mcp][server]")
{
    auto server = MakeTestServer();
    server.RegisterTool({.name = "echo",
                         .title = {},
                         .description = "Echoes input",
                         .inputSchema = {{"type", "object"}},
                         .outputSchema = nullptr,
                         .annotations = {}},
                        [](nlohmann::json const& args) -> std::expected<nlohmann::json, std::string>
                        { return BuildToolResult(args.value("text", "")); });

    InitializeServer(server);
    auto resp = SendRequest(server, 3, "tools/call", {{"name", "echo"}, {"arguments", {{"text", "hello"}}}});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                            // NOLINT(bugprone-unchecked-optional-access)
    CHECK(resp->result.value()["content"][0]["text"] == "hello"); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpServer.ToolsCall.UnknownTool", "[mcp][server]")
{
    auto server = MakeTestServer();
    InitializeServer(server);
    auto resp = SendRequest(server, 3, "tools/call", {{"name", "nonexistent"}});
    REQUIRE(resp.has_value());
    REQUIRE(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp->error.value().code == static_cast<int32_t>(JsonRpcErrorCode::InvalidParams));
}

TEST_CASE("McpServer.ToolsCall.MissingName", "[mcp][server]")
{
    auto server = MakeTestServer();
    InitializeServer(server);
    auto resp = SendRequest(server, 3, "tools/call", {{"arguments", nlohmann::json::object()}});
    REQUIRE(resp.has_value());
    CHECK(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpServer.ToolsCall.HandlerError", "[mcp][server]")
{
    auto server = MakeTestServer();
    server.RegisterTool({.name = "fail_tool",
                         .title = {},
                         .description = "Fails",
                         .inputSchema = {{"type", "object"}},
                         .outputSchema = nullptr,
                         .annotations = {}},
                        [](nlohmann::json const&) -> std::expected<nlohmann::json, std::string>
                        { return std::unexpected(std::string("tool failed")); });

    InitializeServer(server);
    auto resp = SendRequest(server, 4, "tools/call", {{"name", "fail_tool"}});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());         // NOLINT(bugprone-unchecked-optional-access)
    auto const& result = resp->result.value(); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(result["isError"] == true);
    CHECK(result["content"][0]["text"] == "tool failed");
}

// ---------------------------------------------------------------------------
// Prompt tests
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.PromptsList", "[mcp][server]")
{
    auto server = MakeTestServer();
    server.RegisterPrompt(
        {.name = "my_prompt", .title = {}, .description = "A test prompt", .arguments = nlohmann::json::array()},
        [](nlohmann::json const&) -> std::expected<nlohmann::json, std::string>
        { return nlohmann::json{{"messages", nlohmann::json::array()}}; });

    InitializeServer(server);
    auto resp = SendRequest(server, 5, "prompts/list");
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                  // NOLINT(bugprone-unchecked-optional-access)
    CHECK(resp->result.value()["prompts"].size() == 1); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpServer.PromptsGet.Success", "[mcp][server]")
{
    auto server = MakeTestServer();
    server.RegisterPrompt(
        {.name = "greet", .title = {}, .description = "Greet", .arguments = nlohmann::json::array()},
        [](nlohmann::json const&) -> std::expected<nlohmann::json, std::string>
        {
            return nlohmann::json{
                {"messages",
                 nlohmann::json::array({{{"role", "user"}, {"content", {{"type", "text"}, {"text", "Hello"}}}}})}};
        });

    InitializeServer(server);
    auto resp = SendRequest(server, 6, "prompts/get", {{"name", "greet"}});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                   // NOLINT(bugprone-unchecked-optional-access)
    CHECK(resp->result.value()["messages"].size() == 1); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpServer.PromptsGet.UnknownPrompt", "[mcp][server]")
{
    auto server = MakeTestServer();
    InitializeServer(server);
    auto resp = SendRequest(server, 6, "prompts/get", {{"name", "nonexistent"}});
    REQUIRE(resp.has_value());
    CHECK(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
}

// ---------------------------------------------------------------------------
// Unknown method tests
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.UnknownMethod", "[mcp][server]")
{
    auto server = MakeTestServer();
    InitializeServer(server);
    auto resp = SendRequest(server, 99, "bogus/method");
    REQUIRE(resp.has_value());
    REQUIRE(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp->error.value().code == static_cast<int32_t>(JsonRpcErrorCode::MethodNotFound));
}

// ---------------------------------------------------------------------------
// Notification handling
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.NotificationReturnsNullopt", "[mcp][server]")
{
    auto server = MakeTestServer();
    JsonRpcNotification notif;
    notif.method = "notifications/cancelled";
    auto const resp = server.HandleMessage(JsonRpcMessage{notif});
    CHECK_FALSE(resp.has_value());
}

// ---------------------------------------------------------------------------
// Stdio loop tests
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.RunStdioLoop", "[mcp][server]")
{
    auto server = MakeTestServer();

    std::string input;
    // Line 1: initialize
    input +=
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}})"
        "\n";
    // Line 2: initialized notification
    input += R"({"jsonrpc":"2.0","method":"notifications/initialized"})"
             "\n";
    // Line 3: ping
    input += R"({"jsonrpc":"2.0","id":2,"method":"ping"})"
             "\n";

    std::istringstream in(input);
    std::ostringstream out;

    auto const exitCode = server.Run(in, out);
    CHECK(exitCode == 0);

    // Parse output lines
    auto const output = out.str();
    size_t lineStart = 0;
    int responseCount = 0;
    while (lineStart < output.size())
    {
        auto const lineEnd = output.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            break;
        auto const line = output.substr(lineStart, lineEnd - lineStart);
        if (!line.empty())
        {
            auto const doc = nlohmann::json::parse(line);
            CHECK(doc["jsonrpc"] == "2.0");
            ++responseCount;
        }
        lineStart = lineEnd + 1;
    }
    // Should have 2 responses (initialize + ping), notification produces no response
    CHECK(responseCount == 2);
}

// ---------------------------------------------------------------------------
// Coverage: empty lines in Run() stdio loop (line 29) and parse error path (lines 34-37)
// ---------------------------------------------------------------------------

TEST_CASE("McpServer.RunStdioLoopWithEmptyAndInvalidLines", "[mcp][server]")
{
    auto server = MakeTestServer();

    std::string input;
    // Empty line (should be skipped)
    input += "\n";
    // Invalid JSON (should produce a parse error response)
    input += "not valid json\n";
    // Valid initialize
    input +=
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}})"
        "\n";

    std::istringstream in(input);
    std::ostringstream out;

    auto const exitCode = server.Run(in, out);
    CHECK(exitCode == 0);

    auto const output = out.str();
    // Should have at least 2 responses: one error for invalid JSON, one for initialize
    CHECK(!output.empty());
}
