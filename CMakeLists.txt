cmake_minimum_required(VERSION 3.25)

include(cmake/Version.cmake)

project(CodeDupDetector
    VERSION ${PROJECT_VERSION_STRING}
    LANGUAGES CXX
    DESCRIPTION "C++ code duplication detector"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_NATIVE_ARCH "Enable -march=native for optimized builds (POPCNT, BMI2, AVX2)" OFF)

include(cmake/PedanticCompiler.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Coverage.cmake)
include(cmake/ClangTidy.cmake)
include(cmake/StaticLinking.cmake)

if(ENABLE_NATIVE_ARCH)
    add_compile_options(-march=native)
endif()

# CPM for dependency management
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()
include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
    OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
)

add_subdirectory(src/codedup)
add_subdirectory(src/mcpprotocol)
add_subdirectory(src/mcp)
add_subdirectory(src/cli)

enable_testing()
add_subdirectory(src/tests)
