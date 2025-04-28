cmake_minimum_required(VERSION 3.1)

project(tsl-sparse-map VERSION 0.6.2 LANGUAGES CXX)
include(GNUInstallDirs)

add_library(sparse_map INTERFACE)
# Use tsl::sparse_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::sparse_map ALIAS sparse_map)

target_include_directories(sparse_map INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/sparse_growth_policy.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/sparse_hash.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/sparse_map.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/sparse_set.h")
target_sources(sparse_map INTERFACE "$<BUILD_INTERFACE:${headers}>")

if(MSVC)
    target_sources(sparse_map INTERFACE
                   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tsl-sparse-map.natvis>"
                   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATAROOTDIR}/tsl-sparse-map.natvis>")
endif()




# Installation (only compatible with CMake version >= 3.3)
if(${CMAKE_VERSION} VERSION_GREATER "3.2")
    include(CMakePackageConfigHelpers)

    ## Install include directory and potential natvis file
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    if(MSVC)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tsl-sparse-map.natvis"
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}")
    endif()



    ## Create and install tsl-sparse-mapConfig.cmake
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tsl-sparse-mapConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/tsl-sparse-mapConfig.cmake"
                                  INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-sparse-map")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-sparse-mapConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-sparse-map")



    ## Create and install tsl-sparse-mapTargets.cmake
    install(TARGETS sparse_map
            EXPORT tsl-sparse-mapTargets)

    install(EXPORT tsl-sparse-mapTargets
            NAMESPACE tsl::
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-sparse-map")



    ## Create and install tsl-sparse-mapConfigVersion.cmake
    # tsl-sparse-map is header-only and does not depend on the architecture.
    # Remove CMAKE_SIZEOF_VOID_P from tsl-sparse-mapConfigVersion.cmake so that a
    # tsl-sparse-mapConfig.cmake generated for a 64 bit target can be used for 32 bit
    # targets and vice versa.
    set(CMAKE_SIZEOF_VOID_P_BACKUP ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/tsl-sparse-mapConfigVersion.cmake"
                                     COMPATIBILITY SameMajorVersion)
    set(CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P_BACKUP})

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-sparse-mapConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-sparse-map")
endif()
