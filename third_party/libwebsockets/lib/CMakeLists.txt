#
# libwebsockets - small server side websockets and web server implementation
#
# Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

include_directories(.)

macro(add_subdir_include_dirs arg1)
	add_subdirectory(${arg1})
	list(APPEND LWS_LIB_BUILD_INC_PATHS ${_CMAKE_INC_LIST}) 
endmacro()

set(LWS_LIB_INCLUDES "")

#
# Plat specific build items
#

if (LWS_PLAT_FREERTOS)
	add_subdir_include_dirs(plat/freertos)
	if (ESP_PLATFORM)
		list(APPEND LWS_ESP_IDF_DIRS
		    $ENV{IDF_PATH}/components/freertos/include
		    $ENV{IDF_PATH}/components/esp_hw_support/include/soc/
		    $ENV{IDF_PATH}/components/esp_common/include
		    $ENV{IDF_PATH}/components/esp_timer/include
		    $ENV{IDF_PATH}/components/soc/include
		    $ENV{IDF_PATH}/components/soc/src/esp32/include
		    $ENV{IDF_PATH}/components/lwip/port/esp32/include
		    $ENV{IDF_PATH}/components/lwip/lwip/src/include
		    $ENV{IDF_PATH}/components/lwip/port/esp32/include
		    ${CMAKE_BINARY_DIR}/config
		    $ENV{IDF_PATH}/components/esp_rom/include
		    $ENV{IDF_PATH}/components/esp_system/include
		    $ENV{IDF_PATH}/components/lwip/include/apps/sntp
		    $ENV{IDF_PATH}/components/soc/soc/esp32/include
		    $ENV{IDF_PATH}/components/heap/include
		    $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
		    $ENV{IDF_PATH}/components/mbedtls/port/include
		    $ENV{IDF_PATH}/components/esp_wifi/include
		    $ENV{IDF_PATH}/components/esp_event/include
		    $ENV{IDF_PATH}/components/esp_netif/include
		    $ENV{IDF_PATH}/components/esp_eth/include
		    $ENV{IDF_PATH}/components/driver/include
		    $ENV{IDF_PATH}/components/soc/soc/include
		    $ENV{IDF_PATH}/components/tcpip_adapter/include
		    $ENV{IDF_PATH}/components/lwip/include/apps
		    $ENV{IDF_PATH}/components/nvs_flash/include
		    $ENV{IDF_PATH}/components/esp32/include
		    $ENV{IDF_PATH}/components/spi_flash/include
		    $ENV{IDF_PATH}/components/mdns/include
		    $ENV{IDF_PATH}/components/lwip/lwip/src/include/lwip
		    $ENV{IDF_PATH}/components/lwip/lwip/src/include
		    $ENV{IDF_PATH}/components/lwip/lwip/src/include/lwip
		    $ENV{IDF_PATH}/components/newlib/platform_include )

		    include_directories(${LWS_ESP_IDF_DIRS})

		    list(APPEND CMAKE_REQUIRED_INCLUDES ${LWS_ESP_IDF_DIRS})
	endif()

	
else()
	if (LWS_PLAT_OPTEE)
		add_subdir_include_dirs(plat/optee)
	else()
		if (WIN32)
			add_subdir_include_dirs(plat/windows)
		else()
			add_subdir_include_dirs(plat/unix)
		endif()
	endif()
endif()

if (LIB_LIST)
	set(CMAKE_REQUIRED_LIBRARIES ${LIB_LIST} ${CMAKE_REQUIRED_LIBRARIES})
endif()

if (LWS_WITH_ZLIB)
	if (LWS_WITH_BUNDLED_ZLIB)
		if (WIN32)
			# it's trying to delete internal zlib entry
			LIST(REMOVE_AT CMAKE_REQUIRED_LIBRARIES 0 )
		endif()
	endif()
endif()


# ideally we want to use pipe2()

CHECK_C_SOURCE_COMPILES("
	#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
	#endif
	#include <unistd.h>
	int main(void) {
		int fd[2];
		return pipe2(fd, 0);
	}" LWS_HAVE_PIPE2)

# tcp keepalive needs this on linux to work practically... but it only exists
# after kernel 2.6.37

CHECK_C_SOURCE_COMPILES("#include <netinet/tcp.h>\nint main(void) { return TCP_USER_TIMEOUT; }\n" LWS_HAVE_TCP_USER_TIMEOUT)
set(LWS_PUBLIC_INCLUDES "")
if (LWS_WITH_TLS)
	add_subdir_include_dirs(tls)
endif()

# Generate the lws_config.h that includes all the private compilation settings.
configure_file(
       "${PROJECT_SOURCE_DIR}/cmake/lws_config_private.h.in"
       "${PROJECT_BINARY_DIR}/lws_config_private.h")

add_subdir_include_dirs(core)
add_subdir_include_dirs(misc)
add_subdir_include_dirs(system)

if (LWS_WITH_DRIVERS)
	add_subdir_include_dirs(drivers)
endif()

if (LWS_WITH_NETWORK)
	add_subdir_include_dirs(core-net)
	if (LWS_WITH_ABSTRACT)
		add_subdir_include_dirs(abstract)
	endif()
	add_subdir_include_dirs(roles)
endif()

if (LWS_WITH_JOSE)
	add_subdir_include_dirs(jose)
endif()
if (LWS_WITH_COSE)
	add_subdir_include_dirs(cose)
endif()


if (LWS_WITH_SECURE_STREAMS)
	add_subdir_include_dirs(secure-streams)
endif()

add_subdir_include_dirs(event-libs)

if (LWS_WITH_STATIC)
    if (LWS_STATIC_PIC)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()

	add_library(websockets STATIC ${SOURCES})# ${HDR_PUBLIC})
	set_target_properties(websockets PROPERTIES LINKER_LANGUAGE C)
	list(APPEND LWS_LIBRARIES websockets)
	target_include_directories(websockets INTERFACE
		$<INSTALL_INTERFACE:${LWS_INSTALL_INCLUDE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
	)
	target_include_directories(websockets PRIVATE ${LWS_LIB_BUILD_INC_PATHS})
	target_compile_definitions(websockets PRIVATE LWS_BUILDING_STATIC)
	target_include_directories(websockets PUBLIC ${LWS_PUBLIC_INCLUDES})
	set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} PARENT_SCOPE)

	if (WIN32)
		# Windows uses the same .lib ending for static libraries and shared
		# library linker files, so rename the static library.
		set_target_properties(websockets
			PROPERTIES
			OUTPUT_NAME websockets_static)
	endif()
	
endif()

if (LWS_WITH_SHARED)
	if (NOT RESOURCES)
		set(RESOURCES "")
	endif()
	
	add_library(websockets_shared SHARED ${SOURCES} ${RESOURCES})# ${HDR_PUBLIC})
	set_target_properties(websockets_shared PROPERTIES LINKER_LANGUAGE C)
	list(APPEND LWS_LIBRARIES websockets_shared)
	target_include_directories(websockets_shared INTERFACE
		$<INSTALL_INTERFACE:${LWS_INSTALL_INCLUDE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
	)
	target_include_directories(websockets_shared PRIVATE ${LWS_LIB_BUILD_INC_PATHS})
	target_compile_definitions(websockets_shared PRIVATE LWS_BUILDING_SHARED)
	target_include_directories(websockets_shared PUBLIC ${LWS_PUBLIC_INCLUDES})
	set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} PARENT_SCOPE)

	# We want the shared lib to be named "libwebsockets"
	# not "libwebsocket_shared".
	set_target_properties(websockets_shared
		PROPERTIES
		OUTPUT_NAME websockets)

	if (WIN32)
		# Compile as DLL (export function declarations)
		set_property(
			TARGET websockets_shared
			PROPERTY COMPILE_DEFINITIONS
			LWS_DLL
			LWS_INTERNAL)
	endif()

	if (APPLE)
		set_property(TARGET websockets_shared PROPERTY MACOSX_RPATH YES)
	endif()

	if (UNIX AND LWS_WITH_PLUGINS_API)
		set (CMAKE_POSITION_INDEPENDENT_CODE ON)
		if (NOT((${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD") OR
		   (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD") OR
		   (${CMAKE_SYSTEM_NAME} MATCHES "QNX")))
		   if (LWS_WITH_SHARED)
				target_link_libraries(websockets_shared dl)
			endif()
		endif()
	endif()

endif()

#
# expose the library private include dirs to plugins, test apps etc that are
# part of the lib build but different targets 
#

if (LWS_WITH_SHARED)
	get_target_property(LWS_LIB_INCLUDES websockets_shared INCLUDE_DIRECTORIES)
else()
	get_target_property(LWS_LIB_INCLUDES websockets INCLUDE_DIRECTORIES)
endif()


# Set the so version of the lib.
# Equivalent to LDFLAGS=-version-info x:x:x

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR COMPILER_IS_CLANG)
	foreach(lib ${LWS_LIBRARIES})
		set_target_properties(${lib}
			PROPERTIES
			SOVERSION ${SOVERSION})
	endforeach()
endif()


# Setup the linking for all libs.
foreach (lib ${LWS_LIBRARIES})
	target_link_libraries(${lib} ${LIB_LIST})
endforeach()

#
# These will be available to parent projects including libwebsockets
# using add_subdirectory()
#
set(LIBWEBSOCKETS_LIBRARIES ${LWS_LIBRARIES} CACHE STRING "Libwebsocket libraries")
if (LWS_WITH_STATIC)
	set(LIBWEBSOCKETS_LIBRARIES_STATIC websockets CACHE STRING "Libwebsocket static library")
endif()
if (LWS_WITH_SHARED)
	set(LIBWEBSOCKETS_LIBRARIES_SHARED websockets_shared CACHE STRING "Libwebsocket shared library")
endif()

# Install libs and headers.
install(TARGETS ${LWS_LIBRARIES}
		EXPORT LibwebsocketsTargets
		LIBRARY DESTINATION "${LWS_INSTALL_LIB_DIR}${LIB_SUFFIX}" COMPONENT core
		ARCHIVE DESTINATION "${LWS_INSTALL_LIB_DIR}${LIB_SUFFIX}" COMPONENT core
		RUNTIME DESTINATION "${LWS_INSTALL_BIN_DIR}" COMPONENT core # Windows DLLs
		PUBLIC_HEADER DESTINATION "${LWS_INSTALL_INCLUDE_DIR}" COMPONENT dev)
		
	#set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "Libraries" PARENT_SCOPE)
set(CPACK_COMPONENT_DEV_DISPLAY_NAME "Development files" PARENT_SCOPE)


if (UNIX OR MINGW)

# figure out pkfcfg required libs here

set(lws_requires "")
if (LWS_HAVE_LIBCAP)
	if (NOT lws_requires STREQUAL "")
		set(lws_requires "${lws_requires},libcap")
	else()
		set(lws_requires "libcap")
	endif()
endif()


# Generate and install pkgconfig.
# (This is not indented, because the tabs will be part of the output)
file(WRITE "${PROJECT_BINARY_DIR}/libwebsockets.pc"
"prefix=\"${CMAKE_INSTALL_PREFIX}\"
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib${LIB_SUFFIX}
includedir=\${prefix}/include

Name: libwebsockets
Description: Websockets server and client library
Version: ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}

Libs: -L\${libdir} -lwebsockets
Cflags: -I\${includedir}
"
)
if (NOT ${lws_requires} STREQUAL "")
        file(APPEND "${PROJECT_BINARY_DIR}/libwebsockets.pc" "Requires: ${lws_requires}")
endif()


	install(FILES "${PROJECT_BINARY_DIR}/libwebsockets.pc"
		DESTINATION lib${LIB_SUFFIX}/pkgconfig)

file(WRITE "${PROJECT_BINARY_DIR}/libwebsockets_static.pc"
"prefix=\"${CMAKE_INSTALL_PREFIX}\"
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib${LIB_SUFFIX}
includedir=\${prefix}/include

Name: libwebsockets_static
Description: Websockets server and client static library
Version: ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}

Libs: -L\${libdir} -l:libwebsockets${CMAKE_STATIC_LIBRARY_SUFFIX}
Libs.private:
Cflags: -I\${includedir}
"
)

if (NOT ${lws_requires} STREQUAL "")
        file(APPEND "${PROJECT_BINARY_DIR}/libwebsockets_static.pc" "Requires: ${lws_requires}")
endif()


	install(FILES "${PROJECT_BINARY_DIR}/libwebsockets_static.pc"
		DESTINATION lib${LIB_SUFFIX}/pkgconfig)

endif(UNIX OR MINGW)


# Keep explicit parent scope exports at end
#

export_to_parent_intermediate()
if (DEFINED LWS_PLAT_UNIX)
	set(LWS_PLAT_UNIX ${LWS_PLAT_UNIX} PARENT_SCOPE)
	if (ILLUMOS)
		add_definitions("-D__illumos__")
	endif()
endif()
set(LWS_HAVE_MBEDTLS_NET_SOCKETS ${LWS_HAVE_MBEDTLS_NET_SOCKETS} PARENT_SCOPE)
set(TEST_SERVER_SSL_KEY "${TEST_SERVER_SSL_KEY}" PARENT_SCOPE)
set(TEST_SERVER_SSL_CERT "${TEST_SERVER_SSL_CERT}" PARENT_SCOPE)
set(TEST_SERVER_DATA ${TEST_SERVER_DATA} PARENT_SCOPE)
set(LWS_HAVE_PIPE2 ${LWS_HAVE_PIPE2} PARENT_SCOPE)
set(LWS_LIBRARIES ${LWS_LIBRARIES} PARENT_SCOPE)
if (DEFINED WIN32_HELPERS_PATH)
	set(WIN32_HELPERS_PATH ${WIN32_HELPERS_PATH} PARENT_SCOPE)
endif()
if (DEFINED HDR_PRIVATE)
set(HDR_PRIVATE ${HDR_PRIVATE} PARENT_SCOPE)
endif()
if (DEFINED ZLIB_FOUND)
	set(ZLIB_FOUND ${ZLIB_FOUND} PARENT_SCOPE)
endif()
if (DEFINED LIB_LIST_AT_END)
set(LIB_LIST_AT_END ${LIB_LIST_AT_END} PARENT_SCOPE)
endif()
set(USE_WOLFSSL ${USE_WOLFSSL} PARENT_SCOPE)
set(LWS_DEPS_LIB_PATHS ${LWS_DEPS_LIB_PATHS} PARENT_SCOPE)

