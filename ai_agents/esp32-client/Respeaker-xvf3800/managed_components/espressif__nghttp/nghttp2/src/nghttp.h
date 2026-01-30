/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#ifndef NGHTTP_H
#define NGHTTP_H

#include "nghttp2_config.h"

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H

#include <string>
#include <vector>
#include <set>
#include <chrono>
#include <memory>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ev.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#include "llhttp.h"

#include "memchunk.h"
#include "http2.h"
#include "nghttp2_gzip.h"
#include "template.h"

namespace nghttp2 {

class HtmlParser;

struct Config {
  Config();
  ~Config();

  Headers headers;
  Headers trailer;
  std::vector<nghttp2_extpri> extpris;
  std::string certfile;
  std::string keyfile;
  std::string datafile;
  std::string harfile;
  std::string scheme_override;
  std::string host_override;
  nghttp2_option *http2_option;
  int64_t header_table_size;
  int64_t min_header_table_size;
  int64_t encoder_header_table_size;
  size_t padding;
  size_t max_concurrent_streams;
  size_t peer_max_concurrent_streams;
  int multiply;
  // milliseconds
  ev_tstamp timeout;
  int window_bits;
  int connection_window_bits;
  int verbose;
  uint16_t port_override;
  bool null_out;
  bool remote_name;
  bool get_assets;
  bool stat;
  bool upgrade;
  bool continuation;
  bool no_content_length;
  bool hexdump;
  bool no_push;
  bool expect_continue;
  bool verify_peer;
  bool ktls;
};

enum class RequestState { INITIAL, ON_REQUEST, ON_RESPONSE, ON_COMPLETE };

struct RequestTiming {
  // The point in time when request is started to be sent.
  // Corresponds to requestStart in Resource Timing TR.
  std::chrono::steady_clock::time_point request_start_time;
  // The point in time when first byte of response is received.
  // Corresponds to responseStart in Resource Timing TR.
  std::chrono::steady_clock::time_point response_start_time;
  // The point in time when last byte of response is received.
  // Corresponds to responseEnd in Resource Timing TR.
  std::chrono::steady_clock::time_point response_end_time;
  RequestState state;
  RequestTiming() : state(RequestState::INITIAL) {}
};

struct Request; // forward declaration for ContinueTimer

struct ContinueTimer {
  ContinueTimer(struct ev_loop *loop, Request *req);
  ~ContinueTimer();

  void start();
  void stop();

  // Schedules an immediate run of the continue callback on the loop, if the
  // callback has not already been run
  void dispatch_continue();

  struct ev_loop *loop;
  ev_timer timer;
};

struct Request {
  // For pushed request, |uri| is empty and |u| is zero-cleared.
  Request(const std::string &uri, const urlparse_url &u,
          const nghttp2_data_provider2 *data_prd, int64_t data_length,
          const nghttp2_extpri &extpri, int level = 0);
  ~Request();

  void init_inflater();

  void init_html_parser();
  int update_html_parser(const uint8_t *data, size_t len, int fin);

  std::string make_reqpath() const;

  bool is_ipv6_literal_addr() const;

  Headers::value_type *get_res_header(int32_t token);
  Headers::value_type *get_req_header(int32_t token);

  void record_request_start_time();
  void record_response_start_time();
  void record_response_end_time();

  // Returns scheme taking into account overridden scheme.
  StringRef get_real_scheme() const;
  // Returns request host, without port, taking into account
  // overridden host.
  StringRef get_real_host() const;
  // Returns request port, taking into account overridden host, port,
  // and scheme.
  uint16_t get_real_port() const;

  Headers res_nva;
  Headers req_nva;
  std::string method;
  // URI without fragment
  std::string uri;
  urlparse_url u;
  nghttp2_extpri extpri;
  RequestTiming timing;
  int64_t data_length;
  int64_t data_offset;
  // Number of bytes received from server
  int64_t response_len;
  nghttp2_gzip *inflater;
  std::unique_ptr<HtmlParser> html_parser;
  const nghttp2_data_provider2 *data_prd;
  size_t header_buffer_size;
  int32_t stream_id;
  int status;
  // Recursion level: 0: first entity, 1: entity linked from first entity
  int level;
  http2::HeaderIndex res_hdidx;
  // used for incoming PUSH_PROMISE
  http2::HeaderIndex req_hdidx;
  bool expect_final_response;
  // only assigned if this request is using Expect/Continue
  std::unique_ptr<ContinueTimer> continue_timer;
};

struct SessionTiming {
  // The point in time when operation was started.  Corresponds to
  // startTime in Resource Timing TR, but recorded in system clock time.
  std::chrono::system_clock::time_point system_start_time;
  // Same as above, but recorded in steady clock time.
  std::chrono::steady_clock::time_point start_time;
  // The point in time when DNS resolution was completed.  Corresponds
  // to domainLookupEnd in Resource Timing TR.
  std::chrono::steady_clock::time_point domain_lookup_end_time;
  // The point in time when connection was established or SSL/TLS
  // handshake was completed.  Corresponds to connectEnd in Resource
  // Timing TR.
  std::chrono::steady_clock::time_point connect_end_time;
};

enum class ClientState { IDLE, CONNECTED };

struct HttpClient {
  HttpClient(const nghttp2_session_callbacks *callbacks, struct ev_loop *loop,
             SSL_CTX *ssl_ctx);
  ~HttpClient();

  bool need_upgrade() const;
  int resolve_host(const std::string &host, uint16_t port);
  int initiate_connection();
  void disconnect();

  int noop();
  int read_clear();
  int write_clear();
  int connected();
  int tls_handshake();
  int read_tls();
  int write_tls();

  int do_read();
  int do_write();

  int on_upgrade_connect();
  int on_upgrade_read(const uint8_t *data, size_t len);
  int on_read(const uint8_t *data, size_t len);
  int on_write();

  int connection_made();
  void connect_fail();
  void request_done(Request *req);

  void signal_write();

  bool all_requests_processed() const;
  void update_hostport();
  bool add_request(const std::string &uri,
                   const nghttp2_data_provider2 *data_prd, int64_t data_length,
                   const nghttp2_extpri &extpri, int level = 0);

  void record_start_time();
  void record_domain_lookup_end_time();
  void record_connect_end_time();

#ifdef HAVE_JANSSON
  void output_har(FILE *outfile);
#endif // HAVE_JANSSON

  MemchunkPool mcpool;
  DefaultMemchunks wb;
  std::vector<std::unique_ptr<Request>> reqvec;
  // Insert path already added in reqvec to prevent multiple request
  // for 1 resource.
  std::set<std::string> path_cache;
  std::string scheme;
  std::string host;
  std::string hostport;
  // Used for parse the HTTP upgrade response from server
  std::unique_ptr<llhttp_t> htp;
  SessionTiming timing;
  ev_io wev;
  ev_io rev;
  ev_timer wt;
  ev_timer rt;
  ev_timer settings_timer;
  std::function<int(HttpClient &)> readfn, writefn;
  std::function<int(HttpClient &, const uint8_t *, size_t)> on_readfn;
  std::function<int(HttpClient &)> on_writefn;
  nghttp2_session *session;
  const nghttp2_session_callbacks *callbacks;
  struct ev_loop *loop;
  SSL_CTX *ssl_ctx;
  SSL *ssl;
  addrinfo *addrs;
  addrinfo *next_addr;
  addrinfo *cur_addr;
  // The number of completed requests, including failed ones.
  size_t complete;
  // The number of requests that local endpoint received END_STREAM
  // from peer.
  size_t success;
  // The length of settings_payload
  size_t settings_payloadlen;
  ClientState state;
  // The HTTP status code of the response message of HTTP Upgrade.
  unsigned int upgrade_response_status_code;
  int fd;
  // true if the response message of HTTP Upgrade request is fully
  // received. It is not relevant the upgrade succeeds, or not.
  bool upgrade_response_complete;
  // SETTINGS payload sent as token68 in HTTP Upgrade
  std::array<uint8_t, 128> settings_payload;

  enum { ERR_CONNECT_FAIL = -100 };
};

} // namespace nghttp2

#endif // NGHTTP_H
