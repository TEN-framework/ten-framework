#
# CMake build system design considerations:
#
# - Include directories:
#   + Do not define include directories globally using the include_directories
#     command but rather at the target level using the
#     target_include_directories command. That way, it is easier to guarantee
#     that targets are built using the proper list of include directories.
#   + Use the PUBLIC and PRIVATE keywords to specify the scope of include
#     directories. That way, a target linking to a library (using the
#     target_link_libraries command) inherits from the library PUBLIC include
#     directories and not from the PRIVATE ones.
# - MBEDTLS_TARGET_PREFIX: CMake targets are designed to be alterable by calling
#   CMake in order to avoid target name clashes, via the use of
#   MBEDTLS_TARGET_PREFIX. The value of this variable is prefixed to the
#   mbedtls, mbedx509, mbedcrypto and apidoc targets.
#

# We specify a minimum requirement of 3.10.2, but for now use 3.5.1 here
# until our infrastructure catches up.
cmake_minimum_required(VERSION 3.5.1)

include(CMakePackageConfigHelpers)

# https://cmake.org/cmake/help/latest/policy/CMP0011.html
# Setting this policy is required in CMake >= 3.18.0, otherwise a warning is generated. The OLD
# policy setting is deprecated, and will be removed in future versions.
cmake_policy(SET CMP0011 NEW)
# https://cmake.org/cmake/help/latest/policy/CMP0012.html
# Setting the CMP0012 policy to NEW is required for FindPython3 to work with CMake 3.18.2
# (there is a bug in this particular version), otherwise, setting the CMP0012 policy is required
# for CMake versions >= 3.18.3 otherwise a deprecated warning is generated. The OLD policy setting
# is deprecated and will be removed in future versions.
cmake_policy(SET CMP0012 NEW)

if(TEST_CPP)
    project("mbed TLS" C CXX)
else()
    project("mbed TLS" C)
endif()

include(GNUInstallDirs)

# Determine if mbed TLS is being built as a subproject using add_subdirectory()
if(NOT DEFINED MBEDTLS_AS_SUBPROJECT)
  set(MBEDTLS_AS_SUBPROJECT ON)
  if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MBEDTLS_AS_SUBPROJECT OFF)
  endif()
endif()

# Set the project root directory.
set(MBEDTLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_PROGRAMS "Build mbed TLS programs." ON)

option(UNSAFE_BUILD "Allow unsafe builds. These builds ARE NOT SECURE." OFF)
option(MBEDTLS_FATAL_WARNINGS "Compiler warnings treated as errors" ON)
if(CMAKE_HOST_WIN32)
    option(GEN_FILES "Generate the auto-generated files as needed" OFF)
else()
    option(GEN_FILES "Generate the auto-generated files as needed" OFF)
endif()

option(DISABLE_PACKAGE_CONFIG_AND_INSTALL "Disable package configuration, target export and installation" ${MBEDTLS_AS_SUBPROJECT})

string(REGEX MATCH "Clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER_ID}")
string(REGEX MATCH "GNU" CMAKE_COMPILER_IS_GNU "${CMAKE_C_COMPILER_ID}")
string(REGEX MATCH "IAR" CMAKE_COMPILER_IS_IAR "${CMAKE_C_COMPILER_ID}")
string(REGEX MATCH "MSVC" CMAKE_COMPILER_IS_MSVC "${CMAKE_C_COMPILER_ID}")

# the test suites currently have compile errors with MSVC
if(CMAKE_COMPILER_IS_MSVC)
    option(ENABLE_TESTING "Build mbed TLS tests." OFF)
else()
    option(ENABLE_TESTING "Build mbed TLS tests." ON)
endif()

# Warning string - created as a list for compatibility with CMake 2.8
set(CTR_DRBG_128_BIT_KEY_WARN_L1 "****  WARNING!  MBEDTLS_CTR_DRBG_USE_128_BIT_KEY defined!\n")
set(CTR_DRBG_128_BIT_KEY_WARN_L2 "****  Using 128-bit keys for CTR_DRBG limits the security of generated\n")
set(CTR_DRBG_128_BIT_KEY_WARN_L3 "****  keys and operations that use random values generated to 128-bit security\n")

set(CTR_DRBG_128_BIT_KEY_WARNING "${WARNING_BORDER}"
                         "${CTR_DRBG_128_BIT_KEY_WARN_L1}"
                         "${CTR_DRBG_128_BIT_KEY_WARN_L2}"
                         "${CTR_DRBG_128_BIT_KEY_WARN_L3}"
                         "${WARNING_BORDER}")

# Python 3 is only needed here to check for configuration warnings.
if(NOT CMAKE_VERSION VERSION_LESS 3.15.0)
    set(Python3_FIND_STRATEGY LOCATION)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        set(MBEDTLS_PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
    endif()
else()
    find_package(PythonInterp 3)
    if(PYTHONINTERP_FOUND)
        set(MBEDTLS_PYTHON_EXECUTABLE ${PYTHON_EXECUTABLE})
    endif()
endif()
if(MBEDTLS_PYTHON_EXECUTABLE)

    # If 128-bit keys are configured for CTR_DRBG, display an appropriate warning
    execute_process(COMMAND ${MBEDTLS_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/config.py -f ${CMAKE_CURRENT_SOURCE_DIR}/include/mbedtls/mbedtls_config.h get MBEDTLS_CTR_DRBG_USE_128_BIT_KEY
                        RESULT_VARIABLE result)
    if(${result} EQUAL 0)
        message(WARNING ${CTR_DRBG_128_BIT_KEY_WARNING})
    endif()

endif()

# If this is the root project add longer list of available CMAKE_BUILD_TYPE values
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
        CACHE STRING "Choose the type of build: None Debug Release Coverage ASan ASanDbg MemSan MemSanDbg Check CheckFull"
        FORCE)
endif()

# Create a symbolic link from ${base_name} in the binary directory
# to the corresponding path in the source directory.
function(link_to_source base_name)
    # Get OS dependent path to use in `execute_process`
    if (CMAKE_HOST_WIN32)
        #mklink is an internal command of cmd.exe it can only work with \
        string(REPLACE "/" "\\" link "${CMAKE_CURRENT_BINARY_DIR}/${base_name}")
        string(REPLACE "/" "\\" target "${CMAKE_CURRENT_SOURCE_DIR}/${base_name}")
    else()
        set(link "${CMAKE_CURRENT_BINARY_DIR}/${base_name}")
        set(target "${CMAKE_CURRENT_SOURCE_DIR}/${base_name}")
    endif()

    if (NOT EXISTS ${link})
        if (CMAKE_HOST_UNIX)
            set(command ln -s ${target} ${link})
        else()
            if (IS_DIRECTORY ${target})
                set(command cmd.exe /c mklink /j ${link} ${target})
            else()
                set(command cmd.exe /c mklink /h ${link} ${target})
            endif()
        endif()

        execute_process(COMMAND ${command}
            RESULT_VARIABLE result
            ERROR_VARIABLE output)

        if (NOT ${result} EQUAL 0)
            message(FATAL_ERROR "Could not create symbolic link for: ${target} --> ${output}")
        endif()
    endif()
endfunction(link_to_source)

# Get the filename without the final extension (i.e. convert "a.b.c" to "a.b")
function(get_name_without_last_ext dest_var full_name)
    # Split into a list on '.' (but a cmake list is just a ';'-separated string)
    string(REPLACE "." ";" ext_parts "${full_name}")
    # Remove the last item if there are more than one
    list(LENGTH ext_parts ext_parts_len)
    if (${ext_parts_len} GREATER "1")
        math(EXPR ext_parts_last_item "${ext_parts_len} - 1")
        list(REMOVE_AT ext_parts ${ext_parts_last_item})
    endif()
    # Convert back to a string by replacing separators with '.'
    string(REPLACE ";" "." no_ext_name "${ext_parts}")
    # Copy into the desired variable
    set(${dest_var} ${no_ext_name} PARENT_SCOPE)
endfunction(get_name_without_last_ext)

string(REGEX MATCH "Clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER_ID}")

include(CheckCCompilerFlag)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

if(CMAKE_COMPILER_IS_GNU)
    # some warnings we want are not available with old GCC versions
    # note: starting with CMake 2.8 we could use CMAKE_C_COMPILER_VERSION
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GCC_VERSION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wwrite-strings")
    if (GCC_VERSION VERSION_GREATER 3.0 OR GCC_VERSION VERSION_EQUAL 3.0)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat=2 -Wno-format-nonliteral")
    endif()
    if (GCC_VERSION VERSION_GREATER 4.3 OR GCC_VERSION VERSION_EQUAL 4.3)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvla")
    endif()
    if (GCC_VERSION VERSION_GREATER 4.5 OR GCC_VERSION VERSION_EQUAL 4.5)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wlogical-op")
    endif()
    if (GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
    endif()
    if (GCC_VERSION VERSION_GREATER 5.0)
        CHECK_C_COMPILER_FLAG("-Wformat-signedness" C_COMPILER_SUPPORTS_WFORMAT_SIGNEDNESS)
        if(C_COMPILER_SUPPORTS_WFORMAT_SIGNEDNESS)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-signedness")
        endif()
    endif()
    if (GCC_VERSION VERSION_GREATER 7.0 OR GCC_VERSION VERSION_EQUAL 7.0)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-overflow=2 -Wformat-truncation")
    endif()
    set(CMAKE_C_FLAGS_RELEASE     "-O2")
    set(CMAKE_C_FLAGS_DEBUG       "-O0 -g3")
    set(CMAKE_C_FLAGS_COVERAGE    "-O0 -g3 --coverage")
    set(CMAKE_C_FLAGS_ASAN        "-fsanitize=address -fno-common -fsanitize=undefined -fno-sanitize-recover=all -O3")
    set(CMAKE_C_FLAGS_ASANDBG     "-fsanitize=address -fno-common -fsanitize=undefined -fno-sanitize-recover=all -O1 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(CMAKE_C_FLAGS_CHECK       "-Os")
    set(CMAKE_C_FLAGS_CHECKFULL   "${CMAKE_C_FLAGS_CHECK} -Wcast-qual")
endif(CMAKE_COMPILER_IS_GNU)

if(CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wwrite-strings -Wpointer-arith -Wimplicit-fallthrough -Wshadow -Wvla -Wformat=2 -Wno-format-nonliteral")
    set(CMAKE_C_FLAGS_RELEASE     "-O2")
    set(CMAKE_C_FLAGS_DEBUG       "-O0 -g3")
    set(CMAKE_C_FLAGS_COVERAGE    "-O0 -g3 --coverage")
    set(CMAKE_C_FLAGS_ASAN        "-fsanitize=address -fno-common -fsanitize=undefined -fno-sanitize-recover=all -O3")
    set(CMAKE_C_FLAGS_ASANDBG     "-fsanitize=address -fno-common -fsanitize=undefined -fno-sanitize-recover=all -O1 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(CMAKE_C_FLAGS_MEMSAN      "-fsanitize=memory -O3")
    set(CMAKE_C_FLAGS_MEMSANDBG   "-fsanitize=memory -O1 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2")
    set(CMAKE_C_FLAGS_CHECK       "-Os")
endif(CMAKE_COMPILER_IS_CLANG)

if(CMAKE_COMPILER_IS_IAR)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --warn_about_c_style_casts --warnings_are_errors -Ohz")
endif(CMAKE_COMPILER_IS_IAR)

if(CMAKE_COMPILER_IS_MSVC)
    # Strictest warnings, UTF-8 source and execution charset
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /utf-8")
endif(CMAKE_COMPILER_IS_MSVC)

if(MBEDTLS_FATAL_WARNINGS)
    if(CMAKE_COMPILER_IS_MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
    endif(CMAKE_COMPILER_IS_MSVC)

    if(CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_GNU)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
        if(UNSAFE_BUILD)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=cpp")
            set(CMAKE_C_FLAGS_ASAN "${CMAKE_C_FLAGS_ASAN} -Wno-error=cpp")
            set(CMAKE_C_FLAGS_ASANDBG "${CMAKE_C_FLAGS_ASANDBG} -Wno-error=cpp")
        endif(UNSAFE_BUILD)
    endif(CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_GNU)
endif(MBEDTLS_FATAL_WARNINGS)

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    if(CMAKE_COMPILER_IS_GNU OR CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_SHARED_LINKER_FLAGS "--coverage")
    endif(CMAKE_COMPILER_IS_GNU OR CMAKE_COMPILER_IS_CLANG)
endif(CMAKE_BUILD_TYPE STREQUAL "Coverage")

if(LIB_INSTALL_DIR)
    set(CMAKE_INSTALL_LIBDIR "${LIB_INSTALL_DIR}")
endif()

add_subdirectory(include)

add_subdirectory(3rdparty)

add_subdirectory(library)

#
# The C files in tests/src directory contain test code shared among test suites
# and programs. This shared test code is compiled and linked to test suites and
# programs objects as a set of compiled objects. The compiled objects are NOT
# built into a library that the test suite and program objects would link
# against as they link against the mbedcrypto, mbedx509 and mbedtls libraries.
# The reason is that such library is expected to have mutual dependencies with
# the aforementioned libraries and that there is as of today no portable way of
# handling such dependencies (only toolchain specific solutions).
#
# Thus the below definition of the `mbedtls_test` CMake library of objects
# target. This library of objects is used by tests and programs CMake files
# to define the test executables.
#
if(ENABLE_TESTING OR ENABLE_PROGRAMS)
    file(GLOB MBEDTLS_TEST_FILES ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/*.c ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/drivers/*.c)
    add_library(mbedtls_test OBJECT ${MBEDTLS_TEST_FILES})
    target_include_directories(mbedtls_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library)
endif()

if(ENABLE_PROGRAMS)
    add_subdirectory(programs)
endif()

ADD_CUSTOM_TARGET(${MBEDTLS_TARGET_PREFIX}apidoc
    COMMAND doxygen mbedtls.doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doxygen)

if(ENABLE_TESTING)
    enable_testing()

    add_subdirectory(tests)

    # additional convenience targets for Unix only
    if(UNIX)

        ADD_CUSTOM_TARGET(covtest
            COMMAND make test
            COMMAND programs/test/selftest
            COMMAND tests/compat.sh
            COMMAND tests/ssl-opt.sh
        )

        ADD_CUSTOM_TARGET(lcov
            COMMAND rm -rf Coverage
            COMMAND lcov --capture --initial --directory library/CMakeFiles/mbedtls.dir -o files.info
            COMMAND lcov --capture --directory library/CMakeFiles/mbedtls.dir -o tests.info
            COMMAND lcov --add-tracefile files.info --add-tracefile tests.info -o all.info
            COMMAND lcov --remove all.info -o final.info '*.h'
            COMMAND gendesc tests/Descriptions.txt -o descriptions
            COMMAND genhtml --title "mbed TLS" --description-file descriptions --keep-descriptions --legend --no-branch-coverage -o Coverage final.info
            COMMAND rm -f files.info tests.info all.info final.info descriptions
        )

        ADD_CUSTOM_TARGET(memcheck
            COMMAND sed -i.bak s+/usr/bin/valgrind+`which valgrind`+ DartConfiguration.tcl
            COMMAND ctest -O memcheck.log -D ExperimentalMemCheck
            COMMAND tail -n1 memcheck.log | grep 'Memory checking results:' > /dev/null
            COMMAND rm -f memcheck.log
            COMMAND mv DartConfiguration.tcl.bak DartConfiguration.tcl
        )
    endif(UNIX)

    # Make scripts needed for testing available in an out-of-source build.
    if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        link_to_source(scripts)
        # Copy (don't link) DartConfiguration.tcl, needed for memcheck, to
        # keep things simple with the sed commands in the memcheck target.
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/DartConfiguration.tcl
                    ${CMAKE_CURRENT_BINARY_DIR}/DartConfiguration.tcl COPYONLY)
    endif()
endif()

if(NOT DISABLE_PACKAGE_CONFIG_AND_INSTALL)
    configure_package_config_file(
        "cmake/MbedTLSConfig.cmake.in"
        "cmake/MbedTLSConfig.cmake"
            INSTALL_DESTINATION "cmake")

    write_basic_package_version_file(
        "cmake/MbedTLSConfigVersion.cmake"
            COMPATIBILITY SameMajorVersion
            VERSION 3.2.1)

    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/MbedTLSConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/cmake/MbedTLSConfigVersion.cmake"
        DESTINATION "cmake")

    export(
        EXPORT MbedTLSTargets
        NAMESPACE MbedTLS::
        FILE "cmake/MbedTLSTargets.cmake")

    install(
        EXPORT MbedTLSTargets
        NAMESPACE MbedTLS::
        DESTINATION "cmake"
        FILE "MbedTLSTargets.cmake")

    if(CMAKE_VERSION VERSION_GREATER 3.15 OR CMAKE_VERSION VERSION_EQUAL 3.15)
        # Do not export the package by default
        cmake_policy(SET CMP0090 NEW)

        # Make this package visible to the system
        export(PACKAGE MbedTLS)
    endif()
endif()
