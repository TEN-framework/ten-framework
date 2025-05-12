cmake_minimum_required(VERSION 3.1)
include(GNUInstallDirs)


project(tsl-hopscotch-map VERSION 2.3.0)

add_library(hopscotch_map INTERFACE)
# Use tsl::hopscotch_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::hopscotch_map ALIAS hopscotch_map)

target_include_directories(hopscotch_map INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/bhopscotch_map.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/bhopscotch_set.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/hopscotch_growth_policy.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/hopscotch_hash.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/hopscotch_map.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/hopscotch_set.h")
target_sources(hopscotch_map INTERFACE "$<BUILD_INTERFACE:${headers}>")

if(MSVC)
    target_sources(hopscotch_map INTERFACE
                   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tsl-hopscotch-map.natvis>"
                   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATAROOTDIR}/tsl-hopscotch-map.natvis>")
endif()




# Installation (only compatible with CMake version >= 3.3)
if(${CMAKE_VERSION} VERSION_GREATER "3.2")
    include(CMakePackageConfigHelpers)

    ## Install include directory and potential natvis file
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    if(MSVC)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tsl-hopscotch-map.natvis"
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}")
    endif()



    ## Create and install tsl-hopscotch-mapConfig.cmake
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tsl-hopscotch-mapConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/tsl-hopscotch-mapConfig.cmake"
                                  INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-hopscotch-map")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-hopscotch-mapConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-hopscotch-map")



    ## Create and install tsl-hopscotch-mapTargets.cmake
    install(TARGETS hopscotch_map
            EXPORT tsl-hopscotch-mapTargets)

    install(EXPORT tsl-hopscotch-mapTargets
            NAMESPACE tsl::
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-hopscotch-map")



    ## Create and install tsl-hopscotch-mapConfigVersion.cmake
    # tsl-hopscotch-map is header-only and does not depend on the architecture.
    # Remove CMAKE_SIZEOF_VOID_P from tsl-hopscotch-mapConfigVersion.cmake so that a
    # tsl-hopscotch-mapConfig.cmake generated for a 64 bit target can be used for 32 bit
    # targets and vice versa.
    set(CMAKE_SIZEOF_VOID_P_BACKUP ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/tsl-hopscotch-mapConfigVersion.cmake"
                                     COMPATIBILITY SameMajorVersion)
    set(CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P_BACKUP})

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-hopscotch-mapConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-hopscotch-map")
endif()
