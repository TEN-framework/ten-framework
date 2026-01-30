/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "nghttp.h"

#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif // HAVE_FCNTL_H
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif // HAVE_NETINET_IN_H
#include <netinet/tcp.h>
#include <getopt.h>

#include <cassert>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <tuple>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/err.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/err.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#ifdef HAVE_JANSSON
#  include <jansson.h>
#endif // HAVE_JANSSON

#include "app_helper.h"
#include "HtmlParser.h"
#include "util.h"
#include "base64.h"
#include "tls.h"
#include "template.h"

#ifndef O_BINARY
#  define O_BINARY (0)
#endif // O_BINARY

namespace nghttp2 {

Config::Config()
  : header_table_size(-1),
    min_header_table_size(std::numeric_limits<uint32_t>::max()),
    encoder_header_table_size(-1),
    padding(0),
    max_concurrent_streams(100),
    peer_max_concurrent_streams(100),
    multiply(1),
    timeout(0.),
    window_bits(-1),
    connection_window_bits(-1),
    verbose(0),
    port_override(0),
    null_out(false),
    remote_name(false),
    get_assets(false),
    stat(false),
    upgrade(false),
    continuation(false),
    no_content_length(false),
    hexdump(false),
    no_push(false),
    expect_continue(false),
    verify_peer(true),
    ktls(false) {
  nghttp2_option_new(&http2_option);
  nghttp2_option_set_peer_max_concurrent_streams(http2_option,
                                                 peer_max_concurrent_streams);
  nghttp2_option_set_builtin_recv_extension_type(http2_option, NGHTTP2_ALTSVC);
  nghttp2_option_set_builtin_recv_extension_type(http2_option, NGHTTP2_ORIGIN);
}

Config::~Config() { nghttp2_option_del(http2_option); }

namespace {
Config config;
} // namespace

namespace {
void print_protocol_nego_error() {
  std::cerr << "[ERROR] HTTP/2 protocol was not selected."
            << " (nghttp2 expects " << NGHTTP2_PROTO_VERSION_ID << ")"
            << std::endl;
}
} // namespace

namespace {
std::string strip_fragment(const char *raw_uri) {
  const char *end;
  for (end = raw_uri; *end && *end != '#'; ++end)
    ;
  size_t len = end - raw_uri;
  return std::string(raw_uri, len);
}
} // namespace

Request::Request(const std::string &uri, const urlparse_url &u,
                 const nghttp2_data_provider2 *data_prd, int64_t data_length,
                 const nghttp2_extpri &extpri, int level)
  : uri(uri),
    u(u),
    extpri(extpri),
    data_length(data_length),
    data_offset(0),
    response_len(0),
    inflater(nullptr),
    data_prd(data_prd),
    header_buffer_size(0),
    stream_id(-1),
    status(0),
    level(level),
    expect_final_response(false) {
  http2::init_hdidx(res_hdidx);
  http2::init_hdidx(req_hdidx);
}

Request::~Request() { nghttp2_gzip_inflate_del(inflater); }

void Request::init_inflater() {
  int rv;
  // This is required with --disable-assert.
  (void)rv;
  rv = nghttp2_gzip_inflate_new(&inflater);
  assert(rv == 0);
}

StringRef Request::get_real_scheme() const {
  return config.scheme_override.empty()
           ? util::get_uri_field(uri.c_str(), u, URLPARSE_SCHEMA)
           : StringRef{config.scheme_override};
}

StringRef Request::get_real_host() const {
  return config.host_override.empty()
           ? util::get_uri_field(uri.c_str(), u, URLPARSE_HOST)
           : StringRef{config.host_override};
}

uint16_t Request::get_real_port() const {
  auto scheme = get_real_scheme();
  return config.host_override.empty() ? util::has_uri_field(u, URLPARSE_PORT)
                                          ? u.port
                                        : scheme == "https"_sr ? 443
                                                               : 80
         : config.port_override == 0  ? scheme == "https"_sr ? 443 : 80
                                      : config.port_override;
}

void Request::init_html_parser() {
  // We crawl HTML using overridden scheme, host, and port.
  auto scheme = get_real_scheme();
  auto host = get_real_host();
  auto port = get_real_port();
  auto ipv6_lit =
    std::find(std::begin(host), std::end(host), ':') != std::end(host);

  auto base_uri = std::string{scheme};
  base_uri += "://";
  if (ipv6_lit) {
    base_uri += '[';
  }
  base_uri += host;
  if (ipv6_lit) {
    base_uri += ']';
  }
  if (!((scheme == "https"_sr && port == 443) ||
        (scheme == "http"_sr && port == 80))) {
    base_uri += ':';
    base_uri += util::utos(port);
  }
  base_uri += util::get_uri_field(uri.c_str(), u, URLPARSE_PATH);
  if (util::has_uri_field(u, URLPARSE_QUERY)) {
    base_uri += '?';
    base_uri += util::get_uri_field(uri.c_str(), u, URLPARSE_QUERY);
  }

  html_parser = std::make_unique<HtmlParser>(base_uri);
}

int Request::update_html_parser(const uint8_t *data, size_t len, int fin) {
  if (!html_parser) {
    return 0;
  }
  return html_parser->parse_chunk(reinterpret_cast<const char *>(data), len,
                                  fin);
}

std::string Request::make_reqpath() const {
  auto path =
    util::has_uri_field(u, URLPARSE_PATH)
      ? std::string{util::get_uri_field(uri.c_str(), u, URLPARSE_PATH)}
      : "/"s;
  if (util::has_uri_field(u, URLPARSE_QUERY)) {
    path += '?';
    path.append(uri.c_str() + u.field_data[URLPARSE_QUERY].off,
                u.field_data[URLPARSE_QUERY].len);
  }
  return path;
}

namespace {
// Perform special handling |host| if it is IPv6 literal and includes
// zone ID per RFC 6874.
std::string decode_host(const StringRef &host) {
  auto zone_start = std::find(std::begin(host), std::end(host), '%');
  if (zone_start == std::end(host) ||
      !util::ipv6_numeric_addr(
        std::string(std::begin(host), zone_start).c_str())) {
    return std::string{host};
  }
  // case: ::1%
  if (zone_start + 1 == std::end(host)) {
    return {host.data(), host.size() - 1};
  }
  // case: ::1%12 or ::1%1
  if (zone_start + 3 >= std::end(host)) {
    return std::string{host};
  }
  // If we see "%25", followed by more characters, then decode %25 as
  // '%'.
  auto zone_id_src = (*(zone_start + 1) == '2' && *(zone_start + 2) == '5')
                       ? zone_start + 3
                       : zone_start + 1;
  auto zone_id = util::percent_decode(zone_id_src, std::end(host));
  auto res = std::string(std::begin(host), zone_start + 1);
  res += zone_id;
  return res;
}
} // namespace

namespace {
nghttp2_extpri resolve_pri(int res_type) {
  switch (res_type) {
  case REQ_CSS:
  case REQ_JS:
    return {
      .urgency = 0,
    };
  case REQ_UNBLOCK_JS:
    return {
      .urgency = 1,
    };
  case REQ_IMG:
    return {
      .urgency = NGHTTP2_EXTPRI_DEFAULT_URGENCY,
      .inc = 1,
    };
  default:
    return {
      .urgency = NGHTTP2_EXTPRI_DEFAULT_URGENCY,
    };
  }
}
} // namespace

bool Request::is_ipv6_literal_addr() const {
  if (util::has_uri_field(u, URLPARSE_HOST)) {
    return memchr(uri.c_str() + u.field_data[URLPARSE_HOST].off, ':',
                  u.field_data[URLPARSE_HOST].len);
  } else {
    return false;
  }
}

Headers::value_type *Request::get_res_header(int32_t token) {
  auto idx = res_hdidx[token];
  if (idx == -1) {
    return nullptr;
  }
  return &res_nva[idx];
}

Headers::value_type *Request::get_req_header(int32_t token) {
  auto idx = req_hdidx[token];
  if (idx == -1) {
    return nullptr;
  }
  return &req_nva[idx];
}

void Request::record_request_start_time() {
  timing.state = RequestState::ON_REQUEST;
  timing.request_start_time = get_time();
}

void Request::record_response_start_time() {
  timing.state = RequestState::ON_RESPONSE;
  timing.response_start_time = get_time();
}

void Request::record_response_end_time() {
  timing.state = RequestState::ON_COMPLETE;
  timing.response_end_time = get_time();
}

namespace {
void continue_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto client = static_cast<HttpClient *>(ev_userdata(loop));
  auto req = static_cast<Request *>(w->data);
  int error;

  error = nghttp2_submit_data2(client->session, NGHTTP2_FLAG_END_STREAM,
                               req->stream_id, req->data_prd);

  if (error) {
    std::cerr << "[ERROR] nghttp2_submit_data2() returned error: "
              << nghttp2_strerror(error) << std::endl;
    nghttp2_submit_rst_stream(client->session, NGHTTP2_FLAG_NONE,
                              req->stream_id, NGHTTP2_INTERNAL_ERROR);
  }

  client->signal_write();
}
} // namespace

ContinueTimer::ContinueTimer(struct ev_loop *loop, Request *req) : loop(loop) {
  ev_timer_init(&timer, continue_timeout_cb, 1., 0.);
  timer.data = req;
}

ContinueTimer::~ContinueTimer() { stop(); }

void ContinueTimer::start() { ev_timer_start(loop, &timer); }

void ContinueTimer::stop() { ev_timer_stop(loop, &timer); }

void ContinueTimer::dispatch_continue() {
  // Only dispatch the timeout callback if it hasn't already been called.
  if (ev_is_active(&timer)) {
    ev_feed_event(loop, &timer, 0);
  }
}

namespace {
int htp_msg_begincb(llhttp_t *htp) {
  if (config.verbose) {
    print_timer();
    std::cout << " HTTP Upgrade response" << std::endl;
  }
  return 0;
}
} // namespace

namespace {
int htp_msg_completecb(llhttp_t *htp) {
  auto client = static_cast<HttpClient *>(htp->data);
  client->upgrade_response_status_code = htp->status_code;
  client->upgrade_response_complete = true;
  return 0;
}
} // namespace

namespace {
constexpr llhttp_settings_t htp_hooks = {
  htp_msg_begincb,    // llhttp_cb      on_message_begin;
  nullptr,            // llhttp_data_cb on_url;
  nullptr,            // llhttp_data_cb on_status;
  nullptr,            // llhttp_data_cb on_method;
  nullptr,            // llhttp_data_cb on_version;
  nullptr,            // llhttp_data_cb on_header_field;
  nullptr,            // llhttp_data_cb on_header_value;
  nullptr,            // llhttp_data_cb on_chunk_extension_name;
  nullptr,            // llhttp_data_cb on_chunk_extension_value;
  nullptr,            // llhttp_cb      on_headers_complete;
  nullptr,            // llhttp_data_cb on_body;
  htp_msg_completecb, // llhttp_cb      on_message_complete;
  nullptr,            // llhttp_cb      on_url_complete;
  nullptr,            // llhttp_cb      on_status_complete;
  nullptr,            // llhttp_cb      on_method_complete;
  nullptr,            // llhttp_cb      on_version_complete;
  nullptr,            // llhttp_cb      on_header_field_complete;
  nullptr,            // llhttp_cb      on_header_value_complete;
  nullptr,            // llhttp_cb      on_chunk_extension_name_complete;
  nullptr,            // llhttp_cb      on_chunk_extension_value_complete;
  nullptr,            // llhttp_cb      on_chunk_header;
  nullptr,            // llhttp_cb      on_chunk_complete;
  nullptr,            // llhttp_cb      on_reset;
};
} // namespace

namespace {
int submit_request(HttpClient *client, const Headers &headers, Request *req) {
  auto scheme = util::get_uri_field(req->uri.c_str(), req->u, URLPARSE_SCHEMA);
  auto build_headers = Headers{{":method", req->data_prd ? "POST" : "GET"},
                               {":path", req->make_reqpath()},
                               {":scheme", std::string{scheme}},
                               {":authority", client->hostport},
                               {"priority", http2::encode_extpri(req->extpri)},
                               {"accept", "*/*"},
                               {"accept-encoding", "gzip, deflate"},
                               {"user-agent", "nghttp2/" NGHTTP2_VERSION}};
  bool expect_continue = false;

  if (config.continuation) {
    for (size_t i = 0; i < 6; ++i) {
      build_headers.emplace_back("continuation-test-" + util::utos(i + 1),
                                 std::string(4_k, '-'));
    }
  }

  auto num_initial_headers = build_headers.size();

  if (req->data_prd) {
    if (!config.no_content_length) {
      build_headers.emplace_back("content-length",
                                 util::utos(req->data_length));
    }
    if (config.expect_continue) {
      expect_continue = true;
      build_headers.emplace_back("expect", "100-continue");
    }
  }

  for (auto &kv : headers) {
    size_t i;
    for (i = 0; i < num_initial_headers; ++i) {
      if (kv.name == build_headers[i].name) {
        build_headers[i].value = kv.value;
        break;
      }
    }
    if (i < num_initial_headers) {
      continue;
    }

    build_headers.emplace_back(kv.name, kv.value, kv.no_index);
  }

  auto nva = std::vector<nghttp2_nv>();
  nva.reserve(build_headers.size());

  for (auto &kv : build_headers) {
    nva.push_back(
      http2::make_field_nv(kv.name, kv.value, http2::no_index(kv.no_index)));
  }

  auto method = http2::get_header(build_headers, ":method");
  assert(method);

  req->method = method->value;

  std::string trailer_names;
  if (!config.trailer.empty()) {
    trailer_names = config.trailer[0].name;
    for (size_t i = 1; i < config.trailer.size(); ++i) {
      trailer_names += ", ";
      trailer_names += config.trailer[i].name;
    }
    nva.push_back(http2::make_field_v("trailer"_sr, trailer_names));
  }

  int32_t stream_id;

  if (expect_continue) {
    stream_id = nghttp2_submit_headers(client->session, 0, -1, nullptr,
                                       nva.data(), nva.size(), req);
  } else {
    stream_id = nghttp2_submit_request2(client->session, nullptr, nva.data(),
                                        nva.size(), req->data_prd, req);
  }

  if (stream_id < 0) {
    std::cerr << "[ERROR] nghttp2_submit_"
              << (expect_continue ? "headers" : "request2")
              << "() returned error: " << nghttp2_strerror(stream_id)
              << std::endl;
    return -1;
  }

  req->stream_id = stream_id;
  client->request_done(req);

  req->req_nva = std::move(build_headers);

  if (expect_continue) {
    auto timer = std::make_unique<ContinueTimer>(client->loop, req);
    req->continue_timer = std::move(timer);
  }

  return 0;
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto client = static_cast<HttpClient *>(w->data);
  if (client->do_read() != 0) {
    client->disconnect();
  }
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto client = static_cast<HttpClient *>(w->data);
  auto rv = client->do_write();
  if (rv == HttpClient::ERR_CONNECT_FAIL) {
    client->connect_fail();
    return;
  }
  if (rv != 0) {
    client->disconnect();
  }
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto client = static_cast<HttpClient *>(w->data);
  std::cerr << "[ERROR] Timeout" << std::endl;
  client->disconnect();
}
} // namespace

namespace {
void settings_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto client = static_cast<HttpClient *>(w->data);
  ev_timer_stop(loop, w);

  nghttp2_session_terminate_session(client->session, NGHTTP2_SETTINGS_TIMEOUT);

  client->signal_write();
}
} // namespace

HttpClient::HttpClient(const nghttp2_session_callbacks *callbacks,
                       struct ev_loop *loop, SSL_CTX *ssl_ctx)
  : wb(&mcpool),
    session(nullptr),
    callbacks(callbacks),
    loop(loop),
    ssl_ctx(ssl_ctx),
    ssl(nullptr),
    addrs(nullptr),
    next_addr(nullptr),
    cur_addr(nullptr),
    complete(0),
    success(0),
    settings_payloadlen(0),
    state(ClientState::IDLE),
    upgrade_response_status_code(0),
    fd(-1),
    upgrade_response_complete(false) {
  ev_io_init(&wev, writecb, 0, EV_WRITE);
  ev_io_init(&rev, readcb, 0, EV_READ);

  wev.data = this;
  rev.data = this;

  ev_timer_init(&wt, timeoutcb, 0., config.timeout);
  ev_timer_init(&rt, timeoutcb, 0., config.timeout);

  wt.data = this;
  rt.data = this;

  ev_timer_init(&settings_timer, settings_timeout_cb, 0., 10.);

  settings_timer.data = this;
}

HttpClient::~HttpClient() {
  disconnect();

  if (addrs) {
    freeaddrinfo(addrs);
    addrs = nullptr;
    next_addr = nullptr;
  }
}

bool HttpClient::need_upgrade() const {
  return config.upgrade && scheme == "http";
}

int HttpClient::resolve_host(const std::string &host, uint16_t port) {
  int rv;
  this->host = host;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_ADDRCONFIG;
  rv = getaddrinfo(host.c_str(), util::utos(port).c_str(), &hints, &addrs);
  if (rv != 0) {
    std::cerr << "[ERROR] getaddrinfo() failed: " << gai_strerror(rv)
              << std::endl;
    return -1;
  }
  if (addrs == nullptr) {
    std::cerr << "[ERROR] No address returned" << std::endl;
    return -1;
  }
  next_addr = addrs;
  return 0;
}

namespace {
// Just returns 1 to continue handshake.
int verify_cb(int preverify_ok, X509_STORE_CTX *ctx) { return 1; }
} // namespace

int HttpClient::initiate_connection() {
  int rv;

  cur_addr = nullptr;
  while (next_addr) {
    cur_addr = next_addr;
    next_addr = next_addr->ai_next;
    fd = util::create_nonblock_socket(cur_addr->ai_family);
    if (fd == -1) {
      continue;
    }

    if (ssl_ctx) {
      // We are establishing TLS connection.
      ssl = SSL_new(ssl_ctx);
      if (!ssl) {
        std::cerr << "[ERROR] SSL_new() failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return -1;
      }

      SSL_set_connect_state(ssl);

      // If the user overrode the :authority or host header, use that
      // value for the SNI extension
      const auto &host_string =
        config.host_override.empty() ? host : config.host_override;

      auto param = SSL_get0_param(ssl);
      X509_VERIFY_PARAM_set_hostflags(param, 0);
      X509_VERIFY_PARAM_set1_host(param, host_string.c_str(),
                                  host_string.size());
      SSL_set_verify(ssl, SSL_VERIFY_PEER, verify_cb);

      if (!util::numeric_host(host_string.c_str())) {
        SSL_set_tlsext_host_name(ssl, host_string.c_str());
      }
    }

    rv = connect(fd, cur_addr->ai_addr, cur_addr->ai_addrlen);

    if (rv != 0 && errno != EINPROGRESS) {
      if (ssl) {
        SSL_free(ssl);
        ssl = nullptr;
      }
      close(fd);
      fd = -1;
      continue;
    }
    break;
  }

  if (fd == -1) {
    return -1;
  }

  writefn = &HttpClient::connected;

  if (need_upgrade()) {
    on_readfn = &HttpClient::on_upgrade_read;
    on_writefn = &HttpClient::on_upgrade_connect;
  } else {
    on_readfn = &HttpClient::on_read;
    on_writefn = &HttpClient::on_write;
  }

  ev_io_set(&rev, fd, EV_READ);
  ev_io_set(&wev, fd, EV_WRITE);

  ev_io_start(loop, &wev);

  ev_timer_again(loop, &wt);

  return 0;
}

void HttpClient::disconnect() {
  state = ClientState::IDLE;

  for (auto req = std::begin(reqvec); req != std::end(reqvec); ++req) {
    if ((*req)->continue_timer) {
      (*req)->continue_timer->stop();
    }
  }

  ev_timer_stop(loop, &settings_timer);

  ev_timer_stop(loop, &rt);
  ev_timer_stop(loop, &wt);

  ev_io_stop(loop, &rev);
  ev_io_stop(loop, &wev);

  nghttp2_session_del(session);
  session = nullptr;

  if (ssl) {
    SSL_set_shutdown(ssl, SSL_get_shutdown(ssl) | SSL_RECEIVED_SHUTDOWN);
    ERR_clear_error();
    SSL_shutdown(ssl);
    SSL_free(ssl);
    ssl = nullptr;
  }

  if (fd != -1) {
    shutdown(fd, SHUT_WR);
    close(fd);
    fd = -1;
  }
}

int HttpClient::read_clear() {
  ev_timer_again(loop, &rt);

  std::array<uint8_t, 8_k> buf;

  for (;;) {
    ssize_t nread;
    while ((nread = read(fd, buf.data(), buf.size())) == -1 && errno == EINTR)
      ;
    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
      }
      return -1;
    }

    if (nread == 0) {
      return -1;
    }

    if (on_readfn(*this, buf.data(), nread) != 0) {
      return -1;
    }
  }

  return 0;
}

int HttpClient::write_clear() {
  ev_timer_again(loop, &rt);

  std::array<struct iovec, 2> iov;

  for (;;) {
    if (on_writefn(*this) != 0) {
      return -1;
    }

    auto iovcnt = wb.riovec(iov.data(), iov.size());

    if (iovcnt == 0) {
      break;
    }

    ssize_t nwrite;
    while ((nwrite = writev(fd, iov.data(), iovcnt)) == -1 && errno == EINTR)
      ;
    if (nwrite == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        ev_io_start(loop, &wev);
        ev_timer_again(loop, &wt);
        return 0;
      }
      return -1;
    }

    wb.drain(nwrite);
  }

  ev_io_stop(loop, &wev);
  ev_timer_stop(loop, &wt);

  return 0;
}

int HttpClient::noop() { return 0; }

void HttpClient::connect_fail() {
  if (state == ClientState::IDLE) {
    std::cerr << "[ERROR] Could not connect to the address "
              << util::numeric_name(cur_addr->ai_addr, cur_addr->ai_addrlen)
              << std::endl;
  }
  auto cur_state = state;
  disconnect();
  if (cur_state == ClientState::IDLE) {
    if (initiate_connection() == 0) {
      std::cerr << "Trying next address "
                << util::numeric_name(cur_addr->ai_addr, cur_addr->ai_addrlen)
                << std::endl;
    }
  }
}

int HttpClient::connected() {
  if (!util::check_socket_connected(fd)) {
    return ERR_CONNECT_FAIL;
  }

  if (config.verbose) {
    print_timer();
    std::cout << " Connected" << std::endl;
  }

  state = ClientState::CONNECTED;

  ev_io_start(loop, &rev);
  ev_io_stop(loop, &wev);

  ev_timer_again(loop, &rt);
  ev_timer_stop(loop, &wt);

  if (ssl) {
    SSL_set_fd(ssl, fd);

    readfn = &HttpClient::tls_handshake;
    writefn = &HttpClient::tls_handshake;

    return do_write();
  }

  readfn = &HttpClient::read_clear;
  writefn = &HttpClient::write_clear;

  if (need_upgrade()) {
    htp = std::make_unique<llhttp_t>();
    llhttp_init(htp.get(), HTTP_RESPONSE, &htp_hooks);
    htp->data = this;

    return do_write();
  }

  if (connection_made() != 0) {
    return -1;
  }

  return 0;
}

namespace {
size_t populate_settings(nghttp2_settings_entry *iv) {
  size_t niv = 3;

  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = config.max_concurrent_streams;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  if (config.window_bits != -1) {
    iv[1].value = (1 << config.window_bits) - 1;
  } else {
    iv[1].value = NGHTTP2_INITIAL_WINDOW_SIZE;
  }

  iv[2].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv[2].value = 1;

  if (config.header_table_size >= 0) {
    if (config.min_header_table_size < config.header_table_size) {
      iv[niv].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
      iv[niv].value = config.min_header_table_size;
      ++niv;
    }

    iv[niv].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
    iv[niv].value = config.header_table_size;
    ++niv;
  }

  if (config.no_push) {
    iv[niv].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
    iv[niv].value = 0;
    ++niv;
  }

  return niv;
}
} // namespace

int HttpClient::on_upgrade_connect() {
  nghttp2_ssize rv;
  record_connect_end_time();
  assert(!reqvec.empty());
  std::array<nghttp2_settings_entry, 16> iv;
  size_t niv = populate_settings(iv.data());
  assert(settings_payload.size() >= 8 * niv);
  rv = nghttp2_pack_settings_payload2(settings_payload.data(),
                                      settings_payload.size(), iv.data(), niv);
  if (rv < 0) {
    return -1;
  }
  settings_payloadlen = rv;
  auto token68 =
    base64::encode(std::begin(settings_payload),
                   std::begin(settings_payload) + settings_payloadlen);
  util::to_token68(token68);

  std::string req;
  if (reqvec[0]->data_prd) {
    // If the request contains upload data, use OPTIONS * to upgrade
    req = "OPTIONS *";
  } else {
    auto meth =
      std::find_if(std::begin(config.headers), std::end(config.headers),
                   [](const auto &kv) { return ":method"_sr == kv.name; });

    if (meth == std::end(config.headers)) {
      req = "GET ";
      reqvec[0]->method = "GET";
    } else {
      req = (*meth).value;
      req += ' ';
      reqvec[0]->method = (*meth).value;
    }
    req += reqvec[0]->make_reqpath();
  }

  auto headers = Headers{{"host", hostport},
                         {"connection", "Upgrade, HTTP2-Settings"},
                         {"upgrade", NGHTTP2_CLEARTEXT_PROTO_VERSION_ID},
                         {"http2-settings", std::move(token68)},
                         {"accept", "*/*"},
                         {"user-agent", "nghttp2/" NGHTTP2_VERSION}};
  auto initial_headerslen = headers.size();

  if (!reqvec[0]->data_prd) {
    headers.emplace_back("priority", http2::encode_extpri(reqvec[0]->extpri));
    ++initial_headerslen;
  }

  for (auto &kv : config.headers) {
    size_t i;
    if (kv.name.empty() || kv.name[0] == ':') {
      continue;
    }
    for (i = 0; i < initial_headerslen; ++i) {
      if (kv.name == headers[i].name) {
        headers[i].value = kv.value;
        break;
      }
    }
    if (i < initial_headerslen) {
      continue;
    }
    headers.emplace_back(kv.name, kv.value, kv.no_index);
  }

  req += " HTTP/1.1\r\n";

  for (auto &kv : headers) {
    req += kv.name;
    req += ": ";
    req += kv.value;
    req += "\r\n";
  }
  req += "\r\n";

  wb.append(req);

  if (config.verbose) {
    print_timer();
    std::cout << " HTTP Upgrade request\n" << req << std::endl;
  }

  if (!reqvec[0]->data_prd) {
    // record request time if this is a part of real request.
    reqvec[0]->record_request_start_time();
    reqvec[0]->req_nva = std::move(headers);
  }

  on_writefn = &HttpClient::noop;

  signal_write();

  return 0;
}

int HttpClient::on_upgrade_read(const uint8_t *data, size_t len) {
  int rv;

  auto htperr =
    llhttp_execute(htp.get(), reinterpret_cast<const char *>(data), len);
  auto nread = htperr == HPE_OK
                 ? len
                 : static_cast<size_t>(reinterpret_cast<const uint8_t *>(
                                         llhttp_get_error_pos(htp.get())) -
                                       data);

  if (config.verbose) {
    std::cout.write(reinterpret_cast<const char *>(data), nread);
  }

  if (htperr != HPE_OK && htperr != HPE_PAUSED_UPGRADE) {
    std::cerr << "[ERROR] Failed to parse HTTP Upgrade response header: "
              << "(" << llhttp_errno_name(htperr) << ") "
              << llhttp_get_error_reason(htp.get()) << std::endl;
    return -1;
  }

  if (!upgrade_response_complete) {
    return 0;
  }

  if (config.verbose) {
    std::cout << std::endl;
  }

  if (upgrade_response_status_code != 101) {
    std::cerr << "[ERROR] HTTP Upgrade failed" << std::endl;

    return -1;
  }

  if (config.verbose) {
    print_timer();
    std::cout << " HTTP Upgrade success" << std::endl;
  }

  on_readfn = &HttpClient::on_read;
  on_writefn = &HttpClient::on_write;

  rv = connection_made();
  if (rv != 0) {
    return rv;
  }

  // Read remaining data in the buffer because it is not notified
  // callback anymore.
  rv = on_readfn(*this, data + nread, len - nread);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int HttpClient::do_read() { return readfn(*this); }
int HttpClient::do_write() { return writefn(*this); }

int HttpClient::connection_made() {
  int rv;

  if (!need_upgrade()) {
    record_connect_end_time();
  }

  if (ssl) {
    // Check ALPN result
    const unsigned char *next_proto = nullptr;
    unsigned int next_proto_len;

    SSL_get0_alpn_selected(ssl, &next_proto, &next_proto_len);
    if (next_proto) {
      auto proto = StringRef{next_proto, next_proto_len};
      if (config.verbose) {
        std::cout << "The negotiated protocol: " << proto << std::endl;
      }
      if (!util::check_h2_is_selected(proto)) {
        next_proto = nullptr;
      }
    }
    if (!next_proto) {
      print_protocol_nego_error();
      return -1;
    }
  }

  rv =
    nghttp2_session_client_new2(&session, callbacks, this, config.http2_option);

  if (rv != 0) {
    return -1;
  }
  if (need_upgrade()) {
    // Adjust stream user-data depending on the existence of upload
    // data
    Request *stream_user_data = nullptr;
    if (!reqvec[0]->data_prd) {
      stream_user_data = reqvec[0].get();
    }
    // If HEAD is used, that is only when user specified it with -H
    // option.
    auto head_request = stream_user_data && stream_user_data->method == "HEAD";
    rv = nghttp2_session_upgrade2(session, settings_payload.data(),
                                  settings_payloadlen, head_request,
                                  stream_user_data);
    if (rv != 0) {
      std::cerr << "[ERROR] nghttp2_session_upgrade() returned error: "
                << nghttp2_strerror(rv) << std::endl;
      return -1;
    }
    if (stream_user_data) {
      stream_user_data->stream_id = 1;
      request_done(stream_user_data);
    }
  }
  // If upgrade succeeds, the SETTINGS value sent with
  // HTTP2-Settings header field has already been submitted to
  // session object.
  if (!need_upgrade()) {
    std::array<nghttp2_settings_entry, 16> iv;
    auto niv = populate_settings(iv.data());
    rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv.data(), niv);
    if (rv != 0) {
      return -1;
    }
  }

  ev_timer_again(loop, &settings_timer);

  if (config.connection_window_bits != -1) {
    int32_t window_size = (1 << config.connection_window_bits) - 1;
    rv = nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE, 0,
                                               window_size);
    if (rv != 0) {
      return -1;
    }
  }
  // Adjust first request depending on the existence of the upload
  // data
  for (auto i = std::begin(reqvec) + (need_upgrade() && !reqvec[0]->data_prd);
       i != std::end(reqvec); ++i) {
    if (submit_request(this, config.headers, (*i).get()) != 0) {
      return -1;
    }
  }

  signal_write();

  return 0;
}

int HttpClient::on_read(const uint8_t *data, size_t len) {
  if (config.hexdump) {
    util::hexdump(stdout, data, len);
  }

  auto rv = nghttp2_session_mem_recv2(session, data, len);
  if (rv < 0) {
    std::cerr << "[ERROR] nghttp2_session_mem_recv2() returned error: "
              << nghttp2_strerror(rv) << std::endl;
    return -1;
  }

  assert(static_cast<size_t>(rv) == len);

  if (nghttp2_session_want_read(session) == 0 &&
      nghttp2_session_want_write(session) == 0 && wb.rleft() == 0) {
    return -1;
  }

  signal_write();

  return 0;
}

int HttpClient::on_write() {
  for (;;) {
    if (wb.rleft() >= 16384) {
      return 0;
    }

    const uint8_t *data;
    auto len = nghttp2_session_mem_send2(session, &data);
    if (len < 0) {
      std::cerr << "[ERROR] nghttp2_session_send2() returned error: "
                << nghttp2_strerror(len) << std::endl;
      return -1;
    }

    if (len == 0) {
      break;
    }

    wb.append(data, len);
  }

  if (nghttp2_session_want_read(session) == 0 &&
      nghttp2_session_want_write(session) == 0 && wb.rleft() == 0) {
    return -1;
  }

  return 0;
}

int HttpClient::tls_handshake() {
  ev_timer_again(loop, &rt);

  ERR_clear_error();

  auto rv = SSL_do_handshake(ssl);

  if (rv <= 0) {
    auto err = SSL_get_error(ssl, rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
      ev_io_stop(loop, &wev);
      ev_timer_stop(loop, &wt);
      return 0;
    case SSL_ERROR_WANT_WRITE:
      ev_io_start(loop, &wev);
      ev_timer_again(loop, &wt);
      return 0;
    default:
      return -1;
    }
  }

  ev_io_stop(loop, &wev);
  ev_timer_stop(loop, &wt);

  readfn = &HttpClient::read_tls;
  writefn = &HttpClient::write_tls;

  if (config.verify_peer) {
    auto verify_res = SSL_get_verify_result(ssl);
    if (verify_res != X509_V_OK) {
      std::cerr << "[WARNING] Certificate verification failed: "
                << X509_verify_cert_error_string(verify_res) << std::endl;
    }
  }

  if (connection_made() != 0) {
    return -1;
  }

  return 0;
}

int HttpClient::read_tls() {
  ev_timer_again(loop, &rt);

  ERR_clear_error();

  std::array<uint8_t, 8_k> buf;
  for (;;) {
    auto rv = SSL_read(ssl, buf.data(), buf.size());

    if (rv <= 0) {
      auto err = SSL_get_error(ssl, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
        return 0;
      case SSL_ERROR_WANT_WRITE:
        // renegotiation started
        return -1;
      default:
        return -1;
      }
    }

    if (on_readfn(*this, buf.data(), rv) != 0) {
      return -1;
    }
  }
}

int HttpClient::write_tls() {
  ev_timer_again(loop, &rt);

  ERR_clear_error();

  struct iovec iov;

  for (;;) {
    if (on_writefn(*this) != 0) {
      return -1;
    }

    auto iovcnt = wb.riovec(&iov, 1);

    if (iovcnt == 0) {
      break;
    }

    auto rv = SSL_write(ssl, iov.iov_base, iov.iov_len);

    if (rv <= 0) {
      auto err = SSL_get_error(ssl, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
        // renegotiation started
        return -1;
      case SSL_ERROR_WANT_WRITE:
        ev_io_start(loop, &wev);
        ev_timer_again(loop, &wt);
        return 0;
      default:
        return -1;
      }
    }

    wb.drain(rv);
  }

  ev_io_stop(loop, &wev);
  ev_timer_stop(loop, &wt);

  return 0;
}

void HttpClient::signal_write() { ev_io_start(loop, &wev); }

bool HttpClient::all_requests_processed() const {
  return complete == reqvec.size();
}

void HttpClient::update_hostport() {
  if (reqvec.empty()) {
    return;
  }
  scheme =
    util::get_uri_field(reqvec[0]->uri.c_str(), reqvec[0]->u, URLPARSE_SCHEMA);
  std::stringstream ss;
  if (reqvec[0]->is_ipv6_literal_addr()) {
    // we may have zone ID, which must start with "%25", or "%".  RFC
    // 6874 defines "%25" only, and just "%" is allowed for just
    // convenience to end-user input.
    auto host =
      util::get_uri_field(reqvec[0]->uri.c_str(), reqvec[0]->u, URLPARSE_HOST);
    auto end = std::find(std::begin(host), std::end(host), '%');
    ss << "[";
    ss.write(host.data(), end - std::begin(host));
    ss << "]";
  } else {
    util::write_uri_field(ss, reqvec[0]->uri.c_str(), reqvec[0]->u,
                          URLPARSE_HOST);
  }
  if (util::has_uri_field(reqvec[0]->u, URLPARSE_PORT) &&
      reqvec[0]->u.port !=
        util::get_default_port(reqvec[0]->uri.c_str(), reqvec[0]->u)) {
    ss << ":" << reqvec[0]->u.port;
  }
  hostport = ss.str();
}

bool HttpClient::add_request(const std::string &uri,
                             const nghttp2_data_provider2 *data_prd,
                             int64_t data_length, const nghttp2_extpri &extpri,
                             int level) {
  urlparse_url u;
  if (urlparse_parse_url(uri.c_str(), uri.size(), 0, &u) != 0) {
    return false;
  }
  if (path_cache.count(uri)) {
    return false;
  }

  if (config.multiply == 1) {
    path_cache.insert(uri);
  }

  reqvec.push_back(
    std::make_unique<Request>(uri, u, data_prd, data_length, extpri, level));
  return true;
}

void HttpClient::record_start_time() {
  timing.system_start_time = std::chrono::system_clock::now();
  timing.start_time = get_time();
}

void HttpClient::record_domain_lookup_end_time() {
  timing.domain_lookup_end_time = get_time();
}

void HttpClient::record_connect_end_time() {
  timing.connect_end_time = get_time();
}

void HttpClient::request_done(Request *req) {
  if (req->stream_id % 2 == 0) {
    return;
  }
}

#ifdef HAVE_JANSSON
void HttpClient::output_har(FILE *outfile) {
  static auto PAGE_ID = "page_0";

  auto root = json_object();
  auto log = json_object();
  json_object_set_new(root, "log", log);
  json_object_set_new(log, "version", json_string("1.2"));

  auto creator = json_object();
  json_object_set_new(log, "creator", creator);

  json_object_set_new(creator, "name", json_string("nghttp"));
  json_object_set_new(creator, "version", json_string(NGHTTP2_VERSION));

  auto pages = json_array();
  json_object_set_new(log, "pages", pages);

  auto page = json_object();
  json_array_append_new(pages, page);

  json_object_set_new(
    page, "startedDateTime",
    json_string(util::format_iso8601(timing.system_start_time).c_str()));
  json_object_set_new(page, "id", json_string(PAGE_ID));
  json_object_set_new(page, "title", json_string(""));

  json_object_set_new(page, "pageTimings", json_object());

  auto entries = json_array();
  json_object_set_new(log, "entries", entries);

  auto dns_delta = std::chrono::duration_cast<std::chrono::microseconds>(
                     timing.domain_lookup_end_time - timing.start_time)
                     .count() /
                   1000.0;
  auto connect_delta =
    std::chrono::duration_cast<std::chrono::microseconds>(
      timing.connect_end_time - timing.domain_lookup_end_time)
      .count() /
    1000.0;

  for (size_t i = 0; i < reqvec.size(); ++i) {
    auto &req = reqvec[i];

    if (req->timing.state != RequestState::ON_COMPLETE) {
      continue;
    }

    auto entry = json_object();
    json_array_append_new(entries, entry);

    auto &req_timing = req->timing;
    auto request_time =
      (i == 0)
        ? timing.system_start_time
        : timing.system_start_time +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
              req_timing.request_start_time - timing.start_time);

    auto wait_delta =
      std::chrono::duration_cast<std::chrono::microseconds>(
        req_timing.response_start_time - req_timing.request_start_time)
        .count() /
      1000.0;
    auto receive_delta =
      std::chrono::duration_cast<std::chrono::microseconds>(
        req_timing.response_end_time - req_timing.response_start_time)
        .count() /
      1000.0;

    auto time_sum =
      std::chrono::duration_cast<std::chrono::microseconds>(
        (i == 0)
          ? (req_timing.response_end_time - timing.start_time)
          : (req_timing.response_end_time - req_timing.request_start_time))
        .count() /
      1000.0;

    json_object_set_new(
      entry, "startedDateTime",
      json_string(util::format_iso8601(request_time).c_str()));
    json_object_set_new(entry, "time", json_real(time_sum));

    auto pushed = req->stream_id % 2 == 0;

    json_object_set_new(entry, "comment",
                        json_string(pushed ? "Pushed Object" : ""));

    auto request = json_object();
    json_object_set_new(entry, "request", request);

    auto req_headers = json_array();
    json_object_set_new(request, "headers", req_headers);

    for (auto &nv : req->req_nva) {
      auto hd = json_object();
      json_array_append_new(req_headers, hd);

      json_object_set_new(hd, "name", json_string(nv.name.c_str()));
      json_object_set_new(hd, "value", json_string(nv.value.c_str()));
    }

    json_object_set_new(request, "method", json_string(req->method.c_str()));
    json_object_set_new(request, "url", json_string(req->uri.c_str()));
    json_object_set_new(request, "httpVersion", json_string("HTTP/2.0"));
    json_object_set_new(request, "cookies", json_array());
    json_object_set_new(request, "queryString", json_array());
    json_object_set_new(request, "headersSize", json_integer(-1));
    json_object_set_new(request, "bodySize", json_integer(-1));

    auto response = json_object();
    json_object_set_new(entry, "response", response);

    auto res_headers = json_array();
    json_object_set_new(response, "headers", res_headers);

    for (auto &nv : req->res_nva) {
      auto hd = json_object();
      json_array_append_new(res_headers, hd);

      json_object_set_new(hd, "name", json_string(nv.name.c_str()));
      json_object_set_new(hd, "value", json_string(nv.value.c_str()));
    }

    json_object_set_new(response, "status", json_integer(req->status));
    json_object_set_new(response, "statusText", json_string(""));
    json_object_set_new(response, "httpVersion", json_string("HTTP/2.0"));
    json_object_set_new(response, "cookies", json_array());

    auto content = json_object();
    json_object_set_new(response, "content", content);

    json_object_set_new(content, "size", json_integer(req->response_len));

    auto content_type_ptr = http2::get_header(req->res_nva, "content-type");

    const char *content_type = "";
    if (content_type_ptr) {
      content_type = content_type_ptr->value.c_str();
    }

    json_object_set_new(content, "mimeType", json_string(content_type));

    json_object_set_new(response, "redirectURL", json_string(""));
    json_object_set_new(response, "headersSize", json_integer(-1));
    json_object_set_new(response, "bodySize", json_integer(-1));
    json_object_set_new(entry, "cache", json_object());

    auto timings = json_object();
    json_object_set_new(entry, "timings", timings);

    auto dns_timing = (i == 0) ? dns_delta : 0;
    auto connect_timing = (i == 0) ? connect_delta : 0;

    json_object_set_new(timings, "dns", json_real(dns_timing));
    json_object_set_new(timings, "connect", json_real(connect_timing));

    json_object_set_new(timings, "blocked", json_real(0.0));
    json_object_set_new(timings, "send", json_real(0.0));
    json_object_set_new(timings, "wait", json_real(wait_delta));
    json_object_set_new(timings, "receive", json_real(receive_delta));

    json_object_set_new(entry, "pageref", json_string(PAGE_ID));
    json_object_set_new(entry, "connection",
                        json_string(util::utos(req->stream_id).c_str()));
  }

  json_dumpf(root, outfile, JSON_PRESERVE_ORDER | JSON_INDENT(2));
  json_decref(root);
}
#endif // HAVE_JANSSON

namespace {
void update_html_parser(HttpClient *client, Request *req, const uint8_t *data,
                        size_t len, int fin) {
  if (!req->html_parser) {
    return;
  }
  req->update_html_parser(data, len, fin);

  auto scheme = req->get_real_scheme();
  auto host = req->get_real_host();
  auto port = req->get_real_port();

  for (auto &p : req->html_parser->get_links()) {
    auto uri = strip_fragment(p.first.c_str());
    auto res_type = p.second;

    urlparse_url u;
    if (urlparse_parse_url(uri.c_str(), uri.size(), 0, &u) != 0) {
      continue;
    }

    if (!util::fieldeq(uri.c_str(), u, URLPARSE_SCHEMA, scheme) ||
        !util::fieldeq(uri.c_str(), u, URLPARSE_HOST, host)) {
      continue;
    }

    auto link_port = util::has_uri_field(u, URLPARSE_PORT) ? u.port
                     : scheme == "https"_sr                ? 443
                                                           : 80;

    if (port != link_port) {
      continue;
    }

    // No POST data for assets
    auto extpri = resolve_pri(res_type);

    if (client->add_request(uri, nullptr, 0, extpri, req->level + 1)) {
      submit_request(client, config.headers, client->reqvec.back().get());
    }
  }
  req->html_parser->clear_links();
}
} // namespace

namespace {
HttpClient *get_client(void *user_data) {
  return static_cast<HttpClient *>(user_data);
}
} // namespace

namespace {
int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags,
                                int32_t stream_id, const uint8_t *data,
                                size_t len, void *user_data) {
  auto client = get_client(user_data);
  auto req = static_cast<Request *>(
    nghttp2_session_get_stream_user_data(session, stream_id));

  if (!req) {
    return 0;
  }

  if (config.verbose >= 2) {
    verbose_on_data_chunk_recv_callback(session, flags, stream_id, data, len,
                                        user_data);
  }

  req->response_len += len;

  if (req->inflater) {
    while (len > 0) {
      const size_t MAX_OUTLEN = 4_k;
      std::array<uint8_t, MAX_OUTLEN> out;
      size_t outlen = MAX_OUTLEN;
      size_t tlen = len;
      int rv =
        nghttp2_gzip_inflate(req->inflater, out.data(), &outlen, data, &tlen);
      if (rv != 0) {
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                                  NGHTTP2_INTERNAL_ERROR);
        break;
      }

      if (!config.null_out) {
        std::cout.write(reinterpret_cast<const char *>(out.data()), outlen);
      }

      update_html_parser(client, req, out.data(), outlen, 0);
      data += tlen;
      len -= tlen;
    }

    return 0;
  }

  if (!config.null_out) {
    std::cout.write(reinterpret_cast<const char *>(data), len);
  }

  update_html_parser(client, req, data, len, 0);

  return 0;
}
} // namespace

namespace {
nghttp2_ssize select_padding_callback(nghttp2_session *session,
                                      const nghttp2_frame *frame,
                                      size_t max_payload, void *user_data) {
  return std::min(max_payload, frame->hd.length + config.padding);
}
} // namespace

namespace {
void check_response_header(nghttp2_session *session, Request *req) {
  bool gzip = false;

  req->expect_final_response = false;

  auto status_hd = req->get_res_header(http2::HD__STATUS);

  if (!status_hd) {
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, req->stream_id,
                              NGHTTP2_PROTOCOL_ERROR);
    return;
  }

  auto status = http2::parse_http_status_code(StringRef{status_hd->value});
  if (status == -1) {
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, req->stream_id,
                              NGHTTP2_PROTOCOL_ERROR);
    return;
  }

  req->status = status;

  for (auto &nv : req->res_nva) {
    if ("content-encoding" == nv.name) {
      gzip = util::strieq("gzip"_sr, nv.value) ||
             util::strieq("deflate"_sr, nv.value);
      continue;
    }
  }

  if (req->status / 100 == 1) {
    if (req->continue_timer && (req->status == 100)) {
      // If the request is waiting for a 100 Continue, complete the handshake.
      req->continue_timer->dispatch_continue();
    }

    req->expect_final_response = true;
    req->status = 0;
    req->res_nva.clear();
    http2::init_hdidx(req->res_hdidx);
    return;
  } else if (req->continue_timer) {
    // A final response stops any pending Expect/Continue handshake.
    req->continue_timer->stop();
  }

  if (gzip) {
    if (!req->inflater) {
      req->init_inflater();
    }
  }
  if (config.get_assets && req->level == 0) {
    if (!req->html_parser) {
      req->init_html_parser();
    }
  }
}
} // namespace

namespace {
int on_begin_headers_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data) {
  auto client = get_client(user_data);
  switch (frame->hd.type) {
  case NGHTTP2_HEADERS: {
    auto req = static_cast<Request *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!req) {
      break;
    }

    switch (frame->headers.cat) {
    case NGHTTP2_HCAT_RESPONSE:
    case NGHTTP2_HCAT_PUSH_RESPONSE:
      req->record_response_start_time();
      break;
    default:
      break;
    }

    break;
  }
  case NGHTTP2_PUSH_PROMISE: {
    auto stream_id = frame->push_promise.promised_stream_id;
    urlparse_url u{};
    nghttp2_extpri extpri{
      .urgency = NGHTTP2_EXTPRI_DEFAULT_URGENCY,
    };

    auto req = std::make_unique<Request>("", u, nullptr, 0, extpri);
    req->stream_id = stream_id;

    nghttp2_session_set_stream_user_data(session, stream_id, req.get());

    client->request_done(req.get());
    req->record_request_start_time();
    client->reqvec.push_back(std::move(req));

    break;
  }
  }
  return 0;
}
} // namespace

namespace {
int on_header_callback(nghttp2_session *session, const nghttp2_frame *frame,
                       const uint8_t *name, size_t namelen,
                       const uint8_t *value, size_t valuelen, uint8_t flags,
                       void *user_data) {
  if (config.verbose) {
    verbose_on_header_callback(session, frame, name, namelen, value, valuelen,
                               flags, user_data);
  }

  switch (frame->hd.type) {
  case NGHTTP2_HEADERS: {
    auto req = static_cast<Request *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));

    if (!req) {
      break;
    }

    /* ignore trailer header */
    if (frame->headers.cat == NGHTTP2_HCAT_HEADERS &&
        !req->expect_final_response) {
      break;
    }

    if (req->header_buffer_size + namelen + valuelen > 64_k) {
      nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, frame->hd.stream_id,
                                NGHTTP2_INTERNAL_ERROR);
      return 0;
    }

    req->header_buffer_size += namelen + valuelen;

    auto nameref = StringRef{name, namelen};
    auto valueref = StringRef{value, valuelen};
    auto token = http2::lookup_token(nameref);

    http2::index_header(req->res_hdidx, token, req->res_nva.size());
    http2::add_header(req->res_nva, nameref, valueref,
                      flags & NGHTTP2_NV_FLAG_NO_INDEX, token);
    break;
  }
  case NGHTTP2_PUSH_PROMISE: {
    auto req = static_cast<Request *>(nghttp2_session_get_stream_user_data(
      session, frame->push_promise.promised_stream_id));

    if (!req) {
      break;
    }

    if (req->header_buffer_size + namelen + valuelen > 64_k) {
      nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                frame->push_promise.promised_stream_id,
                                NGHTTP2_INTERNAL_ERROR);
      return 0;
    }

    req->header_buffer_size += namelen + valuelen;

    auto nameref = StringRef{name, namelen};
    auto valueref = StringRef{value, valuelen};
    auto token = http2::lookup_token(nameref);

    http2::index_header(req->req_hdidx, token, req->req_nva.size());
    http2::add_header(req->req_nva, nameref, valueref,
                      flags & NGHTTP2_NV_FLAG_NO_INDEX, token);
    break;
  }
  }
  return 0;
}
} // namespace

namespace {
int on_frame_recv_callback2(nghttp2_session *session,
                            const nghttp2_frame *frame, void *user_data) {
  int rv = 0;

  if (config.verbose) {
    verbose_on_frame_recv_callback(session, frame, user_data);
  }

  auto client = get_client(user_data);
  switch (frame->hd.type) {
  case NGHTTP2_DATA: {
    auto req = static_cast<Request *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!req) {
      return 0;
      ;
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      req->record_response_end_time();
      ++client->success;
    }

    break;
  }
  case NGHTTP2_HEADERS: {
    auto req = static_cast<Request *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    // If this is the HTTP Upgrade with OPTIONS method to avoid POST,
    // req is nullptr.
    if (!req) {
      return 0;
      ;
    }

    switch (frame->headers.cat) {
    case NGHTTP2_HCAT_RESPONSE:
    case NGHTTP2_HCAT_PUSH_RESPONSE:
      check_response_header(session, req);
      break;
    case NGHTTP2_HCAT_HEADERS:
      if (req->expect_final_response) {
        check_response_header(session, req);
        break;
      }
      if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) == 0) {
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                  frame->hd.stream_id, NGHTTP2_PROTOCOL_ERROR);
        return 0;
      }
      break;
    default:
      assert(0);
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      req->record_response_end_time();
      ++client->success;
    }

    break;
  }
  case NGHTTP2_SETTINGS:
    if ((frame->hd.flags & NGHTTP2_FLAG_ACK) == 0) {
      break;
    }
    ev_timer_stop(client->loop, &client->settings_timer);
    break;
  case NGHTTP2_PUSH_PROMISE: {
    auto req = static_cast<Request *>(nghttp2_session_get_stream_user_data(
      session, frame->push_promise.promised_stream_id));
    if (!req) {
      break;
    }

    // Reset for response header field reception
    req->header_buffer_size = 0;

    auto scheme = req->get_req_header(http2::HD__SCHEME);
    auto authority = req->get_req_header(http2::HD__AUTHORITY);
    auto path = req->get_req_header(http2::HD__PATH);

    if (!authority) {
      authority = req->get_req_header(http2::HD_HOST);
    }

    // libnghttp2 guarantees :scheme, :method, :path and (:authority |
    // host) exist and non-empty.
    if (path->value[0] != '/') {
      nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                frame->push_promise.promised_stream_id,
                                NGHTTP2_PROTOCOL_ERROR);
      break;
    }
    std::string uri = scheme->value;
    uri += "://";
    uri += authority->value;
    uri += path->value;
    urlparse_url u;
    if (urlparse_parse_url(uri.c_str(), uri.size(), 0, &u) != 0) {
      nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                frame->push_promise.promised_stream_id,
                                NGHTTP2_PROTOCOL_ERROR);
      break;
    }
    req->uri = uri;
    req->u = u;

    if (client->path_cache.count(uri)) {
      nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                frame->push_promise.promised_stream_id,
                                NGHTTP2_CANCEL);
      break;
    }

    if (config.multiply == 1) {
      client->path_cache.emplace(std::move(uri));
    }

    break;
  }
  }
  return rv;
}
} // namespace

namespace {
int before_frame_send_callback(nghttp2_session *session,
                               const nghttp2_frame *frame, void *user_data) {
  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }
  auto req = static_cast<Request *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
  assert(req);
  req->record_request_start_time();
  return 0;
}

} // namespace

namespace {
int on_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  if (config.verbose) {
    verbose_on_frame_send_callback(session, frame, user_data);
  }

  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  auto req = static_cast<Request *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
  if (!req) {
    return 0;
  }

  // If this request is using Expect/Continue, start its ContinueTimer.
  if (req->continue_timer) {
    req->continue_timer->start();
  }

  return 0;
}
} // namespace

namespace {
int on_frame_not_send_callback(nghttp2_session *session,
                               const nghttp2_frame *frame, int lib_error_code,
                               void *user_data) {
  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  auto req = static_cast<Request *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
  if (!req) {
    return 0;
  }

  std::cerr << "[ERROR] request " << req->uri
            << " failed: " << nghttp2_strerror(lib_error_code) << std::endl;

  return 0;
}
} // namespace

namespace {
int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                             uint32_t error_code, void *user_data) {
  auto client = get_client(user_data);
  auto req = static_cast<Request *>(
    nghttp2_session_get_stream_user_data(session, stream_id));

  if (!req) {
    return 0;
  }

  // If this request is using Expect/Continue, stop its ContinueTimer.
  if (req->continue_timer) {
    req->continue_timer->stop();
  }

  update_html_parser(client, req, nullptr, 0, 1);
  ++client->complete;

  if (client->all_requests_processed()) {
    nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);
  }

  return 0;
}
} // namespace

struct RequestResult {
  std::chrono::microseconds time;
};

namespace {
void print_stats(const HttpClient &client) {
  std::cout << "***** Statistics *****" << std::endl;

  std::vector<Request *> reqs;
  reqs.reserve(client.reqvec.size());
  for (const auto &req : client.reqvec) {
    if (req->timing.state == RequestState::ON_COMPLETE) {
      reqs.push_back(req.get());
    }
  }

  std::sort(std::begin(reqs), std::end(reqs),
            [](const Request *lhs, const Request *rhs) {
              const auto &ltiming = lhs->timing;
              const auto &rtiming = rhs->timing;
              return ltiming.response_end_time < rtiming.response_end_time ||
                     (ltiming.response_end_time == rtiming.response_end_time &&
                      ltiming.request_start_time < rtiming.request_start_time);
            });

  std::cout << R"(
Request timing:
  responseEnd: the  time  when  last  byte of  response  was  received
               relative to connectEnd
 requestStart: the time  just before  first byte  of request  was sent
               relative  to connectEnd.   If  '*' is  shown, this  was
               pushed by server.
      process: responseEnd - requestStart
         code: HTTP status code
         size: number  of  bytes  received as  response  body  without
               inflation.
          URI: request URI

see http://www.w3.org/TR/resource-timing/#processing-model

sorted by 'complete'

id  responseEnd requestStart  process code size request path)"
            << std::endl;

  const auto &base = client.timing.connect_end_time;
  for (const auto &req : reqs) {
    auto response_end = std::chrono::duration_cast<std::chrono::microseconds>(
      req->timing.response_end_time - base);
    auto request_start = std::chrono::duration_cast<std::chrono::microseconds>(
      req->timing.request_start_time - base);
    auto total = std::chrono::duration_cast<std::chrono::microseconds>(
      req->timing.response_end_time - req->timing.request_start_time);
    auto pushed = req->stream_id % 2 == 0;

    std::cout << std::setw(3) << req->stream_id << " " << std::setw(11)
              << ("+" + util::format_duration(response_end)) << " "
              << (pushed ? "*" : " ") << std::setw(11)
              << ("+" + util::format_duration(request_start)) << " "
              << std::setw(8) << util::format_duration(total) << " "
              << std::setw(4) << req->status << " " << std::setw(4)
              << util::utos_unit(req->response_len) << " "
              << req->make_reqpath() << std::endl;
  }
}
} // namespace

namespace {
int communicate(
  const std::string &scheme, const std::string &host, uint16_t port,
  std::vector<
    std::tuple<std::string, nghttp2_data_provider2 *, int64_t, nghttp2_extpri>>
    requests,
  const nghttp2_session_callbacks *callbacks) {
  int result = 0;
  auto loop = EV_DEFAULT;
  SSL_CTX *ssl_ctx = nullptr;
  if (scheme == "https") {
    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
      std::cerr << "[ERROR] Failed to create SSL_CTX: "
                << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
      result = -1;
      goto fin;
    }

    auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                    SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

#ifdef SSL_OP_ENABLE_KTLS
    if (config.ktls) {
      ssl_opts |= SSL_OP_ENABLE_KTLS;
    }
#endif // SSL_OP_ENABLE_KTLS

    SSL_CTX_set_options(ssl_ctx, ssl_opts);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

    if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
      std::cerr << "[WARNING] Could not load system trusted CA certificates: "
                << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    }

    if (nghttp2::tls::ssl_ctx_set_proto_versions(
          ssl_ctx, nghttp2::tls::NGHTTP2_TLS_MIN_VERSION,
          nghttp2::tls::NGHTTP2_TLS_MAX_VERSION) != 0) {
      std::cerr << "[ERROR] Could not set TLS versions" << std::endl;
      result = -1;
      goto fin;
    }

    if (SSL_CTX_set_cipher_list(ssl_ctx, tls::DEFAULT_CIPHER_LIST.data()) ==
        0) {
      std::cerr << "[ERROR] " << ERR_error_string(ERR_get_error(), nullptr)
                << std::endl;
      result = -1;
      goto fin;
    }

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
    if (SSL_CTX_set_ciphersuites(ssl_ctx,
                                 tls::DEFAULT_TLS13_CIPHER_LIST.data()) == 0) {
      std::cerr << "[ERROR] " << ERR_error_string(ERR_get_error(), nullptr)
                << std::endl;
      result = -1;
      goto fin;
    }
#endif // NGHTTP2_OPENSSL_IS_WOLFSSL

    if (!config.keyfile.empty()) {
      if (SSL_CTX_use_PrivateKey_file(ssl_ctx, config.keyfile.c_str(),
                                      SSL_FILETYPE_PEM) != 1) {
        std::cerr << "[ERROR] " << ERR_error_string(ERR_get_error(), nullptr)
                  << std::endl;
        result = -1;
        goto fin;
      }
    }
    if (!config.certfile.empty()) {
      if (SSL_CTX_use_certificate_chain_file(ssl_ctx,
                                             config.certfile.c_str()) != 1) {
        std::cerr << "[ERROR] " << ERR_error_string(ERR_get_error(), nullptr)
                  << std::endl;
        result = -1;
        goto fin;
      }
    }

    auto proto_list = util::get_default_alpn();

    SSL_CTX_set_alpn_protos(ssl_ctx, proto_list.data(), proto_list.size());

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
    if (!SSL_CTX_add_cert_compression_alg(
          ssl_ctx, nghttp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
          nghttp2::tls::cert_compress, nghttp2::tls::cert_decompress)) {
      std::cerr << "[ERROR] SSL_CTX_add_cert_compression_alg failed."
                << std::endl;
      result = -1;
      goto fin;
    }
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

    if (tls::setup_keylog_callback(ssl_ctx) != 0) {
      std::cerr << "[ERROR] Failed to setup keylog" << std::endl;

      result = -1;

      goto fin;
    }
  }
  {
    HttpClient client{callbacks, loop, ssl_ctx};

    for (auto &req : requests) {
      for (int i = 0; i < config.multiply; ++i) {
        client.add_request(std::get<0>(req), std::get<1>(req), std::get<2>(req),
                           std::get<3>(req));
      }
    }
    client.update_hostport();

    client.record_start_time();

    if (client.resolve_host(host, port) != 0) {
      goto fin;
    }

    client.record_domain_lookup_end_time();

    if (client.initiate_connection() != 0) {
      std::cerr << "[ERROR] Could not connect to " << host << ", port " << port
                << std::endl;
      goto fin;
    }

    ev_set_userdata(loop, &client);
    ev_run(loop, 0);
    ev_set_userdata(loop, nullptr);

#ifdef HAVE_JANSSON
    if (!config.harfile.empty()) {
      FILE *outfile;
      if (config.harfile == "-") {
        outfile = stdout;
      } else {
        outfile = fopen(config.harfile.c_str(), "wb");
      }

      if (outfile) {
        client.output_har(outfile);

        if (outfile != stdout) {
          fclose(outfile);
        }
      } else {
        std::cerr << "Cannot open file " << config.harfile << ". "
                  << "har file could not be created." << std::endl;
      }
    }
#endif // HAVE_JANSSON

    if (client.success != client.reqvec.size()) {
      std::cerr << "Some requests were not processed. total="
                << client.reqvec.size() << ", processed=" << client.success
                << std::endl;
    }
    if (config.stat) {
      print_stats(client);
    }
  }
fin:
  if (ssl_ctx) {
    SSL_CTX_free(ssl_ctx);
  }
  return result;
}
} // namespace

namespace {
nghttp2_ssize file_read_callback(nghttp2_session *session, int32_t stream_id,
                                 uint8_t *buf, size_t length,
                                 uint32_t *data_flags,
                                 nghttp2_data_source *source, void *user_data) {
  int rv;
  auto req = static_cast<Request *>(
    nghttp2_session_get_stream_user_data(session, stream_id));
  assert(req);
  int fd = source->fd;
  ssize_t nread;

  while ((nread = pread(fd, buf, length, req->data_offset)) == -1 &&
         errno == EINTR)
    ;

  if (nread == -1) {
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
  }

  req->data_offset += nread;

  if (req->data_offset == req->data_length) {
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
    if (!config.trailer.empty()) {
      std::vector<nghttp2_nv> nva;
      nva.reserve(config.trailer.size());
      for (auto &kv : config.trailer) {
        nva.push_back(http2::make_field_nv(kv.name, kv.value,
                                           http2::no_index(kv.no_index)));
      }
      rv = nghttp2_submit_trailer(session, stream_id, nva.data(), nva.size());
      if (rv != 0) {
        if (nghttp2_is_fatal(rv)) {
          return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
      } else {
        *data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
      }
    }

    return static_cast<nghttp2_ssize>(nread);
  }

  if (req->data_offset > req->data_length || nread == 0) {
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
  }

  return static_cast<nghttp2_ssize>(nread);
}
} // namespace

namespace {
int run(char **uris, int n) {
  nghttp2_session_callbacks *callbacks;

  nghttp2_session_callbacks_new(&callbacks);
  auto cbsdel = defer(nghttp2_session_callbacks_del, callbacks);

  nghttp2_session_callbacks_set_on_stream_close_callback(
    callbacks, on_stream_close_callback);

  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       on_frame_recv_callback2);

  if (config.verbose) {
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(
      callbacks, verbose_on_invalid_frame_recv_callback);

    nghttp2_session_callbacks_set_error_callback2(callbacks,
                                                  verbose_error_callback);
  }

  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
    callbacks, on_data_chunk_recv_callback);

  nghttp2_session_callbacks_set_on_begin_headers_callback(
    callbacks, on_begin_headers_callback);

  nghttp2_session_callbacks_set_on_header_callback(callbacks,
                                                   on_header_callback);

  nghttp2_session_callbacks_set_before_frame_send_callback(
    callbacks, before_frame_send_callback);

  nghttp2_session_callbacks_set_on_frame_send_callback(callbacks,
                                                       on_frame_send_callback);

  nghttp2_session_callbacks_set_on_frame_not_send_callback(
    callbacks, on_frame_not_send_callback);

  if (config.padding) {
    nghttp2_session_callbacks_set_select_padding_callback2(
      callbacks, select_padding_callback);
  }

  std::string prev_scheme;
  std::string prev_host;
  uint16_t prev_port = 0;
  int failures = 0;
  int data_fd = -1;
  nghttp2_data_provider2 data_prd;
  struct stat data_stat {};

  if (!config.datafile.empty()) {
    if (config.datafile == "-") {
      if (fstat(0, &data_stat) == 0 &&
          (data_stat.st_mode & S_IFMT) == S_IFREG) {
        // use STDIN if it is a regular file
        data_fd = 0;
      } else {
        // copy the contents of STDIN to a temporary file
        char tempfn[] = "/tmp/nghttp.temp.XXXXXX";
        data_fd = mkstemp(tempfn);
        if (data_fd == -1) {
          std::cerr << "[ERROR] Could not create a temporary file in /tmp"
                    << std::endl;
          return 1;
        }
        if (unlink(tempfn) != 0) {
          std::cerr << "[WARNING] failed to unlink temporary file:" << tempfn
                    << std::endl;
        }
        while (1) {
          std::array<char, 1_k> buf;
          ssize_t rret, wret;
          while ((rret = read(0, buf.data(), buf.size())) == -1 &&
                 errno == EINTR)
            ;
          if (rret == 0)
            break;
          if (rret == -1) {
            std::cerr << "[ERROR] I/O error while reading from STDIN"
                      << std::endl;
            return 1;
          }
          while ((wret = write(data_fd, buf.data(), rret)) == -1 &&
                 errno == EINTR)
            ;
          if (wret != rret) {
            std::cerr << "[ERROR] I/O error while writing to temporary file"
                      << std::endl;
            return 1;
          }
        }
        if (fstat(data_fd, &data_stat) == -1) {
          close(data_fd);
          std::cerr << "[ERROR] Could not stat temporary file" << std::endl;
          return 1;
        }
      }
    } else {
      data_fd = open(config.datafile.c_str(), O_RDONLY | O_BINARY);
      if (data_fd == -1) {
        std::cerr << "[ERROR] Could not open file " << config.datafile
                  << std::endl;
        return 1;
      }
      if (fstat(data_fd, &data_stat) == -1) {
        close(data_fd);
        std::cerr << "[ERROR] Could not stat file " << config.datafile
                  << std::endl;
        return 1;
      }
    }
    data_prd.source.fd = data_fd;
    data_prd.read_callback = file_read_callback;
  }
  std::vector<
    std::tuple<std::string, nghttp2_data_provider2 *, int64_t, nghttp2_extpri>>
    requests;

  size_t next_extpri_idx = 0;

  for (int i = 0; i < n; ++i) {
    urlparse_url u;
    auto uri = strip_fragment(uris[i]);
    if (urlparse_parse_url(uri.c_str(), uri.size(), 0, &u) != 0) {
      ++next_extpri_idx;
      std::cerr << "[ERROR] Could not parse URI " << uri << std::endl;
      continue;
    }
    if (!util::has_uri_field(u, URLPARSE_SCHEMA)) {
      ++next_extpri_idx;
      std::cerr << "[ERROR] URI " << uri << " does not have scheme part"
                << std::endl;
      continue;
    }
    auto port = util::has_uri_field(u, URLPARSE_PORT)
                  ? u.port
                  : util::get_default_port(uri.c_str(), u);
    auto host = decode_host(util::get_uri_field(uri.c_str(), u, URLPARSE_HOST));
    if (!util::fieldeq(uri.c_str(), u, URLPARSE_SCHEMA, prev_scheme.c_str()) ||
        host != prev_host || port != prev_port) {
      if (!requests.empty()) {
        if (communicate(prev_scheme, prev_host, prev_port, std::move(requests),
                        callbacks) != 0) {
          ++failures;
        }
        requests.clear();
      }
      prev_scheme = util::get_uri_field(uri.c_str(), u, URLPARSE_SCHEMA);
      prev_host = std::move(host);
      prev_port = port;
    }
    requests.emplace_back(uri, data_fd == -1 ? nullptr : &data_prd,
                          data_stat.st_size, config.extpris[next_extpri_idx++]);
  }
  if (!requests.empty()) {
    if (communicate(prev_scheme, prev_host, prev_port, std::move(requests),
                    callbacks) != 0) {
      ++failures;
    }
  }
  return failures;
}
} // namespace

namespace {
void print_version(std::ostream &out) {
  out << "nghttp nghttp2/" NGHTTP2_VERSION << std::endl;
}
} // namespace

namespace {
void print_usage(std::ostream &out) {
  out << R"(Usage: nghttp [OPTIONS]... <URI>...
HTTP/2 client)"
      << std::endl;
}
} // namespace

namespace {
void print_help(std::ostream &out) {
  print_usage(out);
  out << R"(
  <URI>       Specify URI to access.
Options:
  -v, --verbose
              Print   debug   information   such  as   reception   and
              transmission of frames and name/value pairs.  Specifying
              this option multiple times increases verbosity.
  -n, --null-out
              Discard downloaded data.
  -O, --remote-name
              Save  download  data  in  the  current  directory.   The
              filename is  derived from  URI.  If  URI ends  with '/',
              'index.html'  is used  as a  filename.  Not  implemented
              yet.
  -t, --timeout=<DURATION>
              Timeout each request after <DURATION>.  Set 0 to disable
              timeout.
  -w, --window-bits=<N>
              Sets the stream level initial window size to 2**<N>-1.
  -W, --connection-window-bits=<N>
              Sets  the  connection  level   initial  window  size  to
              2**<N>-1.
  -a, --get-assets
              Download assets  such as stylesheets, images  and script
              files linked  from the downloaded resource.   Only links
              whose  origins are  the same  with the  linking resource
              will be downloaded.   nghttp prioritizes resources using
              HTTP/2 dependency  based priority.  The  priority order,
              from highest to lowest,  is html itself, css, javascript
              and images.
  -s, --stat  Print statistics.
  -H, --header=<HEADER>
              Add a header to the requests.  Example: -H':method: PUT'
  --trailer=<HEADER>
              Add a trailer header to the requests.  <HEADER> must not
              include pseudo header field  (header field name starting
              with ':').  To  send trailer, one must use  -d option to
              send request body.  Example: --trailer 'foo: bar'.
  --cert=<CERT>
              Use  the specified  client certificate  file.  The  file
              must be in PEM format.
  --key=<KEY> Use the  client private key  file.  The file must  be in
              PEM format.
  -d, --data=<PATH>
              Post FILE to server. If '-'  is given, data will be read
              from stdin.
  -m, --multiply=<N>
              Request each URI <N> times.  By default, same URI is not
              requested twice.  This option disables it too.
  -u, --upgrade
              Perform HTTP Upgrade for HTTP/2.  This option is ignored
              if the request URI has https scheme.  If -d is used, the
              HTTP upgrade request is performed with OPTIONS method.
  --extpri=<PRI>
              Sets RFC 9218 priority of  given URI.  <PRI> must be the
              wire format  of priority  header field  (e.g., "u=3,i").
              This  option  can  be  used  multiple  times,  and  N-th
              --extpri option sets priority of N-th URI in the command
              line.  If  the number  of this option  is less  than the
              number of  URI, the last  option value is  repeated.  If
              there  is   no  --extpri  option,  urgency   is  3,  and
              incremental is false.
  -M, --peer-max-concurrent-streams=<N>
              Use  <N>  as  SETTINGS_MAX_CONCURRENT_STREAMS  value  of
              remote endpoint as if it  is received in SETTINGS frame.
              Default: 100
  -c, --header-table-size=<SIZE>
              Specify decoder  header table  size.  If this  option is
              used  multiple times,  and the  minimum value  among the
              given values except  for last one is  strictly less than
              the last  value, that minimum  value is set  in SETTINGS
              frame  payload  before  the   last  value,  to  simulate
              multiple header table size change.
  --encoder-header-table-size=<SIZE>
              Specify encoder header table size.  The decoder (server)
              specifies  the maximum  dynamic table  size it  accepts.
              Then the negotiated dynamic table size is the minimum of
              this option value and the value which server specified.
  -b, --padding=<N>
              Add at  most <N>  bytes to a  frame payload  as padding.
              Specify 0 to disable padding.
  -r, --har=<PATH>
              Output HTTP  transactions <PATH> in HAR  format.  If '-'
              is given, data is written to stdout.
  --color     Force colored log output.
  --continuation
              Send large header to test CONTINUATION.
  --no-content-length
              Don't send content-length header field.
  --hexdump   Display the  incoming traffic in  hexadecimal (Canonical
              hex+ASCII display).  If SSL/TLS  is used, decrypted data
              are used.
  --no-push   Disable server push.
  --max-concurrent-streams=<N>
              The  number of  concurrent  pushed  streams this  client
              accepts.
  --expect-continue
              Perform an Expect/Continue handshake:  wait to send DATA
              (up to  a short  timeout)  until the server sends  a 100
              Continue interim response. This option is ignored unless
              combined with the -d option.
  -y, --no-verify-peer
              Suppress  warning  on  server  certificate  verification
              failure.
  --ktls      Enable ktls.
  --version   Display version information and exit.
  -h, --help  Display this help and exit.

--

  The <SIZE> argument is an integer and an optional unit (e.g., 10K is
  10 * 1024).  Units are K, M and G (powers of 1024).

  The <DURATION> argument is an integer and an optional unit (e.g., 1s
  is 1 second and 500ms is 500 milliseconds).  Units are h, m, s or ms
  (hours, minutes, seconds and milliseconds, respectively).  If a unit
  is omitted, a second is used as unit.)"
      << std::endl;
}
} // namespace

int main(int argc, char **argv) {
  bool color = false;
  while (1) {
    static int flag = 0;
    constexpr static option long_options[] = {
      {"verbose", no_argument, nullptr, 'v'},
      {"null-out", no_argument, nullptr, 'n'},
      {"remote-name", no_argument, nullptr, 'O'},
      {"timeout", required_argument, nullptr, 't'},
      {"window-bits", required_argument, nullptr, 'w'},
      {"connection-window-bits", required_argument, nullptr, 'W'},
      {"get-assets", no_argument, nullptr, 'a'},
      {"stat", no_argument, nullptr, 's'},
      {"help", no_argument, nullptr, 'h'},
      {"header", required_argument, nullptr, 'H'},
      {"data", required_argument, nullptr, 'd'},
      {"multiply", required_argument, nullptr, 'm'},
      {"upgrade", no_argument, nullptr, 'u'},
      {"weight", required_argument, nullptr, 'p'},
      {"peer-max-concurrent-streams", required_argument, nullptr, 'M'},
      {"header-table-size", required_argument, nullptr, 'c'},
      {"padding", required_argument, nullptr, 'b'},
      {"har", required_argument, nullptr, 'r'},
      {"no-verify-peer", no_argument, nullptr, 'y'},
      {"cert", required_argument, &flag, 1},
      {"key", required_argument, &flag, 2},
      {"color", no_argument, &flag, 3},
      {"continuation", no_argument, &flag, 4},
      {"version", no_argument, &flag, 5},
      {"no-content-length", no_argument, &flag, 6},
      {"no-dep", no_argument, &flag, 7},
      {"trailer", required_argument, &flag, 9},
      {"hexdump", no_argument, &flag, 10},
      {"no-push", no_argument, &flag, 11},
      {"max-concurrent-streams", required_argument, &flag, 12},
      {"expect-continue", no_argument, &flag, 13},
      {"encoder-header-table-size", required_argument, &flag, 14},
      {"ktls", no_argument, &flag, 15},
      {"no-rfc7540-pri", no_argument, &flag, 16},
      {"extpri", required_argument, &flag, 17},
      {nullptr, 0, nullptr, 0}};
    int option_index = 0;
    int c =
      getopt_long(argc, argv, "M:Oab:c:d:m:np:r:hH:vst:uw:yW:", long_options,
                  &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'M': {
      // peer-max-concurrent-streams option
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-M: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.peer_max_concurrent_streams = *n;
      break;
    }
    case 'O':
      config.remote_name = true;
      break;
    case 'h':
      print_help(std::cout);
      exit(EXIT_SUCCESS);
    case 'b': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-b: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.padding = *n;
      break;
    }
    case 'n':
      config.null_out = true;
      break;
    case 'p':
      std::cerr << "[WARNING]: --weight option has been deprecated."
                << std::endl;
      break;
    case 'r':
#ifdef HAVE_JANSSON
      config.harfile = optarg;
#else  // !HAVE_JANSSON
      std::cerr << "[WARNING]: -r, --har option is ignored because\n"
                << "the binary was not compiled with libjansson." << std::endl;
#endif // !HAVE_JANSSON
      break;
    case 'v':
      ++config.verbose;
      break;
    case 't': {
      auto d = util::parse_duration_with_unit(optarg);
      if (!d) {
        std::cerr << "-t: bad timeout value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.timeout = *d;
      break;
    }
    case 'u':
      config.upgrade = true;
      break;
    case 'w':
    case 'W': {
      auto n = util::parse_uint(optarg);
      if (!n || n > 30) {
        std::cerr << "-" << static_cast<char>(c)
                  << ": specify the integer in the range [0, 30], inclusive"
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      if (c == 'w') {
        config.window_bits = *n;
      } else {
        config.connection_window_bits = *n;
      }
      break;
    }
    case 'H': {
      char *header = optarg;
      // Skip first possible ':' in the header name
      char *value = strchr(optarg + 1, ':');
      if (!value || (header[0] == ':' && header + 1 == value)) {
        std::cerr << "-H: invalid header: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      *value = 0;
      value++;
      while (isspace(*value)) {
        value++;
      }
      if (*value == 0) {
        // This could also be a valid case for suppressing a header
        // similar to curl
        std::cerr << "-H: invalid header - value missing: " << optarg
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      config.headers.emplace_back(header, value, false);
      util::inp_strlower(config.headers.back().name);
      break;
    }
    case 'a':
#ifdef HAVE_LIBXML2
      config.get_assets = true;
#else  // !HAVE_LIBXML2
      std::cerr << "[WARNING]: -a, --get-assets option is ignored because\n"
                << "the binary was not compiled with libxml2." << std::endl;
#endif // !HAVE_LIBXML2
      break;
    case 's':
      config.stat = true;
      break;
    case 'd':
      config.datafile = optarg;
      break;
    case 'm': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-m: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.multiply = *n;
      break;
    }
    case 'c': {
      auto n = util::parse_uint_with_unit(optarg);
      if (!n) {
        std::cerr << "-c: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      if (n > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "-c: Value too large.  It should be less than or equal to "
                  << std::numeric_limits<uint32_t>::max() << std::endl;
        exit(EXIT_FAILURE);
      }
      config.header_table_size = *n;
      config.min_header_table_size = std::min(config.min_header_table_size, *n);
      break;
    }
    case 'y':
      config.verify_peer = false;
      break;
    case '?':
      util::show_candidates(argv[optind - 1], long_options);
      exit(EXIT_FAILURE);
    case 0:
      switch (flag) {
      case 1:
        // cert option
        config.certfile = optarg;
        break;
      case 2:
        // key option
        config.keyfile = optarg;
        break;
      case 3:
        // color option
        color = true;
        break;
      case 4:
        // continuation option
        config.continuation = true;
        break;
      case 5:
        // version option
        print_version(std::cout);
        exit(EXIT_SUCCESS);
      case 6:
        // no-content-length option
        config.no_content_length = true;
        break;
      case 7:
        // no-dep option
        std::cerr << "[WARNING]: --no-dep option has been deprecated."
                  << std::endl;
        break;
      case 9: {
        // trailer option
        auto header = optarg;
        auto value = strchr(optarg, ':');
        if (!value) {
          std::cerr << "--trailer: invalid header: " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        *value = 0;
        value++;
        while (isspace(*value)) {
          value++;
        }
        if (*value == 0) {
          // This could also be a valid case for suppressing a header
          // similar to curl
          std::cerr << "--trailer: invalid header - value missing: " << optarg
                    << std::endl;
          exit(EXIT_FAILURE);
        }
        config.trailer.emplace_back(header, value, false);
        util::inp_strlower(config.trailer.back().name);
        break;
      }
      case 10:
        // hexdump option
        config.hexdump = true;
        break;
      case 11:
        // no-push option
        config.no_push = true;
        break;
      case 12: {
        // max-concurrent-streams option
        auto n = util::parse_uint(optarg);
        if (!n) {
          std::cerr << "--max-concurrent-streams: Bad option value: " << optarg
                    << std::endl;
          exit(EXIT_FAILURE);
        }
        config.max_concurrent_streams = *n;
        break;
      }
      case 13:
        // expect-continue option
        config.expect_continue = true;
        break;
      case 14: {
        // encoder-header-table-size option
        auto n = util::parse_uint_with_unit(optarg);
        if (!n) {
          std::cerr << "--encoder-header-table-size: Bad option value: "
                    << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        if (n > std::numeric_limits<uint32_t>::max()) {
          std::cerr << "--encoder-header-table-size: Value too large.  It "
                       "should be less than or equal to "
                    << std::numeric_limits<uint32_t>::max() << std::endl;
          exit(EXIT_FAILURE);
        }
        config.encoder_header_table_size = *n;
        break;
      }
      case 15:
        // ktls option
        config.ktls = true;
        break;
      case 16:
        // no-rfc7540-pri option
        std::cerr << "[WARNING]: --no-rfc7540-pri option has been deprecated."
                  << std::endl;
        break;
      case 17: {
        // extpri option
        nghttp2_extpri pri{
          .urgency = NGHTTP2_EXTPRI_DEFAULT_URGENCY,
        };

        if (nghttp2_extpri_parse_priority(
              &pri, reinterpret_cast<const uint8_t *>(optarg),
              strlen(optarg)) != 0) {
          std::cerr << "--extpri: Bad option value: " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }

        config.extpris.emplace_back(std::move(pri));

        break;
      }
      }
      break;
    default:
      break;
    }
  }

  nghttp2_extpri extpri_to_fill{
    .urgency = NGHTTP2_EXTPRI_DEFAULT_URGENCY,
  };

  if (!config.extpris.empty()) {
    extpri_to_fill = config.extpris.back();
  }
  config.extpris.insert(std::end(config.extpris), argc - optind,
                        extpri_to_fill);

  // Find scheme overridden by extra header fields.
  auto scheme_it =
    std::find_if(std::begin(config.headers), std::end(config.headers),
                 [](const Header &nv) { return nv.name == ":scheme"; });
  if (scheme_it != std::end(config.headers)) {
    config.scheme_override = (*scheme_it).value;
  }

  // Find host and port overridden by extra header fields.
  auto authority_it =
    std::find_if(std::begin(config.headers), std::end(config.headers),
                 [](const Header &nv) { return nv.name == ":authority"; });
  if (authority_it == std::end(config.headers)) {
    authority_it =
      std::find_if(std::begin(config.headers), std::end(config.headers),
                   [](const Header &nv) { return nv.name == "host"; });
  }

  if (authority_it != std::end(config.headers)) {
    // authority_it may looks like "host:port".
    auto uri = "https://" + (*authority_it).value;
    urlparse_url u;
    if (urlparse_parse_url(uri.c_str(), uri.size(), 0, &u) != 0) {
      std::cerr << "[ERROR] Could not parse authority in "
                << (*authority_it).name << ": " << (*authority_it).value
                << std::endl;
      exit(EXIT_FAILURE);
    }

    config.host_override = util::get_uri_field(uri.c_str(), u, URLPARSE_HOST);
    if (util::has_uri_field(u, URLPARSE_PORT)) {
      config.port_override = u.port;
    }
  }

  set_color_output(color || isatty(fileno(stdout)));

  nghttp2_option_set_peer_max_concurrent_streams(
    config.http2_option, config.peer_max_concurrent_streams);

  if (config.encoder_header_table_size != -1) {
    nghttp2_option_set_max_deflate_dynamic_table_size(
      config.http2_option, config.encoder_header_table_size);
  }

  struct sigaction act {};
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, nullptr);
  reset_timer();
  return run(argv + optind, argc - optind);
}

} // namespace nghttp2

int main(int argc, char **argv) {
  return nghttp2::run_app(nghttp2::main, argc, argv);
}
