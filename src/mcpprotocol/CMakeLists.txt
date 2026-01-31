add_library(mcpprotocol STATIC
    JsonRpc.cpp
    McpProtocol.cpp
    McpServer.cpp
)
target_include_directories(mcpprotocol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_link_libraries(mcpprotocol PUBLIC nlohmann_json::nlohmann_json)

set_pedantic_compiler_warnings(mcpprotocol)
enable_coverage(mcpprotocol)
enable_clang_tidy(mcpprotocol)
