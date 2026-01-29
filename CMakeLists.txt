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
option(ENABLE_PGO_INSTRUMENT "Enable PGO instrumentation (profile generation)" OFF)
option(ENABLE_PGO_OPTIMIZE "Enable PGO optimization (profile use)" OFF)
set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo-profiles" CACHE PATH "Directory for PGO profile data")

include(cmake/PedanticCompiler.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Coverage.cmake)
include(cmake/ClangTidy.cmake)
include(cmake/StaticLinking.cmake)

if(ENABLE_NATIVE_ARCH)
    add_compile_options(-march=native)
endif()

if(ENABLE_PGO_INSTRUMENT)
    add_compile_options(-fprofile-instr-generate=${PGO_PROFILE_DIR}/default-%p.profraw)
    add_link_options(-fprofile-instr-generate=${PGO_PROFILE_DIR}/default-%p.profraw)
elseif(ENABLE_PGO_OPTIMIZE)
    if(NOT EXISTS "${PGO_PROFILE_DIR}/merged.profdata")
        message(FATAL_ERROR "PGO profile data not found at ${PGO_PROFILE_DIR}/merged.profdata. "
            "Run the instrumented build first, then merge with: "
            "llvm-profdata merge -output=${PGO_PROFILE_DIR}/merged.profdata ${PGO_PROFILE_DIR}/*.profraw")
    endif()
    add_compile_options(-fprofile-instr-use=${PGO_PROFILE_DIR}/merged.profdata)
    add_link_options(-fprofile-instr-use=${PGO_PROFILE_DIR}/merged.profdata)
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
    EXCLUDE_FROM_ALL YES
    OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
)

CPMAddPackage(
    NAME unordered_dense
    GITHUB_REPOSITORY martinus/unordered_dense
    VERSION 4.4.0
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME stdexec
    GITHUB_REPOSITORY NVIDIA/stdexec
    GIT_TAG main
    EXCLUDE_FROM_ALL YES
    OPTIONS "STDEXEC_BUILD_TESTS OFF" "STDEXEC_BUILD_EXAMPLES OFF"
)

add_subdirectory(src/codedup)
add_subdirectory(src/mcpprotocol)
add_subdirectory(src/mcp)
add_subdirectory(src/cli)

enable_testing()
add_subdirectory(src/tests)
