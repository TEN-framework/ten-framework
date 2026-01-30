# nghttp2 - HTTP/2 C Library
#
# Copyright (c) 2012, 2013, 2014, 2015 Tatsuhiro Tsujikawa
# Copyright (c) 2016 Peter Wu <peter@lekensteyn.nl>
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.14)
# XXX using 1.8.90 instead of 1.9.0-DEV
project(nghttp2 VERSION 1.65.0 LANGUAGES C)

# See versioning rule:
#  https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
set(LT_CURRENT  42)
set(LT_REVISION 4)
set(LT_AGE      28)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(Version)

math(EXPR LT_SOVERSION "${LT_CURRENT} - ${LT_AGE}")
set(LT_VERSION "${LT_SOVERSION}.${LT_AGE}.${LT_REVISION}")
set(PACKAGE_VERSION     "${PROJECT_VERSION}")
HexVersion(PACKAGE_VERSION_NUM ${PROJECT_VERSION_MAJOR} ${PROJECT_VERSION_MINOR} ${PROJECT_VERSION_PATCH})

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the build type" FORCE)

  # Include "None" as option to disable any additional (optimization) flags,
  # relying on just CMAKE_C_FLAGS and CMAKE_CXX_FLAGS (which are empty by
  # default). These strings are presented in cmake-gui.
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "None" "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

include(GNUInstallDirs)
include(CMakeDependentOption)

# For documentation
find_package(Python3 COMPONENTS Interpreter)

# Auto-detection of features that can be toggled
if(NOT ENABLE_LIB_ONLY)
  enable_language(CXX)
  find_package(Libev 4.11)
  find_package(Libcares 1.7.5)
  find_package(ZLIB 1.2.3)
  find_package(Libbrotlienc 1.0.9)
  find_package(Libbrotlidec 1.0.9)
endif()

if(WITH_WOLFSSL)
  find_package(WolfSSL 5.7.0)
else()
  find_package(OpenSSL 1.1.1)
endif()
find_package(Libngtcp2 1.0.0)
if(OPENSSL_FOUND)
  find_package(Libngtcp2_crypto_quictls 1.0.0)
  if(LIBNGTCP2_CRYPTO_QUICTLS_FOUND)
    set(HAVE_LIBNGTCP2_CRYPTO_QUICTLS 1)
  endif()
endif()
if(WOLFSSL_FOUND)
  find_package(Libngtcp2_crypto_wolfssl 1.0.0)
  if(LIBNGTCP2_CRYPTO_WOLFSSL_FOUND)
    set(HAVE_LIBNGTCP2_CRYPTO_WOLFSSL 1)
  endif()
endif()
find_package(Libnghttp3 1.1.0)
if(WITH_LIBBPF)
  find_package(Libbpf 0.7.0)
  if(NOT LIBBPF_FOUND)
    message(FATAL_ERROR "libbpf was requested (WITH_LIBBPF=1) but not found.")
  endif()
endif()
if((OPENSSL_FOUND OR WOLFSSL_FOUND) AND LIBEV_FOUND AND ZLIB_FOUND)
  set(ENABLE_APP_DEFAULT ON)
else()
  set(ENABLE_APP_DEFAULT OFF)
endif()
find_package(Systemd 209)
find_package(Jansson  2.5)
set(ENABLE_HPACK_TOOLS_DEFAULT ${JANSSON_FOUND})
# 2.0.8 is required because we use evconnlistener_set_error_cb()
find_package(Libevent 2.0.8 COMPONENTS core extra openssl)
set(ENABLE_EXAMPLES_DEFAULT ${LIBEVENT_OPENSSL_FOUND})

find_package(LibXml2 2.6.26)
set(WITH_LIBXML2_DEFAULT    ${LIBXML2_FOUND})
find_package(Jemalloc)
set(WITH_JEMALLOC_DEFAULT   ${JEMALLOC_FOUND})

include(CMakeOptions.txt)

if(ENABLE_LIB_ONLY AND (ENABLE_APP OR ENABLE_HPACK_TOOLS OR ENABLE_EXAMPLES))
  # Remember when disabled options are disabled for later diagnostics.
  set(ENABLE_LIB_ONLY_DISABLED_OTHERS 1)
else()
  set(ENABLE_LIB_ONLY_DISABLED_OTHERS 0)
endif()
if(ENABLE_LIB_ONLY)
  set(ENABLE_APP            OFF)
  set(ENABLE_HPACK_TOOLS    OFF)
  set(ENABLE_EXAMPLES       OFF)
endif()

# Do not disable assertions based on CMAKE_BUILD_TYPE.
foreach(_build_type "Release" "MinSizeRel" "RelWithDebInfo")
  foreach(_lang C CXX)
    string(TOUPPER "CMAKE_${_lang}_FLAGS_${_build_type}" _var)
    string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " " ${_var} "${${_var}}")
  endforeach()
endforeach()

if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(HINT_NORETURN       "__attribute__((noreturn))")
else()
  set(HINT_NORETURN)
endif()

if(NOT ENABLE_LIB_ONLY)
  include(ExtractValidFlags)
  foreach(_cxx1x_flag -std=c++20)
    extract_valid_cxx_flags(_cxx1x_flag_supported ${_cxx1x_flag})
    if(_cxx1x_flag_supported)
      set(CXX1XCXXFLAGS ${_cxx1x_flag})
      break()
    endif()
  endforeach()

  include(CMakePushCheckState)
  include(CheckCXXSourceCompiles)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_DEFINITIONS "${CXX1XCXXFLAGS}")
  # Check that std::future is available.
  check_cxx_source_compiles("
  #include <vector>
  #include <future>
  int main() { std::vector<std::future<int>> v; }" HAVE_STD_FUTURE)
  # Check that std::map::emplace is available for g++-4.7.
  check_cxx_source_compiles("
  #include <map>
  int main() { std::map<int, int>().emplace(1, 2); }" HAVE_STD_MAP_EMPLACE)
  cmake_pop_check_state()
endif()

# Checks for libraries.
# Additional libraries required for programs under src directory.
set(APP_LIBRARIES)

set(CMAKE_THREAD_PREFER_PTHREAD 1)
find_package(Threads)
if(CMAKE_USE_PTHREADS_INIT)
  list(APPEND APP_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif()
# XXX android and C++, is this still needed in cmake?
# case "$host" in
#   *android*)
#     android_build=yes
#     # android does not need -pthread, but needs following 3 libs for C++
#     APPLDFLAGS="$APPLDFLAGS -lstdc++ -latomic -lsupc++"

# dl: openssl requires libdl when it is statically linked.
# XXX shouldn't ${CMAKE_DL_LIBS} be appended to OPENSSL_LIBRARIES instead of
# APP_LIBRARIES if it is really specific to OpenSSL?

enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

# openssl (for src)
include(CheckSymbolExists)
set(HAVE_OPENSSL    ${OPENSSL_FOUND})
if(NOT ENABLE_LIB_ONLY AND OPENSSL_FOUND)
  set(OPENSSL_INCLUDE_DIRS  ${OPENSSL_INCLUDE_DIR})
  cmake_push_check_state()
  set(CMAKE_REQUIRED_INCLUDES   "${OPENSSL_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES  "${OPENSSL_LIBRARIES}")
  if(WIN32)
    set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}" "ws2_32" "bcrypt")
  endif()
  if(ENABLE_HTTP3)
    check_symbol_exists(SSL_provide_quic_data "openssl/ssl.h" HAVE_SSL_PROVIDE_QUIC_DATA)
    if(NOT HAVE_SSL_PROVIDE_QUIC_DATA)
        message(WARNING "OpenSSL in ${OPENSSL_LIBRARIES} does not have SSL_provide_quic_data.  HTTP/3 support cannot be enabled")
    endif()
  endif()
  cmake_pop_check_state()
else()
  set(OPENSSL_INCLUDE_DIRS  "")
  set(OPENSSL_LIBRARIES     "")
endif()
# wolfSSL (for src)
set(HAVE_WOLFSSL ${WOLFSSL_FOUND})
if(WOLFSSL_FOUND)
  set(WOLFSSL_INCLUDE_DIRS ${WOLFSSL_INCLUDE_DIR})
  cmake_push_check_state()
  set(CMAKE_REQUIRED_INCLUDES   "${WOLFSSL_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES  "${WOLFSSL_LIBRARIES}")
  check_symbol_exists(SSL_provide_quic_data "wolfssl/options.h;wolfssl/ssl.h" HAVE_WOLFSSL_SSL_PROVIDE_QUIC_DATA)
  if(NOT HAVE_WOLFSSL_SSL_PROVIDE_QUIC_DATA)
    message(WARNING "wolfSSL in ${WOLFSSL_LIBRARIES} does not have SSL_provide_quic_data.  HTTP/3 support cannot be enabled")
  endif()
  cmake_pop_check_state()
else()
  set(WOLFSSL_INCLUDE_DIRS "")
  set(WOLFSSL_LIBRARIES    "")
endif()
# libev (for src)
set(HAVE_LIBEV      ${LIBEV_FOUND})
set(HAVE_ZLIB       ${ZLIB_FOUND})
set(HAVE_SYSTEMD    ${SYSTEMD_FOUND})
set(HAVE_LIBEVENT_OPENSSL ${LIBEVENT_FOUND})
if(LIBEVENT_FOUND)
  # Must both link the core and openssl libraries.
  set(LIBEVENT_OPENSSL_LIBRARIES ${LIBEVENT_LIBRARIES})
endif()
# libc-ares (for src)
set(HAVE_LIBCARES   ${LIBCARES_FOUND})
if(LIBCARES_FOUND)
  set(LIBCARES_INCLUDE_DIRS ${LIBCARES_INCLUDE_DIR})
else()
  set(LIBCARES_INCLUDE_DIRS "")
  set(LIBCARES_LIBRARIES    "")
endif()
# jansson (for src/nghttp, src/deflatehd and src/inflatehd)
set(HAVE_JANSSON    ${JANSSON_FOUND})
# libxml2 (for src/nghttp)
set(HAVE_LIBXML2    ${LIBXML2_FOUND})
if(LIBXML2_FOUND)
  set(LIBXML2_INCLUDE_DIRS  ${LIBXML2_INCLUDE_DIR})
else()
  set(LIBXML2_INCLUDE_DIRS  "")
  set(LIBXML2_LIBRARIES     "")
endif()
# jemalloc
set(HAVE_JEMALLOC   ${JEMALLOC_FOUND})

# libbrotli (for src)
set(HAVE_LIBBROTLIENC ${LIBBROTLIENC_FOUND})
set(HAVE_LIBBROTLIDEC ${LIBBROTLIDEC_FOUND})
if(LIBBROTLIENC_FOUND AND LIBBROTLIDEC_FOUND)
  set(HAVE_LIBBROTLI 1)
endif()

# libbpf (for bpf)
set(HAVE_LIBBPF ${LIBBPF_FOUND})
if(LIBBPF_FOUND)
  set(BPFCFLAGS -Wall -O2 -g)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # For Debian/Ubuntu
    set(EXTRABPFCFLAGS -I/usr/include/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  endif()

  check_c_source_compiles("
#include <linux/bpf.h>
int main() { enum bpf_stats_type foo; (void)foo; }" HAVE_BPF_STATS_TYPE)
endif()

# The nghttp, nghttpd and nghttpx under src depend on zlib, OpenSSL and libev
if(ENABLE_APP AND NOT (ZLIB_FOUND AND (OPENSSL_FOUND OR WOLFSSL_FOUND) AND LIBEV_FOUND))
  message(FATAL_ERROR "Applications were requested (ENABLE_APP=1) but dependencies are not met.")
endif()

# HTTP/3 requires libngtcp2 + (quictls/openssl +
# libngtcp2_crypto_quictls or wolfSSL + libngtcp2_crypto_wolfssl) and
# libnghttp3.
if(ENABLE_HTTP3 AND NOT (LIBNGTCP2_FOUND AND LIBNGHTTP3_FOUND AND
      ((HAVE_SSL_PROVIDE_QUIC_DATA AND LIBNGTCP2_CRYPTO_QUICTLS_FOUND) OR
	(HAVE_WOLFSSL_SSL_PROVIDE_QUIC_DATA AND LIBNGTCP2_CRYPTO_WOLFSSL_FOUND))))
  message(FATAL_ERROR "HTTP/3 was requested (ENABLE_HTTP3=1) but dependencies are not met.")
endif()

# HPACK tools requires jansson
if(ENABLE_HPACK_TOOLS AND NOT HAVE_JANSSON)
  message(FATAL_ERROR "HPACK tools were requested (ENABLE_HPACK_TOOLS=1) but dependencies are not met.")
endif()

# examples
if(ENABLE_EXAMPLES AND NOT (OPENSSL_FOUND AND LIBEVENT_OPENSSL_FOUND))
  message(FATAL_ERROR "examples were requested (ENABLE_EXAMPLES=1) but dependencies are not met.")
endif()

# third-party http-parser only be built when needed
if(ENABLE_EXAMPLES OR ENABLE_APP OR ENABLE_HPACK_TOOLS)
  set(ENABLE_THIRD_PARTY 1)
  # mruby (for src/nghttpx)
  set(HAVE_MRUBY      ${WITH_MRUBY})
  set(HAVE_NEVERBLEED ${WITH_NEVERBLEED})
else()
  set(HAVE_MRUBY 0)
  set(HAVE_NEVERBLEED 0)
endif()

# Checks for header files.
include(CheckIncludeFile)
check_include_file("arpa/inet.h"    HAVE_ARPA_INET_H)
check_include_file("fcntl.h"        HAVE_FCNTL_H)
check_include_file("inttypes.h"     HAVE_INTTYPES_H)
check_include_file("limits.h"       HAVE_LIMITS_H)
check_include_file("netdb.h"        HAVE_NETDB_H)
check_include_file("netinet/in.h"   HAVE_NETINET_IN_H)
check_include_file("netinet/ip.h"   HAVE_NETINET_IP_H)
check_include_file("pwd.h"          HAVE_PWD_H)
check_include_file("sys/socket.h"   HAVE_SYS_SOCKET_H)
check_include_file("sys/time.h"     HAVE_SYS_TIME_H)
check_include_file("syslog.h"       HAVE_SYSLOG_H)
check_include_file("unistd.h"       HAVE_UNISTD_H)
check_include_file("windows.h"      HAVE_WINDOWS_H)

include(CheckTypeSize)
# Checks for typedefs, structures, and compiler characteristics.
# AC_TYPE_SIZE_T
check_type_size("ssize_t" SIZEOF_SSIZE_T)
if(SIZEOF_SSIZE_T STREQUAL "")
  # ssize_t is a signed type in POSIX storing at least -1.
  # Set it to "int" to match the behavior of AC_TYPE_SSIZE_T (autotools).
  set(ssize_t int)
endif()
# AC_TYPE_UINT8_T
# AC_TYPE_UINT16_T
# AC_TYPE_UINT32_T
# AC_TYPE_UINT64_T
# AC_TYPE_INT8_T
# AC_TYPE_INT16_T
# AC_TYPE_INT32_T
# AC_TYPE_INT64_T
# AC_TYPE_OFF_T
# AC_TYPE_PID_T
# AC_TYPE_UID_T
# XXX To support inline for crappy compilers, see https://cmake.org/Wiki/CMakeTestInline
# AC_C_INLINE
# XXX is AC_SYS_LARGEFILE still needed for modern systems?
# add_definitions(-D_FILE_OFFSET_BITS=64)

include(CheckStructHasMember)
check_struct_has_member("struct tm" tm_gmtoff time.h HAVE_STRUCT_TM_TM_GMTOFF)

# Checks for library functions.
include(CheckFunctionExists)
check_function_exists(_Exit     HAVE__EXIT)
check_function_exists(accept4   HAVE_ACCEPT4)
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists(mkostemp  HAVE_MKOSTEMP)
check_function_exists(pipe2     HAVE_PIPE2)

check_symbol_exists(GetTickCount64 sysinfoapi.h HAVE_GETTICKCOUNT64)

include(CheckSymbolExists)
# XXX does this correctly detect initgroups (un)availability on cygwin?
check_symbol_exists(initgroups grp.h HAVE_DECL_INITGROUPS)
if(NOT HAVE_DECL_INITGROUPS AND HAVE_UNISTD_H)
  # FreeBSD declares initgroups() in unistd.h
  check_symbol_exists(initgroups unistd.h HAVE_DECL_INITGROUPS2)
  if(HAVE_DECL_INITGROUPS2)
    set(HAVE_DECL_INITGROUPS 1)
  endif()
endif()

check_symbol_exists(CLOCK_MONOTONIC "time.h" HAVE_DECL_CLOCK_MONOTONIC)

set(WARNCFLAGS)
set(WARNCXXFLAGS)
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  if(ENABLE_WERROR)
    set(WARNCFLAGS    /WX)
    set(WARNCXXFLAGS  /WX)
  endif()
else()
  if(ENABLE_WERROR)
    set(WARNCFLAGS   "-Werror")
    set(WARNCXXFLAGS "-Werror")
  endif()

  include(PickyWarningsC)
  include(PickyWarningsCXX)
endif()

if(ENABLE_STATIC_CRT)
  foreach(lang C CXX)
    foreach(suffix "" _DEBUG _MINSIZEREL _RELEASE _RELWITHDEBINFO)
      set(var "CMAKE_${lang}_FLAGS${suffix}")
      string(REPLACE "/MD" "/MT" ${var} "${${var}}")
    endforeach()
  endforeach()
endif()

if(ENABLE_DEBUG)
  set(DEBUGBUILD 1)
endif()

# Some platform does not have working std::future.  We disable
# threading for those platforms.
if(NOT ENABLE_THREADS OR NOT HAVE_STD_FUTURE)
  set(NOTHREADS 1)
endif()

add_definitions(-DHAVE_CONFIG_H)
configure_file(cmakeconfig.h.in config.h)
# autotools-compatible names
# Sphinx expects relative paths in the .rst files. Use the fact that the files
# below are all one directory level deep.
file(RELATIVE_PATH top_srcdir   "${CMAKE_CURRENT_BINARY_DIR}/dir" "${CMAKE_CURRENT_SOURCE_DIR}")
file(RELATIVE_PATH top_builddir "${CMAKE_CURRENT_BINARY_DIR}/dir" "${CMAKE_CURRENT_BINARY_DIR}")
set(abs_top_srcdir  "${CMAKE_CURRENT_SOURCE_DIR}")
set(abs_top_builddir "${CMAKE_CURRENT_BINARY_DIR}")
# libnghttp2.pc (pkg-config file)
set(prefix          "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix     "${CMAKE_INSTALL_PREFIX}")
set(libdir          "${CMAKE_INSTALL_FULL_LIBDIR}")
set(includedir      "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
set(VERSION         "${PACKAGE_VERSION}")
# For init scripts and systemd service file (in contrib/)
set(bindir          "${CMAKE_INSTALL_FULL_BINDIR}")
set(sbindir         "${CMAKE_INSTALL_FULL_SBINDIR}")
foreach(name
   lib/libnghttp2.pc
   lib/includes/nghttp2/nghttp2ver.h
   integration-tests/config.go
   integration-tests/setenv
   doc/conf.py
   doc/index.rst
   doc/package_README.rst
   doc/tutorial-client.rst
   doc/tutorial-server.rst
   doc/tutorial-hpack.rst
   doc/nghttpx-howto.rst
   doc/h2load-howto.rst
   doc/building-android-binary.rst
   doc/nghttp2.h.rst
   doc/nghttp2ver.h.rst
   doc/contribute.rst
)
  configure_file("${name}.in" "${name}" @ONLY)
endforeach()

if(APPLE)
  add_definitions(-D__APPLE_USE_RFC_3542)
endif()

include_directories(
  "${CMAKE_CURRENT_BINARY_DIR}" # for config.h
)
# For use in src/CMakeLists.txt
set(PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/${CMAKE_PROJECT_NAME}")
set(PKGLIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}")

install(FILES README.rst DESTINATION "${CMAKE_INSTALL_DOCDIR}")

add_subdirectory(lib)
#add_subdirectory(lib/includes)
add_subdirectory(third-party)
add_subdirectory(src)
add_subdirectory(examples)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  add_subdirectory(tests)
  #add_subdirectory(tests/testdata)
  add_subdirectory(integration-tests)
endif()
if(ENABLE_DOC)
  add_subdirectory(doc)
endif()
add_subdirectory(contrib)
add_subdirectory(script)
add_subdirectory(bpf)


string(TOUPPER "${CMAKE_BUILD_TYPE}" _build_type)
message(STATUS "summary of build options:

    Package version: ${VERSION}
    Library version: ${LT_CURRENT}:${LT_REVISION}:${LT_AGE}
    Install prefix:  ${CMAKE_INSTALL_PREFIX}
    Target system:   ${CMAKE_SYSTEM_NAME}
    Compiler:
      Build type:     ${CMAKE_BUILD_TYPE}
      C compiler:     ${CMAKE_C_COMPILER}
      CFLAGS:         ${CMAKE_C_FLAGS_${_build_type}} ${CMAKE_C_FLAGS}
      C++ compiler:   ${CMAKE_CXX_COMPILER}
      CXXFLAGS:       ${CMAKE_CXX_FLAGS_${_build_type}} ${CMAKE_CXX_FLAGS}
      WARNCFLAGS:     ${WARNCFLAGS}
      CXX1XCXXFLAGS:  ${CXX1XCXXFLAGS}
      WARNCXXFLAGS:   ${WARNCXXFLAGS}
    Python:
      Python:          ${Python3_EXECUTABLE}
      Python3_VERSION: ${Python3_VERSION}
    Test:
      Failmalloc:     ${ENABLE_FAILMALLOC}
      Build Test:     ${BUILD_TESTING}
    Libs:
      OpenSSL:        ${HAVE_OPENSSL} (LIBS='${OPENSSL_LIBRARIES}')
      wolfSSL:        ${HAVE_WOLFSSL} (LIBS='${WOLFSSL_LIBRARIES}')
      Libxml2:        ${HAVE_LIBXML2} (LIBS='${LIBXML2_LIBRARIES}')
      Libev:          ${HAVE_LIBEV} (LIBS='${LIBEV_LIBRARIES}')
      Libc-ares:      ${HAVE_LIBCARES} (LIBS='${LIBCARES_LIBRARIES}')
      Libngtcp2:      ${HAVE_LIBNGTCP2} (LIBS='${LIBNGTCP2_LIBRARIES}')
      Libngtcp2_crypto_quictls: ${HAVE_LIBNGTCP2_CRYPTO_QUICTLS} (LIBS='${LIBNGTCP2_CRYPTO_QUICTLS_LIBRARIES}')
      Libngtcp2_crypto_wolfssl: ${HAVE_LIBNGTCP2_CRYPTO_WOLFSSL} (LIBS='${LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARIES}')
      Libnghttp3:     ${HAVE_LIBNGHTTP3} (LIBS='${LIBNGHTTP3_LIBRARIES}')
      Libbpf:         ${HAVE_LIBBPF} (LIBS='${LIBBPF_LIBRARIES}')
      Libevent(SSL):  ${HAVE_LIBEVENT_OPENSSL} (LIBS='${LIBEVENT_OPENSSL_LIBRARIES}')
      Jansson:        ${HAVE_JANSSON} (LIBS='${JANSSON_LIBRARIES}')
      Jemalloc:       ${HAVE_JEMALLOC} (LIBS='${JEMALLOC_LIBRARIES}')
      Zlib:           ${HAVE_ZLIB} (LIBS='${ZLIB_LIBRARIES}')
      Systemd:        ${HAVE_SYSTEMD} (LIBS='${SYSTEMD_LIBRARIES}')
      Libbrotlienc:   ${HAVE_LIBBROTLIENC} (LIBS='${LIBBROTLIENC_LIBRARIES}')
      Libbrotlidec:   ${HAVE_LIBBROTLIDEC} (LIBS='${LIBBROTLIDEC_LIBRARIES}')
    Third-party:
      http-parser:    ${ENABLE_THIRD_PARTY}
      MRuby:          ${HAVE_MRUBY}
      Neverbleed:     ${HAVE_NEVERBLEED}
    Features:
      Applications:   ${ENABLE_APP}
      HPACK tools:    ${ENABLE_HPACK_TOOLS}
      Examples:       ${ENABLE_EXAMPLES}
      Threading:      ${ENABLE_THREADS}
      HTTP/3(EXPERIMENTAL): ${ENABLE_HTTP3}
")
if(ENABLE_LIB_ONLY_DISABLED_OTHERS)
  message("Only the library will be built. To build other components "
    "(such as applications and examples), set ENABLE_LIB_ONLY=OFF.")
endif()
