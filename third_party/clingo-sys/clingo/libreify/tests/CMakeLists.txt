# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/program.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_executable(test_reify ${source})
target_link_libraries(test_reify PRIVATE libreify Catch2::Catch2WithMain)
target_include_directories(test_reify PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
set_target_properties(test_reify PROPERTIES FOLDER test)

add_test(NAME test_reify COMMAND test_reify)
