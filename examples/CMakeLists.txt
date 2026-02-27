# ===========================================================================
#  examples/CMakeLists.txt
#  Example programs — skeleton implementations
#
#  Copyright (c) 2026 M. A. Chatterjee
#  SPDX-License-Identifier: BSD-2-Clause
# ===========================================================================

add_executable(basic_encode_decode basic_encode_decode.c)
target_link_libraries(basic_encode_decode PRIVATE triepack_core)

add_executable(rom_lookup rom_lookup.c)
target_link_libraries(rom_lookup PRIVATE triepack_core)

add_executable(prefix_search prefix_search.c)
target_link_libraries(prefix_search PRIVATE triepack_core)

if(BUILD_JSON)
    add_executable(json_roundtrip json_roundtrip.c)
    target_link_libraries(json_roundtrip PRIVATE triepack_json)
endif()

add_executable(cpp_usage cpp_usage.cpp)
target_link_libraries(cpp_usage PRIVATE triepack_wrapper)
set_target_properties(cpp_usage PROPERTIES LINKER_LANGUAGE CXX)
