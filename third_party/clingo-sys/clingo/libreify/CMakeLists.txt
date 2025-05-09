# [[[header: .
set(ide_header_group "Header Files")
set(header-group-reify
    "${CMAKE_CURRENT_SOURCE_DIR}/reify/program.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/reify/util.hh")
source_group("${ide_header_group}\\reify" FILES ${header-group-reify})
set(header
    ${header-group-reify})
# ]]]
# [[[source: src
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/src/program.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_library(libreify STATIC ${header} ${source})
target_link_libraries(libreify PUBLIC libpotassco)
target_include_directories(libreify PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    # NOTE: for the tarjan algorithm: a good candidate for libpotassco
    "$<BUILD_INTERFACE:${CLINGO_SOURCE_DIR}/libgringo>")
set_target_properties(libreify PROPERTIES
    OUTPUT_NAME reify
    FOLDER lib)

if (CLINGO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
