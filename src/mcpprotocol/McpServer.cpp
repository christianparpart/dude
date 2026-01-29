// SPDX-License-Identifier: Apache-2.0
#include <mcpprotocol/McpServer.hpp>

#include <algorithm>
#include <ranges>
#include <string>

namespace mcpprotocol
{

McpServer::McpServer(ServerInfo info) : _info(std::move(info)) {}

void McpServer::RegisterTool(ToolDescriptor descriptor, ToolHandler handler)
{
    _tools.emplace_back(std::move(descriptor), std::move(handler));
}

void McpServer::RegisterPrompt(PromptDescriptor descriptor, PromptHandler handler)
{
    _prompts.emplace_back(std::move(descriptor), std::move(handler));
}

auto McpServer::Run(std::istream& in, std::ostream& out) -> int
{
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;

        auto parsed = ParseMessage(line);

        if (auto* errorResponse = std::get_if<JsonRpcResponse>(&parsed))
        {
            out << SerializeResponse(*errorResponse) << '\n';
            out.flush();
            continue;
        }

        auto& message = std::get<JsonRpcMessage>(parsed);
        auto response = HandleMessage(message);
        if (response)
        {
            out << SerializeResponse(*response) << '\n';
            out.flush();
        }
    }

    return 0;
}

auto McpServer::HandleMessage(JsonRpcMessage const& message) -> std::optional<JsonRpcResponse>
{
    if (auto const* req = std::get_if<JsonRpcRequest>(&message))
        return HandleRequest(*req);

    if (auto const* notif = std::get_if<JsonRpcNotification>(&message))
    {
        HandleNotification(*notif);
        return std::nullopt;
    }

    return std::nullopt;
}

auto McpServer::HandleRequest(JsonRpcRequest const& req) -> JsonRpcResponse
{
    if (req.method == "initialize")
        return HandleInitialize(req);

    // Before initialization, only initialize is allowed.
    if (_state == ServerState::Uninitialized)
        return MakeErrorResponse(req.id, JsonRpcErrorCode::InvalidRequest, "Server not initialized");

    if (req.method == "ping")
        return HandlePing(req);
    if (req.method == "tools/list")
        return HandleToolsList(req);
    if (req.method == "tools/call")
        return HandleToolsCall(req);
    if (req.method == "prompts/list")
        return HandlePromptsList(req);
    if (req.method == "prompts/get")
        return HandlePromptsGet(req);

    return MakeErrorResponse(req.id, JsonRpcErrorCode::MethodNotFound, "Method not found: " + req.method);
}

void McpServer::HandleNotification(JsonRpcNotification const& notif)
{
    if (notif.method == "notifications/initialized")
    {
        if (_state == ServerState::Initializing)
            _state = ServerState::Running;
    }
    // Notifications for cancelled, progress, etc. are silently ignored.
}

auto McpServer::HandleInitialize(JsonRpcRequest const& req) -> JsonRpcResponse
{
    if (_state != ServerState::Uninitialized)
        return MakeErrorResponse(req.id, JsonRpcErrorCode::InvalidRequest, "Server already initialized");

    _state = ServerState::Initializing;

    auto const hasTools = !_tools.empty();
    auto const hasPrompts = !_prompts.empty();
    auto result = BuildInitializeResult(_info, hasTools, hasPrompts);

    return MakeSuccessResponse(req.id, std::move(result));
}

auto McpServer::HandleToolsList(JsonRpcRequest const& req) -> JsonRpcResponse
{
    std::vector<ToolDescriptor> descriptors;
    descriptors.reserve(_tools.size());
    for (auto const& [desc, handler] : _tools)
        descriptors.push_back(desc);

    return MakeSuccessResponse(req.id, BuildToolsListResult(descriptors));
}

auto McpServer::HandleToolsCall(JsonRpcRequest const& req) -> JsonRpcResponse
{
    if (!req.params.contains("name") || !req.params["name"].is_string())
        return MakeErrorResponse(req.id, JsonRpcErrorCode::InvalidParams, "Missing required parameter: name");

    auto const toolName = req.params["name"].get<std::string>();
    auto const arguments = req.params.value("arguments", nlohmann::json::object());

    auto const it = std::ranges::find_if(_tools, [&](auto const& pair) { return pair.first.name == toolName; });

    if (it == _tools.end())
        return MakeErrorResponse(req.id, JsonRpcErrorCode::InvalidParams, "Unknown tool: " + toolName);

    auto result = it->second(arguments);
    if (result)
        return MakeSuccessResponse(req.id, *result);

    return MakeSuccessResponse(req.id, BuildToolResult(result.error(), /*isError=*/true));
}

auto McpServer::HandlePromptsList(JsonRpcRequest const& req) -> JsonRpcResponse
{
    std::vector<PromptDescriptor> descriptors;
    descriptors.reserve(_prompts.size());
    for (auto const& [desc, handler] : _prompts)
        descriptors.push_back(desc);

    return MakeSuccessResponse(req.id, BuildPromptsListResult(descriptors));
}

auto McpServer::HandlePromptsGet(JsonRpcRequest const& req) -> JsonRpcResponse
{
    if (!req.params.contains("name") || !req.params["name"].is_string())
        return MakeErrorResponse(req.id, JsonRpcErrorCode::InvalidParams, "Missing required parameter: name");

    auto const promptName = req.params["name"].get<std::string>();
    auto const arguments = req.params.value("arguments", nlohmann::json::object());

    auto const it = std::ranges::find_if(_prompts, [&](auto const& pair) { return pair.first.name == promptName; });

    if (it == _prompts.end())
        return MakeErrorResponse(req.id, JsonRpcErrorCode::InvalidParams, "Unknown prompt: " + promptName);

    auto result = it->second(arguments);
    if (result)
        return MakeSuccessResponse(req.id, *result);

    return MakeErrorResponse(req.id, JsonRpcErrorCode::InternalError, result.error());
}

auto McpServer::HandlePing(JsonRpcRequest const& req) -> JsonRpcResponse
{
    return MakeSuccessResponse(req.id, nlohmann::json::object());
}

} // namespace mcpprotocol
