## Adds code coverage support.

option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

function(enable_coverage target)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
        )
        target_link_options(${target} PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()

function(add_coverage_target test_target)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=coverage.profraw $<TARGET_FILE:${test_target}>
            COMMAND llvm-profdata merge -sparse coverage.profraw -o coverage.profdata
            COMMAND llvm-cov report $<TARGET_FILE:${test_target}> -instr-profile=coverage.profdata
            COMMAND llvm-cov show $<TARGET_FILE:${test_target}> -instr-profile=coverage.profdata
                -format=html -output-dir=${CMAKE_BINARY_DIR}/coverage
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${test_target}
            COMMENT "Generating code coverage report"
        )
    endif()
endfunction()
