cmake_minimum_required(VERSION 3.14)
project(tl-optional 
  HOMEPAGE_URL https://tl.tartanllama.xyz
  DESCRIPTION "C++11/14/17 std::optional with functional-style extensions and reference support"
  VERSION 1.0.0
  LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(FetchContent)
include(CTest)

find_program(DPKG_BUILDPACKAGE_FOUND dpkg-buildpackage)
find_program(RPMBUILD_FOUND rpmbuild)

if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

option(OPTIONAL_BUILD_PACKAGE "Build packages" ON)

cmake_dependent_option(OPTIONAL_BUILD_TESTS
  "Enable tl::optional tests" ON
  "BUILD_TESTING" OFF)

cmake_dependent_option(OPTIONAL_BUILD_PACKAGE_DEB
  "Create DEB Package (${PROJECT_NAME})" ON
  "OPTIONAL_BUILD_PACKAGE;DPKG_BUILDPACKAGE_FOUND" OFF)
cmake_dependent_option(OPTIONAL_BUILD_PACKAGE_RPM
  "Create RPM Package (${PROJECT_NAME})" ON
  "OPTIONAL_BUILD_PACKAGE;RPMBUILD_FOUND" OFF)
cmake_dependent_option(OPTIONAL_BUILD_PACKAGE_MSI
  "Create MSI (${PROJECT_NAME})" ON
  "OPTIONAL_BUILD_PACKAGE;CMAKE_HOST_WIN32" OFF)

add_library(optional INTERFACE)
target_include_directories(optional
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if (NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  add_library(tl::optional ALIAS optional)
endif()

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}")

write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
  COMPATIBILITY SameMajorVersion
  ARCH_INDEPENDENT)

install(TARGETS optional EXPORT ${PROJECT_NAME}-targets)

install(EXPORT ${PROJECT_NAME}-targets
  DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
  NAMESPACE tl::
  FILE "${PROJECT_NAME}-targets.cmake")

install(FILES
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}")

install(DIRECTORY "include/" TYPE INCLUDE)

if (OPTIONAL_BUILD_TESTS)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(CATCH_INSTALL_HELPERS OFF)
  set(CATCH_BUILD_TESTING OFF)
  set(CATCH_INSTALL_DOCS OFF)
  FetchContent_Declare(Catch2 URL
    https://github.com/catchorg/Catch2/archive/v2.9.2.zip) 
  FetchContent_MakeAvailable(Catch2)

  file(GENERATE OUTPUT catch.main.cpp
    CONTENT [[
      #define CATCH_CONFIG_MAIN
      #include <catch2/catch.hpp>
  ]])
  set_property(SOURCE ${PROJECT_BINARY_DIR}/catch.main.cpp
    PROPERTY
      GENERATED ON)
  add_library(${PROJECT_NAME}-catch-main OBJECT)
  target_sources(${PROJECT_NAME}-catch-main PRIVATE
    ${PROJECT_BINARY_DIR}/catch.main.cpp)
  target_compile_options(${PROJECT_NAME}-catch-main
    PUBLIC
      $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:"$ENV{TRAVIS}">>:-stdlib=libc++>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>)
  target_link_options(${PROJECT_NAME}-catch-main
    PUBLIC
      $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:"$ENV{TRAVIS}">>:-stdlib=libc++>)
  target_link_libraries(${PROJECT_NAME}-catch-main
    PUBLIC
      Catch2::Catch2
      optional)

  file(GLOB test-sources CONFIGURE_DEPENDS tests/*.cpp)
  foreach (source IN LISTS test-sources)
    get_filename_component(name "${source}" NAME_WE)
    set(test "${PROJECT_NAME}-test-${name}")
    add_executable(${test}
      "${source}"
      $<TARGET_OBJECTS:${PROJECT_NAME}-catch-main>)
    target_link_libraries(${test}
      PRIVATE
        ${PROJECT_NAME}-catch-main)
    add_test(NAME ${PROJECT_NAME}::test::${name} COMMAND ${test})
  endforeach()
endif()

if (NOT OPTIONAL_BUILD_PACKAGE)
  return()
endif()

list(APPEND source-generators TBZ2 TGZ TXZ ZIP)

if (OPTIONAL_BUILD_PACKAGE_MSI)
  list(APPEND binary-generators "WIX")
endif()

if (OPTIONAL_BUILD_PACKAGE_DEB)
  list(APPEND binary-generators "DEB")
endif()

if (OPTIONAL_BUILD_PACKAGE_RPM)
  list(APPEND binary-generators "RPM")
endif()

set(CPACK_SOURCE_GENERATOR ${source-generators})
set(CPACK_GENERATOR ${binary-generators})

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILENAME}")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sy Brand")

list(APPEND CPACK_SOURCE_IGNORE_FILES /.git/ /build/ .gitignore .DS_Store)

include(CPack)
