add_library(triepack_core
    encoder.c
    decoder.c
    value.c
    header.c
    suffix.c
    integrity.c
)

target_include_directories(triepack_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(triepack_core PUBLIC triepack_bitstream)

triepack_set_warnings(triepack_core)

if(ENABLE_COVERAGE)
    triepack_enable_coverage(triepack_core)
endif()

add_library(triepack::core ALIAS triepack_core)

install(TARGETS triepack_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
