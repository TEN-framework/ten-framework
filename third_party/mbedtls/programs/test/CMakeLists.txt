set(libs
    ${mbedtls_target}
)

set(executables_libs
    selftest
    udp_proxy
)

set(executables_mbedcrypto
    benchmark
    query_compile_time_config
    zeroize
)

if(TEST_CPP)
    set(cpp_dummy_build_cpp "${CMAKE_CURRENT_BINARY_DIR}/cpp_dummy_build.cpp")
    set(generate_cpp_dummy_build "${CMAKE_CURRENT_SOURCE_DIR}/generate_cpp_dummy_build.sh")
    add_custom_command(
        OUTPUT "${cpp_dummy_build_cpp}"
        COMMAND "${generate_cpp_dummy_build}" "${cpp_dummy_build_cpp}"
        DEPENDS "${generate_cpp_dummy_build}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_executable(cpp_dummy_build "${cpp_dummy_build_cpp}")
    target_include_directories(cpp_dummy_build PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(cpp_dummy_build ${mbedcrypto_target})
endif()

if(USE_SHARED_MBEDTLS_LIBRARY AND
   NOT ${CMAKE_SYSTEM_NAME} MATCHES "[Ww][Ii][Nn]")
    add_executable(dlopen "dlopen.c")
    target_include_directories(dlopen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(dlopen ${CMAKE_DL_LIBS})
endif()

if(GEN_FILES)
    find_package(Perl REQUIRED)

    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/query_config.c
        COMMAND
            ${PERL}
                ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/generate_query_config.pl
                ${CMAKE_CURRENT_SOURCE_DIR}/../../include/mbedtls/mbedtls_config.h
                ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/data_files/query_config.fmt
                ${CMAKE_CURRENT_BINARY_DIR}/query_config.c
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/generate_query_config.pl
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include/mbedtls/mbedtls_config.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/data_files/query_config.fmt
    )
    # this file will also be used in another directory, so create a target, see
    # https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#how-can-i-add-a-dependency-to-a-source-file-which-is-generated-in-a-subdirectory
    add_custom_target(generate_query_config_c
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/query_config.c)
else()
    link_to_source(query_config.c)
endif()

foreach(exe IN LISTS executables_libs executables_mbedcrypto)
    set(extra_sources "")
    if(exe STREQUAL "query_compile_time_config")
        list(APPEND extra_sources
            ${CMAKE_CURRENT_SOURCE_DIR}/query_config.h
            ${CMAKE_CURRENT_BINARY_DIR}/query_config.c)
    endif()
    add_executable(${exe} ${exe}.c $<TARGET_OBJECTS:mbedtls_test>
        ${extra_sources})
    target_include_directories(${exe} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/include)
    if(exe STREQUAL "query_compile_time_config")
        target_include_directories(${exe} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    # This emulates "if ( ... IN_LIST ... )" which becomes available in CMake 3.3
    list(FIND executables_libs ${exe} exe_index)
    if (${exe_index} GREATER -1)
        target_link_libraries(${exe} ${libs})
    else()
        target_link_libraries(${exe} ${mbedcrypto_target})
    endif()
endforeach()

install(TARGETS ${executables_libs} ${executables_mbedcrypto}
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
