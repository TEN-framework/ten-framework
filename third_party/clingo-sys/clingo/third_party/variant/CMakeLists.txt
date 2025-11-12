# MPark.Variant
#
# Copyright Michael Park, 2015-2017
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.6.3)

project(MPark.Variant VERSION 1.4.1 LANGUAGES CXX)

# Option.
set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING
    "Semicolon-separated list of tests to build. \
     Possible values are `mpark`, and `libc++`.")

# Target.
add_library(mpark_variant INTERFACE)
target_include_directories(mpark_variant INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# Config.
include(CMakePackageConfigHelpers)

configure_package_config_file(
  cmake/mpark_variant-config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/mpark_variant-config.cmake"
  INSTALL_DESTINATION lib/cmake/mpark_variant
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/mpark_variant-config-version.cmake"
  COMPATIBILITY AnyNewerVersion)

# Export.
export(
  TARGETS mpark_variant
  FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/mpark_variant-targets.cmake")

# Install.
install(TARGETS mpark_variant EXPORT mpark_variant)

install(
  EXPORT mpark_variant
  FILE mpark_variant-targets.cmake
  DESTINATION lib/cmake/mpark_variant)

install(DIRECTORY include/mpark DESTINATION include)

install(
  FILES
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/mpark_variant-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/mpark_variant-config-version.cmake"
  DESTINATION lib/cmake/mpark_variant)

# Test.
list(REMOVE_DUPLICATES MPARK_VARIANT_INCLUDE_TESTS)

list(FIND MPARK_VARIANT_INCLUDE_TESTS "mpark" MPARK_VARIANT_INDEX)
if(NOT MPARK_VARIANT_INDEX EQUAL -1)
  set(MPARK_VARIANT_INCLUDE_MPARK_TESTS ON)
  list(REMOVE_AT MPARK_VARIANT_INCLUDE_TESTS MPARK_VARIANT_INDEX)
endif()

list(FIND MPARK_VARIANT_INCLUDE_TESTS "libc++" MPARK_VARIANT_INDEX)
if(NOT MPARK_VARIANT_INDEX EQUAL -1)
  set(MPARK_VARIANT_INCLUDE_LIBCXX_TESTS ON)
  list(REMOVE_AT MPARK_VARIANT_INCLUDE_TESTS MPARK_VARIANT_INDEX)
endif()

list(LENGTH MPARK_VARIANT_INCLUDE_TESTS MPARK_VARIANT_LENGTH)
if(MPARK_VARIANT_LENGTH GREATER 0)
  message(FATAL_ERROR
    "The following values in `MPARK_VARIANT_INCLUDE_TESTS` are not one of "
    "the possible values, `mpark`, and `libc++`: ${MPARK_VARIANT_INCLUDE_TESTS}")
endif()

if(MPARK_VARIANT_INCLUDE_MPARK_TESTS OR MPARK_VARIANT_INCLUDE_LIBCXX_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
