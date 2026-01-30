add_library(mcp STATIC
    AnalysisSession.cpp
    McpTooling.cpp
)
target_include_directories(mcp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_link_libraries(mcp PUBLIC mcpprotocol codedup git)

set_pedantic_compiler_warnings(mcp)
enable_clang_tidy(mcp)
