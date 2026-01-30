add_executable(dude
    main.cpp
)
target_link_libraries(dude PRIVATE dude-lib mcp git)
target_compile_definitions(dude PRIVATE
    DUDE_VERSION="${CMAKE_PROJECT_VERSION}"
)

set_pedantic_compiler_warnings(dude)
enable_sanitizers(dude)
enable_clang_tidy(dude)
enable_static_linking(dude)

install(TARGETS dude RUNTIME DESTINATION bin)
