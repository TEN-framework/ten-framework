cmake_minimum_required (VERSION 3.5)
project (example)

if(EXAMPLE_MSGPACK_EMBEDDED)
    add_subdirectory(msgpack-c)
    set(msgpack-c_DIR ${CMAKE_CURRENT_BINARY_DIR}/msgpack-c)
endif()

find_package(msgpack-c REQUIRED)

add_executable (${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/../simple_c.c)
target_link_libraries(${PROJECT_NAME} msgpack-c)

if(TARGET msgpack-c-static)
    add_executable (${PROJECT_NAME}-static ${CMAKE_CURRENT_LIST_DIR}/../simple_c.c)
    target_link_libraries(${PROJECT_NAME}-static msgpack-c-static)
endif()
