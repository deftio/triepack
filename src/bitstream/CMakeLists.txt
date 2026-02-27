add_library(triepack_bitstream
    bitstream.c
    bitstream_read.c
    bitstream_write.c
    bitstream_varint.c
    bitstream_symbol.c
)

target_include_directories(triepack_bitstream
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

triepack_set_warnings(triepack_bitstream)

if(ENABLE_COVERAGE)
    triepack_enable_coverage(triepack_bitstream)
endif()

add_library(triepack::bitstream ALIAS triepack_bitstream)

install(TARGETS triepack_bitstream
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
