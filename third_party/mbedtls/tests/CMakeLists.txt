set(libs
    ${mbedtls_target}
)

# Set the project root directory if it's not already defined, as may happen if
# the tests folder is included directly by a parent project, without including
# the top level CMakeLists.txt.
if(NOT DEFINED MBEDTLS_DIR)
    set(MBEDTLS_DIR ${CMAKE_SOURCE_DIR})
endif()

if(NOT MBEDTLS_PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Cannot build test suites without Python 3")
endif()

# generated .data files will go there
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/suites)

# Get base names for generated files (starting at "suites/")
execute_process(
    COMMAND
        ${MBEDTLS_PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/generate_psa_tests.py
        --list-for-cmake
        --directory suites
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE
        base_generated_data_files)

# Derive generated file paths in the build directory
set(generated_data_files "")
foreach(file ${base_generated_data_files})
    list(APPEND generated_data_files ${CMAKE_CURRENT_BINARY_DIR}/${file})
endforeach()

if(GEN_FILES)
    add_custom_command(
        OUTPUT
            ${generated_data_files}
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/..
        COMMAND
            ${MBEDTLS_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/generate_psa_tests.py
            --directory ${CMAKE_CURRENT_BINARY_DIR}/suites
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/generate_psa_tests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/mbedtls_dev/crypto_knowledge.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/mbedtls_dev/macro_collector.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/mbedtls_dev/psa_storage.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/mbedtls_dev/test_case.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/psa/crypto_config.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/psa/crypto_values.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/psa/crypto_extra.h
    )

else()
    foreach(file ${base_generated_data_files})
        link_to_source(${file})
    endforeach()
endif()
# CMake generates sub-makefiles for each target and calls them in subprocesses.
# Without this command, cmake will generate rules in each sub-makefile. As a result,
# they can cause race conditions in parallel builds.
# With this line, only 4 sub-makefiles include the above command, that reduces
# the risk of a race.
add_custom_target(test_suite_generated_data DEPENDS ${generated_data_files})
# Test suites caught by SKIP_TEST_SUITES are built but not executed.
# "foo" as a skip pattern skips "test_suite_foo" and "test_suite_foo.bar"
# but not "test_suite_foobar".
string(REGEX REPLACE "[ ,;]" "|" SKIP_TEST_SUITES_REGEX "${SKIP_TEST_SUITES}")
string(REPLACE "." "\\." SKIP_TEST_SUITES_REGEX "${SKIP_TEST_SUITES_REGEX}")
set(SKIP_TEST_SUITES_REGEX "^(${SKIP_TEST_SUITES_REGEX})(\$|\\.)")

function(add_test_suite suite_name)
    if(ARGV1)
        set(data_name ${ARGV1})
    else()
        set(data_name ${suite_name})
    endif()

    # Get the test names of the tests with generated .data files
    # from the generated_data_files list in parent scope.
    set(generated_data_names "")
    foreach(generated_data_file ${generated_data_files})
        # Get the plain filename
        get_filename_component(generated_data_name ${generated_data_file} NAME)
        # Remove the ".data" extension
        get_name_without_last_ext(generated_data_name ${generated_data_name})
        # Remove leading "test_suite_"
        string(SUBSTRING ${generated_data_name} 11 -1 generated_data_name)
        list(APPEND generated_data_names ${generated_data_name})
    endforeach()

    if(";${generated_data_names};" MATCHES ";${data_name};")
        set(data_file
            ${CMAKE_CURRENT_BINARY_DIR}/suites/test_suite_${data_name}.data)
    else()
        set(data_file
            ${CMAKE_CURRENT_SOURCE_DIR}/suites/test_suite_${data_name}.data)
    endif()

    add_custom_command(
        OUTPUT
            # The output filename of generate_test_code.py is derived from the -d
            # input argument.
            test_suite_${data_name}.c
        COMMAND
            ${MBEDTLS_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_code.py
            -f ${CMAKE_CURRENT_SOURCE_DIR}/suites/test_suite_${suite_name}.function
            -d ${data_file}
            -t ${CMAKE_CURRENT_SOURCE_DIR}/suites/main_test.function
            -p ${CMAKE_CURRENT_SOURCE_DIR}/suites/host_test.function
            -s ${CMAKE_CURRENT_SOURCE_DIR}/suites
            --helpers-file ${CMAKE_CURRENT_SOURCE_DIR}/suites/helpers.function
            -o .
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_code.py
            ${CMAKE_CURRENT_SOURCE_DIR}/suites/test_suite_${suite_name}.function
            ${data_file}
            ${CMAKE_CURRENT_SOURCE_DIR}/suites/main_test.function
            ${CMAKE_CURRENT_SOURCE_DIR}/suites/host_test.function
            ${CMAKE_CURRENT_SOURCE_DIR}/suites/helpers.function
            ${mbedtls_target}
        BYPRODUCTS
            test_suite_${data_name}.datax
    )

    add_executable(test_suite_${data_name} test_suite_${data_name}.c $<TARGET_OBJECTS:mbedtls_test>)
    add_dependencies(test_suite_${data_name} test_suite_generated_data)
    target_link_libraries(test_suite_${data_name} ${libs})
    # Include test-specific header files from ./include and private header
    # files (used by some invasive tests) from ../library. Public header
    # files are automatically included because the library targets declare
    # them as PUBLIC.
    target_include_directories(test_suite_${data_name}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../library)

    if(${data_name} MATCHES ${SKIP_TEST_SUITES_REGEX})
        message(STATUS "The test suite ${data_name} will not be executed.")
    else()
        add_test(${data_name}-suite test_suite_${data_name} --verbose)
    endif()
endfunction(add_test_suite)

# Enable definition of various functions used throughout the testsuite
# (gethostname, strdup, fileno...) even when compiling with -std=c99. Harmless
# on non-POSIX platforms.
add_definitions("-D_POSIX_C_SOURCE=200809L")

if(CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdocumentation -Wno-documentation-deprecated-sync -Wunreachable-code")
endif(CMAKE_COMPILER_IS_CLANG)

if(MSVC)
    # If a warning level has been defined, suppress all warnings for test code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX-")
endif(MSVC)

file(GLOB test_suites RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" suites/*.data)
list(APPEND test_suites ${base_generated_data_files})
# If the generated .data files are present in the source tree, we just added
# them twice, both through GLOB and through ${base_generated_data_files}.
list(REMOVE_DUPLICATES test_suites)
list(SORT test_suites)
foreach(test_suite ${test_suites})
    get_filename_component(data_name ${test_suite} NAME)
    string(REGEX REPLACE "\\.data\$" "" data_name "${data_name}")
    string(REPLACE "test_suite_" "" data_name "${data_name}")
    string(REGEX MATCH "[^.]*" function_name "${data_name}")
    add_test_suite(${function_name} ${data_name})
endforeach(test_suite)

# Make scripts and data files needed for testing available in an
# out-of-source build.
if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/seedfile")
        link_to_source(seedfile)
    endif()
    link_to_source(compat.sh)
    link_to_source(context-info.sh)
    link_to_source(data_files)
    link_to_source(scripts)
    link_to_source(ssl-opt.sh)
    link_to_source(opt-testcases)
endif()
