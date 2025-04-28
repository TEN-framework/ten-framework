cmake_minimum_required(VERSION 3.1)
include(GNUInstallDirs)


project(tsl-ordered-map VERSION 1.1.0)

add_library(ordered_map INTERFACE)
# Use tsl::ordered_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::ordered_map ALIAS ordered_map)

target_include_directories(ordered_map INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_hash.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_map.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_set.h")
target_sources(ordered_map INTERFACE "$<BUILD_INTERFACE:${headers}>")

if(MSVC)
    target_sources(ordered_map INTERFACE
                   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tsl-ordered-map.natvis>"
                   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATAROOTDIR}/tsl-ordered-map.natvis>")
endif()




# Installation (only compatible with CMake version >= 3.3)
if(${CMAKE_VERSION} VERSION_GREATER "3.2")
    include(CMakePackageConfigHelpers)

    ## Install include directory and potential natvis file
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    if(MSVC)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tsl-ordered-map.natvis"
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}")
    endif()



    ## Create and install tsl-ordered-mapConfig.cmake
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tsl-ordered-mapConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfig.cmake"
                                  INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")



    ## Create and install tsl-ordered-mapTargets.cmake
    install(TARGETS ordered_map
            EXPORT tsl-ordered-mapTargets)

    install(EXPORT tsl-ordered-mapTargets
            NAMESPACE tsl::
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")



    ## Create and install tsl-ordered-mapConfigVersion.cmake
    # tsl-ordered-map is header-only and does not depend on the architecture.
    # Remove CMAKE_SIZEOF_VOID_P from tsl-ordered-mapConfigVersion.cmake so that a
    # tsl-ordered-mapConfig.cmake generated for a 64 bit target can be used for 32 bit
    # targets and vice versa.
    set(CMAKE_SIZEOF_VOID_P_BACKUP ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfigVersion.cmake"
                                     COMPATIBILITY SameMajorVersion)
    set(CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P_BACKUP})

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")
endif()
