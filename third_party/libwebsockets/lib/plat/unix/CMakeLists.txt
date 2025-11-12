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

include_directories(.)

execute_process( COMMAND grep -c illumos /lib/ld.so.1 
		 OUTPUT_VARIABLE ILLUMOS ERROR_QUIET )
# Chomp the \n at end of output.
string(REGEX REPLACE "[\n]+" "" ILLUMOS "${ILLUMOS}")

if (NOT ${ILLUMOS} MATCHES "0")
	set(ILLUMOS 1)
endif()

set(LWS_PLAT_UNIX 1)
list(APPEND SOURCES
	plat/unix/unix-caps.c
	plat/unix/unix-misc.c
	plat/unix/unix-init.c
)
if (LWS_WITH_FILE_OPS)
	list(APPEND SOURCES plat/unix/unix-file.c)
endif()
if (LWS_WITH_NETWORK)
	list(APPEND SOURCES
		plat/unix/unix-pipe.c
		plat/unix/unix-service.c
		plat/unix/unix-sockets.c
		plat/unix/unix-fds.c
	)
	if (LWS_WITH_SYS_ASYNC_DNS)
		if (LWS_PLAT_ANDROID)
			list(APPEND SOURCES plat/unix/android/android-resolv.c)
		else()
			list(APPEND SOURCES plat/unix/unix-resolv.c)
		endif()
	endif()
endif()
	
if (LWS_WITH_PLUGINS_API)
	list(APPEND SOURCES plat/unix/unix-plugins.c)
endif()

if (LWS_WITH_SPAWN)
	list(APPEND SOURCES plat/unix/unix-spawn.c)
endif()

if (HAIKU)
	set(CMAKE_REQUIRED_LIBRARIES network)
	list(APPEND LIB_LIST_AT_END network)
endif()

IF (CMAKE_SYSTEM_NAME STREQUAL Linux AND NOT LWS_WITHOUT_EVENTFD)
	CHECK_FUNCTION_EXISTS(eventfd_read LWS_HAVE_EVENTFD)
endif()

list(APPEND LIB_LIST_AT_END m)

if (ILLUMOS)
	list(APPEND LIB_LIST_AT_END socket)
endif()

if (LWS_HAVE_LIBCAP)
	list(APPEND LIB_LIST_AT_END cap)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
	list(APPEND LIB_LIST_AT_END socket)
endif()

list(APPEND LIB_LIST_AT_END ${CMAKE_DL_LIBS})

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(LWS_PLAT_UNIX ${LWS_PLAT_UNIX} PARENT_SCOPE)
set(ILLUMOS ${ILLUMOS} PARENT_SCOPE)
set(LIB_LIST_AT_END ${LIB_LIST_AT_END} PARENT_SCOPE)
set(LWS_PLAT_UNIX ${LWS_PLAT_UNIX} PARENT_SCOPE)
