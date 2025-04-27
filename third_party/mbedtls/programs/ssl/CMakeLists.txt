set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)

set(libs
    ${mbedtls_target}
)

set(executables
    dtls_client
    dtls_server
    mini_client
    ssl_client1
    ssl_client2
    ssl_context_info
    ssl_fork_server
    ssl_mail_client
    ssl_server
    ssl_server2
)

if(GEN_FILES)
    # Inform CMake that the following file will be generated as part of the build
    # process, so it doesn't complain that it doesn't exist yet. Starting from
    # CMake 3.20, this will no longer be necessary as CMake will automatically
    # propagate this information across the tree, for now it's only visible
    # inside the same directory, so we need to propagate manually.
    set_source_files_properties(
        ${CMAKE_CURRENT_BINARY_DIR}/../test/query_config.c
        PROPERTIES GENERATED TRUE)
endif()

foreach(exe IN LISTS executables)
    set(extra_sources "")
    if(exe STREQUAL "ssl_client2" OR exe STREQUAL "ssl_server2")
        list(APPEND extra_sources
            ssl_test_lib.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../test/query_config.h
            ${CMAKE_CURRENT_BINARY_DIR}/../test/query_config.c)
    endif()
    add_executable(${exe} ${exe}.c $<TARGET_OBJECTS:mbedtls_test>
        ${extra_sources})
    target_link_libraries(${exe} ${libs})
    target_include_directories(${exe} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/include)
    if(exe STREQUAL "ssl_client2" OR exe STREQUAL "ssl_server2")
        if(GEN_FILES)
            add_dependencies(${exe} generate_query_config_c)
        endif()
        target_include_directories(${exe}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../test)
    endif()
endforeach()

if(THREADS_FOUND)
    add_executable(ssl_pthread_server ssl_pthread_server.c $<TARGET_OBJECTS:mbedtls_test>)
    target_include_directories(ssl_pthread_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/include)
    target_link_libraries(ssl_pthread_server ${libs} ${CMAKE_THREAD_LIBS_INIT})
    list(APPEND executables ssl_pthread_server)
endif(THREADS_FOUND)

install(TARGETS ${executables}
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
