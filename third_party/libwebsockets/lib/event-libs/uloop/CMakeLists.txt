#
# libwebsockets - small server side websockets and web server implementation
#
# Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
#  - includes via include_directories
#
# and keep everything else private

include_directories(.)

set(LWS_ULOOP_INCLUDE_DIRS CACHE PATH "Path to the libubox / uloop.h include directory")
set(LWS_ULOOP_LIBRARIES CACHE PATH "Path to the libubox library")

if (NOT ULOOP_FOUND)
	find_path(ULOOP_INCLUDE_DIRS NAMES libubox/uloop.h)
	find_library(ULOOP_LIBRARIES NAMES ubox)
endif()
message("libubox include dir: ${ULOOP_INCLUDE_DIRS}")
message("libubox libraries: ${ULOOP_LIBRARIES}")
include_directories("${ULOOP_INCLUDE_DIRS}")

if ("${LWS_ULOOP_LIBRARIES}" STREQUAL "" OR "${LWS_ULOOP_INCLUDE_DIRS}" STREQUAL "")
else()
	set(ULOOP_LIBRARIES ${LWS_ULOOP_LIBRARIES})
	set(ULOOP_INCLUDE_DIRS ${LWS_ULOOP_INCLUDE_DIRS})
endif()


if (LWS_WITH_EVLIB_PLUGINS)

	create_evlib_plugin(evlib_uloop
			    uloop.c
			    private-lib-event-libs-uloop.h
			    ${ULOOP_LIBRARIES})

else()

	list(APPEND LIB_LIST ${ULOOP_LIBRARIES})
	set(ULOOP_FOUND 1 PARENT_SCOPE)
	if (LWS_WITH_NETWORK)
		list(APPEND SOURCES
			event-libs/uloop/uloop.c)
	endif()
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
