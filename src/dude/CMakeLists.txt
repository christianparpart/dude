find_package(Threads REQUIRED)

add_library(dude-lib STATIC
    AnalysisScope.cpp
    DiffFilter.cpp
    Encoding.cpp
    Token.cpp
    TokenNormalizer.cpp
    LanguageRegistry.cpp
    Languages/CppLanguage.cpp
    Languages/CSharpLanguage.cpp
    Languages/PythonLanguage.cpp
    CloneDetector.cpp
    IntraFunctionDetector.cpp
    ProgressBar.cpp
    FileScanner.cpp
    ConsoleReporter.cpp
    JsonReporter.cpp
    ReporterFactory.cpp
    ScopeFilter.cpp
    HelpFormatter.cpp
)

target_include_directories(dude-lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dude-lib PUBLIC Threads::Threads nlohmann_json::nlohmann_json unordered_dense::unordered_dense STDEXEC::stdexec)

# Mark stdexec headers as SYSTEM to suppress warnings from third-party code
get_target_property(_stdexec_includes STDEXEC::stdexec INTERFACE_INCLUDE_DIRECTORIES)
if(_stdexec_includes)
    target_include_directories(dude-lib SYSTEM PUBLIC ${_stdexec_includes})
endif()

set_target_properties(dude-lib PROPERTIES
    OUTPUT_NAME dude
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set_pedantic_compiler_warnings(dude-lib)
enable_sanitizers(dude-lib)
enable_coverage(dude-lib)
enable_clang_tidy(dude-lib)
