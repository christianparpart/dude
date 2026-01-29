// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mcpprotocol/JsonRpc.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace mcpprotocol
{

/// @brief The supported MCP protocol version.
inline constexpr auto kProtocolVersion = "2025-11-25";

/// @brief Server lifecycle states.
enum class ServerState : uint8_t
{
    Uninitialized, ///< Server has not received an initialize request.
    Initializing,  ///< Server has received initialize, awaiting initialized notification.
    Running,       ///< Server is fully operational.
    ShuttingDown,  ///< Server is shutting down.
};

/// @brief Server identity information returned during initialization.
struct ServerInfo
{
    std::string name;        ///< Programmatic server identifier.
    std::string version;     ///< Server version string.
    std::string title;       ///< Human-readable display name for UI presentation.
    std::string description; ///< Brief description of the server's purpose.
    std::string websiteUrl;  ///< URL to the server's website or documentation.
};

/// @brief Tool behavior annotations per MCP 2025-11-25 spec.
///
/// These hints inform clients about the tool's side effects and behavior,
/// enabling better UX decisions such as confirmation prompts or retry logic.
struct ToolAnnotations
{
    bool readOnlyHint = true;     ///< Whether the tool only reads data without modifying state.
    bool destructiveHint = false; ///< Whether the tool may perform irreversible destructive operations.
    bool idempotentHint = false;  ///< Whether repeated calls with same args produce the same result.
    bool openWorldHint = true;    ///< Whether the tool interacts with external entities beyond the local environment.
};

/// @brief Descriptor for a registered MCP tool.
struct ToolDescriptor
{
    std::string name;            ///< Unique programmatic identifier for the tool.
    std::string title;           ///< Human-readable display name for UI presentation.
    std::string description;     ///< Human-readable description of the tool's functionality.
    nlohmann::json inputSchema;  ///< JSON Schema defining the tool's expected input arguments.
    nlohmann::json outputSchema; ///< JSON Schema defining the tool's expected output structure. May be null.
    ToolAnnotations annotations; ///< Behavioral hints about the tool for client-side UX decisions.
};

/// @brief Descriptor for a registered MCP prompt.
struct PromptDescriptor
{
    std::string name;         ///< Unique programmatic identifier for the prompt.
    std::string title;        ///< Human-readable display name for UI presentation.
    std::string description;  ///< Human-readable description of the prompt's purpose.
    nlohmann::json arguments; ///< JSON array of argument descriptors for prompt customization.
};

/// @brief Builds the result object for the MCP initialize response.
/// @param info Server identity information.
/// @param hasTools Whether the server supports tools.
/// @param hasPrompts Whether the server supports prompts.
/// @return JSON result object for the initialize response.
[[nodiscard]] auto BuildInitializeResult(ServerInfo const& info, bool hasTools, bool hasPrompts) -> nlohmann::json;

/// @brief Builds the result object for a tools/list response.
/// @param tools The registered tool descriptors.
/// @return JSON result object containing the tools array.
[[nodiscard]] auto BuildToolsListResult(std::vector<ToolDescriptor> const& tools) -> nlohmann::json;

/// @brief Builds the result object for a prompts/list response.
/// @param prompts The registered prompt descriptors.
/// @return JSON result object containing the prompts array.
[[nodiscard]] auto BuildPromptsListResult(std::vector<PromptDescriptor> const& prompts) -> nlohmann::json;

/// @brief Builds a text content tool result.
/// @param text The text content to include.
/// @param isError Whether this result represents an error.
/// @return JSON result object containing the tool result.
[[nodiscard]] auto BuildToolResult(std::string const& text, bool isError = false) -> nlohmann::json;

/// @brief Builds a tool result from a JSON object (serialized as text content).
/// @param data The JSON data to include.
/// @param isError Whether this result represents an error.
/// @return JSON result object containing the tool result.
[[nodiscard]] auto BuildToolResultJson(nlohmann::json const& data, bool isError = false) -> nlohmann::json;

} // namespace mcpprotocol
