# ===========================================================================
#  tools/CMakeLists.txt
#  Build fixture generator and other tooling
#
#  Copyright (c) 2026 M. A. Chatterjee
#  SPDX-License-Identifier: BSD-2-Clause
# ===========================================================================

add_executable(generate_fixtures generate_fixtures.c)
target_link_libraries(generate_fixtures PRIVATE triepack_core)

add_executable(trp trp_inspect.c)
target_link_libraries(trp PRIVATE triepack_json)
install(TARGETS trp DESTINATION ${CMAKE_INSTALL_BINDIR})

# Custom target to generate test fixtures
add_custom_target(fixtures
    COMMAND generate_fixtures "${CMAKE_SOURCE_DIR}/tests/fixtures"
    DEPENDS generate_fixtures
    COMMENT "Generating .trp test fixtures"
)
