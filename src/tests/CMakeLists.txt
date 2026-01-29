find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found on system, fetching via CPM")
    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        VERSION 3.7.1
        OPTIONS
            "CATCH_INSTALL_DOCS OFF"
            "CATCH_INSTALL_EXTRAS OFF"
    )
endif()

add_executable(CodeDupTest
    AnalysisScopeTest.cpp
    DiffRangeTest.cpp
    DiffFilterTest.cpp
    GitDiffParserTest.cpp
    GitFileFilterTest.cpp
    EncodingTest.cpp
    TokenizerTest.cpp
    TokenNormalizerTest.cpp
    CodeBlockTest.cpp
    CloneDetectorTest.cpp
    IntraFunctionDetectorTest.cpp
    FileScannerTest.cpp
    ReporterTest.cpp
    JsonReporterTest.cpp
    ReporterFactoryTest.cpp
    ScopeFilterTest.cpp
    IntegrationTest.cpp
    LanguageRegistryTest.cpp
    CSharpTokenizerTest.cpp
    CSharpCodeBlockTest.cpp
    PythonTokenizerTest.cpp
    PythonCodeBlockTest.cpp
    McpJsonRpcTest.cpp
    McpProtocolTest.cpp
    McpServerTest.cpp
    McpAnalysisSessionTest.cpp
    McpToolsTest.cpp
    HelpFormatterTest.cpp
)

# GitDiffParser and GitFileFilter live in src/cli/ — compile their sources and expose headers to the test.
target_sources(CodeDupTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../cli/GitDiffParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cli/GitFileFilter.cpp
)
target_include_directories(CodeDupTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(CodeDupTest PRIVATE
    codedup
    mcp
    Catch2::Catch2WithMain
)

target_compile_definitions(CodeDupTest PRIVATE
    CODEDUPDETECTOR_VERSION="${CMAKE_PROJECT_VERSION}"
)

set_pedantic_compiler_warnings(CodeDupTest)
enable_sanitizers(CodeDupTest)
enable_coverage(CodeDupTest)
enable_clang_tidy(CodeDupTest)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(CodeDupTest)

add_coverage_target(CodeDupTest)
