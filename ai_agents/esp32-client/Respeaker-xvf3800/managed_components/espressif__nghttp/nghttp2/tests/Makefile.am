# nghttp2 - HTTP/2 C Library

# Copyright (c) 2012 Tatsuhiro Tsujikawa

# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:

# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
SUBDIRS = testdata

EXTRA_DIST = CMakeLists.txt munit/COPYING

check_PROGRAMS = main

if ENABLE_FAILMALLOC
check_PROGRAMS += failmalloc
endif # ENABLE_FAILMALLOC

OBJECTS = main.c nghttp2_pq_test.c nghttp2_map_test.c nghttp2_queue_test.c \
	nghttp2_test_helper.c \
	nghttp2_frame_test.c \
	nghttp2_stream_test.c \
	nghttp2_session_test.c \
	nghttp2_hd_test.c \
	nghttp2_alpn_test.c \
	nghttp2_helper_test.c \
	nghttp2_buf_test.c \
	nghttp2_http_test.c \
	nghttp2_extpri_test.c \
	nghttp2_ratelim_test.c \
	munit/munit.c

HFILES = nghttp2_pq_test.h nghttp2_map_test.h nghttp2_queue_test.h \
	nghttp2_session_test.h \
	nghttp2_frame_test.h nghttp2_stream_test.h nghttp2_hd_test.h \
	nghttp2_alpn_test.h nghttp2_helper_test.h \
	nghttp2_assertion.h \
	nghttp2_test_helper.h \
	nghttp2_buf_test.h \
	nghttp2_http_test.h \
	nghttp2_extpri_test.h \
	nghttp2_ratelim_test.h \
	munit/munit.h

main_SOURCES = $(HFILES) $(OBJECTS)

if ENABLE_STATIC
main_LDADD = ${top_builddir}/lib/libnghttp2.la
else
# With static lib disabled and symbol hiding enabled, we have to link object
# files directly because the tests use symbols not included in public API.
main_LDADD = ${top_builddir}/lib/.libs/*.o
endif

main_LDADD += @TESTLDADD@
main_LDFLAGS = -static

if ENABLE_FAILMALLOC
failmalloc_SOURCES = failmalloc.c failmalloc_test.c failmalloc_test.h \
	malloc_wrapper.c malloc_wrapper.h \
	nghttp2_test_helper.c nghttp2_test_helper.h \
	munit/munit.c munit/munit.h
failmalloc_LDADD = $(main_LDADD)
failmalloc_LDFLAGS = $(main_LDFLAGS)
endif # ENABLE_FAILMALLOC

AM_CFLAGS = $(WARNCFLAGS) \
	-I${top_srcdir}/lib \
	-I${top_srcdir}/lib/includes \
	-I${top_srcdir}/tests/munit \
	-I${top_builddir}/lib/includes \
	-DBUILDING_NGHTTP2 \
	-DNGHTTP2_STATICLIB \
	@DEFS@

TESTS = main

if ENABLE_FAILMALLOC
TESTS += failmalloc
endif # ENABLE_FAILMALLOC
