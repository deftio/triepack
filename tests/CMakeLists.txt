include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

function(triepack_add_c_test name source lib)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${lib} unity)
    target_include_directories(${name} PRIVATE ${unity_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
    if(ENABLE_COVERAGE)
        triepack_enable_coverage(${name})
    endif()
endfunction()

function(triepack_add_cpp_test name source lib)
    add_executable(${name} ${source})
    set_target_properties(${name} PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${name} PRIVATE ${lib} unity)
    target_include_directories(${name} PRIVATE ${unity_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
    if(ENABLE_COVERAGE)
        triepack_enable_coverage(${name})
    endif()
endfunction()

# Bitstream tests
triepack_add_c_test(test_bitstream_bits    test_bitstream_bits.c    triepack_bitstream)
triepack_add_c_test(test_bitstream_bytes   test_bitstream_bytes.c   triepack_bitstream)
triepack_add_c_test(test_bitstream_varint  test_bitstream_varint.c  triepack_bitstream)
triepack_add_c_test(test_bitstream_symbol  test_bitstream_symbol.c  triepack_bitstream)
triepack_add_c_test(test_bitstream_rom     test_bitstream_rom.c     triepack_bitstream)

# Core tests
triepack_add_c_test(test_core_roundtrip    test_core_roundtrip.c    triepack_core)
triepack_add_c_test(test_core_lookup       test_core_lookup.c       triepack_core)
triepack_add_c_test(test_core_values       test_core_values.c       triepack_core)
triepack_add_c_test(test_core_edge_cases   test_core_edge_cases.c   triepack_core)
triepack_add_c_test(test_core_integrity    test_core_integrity.c    triepack_core)

# JSON tests
if(BUILD_JSON)
    triepack_add_c_test(test_json_roundtrip    test_json_roundtrip.c    triepack_json)
    triepack_add_c_test(test_json_dom          test_json_dom.c          triepack_json)
    triepack_add_c_test(test_json_edge_cases   test_json_edge_cases.c   triepack_json)
endif()

# C++ wrapper tests
triepack_add_cpp_test(test_wrapper_bitstream test_wrapper_bitstream.cpp triepack_wrapper)
triepack_add_cpp_test(test_wrapper_core      test_wrapper_core.cpp      triepack_wrapper)
if(BUILD_JSON)
    triepack_add_cpp_test(test_wrapper_json  test_wrapper_json.cpp      triepack_wrapper)
endif()
