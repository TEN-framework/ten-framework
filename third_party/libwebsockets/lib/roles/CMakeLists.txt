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

if (LWS_ROLE_MQTT)
	add_subdir_include_directories(mqtt)
endif()

if (LWS_ROLE_DBUS AND NOT LWS_PLAT_FREERTOS)
	add_subdir_include_directories(dbus)
endif()

if (LWS_ROLE_H1 OR LWS_ROLE_H2)
	add_subdir_include_directories(http)
endif()

if (LWS_ROLE_H1)
	add_subdir_include_directories(h1)
endif()

if (LWS_ROLE_H2)
	add_subdir_include_directories(h2)
endif()

if (LWS_ROLE_WS)
	add_subdir_include_directories(ws)
endif()

if (LWS_ROLE_RAW)
	add_subdir_include_directories(raw-skt)
endif()

if (LWS_ROLE_RAW_FILE)
	add_subdir_include_directories(raw-file)
endif()

if (LWS_WITH_CGI)
	add_subdir_include_directories(cgi)
endif()

if (LWS_ROLE_RAW_PROXY)
	add_subdir_include_directories(raw-proxy)
endif()

if (NOT LWS_WITHOUT_SERVER OR LWS_WITH_SECURE_STREAMS_PROCESS_API)
	add_subdir_include_directories(listen)
endif()

if (LWS_WITH_CLIENT AND (LWS_ROLE_H1 OR LWS_ROLE_H2))
	list(APPEND SOURCES
		roles/http/client/client-http.c)
endif()

if (LWS_WITH_NETLINK)
	list(APPEND SOURCES roles/netlink/ops-netlink.c)
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(LWS_DEPS_LIB_PATHS ${LWS_DEPS_LIB_PATHS} PARENT_SCOPE)

