#!/bin/sh
#
# nghttp2 - HTTP/2 C Library
#
# Copyright (c) 2013 Tatsuhiro Tsujikawa
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

. ./android-env

./configure \
    --disable-shared \
    --host=$TARGET \
    --build=`dpkg-architecture -qDEB_BUILD_GNU_TYPE` \
    --without-libxml2 \
    --disable-examples \
    --disable-threads \
    CPPFLAGS="-fPIE -I$PREFIX/include" \
    PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" \
    LDFLAGS="-fPIE -pie -L$PREFIX/lib"
