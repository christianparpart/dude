add_library(git STATIC
    GitDiffParser.cpp
    GitFileFilter.cpp
)
target_include_directories(git PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_link_libraries(git PUBLIC dude-lib)

set_pedantic_compiler_warnings(git)
enable_clang_tidy(git)
