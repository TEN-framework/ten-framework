# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/_clingo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/pyclingo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/pyclingo.h")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")
add_library(libpyclingo STATIC ${source})
target_link_libraries(libpyclingo PUBLIC libclingo PRIVATE Python::Python Threads::Threads)
target_include_directories(libpyclingo
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
target_compile_definitions(libpyclingo PRIVATE PYCLINGO_BUILD_LIBRARY PUBLIC CLINGO_WITH_PYTHON)
# TODO: we do not need visibility attributes at all
# target_compile_definitions(libpyclingo PUBLIC PYCLINGO_NO_VISIBILITY)
set_target_properties(libpyclingo PROPERTIES
    OUTPUT_NAME pyclingo
    FOLDER lib)
