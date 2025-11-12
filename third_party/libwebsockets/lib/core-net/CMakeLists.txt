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

include_directories(.)

list(APPEND SOURCES
	core-net/dummy-callback.c
	core-net/output.c
	core-net/close.c
	core-net/network.c
	core-net/vhost.c
	core-net/pollfd.c
	core-net/service.c
	core-net/sorted-usec-list.c
	core-net/wsi.c
	core-net/wsi-timeout.c
	core-net/adopt.c
	roles/pipe/ops-pipe.c
)

if (LWS_WITH_SYS_STATE)
	list(APPEND SOURCES
		core-net/state.c
	)
endif()

if (LWS_WITH_NETLINK)
	list(APPEND SOURCES
		core-net/route.c
	)
endif()

if (LWS_WITH_LWS_DSH)
	list(APPEND SOURCES
		core-net/lws-dsh.c)
endif()

if (LWS_WITH_SEQUENCER)
	list(APPEND SOURCES
		core-net/sequencer.c)
endif()

if (LWS_WITH_CLIENT)
	list(APPEND SOURCES
		core-net/client/client.c
		core-net/client/connect.c
		core-net/client/connect2.c
		core-net/client/connect3.c
		core-net/client/connect4.c
		core-net/client/sort-dns.c
	)
	if (LWS_WITH_CONMON)
		list(APPEND SOURCES
			core-net/client/conmon.c
		)
	endif()
endif()

if (LWS_WITH_SOCKS5 AND NOT LWS_WITHOUT_CLIENT)
	list(APPEND SOURCES
		core-net/socks5-client.c)
endif()

exports_to_parent_scope()
