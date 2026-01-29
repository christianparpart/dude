// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mcpprotocol/JsonRpc.hpp>
#include <mcpprotocol/McpProtocol.hpp>

#include <expected>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mcpprotocol
{

/// @brief Callback for tool execution. Returns JSON result or error string.
using ToolHandler = std::function<std::expected<nlohmann::json, std::string>(nlohmann::json const& arguments)>;

/// @brief Callback for prompt execution. Returns JSON result or error string.
using PromptHandler = std::function<std::expected<nlohmann::json, std::string>(nlohmann::json const& arguments)>;

/// @brief MCP server that implements the JSON-RPC 2.0 stdio transport.
///
/// Manages the MCP lifecycle state machine, dispatches method calls to
/// registered tool and prompt handlers, and runs a newline-delimited
/// JSON loop over stdin/stdout.
class McpServer
{
public:
    /// @brief Constructs an MCP server with the given identity.
    /// @param info Server name and version information.
    explicit McpServer(ServerInfo info);

    /// @brief Registers a tool with its descriptor and handler.
    /// @param descriptor The tool's metadata and input schema.
    /// @param handler The callback to invoke when the tool is called.
    void RegisterTool(ToolDescriptor descriptor, ToolHandler handler);

    /// @brief Registers a prompt with its descriptor and handler.
    /// @param descriptor The prompt's metadata and argument descriptors.
    /// @param handler The callback to invoke when the prompt is retrieved.
    void RegisterPrompt(PromptDescriptor descriptor, PromptHandler handler);

    /// @brief Runs the stdio main loop (blocking).
    ///
    /// Reads newline-delimited JSON from the input stream, processes each
    /// message, and writes responses to the output stream.
    ///
    /// @param in Input stream (default: std::cin).
    /// @param out Output stream (default: std::cout).
    /// @return Exit code (0 on clean shutdown).
    auto Run(std::istream& in = std::cin, std::ostream& out = std::cout) -> int;

    /// @brief Processes a single JSON-RPC message (for testing).
    /// @param message The parsed message to handle.
    /// @return A response if the message was a request, or std::nullopt for notifications.
    [[nodiscard]] auto HandleMessage(JsonRpcMessage const& message) -> std::optional<JsonRpcResponse>;

    /// @brief Returns the current server state.
    [[nodiscard]] auto State() const -> ServerState { return _state; }

private:
    ServerState _state = ServerState::Uninitialized;
    ServerInfo _info;
    std::vector<std::pair<ToolDescriptor, ToolHandler>> _tools;
    std::vector<std::pair<PromptDescriptor, PromptHandler>> _prompts;

    [[nodiscard]] auto HandleRequest(JsonRpcRequest const& req) -> JsonRpcResponse;
    void HandleNotification(JsonRpcNotification const& notif);
    [[nodiscard]] auto HandleInitialize(JsonRpcRequest const& req) -> JsonRpcResponse;
    [[nodiscard]] auto HandleToolsList(JsonRpcRequest const& req) -> JsonRpcResponse;
    [[nodiscard]] auto HandleToolsCall(JsonRpcRequest const& req) -> JsonRpcResponse;
    [[nodiscard]] auto HandlePromptsList(JsonRpcRequest const& req) -> JsonRpcResponse;
    [[nodiscard]] auto HandlePromptsGet(JsonRpcRequest const& req) -> JsonRpcResponse;
    [[nodiscard]] static auto HandlePing(JsonRpcRequest const& req) -> JsonRpcResponse;
};

} // namespace mcpprotocol
