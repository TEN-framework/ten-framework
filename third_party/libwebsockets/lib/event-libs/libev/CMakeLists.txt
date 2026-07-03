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

set(LWS_LIBEV_LIBRARIES CACHE PATH "Path to the libev library")
set(LWS_LIBEV_INCLUDE_DIRS CACHE PATH "Path to the libev include directory")

if (NOT LIBEV_FOUND)
	find_path(LIBEV_INCLUDE_DIRS NAMES ev.h)
	find_library(LIBEV_LIBRARIES NAMES ev)
endif()
message("libev include dir: ${LIBEV_INCLUDE_DIRS}")
message("libev libraries: ${LIBEV_LIBRARIES}")
include_directories("${LIBEV_INCLUDE_DIRS}")

if ("${LWS_LIBEV_LIBRARIES}" STREQUAL "" OR "${LWS_LIBEV_INCLUDE_DIRS}" STREQUAL "")
else()
	set(LIBEV_LIBRARIES ${LWS_LIBEV_LIBRARIES})
	set(LIBEV_INCLUDE_DIRS ${LWS_LIBEV_INCLUDE_DIRS})
endif()

if (LWS_WITH_EVLIB_PLUGINS)

	create_evlib_plugin(
		evlib_ev
		libev.c
		private-lib-event-libs-libev.h
		${LIBEV_LIBRARIES})

else()

	list(APPEND LIB_LIST ${LIBEV_LIBRARIES})

	list(APPEND SOURCES
			event-libs/libev/libev.c)
# see README.build.md for discussion of why of the supported event libs,
# only libev cannot cope with -Werror
	set_source_files_properties(event-libs/libev/libev.c
				    PROPERTIES COMPILE_FLAGS "-Wno-error" )
endif()

set(CMAKE_REQUIRED_LIBRARIES ${LIB_LIST})

CHECK_C_SOURCE_COMPILES(
	"#include <ev.h>
	 int main(int argc, char **argv) { return EVBACKEND_LINUXAIO; }
	" LWS_HAVE_EVBACKEND_LINUXAIO)

CHECK_C_SOURCE_COMPILES(
	"#include <ev.h>
	 int main(int argc, char **argv) { return EVBACKEND_IOURING; }
	 " LWS_HAVE_EVBACKEND_IOURING)

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(LWS_HAVE_EVBACKEND_LINUXAIO ${LWS_HAVE_EVBACKEND_LINUXAIO} PARENT_SCOPE)
set(LWS_HAVE_EVBACKEND_IOURING ${LWS_HAVE_EVBACKEND_IOURING} PARENT_SCOPE)
