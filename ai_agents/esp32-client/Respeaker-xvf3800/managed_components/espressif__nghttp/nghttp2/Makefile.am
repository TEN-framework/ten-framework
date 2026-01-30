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
SUBDIRS = lib third-party src bpf examples tests integration-tests \
	doc contrib script

ACLOCAL_AMFLAGS = -I m4

dist_doc_DATA = README.rst

EXTRA_DIST = nghttpx.conf.sample proxy.pac.sample android-config android-env \
	Dockerfile.android \
	cmakeconfig.h.in \
	CMakeLists.txt \
	CMakeOptions.txt \
	cmake/ExtractValidFlags.cmake \
	cmake/FindJemalloc.cmake \
	cmake/FindLibev.cmake \
	cmake/Version.cmake \
	cmake/FindLibevent.cmake \
	cmake/FindJansson.cmake \
	cmake/FindLibcares.cmake \
	cmake/FindSystemd.cmake \
	cmake/FindLibbpf.cmake \
	cmake/FindLibnghttp3.cmake \
	cmake/FindLibngtcp2.cmake \
	cmake/FindLibngtcp2_crypto_quictls.cmake \
	cmake/FindLibbrotlienc.cmake \
	cmake/FindLibbrotlidec.cmake \
	cmake/FindLibngtcp2_crypto_wolfssl.cmake \
	cmake/FindWolfSSL.cmake \
	cmake/PickyWarningsC.cmake \
	cmake/PickyWarningsCXX.cmake

.PHONY: clang-format

# Format source files using clang-format.  Don't format source files
# under third-party directory since we are not responsible for their
# coding style.
clang-format:
	CLANGFORMAT=`git config --get clangformat.binary`; \
	test -z $${CLANGFORMAT} && CLANGFORMAT="clang-format"; \
	$${CLANGFORMAT} -i lib/*.{c,h} lib/includes/nghttp2/*.h \
	src/*.{c,cc,h} examples/*.c \
	tests/*.{c,h} bpf/*.c fuzz/*.cc
