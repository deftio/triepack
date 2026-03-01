# ===========================================================================
#  examples/CMakeLists.txt
#  Example programs — working implementations
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

add_executable(compaction_benchmark compaction_benchmark.c)
target_link_libraries(compaction_benchmark PRIVATE triepack_core)

add_executable(cpp_usage cpp_usage.cpp)
target_link_libraries(cpp_usage PRIVATE triepack_wrapper)
set_target_properties(cpp_usage PROPERTIES LINKER_LANGUAGE CXX)

# ── Register examples as tests so ctest runs them ──────────────────────
if(BUILD_TESTS)
    add_test(NAME example_basic_encode_decode COMMAND basic_encode_decode)
    add_test(NAME example_compaction_benchmark COMMAND compaction_benchmark)
    add_test(NAME example_rom_lookup COMMAND rom_lookup)
    add_test(NAME example_prefix_search COMMAND prefix_search)
    add_test(NAME example_cpp_usage COMMAND cpp_usage)
    if(BUILD_JSON)
        add_test(NAME example_json_roundtrip COMMAND json_roundtrip)
    endif()
endif()
