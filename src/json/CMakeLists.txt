add_library(triepack_json
    json_encode.c
    json_decode.c
    json_dom.c
)

target_include_directories(triepack_json
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/core
)

target_link_libraries(triepack_json PUBLIC triepack_core)

triepack_set_warnings(triepack_json)

if(ENABLE_COVERAGE)
    triepack_enable_coverage(triepack_json)
endif()

add_library(triepack::json ALIAS triepack_json)

install(TARGETS triepack_json
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
