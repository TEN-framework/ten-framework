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

EXTRA_DIST = \
	CMakeLists.txt \
	test.example.com.pem \
	test.nghttp2.org.pem

bin_PROGRAMS =
check_PROGRAMS =
TESTS =

AM_CFLAGS = $(WARNCFLAGS)
AM_CXXFLAGS = $(WARNCXXFLAGS) $(CXX1XCXXFLAGS)
AM_CPPFLAGS = \
	-DPKGDATADIR='"$(pkgdatadir)"' \
	-DPKGLIBDIR='"$(pkglibdir)"' \
	-I$(top_srcdir)/lib/includes \
	-I$(top_builddir)/lib/includes \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/third-party/urlparse \
	-I$(top_srcdir)/third-party/llhttp/include \
	@JEMALLOC_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	@LIBEV_CFLAGS@ \
	@LIBNGHTTP3_CFLAGS@ \
	@LIBNGTCP2_CRYPTO_WOLFSSL_CFLAGS@ \
	@LIBNGTCP2_CRYPTO_QUICTLS_CFLAGS@ \
	@LIBNGTCP2_CRYPTO_BORINGSSL_CFLAGS@ \
	@LIBNGTCP2_CFLAGS@ \
	@WOLFSSL_CFLAGS@ \
	@OPENSSL_CFLAGS@ \
	@LIBCARES_CFLAGS@ \
	@JANSSON_CFLAGS@ \
	@LIBBPF_CFLAGS@ \
	@ZLIB_CFLAGS@ \
	@LIBBROTLIENC_CFLAGS@ \
	@LIBBROTLIDEC_CFLAGS@ \
	@EXTRA_DEFS@ \
	@DEFS@
AM_LDFLAGS = @LIBTOOL_LDFLAGS@

LDADD = $(top_builddir)/lib/libnghttp2.la \
	$(top_builddir)/third-party/liburlparse.la \
	$(top_builddir)/third-party/libllhttp.la \
	@JEMALLOC_LIBS@ \
	@LIBXML2_LIBS@ \
	@LIBEV_LIBS@ \
	@LIBNGHTTP3_LIBS@ \
	@LIBNGTCP2_CRYPTO_WOLFSSL_LIBS@ \
	@LIBNGTCP2_CRYPTO_QUICTLS_LIBS@ \
	@LIBNGTCP2_CRYPTO_BORINGSSL_LIBS@ \
	@LIBNGTCP2_LIBS@ \
	@WOLFSSL_LIBS@ \
	@OPENSSL_LIBS@ \
	@LIBCARES_LIBS@ \
	@SYSTEMD_LIBS@ \
	@JANSSON_LIBS@ \
	@LIBBPF_LIBS@ \
	@ZLIB_LIBS@ \
	@LIBBROTLIENC_LIBS@ \
	@LIBBROTLIDEC_LIBS@ \
	@APPLDFLAGS@

if ENABLE_APP

bin_PROGRAMS += nghttp nghttpd nghttpx

HELPER_OBJECTS = util.cc \
	http2.cc timegm.c app_helper.cc nghttp2_gzip.c
HELPER_HFILES = util.h \
	http2.h timegm.h app_helper.h nghttp2_config.h \
	nghttp2_gzip.h network.h

HTML_PARSER_OBJECTS =
HTML_PARSER_HFILES = HtmlParser.h

if HAVE_LIBXML2
HTML_PARSER_OBJECTS += HtmlParser.cc
endif # HAVE_LIBXML2

nghttp_SOURCES = ${HELPER_OBJECTS} ${HELPER_HFILES} nghttp.cc nghttp.h \
	${HTML_PARSER_OBJECTS} ${HTML_PARSER_HFILES} \
	tls.cc tls.h ssl_compat.h

nghttpd_SOURCES = ${HELPER_OBJECTS} ${HELPER_HFILES} nghttpd.cc \
	tls.cc tls.h ssl_compat.h \
	HttpServer.cc HttpServer.h

bin_PROGRAMS += h2load

h2load_SOURCES = util.cc util.h \
	http2.cc http2.h h2load.cc h2load.h \
	timegm.c timegm.h \
	tls.cc tls.h ssl_compat.h \
	h2load_session.h \
	h2load_http2_session.cc h2load_http2_session.h \
	h2load_http1_session.cc h2load_http1_session.h

if ENABLE_HTTP3
h2load_SOURCES += \
	h2load_http3_session.cc h2load_http3_session.h \
	h2load_quic.cc h2load_quic.h \
	quic.cc quic.h
endif # ENABLE_HTTP3

NGHTTPX_SRCS = \
	util.cc util.h http2.cc http2.h timegm.c timegm.h base64.h \
	app_helper.cc app_helper.h \
	tls.cc tls.h ssl_compat.h \
	shrpx_config.cc shrpx_config.h \
	shrpx_error.h \
	shrpx_accept_handler.cc shrpx_accept_handler.h \
	shrpx_connection_handler.cc shrpx_connection_handler.h \
	shrpx_client_handler.cc shrpx_client_handler.h \
	shrpx_upstream.h \
	shrpx_http2_upstream.cc shrpx_http2_upstream.h \
	shrpx_https_upstream.cc shrpx_https_upstream.h \
	shrpx_downstream.cc shrpx_downstream.h \
	shrpx_downstream_connection.cc shrpx_downstream_connection.h \
	shrpx_http_downstream_connection.cc shrpx_http_downstream_connection.h \
	shrpx_http2_downstream_connection.cc shrpx_http2_downstream_connection.h \
	shrpx_http2_session.cc shrpx_http2_session.h \
	shrpx_downstream_queue.cc shrpx_downstream_queue.h \
	shrpx_log.cc shrpx_log.h \
	shrpx_http.cc shrpx_http.h \
	shrpx_io_control.cc shrpx_io_control.h \
	shrpx_tls.cc shrpx_tls.h \
	shrpx_worker.cc shrpx_worker.h \
	shrpx_log_config.cc shrpx_log_config.h \
	shrpx_connect_blocker.cc shrpx_connect_blocker.h \
	shrpx_live_check.cc shrpx_live_check.h \
	shrpx_downstream_connection_pool.cc shrpx_downstream_connection_pool.h \
	shrpx_rate_limit.cc shrpx_rate_limit.h \
	shrpx_connection.cc shrpx_connection.h \
	shrpx_memcached_dispatcher.cc shrpx_memcached_dispatcher.h \
	shrpx_memcached_connection.cc shrpx_memcached_connection.h \
	shrpx_memcached_request.h \
	shrpx_memcached_result.h \
	shrpx_worker_process.cc shrpx_worker_process.h \
	shrpx_process.h \
	shrpx_signal.cc shrpx_signal.h \
	shrpx_router.cc shrpx_router.h \
	shrpx_api_downstream_connection.cc shrpx_api_downstream_connection.h \
	shrpx_health_monitor_downstream_connection.cc \
	shrpx_health_monitor_downstream_connection.h \
	shrpx_null_downstream_connection.cc shrpx_null_downstream_connection.h \
	shrpx_exec.cc shrpx_exec.h \
	shrpx_dns_resolver.cc shrpx_dns_resolver.h \
	shrpx_dual_dns_resolver.cc shrpx_dual_dns_resolver.h \
	shrpx_dns_tracker.cc shrpx_dns_tracker.h \
	buffer.h memchunk.h template.h allocator.h \
	xsi_strerror.c xsi_strerror.h

if HAVE_MRUBY
NGHTTPX_SRCS += \
	shrpx_mruby.cc shrpx_mruby.h \
	shrpx_mruby_module.cc shrpx_mruby_module.h \
	shrpx_mruby_module_env.cc shrpx_mruby_module_env.h \
	shrpx_mruby_module_request.cc shrpx_mruby_module_request.h \
	shrpx_mruby_module_response.cc shrpx_mruby_module_response.h
endif # HAVE_MRUBY

if ENABLE_HTTP3
NGHTTPX_SRCS += \
	shrpx_quic.cc shrpx_quic.h \
	shrpx_quic_listener.cc shrpx_quic_listener.h \
	shrpx_quic_connection_handler.cc shrpx_quic_connection_handler.h \
	shrpx_http3_upstream.cc shrpx_http3_upstream.h \
	http3.cc http3.h \
	quic.cc quic.h \
	siphash.cc siphash.h
endif # ENABLE_HTTP3

noinst_LIBRARIES = libnghttpx.a
libnghttpx_a_SOURCES = ${NGHTTPX_SRCS}
libnghttpx_a_CPPFLAGS = ${AM_CPPFLAGS}

nghttpx_SOURCES = shrpx.cc shrpx.h
nghttpx_CPPFLAGS = ${libnghttpx_a_CPPFLAGS}
nghttpx_LDADD = libnghttpx.a ${LDADD}

if HAVE_MRUBY
libnghttpx_a_CPPFLAGS += \
	-I${top_srcdir}/third-party/mruby/include @LIBMRUBY_CFLAGS@
nghttpx_LDADD += -L${top_builddir}/third-party/mruby/build/lib @LIBMRUBY_LIBS@
endif # HAVE_MRUBY

if HAVE_NEVERBLEED
libnghttpx_a_CPPFLAGS += -I${top_srcdir}/third-party/neverbleed
nghttpx_LDADD += ${top_builddir}/third-party/libneverbleed.la
endif # HAVE_NEVERBLEED

check_PROGRAMS += nghttpx-unittest
nghttpx_unittest_SOURCES = shrpx-unittest.cc \
	shrpx_tls_test.cc shrpx_tls_test.h \
	shrpx_downstream_test.cc shrpx_downstream_test.h \
	shrpx_config_test.cc shrpx_config_test.h \
	shrpx_worker_test.cc shrpx_worker_test.h \
	shrpx_http_test.cc shrpx_http_test.h \
	shrpx_router_test.cc shrpx_router_test.h \
	http2_test.cc http2_test.h \
	util_test.cc util_test.h \
	nghttp2_gzip_test.c nghttp2_gzip_test.h \
	nghttp2_gzip.c nghttp2_gzip.h \
	buffer_test.cc buffer_test.h \
	memchunk_test.cc memchunk_test.h \
	template_test.cc template_test.h \
	base64_test.cc base64_test.h \
	$(top_srcdir)/tests/munit/munit.c $(top_srcdir)/tests/munit/munit.h \
	$(top_srcdir)/tests/munit/munitxx.h
if ENABLE_HTTP3
nghttpx_unittest_SOURCES += siphash_test.cc siphash_test.h
endif # ENABLE_HTTP3
nghttpx_unittest_CPPFLAGS = ${AM_CPPFLAGS} \
	-I$(top_srcdir)/tests/munit \
	-DNGHTTP2_SRC_DIR=\"$(top_srcdir)/src\"
nghttpx_unittest_LDADD = libnghttpx.a ${LDADD} @TESTLDADD@

if HAVE_MRUBY
nghttpx_unittest_CPPFLAGS += \
	-I${top_srcdir}/third-party/mruby/include @LIBMRUBY_CFLAGS@
nghttpx_unittest_LDADD += \
	-L${top_builddir}/third-party/mruby/build/lib @LIBMRUBY_LIBS@
endif # HAVE_MRUBY

if HAVE_NEVERBLEED
nghttpx_unittest_CPPFLAGS += -I${top_srcdir}/third-party/neverbleed
nghttpx_unittest_LDADD += ${top_builddir}/third-party/libneverbleed.la
endif # HAVE_NEVERBLEED

TESTS += nghttpx-unittest

endif # ENABLE_APP

if ENABLE_HPACK_TOOLS

bin_PROGRAMS += inflatehd deflatehd

HPACK_TOOLS_COMMON_SRCS = \
	comp_helper.c comp_helper.h \
	util.cc util.h \
	timegm.c timegm.h

inflatehd_SOURCES = inflatehd.cc $(HPACK_TOOLS_COMMON_SRCS)

deflatehd_SOURCES = deflatehd.cc $(HPACK_TOOLS_COMMON_SRCS)

endif # ENABLE_HPACK_TOOLS
