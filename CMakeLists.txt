cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Version)
GetVersionInformation(DUDE_VERSION DUDE_VERSION_STRING)
file(WRITE "${CMAKE_BINARY_DIR}/version.txt" "${DUDE_VERSION_STRING}")

project(dude
    VERSION "${DUDE_VERSION}"
    LANGUAGES CXX
    DESCRIPTION "Code Duplication Analysis Tool"
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

if(MSVC)
    # Suppress MSVC deprecation warnings for standard C functions (fopen, getenv, etc.)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
    # Required for correct __cplusplus macro value
    add_compile_options(/Zc:__cplusplus)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # clang-cl's preprocessor is already standards-conforming; this flag is only needed for cl.exe
        add_compile_options(/Zc:preprocessor)
    endif()
    # Suppress C4702 (unreachable code) triggered by stdexec template instantiations
    add_compile_options(/wd4702)
endif()

if(ENABLE_NATIVE_ARCH)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-march=native)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

# nlohmann_json: prefer system/vcpkg, fall back to CPM
find_package(nlohmann_json 3 QUIET)
if(NOT nlohmann_json_FOUND)
    CPMAddPackage(
        NAME nlohmann_json
        GITHUB_REPOSITORY nlohmann/json
        VERSION 3.11.3
        EXCLUDE_FROM_ALL YES
        OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
    )
endif()

# unordered_dense: prefer system/vcpkg, fall back to CPM
find_package(unordered_dense 4 QUIET)
if(NOT unordered_dense_FOUND)
    CPMAddPackage(
        NAME unordered_dense
        GITHUB_REPOSITORY martinus/unordered_dense
        VERSION 4.4.0
        EXCLUDE_FROM_ALL YES
    )
endif()

# stdexec: prefer system/vcpkg, fall back to CPM
find_package(stdexec QUIET)
if(NOT stdexec_FOUND)
    CPMAddPackage(
        NAME stdexec
        GITHUB_REPOSITORY NVIDIA/stdexec
        GIT_TAG main
        EXCLUDE_FROM_ALL YES
        OPTIONS "STDEXEC_BUILD_TESTS OFF" "STDEXEC_BUILD_EXAMPLES OFF"
    )
endif()

add_subdirectory(src/dude)
add_subdirectory(src/mcpprotocol)
add_subdirectory(src/git)
add_subdirectory(src/mcp)
add_subdirectory(src/cli)

enable_testing()
add_subdirectory(src/tests)
