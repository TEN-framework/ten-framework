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
#  - includes via include_directories
#
# and keep everything else private

include_directories(.)

set(LWS_LIBEVENT_INCLUDE_DIRS CACHE PATH "Path to the libevent include directory")
set(LWS_LIBEVENT_LIBRARIES CACHE PATH "Path to the libevent library")

if (NOT LIBEVENT_FOUND)
	find_path(LIBEVENT_INCLUDE_DIRS NAMES event2/event.h)
	find_library(LIBEVENT_LIBRARIES NAMES event)
endif()
message("libevent include dir: ${LIBEVENT_INCLUDE_DIRS}")
message("libevent libraries: ${LIBEVENT_LIBRARIES}")
include_directories("${LIBEVENT_INCLUDE_DIRS}")

if ("${LWS_LIBEVENT_LIBRARIES}" STREQUAL "" OR "${LWS_LIBEVENT_INCLUDE_DIRS}" STREQUAL "")
else()
	set(LIBEVENT_LIBRARIES ${LWS_LIBEVENT_LIBRARIES})
	set(LIBEVENT_INCLUDE_DIRS ${LWS_LIBEVENT_INCLUDE_DIRS})
endif()


if (LWS_WITH_EVLIB_PLUGINS)

	create_evlib_plugin(evlib_event
			    libevent.c
			    private-lib-event-libs-libevent.h
			    ${LIBEVENT_LIBRARIES})

else()

	list(APPEND LIB_LIST ${LIBEVENT_LIBRARIES})
	set(LIBEVENT_FOUND 1 PARENT_SCOPE)
	if (LWS_WITH_NETWORK)
		list(APPEND SOURCES
			event-libs/libevent/libevent.c)
	endif()
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
