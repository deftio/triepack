include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_SUPPORT_64)

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
triepack_add_c_test(test_bitstream_errors  test_bitstream_errors.c  triepack_bitstream)

# Core tests
triepack_add_c_test(test_core_roundtrip    test_core_roundtrip.c    triepack_core)
triepack_add_c_test(test_core_lookup       test_core_lookup.c       triepack_core)
triepack_add_c_test(test_core_values       test_core_values.c       triepack_core)
triepack_add_c_test(test_core_edge_cases   test_core_edge_cases.c   triepack_core)
triepack_add_c_test(test_core_limits      test_core_limits.c       triepack_core)
target_compile_definitions(test_core_limits PRIVATE FIXTURE_DIR="${CMAKE_SOURCE_DIR}")
triepack_add_c_test(test_core_integrity    test_core_integrity.c    triepack_core)
triepack_add_c_test(test_core_iterate      test_core_iterate.c      triepack_core)
triepack_add_c_test(test_guards           test_guards.c            triepack_core)

# Format v2 groundwork, written test-first from the specification.
triepack_add_c_test(test_v2_bitvec        test_v2_bitvec.c         triepack_v2)
triepack_add_c_test(test_v2_louds         test_v2_louds.c          triepack_v2)
triepack_add_c_test(test_v2_tails         test_v2_tails.c          triepack_v2)

# Robustness against whatever real-world file is sitting in data/ (gitignored,
# so absent in CI -- the test passes when there is nothing to read).
triepack_add_c_test(test_robustness       test_robustness.c        triepack_core)
target_compile_definitions(test_robustness PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/data")
set_tests_properties(test_robustness PROPERTIES LABELS "robustness" TIMEOUT 1800)

# Large-dictionary tests. The default tier is a few megabytes and runs with
# everything else; TRIEPACK_SCALE_MB raises it. Labelled so CI can select it.
triepack_add_c_test(test_scale            test_scale.c             triepack_core)
set_tests_properties(test_scale PROPERTIES LABELS "scale" TIMEOUT 1800)

# The v2 prototype verifies every key through its own serialised bytes and
# exits non-zero on any mismatch, so running it is a real conformance check
# of the proposed format against a real corpus.
add_test(NAME v2_prototype_roundtrip
         COMMAND v2_prototype "${CMAKE_SOURCE_DIR}/tests/data/common_words_10k.txt")
set_tests_properties(v2_prototype_roundtrip PROPERTIES LABELS "v2")

# xjarchive reference grammar tests (Python). Skipped when pytest is absent
# rather than failing a C-only build.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    # Finding an interpreter is not enough: CMake may pick a different Python
    # from the one on PATH, and that one may not have pytest. Check for the
    # module, not the language, or the test fails where it meant to skip.
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import pytest"
                    RESULT_VARIABLE TP_PYTEST_MISSING
                    OUTPUT_QUIET ERROR_QUIET)
    if(TP_PYTEST_MISSING EQUAL 0)
        add_test(NAME xjarchive_grammar
                 COMMAND ${Python3_EXECUTABLE} -m pytest -q
                         "${CMAKE_SOURCE_DIR}/xjarchive/test_xjarchive.py")
        set_tests_properties(xjarchive_grammar PROPERTIES LABELS "xjarchive")
    else()
        message(STATUS "xjarchive grammar tests skipped: ${Python3_EXECUTABLE} has no pytest")
    endif()
endif()

# Core internals test (needs access to src/core/ for core_internal.h)
triepack_add_c_test(test_core_internal     test_core_internal.c     triepack_core)
target_include_directories(test_core_internal PRIVATE ${CMAKE_SOURCE_DIR}/src/core)

# JSON tests
if(BUILD_JSON)
    triepack_add_c_test(test_json_roundtrip    test_json_roundtrip.c    triepack_json)
    triepack_add_c_test(test_json_dom          test_json_dom.c          triepack_json)
    triepack_add_c_test(test_json_edge_cases   test_json_edge_cases.c   triepack_json)
    triepack_add_c_test(test_json_decode       test_json_decode.c       triepack_json)
    target_include_directories(test_json_decode PRIVATE ${CMAKE_SOURCE_DIR}/src/core)
endif()

# Cross-language fixture tests
triepack_add_c_test(test_cross_language test_cross_language.c triepack_core)
target_compile_definitions(test_cross_language PRIVATE FIXTURE_DIR="${CMAKE_SOURCE_DIR}")

# Cross-language conformance corpus (tests/conformance/)
triepack_add_c_test(test_conformance test_conformance.c triepack_core)
target_compile_definitions(test_conformance PRIVATE FIXTURE_DIR="${CMAKE_SOURCE_DIR}")

# C++ wrapper tests
triepack_add_cpp_test(test_wrapper_bitstream test_wrapper_bitstream.cpp triepack_wrapper)
triepack_add_cpp_test(test_wrapper_core      test_wrapper_core.cpp      triepack_wrapper)
if(BUILD_JSON)
    triepack_add_cpp_test(test_wrapper_json  test_wrapper_json.cpp      triepack_wrapper)
endif()
