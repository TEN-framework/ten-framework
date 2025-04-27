# [[[header: .
set(ide_header_group "Header Files")
set(header-group
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo.hh")
source_group("${ide_header_group}" FILES ${header-group})
set(header-group-clingo
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo/astv2.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo/clingo_app.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo/clingocontrol.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo/control.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo/incmode.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo/scripts.hh")
source_group("${ide_header_group}\\clingo" FILES ${header-group-clingo})
set(header
    ${header-group}
    ${header-group-clingo})
# ]]]
# [[[source: src
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astv2.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astv2_build.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astv2_parse.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astv2_str.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/astv2_unpool.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/clingo_app.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/clingocontrol.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/control.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gringo_app.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/incmode.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripts.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

set(clingo_public_scope_ PUBLIC)
set(clingo_private_scope_ PRIVATE)
if (CLINGO_BUILD_SHARED)
    if (CLINGO_BUILD_WITH_PYTHON STREQUAL "pip")
        set(clingo_lib_type INTERFACE)
        set(clingo_public_scope_ INTERFACE)
        set(clingo_private_scope_ INTERFACE)
    else()
        set(clingo_lib_type SHARED)
    endif()
else()
    set(clingo_lib_type STATIC)
endif()

add_library(libclingo ${clingo_lib_type})
target_sources(libclingo ${clingo_private_scope_} ${header} ${source})
target_link_libraries(libclingo ${clingo_private_scope_} libgringo libclasp)
target_include_directories(libclingo
    ${clingo_public_scope_}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_definitions(libclingo ${clingo_private_scope_} CLINGO_BUILD_LIBRARY)

if (CLINGO_USE_VERSION_SCRIPT)
    set_target_properties(libclingo PROPERTIES LINK_FLAGS "-Wl,--version-script='${CMAKE_CURRENT_SOURCE_DIR}/clingo.map'")
endif()

if (NOT CLINGO_BUILD_SHARED)
    target_compile_definitions(libclingo PUBLIC CLINGO_NO_VISIBILITY)
endif()

if ((CLINGO_BUILD_SHARED OR CLINGO_INSTALL_LIB) AND NOT CLINGO_BUILD_WITH_PYTHON STREQUAL "pip")
    set(clingo_targets libclingo)
    if (CLINGO_BUILD_SHARED)
        set_target_properties(libclingo PROPERTIES SOVERSION 4 VERSION 4.0)
    else()
        list(APPEND clingo_targets libgringo libreify ordered_map hopscotch_map sparse_map optional variant wide_integer)
    endif()

    install(TARGETS ${clingo_targets}
        EXPORT ClingoTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/clingo.hh"
        "${CMAKE_CURRENT_SOURCE_DIR}/clingo.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT ClingoTargets
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Clingo)

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/ClingoConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Clingo)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Clingo)
endif()

if (MSVC)
    target_compile_definitions(libclingo ${clingo_private_scope_} _SCL_SECURE_NO_WARNINGS)
endif()
if (CLINGO_BUILD_REVISION)
    target_compile_definitions(libclingo PRIVATE CLINGO_BUILD_REVISION=${CLINGO_BUILD_REVISION})
endif()
if (NOT CLINGO_BUILD_WITH_PYTHON STREQUAL "pip")
    set_target_properties(libclingo PROPERTIES
        OUTPUT_NAME clingo
        FOLDER lib)
endif()

if (CLINGO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
