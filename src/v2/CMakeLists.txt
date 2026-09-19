# Format v2 groundwork. Not used by the v1 encoder or decoder and not part of
# any .trp file yet; see docs/internals/format-spec-v2.md.

add_library(triepack_v2
    v2_bitvec.c
    v2_tails.c
)

target_include_directories(triepack_v2
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

triepack_set_warnings(triepack_v2)

if(ENABLE_COVERAGE)
    triepack_enable_coverage(triepack_v2)
endif()

add_library(triepack::v2 ALIAS triepack_v2)
