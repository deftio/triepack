# CodeCoverage.cmake
# Adds --coverage flags and a 'coverage' target that generates HTML reports.

function(triepack_enable_coverage target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE --coverage)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()

function(triepack_add_coverage_target)
    find_program(LCOV lcov)
    find_program(GENHTML genhtml)

    if(NOT LCOV OR NOT GENHTML)
        message(STATUS "lcov/genhtml not found — 'coverage' target disabled")
        return()
    endif()

    add_custom_target(coverage
        COMMAND ${LCOV} --capture --directory . --output-file lcov_raw.info
                --rc branch_coverage=1 --ignore-errors empty,empty
        COMMAND ${LCOV} --remove lcov_raw.info
                "*/tests/*" "*/Unity/*" "*/_deps/*" "/usr/*"
                --output-file lcov.info
                --rc branch_coverage=1 --ignore-errors empty,empty
        COMMAND ${GENHTML} lcov.info --output-directory coverage
                --branch-coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report → build/coverage/"
    )
endfunction()
