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

include_directories(. ./compression)

list(APPEND SOURCES
	roles/http/header.c
	roles/http/date.c
	roles/http/parsers.c)

if (NOT LWS_WITHOUT_SERVER)
	list(APPEND SOURCES
		roles/http/server/server.c
		roles/http/server/lws-spa.c)
endif()

if (LWS_WITH_CACHE_NSCOOKIEJAR AND LWS_WITH_CLIENT)
	list(APPEND SOURCES
		roles/http/cookie.c)
endif()

if (LWS_WITH_HTTP_PROXY AND LWS_WITH_HUBBUB)
	list(APPEND SOURCES
		roles/http/server/rewrite.c)
endif()

if (LWS_WITH_ACCESS_LOG)
	list(APPEND SOURCES
		roles/http/server/access-log.c)
endif()
	
if (LWS_WITH_HTTP_STREAM_COMPRESSION)
	list(APPEND SOURCES
		roles/http/compression/stream.c
		roles/http/compression/deflate/deflate.c)

	if (LWS_WITH_HTTP_BROTLI)
		list(APPEND SOURCES
			roles/http/compression/brotli/brotli.c)
		list(APPEND LIB_LIST brotlienc brotlidec brotlidec)
	endif()
endif()

if (LWS_WITH_LEJP_CONF AND LWS_WITH_NETWORK AND NOT LWS_PLAT_OPTEE)
	list(APPEND SOURCES
		roles/http/server/lejp-conf.c
	)
endif()

if (LWS_WITH_RANGES)
	list(APPEND SOURCES
		roles/http/server/ranges.c)
endif()

if (LWS_WITH_ZIP_FOPS)
       if (LWS_WITH_ZLIB)
               list(APPEND SOURCES
                       roles/http/server/fops-zip.c)
       else()
               message(FATAL_ERROR "Pre-zipped file support (LWS_WITH_ZIP_FOPS) requires ZLIB (LWS_WITH_ZLIB)")
       endif()
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
