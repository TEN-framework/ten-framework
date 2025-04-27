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
	roles/dbus/dbus.c)
	
if (NOT LWS_DBUS_LIB)
	set(LWS_DBUS_LIB "dbus-1")
endif()

find_package(PkgConfig QUIET)
pkg_check_modules(PC_DBUS1 dbus-1 QUIET)
list(APPEND LWS_DBUS_INCLUDE1 ${PC_DBUS1_INCLUDE_DIRS})
list(APPEND LWS_DBUS_LIB ${PC_DBUS1_LIBRARIES})
list(APPEND LWS_DEPS_LIB_PATHS ${PC_DBUS1_LIBRARY_DIRS})

set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES};${LWS_DBUS_INCLUDE1})

CHECK_C_SOURCE_COMPILES("#include <dbus/dbus.h>
int main(void) {
	return 0;
}" LWS_DBUS_CHECK_OK)

message("dbus include dir 1: ${LWS_DBUS_INCLUDE1}")
if (LWS_DBUS_INCLUDE1)
include_directories("${LWS_DBUS_INCLUDE1}")
endif()
list(APPEND LIB_LIST ${LWS_DBUS_LIB})

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(LWS_DBUS_CHECK_OK ${LWS_DBUS_CHECK_OK} PARENT_SCOPE)
set(LWS_DEPS_LIB_PATHS ${LWS_DEPS_LIB_PATHS} PARENT_SCOPE)

