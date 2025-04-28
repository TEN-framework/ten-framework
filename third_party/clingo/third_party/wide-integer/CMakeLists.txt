project(wide-integer)

cmake_minimum_required(VERSION 3.16.3)

find_package(Boost)
if (Boost_FOUND)
  include(CTest)

  add_subdirectory("examples")
  add_subdirectory("test")
endif()

add_library(WideInteger INTERFACE)
target_compile_features(WideInteger INTERFACE cxx_std_11)

target_include_directories(
  WideInteger SYSTEM INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)

install(TARGETS WideInteger EXPORT WideIntegerTargets)
install(
  FILES math/wide_integer/uintwide_t.h
  DESTINATION include/math/wide_integer/)
install(EXPORT WideIntegerTargets
  FILE WideIntegerConfig.cmake
  NAMESPACE WideInteger::
  DESTINATION lib/cmake/wide-integer)
