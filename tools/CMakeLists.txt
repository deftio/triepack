# ===========================================================================
#  tools/CMakeLists.txt
#  Build fixture generator and other tooling
#
#  Copyright (c) 2026 M. A. Chatterjee
#  SPDX-License-Identifier: BSD-2-Clause
# ===========================================================================

add_executable(generate_fixtures generate_fixtures.c)
target_link_libraries(generate_fixtures PRIVATE triepack_core)

add_executable(generate_conformance generate_conformance.c)
target_link_libraries(generate_conformance PRIVATE triepack_core)

add_executable(trp trp_inspect.c)
target_link_libraries(trp PRIVATE triepack_json)
install(TARGETS trp DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(run_benchmarks run_benchmarks.c)
target_link_libraries(run_benchmarks PRIVATE triepack_json)

# Format v2 measurement prototype. Standalone by design: it implements the
# proposed format directly rather than through the v1 library, so its numbers
# describe the format and not the current encoder. See
# docs/internals/format-spec-v2.md and the Phase 2 gate in the v2 plan.
add_executable(v2_prototype v2_prototype.c)

# Custom target to generate test fixtures
add_custom_target(fixtures
    COMMAND generate_fixtures "${CMAKE_SOURCE_DIR}/tests/fixtures"
    COMMAND generate_conformance
            "${CMAKE_SOURCE_DIR}/tests/conformance/cases.txt"
            "${CMAKE_SOURCE_DIR}/tests/conformance/fixtures"
    DEPENDS generate_fixtures generate_conformance
    COMMENT "Generating .trp test fixtures"
)
