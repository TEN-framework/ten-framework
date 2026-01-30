# urlparse
#
# Copyright (c) 2024 urlparse contributors
# Copyright (c) 2023 sfparse contributors
# Copyright (c) 2019 nghttp3 contributors
# Copyright (c) 2016 ngtcp2 contributors
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
SUBDIRS = doc

EXTRA_DIST = munit/COPYING

ACLOCAL_AMFLAGS = -I m4

# Format source files using clang-format.  Don't format source files
# under third-party directory since we are not responsible for their
# coding style.
clang-format:
	CLANGFORMAT=`git config --get clangformat.binary`; \
	test -z $${CLANGFORMAT} && CLANGFORMAT="clang-format"; \
	$${CLANGFORMAT} -i *.{c,h} fuzz/*.cc

lib_LTLIBRARIES = liburlparse.la

liburlparse_la_SOURCES = urlparse.c urlparse.h
liburlparse_la_CFLAGS = $(WARNCFLAGS)
liburlparse_la_CPPFLAGS = -I$(srcdir)
liburlparse_la_LDFLAGS = -no-undefined \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

include_HEADERS = urlparse.h

noinst_PROGRAMS = examples

examples_SOURCES = examples.c
examples_LDADD = $(top_builddir)/liburlparse.la

check_LTLIBRARIES = libhttp-parser.la
libhttp_parser_la_SOURCES = \
	http-parser/http_parser.c \
	http-parser/http_parser.h

check_PROGRAMS = test

test_SOURCES = urlparse_test_main.c urlparse_test.c urlparse_test.h \
	http_parser_compat_test.c http_parser_compat_test.h \
	munit/munit.c munit/munit.h

test_CFLAGS = $(WARNCFLAGS) -I$(top_srcdir)
test_CPPFLAGS = -I$(srcdir) -I$(srcdir)/munit
test_LDADD = $(top_builddir)/liburlparse.la $(top_builddir)/libhttp-parser.la
test_LDFLAGS = -static

TESTS = test
