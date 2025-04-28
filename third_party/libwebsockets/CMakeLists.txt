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

cmake_minimum_required(VERSION 2.8.12)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)

if (POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif()

#if (POLICY CMP0024)
#	cmake_policy(SET CMP0024 NEW)
#endif()

if (POLICY CMP0075)
	cmake_policy(SET CMP0075 NEW)
endif()

# General Advice
#
# For selecting between DEBUG / RELEASE, use -DCMAKE_BUILD_TYPE=DEBUG or =RELEASE
#   debug builds include source level debug info and extra logging

set(LWS_WITH_BUNDLED_ZLIB_DEFAULT OFF)
if(WIN32)
	set(LWS_WITH_BUNDLED_ZLIB_DEFAULT ON)
endif()

set(LWS_ROLE_RAW 1)
set(LWS_WITH_POLL 1)

if (ESP_PLATFORM)
	set(LWS_ESP_PLATFORM 1)
	#set(CMAKE_TOOLCHAIN_FILE contrib/cross-esp32.cmake)
	set(LWIP_PROVIDE_ERRNO 1)
endif()

# it's at this point any toolchain file is brought in
project(libwebsockets C)
if (LWS_WITH_SECURE_STREAMS_CPP)
	enable_language(CXX)
endif()
include(CTest)

if (ESP_PLATFORM)
       include_directories(
		$ENV{IDF_PATH}/components/esp_hw_support/include/soc/
		$ENV{IDF_PATH}/components/freertos/include/
		$ENV{IDF_PATH}/components/freertos/esp_additions/include/
		$ENV{IDF_PATH}/components/freertos/esp_additions/include/freertos/
		$ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/
		$ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/linux/include/
		$ENV{IDF_PATH}/components/xtensa/${CONFIG_IDF_TARGET}/include/
		$ENV{IDF_PATH}/components/freertos/include/esp_additions
		$ENV{IDF_PATH}/components/hal/include
		$ENV{IDF_PATH}/components/soc/${CONFIG_IDF_TARGET}/include/
		$ENV{IDF_PATH}/components/soc/include/
		$ENV{IDF_PATH}/components/esp_hw_support/include
		$ENV{IDF_PATH}/components/hal/${CONFIG_IDF_TARGET}/include/
	)

	if (CONFIG_IDF_TARGET_ARCH_RISCV)
                include_directories(
                       $ENV{IDF_PATH}/components/freertos/port/riscv/include
                       $ENV{IDF_PATH}/components/riscv/include)
        else()
                include_directories(
                       $ENV{IDF_PATH}/components/freertos/port/xtensa/include
                       $ENV{IDF_PATH}/components/xtensa/include)
        endif()

endif()


#
# Select features recommended for PC distro packaging
#
option(LWS_WITH_DISTRO_RECOMMENDED "Enable features recommended for distro packaging" OFF)
option(LWS_FOR_GITOHASHI "Enable features recommended for use with gitohashi" OFF)

#
# Compiler features
#
option(DISABLE_WERROR "Avoid treating compiler warnings as fatal errors" OFF)

#
# Major individual features
#
option(LWS_WITH_NETWORK "Compile with network-related code" ON)
option(LWS_ROLE_H1 "Compile with support for http/1 (needed for ws)" ON)
option(LWS_ROLE_WS "Compile with support for websockets" ON)
option(LWS_ROLE_MQTT "Build with support for MQTT client" OFF)
option(LWS_ROLE_DBUS "Compile with support for DBUS" OFF)
option(LWS_ROLE_RAW_PROXY "Raw packet proxy" OFF)
option(LWS_ROLE_RAW_FILE "Compile with support for raw files" ON)
option(LWS_WITH_HTTP2 "Compile with server support for HTTP/2" ON)
option(LWS_WITH_LWSWS "Libwebsockets Webserver" OFF)
option(LWS_WITH_CGI "Include CGI (spawn process with network-connected stdin/out/err) APIs" OFF)
option(LWS_IPV6 "Compile with support for ipv6" OFF)
option(LWS_UNIX_SOCK "Compile with support for UNIX domain socket if OS supports it" ON)
option(LWS_WITH_PLUGINS "Support plugins for protocols and extensions (implies LWS_WITH_PLUGINS_API)" OFF)
option(LWS_WITH_PLUGINS_BUILTIN "Build the plugin protocols directly into lws library" OFF)
option(LWS_WITH_HTTP_PROXY "Support for active HTTP proxying" OFF)
option(LWS_WITH_ZIP_FOPS "Support serving pre-zipped files" OFF)
option(LWS_WITH_SOCKS5 "Allow use of SOCKS5 proxy on client connections" OFF)
option(LWS_WITH_PEER_LIMITS "Track peers and restrict resources a single peer can allocate" OFF)
option(LWS_WITH_ACCESS_LOG "Support generating Apache-compatible access logs" OFF)
option(LWS_WITH_RANGES "Support http ranges (RFC7233)" OFF)
option(LWS_WITH_THREADPOOL "Managed worker thread pool support (relies on pthreads)" OFF)
option(LWS_WITH_HTTP_STREAM_COMPRESSION "Support HTTP stream compression" OFF)
option(LWS_WITH_HTTP_BROTLI "Also offer brotli http stream compression (requires LWS_WITH_HTTP_STREAM_COMPRESSION)" OFF)
option(LWS_WITH_ACME "Enable support for ACME automatic cert acquisition + maintenance (letsencrypt etc)" OFF)
option(LWS_WITH_HUBBUB "Enable libhubbub rewriting support" OFF)
option(LWS_WITH_ALSA "Enable alsa audio example" OFF)
option(LWS_WITH_GTK "Enable gtk example" OFF)
option(LWS_WITH_FTS "Full Text Search support" OFF)
option(LWS_WITH_SYS_ASYNC_DNS "Nonblocking internal IPv4 + IPv6 DNS resolver" OFF)
option(LWS_WITH_SYS_NTPCLIENT "Build in tiny ntpclient good for tls date validation and run via lws_system" OFF)
option(LWS_WITH_SYS_DHCP_CLIENT "Build in tiny DHCP client" OFF)
option(LWS_WITH_HTTP_BASIC_AUTH "Support Basic Auth" ON)
option(LWS_WITH_HTTP_UNCOMMON_HEADERS "Include less common http header support" ON)
option(LWS_WITH_SYS_STATE "lws_system state support" ON)
option(LWS_WITH_SYS_SMD "Lws System Message Distribution" ON)
option(LWS_WITH_SYS_FAULT_INJECTION "Enable fault injection support" OFF)
option(LWS_WITH_SYS_METRICS "Lws Metrics API" OFF)

#
# Secure Streams
#
option(LWS_WITH_SECURE_STREAMS "Secure Streams protocol-agnostic API" OFF)
option(LWS_WITH_SECURE_STREAMS_CPP "Secure Streams C++ classes" OFF)
option(LWS_WITH_SECURE_STREAMS_PROXY_API "Secure Streams support to work across processes" OFF)
option(LWS_WITH_SECURE_STREAMS_SYS_AUTH_API_AMAZON_COM "Auth support for api.amazon.com" OFF)
option(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY "Secure Streams Policy is hardcoded only" OFF)
option(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4 "Secure Streams Auth support for AWS Sigv4" OFF)
option(LWS_WITH_SECURE_STREAMS_BUFFER_DUMP "Secure Streams protocol buffer dump" OFF)
option(LWS_WITH_SS_DIRECT_PROTOCOL_STR "Secure Streams directly set/get metadata w/o policy" OFF)

#
# CTest options
#
#
# If you build with LWS_WITH_MINIMAL_EXAMPLES, you can use CTest / make test to run
# examples that can give a pass/fail response.  By default it runs tests both against
# a local server peer and warmcat.com, if your CI wants to do the tests but does not
# have internet routing, then you can still run a subset of tests with CTest / make
# test that only does local tests by disabling this option.
#
option(LWS_CTEST_INTERNET_AVAILABLE "CTest will performs tests that need the Internet" ON)

#
# TLS library options... all except mbedTLS are basically OpenSSL variants.
#
option(LWS_WITH_SSL "Include SSL support (defaults to OpenSSL or similar, mbedTLS if LWS_WITH_MBEDTLS is set)" ON)
option(LWS_WITH_MBEDTLS "Use mbedTLS (>=2.0) replacement for OpenSSL. When setting this, you also may need to specify LWS_MBEDTLS_LIBRARIES and LWS_MBEDTLS_INCLUDE_DIRS" OFF)
option(LWS_WITH_BORINGSSL "Use BoringSSL replacement for OpenSSL" OFF)
option(LWS_WITH_CYASSL "Use CyaSSL replacement for OpenSSL. When setting this, you also need to specify LWS_CYASSL_LIBRARIES and LWS_CYASSL_INCLUDE_DIRS" OFF)
option(LWS_WITH_WOLFSSL "Use wolfSSL replacement for OpenSSL. When setting this, you also may need to specify LWS_WOLFSSL_LIBRARIES and LWS_WOLFSSL_INCLUDE_DIRS" OFF)
option(LWS_SSL_CLIENT_USE_OS_CA_CERTS "SSL support should make use of the OS-installed CA root certs" ON)
option(LWS_TLS_LOG_PLAINTEXT_RX "For debugging log the received plaintext as soon as decrypted" OFF)
option(LWS_TLS_LOG_PLAINTEXT_TX "For debugging log the transmitted plaintext just before encryption" OFF)
option(LWS_WITH_TLS_SESSIONS "Enable persistent, resumable TLS sessions" ON)
option(LWS_WITH_TLS_JIT_TRUST "Enable dynamically computing which trusted TLS CA is needed to be instantiated" OFF)

#
# Event library options (may select multiple, or none for default poll()
#
option(LWS_WITH_LIBEV "Compile with support for libev" OFF)
option(LWS_WITH_LIBUV "Compile with support for libuv" OFF)
option(LWS_WITH_LIBEVENT "Compile with support for libevent" OFF)
option(LWS_WITH_GLIB "Compile with support for glib event loop" OFF)
option(LWS_WITH_SDEVENT "Compile with support for sd-event loop" OFF)
option(LWS_WITH_ULOOP "Compile with support for uloop" OFF)

if (UNIX)
# since v4.1, on unix platforms default is build any event libs as runtime plugins
option(LWS_WITH_EVLIB_PLUGINS "Compile event lib support into runtime-selected plugins" ON)
else()
# otherwise default to linking the event lib(s) to libwebsockets.so
option(LWS_WITH_EVLIB_PLUGINS "Compile event lib support into runtime-selected plugins" OFF)
endif()
#
# LWS Drivers
#

option(LWS_WITH_DRIVERS "With generic drivers for gpio, i2c, display etc" OFF)

#
# Static / Dynamic build options
#
option(LWS_WITH_STATIC "Build the static version of the library" ON)
option(LWS_WITH_SHARED "Build the shared version of the library" ON)
option(LWS_LINK_TESTAPPS_DYNAMIC "Link the test apps to the shared version of the library. Default is to link statically" OFF)
option(LWS_STATIC_PIC "Build the static version of the library with position-independent code" OFF)
option(LWS_SUPPRESS_DEPRECATED_API_WARNINGS "Turn off complaints about, eg, openssl 3 deprecated api usage" ON)

#
# Specific platforms
#
option(LWS_WITH_ESP32 "Build for ESP32" OFF)
option(LWS_PLAT_OPTEE "Build for OPTEE" OFF)
option(LWS_PLAT_FREERTOS "Build for FreeRTOS" OFF)
option(LWS_PLAT_ANDROID "Android flavour of unix platform" OFF)

#
# Client / Server / Test Apps build control
#
option(LWS_WITHOUT_CLIENT "Don't build the client part of the library" OFF)
option(LWS_WITHOUT_SERVER "Don't build the server part of the library" OFF)
option(LWS_WITHOUT_TESTAPPS "Don't build the libwebsocket-test-apps" OFF)
option(LWS_WITHOUT_TEST_SERVER "Don't build the test server" OFF)
option(LWS_WITHOUT_TEST_SERVER_EXTPOLL "Don't build the test server version that uses external poll" OFF)
option(LWS_WITHOUT_TEST_PING "Don't build the ping test application" OFF)
option(LWS_WITHOUT_TEST_CLIENT "Don't build the client test application" OFF)
#
# Extensions (permessage-deflate)
#
option(LWS_WITHOUT_EXTENSIONS "Don't compile with extensions" ON)
#
# Helpers + misc
#
option(LWS_WITHOUT_BUILTIN_GETIFADDRS "Don't use the BSD getifaddrs implementation from libwebsockets if it is missing (this will result in a compilation error) ... The default is to assume that your libc provides it. On some systems such as uclibc it doesn't exist." OFF)
option(LWS_FALLBACK_GETHOSTBYNAME "Also try to do dns resolution using gethostbyname if getaddrinfo fails" OFF)
option(LWS_WITHOUT_BUILTIN_SHA1 "Don't build the lws sha-1 (eg, because openssl will provide it" OFF)
option(LWS_WITHOUT_DAEMONIZE "Don't build the daemonization api" ON)
option(LWS_SSL_SERVER_WITH_ECDH_CERT "Include SSL server use ECDH certificate" OFF)
option(LWS_WITH_LEJP "With the Lightweight JSON Parser" ON)
option(LWS_WITH_CBOR "With the Lightweight LECP CBOR Parser" OFF)
option(LWS_WITH_CBOR_FLOAT "Build floating point types if building CBOR LECP" ON)
option(LWS_WITH_SQLITE3 "Require SQLITE3 support" OFF)
option(LWS_WITH_STRUCT_JSON "Generic struct serialization to and from JSON" OFF)
option(LWS_WITH_STRUCT_SQLITE3 "Generic struct serialization to and from SQLITE3" OFF)
# broken atm
#option(LWS_WITH_SMTP "Provide SMTP support" OFF)
if (LWS_WITH_ESP32)
option(LWS_WITH_DIR "Directory scanning api support" OFF)
option(LWS_WITH_LEJP_CONF "With LEJP configuration parser as used by lwsws" OFF)
else()
option(LWS_WITH_DIR "Directory scanning api support" ON)
option(LWS_WITH_LEJP_CONF "With LEJP configuration parser as used by lwsws" ON)
endif()
option(LWS_WITH_NO_LOGS "Disable all logging other than _err and _user from being compiled in" OFF)
set(LWS_LOGGING_BITFIELD_SET 0 CACHE STRING "Bitfield describing which log levels to force included into the build")
set(LWS_LOGGING_BITFIELD_CLEAR 0 CACHE STRING "Bitfield describing which log levels to force removed from the build")
option(LWS_LOGS_TIMESTAMP "Timestamp at start of logs" ON)
option(LWS_LOG_TAG_LIFECYCLE "Log tagged object lifecycle as NOTICE" ON)
option(LWS_AVOID_SIGPIPE_IGN "Android 7+ reportedly needs this" OFF)
option(LWS_WITH_JOSE "JOSE JSON Web Signature / Encryption / Keys (RFC7515/6/) API" OFF)
option(LWS_WITH_COSE "COSE CBOR Signature / Encryption / Keys (RFC8152) API" OFF)
option(LWS_WITH_GENCRYPTO "Enable support for Generic Crypto apis independent of TLS backend" OFF)
option(LWS_WITH_SELFTESTS "Selftests run at context creation" OFF)
option(LWS_WITH_GCOV "Build with gcc gcov coverage instrumentation" OFF)
option(LWS_WITH_EXPORT_LWSTARGETS "Export libwebsockets CMake targets.  Disable if they conflict with an outer cmake project." ON)
option(LWS_REPRODUCIBLE "Build libwebsockets reproducible. It removes the build user and hostname from the build" ON)
option(LWS_WITH_MINIMAL_EXAMPLES "Also build the normally standalone minimal examples, for QA" OFF)
option(LWS_WITH_LWSAC "lwsac Chunk Allocation api" ON)
option(LWS_WITH_CUSTOM_HEADERS "Store and allow querying custom HTTP headers (H1 only)" ON)
option(LWS_WITH_DISKCACHE "Hashed cache directory with lazy LRU deletion to size limit (unrelated to lws_cache_ttl)" OFF)
option(LWS_WITH_ASAN "Build with gcc runtime sanitizer options enabled (needs libasan)" OFF)
option(LWS_WITH_LEJP_CONF "With LEJP configuration parser as used by lwsws" OFF)
option(LWS_WITH_ZLIB "Include zlib support (required for extensions)" OFF)
option(LWS_WITH_BUNDLED_ZLIB "Use bundled zlib version (Windows only)" ${LWS_WITH_BUNDLED_ZLIB_DEFAULT})
option(LWS_WITH_MINIZ "Use miniz instead of zlib" OFF)
option(LWS_WITH_SEQUENCER "lws_seq_t support" OFF)
option(LWS_WITH_EXTERNAL_POLL "Support external POLL integration using callback messages (not recommended)" OFF)
option(LWS_WITH_LWS_DSH "Support lws_dsh_t Disordered Shared Heap" OFF)
option(LWS_CLIENT_HTTP_PROXYING "Support external http proxies for client connections" ON)
option(LWS_WITH_FILE_OPS "Support file operations vfs" ON)
option(LWS_WITH_UDP "Platform supports UDP" ON)
option(LWS_WITH_SPAWN "Spawn subprocesses with piped stdin/out/stderr" OFF)
option(LWS_WITH_FSMOUNT "Overlayfs and fallback mounting apis" OFF)
option(LWS_WITH_FANALYZER "Enable gcc -fanalyzer if compiler supports" OFF)
option(LWS_HTTP_HEADERS_ALL "Override header reduction optimization and include all like older lws versions" OFF)
option(LWS_WITH_SUL_DEBUGGING "Enable zombie lws_sul checking on object deletion" OFF)
option(LWS_WITH_PLUGINS_API "Build generic lws_plugins apis (see LWS_WITH_PLUGINS to also build protocol plugins)" OFF)
option(LWS_WITH_CONMON "Collect introspectable connection latency stats on individual client connections" ON)
option(LWS_WITHOUT_EVENTFD "Force using pipe instead of eventfd" OFF)
if (UNIX OR WIN32)
	option(LWS_WITH_CACHE_NSCOOKIEJAR "Build file-backed lws-cache-ttl that uses netscape cookie jar format (linux-only)" ON)
else()
	option(LWS_WITH_CACHE_NSCOOKIEJAR "Build file-backed lws-cache-ttl that uses netscape cookie jar format (linux-only)" OFF)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	option(LWS_WITH_NETLINK "Monitor Netlink for Routing Table changes" ON)
else()
	set(LWS_WITH_NETLINK 0)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
       # its openssl has md5 deprecated
       set(LWS_SUPPRESS_DEPRECATED_API_WARNINGS 1)
endif()


#
# to use miniz, enable both LWS_WITH_ZLIB and LWS_WITH_MINIZ
#
# End of user settings
#

# sets of sub-options implied by other options
#
set(LIB_LIST "")
set(LIB_LIST_AT_END)
set(LWS_LIBRARIES)
set(LWS_OPENSSL_SUPPORT 0)
include(CMakeLists-implied-options.txt)

#
# Structural helpers for cmake in subdirs
#

macro(add_subdir_include_directories arg1)
	add_subdirectory(${arg1})
	include_directories(${_CMAKE_INC_LIST})
endmacro()

macro(exports_to_parent_scope)
	set(SOURCES ${SOURCES} PARENT_SCOPE)
	if (LIB_LIST)
		set(LIB_LIST ${LIB_LIST} PARENT_SCOPE)
	endif()
	get_property(_CURR DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
	set(_CMAKE_INC_LIST ${_CURR} PARENT_SCOPE)
	if (LWS_LIB_BUILD_INC_PATHS)
		set(LWS_LIB_BUILD_INC_PATHS ${LWS_LIB_BUILD_INC_PATHS} PARENT_SCOPE)
	endif()
endmacro()

macro(export_to_parent_intermediate)
	set(SOURCES ${SOURCES} PARENT_SCOPE)
	if (LIB_LIST)
		set(LIB_LIST ${LIB_LIST} PARENT_SCOPE)
	endif()
	set(_CMAKE_INC_LIST ${_CMAKE_INC_LIST} PARENT_SCOPE)
	if (LWS_LIB_BUILD_INC_PATHS)
		set(LWS_LIB_BUILD_INC_PATHS ${LWS_LIB_BUILD_INC_PATHS} PARENT_SCOPE)
	endif()
endmacro()

#
# Try to find the current Git hash
#

find_package(Git)
if(GIT_EXECUTABLE)
	execute_process(
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMAND "${GIT_EXECUTABLE}" describe --tags --always
		OUTPUT_VARIABLE GIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	set(LWS_BUILD_HASH ${GIT_HASH})

	# append the build user and hostname
	if (NOT LWS_REPRODUCIBLE)
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
	endif()

	message("Git commit hash: ${LWS_BUILD_HASH}")
endif()

if ("${LWS_BUILD_HASH}" STREQUAL "")
	set(LWS_BUILD_HASH "unknown")
endif()

set(PACKAGE "libwebsockets")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_PACKAGE_NAME "${PACKAGE}")
set(CPACK_PACKAGE_VERSION_MAJOR "4")
set(CPACK_PACKAGE_VERSION_MINOR "3")
set(CPACK_PACKAGE_VERSION_PATCH_NUMBER "2")

set(CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH_NUMBER}-${LWS_BUILD_HASH}")
set(CPACK_PACKAGE_RELEASE 1)

set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_VENDOR "andy@warmcat.com")
set(CPACK_PACKAGE_CONTACT "andy@warmcat.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PACKAGE} ${CPACK_PACKAGE_VERSION}")
set(SOVERSION "19")
if(NOT CPACK_GENERATOR)
    if(UNIX)
        set(CPACK_GENERATOR "TGZ")
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
endif()
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(VERSION "${CPACK_PACKAGE_VERSION}")

set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
# below makes path length problems in CI
set(CPACK_RPM_DEBUGINFO_PACKAGE OFF)
# below makes some kind of chimera rpm with binaries and sources
set(CPACK_RPM_PACKAGE_SOURCES OFF)
set(CPACK_RPM_INSTALL_WITH_EXEC ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_PACKAGE_SOURCE ON)
set(CPACK_DEBIAN_COMPONENT_INSTALL ON)


set(LWS_LIBRARY_VERSION ${CPACK_PACKAGE_VERSION})
set(LWS_LIBRARY_VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR})
set(LWS_LIBRARY_VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR})
set(LWS_LIBRARY_VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH_NUMBER})
set(LWS_LIBRARY_VERSION_PATCH_ELABORATED ${CPACK_PACKAGE_VERSION_PATCH})

if (NOT CMAKE_MODULE_PATH)
	set(CMAKE_MODULE_PATH "")
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")


if (CMAKE_TOOLCHAIN_FILE)
	message(STATUS "CMAKE_TOOLCHAIN_FILE='${CMAKE_TOOLCHAIN_FILE}'")
endif()

if (NOT LIB_SUFFIX)
	set(LIB_SUFFIX "")
endif()

if (WIN32)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/win32port/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/win32port/version.rc @ONLY)
	set(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/win32port/version.rc)
endif()

include_directories(include)

# Allow the user to override installation directories.
set(LWS_INSTALL_LIB_DIR       lib CACHE PATH "Installation directory for libraries")
set(LWS_INSTALL_BIN_DIR       bin CACHE PATH "Installation directory for executables")
set(LWS_INSTALL_INCLUDE_DIR   include CACHE PATH "Installation directory for header files")
set(LWS_INSTALL_EXAMPLES_DIR  bin CACHE PATH "Installation directory for example files")

# if you gave LWS_WITH_MINIZ, point to MINIZ here if not found
# automatically

set(LWS_ZLIB_LIBRARIES CACHE PATH "Path to the zlib/miniz library")
set(LWS_ZLIB_INCLUDE_DIRS CACHE PATH "Path to the zlib/miniz include directory")
set(LWS_SQLITE3_LIBRARIES CACHE PATH "Path to the sqlite3 library")
set(LWS_SQLITE3_INCLUDE_DIRS CACHE PATH "Path to the sqlite3 include directory")
set(LWS_LIBMOUNT_INCLUDE_DIRS CACHE PATH "Path to the libmount include directory")
set(LWS_LIBMOUNT_LIBRARIES CACHE PATH "Path to the libmount library")
# on unix, these are in the toolchain.  On win32 you have to put them somewhere
# yourself and point to them here
set(LWS_EXT_PTHREAD_INCLUDE_DIR CACHE PATH "Path to an external pthreads include directory")
set(LWS_EXT_PTHREAD_LIBRARIES CACHE PATH "Path to an external pthreads library")


if (LWS_WITH_HTTP_STREAM_COMPRESSION)
	set(LWS_WITH_ZLIB 1)
endif()

if (LWS_WITH_ZLIB AND NOT LWS_WITH_BUNDLED_ZLIB)
	if ("${LWS_ZLIB_LIBRARIES}" STREQUAL "" OR "${LWS_ZLIB_INCLUDE_DIRS}" STREQUAL "")
	else()
		set(ZLIB_LIBRARIES ${LWS_ZLIB_LIBRARIES})
		set(ZLIB_INCLUDE_DIRS ${LWS_ZLIB_INCLUDE_DIRS})
		set(ZLIB_FOUND 1)
	endif()
endif()


if (LWS_WITH_SQLITE3)
	if ("${LWS_SQLITE3_LIBRARIES}" STREQUAL "" OR "${LWS_SQLITE3_INCLUDE_DIRS}" STREQUAL "")
	else()
		set(SQLITE3_LIBRARIES ${LWS_SQLITE3_LIBRARIES})
		set(SQLITE3_INCLUDE_DIRS ${LWS_SQLITE3_INCLUDE_DIRS})
		set(SQLITE3_FOUND 1)
	endif()
endif()

include_directories("${PROJECT_BINARY_DIR}")

# Check for different inline keyword versions.
foreach(KEYWORD "inline" "__inline__" "__inline")
	set(CMAKE_REQUIRED_DEFINITIONS "-DKEYWORD=${KEYWORD}")
	CHECK_C_SOURCE_COMPILES(
		"
		#include <stdio.h>
		static KEYWORD void a() {}
		int main(int argc, char **argv) { a(); return 0; }
		" LWS_HAVE_${KEYWORD})
endforeach()

if (NOT LWS_HAVE_inline)
	if (LWS_HAVE___inline__)
		set(inline __inline__)
	elseif(LWS_HAVE___inline)
		set(inline __inline)
	endif()
endif()

# Put the libraries and binaries that get built into directories at the
# top of the build tree rather than in hard-to-find leaf directories.
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

SET(LWS_INSTALL_PATH "${CMAKE_INSTALL_PREFIX}")

# Commented out to avoid using absolute paths in the install_name on macOS.
# When CMAKE_INSTALL_NAME_DIR is set to an absolute path, binaries that link
# against libwebsockets will hardcode this absolute path, making the library
# difficult to relocate and potentially causing "library not found" errors at
# runtime if the library is installed in a different location.
# SET(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/${LWS_INSTALL_LIB_DIR}${LIB_SUFFIX}")

if (LWS_WITHOUT_BUILTIN_SHA1)
	set(LWS_SHA1_USE_OPENSSL_NAME 1)
endif()

CHECK_C_SOURCE_COMPILES(
	"#include <malloc.h>
	int main(int argc, char **argv) { return malloc_trim(0); }
	" LWS_HAVE_MALLOC_TRIM)
CHECK_C_SOURCE_COMPILES(
	"#include <malloc.h>
	int main(int argc, char **argv) { return (int)malloc_usable_size((void *)0); }
	" LWS_HAVE_MALLOC_USABLE_SIZE)

CHECK_FUNCTION_EXISTS(fork LWS_HAVE_FORK)
CHECK_FUNCTION_EXISTS(getenv LWS_HAVE_GETENV)
CHECK_FUNCTION_EXISTS(malloc LWS_HAVE_MALLOC)
CHECK_FUNCTION_EXISTS(memset LWS_HAVE_MEMSET)
CHECK_FUNCTION_EXISTS(realloc LWS_HAVE_REALLOC)
CHECK_FUNCTION_EXISTS(socket LWS_HAVE_SOCKET)
CHECK_FUNCTION_EXISTS(strerror LWS_HAVE_STRERROR)
CHECK_FUNCTION_EXISTS(vfork LWS_HAVE_VFORK)
CHECK_FUNCTION_EXISTS(execvpe LWS_HAVE_EXECVPE)
CHECK_FUNCTION_EXISTS(getifaddrs LWS_HAVE_GETIFADDRS)
CHECK_FUNCTION_EXISTS(snprintf LWS_HAVE_SNPRINTF)
CHECK_FUNCTION_EXISTS(_snprintf LWS_HAVE__SNPRINTF)
CHECK_FUNCTION_EXISTS(_vsnprintf LWS_HAVE__VSNPRINTF)
CHECK_FUNCTION_EXISTS(getloadavg LWS_HAVE_GETLOADAVG)
CHECK_FUNCTION_EXISTS(atoll LWS_HAVE_ATOLL)
CHECK_FUNCTION_EXISTS(_atoi64 LWS_HAVE__ATOI64)
CHECK_FUNCTION_EXISTS(_stat32i64 LWS_HAVE__STAT32I64)
CHECK_FUNCTION_EXISTS(clock_gettime LWS_HAVE_CLOCK_GETTIME)
CHECK_FUNCTION_EXISTS(localtime_r LWS_HAVE_LOCALTIME_R)
CHECK_FUNCTION_EXISTS(gmtime_r LWS_HAVE_GMTIME_R)
CHECK_FUNCTION_EXISTS(ctime_r LWS_HAVE_CTIME_R)
CHECK_FUNCTION_EXISTS(getgrgid_r LWS_HAVE_GETGRGID_R)
CHECK_FUNCTION_EXISTS(getgrnam_r LWS_HAVE_GETGRNAM_R)
CHECK_FUNCTION_EXISTS(getpwuid_r LWS_HAVE_GETPWUID_R)
CHECK_FUNCTION_EXISTS(getpwnam_r LWS_HAVE_GETPWNAM_R)
CHECK_FUNCTION_EXISTS(timegm LWS_HAVE_TIMEGM)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	if(CMAKE_OSX_DEPLOYMENT_TARGET LESS "10.12")
		message("No clock_gettime found on macOS ${CMAKE_OSX_DEPLOYMENT_TARGET}. Disabling LWS_HAVE_CLOCK_GETTIME.")
		set(LWS_HAVE_CLOCK_GETTIME 0)
	endif()
endif()

if (NOT LWS_HAVE_GETIFADDRS)
	if (LWS_WITHOUT_BUILTIN_GETIFADDRS)
		message(FATAL_ERROR "No getifaddrs was found on the system. Turn off the LWS_WITHOUT_BUILTIN_GETIFADDRS compile option to use the supplied BSD version.")
	endif()
	set(LWS_BUILTIN_GETIFADDRS 1)
endif()

if (LWS_EXT_PTHREAD_INCLUDE_DIR)
	set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES};${LWS_EXT_PTHREAD_INCLUDE_DIR})
	include_directories(${LWS_EXT_PTHREAD_INCLUDE_DIR})

	list(APPEND LIB_LIST_AT_END ${LWS_EXT_PTHREAD_LIBRARIES})
	set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} " -DHAVE_STRUCT_TIMESPEC=1")
endif()

#
# add libs here that need to be at the end of the link order
#

if (LWS_EXT_PTHREAD_INCLUDE_DIR)
	list(APPEND LIB_LIST_AT_END ${LWS_EXT_PTHREAD_LIBRARIES})
endif()

if (LWS_WITH_ZLIB AND NOT LWS_WITH_BUNDLED_ZLIB)
	list(APPEND LIB_LIST_AT_END "${ZLIB_LIBRARIES}")
endif()

if (LWS_WITH_PLUGINS_API AND UNIX AND CMAKE_DL_LIBS AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "QNX"))
	list(APPEND LIB_LIST_AT_END ${CMAKE_DL_LIBS})
endif()


CHECK_INCLUDE_FILE(in6addr.h LWS_HAVE_IN6ADDR_H)
CHECK_INCLUDE_FILE(memory.h LWS_HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(netinet/in.h LWS_HAVE_NETINET_IN_H)
CHECK_INCLUDE_FILE(stdint.h LWS_HAVE_STDINT_H)
CHECK_INCLUDE_FILE(stdlib.h LWS_HAVE_STDLIB_H)
CHECK_INCLUDE_FILE(strings.h LWS_HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(string.h LWS_HAVE_STRING_H)
CHECK_INCLUDE_FILE(sys/prctl.h LWS_HAVE_SYS_PRCTL_H)
CHECK_INCLUDE_FILE(sys/socket.h LWS_HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILE(sys/sockio.h LWS_HAVE_SYS_SOCKIO_H)
CHECK_INCLUDE_FILE(sys/stat.h LWS_HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/types.h LWS_HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(unistd.h LWS_HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(vfork.h LWS_HAVE_VFORK_H)
CHECK_INCLUDE_FILE(sys/capability.h LWS_HAVE_SYS_CAPABILITY_H)
CHECK_INCLUDE_FILE(malloc.h LWS_HAVE_MALLOC_H)
CHECK_INCLUDE_FILE(pthread.h LWS_HAVE_PTHREAD_H)
CHECK_INCLUDE_FILE(inttypes.h LWS_HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(sys/resource.h LWS_HAVE_SYS_RESOURCE_H)

if (WIN32 OR MSVC)
	CHECK_C_SOURCE_COMPILES("#include <winsock2.h>
				 #include <afunix.h>
				 int main() { return 0; }" LWS_HAVE_WIN32_AFUNIX_H)

	if (LWS_UNIX_SOCK AND NOT LWS_HAVE_WIN32_AFUNIX_H)
		message("No afunix.h found. Disabling LWS_UNIX_SOCK.")
		set(LWS_WITH_UNIX_SOCK OFF)
	endif()
endif()

CHECK_LIBRARY_EXISTS(cap cap_set_flag "" LWS_HAVE_LIBCAP)


if (LWS_WITH_ZLIB AND NOT LWS_WITH_BUNDLED_ZLIB)
	if (LWS_WITH_MINIZ)
		CHECK_INCLUDE_FILE(miniz.h LWS_HAVE_ZLIB_H)
	else()
		CHECK_INCLUDE_FILE(zlib.h LWS_HAVE_ZLIB_H)
	endif()
endif()

CHECK_INCLUDE_FILES("stdlib.h;stdarg.h;string.h" STDC_HEADERS)

if (NOT CMAKE_REQUIRED_FLAGS)
	set(CMAKE_REQUIRED_FLAGS "")
endif()
if (NOT CMAKE_REQUIRED_INCLUDES)
	set(CMAKE_REQUIRED_INCLUDES "")
endif()
if (NOT CMAKE_REQUIRED_LIBRARIES)
	set(CMAKE_REQUIRED_LIBRARIES "")
endif()

CHECK_C_SOURCE_COMPILES("#include <stdint.h>
	int main(void) {
		intptr_t test = 1;
		return 0;
	}" LWS_HAS_INTPTR_T)

if ((CMAKE_C_COMPILER_ID MATCHES "Clang") OR
    (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
	set(COMPILER_IS_CLANG ON)
endif()

if (LWS_HAVE_PTHREAD_H AND NOT LWS_PLAT_FREERTOS)
	CHECK_C_SOURCE_COMPILES("
		#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
		#endif
		#include <pthread.h>
		int main(void) {
		#ifdef __PTW32_H
			pthread_t th = {0,0};
		#else
			pthread_t th = 0;
		#endif
			pthread_setname_np(th, NULL);
			return 0;
		}" LWS_HAS_PTHREAD_SETNAME_NP)
endif()

CHECK_C_SOURCE_COMPILES("#include <stddef.h>
	#include <getopt.h>
	int main(void) {
		void *p = (void *)getopt_long;
		return p != NULL;
	}" LWS_HAS_GETOPT_LONG)

CHECK_C_SOURCE_COMPILES("#include <linux/rtnetlink.h>
	int main(void) {
		int test = RTA_PREF;
		return 0;
	}" LWS_HAVE_RTA_PREF)

CHECK_C_SOURCE_COMPILES("#include <sys/types.h>
	int main(void) {
		suseconds_t x = 0;
		return (int)x;
	}" LWS_HAVE_SUSECONDS_T)

if (NOT PID_T_SIZE)
	set(pid_t int)
endif()

if (NOT SIZE_T_SIZE)
	set(size_t "unsigned int")
endif()

if (NOT LWS_HAVE_MALLOC)
	set(malloc rpl_malloc)
endif()

if (NOT LWS_HAVE_REALLOC)
	set(realloc rpl_realloc)
endif()




if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR COMPILER_IS_CLANG)
    include (CheckCCompilerFlag)
    CHECK_C_COMPILER_FLAG(-fvisibility=hidden LWS_HAVE_VISIBILITY)
    if (LWS_WITH_FANALYZER)
	    CHECK_C_COMPILER_FLAG(-fanalyzer LWS_HAVE_FANALYZER)
    endif()
    if (LWS_HAVE_VISIBILITY)
                set(VISIBILITY_FLAG -fvisibility=hidden)
    endif()
    if (LWS_WITH_GCOV)
	    set (GCOV_FLAGS "-fprofile-arcs -ftest-coverage ")
    else()
	    set(GCOV_FLAGS "")
    endif()

	if (LWS_WITH_ASAN)
		set (ASAN_FLAGS "-fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize-undefined-trap-on-error")
		if (NOT COMPILER_IS_CLANG)
			set (ASAN_FLAGS "${ASAN_FLAGS} -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak")
		endif()
		message("Enabling ASAN")
	else()
		set(ASAN_FLAGS "")
	endif()

	check_c_compiler_flag("-Wignored-qualifiers" LWS_GCC_HAS_IGNORED_QUALIFIERS)
	check_c_compiler_flag("-Wtype-limits" LWS_GCC_HAS_TYPE_LIMITS)
	check_c_compiler_flag("-Wno-deprecated-declarations" LWS_GCC_HAS_NO_DEPRECATED_DECLARATIONS)

	if (LWS_GCC_HAS_IGNORED_QUALIFIERS)
		set(CMAKE_C_FLAGS "-Wignored-qualifiers ${CMAKE_C_FLAGS}" )
	endif()

	if (LWS_GCC_HAS_TYPE_LIMITS)
		set(CMAKE_C_FLAGS "-Wtype-limits ${CMAKE_C_FLAGS}" )
	endif()

	if (LWS_WITH_FANALYZER AND LWS_HAVE_FANALYZER)
		set(CMAKE_C_FLAGS "-fanalyzer ${CMAKE_C_FLAGS}" )
	endif()

	if (CMAKE_COMPILER_IS_CLANG OR CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.4)
		set(CMAKE_C_FLAGS "-Wuninitialized ${CMAKE_C_FLAGS}")
	endif()

	# always warn all and generate debug info
	if (UNIX AND NOT LWS_PLAT_FREERTOS)
		set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wconversion -Wsign-compare -Wstrict-aliasing ${VISIBILITY_FLAG} -Wundef ${GCOV_FLAGS} ${CMAKE_C_FLAGS} ${ASAN_FLAGS}" )
	else()
		set(CMAKE_C_FLAGS "-Wall -Wsign-compare ${VISIBILITY_FLAG} ${GCOV_FLAGS} ${CMAKE_C_FLAGS}" )
	endif()

	if (PICO_SDK_PATH)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wconversion -Wsign-compare -Wstrict-aliasing -Wundef -nolibc")
	endif()

	if (ESP_PLATFORM AND (CONFIG_IDF_TARGET_ESP32 OR CONFIG_IDF_TARGET_ESP32S2 OR CONFIG_IDF_TARGET_ESP32S3))
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mlongcalls")
	endif()

	if ("${DISABLE_WERROR}" STREQUAL "OFF")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
	endif()

	if (LWS_SUPPRESS_DEPRECATED_API_WARNINGS)
		set(CMAKE_C_FLAGS "-Wno-deprecated ${CMAKE_C_FLAGS}")
		if (LWS_GCC_HAS_NO_DEPRECATED_DECLARATIONS)
			set(CMAKE_C_FLAGS "-Wno-deprecated-declarations ${CMAKE_C_FLAGS}")
		endif()
	endif()
endif ()

if ((CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX) AND NOT LWS_WITHOUT_TESTAPPS)
	if (UNIX AND LWS_HAVE_PTHREAD_H AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "QNX"))
	# jeez clang understands -pthread but dies if he sees it at link time!
	# http://stackoverflow.com/questions/2391194/what-is-gs-pthread-equiv-in-clang
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread" )
	list(APPEND LIB_LIST_AT_END -lpthread)
    endif()
endif()

if (COMPILER_IS_CLANG)

	# otherwise osx blows a bunch of openssl deprecated api errors
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations" )
	if (UNIX AND LWS_HAVE_PTHREAD_H)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -Wno-error=unused-command-line-argument" )
	endif()
endif()

if (WINCE)
	list(APPEND LIB_LIST_AT_END ws2.lib)
elseif (WIN32)
	list(APPEND LIB_LIST_AT_END ws2_32.lib userenv.lib psapi.lib iphlpapi.lib crypt32.lib)
endif()

if (MSVC)
	# Turn off pointless microsoft security warnings.
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
	# Fail the build if any warnings
	add_compile_options(/W3 /WX)
	add_compile_options(/Zc:preprocessor /wd4819)
	# Unbreak MSVC broken preprocessor __VA_ARGS__ behaviour
	if (MSVC_VERSION GREATER 1925)
		add_compile_options(/Zc:preprocessor /wd5105)
	else()
		add_compile_options(/experimental:preprocessor /wd5105)
	endif()
endif(MSVC)

if (MINGW)
	set(LWS_MINGW_SUPPORT 1)
	set(CMAKE_C_FLAGS "-D__USE_MINGW_ANSI_STDIO ${CMAKE_C_FLAGS}")
	add_definitions(-DWINVER=0x0601 -D_WIN32_WINNT=0x0601)
endif()

if (HDR_PRIVATE)
	source_group("Headers Private"  FILES ${HDR_PRIVATE})
endif()
if (HDR_PUBLIC)
	source_group("Headers Public"   FILES ${HDR_PUBLIC})
endif()
if (SOURCES)
	source_group("Sources"          FILES ${SOURCES})
endif()
if (RESOURCES)
	source_group("Resources"        FILES ${RESOURCES})
endif()


#
# ZLIB (needed for deflate extension and if LWS_WITH_HTTP_STREAM_COMPRESSION)
#
if (LWS_WITH_ZLIB)
	if (NOT ZLIB_FOUND)
		if (LWS_WITH_MINIZ)
			find_package(Miniz REQUIRED)
			set(ZLIB_INCLUDE_DIRS ${MINIZ_INCLUDE_DIR})
			set(ZLIB_LIBRARIES ${MINIZ_LIBRARIES})
		else()
			find_package(ZLIB REQUIRED)
		endif()
	endif()
	message("zlib/miniz include dirs: ${ZLIB_INCLUDE_DIRS}")
	message("zlib/miniz libraries: ${ZLIB_LIBRARIES}")
	include_directories(${ZLIB_INCLUDE_DIRS})
	# done later at end of link list
	# list(APPEND LIB_LIST ${ZLIB_LIBRARIES})
	set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${ZLIB_LIBRARIES})
	list(APPEND LIB_LIST_AT_END ${ZLIB_LIBRARIES})
endif()


if (LWS_WITH_FSMOUNT AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	if (NOT LWS_LIBMOUNT_INCLUDE_DIRS STREQUAL "")
		include_directories(${LWS_LIBMOUNT_INCLUDE_DIRS})
		message("libmount include dir: ${LWS_LIBMOUNT_INCLUDE_DIRS}")
	endif()
	if (NOT LWS_LIBMOUNT_LIBRARIES STREQUAL "")
		message("libmount libraries: ${LWS_LIBMOUNT_LIBRARIES}")
		list(APPEND LIB_LIST ${LWS_LIBMOUNT_LIBRARIES})
	else()
		list(APPEND LIB_LIST mount)
	endif()
endif()


if (LWS_WITH_SQLITE3)
	if (NOT SQLITE3_FOUND)
		find_path(SQLITE3_INCLUDE_DIRS NAMES sqlite3.h)
		find_library(SQLITE3_LIBRARIES NAMES sqlite3)
		if(SQLITE3_INCLUDE_DIRS AND SQLITE3_LIBRARIES)
			set(SQLITE3_FOUND 1)
		endif()
	endif()
	message("sqlite3 include dir: ${SQLITE3_INCLUDE_DIRS}")
	message("sqlite3 libraries: ${SQLITE3_LIBRARIES}")
	include_directories("${SQLITE3_INCLUDE_DIRS}")
	list(APPEND LIB_LIST ${SQLITE3_LIBRARIES})
endif()


if (LWS_WITH_HUBBUB)
	find_library(LIBHUBBUB_LIBRARIES NAMES hubbub)
	list(APPEND LIB_LIST ${LIBHUBBUB_LIBRARIES} )
endif()

if (LWS_HAVE_LIBCAP)
	find_library(LIBCAP_LIBRARIES NAMES cap)
	list(APPEND LIB_LIST ${LIBCAP_LIBRARIES} )
endif()

if (LWS_WITH_PLUGINS_BUILTIN)
	add_subdirectory(plugins)
endif()


#
# Append the "at end" pieces to the lib list
#
list(APPEND LIB_LIST ${LIB_LIST_AT_END})

#
# Second-level CMakeLists
#

include_directories("${PROJECT_SOURCE_DIR}/lib")

add_subdirectory(lib)


if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib${LIB_SUFFIX}/cmake/libwebsockets)
endif()

configure_file(${PROJECT_SOURCE_DIR}/cmake/LwsCheckRequirements.cmake
		${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/LwsCheckRequirements.cmake
		@ONLY)

configure_file(${PROJECT_SOURCE_DIR}/cmake/LwsCheckRequirements.cmake
		${PROJECT_BINARY_DIR}/LwsCheckRequirements.cmake
		@ONLY)

# Generate version info for both build-tree and install-tree.
configure_file(${PROJECT_SOURCE_DIR}/cmake/libwebsockets-config-version.cmake.in
                ${PROJECT_BINARY_DIR}/libwebsockets-config-version.cmake
                @ONLY)

# Generate the config file for the build-tree.
set(LWS__INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/lib"
    "${PROJECT_BINARY_DIR}")
set(LIBWEBSOCKETS_INCLUDE_DIRS ${LWS__INCLUDE_DIRS} CACHE PATH "Libwebsockets include directories")
configure_file(${PROJECT_SOURCE_DIR}/cmake/libwebsockets-config.cmake.in
                ${PROJECT_BINARY_DIR}/libwebsockets-config.cmake
                @ONLY)
set(LWS_INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Export targets (This is used for other CMake projects to easily find the libraries and include files).
if (LWS_WITH_EXPORT_LWSTARGETS)
    export(TARGETS ${LWS_LIBRARIES}
            FILE "${PROJECT_BINARY_DIR}/LibwebsocketsTargets.cmake")
endif()



set(libwebsockets_DIR ${PROJECT_BINARY_DIR})
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
message("DIR ${libwebsockets_DIR} CMP ${CMAKE_MODULE_PATH}")

if (LWS_WITH_MINIMAL_EXAMPLES)
	add_subdirectory(minimal-examples)
endif()

if (NOT LWS_WITHOUT_TESTAPPS)
	add_subdirectory(test-apps)
endif()

if (NOT LWS_WITH_PLUGINS_BUILTIN)
add_subdirectory(plugins)
endif()
add_subdirectory(lwsws)

# Generate the lws_config.h that includes all the public compilation settings.
configure_file(
       "${PROJECT_SOURCE_DIR}/cmake/lws_config.h.in"
       "${PROJECT_BINARY_DIR}/lws_config.h")

add_custom_command(
		OUTPUT ${PROJECT_BINARY_DIR}/include/lws_config.h
			${PROJECT_BINARY_DIR}/include/libwebsockets
			${PROJECT_BINARY_DIR}/include/libwebsockets.h
		COMMENT "Creating build include dir"
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/libwebsockets.h
     			${CMAKE_CURRENT_BINARY_DIR}/include/libwebsockets.h
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/libwebsockets/
			${CMAKE_CURRENT_BINARY_DIR}/include/libwebsockets
		COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/lws_config.h
     			${CMAKE_CURRENT_BINARY_DIR}/include/lws_config.h
		MAIN_DEPENDENCY ${PROJECT_BINARY_DIR}/lws_config.h
)

add_custom_target(GENHDR DEPENDS  ${PROJECT_BINARY_DIR}/include/lws_config.h)

file(GLOB HDR_PUBLIC1 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/libwebsockets/*.h)
file(GLOB HDR_PUBLIC2 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/libwebsockets.h)
file(GLOB HDR_PUBLIC3 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include/lws_config.h)
list(APPEND HDR_PUBLIC ${HDR_PUBLIC1} ${HDR_PUBLIC2} ${HDR_PUBLIC3})

set_source_files_properties(${HDR_PUBLIC} PROPERTIES GENERATED 1)

if (LWS_WITH_STATIC)
	add_dependencies(websockets GENHDR)
endif()
if (LWS_WITH_SHARED)
	add_dependencies(websockets_shared GENHDR)
endif()



#
#
# Installation preparations.
#

export(PACKAGE libwebsockets)

install(DIRECTORY include/libwebsockets
	DESTINATION "${LWS_INSTALL_INCLUDE_DIR}" COMPONENT dev)
install(FILES ${PROJECT_BINARY_DIR}/include/libwebsockets.h ${PROJECT_BINARY_DIR}/include/lws_config.h
	DESTINATION "${LWS_INSTALL_INCLUDE_DIR}" COMPONENT dev)

# Generate the config file for the installation tree.
get_filename_component(LWS_ABSOLUTE_INSTALL_CMAKE_DIR ${LWS_INSTALL_CMAKE_DIR} ABSOLUTE)
get_filename_component(LWS_ABSOLUTE_INSTALL_INCLUDE_DIR ${LWS_INSTALL_INCLUDE_DIR} ABSOLUTE)
file(RELATIVE_PATH
    REL_INCLUDE_DIR
    "${LWS_ABSOLUTE_INSTALL_CMAKE_DIR}"
    "${LWS_ABSOLUTE_INSTALL_INCLUDE_DIR}") # Calculate the relative directory from the cmake dir.

if (DEFINED REL_INCLUDE_DIR)
    set(LWS__INCLUDE_DIRS "\${LWS_CMAKE_DIR}/${REL_INCLUDE_DIR}")
endif()
if (DEFINED OPENSSL_INCLUDE_DIRS)
	set(LWS__INCLUDE_DIRS "${LWS__INCLUDE_DIRS};${OPENSSL_INCLUDE_DIRS}")
endif()

configure_file(${PROJECT_SOURCE_DIR}/cmake/libwebsockets-config.cmake.in
                ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/libwebsockets-config.cmake
                @ONLY)

set_target_properties(${LWS_LIBRARIES}
		PROPERTIES PUBLIC_HEADER "${HDR_PUBLIC}")

# Install the LibwebsocketsConfig.cmake and LibwebsocketsConfigVersion.cmake
install(FILES
               "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/libwebsockets-config.cmake"
               "${PROJECT_BINARY_DIR}/libwebsockets-config-version.cmake"
               "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/LwsCheckRequirements.cmake"
               DESTINATION "${LWS_INSTALL_CMAKE_DIR}" COMPONENT dev)

# Install exports for the install-tree.
if (LWS_WITH_EXPORT_LWSTARGETS)
    install(EXPORT LibwebsocketsTargets
            DESTINATION "${LWS_INSTALL_CMAKE_DIR}" COMPONENT dev)
endif()

# build subdir is not part of sources
set(CPACK_SOURCE_IGNORE_FILES $(CPACK_SOURCE_IGNORE_FILES) "/.git/" "/build/" "\\\\.tgz$" "\\\\.tar\\\\.gz$")

# Most people are more used to "make dist" compared to "make package_source"
add_custom_target(dist COMMAND "${CMAKE_MAKE_PROGRAM}" package_source)



# This must always be last!
include(CPack)
