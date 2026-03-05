cmake_minimum_required(VERSION 3.16)
project(triepack VERSION 1.1.0 LANGUAGES C CXX)

# ── Options ──────────────────────────────────────────────────────────────
option(BUILD_TESTS    "Build test suite"           ON)
option(BUILD_EXAMPLES "Build example programs"     ON)
option(BUILD_JSON     "Build JSON library"         ON)
option(BUILD_DOCS     "Build Doxygen docs"         OFF)
option(ENABLE_COVERAGE "Enable code coverage"      OFF)

# ── Standards ────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compile commands for clang-tidy ──────────────────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Modules ──────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(CodeCoverage)
include(GNUInstallDirs)

# ── Global coverage flags (ensures static library objects are instrumented) ──
if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# ── Version header ───────────────────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/triepack/triepack_version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/triepack/triepack_version.h"
    @ONLY
)

# ── Libraries (dependency order) ─────────────────────────────────────────
add_subdirectory(src/bitstream)
add_subdirectory(src/core)
if(BUILD_JSON)
    add_subdirectory(src/json)
endif()
add_subdirectory(wrapper)

# ── Tests ────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Tools ────────────────────────────────────────────────────────────────
add_subdirectory(tools)

# ── Examples ─────────────────────────────────────────────────────────────
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ── Docs ─────────────────────────────────────────────────────────────────
if(BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
    )
endif()

# ── Coverage target ──────────────────────────────────────────────────────
if(ENABLE_COVERAGE)
    triepack_add_coverage_target()
endif()

# ── Install ──────────────────────────────────────────────────────────────
install(DIRECTORY include/triepack
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/triepack/triepack_version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/triepack
)
