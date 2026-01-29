// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mcp/AnalysisSession.hpp>
#include <mcpprotocol/McpServer.hpp>

namespace mcp
{

/// @brief Registers all CodeDupDetector MCP tools and prompts on the given server.
/// @param server The MCP server to register on.
/// @param session The analysis session to use for all tool invocations.
void RegisterCodeDupTools(mcpprotocol::McpServer& server, AnalysisSession& session);

} // namespace mcp
