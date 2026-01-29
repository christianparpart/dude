// SPDX-License-Identifier: Apache-2.0
#include <mcpprotocol/JsonRpc.hpp>

namespace mcpprotocol
{

auto ParseMessage(std::string_view jsonStr) -> std::variant<JsonRpcMessage, JsonRpcResponse>
{
    nlohmann::json doc;
    try
    {
        doc = nlohmann::json::parse(jsonStr);
    }
    catch (nlohmann::json::parse_error const& e)
    {
        return MakeErrorResponse(nullptr, JsonRpcErrorCode::ParseError, e.what());
    }

    if (!doc.is_object())
        return MakeErrorResponse(nullptr, JsonRpcErrorCode::InvalidRequest, "Message must be a JSON object");

    if (!doc.contains("jsonrpc") || doc["jsonrpc"] != "2.0")
        return MakeErrorResponse(nullptr, JsonRpcErrorCode::InvalidRequest, "Missing or invalid jsonrpc version");

    if (!doc.contains("method") || !doc["method"].is_string())
        return MakeErrorResponse(doc.value("id", nlohmann::json(nullptr)), JsonRpcErrorCode::InvalidRequest,
                                 "Missing or invalid method");

    auto const method = doc["method"].get<std::string>();
    auto const params = doc.value("params", nlohmann::json(nlohmann::json::value_t::object));

    if (doc.contains("id"))
    {
        return JsonRpcMessage{JsonRpcRequest{
            .id = doc["id"],
            .method = method,
            .params = params,
        }};
    }

    return JsonRpcMessage{JsonRpcNotification{
        .method = method,
        .params = params,
    }};
}

auto SerializeResponse(JsonRpcResponse const& response) -> std::string
{
    nlohmann::json doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = response.id;

    if (response.error)
    {
        auto& err = doc["error"];
        err["code"] = response.error->code;
        err["message"] = response.error->message;
        if (!response.error->data.is_null())
            err["data"] = response.error->data;
    }
    else
    {
        doc["result"] = response.result.value_or(nlohmann::json(nullptr));
    }

    return doc.dump();
}

auto MakeSuccessResponse(nlohmann::json id, nlohmann::json result) -> JsonRpcResponse
{
    return JsonRpcResponse{
        .id = std::move(id),
        .result = std::move(result),
        .error = std::nullopt,
    };
}

auto MakeErrorResponse(nlohmann::json id, JsonRpcErrorCode code, std::string message, nlohmann::json data)
    -> JsonRpcResponse
{
    return JsonRpcResponse{
        .id = std::move(id),
        .result = std::nullopt,
        .error =
            JsonRpcError{
                .code = static_cast<int32_t>(code),
                .message = std::move(message),
                .data = std::move(data),
            },
    };
}

} // namespace mcpprotocol
