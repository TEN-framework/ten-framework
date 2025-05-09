# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_executable(web ${header} ${source})
target_link_libraries(web libclingo libgringo libclasp libluaclingo libpyclingo)
set_target_properties(web PROPERTIES FOLDER exe)
target_include_directories(web PRIVATE
    "$<BUILD_INTERFACE:${CLINGO_SOURCE_DIR}/libgringo>"
    "$<BUILD_INTERFACE:${CLASP_SOURCE_DIR}/app>")

set_target_properties(web PROPERTIES LINK_FLAGS
    "-s EXPORTED_FUNCTIONS=_run,_memset -s EXPORTED_RUNTIME_METHODS=ccall")
set_target_properties(web PROPERTIES OUTPUT_NAME "clingo")
