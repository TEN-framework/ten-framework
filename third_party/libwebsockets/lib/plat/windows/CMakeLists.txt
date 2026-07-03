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

list(APPEND SOURCES
	plat/windows/windows-fds.c
	plat/windows/windows-file.c
	plat/windows/windows-init.c
	plat/windows/windows-misc.c
	plat/windows/windows-pipe.c
	plat/windows/windows-plugins.c
	plat/windows/windows-service.c
	plat/windows/windows-sockets.c
	)
if (LWS_WITH_SYS_ASYNC_DNS)
	list(APPEND SOURCES plat/windows/windows-resolv.c)
endif()

if (LWS_WITH_SPAWN)
	list(APPEND SOURCES plat/windows/windows-spawn.c)
endif()

if (LWS_WITH_ZLIB AND LWS_WITH_BUNDLED_ZLIB)
       set(WIN32_ZLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../win32port/zlib")
	set(ZLIB_SRCS
		${WIN32_ZLIB_PATH}/adler32.c
		${WIN32_ZLIB_PATH}/compress.c
		${WIN32_ZLIB_PATH}/crc32.c
		${WIN32_ZLIB_PATH}/deflate.c
		${WIN32_ZLIB_PATH}/gzlib.c
		${WIN32_ZLIB_PATH}/gzread.c
		${WIN32_ZLIB_PATH}/gzwrite.c
		${WIN32_ZLIB_PATH}/infback.c
		${WIN32_ZLIB_PATH}/inffast.c
		${WIN32_ZLIB_PATH}/inflate.c
		${WIN32_ZLIB_PATH}/inftrees.c
		${WIN32_ZLIB_PATH}/trees.c
		${WIN32_ZLIB_PATH}/uncompr.c
		${WIN32_ZLIB_PATH}/zutil.c)
	add_library(zlib_internal STATIC ${ZLIB_SRCS})
	set(ZLIB_INCLUDE_DIRS ${WIN32_ZLIB_PATH})
       set(ZLIB_LIBRARIES "")
	set(ZLIB_FOUND 1)
	# Make sure zlib_internal is compiled before the libs.
	foreach (lib ${LWS_LIBRARIES})
		add_dependencies(${lib} zlib_internal)
	endforeach()
endif()

# Add helper files for Windows

#  (from ./lib perspective)
set(WIN32_HELPERS_PATH ../win32port/win32helpers)

# from our perspective in ./lib/plat/windows
include_directories(../../${WIN32_HELPERS_PATH})

list(APPEND SOURCES
	${WIN32_HELPERS_PATH}/gettimeofday.c
)

list(APPEND HDR_PRIVATE
	${WIN32_HELPERS_PATH}/gettimeofday.h
)

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(WIN32_HELPERS_PATH ${WIN32_HELPERS_PATH} PARENT_SCOPE)
set(HDR_PRIVATE ${HDR_PRIVATE} PARENT_SCOPE)
set(ZLIB_FOUND ${ZLIB_FOUND} PARENT_SCOPE)
set(LIB_LIST_AT_END ${LIB_LIST_AT_END} PARENT_SCOPE)
