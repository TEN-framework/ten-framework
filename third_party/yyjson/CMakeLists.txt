# Copyright (C) 2019 Yaoyuan <ibireme@gmail.com>.
# Released under the MIT License:
# https://github.com/ibireme/yyjson/blob/master/LICENSE

cmake_minimum_required(VERSION 3.5)
project(yyjson VERSION 0.10.0 LANGUAGES C)
set(YYJSON_SOVERSION 0)



# ------------------------------------------------------------------------------
# Build Type
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS "No build type selected, default to: Release")
        set(CMAKE_BUILD_TYPE Release)
    endif()
endif()



# ------------------------------------------------------------------------------
# Build Options for tests and docs
option(YYJSON_BUILD_TESTS "Build all tests" OFF)
option(YYJSON_BUILD_FUZZER "Build fuzzer" OFF)
option(YYJSON_BUILD_MISC "Build misc" OFF)
option(YYJSON_BUILD_DOC "Build documentation with doxygen" OFF)
option(YYJSON_ENABLE_COVERAGE "Enable code coverage for tests" OFF)
option(YYJSON_ENABLE_VALGRIND "Enable valgrind memory checker for tests" OFF)
option(YYJSON_ENABLE_SANITIZE "Enable sanitizer for tests" OFF)
option(YYJSON_ENABLE_FASTMATH "Enable fast-math for tests" OFF)
option(YYJSON_FORCE_32_BIT "Force 32-bit for tests" OFF)



# ------------------------------------------------------------------------------
# Compilation options, see yyjson.h for more explanation
option(YYJSON_DISABLE_READER "Disable JSON reader" OFF)
option(YYJSON_DISABLE_WRITER "Disable JSON writer" OFF)
option(YYJSON_DISABLE_UTILS "Disable JSON Pointer, JSON Patch, JSON Merge Patch" OFF)
option(YYJSON_DISABLE_FAST_FP_CONV "Disable custom floating-point number conversion" OFF)
option(YYJSON_DISABLE_NON_STANDARD "Disable non-standard JSON support" OFF)
option(YYJSON_DISABLE_UTF8_VALIDATION "Disable UTF-8 validation" OFF)
option(YYJSON_DISABLE_UNALIGNED_MEMORY_ACCESS "Disable unaligned memory access explicit" OFF)

if(YYJSON_DISABLE_READER)
    add_definitions(-DYYJSON_DISABLE_READER)
endif()
if(YYJSON_DISABLE_WRITER)
    add_definitions(-DYYJSON_DISABLE_WRITER)
endif()
if(YYJSON_DISABLE_UTILS)
    add_definitions(-DYYJSON_DISABLE_UTILS)
endif()
if(YYJSON_DISABLE_FAST_FP_CONV)
    add_definitions(-DYYJSON_DISABLE_FAST_FP_CONV)
endif()
if(YYJSON_DISABLE_NON_STANDARD)
    add_definitions(-DYYJSON_DISABLE_NON_STANDARD)
endif()
if(YYJSON_DISABLE_UTF8_VALIDATION)
    add_definitions(-DYYJSON_DISABLE_UTF8_VALIDATION)
endif()
if(YYJSON_DISABLE_UNALIGNED_MEMORY_ACCESS)
    add_definitions(-DYYJSON_DISABLE_UNALIGNED_MEMORY_ACCESS)
endif()



# ------------------------------------------------------------------------------
# Library
add_library(yyjson src/yyjson.h src/yyjson.c)
target_include_directories(yyjson PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
set_target_properties(yyjson PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${YYJSON_SOVERSION})



# ------------------------------------------------------------------------------
# Project Config
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(XcodeProperty)

if(XCODE)
    set(YYJSON_FLAGS "-Wall -Wextra -Werror -pedantic -pedantic-errors")
    if(YYJSON_ENABLE_FASTMATH)
        set(YYJSON_FLAGS "${YYJSON_FLAGS} -ffast-math")
    endif()
    
    set_default_xcode_property(yyjson)
    set_xcode_deployment_version(yyjson "10.13" "12.0" "12.0" "4.0")

    set_xcode_property(yyjson GCC_C_LANGUAGE_STANDARD "c89")
    set_xcode_property(yyjson CLANG_CXX_LANGUAGE_STANDARD "c++98")
    
    set_xcode_property(yyjson OTHER_CFLAGS[variant=Debug] ${YYJSON_FLAGS})
    set_xcode_property(yyjson OTHER_CFLAGS[variant=MinSizeRel] ${YYJSON_FLAGS})
    set_xcode_property(yyjson OTHER_CFLAGS[variant=RelWithDebInfo] ${YYJSON_FLAGS})
    set_xcode_property(yyjson OTHER_CFLAGS[variant=Release] ${YYJSON_FLAGS})
    
elseif(MSVC)
    set(YYJSON_FLAGS "/utf-8")
    if(YYJSON_ENABLE_FASTMATH)
        set(YYJSON_FLAGS "${YYJSON_FLAGS} /fp:fast")
    endif()
    
    target_compile_options(yyjson PRIVATE $<$<C_COMPILER_ID:MSVC>:${YYJSON_FLAGS}>)
    target_compile_options(yyjson PRIVATE $<$<CXX_COMPILER_ID:MSVC>:${YYJSON_FLAGS}>)
    
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|Intel")
    set(YYJSON_FLAGS "")
    if(YYJSON_ENABLE_FASTMATH)
        set(YYJSON_FLAGS "-ffast-math")
    endif()
    
    target_compile_options(yyjson PRIVATE $<$<COMPILE_LANGUAGE:C>:${YYJSON_FLAGS}>)
    target_compile_options(yyjson PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${YYJSON_FLAGS}>)
    
    if(YYJSON_FORCE_32_BIT)
        set(CMAKE_C_FLAGS -m32)
        set(CMAKE_CXX_FLAGS -m32)
    endif()

endif()

include(CheckIncludeFile)
check_include_file(stdint.h YYJSON_HAS_STDINT_H)
check_include_file(stdbool.h YYJSON_HAS_STDBOOL_H)

if(BUILD_SHARED_LIBS)
    if(WIN32)
        target_compile_definitions(yyjson PUBLIC
            $<BUILD_INTERFACE:YYJSON_EXPORTS>
            $<INSTALL_INTERFACE:YYJSON_IMPORTS>)
    endif()
endif()



# ------------------------------------------------------------------------------
# Install
include(GNUInstallDirs)

install(TARGETS yyjson
        EXPORT yyjson-targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT yyjson-targets
        FILE yyjson-config.cmake
        NAMESPACE yyjson::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/yyjson")
install(FILES src/yyjson.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

configure_file(yyjson.pc.in ${CMAKE_BINARY_DIR}/yyjson.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/yyjson.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


# ------------------------------------------------------------------------------
# Testing
if(YYJSON_BUILD_TESTS)
    enable_testing()

    # Check C++ compiler
    include(CheckLanguage)
    check_language(CXX)
    if(CMAKE_CXX_COMPILER)
        enable_language(CXX)
    endif()
    
    if(XCODE)
        # Config XCTest
        find_package(XCTest REQUIRED)
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
        set(YYJSON_TEST_DATA ${CMAKE_CURRENT_SOURCE_DIR}/test/data)
        
        # Add all test cases to XCTest: c files prefixed with 'test_'
        file(GLOB YYJSON_TEST_SOURCE
            "test/test_*.c"
        )
        set(YYJSON_TEST_LINES "")
        foreach(SRC_FILE ${YYJSON_TEST_SOURCE})
            string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" SRC_NAME ${SRC_FILE})
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}- (void)${SRC_NAME} {\n")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}    extern void ${SRC_NAME}(void);\n")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}    ${SRC_NAME}();\n")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}}\n\n")
        endforeach()
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/test/xctest/yy_xctest.m.in"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/xctest/yy_xctest.m"
            @ONLY
        )
        unset(YYJSON_TEST_LINES)
        
        # Add source files and search path to XCTest
        file(GLOB YYJSON_TEST_SOURCE
            "test/test_*.c"
            "test/util/*.h"
            "test/util/*.c"
            "test/xctest/*"
        )
        xctest_add_bundle(yyjson_tests yyjson
            ${YYJSON_TEST_SOURCE}
            ${YYJSON_TEST_DATA}
        )
        set_target_properties(yyjson_tests PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/test/xctest/Info.plist
            RESOURCE ${YYJSON_TEST_DATA}
        )
        target_include_directories(yyjson_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/test/util
            ${CMAKE_CURRENT_SOURCE_DIR}/test/xctest
        )
        xctest_add_test(XCTest.yyjson yyjson_tests)
        
        set_default_xcode_property(yyjson_tests)
        set_xcode_deployment_version(yyjson_tests "10.13" "12.0" "12.0" "4.0")
        
    else()
        # Check valgrind command
        if (YYJSON_ENABLE_VALGRIND)
            find_program(MEMORYCHECK_COMMAND valgrind)
            if ("${MEMORYCHECK_COMMAND}" MATCHES "MEMORYCHECK_COMMAND-NOTFOUND")
                message(WARNING "Valgrind not found")
                unset(MEMORYCHECK_COMMAND)
            else()
                message(STATUS "Valgrind found")
                set(MEMORYCHECK_COMMAND_OPTIONS
                    --tool=memcheck
                    --leak-check=full
                    --trace-children=yes
                    --error-exitcode=1
                )
            endif()
        endif()
        
        # Copy test data
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/test/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
        add_definitions(-DYYJSON_TEST_DATA_PATH="${CMAKE_CURRENT_BINARY_DIR}")
        
        # Add dependency
        add_library(yyjson_test_utils
            test/util/yy_test_utils.c
            test/util/goo_double_conv.c
        )
        target_include_directories(yyjson_test_utils PUBLIC test/util)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(yyjson_test_utils PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=c99>)
        endif()
        
        # Add all test cases: c files prefixed with 'test_'
        file(GLOB YYJSON_TEST_SOURCE
            "test/test_*.c"
        )
        foreach(SRC_FILE ${YYJSON_TEST_SOURCE})
            string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" SRC_NAME ${SRC_FILE})
            add_executable(${SRC_NAME} ${SRC_FILE})
            target_link_libraries(${SRC_NAME} PRIVATE yyjson yyjson_test_utils)
            
            if(MSVC)
                target_compile_options(${SRC_NAME} PRIVATE $<$<C_COMPILER_ID:MSVC>:/utf-8>)
                target_compile_options(${SRC_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
            endif()
            
            if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${SRC_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=c99>)
            endif()
            
            if(MEMORYCHECK_COMMAND)
                add_test(NAME ${SRC_NAME}
                         COMMAND "${MEMORYCHECK_COMMAND}" ${MEMORYCHECK_COMMAND_OPTIONS} "${CMAKE_CURRENT_BINARY_DIR}/${SRC_NAME}")
            else()
                add_test(${SRC_NAME} ${SRC_NAME})
            endif()
            
            set_tests_properties(${SRC_NAME} PROPERTIES TIMEOUT 300)
            message(STATUS "Add test: ${SRC_NAME}")
        endforeach()
        
        # Add code coverage and sanitize
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            if (YYJSON_ENABLE_COVERAGE)
                if(CMAKE_C_COMPILER_ID MATCHES "Clang")
                    set(COMPILE_FLAGS -fprofile-instr-generate -fcoverage-mapping)
                else()
                    set(COMPILE_FLAGS --coverage)
                endif()
                target_compile_options(yyjson PRIVATE ${COMPILE_FLAGS})
                target_link_libraries(yyjson INTERFACE ${COMPILE_FLAGS})
                foreach(SRC_FILE ${YYJSON_TEST_SOURCE})
                    string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" SRC_NAME ${SRC_FILE})
                    target_compile_options(${SRC_NAME} PRIVATE ${COMPILE_FLAGS})
                endforeach()
            endif()
            if (YYJSON_ENABLE_SANITIZE)
                set(COMPILE_FLAGS
                    -fsanitize=address
                    -fsanitize=undefined
                    -fsanitize=leak
                    -fsanitize-recover=all
                    -fno-omit-frame-pointer
                    -fno-optimize-sibling-calls
                    -O1
                    -g
                )
                target_compile_options(yyjson PRIVATE ${COMPILE_FLAGS})
                target_link_libraries(yyjson INTERFACE ${COMPILE_FLAGS})
            endif()
        endif()

    endif()
    
    
    # Test compatibility
    if(MSVC)
        add_executable(test_compile_ansi_c test/compile_ansi.c)
        target_include_directories(test_compile_ansi_c PRIVATE src)
        add_executable(test_compile_ansi_cpp test/compile_ansi.cpp)
        target_include_directories(test_compile_ansi_cpp PRIVATE src)
        
        # set warning level 4, treat warnings as errors
        set(YYJSON_STRICT_FLAGS /W4 /WX)
        target_compile_options(test_compile_ansi_c PRIVATE
            $<$<COMPILE_LANGUAGE:C>:${YYJSON_STRICT_FLAGS}>)
        target_compile_options(test_compile_ansi_cpp PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:${YYJSON_STRICT_FLAGS}>)
        
        # add tests
        add_test(test_compile_ansi_c test_compile_ansi_c)
        message(STATUS "Add test: test_compile_ansi_c")
        add_test(test_compile_ansi_cpp test_compile_ansi_cpp)
        message(STATUS "Add test: test_compile_ansi_cpp")
        
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        
        # Compiler flag check
        include(CheckCCompilerFlag)
        include(CheckCXXCompilerFlag)
        
        # Flags to check ANSI C/C++ standard strictly, treat warnings as errors
        set(YYJSON_STRICT_C_FLAGS "")
        set(YYJSON_STRICT_CXX_FLAGS "")
        
        # Base flags
        set(YYJSON_BASE_FLAGS
            -pedantic
            -pedantic-errors
            -Werror
            -Wall
            -Wextra
            -Wconversion
            -Wundef
            -Wcast-qual
            -Wfloat-equal
            -Wredundant-decls
            -Wpointer-arith
        )
        # Extra flags supported by different compilers
        if(CMAKE_C_COMPILER_ID MATCHES "GNU")
            set(YYJSON_EXTRA_FLAGS
                -Wmissing-prototypes    # C only
                -Wstrict-prototypes     # C only
                -Wlogical-op            # GCC 4.3
                -Wdouble-promotion      # GCC 4.6
                -Wduplicated-cond       # GCC 6.0
                -Wduplicated-branches   # GCC 7.0
                -Wcast-align=strict     # GCC 8.0
                -Wshadow=global         # GCC 7.0
            )
        else()
            set(YYJSON_EXTRA_FLAGS
                -Wmissing-prototypes    # C only
                -Wstrict-prototypes     # C only
                -Wdouble-promotion      # Clang
                -Wcast-align            # Clang
                -Wcomma                 # Clang
                -Wdeprecated            # Clang
                -Wdocumentation         # Clang
                -Wnewline-eof           # Clang
                -Wshadow-all            # Clang
                -Wunaligned-access      # Clang
                -Wunreachable-code      # Clang
                -Wextra-semi            # Clang
                -Wextra-semi-stmt       # Clang
                # used for finding potential issues, but too noisy for regular testing:
                # -Weverything                        # enable all warnings
                # -Wno-reserved-macro-identifier      # disable for stdbool define
                # -Wno-padded                         # disable for some structs
                # -Wno-old-style-cast                 # disable for C++
                # -Wno-zero-as-null-pointer-constant  # disable for C++
            )
        endif()
        
        # Combine all available flags
        list(APPEND YYJSON_STRICT_C_FLAGS ${YYJSON_BASE_FLAGS})
        list(APPEND YYJSON_STRICT_CXX_FLAGS ${YYJSON_BASE_FLAGS})
        foreach(flag ${YYJSON_EXTRA_FLAGS})
            string(REGEX REPLACE "[-.+/:=]" "_" flag_name "${flag}")
            
            check_c_compiler_flag(${flag} COMPILER_SUPPORT_C_FLAG${flag_name})
            if(COMPILER_SUPPORT_C_FLAG${flag_name})
                list(APPEND YYJSON_STRICT_C_FLAGS ${flag})
            endif()
            
            if(CMAKE_CXX_COMPILER)
                check_cxx_compiler_flag(${flag} COMPILER_SUPPORT_CXX_FLAG${flag_name})
                if(COMPILER_SUPPORT_CXX_FLAG${flag_name})
                    list(APPEND YYJSON_STRICT_CXX_FLAGS ${flag})
                endif()
            endif()
            
        endforeach()
        
        # Check ANSI C
        check_c_compiler_flag("-ansi" COMPILER_SUPPORTS_ANSI_C)
        if(COMPILER_SUPPORTS_ANSI_C)
            add_executable(test_compile_ansi_c test/compile_ansi.c)
            target_include_directories(test_compile_ansi_c PRIVATE src)
            target_compile_options(test_compile_ansi_c PRIVATE
                $<$<COMPILE_LANGUAGE:C>:-ansi ${YYJSON_STRICT_C_FLAGS}>)
        endif()
        
        # Check modern C
        check_c_compiler_flag("-std=c11" COMPILER_SUPPORTS_C11)
        if(COMPILER_SUPPORTS_C11)
            add_executable(test_compile_c11 test/compile_ansi.c)
            target_include_directories(test_compile_c11 PRIVATE src)
            target_compile_options(test_compile_c11 PRIVATE
                $<$<COMPILE_LANGUAGE:C>:-std=c11 ${YYJSON_STRICT_C_FLAGS}>)
        endif()
        
        if(CMAKE_CXX_COMPILER)
            
            # Check ANSI C++
            check_cxx_compiler_flag("-ansi" COMPILER_SUPPORTS_ANSI_CXX)
            if(COMPILER_SUPPORTS_ANSI_CXX)
                add_executable(test_compile_ansi_cxx test/compile_ansi.cpp)
                target_include_directories(test_compile_ansi_cxx PRIVATE src)
                target_compile_options(test_compile_ansi_cxx PRIVATE
                    $<$<COMPILE_LANGUAGE:CXX>:-ansi ${YYJSON_STRICT_CXX_FLAGS}>)
            endif()
            
            # Check modern C++
            check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
            if(COMPILER_SUPPORTS_CXX17)
                add_executable(test_compile_cxx17 test/compile_ansi.cpp)
                target_include_directories(test_compile_cxx17 PRIVATE src)
                target_compile_options(test_compile_cxx17 PRIVATE
                    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 ${YYJSON_STRICT_CXX_FLAGS}>)
            endif()
            
        endif()
        
    endif()
    
endif()



# ------------------------------------------------------------------------------
# Fuzzing
if (YYJSON_BUILD_FUZZER)
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        enable_testing()
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzzer.dict" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
        file(GLOB YYJSON_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test/data/json/**/*.json")
        file(COPY ${YYJSON_FILES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/corpus")
        
        set(COMPILE_FLAGS -fsanitize=fuzzer,address -O1 -g)
        target_compile_options(yyjson PRIVATE ${COMPILE_FLAGS})
        target_link_libraries(yyjson INTERFACE ${COMPILE_FLAGS})

        add_executable(fuzzer "fuzz/fuzzer.c")
        target_link_libraries(fuzzer yyjson)
        add_test(test_fuzzer ${CMAKE_CURRENT_BINARY_DIR}/fuzzer -dict=fuzzer.dict -max_total_time=300 ${CMAKE_CURRENT_BINARY_DIR}/corpus)
        set_tests_properties(test_fuzzer PROPERTIES TIMEOUT 600)
    else()
        message(WARNING "LibFuzzer requires LLVM Clang as compiler")
    endif()
endif()



# ------------------------------------------------------------------------------
# Miscellaneous
if(YYJSON_BUILD_MISC)
    # jsoninfo
    add_executable(jsoninfo "misc/jsoninfo.c")
    target_link_libraries(jsoninfo PRIVATE yyjson)
    if(XCODE)
        set_default_xcode_property(jsoninfo)
    endif()

    # make tables
    find_package(GMP REQUIRED)
    find_package(MPFR REQUIRED)
    add_executable(make_tables "misc/make_tables.c")
    target_include_directories(make_tables PRIVATE
        ${GMP_INCLUDE_DIR}
        ${MPFR_INCLUDES}
    )
    target_link_libraries(make_tables PRIVATE
        ${GMP_LIBRARIES}
        ${MPFR_LIBRARIES}
    )
    if(XCODE)
        set_default_xcode_property(make_tables)
    endif()
endif()



# ------------------------------------------------------------------------------
# Doxygen
if(YYJSON_BUILD_DOC)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        message(STATUS "Doxygen found")
        
        set(DOXYGEN_IN "${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/Doxyfile.in")
        set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        message(STATUS "Doxygen will output to ${CMAKE_CURRENT_BINARY_DIR}/doxygen/html")
        
        add_custom_target(yyjson_doc ALL
            COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating documentation with Doxygen"
            VERBATIM)
    else()
        message(WARNING "Doxygen not found")
    endif()
endif()
