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
	misc/base64-decode.c
	misc/prng.c
	misc/lws-ring.c)

if (LWS_WITH_NETWORK)
	list(APPEND SOURCES
		misc/cache-ttl/lws-cache-ttl.c
		misc/cache-ttl/heap.c
		)

	if (LWS_WITH_CACHE_NSCOOKIEJAR)
		list(APPEND SOURCES
			misc/cache-ttl/file.c)
	endif()

endif()

if (LWS_WITH_FTS)
	list(APPEND SOURCES
		misc/fts/trie.c
		misc/fts/trie-fd.c)
endif()

# this is an older, standalone hashed disk cache
# implementation unrelated to lws-cache-ttl
if (LWS_WITH_DISKCACHE)
	list(APPEND SOURCES
		misc/diskcache.c)
endif()

if (LWS_WITH_STRUCT_JSON)
	list(APPEND SOURCES
		misc/lws-struct-lejp.c)
endif()

if (LWS_WITH_STRUCT_SQLITE3)
	list(APPEND SOURCES
		misc/lws-struct-sqlite.c)
endif()

if (LWS_WITH_FSMOUNT AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	list(APPEND SOURCES misc/fsmount.c)
endif()

if (LWS_WITH_DIR)
	list(APPEND SOURCES misc/dir.c)
endif()

if (LWS_WITH_THREADPOOL AND LWS_HAVE_PTHREAD_H)
	list(APPEND SOURCES misc/threadpool/threadpool.c)
endif()

if (LWS_WITH_PEER_LIMITS)
	list(APPEND SOURCES
		misc/peer-limits.c)
endif()

if (LWS_WITH_LWSAC)
	list(APPEND SOURCES
		misc/lwsac/lwsac.c)
	if (NOT LWS_PLAT_FREERTOS)
		list(APPEND SOURCES
			misc/lwsac/cached-file.c)
	endif()
	if (LWS_WITH_SECURE_STREAMS_CPP)
		list(APPEND SOURCES misc/lwsac/lwsac.cxx)
	endif()
endif()

if (NOT LWS_WITHOUT_BUILTIN_SHA1)
	list(APPEND SOURCES
		misc/sha-1.c)
endif()

if (LWS_WITH_LEJP)
	list(APPEND SOURCES
		misc/lejp.c)
endif()
if (LWS_WITH_CBOR)
	list(APPEND SOURCES
		misc/lecp.c
		misc/ieeehalfprecision.c)
endif()


if (UNIX)
	if (NOT LWS_HAVE_GETIFADDRS)
		list(APPEND HDR_PRIVATE misc/getifaddrs.h)
		list(APPEND SOURCES misc/getifaddrs.c)
	endif()
endif()

if (NOT WIN32 AND NOT LWS_WITHOUT_DAEMONIZE)
	list(APPEND SOURCES
		misc/daemonize.c)
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
