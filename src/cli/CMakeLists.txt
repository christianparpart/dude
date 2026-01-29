add_executable(codedupdetector
    GitDiffParser.cpp
    GitFileFilter.cpp
    main.cpp
)
target_link_libraries(codedupdetector PRIVATE codedup mcp)
target_compile_definitions(codedupdetector PRIVATE
    CODEDUPDETECTOR_VERSION="${CMAKE_PROJECT_VERSION}"
)

set_pedantic_compiler_warnings(codedupdetector)
enable_sanitizers(codedupdetector)
enable_clang_tidy(codedupdetector)
enable_static_linking(codedupdetector)

install(TARGETS codedupdetector RUNTIME DESTINATION bin)
