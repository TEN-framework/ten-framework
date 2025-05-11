# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/luaclingo.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/luaclingo.h")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_library(libluaclingo STATIC ${source})
target_link_libraries(libluaclingo PUBLIC libclingo PRIVATE Lua::Lua)
target_include_directories(libluaclingo PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_definitions(libluaclingo PRIVATE LUACLINGO_BUILD_LIBRARY PUBLIC CLINGO_WITH_LUA)
set_target_properties(libluaclingo PROPERTIES
    OUTPUT_NAME luaclingo
    FOLDER lib)
if (NOT CLINGO_BUILD_LUA_SHARED)
    target_compile_definitions(libluaclingo PUBLIC LUACLINGO_NO_VISIBILITY)
else()
    set_target_properties(libluaclingo PROPERTIES SOVERSION 1 VERSION 1.0)
    install(TARGETS libluaclingo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
