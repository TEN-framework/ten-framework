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

set(LWS_GLIB_INCLUDE_DIRS CACHE PATH "Path to the glib include directory")
set(LWS_GLIB_LIBRARIES CACHE PATH "Path to the glib library")

include (FindPkgConfig)
if (NOT GLIB_FOUND)
	find_path(GLIB_INCLUDE_DIRS NAMES glib-2.0/glib.h)
	find_library(GLIB_LIBRARIES NAMES glib-2.0)
	if (GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
		set(GLIB_FOUND)
	endif()
	if (GLIB_INCLUDE_DIRS)
		set(GLIB_INCLUDE_DIRS "${GLIB_INCLUDE_DIRS}/glib-2.0" PARENT_SCOPE)
	endif()
endif()
PKG_SEARCH_MODULE(LWS_GLIB2 glib-2.0)
if (LWS_GLIB2_FOUND)
	list(APPEND GLIB_INCLUDE_DIRS "${LWS_GLIB2_INCLUDE_DIRS}")
endif()

message("glib include dir: ${GLIB_INCLUDE_DIRS}")
message("glib libraries: ${GLIB_LIBRARIES}")
include_directories("${GLIB_INCLUDE_DIRS}")

if (LWS_WITH_EVLIB_PLUGINS)

	create_evlib_plugin(evlib_glib
			    glib.c
			    private-lib-event-libs-glib.h
			    ${GLIB_LIBRARIES})

else()

	list(APPEND LIB_LIST ${GLIB_LIBRARIES})
	
	if (LWS_WITH_NETWORK)
		list(APPEND SOURCES
			event-libs/glib/glib.c)
	endif()

endif()
#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
