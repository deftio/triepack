cmake_minimum_required(VERSION 3.16)

# Single source of truth for the release version. Every package manifest,
# the docs site and the CLI derive from this file; scripts/sync_version.sh
# propagates it and --check fails if anything has drifted.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/triepack-version.txt" TRIEPACK_VERSION)
string(STRIP "${TRIEPACK_VERSION}" TRIEPACK_VERSION)

# Without this, editing the version file leaves the generated header stale:
# file(READ) runs at configure time and nothing tells CMake to re-run.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/triepack-version.txt")

project(triepack VERSION ${TRIEPACK_VERSION} LANGUAGES C CXX)

# ── Options ──────────────────────────────────────────────────────────────
option(BUILD_TESTS    "Build test suite"           ON)
option(BUILD_EXAMPLES "Build example programs"     ON)
option(BUILD_JSON     "Build JSON library"         ON)
option(BUILD_DOCS     "Build Doxygen docs"         OFF)
option(ENABLE_COVERAGE "Enable code coverage"      OFF)
option(ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

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

# ── Sanitizers ───────────────────────────────────────────────────────────
# triepack parses untrusted binary input, so the tests that walk corrupted
# dictionaries only prove anything when a bad read actually aborts. Not
# combinable with coverage: the instrumentation fights over the same runtime.
if(ENABLE_SANITIZERS)
    if(ENABLE_COVERAGE)
        message(FATAL_ERROR "ENABLE_SANITIZERS and ENABLE_COVERAGE cannot be used together")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
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
add_subdirectory(src/v2)
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
