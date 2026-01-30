# nghttp2 - HTTP/2 C Library

# Copyright (c) 2015 Tatsuhiro Tsujikawa

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

GO_FILES = \
	nghttpx_http1_test.go \
	nghttpx_http2_test.go \
	nghttpx_http3_test.go \
	server_tester.go \
	server_tester_http3.go

EXTRA_DIST = \
	CMakeLists.txt \
	$(GO_FILES) \
	server.key \
	server.crt \
	alt-server.key \
	alt-server.crt \
	setenv \
	req-set-header.rb \
	resp-set-header.rb \
	req-return.rb \
	resp-return.rb

GO_TEST_TAGS =

if ENABLE_HTTP3
GO_TEST_TAGS += quic
endif # ENABLE_HTTP3

it:
	for i in $(GO_FILES); do [ -e $(builddir)/$$i ] || cp $(srcdir)/$$i $(builddir); done
	sh setenv go test -v --tags=${GO_TEST_TAGS}
