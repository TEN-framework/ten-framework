# MPark.Variant
#
# Copyright Michael Park, 2015-2017
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.6.3)

option(MPARK_VARIANT_EXCEPTIONS
       "Build the tests with exceptions support." ON)

if(MPARK_VARIANT_INCLUDE_MPARK_TESTS)

  add_definitions(-DGTEST_HAS_TR1_TUPLE=0)
  add_definitions(-DGTEST_HAS_STD_TUPLE=1)

  add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/googletest
                   ${CMAKE_BINARY_DIR}/3rdparty/googletest)

  config_compiler_and_linker()

  if(MPARK_VARIANT_EXCEPTIONS)
    set(compile_flags ${cxx_strict})
  else()
    set(compile_flags ${cxx_no_exception})
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
     (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    set(compile_flags "${compile_flags} /wd4577")
  endif()

  set(MPARK_TESTS
    assign.copy
    assign.fwd
    assign.move
    ctor.copy
    ctor.default
    ctor.fwd
    ctor.in_place
    ctor.move
    dtor
    get
    hash
    intro
    issue
    json
    mod
    relops
    swap
    visit)

  foreach(test ${MPARK_TESTS})
    add_executable(${test} ${test}.cpp)
    set_target_properties(${test} PROPERTIES COMPILE_FLAGS "${compile_flags}")
    target_link_libraries(${test} gtest_main mpark_variant)
    add_test(${test} ${test} --gtest_color=yes)
  endforeach()

endif()

if(MPARK_VARIANT_INCLUDE_LIBCXX_TESTS)

  include(ExternalProject)

  set(MPARK_VARIANT_LLVM_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/llvm)
  set(MPARK_VARIANT_LLVM_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/llvm-build)

  ExternalProject_Add(llvm
    GIT_REPOSITORY    https://git.llvm.org/git/llvm.git
    GIT_SHALLOW       1
    CMAKE_ARGS        -DCMAKE_BUILD_TYPE=Release
                      -DCMAKE_CXX_FLAGS="-w"
                      -DLIBCXX_ENABLE_EXCEPTIONS=${MPARK_VARIANT_EXCEPTIONS}
    SOURCE_DIR        ${MPARK_VARIANT_LLVM_SOURCE_DIR}
    BINARY_DIR        ${MPARK_VARIANT_LLVM_BINARY_DIR}
    BUILD_COMMAND     ${CMAKE_COMMAND} --build . --target cxx
          COMMAND     ${CMAKE_COMMAND} --build . --target cxxabi
          COMMAND     ${CMAKE_COMMAND} --build . --target cxx_experimental
    INSTALL_COMMAND   ""  # Disable install step
    STEP_TARGETS      download
  )

  set(MPARK_VARIANT_LIBCXX_SOURCE_DIR ${MPARK_VARIANT_LLVM_SOURCE_DIR}/projects/libcxx)
  set(MPARK_VARIANT_LIBCXX_BINARY_DIR ${MPARK_VARIANT_LLVM_BINARY_DIR}/projects/libcxx)

  ExternalProject_Add(libcxx-download
    GIT_REPOSITORY    https://git.llvm.org/git/libcxx.git
    GIT_SHALLOW       1
    DEPENDS           llvm-download
    SOURCE_DIR        ${MPARK_VARIANT_LIBCXX_SOURCE_DIR}
    BINARY_DIR        ${MPARK_VARIANT_LIBCXX_BINARY_DIR}
    CONFIGURE_COMMAND ""  # Disable configure step
    BUILD_COMMAND     ""  # Disable build step
    INSTALL_COMMAND   ""  # Disable install step
  )

  set(MPARK_VARIANT_LIBCXXABI_SOURCE_DIR ${MPARK_VARIANT_LLVM_SOURCE_DIR}/projects/libcxxabi)
  set(MPARK_VARIANT_LIBCXXABI_BINARY_DIR ${MPARK_VARIANT_LLVM_BINARY_DIR}/projects/libcxxabi)

  ExternalProject_Add(libcxxabi-download
    GIT_REPOSITORY    https://git.llvm.org/git/libcxxabi.git
    GIT_SHALLOW       1
    DEPENDS           llvm-download
    SOURCE_DIR        ${MPARK_VARIANT_LIBCXXABI_SOURCE_DIR}
    BINARY_DIR        ${MPARK_VARIANT_LIBCXXABI_BINARY_DIR}
    CONFIGURE_COMMAND ""  # Disable configure step
    BUILD_COMMAND     ""  # Disable build step
    INSTALL_COMMAND   ""  # Disable install step
  )

  ExternalProject_Add_StepDependencies(llvm configure libcxx-download libcxxabi-download)

  add_test(libcxx
    ${CMAKE_COMMAND} -E
        env MPARK_VARIANT_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        env MPARK_VARIANT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        env MPARK_VARIANT_LIBCXX_SOURCE_DIR=${MPARK_VARIANT_LIBCXX_SOURCE_DIR}
        env MPARK_VARIANT_LIBCXX_SITE_CONFIG=${MPARK_VARIANT_LIBCXX_BINARY_DIR}/test/lit.site.cfg
        env MPARK_VARIANT_LIT=${MPARK_VARIANT_LLVM_BINARY_DIR}/bin/llvm-lit
        ${CMAKE_CURRENT_SOURCE_DIR}/libcxx.sh
  )

endif()
