// SPDX-License-Identifier: Apache-2.0

#include <mcp/AnalysisSession.hpp>
#include <mcp/McpTooling.hpp>
#include <mcpprotocol/McpServer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace mcp;
using namespace mcpprotocol;

namespace
{

struct TempTestDir
{
    std::filesystem::path root;

    TempTestDir()
    {
        root = std::filesystem::temp_directory_path() / "mcp_tools_test";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }

    TempTestDir(TempTestDir const&) = delete;
    TempTestDir(TempTestDir&&) = delete;
    auto operator=(TempTestDir const&) -> TempTestDir& = delete;
    auto operator=(TempTestDir&&) -> TempTestDir& = delete;

    void WriteFile(std::string const& name, std::string const& content) const
    {
        std::ofstream out(root / name);
        out << content;
    }

    ~TempTestDir() { std::filesystem::remove_all(root); }
};

auto constexpr kDuplicateSource = R"(
void functionA(int x) {
    int result = 0;
    for (int i = 0; i < x; ++i) {
        result += i * 2;
        if (result > 100) {
            result = 100;
        }
    }
    return;
}

void functionB(int y) {
    int result = 0;
    for (int i = 0; i < y; ++i) {
        result += i * 2;
        if (result > 100) {
            result = 100;
        }
    }
    return;
}
)";

/// @brief Helper: fully initialize a server, send a tool call, and return the result JSON.
auto CallTool(McpServer& server, std::string const& toolName, nlohmann::json const& arguments) -> JsonRpcResponse
{
    JsonRpcRequest req;
    req.id = 100;
    req.method = "tools/call";
    req.params = {{"name", toolName}, {"arguments", arguments}};
    auto resp = server.HandleMessage(JsonRpcMessage{req});
    REQUIRE(resp.has_value());
    return *resp; // NOLINT(bugprone-unchecked-optional-access)
}

/// @brief Initialize and ready a server.
void InitServer(McpServer& server)
{
    JsonRpcRequest initReq;
    initReq.id = 1;
    initReq.method = "initialize";
    initReq.params = {{"protocolVersion", kProtocolVersion},
                      {"capabilities", nlohmann::json::object()},
                      {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}};
    (void)server.HandleMessage(JsonRpcMessage{initReq});
    JsonRpcNotification initNotif;
    initNotif.method = "notifications/initialized";
    (void)server.HandleMessage(JsonRpcMessage{initNotif});
}

/// @brief Parse the text content from a tool result.
auto ParseToolResultText(JsonRpcResponse const& resp) -> nlohmann::json
{
    REQUIRE(resp.result.has_value());         // NOLINT(bugprone-unchecked-optional-access)
    auto const& result = resp.result.value(); // NOLINT(bugprone-unchecked-optional-access)
    auto const& content = result["content"];
    REQUIRE(content.is_array());
    REQUIRE(!content.empty());
    auto const text = content[0]["text"].get<std::string>();
    return nlohmann::json::parse(text);
}

} // namespace

// ---------------------------------------------------------------------------
// Tool registration tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.ToolsAreRegistered", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    JsonRpcRequest req;
    req.id = 2;
    req.method = "tools/list";
    auto resp = server.HandleMessage(JsonRpcMessage{req});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                 // NOLINT(bugprone-unchecked-optional-access)
    auto const& tools = resp->result.value()["tools"]; // NOLINT(bugprone-unchecked-optional-access)
    CHECK(tools.size() == 6);

    // Verify all expected tool names
    std::vector<std::string> names;
    for (auto const& t : tools)
        names.push_back(t["name"].get<std::string>());

    CHECK(std::ranges::find(names, "analyze_directory") != names.end());
    CHECK(std::ranges::find(names, "get_clone_groups") != names.end());
    CHECK(std::ranges::find(names, "get_code_block") != names.end());
    CHECK(std::ranges::find(names, "query_file_duplicates") != names.end());
    CHECK(std::ranges::find(names, "get_summary") != names.end());
    CHECK(std::ranges::find(names, "configure_analysis") != names.end());
}

TEST_CASE("McpTools.PromptsAreRegistered", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    JsonRpcRequest req;
    req.id = 3;
    req.method = "prompts/list";
    auto resp = server.HandleMessage(JsonRpcMessage{req});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                     // NOLINT(bugprone-unchecked-optional-access)
    auto const& prompts = resp->result.value()["prompts"]; // NOLINT(bugprone-unchecked-optional-access)
    CHECK(prompts.size() == 2);
}

// ---------------------------------------------------------------------------
// analyze_directory tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.AnalyzeDirectory.Success", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    auto const resp = CallTool(server, "analyze_directory",
                               {{"directory", dir.root.string()}, {"min_tokens", 10}, {"threshold", 0.70}});
    auto const data = ParseToolResultText(resp);
    CHECK(data["total_files"].get<int>() == 1);
    CHECK(data["total_blocks"].get<int>() >= 2);
}

TEST_CASE("McpTools.AnalyzeDirectory.InvalidDirectory", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    auto const resp = CallTool(server, "analyze_directory", {{"directory", "/nonexistent/xyzzy"}});
    REQUIRE(resp.result.has_value());
    CHECK(resp.result.value()["isError"] == true); // NOLINT(bugprone-unchecked-optional-access)
}

// ---------------------------------------------------------------------------
// get_clone_groups tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.GetCloneGroups.RequiresAnalysis", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    auto const resp = CallTool(server, "get_clone_groups", nlohmann::json::object());
    REQUIRE(resp.result.has_value());
    CHECK(resp.result.value()["isError"] == true); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpTools.GetCloneGroups.WithResults", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    CallTool(server, "analyze_directory", {{"directory", dir.root.string()}, {"min_tokens", 10}, {"threshold", 0.70}});

    auto const resp = CallTool(server, "get_clone_groups", {{"limit", 10}});
    auto const data = ParseToolResultText(resp);
    CHECK(data.contains("total_groups"));
    CHECK(data.contains("groups"));
    CHECK(data["groups"].is_array());
}

// ---------------------------------------------------------------------------
// get_code_block tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.GetCodeBlock.RequiresAnalysis", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    auto const resp = CallTool(server, "get_code_block", {{"block_index", 0}});
    REQUIRE(resp.result.has_value());
    CHECK(resp.result.value()["isError"] == true); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpTools.GetCodeBlock.WithResults", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    CallTool(server, "analyze_directory", {{"directory", dir.root.string()}, {"min_tokens", 10}});

    auto const resp = CallTool(server, "get_code_block", {{"block_index", 0}});
    auto const data = ParseToolResultText(resp);
    CHECK(data.contains("name"));
    CHECK(data.contains("source_code"));
    CHECK(!data["source_code"].get<std::string>().empty());
}

// ---------------------------------------------------------------------------
// get_summary tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.GetSummary.TextFormat", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    CallTool(server, "analyze_directory", {{"directory", dir.root.string()}, {"min_tokens", 10}});

    auto const resp = CallTool(server, "get_summary", {{"format", "text"}});
    REQUIRE(resp.result.has_value());
    auto const text =
        resp.result.value()["content"][0]["text"].get<std::string>(); // NOLINT(bugprone-unchecked-optional-access)
    CHECK(text.find("Analysis Summary") != std::string::npos);
}

TEST_CASE("McpTools.GetSummary.JsonFormat", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    CallTool(server, "analyze_directory", {{"directory", dir.root.string()}, {"min_tokens", 10}});

    auto const resp = CallTool(server, "get_summary", {{"format", "json"}});
    auto const data = ParseToolResultText(resp);
    CHECK(data.contains("total_files"));
    CHECK(data.contains("directory"));
    CHECK(data.contains("threshold"));
}

// ---------------------------------------------------------------------------
// configure_analysis tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.ConfigureAnalysis.RequiresAnalysis", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    auto const resp = CallTool(server, "configure_analysis", {{"threshold", 0.95}});
    REQUIRE(resp.result.has_value());
    CHECK(resp.result.value()["isError"] == true); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpTools.ConfigureAnalysis.UpdatesParameters", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    CallTool(server, "analyze_directory", {{"directory", dir.root.string()}, {"min_tokens", 10}, {"threshold", 0.70}});

    auto const resp = CallTool(server, "configure_analysis", {{"threshold", 0.99}});
    auto const data = ParseToolResultText(resp);
    CHECK(data.contains("total_clone_groups"));
}

// ---------------------------------------------------------------------------
// query_file_duplicates tool tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.QueryFileDuplicates.RequiresAnalysis", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    auto const resp = CallTool(server, "query_file_duplicates", {{"file_path", "test.cpp"}});
    REQUIRE(resp.result.has_value());
    CHECK(resp.result.value()["isError"] == true); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpTools.QueryFileDuplicates.WithResults", "[mcp][tools]")
{
    TempTestDir dir;
    dir.WriteFile("test.cpp", kDuplicateSource);

    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    CallTool(server, "analyze_directory", {{"directory", dir.root.string()}, {"min_tokens", 10}, {"threshold", 0.70}});

    auto const resp = CallTool(server, "query_file_duplicates", {{"file_path", "test.cpp"}});
    auto const data = ParseToolResultText(resp);
    CHECK(data.contains("inter_function_clones"));
    CHECK(data.contains("intra_function_clones"));
    CHECK(data["inter_function_clones"].is_array());
}

// ---------------------------------------------------------------------------
// Prompt tests
// ---------------------------------------------------------------------------

TEST_CASE("McpTools.AnalyzeAndReportPrompt", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    JsonRpcRequest req;
    req.id = 10;
    req.method = "prompts/get";
    req.params = {{"name", "analyze_and_report"}, {"arguments", {{"directory", "/tmp/test"}}}};
    auto resp = server.HandleMessage(JsonRpcMessage{req});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                   // NOLINT(bugprone-unchecked-optional-access)
    CHECK(resp->result.value()["messages"].size() == 1); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpTools.ReviewFilePrompt", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    JsonRpcRequest req;
    req.id = 11;
    req.method = "prompts/get";
    req.params = {{"name", "review_file"}, {"arguments", {{"file_path", "/tmp/test.cpp"}}}};
    auto resp = server.HandleMessage(JsonRpcMessage{req});
    REQUIRE(resp.has_value());
    REQUIRE(resp->result.has_value());                   // NOLINT(bugprone-unchecked-optional-access)
    CHECK(resp->result.value()["messages"].size() == 1); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("McpTools.ReviewFilePrompt.MissingArg", "[mcp][tools]")
{
    AnalysisSession session;
    McpServer server({.name = "test", .version = "1.0", .title = {}, .description = {}, .websiteUrl = {}});
    RegisterCodeDupTools(server, session);
    InitServer(server);

    JsonRpcRequest req;
    req.id = 12;
    req.method = "prompts/get";
    req.params = {{"name", "review_file"}, {"arguments", nlohmann::json::object()}};
    auto resp = server.HandleMessage(JsonRpcMessage{req});
    REQUIRE(resp.has_value());
    CHECK(resp->error.has_value()); // NOLINT(bugprone-unchecked-optional-access)
}
