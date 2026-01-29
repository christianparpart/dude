// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace mcpprotocol
{

/// @brief Standard JSON-RPC 2.0 error codes.
enum class JsonRpcErrorCode : int32_t // NOLINT(performance-enum-size)
{
    ParseError = -32700,     ///< Invalid JSON was received.
    InvalidRequest = -32600, ///< The JSON sent is not a valid Request object.
    MethodNotFound = -32601, ///< The method does not exist or is not available.
    InvalidParams = -32602,  ///< Invalid method parameter(s).
    InternalError = -32603,  ///< Internal JSON-RPC error.
};

/// @brief A JSON-RPC 2.0 error object.
struct JsonRpcError
{
    int32_t code = 0;    ///< Error code.
    std::string message; ///< Error message.
    nlohmann::json data; ///< Optional additional data.
};

/// @brief A JSON-RPC 2.0 request (has an id).
struct JsonRpcRequest
{
    nlohmann::json id;     ///< Request identifier (string or number).
    std::string method;    ///< Method name.
    nlohmann::json params; ///< Method parameters (object or array).
};

/// @brief A JSON-RPC 2.0 notification (no id).
struct JsonRpcNotification
{
    std::string method;    ///< Method name.
    nlohmann::json params; ///< Method parameters (object or array).
};

/// @brief A JSON-RPC 2.0 response.
struct JsonRpcResponse
{
    nlohmann::json id;                    ///< Request identifier this response corresponds to.
    std::optional<nlohmann::json> result; ///< The result on success.
    std::optional<JsonRpcError> error;    ///< The error on failure.
};

/// @brief A parsed JSON-RPC 2.0 message, either a request or notification.
using JsonRpcMessage = std::variant<JsonRpcRequest, JsonRpcNotification>;

/// @brief Parses a JSON string into a JSON-RPC 2.0 message.
/// @param jsonStr The raw JSON string.
/// @return A parsed message on success, or a response containing the parse error.
[[nodiscard]] auto ParseMessage(std::string_view jsonStr) -> std::variant<JsonRpcMessage, JsonRpcResponse>;

/// @brief Serializes a JSON-RPC 2.0 response to a JSON string.
/// @param response The response to serialize.
/// @return The JSON string representation.
[[nodiscard]] auto SerializeResponse(JsonRpcResponse const& response) -> std::string;

/// @brief Creates a JSON-RPC 2.0 success response.
/// @param id The request identifier.
/// @param result The result value.
/// @return A success response.
[[nodiscard]] auto MakeSuccessResponse(nlohmann::json id, nlohmann::json result) -> JsonRpcResponse;

/// @brief Creates a JSON-RPC 2.0 error response.
/// @param id The request identifier (may be null for parse errors).
/// @param code The error code.
/// @param message The error message.
/// @param data Optional additional error data.
/// @return An error response.
[[nodiscard]] auto MakeErrorResponse(nlohmann::json id, JsonRpcErrorCode code, std::string message,
                                     nlohmann::json data = nullptr) -> JsonRpcResponse;

} // namespace mcpprotocol
