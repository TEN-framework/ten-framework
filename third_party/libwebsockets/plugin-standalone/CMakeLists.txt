cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)

if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

# This shows one way to build a standalone plugin
# outside of lws itself

project(lws-protocol-plugin-example C)

set(PACKAGE "lws-protocol-plugin-example")
set(CPACK_PACKAGE_NAME "${PACKAGE}")
set(CPACK_PACKAGE_VERSION "0.1")
set(CPACK_PACKAGE_VENDOR "andy@warmcat.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PACKAGE} ${PACKAGE_VERSION}")
set(SOVERSION "1")
set(VERSION "0.1")

set(PLUGIN_NAME "protocol_example_standalone")
# space-separated list of sources
set(PLUGIN_SRCS protocol_example_standalone.c)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

message(STATUS "CMAKE_TOOLCHAIN_FILE='${CMAKE_TOOLCHAIN_FILE}'")

# Try to find the current Git hash.
find_package(Git)
if(GIT_EXECUTABLE)
	execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "${GIT_EXECUTABLE}" describe
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "whoami"
    OUTPUT_VARIABLE GIT_USER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "hostname"
    OUTPUT_VARIABLE GIT_HOST
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	string(REGEX REPLACE "([^\\])[\\]([^\\])" "\\1\\\\\\\\\\2" GIT_USER ${GIT_USER})
    set(LWS_BUILD_HASH ${GIT_USER}@${GIT_HOST}-${GIT_HASH})
    message("Git commit hash: ${LWS_BUILD_HASH}")
endif()

set(CMAKE_C_FLAGS "-fPIC ${CMAKE_C_FLAGS}")

source_group("Headers Private"   FILES ${PLUGIN_HDR})
source_group("Sources"   FILES ${PLUGIN_SRCS})
add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SRCS} ${PLUGIN_HDR})
target_compile_definitions(${PLUGIN_NAME} PRIVATE LWS_BUILDING_SHARED)

target_link_libraries(${PLUGIN_NAME} -lwebsockets ${LIBWEBSOCKETS_DEP_LIBS})

# Set test app specific defines.
set_property(TARGET ${PLUGIN_NAME}
	     PROPERTY COMPILE_DEFINITIONS
	     INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/plugins"
)

list(APPEND PLUGINS_LIST ${PLUGIN_NAME})

install(TARGETS ${PLUGINS_LIST}
	PERMISSIONS  OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/libwebsockets-test-server/plugins
	COMPONENT plugins)
