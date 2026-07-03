# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_executable(gringo ${header} ${source})
target_link_libraries(gringo PRIVATE libpyclingo libluaclingo libclingo)
set_target_properties(gringo PROPERTIES FOLDER exe)

install(TARGETS gringo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
