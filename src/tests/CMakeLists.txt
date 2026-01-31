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

add_executable(dude_test
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
    GlobMatchTest.cpp
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
    ProgressBarTest.cpp
    TokenTest.cpp
    MappedFileTest.cpp
    SyntaxHighlighterTest.cpp
)

target_link_libraries(dude_test PRIVATE
    dude-lib
    mcp
    git
    Catch2::Catch2WithMain
)

target_compile_definitions(dude_test PRIVATE
    DUDE_VERSION="${CMAKE_PROJECT_VERSION}"
)

set_pedantic_compiler_warnings(dude_test)
enable_sanitizers(dude_test)
enable_coverage(dude_test)
enable_clang_tidy(dude_test)

include(CTest)
if(Catch2_SOURCE_DIR)
    # When fetched via CPM, the Catch module is in the source tree's extras/ directory.
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()
include(Catch)
catch_discover_tests(dude_test)

add_coverage_target(dude_test)
