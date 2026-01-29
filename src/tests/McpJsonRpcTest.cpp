// SPDX-License-Identifier: Apache-2.0

#include <mcpprotocol/JsonRpc.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace mcpprotocol;

// ---------------------------------------------------------------------------
// ParseMessage tests
// ---------------------------------------------------------------------------

TEST_CASE("ParseMessage.ValidRequest", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"jsonrpc":"2.0","id":1,"method":"test","params":{"key":"value"}})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcMessage>(result));
    auto const& message = std::get<JsonRpcMessage>(result);
    REQUIRE(std::holds_alternative<JsonRpcRequest>(message));
    auto const& req = std::get<JsonRpcRequest>(message);
    CHECK(req.id == 1);
    CHECK(req.method == "test");
    CHECK(req.params["key"] == "value");
}

TEST_CASE("ParseMessage.ValidNotification", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"jsonrpc":"2.0","method":"notifications/initialized"})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcMessage>(result));
    auto const& message = std::get<JsonRpcMessage>(result);
    REQUIRE(std::holds_alternative<JsonRpcNotification>(message));
    auto const& notif = std::get<JsonRpcNotification>(message);
    CHECK(notif.method == "notifications/initialized");
}

TEST_CASE("ParseMessage.RequestWithStringId", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"jsonrpc":"2.0","id":"abc","method":"test"})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcMessage>(result));
    auto const& req = std::get<JsonRpcRequest>(std::get<JsonRpcMessage>(result));
    CHECK(req.id == "abc");
}

TEST_CASE("ParseMessage.RequestWithNoParams", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcMessage>(result));
    auto const& req = std::get<JsonRpcRequest>(std::get<JsonRpcMessage>(result));
    CHECK(req.method == "ping");
    CHECK(req.params.is_object());
}

TEST_CASE("ParseMessage.InvalidJson", "[mcp][jsonrpc]")
{
    auto const result = ParseMessage("{broken");
    REQUIRE(std::holds_alternative<JsonRpcResponse>(result));
    auto const& resp = std::get<JsonRpcResponse>(result);
    REQUIRE(resp.error.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp.error.value().code == static_cast<int32_t>(JsonRpcErrorCode::ParseError));
}

TEST_CASE("ParseMessage.MissingJsonrpcVersion", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"id":1,"method":"test"})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcResponse>(result));
    auto const& resp = std::get<JsonRpcResponse>(result);
    REQUIRE(resp.error.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp.error.value().code == static_cast<int32_t>(JsonRpcErrorCode::InvalidRequest));
}

TEST_CASE("ParseMessage.WrongJsonrpcVersion", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"jsonrpc":"1.0","id":1,"method":"test"})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcResponse>(result));
    auto const& resp = std::get<JsonRpcResponse>(result);
    REQUIRE(resp.error.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp.error.value().code == static_cast<int32_t>(JsonRpcErrorCode::InvalidRequest));
}

TEST_CASE("ParseMessage.MissingMethod", "[mcp][jsonrpc]")
{
    auto const* const input = R"({"jsonrpc":"2.0","id":1})";
    auto const result = ParseMessage(input);
    REQUIRE(std::holds_alternative<JsonRpcResponse>(result));
}

TEST_CASE("ParseMessage.NotAnObject", "[mcp][jsonrpc]")
{
    auto const result = ParseMessage("[1,2,3]");
    REQUIRE(std::holds_alternative<JsonRpcResponse>(result));
    auto const& resp = std::get<JsonRpcResponse>(result);
    REQUIRE(resp.error.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    CHECK(resp.error.value().code == static_cast<int32_t>(JsonRpcErrorCode::InvalidRequest));
}

// ---------------------------------------------------------------------------
// SerializeResponse tests
// ---------------------------------------------------------------------------

TEST_CASE("SerializeResponse.SuccessResponse", "[mcp][jsonrpc]")
{
    auto const resp = MakeSuccessResponse(1, nlohmann::json{{"status", "ok"}});
    auto const json = SerializeResponse(resp);
    auto const doc = nlohmann::json::parse(json);
    CHECK(doc["jsonrpc"] == "2.0");
    CHECK(doc["id"] == 1);
    CHECK(doc["result"]["status"] == "ok");
    CHECK_FALSE(doc.contains("error"));
}

TEST_CASE("SerializeResponse.ErrorResponse", "[mcp][jsonrpc]")
{
    auto const resp = MakeErrorResponse(42, JsonRpcErrorCode::MethodNotFound, "No such method");
    auto const json = SerializeResponse(resp);
    auto const doc = nlohmann::json::parse(json);
    CHECK(doc["jsonrpc"] == "2.0");
    CHECK(doc["id"] == 42);
    CHECK(doc["error"]["code"] == -32601);
    CHECK(doc["error"]["message"] == "No such method");
    CHECK_FALSE(doc.contains("result"));
}

TEST_CASE("SerializeResponse.ErrorWithNullId", "[mcp][jsonrpc]")
{
    auto const resp = MakeErrorResponse(nullptr, JsonRpcErrorCode::ParseError, "Parse error");
    auto const json = SerializeResponse(resp);
    auto const doc = nlohmann::json::parse(json);
    CHECK(doc["id"].is_null());
    CHECK(doc["error"]["code"] == -32700);
}

TEST_CASE("SerializeResponse.ErrorWithData", "[mcp][jsonrpc]")
{
    auto const resp =
        MakeErrorResponse(1, JsonRpcErrorCode::InternalError, "Internal", nlohmann::json{{"detail", "info"}});
    auto const json = SerializeResponse(resp);
    auto const doc = nlohmann::json::parse(json);
    CHECK(doc["error"]["data"]["detail"] == "info");
}
