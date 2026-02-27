# Copyright (c) 2026 M. A. Chatterjee
# SPDX-License-Identifier: BSD-2-Clause

add_library(triepack_wrapper
    src/bitstream_wrapper.cpp
    src/triepack_wrapper.cpp
    src/json_wrapper.cpp
)

target_include_directories(triepack_wrapper
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(triepack_wrapper PUBLIC triepack_core)
if(BUILD_JSON)
    target_link_libraries(triepack_wrapper PUBLIC triepack_json)
endif()

triepack_set_warnings(triepack_wrapper)

if(ENABLE_COVERAGE)
    triepack_enable_coverage(triepack_wrapper)
endif()

add_library(triepack::wrapper ALIAS triepack_wrapper)

install(TARGETS triepack_wrapper
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/triepack
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)
