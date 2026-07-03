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
# The strategy is to only export to PARENT_SCOPE
#
#  - changes to LIB_LIST
#  - changes to SOURCES
#  - includes via include_directories
#
# and keep everything else private

include_directories(wrapper/include wrapper/include/internal)

 	set(LWS_WITH_SSL ON)
 	
	include_directories(wrapper/include)
	include_directories(wrapper/include/platform)
	include_directories(wrapper/include/internal)
	include_directories(wrapper/include/openssl)
 	
 	if (LWS_WITH_NETWORK)
		list(APPEND HDR_PRIVATE
			tls/mbedtls/wrapper/include/internal/ssl3.h
			tls/mbedtls/wrapper/include/internal/ssl_cert.h
			tls/mbedtls/wrapper/include/internal/ssl_code.h
			tls/mbedtls/wrapper/include/internal/ssl_dbg.h
			tls/mbedtls/wrapper/include/internal/ssl_lib.h
			tls/mbedtls/wrapper/include/internal/ssl_methods.h
			tls/mbedtls/wrapper/include/internal/ssl_pkey.h
			tls/mbedtls/wrapper/include/internal/ssl_stack.h
			tls/mbedtls/wrapper/include/internal/ssl_types.h
			tls/mbedtls/wrapper/include/internal/ssl_x509.h
			tls/mbedtls/wrapper/include/internal/tls1.h
			tls/mbedtls/wrapper/include/internal/x509_vfy.h)
	
		list(APPEND HDR_PRIVATE
			tls/mbedtls/wrapper/include/openssl/ssl.h)
	
		list(APPEND HDR_PRIVATE
			tls/mbedtls/wrapper/include/platform/ssl_pm.h
			tls/mbedtls/wrapper/include/platform/ssl_port.h)
	
		list(APPEND SOURCES
			tls/mbedtls/wrapper/library/ssl_cert.c
			tls/mbedtls/wrapper/library/ssl_lib.c
			tls/mbedtls/wrapper/library/ssl_methods.c
			tls/mbedtls/wrapper/library/ssl_pkey.c
			tls/mbedtls/wrapper/library/ssl_stack.c
			tls/mbedtls/wrapper/library/ssl_x509.c)
	
		list(APPEND SOURCES
			tls/mbedtls/wrapper/platform/ssl_pm.c
			tls/mbedtls/wrapper/platform/ssl_port.c)
	endif()

	set(_WANT_MBT 0)
	if (NOT LWS_PLAT_FREERTOS)
		if (NOT DEFINED LWS_MBEDTLS_LIBRARIES)
			set(_WANT_MBT 1)
		endif()
		if (NOT DEFINED LWS_MBEDTLS_INCLUDE_DIRS)
			set(_WANT_MBT 1)
		endif()
	endif()

	if (_WANT_MBT)

		find_path(LWS_MBEDTLS_INCLUDE_DIRS mbedtls/ssl.h)

		find_library(MBEDTLS_LIBRARY mbedtls)
		find_library(MBEDX509_LIBRARY mbedx509)
		find_library(MBEDCRYPTO_LIBRARY mbedcrypto)

		set(LWS_MBEDTLS_LIBRARIES "${MBEDTLS_LIBRARY}" "${MBEDX509_LIBRARY}" "${MBEDCRYPTO_LIBRARY}")

		include(FindPackageHandleStandardArgs)
		find_package_handle_standard_args(MBEDTLS DEFAULT_MSG
			LWS_MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARY MBEDX509_LIBRARY MBEDCRYPTO_LIBRARY)

		mark_as_advanced(LWS_MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARY MBEDX509_LIBRARY MBEDCRYPTO_LIBRARY)

		if ("${LWS_MBEDTLS_LIBRARIES}" STREQUAL "" OR "${LWS_MBEDTLS_INCLUDE_DIRS}" STREQUAL "")
			message(FATAL_ERROR "You must set LWS_MBEDTLS_LIBRARIES and LWS_MBEDTLS_INCLUDE_DIRS when LWS_WITH_MBEDTLS is turned on.")
		endif()
	endif()
	if (LWS_MBEDTLS_LIBRARIES)
		set(MBEDTLS_LIBRARIES ${LWS_MBEDTLS_LIBRARIES})
		set(MBEDTLS_LIBRARIES ${LWS_MBEDTLS_LIBRARIES} PARENT_SCOPE)
	endif()
	if (LWS_MBEDTLS_INCLUDE_DIRS)
		set(MBEDTLS_INCLUDE_DIRS ${LWS_MBEDTLS_INCLUDE_DIRS})
		set(MBEDTLS_INCLUDE_DIRS ${LWS_MBEDTLS_INCLUDE_DIRS} PARENT_SCOPE)
	endif()
	set(USE_MBEDTLS 1 PARENT_SCOPE)
	if (DEFINED MBEDTLS_INCLUDE_DIRS)
		include_directories(${MBEDTLS_INCLUDE_DIRS})
		set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${MBEDTLS_INCLUDE_DIRS})
	endif()

	if (DEFINED MBEDTLS_LIBRARIES)
		list(APPEND LIB_LIST ${MBEDTLS_LIBRARIES})
	endif()

# old mbedtls has everything in mbedtls/net.h

CHECK_C_SOURCE_COMPILES("#include <mbedtls/net_sockets.h>\nint main(void) { return 0;}\n" LWS_HAVE_MBEDTLS_NET_SOCKETS)

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(LWS_HAVE_MBEDTLS_NET_SOCKETS ${LWS_HAVE_MBEDTLS_NET_SOCKETS} PARENT_SCOPE)
