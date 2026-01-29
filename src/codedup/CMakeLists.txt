find_package(Threads REQUIRED)

add_library(codedup STATIC
    AnalysisScope.cpp
    DiffFilter.cpp
    Encoding.cpp
    Token.cpp
    TokenNormalizer.cpp
    LanguageRegistry.cpp
    Languages/CppLanguage.cpp
    Languages/CSharpLanguage.cpp
    CloneDetector.cpp
    IntraFunctionDetector.cpp
    FileScanner.cpp
    Reporter.cpp
    ScopeFilter.cpp
)

target_include_directories(codedup PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(codedup PUBLIC Threads::Threads)

set_target_properties(codedup PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set_pedantic_compiler_warnings(codedup)
enable_sanitizers(codedup)
enable_coverage(codedup)
enable_clang_tidy(codedup)
