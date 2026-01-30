/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
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
#include "shrpx_https_upstream.h"

#include <cassert>
#include <set>
#include <sstream>

#include "shrpx_client_handler.h"
#include "shrpx_downstream.h"
#include "shrpx_downstream_connection.h"
#include "shrpx_http.h"
#include "shrpx_config.h"
#include "shrpx_error.h"
#include "shrpx_log_config.h"
#include "shrpx_worker.h"
#include "shrpx_http2_session.h"
#include "shrpx_log.h"
#ifdef HAVE_MRUBY
#  include "shrpx_mruby.h"
#endif // HAVE_MRUBY
#include "http2.h"
#include "util.h"
#include "template.h"
#include "base64.h"
#include "urlparse.h"

using namespace nghttp2;

namespace shrpx {

namespace {
int htp_msg_begin(llhttp_t *htp);
int htp_uricb(llhttp_t *htp, const char *data, size_t len);
int htp_hdr_keycb(llhttp_t *htp, const char *data, size_t len);
int htp_hdr_valcb(llhttp_t *htp, const char *data, size_t len);
int htp_hdrs_completecb(llhttp_t *htp);
int htp_bodycb(llhttp_t *htp, const char *data, size_t len);
int htp_msg_completecb(llhttp_t *htp);
} // namespace

namespace {
constexpr llhttp_settings_t htp_hooks = {
  htp_msg_begin,       // llhttp_cb      on_message_begin;
  htp_uricb,           // llhttp_data_cb on_url;
  nullptr,             // llhttp_data_cb on_status;
  nullptr,             // llhttp_data_cb on_method;
  nullptr,             // llhttp_data_cb on_version;
  htp_hdr_keycb,       // llhttp_data_cb on_header_field;
  htp_hdr_valcb,       // llhttp_data_cb on_header_value;
  nullptr,             // llhttp_data_cb on_chunk_extension_name;
  nullptr,             // llhttp_data_cb on_chunk_extension_value;
  htp_hdrs_completecb, // llhttp_cb      on_headers_complete;
  htp_bodycb,          // llhttp_data_cb on_body;
  htp_msg_completecb,  // llhttp_cb      on_message_complete;
  nullptr,             // llhttp_cb      on_url_complete;
  nullptr,             // llhttp_cb      on_status_complete;
  nullptr,             // llhttp_cb      on_method_complete;
  nullptr,             // llhttp_cb      on_version_complete;
  nullptr,             // llhttp_cb      on_header_field_complete;
  nullptr,             // llhttp_cb      on_header_value_complete;
  nullptr,             // llhttp_cb      on_chunk_extension_name_complete;
  nullptr,             // llhttp_cb      on_chunk_extension_value_complete;
  nullptr,             // llhttp_cb      on_chunk_header;
  nullptr,             // llhttp_cb      on_chunk_complete;
  nullptr,             // llhttp_cb      on_reset;
};
} // namespace

HttpsUpstream::HttpsUpstream(ClientHandler *handler)
  : handler_(handler),
    current_header_length_(0),
    ioctrl_(handler->get_rlimit()),
    num_requests_(0) {
  llhttp_init(&htp_, HTTP_REQUEST, &htp_hooks);
  htp_.data = this;
}

HttpsUpstream::~HttpsUpstream() {}

void HttpsUpstream::reset_current_header_length() {
  current_header_length_ = 0;
}

void HttpsUpstream::on_start_request() {
  if (LOG_ENABLED(INFO)) {
    ULOG(INFO, this) << "HTTP request started";
  }
  reset_current_header_length();

  auto downstream =
    std::make_unique<Downstream>(this, handler_->get_mcpool(), 0);

  attach_downstream(std::move(downstream));

  auto &httpconf = get_config()->http;

  handler_->reset_upstream_read_timeout(httpconf.timeout.header);

  ++num_requests_;
}

namespace {
int htp_msg_begin(llhttp_t *htp) {
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  upstream->on_start_request();
  return 0;
}
} // namespace

namespace {
int htp_uricb(llhttp_t *htp, const char *data, size_t len) {
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  auto downstream = upstream->get_downstream();
  auto &req = downstream->request();

  auto &balloc = downstream->get_block_allocator();

  // We happen to have the same value for method token.
  req.method = htp->method;

  if (req.fs.buffer_size() + len >
      get_config()->http.request_header_field_buffer) {
    if (LOG_ENABLED(INFO)) {
      ULOG(INFO, upstream) << "Too large URI size="
                           << req.fs.buffer_size() + len;
    }
    assert(downstream->get_request_state() == DownstreamState::INITIAL);
    downstream->set_request_state(
      DownstreamState::HTTP1_REQUEST_HEADER_TOO_LARGE);
    llhttp_set_error_reason(htp, "too long request URI");
    return HPE_USER;
  }

  req.fs.add_extra_buffer_size(len);

  if (req.method == HTTP_CONNECT) {
    req.authority =
      concat_string_ref(balloc, req.authority, StringRef{data, len});
  } else {
    req.path = concat_string_ref(balloc, req.path, StringRef{data, len});
  }

  return 0;
}
} // namespace

namespace {
int htp_hdr_keycb(llhttp_t *htp, const char *data, size_t len) {
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  auto downstream = upstream->get_downstream();
  auto &req = downstream->request();
  auto &httpconf = get_config()->http;

  if (req.fs.buffer_size() + len > httpconf.request_header_field_buffer) {
    if (LOG_ENABLED(INFO)) {
      ULOG(INFO, upstream) << "Too large header block size="
                           << req.fs.buffer_size() + len;
    }
    if (downstream->get_request_state() == DownstreamState::INITIAL) {
      downstream->set_request_state(
        DownstreamState::HTTP1_REQUEST_HEADER_TOO_LARGE);
    }
    llhttp_set_error_reason(htp, "too large header");
    return HPE_USER;
  }
  if (downstream->get_request_state() == DownstreamState::INITIAL) {
    if (req.fs.header_key_prev()) {
      req.fs.append_last_header_key(data, len);
    } else {
      if (req.fs.num_fields() >= httpconf.max_request_header_fields) {
        if (LOG_ENABLED(INFO)) {
          ULOG(INFO, upstream)
            << "Too many header field num=" << req.fs.num_fields() + 1;
        }
        downstream->set_request_state(
          DownstreamState::HTTP1_REQUEST_HEADER_TOO_LARGE);
        llhttp_set_error_reason(htp, "too many headers");
        return HPE_USER;
      }
      req.fs.alloc_add_header_name(StringRef{data, len});
    }
  } else {
    // trailer part
    if (req.fs.trailer_key_prev()) {
      req.fs.append_last_trailer_key(data, len);
    } else {
      if (req.fs.num_fields() >= httpconf.max_request_header_fields) {
        if (LOG_ENABLED(INFO)) {
          ULOG(INFO, upstream)
            << "Too many header field num=" << req.fs.num_fields() + 1;
        }
        llhttp_set_error_reason(htp, "too many headers");
        return HPE_USER;
      }
      req.fs.alloc_add_trailer_name(StringRef{data, len});
    }
  }
  return 0;
}
} // namespace

namespace {
int htp_hdr_valcb(llhttp_t *htp, const char *data, size_t len) {
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  auto downstream = upstream->get_downstream();
  auto &req = downstream->request();

  if (req.fs.buffer_size() + len >
      get_config()->http.request_header_field_buffer) {
    if (LOG_ENABLED(INFO)) {
      ULOG(INFO, upstream) << "Too large header block size="
                           << req.fs.buffer_size() + len;
    }
    if (downstream->get_request_state() == DownstreamState::INITIAL) {
      downstream->set_request_state(
        DownstreamState::HTTP1_REQUEST_HEADER_TOO_LARGE);
    }
    llhttp_set_error_reason(htp, "too large header");
    return HPE_USER;
  }
  if (downstream->get_request_state() == DownstreamState::INITIAL) {
    req.fs.append_last_header_value(data, len);
  } else {
    req.fs.append_last_trailer_value(data, len);
  }
  return 0;
}
} // namespace

namespace {
void rewrite_request_host_path_from_uri(BlockAllocator &balloc, Request &req,
                                        const StringRef &uri, urlparse_url &u) {
  assert(u.field_set & (1 << URLPARSE_HOST));

  // As per https://tools.ietf.org/html/rfc7230#section-5.4, we
  // rewrite host header field with authority component.
  auto authority = util::get_uri_field(uri.data(), u, URLPARSE_HOST);
  // TODO properly check IPv6 numeric address
  auto ipv6 = std::find(std::begin(authority), std::end(authority), ':') !=
              std::end(authority);
  auto authoritylen = authority.size();
  if (ipv6) {
    authoritylen += 2;
  }
  if (u.field_set & (1 << URLPARSE_PORT)) {
    authoritylen += 1 + str_size("65535");
  }
  if (authoritylen > authority.size()) {
    auto iovec = make_byte_ref(balloc, authoritylen + 1);
    auto p = std::begin(iovec);
    if (ipv6) {
      *p++ = '[';
    }
    p = std::copy(std::begin(authority), std::end(authority), p);
    if (ipv6) {
      *p++ = ']';
    }

    if (u.field_set & (1 << URLPARSE_PORT)) {
      *p++ = ':';
      p = util::utos(p, u.port);
    }
    *p = '\0';

    req.authority = StringRef{std::span{std::begin(iovec), p}};
  } else {
    req.authority = authority;
  }

  req.scheme = util::get_uri_field(uri.data(), u, URLPARSE_SCHEMA);

  StringRef path;
  if (u.field_set & (1 << URLPARSE_PATH)) {
    path = util::get_uri_field(uri.data(), u, URLPARSE_PATH);
  } else if (req.method == HTTP_OPTIONS) {
    // Server-wide OPTIONS takes following form in proxy request:
    //
    // OPTIONS http://example.org HTTP/1.1
    //
    // Notice that no slash after authority. See
    // http://tools.ietf.org/html/rfc7230#section-5.3.4
    req.path = ""_sr;
    // we ignore query component here
    return;
  } else {
    path = "/"_sr;
  }

  if (u.field_set & (1 << URLPARSE_QUERY)) {
    auto &fdata = u.field_data[URLPARSE_QUERY];

    if (u.field_set & (1 << URLPARSE_PATH)) {
      auto q = util::get_uri_field(uri.data(), u, URLPARSE_QUERY);
      path = StringRef{std::begin(path), std::end(q)};
    } else {
      path = concat_string_ref(balloc, path, "?"_sr,
                               StringRef{&uri[fdata.off], fdata.len});
    }
  }

  req.path = http2::rewrite_clean_path(balloc, path);
}
} // namespace

namespace {
int htp_hdrs_completecb(llhttp_t *htp) {
  int rv;
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  if (LOG_ENABLED(INFO)) {
    ULOG(INFO, upstream) << "HTTP request headers completed";
  }

  auto handler = upstream->get_client_handler();

  auto downstream = upstream->get_downstream();
  auto &req = downstream->request();
  auto &balloc = downstream->get_block_allocator();

  for (auto &kv : req.fs.headers()) {
    kv.value = util::rstrip(balloc, kv.value);

    if (kv.token == http2::HD_TRANSFER_ENCODING &&
        !http2::check_transfer_encoding(kv.value)) {
      return -1;
    }
  }

  auto lgconf = log_config();
  lgconf->update_tstamp(std::chrono::system_clock::now());
  req.tstamp = lgconf->tstamp;

  req.http_major = htp->http_major;
  req.http_minor = htp->http_minor;

  req.connection_close = !llhttp_should_keep_alive(htp);

  handler->stop_read_timer();

  auto method = req.method;

  if (LOG_ENABLED(INFO)) {
    std::stringstream ss;
    ss << http2::to_method_string(method) << " "
       << (method == HTTP_CONNECT ? req.authority : req.path) << " "
       << "HTTP/" << req.http_major << "." << req.http_minor << "\n";

    for (const auto &kv : req.fs.headers()) {
      if (kv.name == "authorization"_sr) {
        ss << TTY_HTTP_HD << kv.name << TTY_RST << ": <redacted>\n";
        continue;
      }
      ss << TTY_HTTP_HD << kv.name << TTY_RST << ": " << kv.value << "\n";
    }

    ULOG(INFO, upstream) << "HTTP request headers\n" << ss.str();
  }

  // set content-length if method is not CONNECT, and no
  // transfer-encoding is given.  If transfer-encoding is given, leave
  // req.fs.content_length to -1.
  if (method != HTTP_CONNECT && !req.fs.header(http2::HD_TRANSFER_ENCODING)) {
    // llhttp sets 0 to htp->content_length if there is no
    // content-length header field.  If we don't have both
    // transfer-encoding and content-length header field, we assume
    // that there is no request body.
    req.fs.content_length = htp->content_length;
  }

  auto host = req.fs.header(http2::HD_HOST);

  if (req.http_major > 1 || req.http_minor > 1) {
    req.http_major = 1;
    req.http_minor = 1;
    return -1;
  }

  if (req.http_major == 1 && req.http_minor == 1 && !host) {
    return -1;
  }

  if (host) {
    const auto &value = host->value;
    // Not allow at least '"' or '\' in host.  They are illegal in
    // authority component, also they cause headaches when we put them
    // in quoted-string.
    if (std::find_if(std::begin(value), std::end(value), [](char c) {
          return c == '"' || c == '\\';
        }) != std::end(value)) {
      return -1;
    }
  }

  downstream->inspect_http1_request();

  if (htp->flags & F_CHUNKED) {
    downstream->set_chunked_request(true);
  }

  auto transfer_encoding = req.fs.header(http2::HD_TRANSFER_ENCODING);
  if (transfer_encoding &&
      http2::legacy_http1(req.http_major, req.http_minor)) {
    return -1;
  }

  auto faddr = handler->get_upstream_addr();
  auto config = get_config();

  if (method != HTTP_CONNECT) {
    urlparse_url u;
    rv = urlparse_parse_url(req.path.data(), req.path.size(), 0, &u);
    if (rv != 0) {
      // Expect to respond with 400 bad request
      return -1;
    }
    // checking URLPARSE_HOST could be redundant, but just in case ...
    if (!(u.field_set & (1 << URLPARSE_SCHEMA)) ||
        !(u.field_set & (1 << URLPARSE_HOST))) {
      req.no_authority = true;

      if (method == HTTP_OPTIONS && req.path == "*"_sr) {
        req.path = StringRef{};
      } else {
        req.path = http2::rewrite_clean_path(balloc, req.path);
      }

      if (host) {
        req.authority = host->value;
      }

      if (handler->get_ssl()) {
        req.scheme = "https"_sr;
      } else {
        req.scheme = "http"_sr;
      }
    } else {
      rewrite_request_host_path_from_uri(balloc, req, req.path, u);
    }
  }

  downstream->set_request_state(DownstreamState::HEADER_COMPLETE);

  auto &resp = downstream->response();

  if (config->http.require_http_scheme &&
      !http::check_http_scheme(req.scheme, handler->get_ssl() != nullptr)) {
    resp.http_status = 400;
    return -1;
  }

#ifdef HAVE_MRUBY
  auto worker = handler->get_worker();
  auto mruby_ctx = worker->get_mruby_context();

  if (mruby_ctx->run_on_request_proc(downstream) != 0) {
    resp.http_status = 500;
    return -1;
  }
#endif // HAVE_MRUBY

  // mruby hook may change method value

  if (req.no_authority && config->http2_proxy &&
      faddr->alt_mode == UpstreamAltMode::NONE) {
    // Request URI should be absolute-form for client proxy mode
    return -1;
  }

  if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
    return 0;
  }

#ifdef HAVE_MRUBY
  DownstreamConnection *dconn_ptr;
#endif // HAVE_MRUBY

  for (;;) {
    auto dconn = handler->get_downstream_connection(rv, downstream);

    if (!dconn) {
      if (rv == SHRPX_ERR_TLS_REQUIRED) {
        upstream->redirect_to_https(downstream);
      }
      downstream->set_request_state(DownstreamState::CONNECT_FAIL);
      return -1;
    }

#ifdef HAVE_MRUBY
    dconn_ptr = dconn.get();
#endif // HAVE_MRUBY
    if (downstream->attach_downstream_connection(std::move(dconn)) == 0) {
      break;
    }
  }

#ifdef HAVE_MRUBY
  const auto &group = dconn_ptr->get_downstream_addr_group();
  if (group) {
    const auto &dmruby_ctx = group->shared_addr->mruby_ctx;

    if (dmruby_ctx->run_on_request_proc(downstream) != 0) {
      resp.http_status = 500;
      return -1;
    }

    if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
      return 0;
    }
  }
#endif // HAVE_MRUBY

  rv = downstream->push_request_headers();

  if (rv != 0) {
    return -1;
  }

  if (faddr->alt_mode != UpstreamAltMode::NONE) {
    // Normally, we forward expect: 100-continue to backend server,
    // and let them decide whether responds with 100 Continue or not.
    // For alternative mode, we have no backend, so just send 100
    // Continue here to make the client happy.
    if (downstream->get_expect_100_continue()) {
      auto output = downstream->get_response_buf();
      constexpr auto res = "HTTP/1.1 100 Continue\r\n\r\n"_sr;
      output->append(res);
      handler->signal_write();
    }
  }

  return 0;
}
} // namespace

namespace {
int htp_bodycb(llhttp_t *htp, const char *data, size_t len) {
  int rv;
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  auto downstream = upstream->get_downstream();
  rv = downstream->push_upload_data_chunk(
    reinterpret_cast<const uint8_t *>(data), len);
  if (rv != 0) {
    // Ignore error if response has been completed.  We will end up in
    // htp_msg_completecb, and request will end gracefully.
    if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
      return 0;
    }

    llhttp_set_error_reason(htp, "could not process request body");
    return HPE_USER;
  }
  return 0;
}
} // namespace

namespace {
int htp_msg_completecb(llhttp_t *htp) {
  int rv;
  auto upstream = static_cast<HttpsUpstream *>(htp->data);
  if (LOG_ENABLED(INFO)) {
    ULOG(INFO, upstream) << "HTTP request completed";
  }
  auto handler = upstream->get_client_handler();
  auto downstream = upstream->get_downstream();
  auto &req = downstream->request();
  auto &balloc = downstream->get_block_allocator();

  for (auto &kv : req.fs.trailers()) {
    kv.value = util::rstrip(balloc, kv.value);
  }

  downstream->set_request_state(DownstreamState::MSG_COMPLETE);
  rv = downstream->end_upload_data();
  if (rv != 0) {
    if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
      // Here both response and request were completed.  One of the
      // reason why end_upload_data() failed is when we sent response
      // in request phase hook.  We only delete and proceed to the
      // next request handling (if we don't close the connection).  We
      // first pause parser here just as we normally do, and call
      // signal_write() to run on_write().
      return HPE_PAUSED;
    }
    return -1;
  }

  if (handler->get_http2_upgrade_allowed() &&
      downstream->get_http2_upgrade_request() &&
      handler->perform_http2_upgrade(upstream) != 0) {
    if (LOG_ENABLED(INFO)) {
      ULOG(INFO, upstream) << "HTTP Upgrade to HTTP/2 failed";
    }
  }

  // Stop further processing to complete this request
  return HPE_PAUSED;
}
} // namespace

// on_read() does not consume all available data in input buffer if
// one http request is fully received.
int HttpsUpstream::on_read() {
  auto rb = handler_->get_rb();
  auto rlimit = handler_->get_rlimit();
  auto downstream = get_downstream();

  if (rb->rleft() == 0 || handler_->get_should_close_after_write()) {
    return 0;
  }

  // downstream can be nullptr here, because it is initialized in the
  // callback chain called by llhttp_execute()
  if (downstream && downstream->get_upgraded()) {
    auto rv = downstream->push_upload_data_chunk(rb->pos(), rb->rleft());

    if (rv != 0) {
      return -1;
    }

    rb->reset();
    rlimit->startw();

    if (downstream->request_buf_full()) {
      if (LOG_ENABLED(INFO)) {
        ULOG(INFO, this) << "Downstream request buf is full";
      }
      pause_read(SHRPX_NO_BUFFER);

      return 0;
    }

    return 0;
  }

  if (downstream) {
    // To avoid reading next pipelined request
    switch (downstream->get_request_state()) {
    case DownstreamState::INITIAL:
    case DownstreamState::HEADER_COMPLETE:
      break;
    default:
      return 0;
    }
  }

  // llhttp_execute() does nothing once it entered error state.
  auto htperr = llhttp_execute(&htp_, reinterpret_cast<const char *>(rb->pos()),
                               rb->rleft());

  if (htperr == HPE_PAUSED_UPGRADE &&
      rb->pos() ==
        reinterpret_cast<const uint8_t *>(llhttp_get_error_pos(&htp_))) {
    llhttp_resume_after_upgrade(&htp_);

    htperr = llhttp_execute(&htp_, reinterpret_cast<const char *>(rb->pos()),
                            rb->rleft());
  }

  auto nread =
    htperr == HPE_OK
      ? rb->rleft()
      : reinterpret_cast<const uint8_t *>(llhttp_get_error_pos(&htp_)) -
          rb->pos();
  rb->drain(nread);
  rlimit->startw();

  // Well, actually header length + some body bytes
  current_header_length_ += nread;

  // Get downstream again because it may be initialized in http parser
  // execution
  downstream = get_downstream();

  if (htperr == HPE_PAUSED) {
    // We may pause parser in htp_msg_completecb when both side are
    // completed.  Signal write, so that we can run on_write().
    if (downstream &&
        downstream->get_request_state() == DownstreamState::MSG_COMPLETE &&
        downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
      handler_->signal_write();
    }
    return 0;
  }

  if (htperr != HPE_OK) {
    if (LOG_ENABLED(INFO)) {
      ULOG(INFO, this) << "HTTP parse failure: "
                       << "(" << llhttp_errno_name(htperr) << ") "
                       << llhttp_get_error_reason(&htp_);
    }

    if (downstream &&
        downstream->get_response_state() != DownstreamState::INITIAL) {
      handler_->set_should_close_after_write(true);
      handler_->signal_write();
      return 0;
    }

    unsigned int status_code;

    if (htperr == HPE_INVALID_METHOD) {
      status_code = 501;
    } else if (downstream) {
      status_code = downstream->response().http_status;
      if (status_code == 0) {
        if (downstream->get_request_state() == DownstreamState::CONNECT_FAIL) {
          status_code = 502;
        } else if (downstream->get_request_state() ==
                   DownstreamState::HTTP1_REQUEST_HEADER_TOO_LARGE) {
          status_code = 431;
        } else {
          status_code = 400;
        }
      }
    } else {
      status_code = 400;
    }

    error_reply(status_code);

    handler_->signal_write();

    return 0;
  }

  // downstream can be NULL here.
  if (downstream && downstream->request_buf_full()) {
    if (LOG_ENABLED(INFO)) {
      ULOG(INFO, this) << "Downstream request buffer is full";
    }

    pause_read(SHRPX_NO_BUFFER);

    return 0;
  }

  return 0;
}

int HttpsUpstream::on_write() {
  auto downstream = get_downstream();
  if (!downstream) {
    return 0;
  }

  auto output = downstream->get_response_buf();
  const auto &resp = downstream->response();

  if (output->rleft() > 0) {
    return 0;
  }

  // We need to postpone detachment until all data are sent so that
  // we can notify nghttp2 library all data consumed.
  if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
    if (downstream->can_detach_downstream_connection()) {
      // Keep-alive
      downstream->detach_downstream_connection();
    } else {
      // Connection close
      downstream->pop_downstream_connection();
      // dconn was deleted
    }
    // We need this if response ends before request.
    if (downstream->get_request_state() == DownstreamState::MSG_COMPLETE) {
      delete_downstream();

      if (handler_->get_should_close_after_write()) {
        return 0;
      }

      auto &upstreamconf = get_config()->conn.upstream;

      handler_->reset_upstream_read_timeout(upstreamconf.timeout.idle);

      return resume_read(SHRPX_NO_BUFFER, nullptr, 0);
    } else {
      // If the request is not complete, close the connection.
      delete_downstream();

      handler_->set_should_close_after_write(true);

      return 0;
    }
  }

  return downstream->resume_read(SHRPX_NO_BUFFER, resp.unconsumed_body_length);
}

int HttpsUpstream::on_event() { return 0; }

ClientHandler *HttpsUpstream::get_client_handler() const { return handler_; }

void HttpsUpstream::pause_read(IOCtrlReason reason) {
  ioctrl_.pause_read(reason);
}

int HttpsUpstream::resume_read(IOCtrlReason reason, Downstream *downstream,
                               size_t consumed) {
  // downstream could be nullptr
  if (downstream && downstream->request_buf_full()) {
    return 0;
  }
  if (ioctrl_.resume_read(reason)) {
    // Process remaining data in input buffer here because these bytes
    // are not notified by readcb until new data arrive.
    llhttp_resume(&htp_);

    auto conn = handler_->get_connection();
    ev_feed_event(conn->loop, &conn->rev, EV_READ);
    return 0;
  }

  return 0;
}

int HttpsUpstream::downstream_read(DownstreamConnection *dconn) {
  auto downstream = dconn->get_downstream();
  int rv;

  rv = downstream->on_read();

  if (rv == SHRPX_ERR_EOF) {
    if (downstream->get_request_header_sent()) {
      return downstream_eof(dconn);
    }
    return SHRPX_ERR_RETRY;
  }

  if (rv == SHRPX_ERR_DCONN_CANCELED) {
    downstream->pop_downstream_connection();
    goto end;
  }

  if (rv < 0) {
    return downstream_error(dconn, Downstream::EVENT_ERROR);
  }

  if (downstream->get_response_state() == DownstreamState::MSG_RESET) {
    return -1;
  }

  if (downstream->get_response_state() == DownstreamState::MSG_BAD_HEADER) {
    error_reply(502);
    downstream->pop_downstream_connection();
    goto end;
  }

  if (downstream->can_detach_downstream_connection()) {
    // Keep-alive
    downstream->detach_downstream_connection();
  }

end:
  handler_->signal_write();

  return 0;
}

int HttpsUpstream::downstream_write(DownstreamConnection *dconn) {
  int rv;
  rv = dconn->on_write();
  if (rv == SHRPX_ERR_NETWORK) {
    return downstream_error(dconn, Downstream::EVENT_ERROR);
  }

  if (rv != 0) {
    return rv;
  }

  return 0;
}

int HttpsUpstream::downstream_eof(DownstreamConnection *dconn) {
  auto downstream = dconn->get_downstream();

  if (LOG_ENABLED(INFO)) {
    DCLOG(INFO, dconn) << "EOF";
  }

  if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
    goto end;
  }

  if (downstream->get_response_state() == DownstreamState::HEADER_COMPLETE) {
    // Server may indicate the end of the request by EOF
    if (LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "The end of the response body was indicated by "
                         << "EOF";
    }
    on_downstream_body_complete(downstream);
    downstream->set_response_state(DownstreamState::MSG_COMPLETE);
    downstream->pop_downstream_connection();
    goto end;
  }

  if (downstream->get_response_state() == DownstreamState::INITIAL) {
    // we did not send any response headers, so we can reply error
    // message.
    if (LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "Return error reply";
    }
    error_reply(502);
    downstream->pop_downstream_connection();
    goto end;
  }

  // Otherwise, we don't know how to recover from this situation. Just
  // drop connection.
  return -1;
end:
  handler_->signal_write();

  return 0;
}

int HttpsUpstream::downstream_error(DownstreamConnection *dconn, int events) {
  auto downstream = dconn->get_downstream();
  if (LOG_ENABLED(INFO)) {
    if (events & Downstream::EVENT_ERROR) {
      DCLOG(INFO, dconn) << "Network error/general error";
    } else {
      DCLOG(INFO, dconn) << "Timeout";
    }
  }
  if (downstream->get_response_state() != DownstreamState::INITIAL) {
    return -1;
  }

  unsigned int status;
  if (events & Downstream::EVENT_TIMEOUT) {
    if (downstream->get_request_header_sent()) {
      status = 504;
    } else {
      status = 408;
    }
  } else {
    status = 502;
  }
  error_reply(status);

  downstream->pop_downstream_connection();

  handler_->signal_write();
  return 0;
}

int HttpsUpstream::send_reply(Downstream *downstream, const uint8_t *body,
                              size_t bodylen) {
  const auto &req = downstream->request();
  auto &resp = downstream->response();
  auto &balloc = downstream->get_block_allocator();
  auto config = get_config();
  auto &httpconf = config->http;

  auto connection_close = false;

  auto worker = handler_->get_worker();

  if (httpconf.max_requests <= num_requests_ ||
      worker->get_graceful_shutdown()) {
    resp.fs.add_header_token("connection"_sr, "close"_sr, false,
                             http2::HD_CONNECTION);
    connection_close = true;
  } else if (req.http_major <= 0 ||
             (req.http_major == 1 && req.http_minor == 0)) {
    connection_close = true;
  } else {
    auto c = resp.fs.header(http2::HD_CONNECTION);
    if (c && util::strieq("close"_sr, c->value)) {
      connection_close = true;
    }
  }

  if (connection_close) {
    resp.connection_close = true;
    handler_->set_should_close_after_write(true);
  }

  auto output = downstream->get_response_buf();

  output->append("HTTP/1.1 ");
  output->append(http2::stringify_status(balloc, resp.http_status));
  output->append(' ');
  output->append(http2::get_reason_phrase(resp.http_status));
  output->append("\r\n");

  for (auto &kv : resp.fs.headers()) {
    if (kv.name.empty() || kv.name[0] == ':') {
      continue;
    }
    http2::capitalize(output, kv.name);
    output->append(": ");
    output->append(kv.value);
    output->append("\r\n");
  }

  if (!resp.fs.header(http2::HD_SERVER)) {
    output->append("Server: ");
    output->append(config->http.server_name);
    output->append("\r\n");
  }

  for (auto &p : httpconf.add_response_headers) {
    output->append(p.name);
    output->append(": ");
    output->append(p.value);
    output->append("\r\n");
  }

  output->append("\r\n");

  if (req.method != HTTP_HEAD) {
    output->append(body, bodylen);

    downstream->response_sent_body_length += bodylen;
  }

  downstream->set_response_state(DownstreamState::MSG_COMPLETE);

  return 0;
}

void HttpsUpstream::error_reply(unsigned int status_code) {
  auto downstream = get_downstream();

  if (!downstream) {
    attach_downstream(
      std::make_unique<Downstream>(this, handler_->get_mcpool(), 1));
    downstream = get_downstream();
  }

  auto &resp = downstream->response();
  auto &balloc = downstream->get_block_allocator();

  auto html = http::create_error_html(balloc, status_code);

  resp.http_status = status_code;
  // we are going to close connection for both frontend and backend in
  // error condition.  This is safest option.
  resp.connection_close = true;
  handler_->set_should_close_after_write(true);

  auto output = downstream->get_response_buf();

  output->append("HTTP/1.1 ");
  output->append(http2::stringify_status(balloc, status_code));
  output->append(' ');
  output->append(http2::get_reason_phrase(status_code));
  output->append("\r\nServer: ");
  output->append(get_config()->http.server_name);
  output->append("\r\nContent-Length: ");
  std::array<char, NGHTTP2_MAX_UINT64_DIGITS> intbuf;
  output->append(
    StringRef{std::begin(intbuf), util::utos(std::begin(intbuf), html.size())});
  output->append("\r\nDate: ");
  auto lgconf = log_config();
  lgconf->update_tstamp(std::chrono::system_clock::now());
  output->append(lgconf->tstamp->time_http);
  output->append("\r\nContent-Type: text/html; "
                 "charset=UTF-8\r\nConnection: close\r\n\r\n");

  const auto &req = downstream->request();

  if (req.method != HTTP_HEAD) {
    output->append(html);

    downstream->response_sent_body_length += html.size();
  }

  downstream->set_response_state(DownstreamState::MSG_COMPLETE);
}

void HttpsUpstream::attach_downstream(std::unique_ptr<Downstream> downstream) {
  assert(!downstream_);
  downstream_ = std::move(downstream);
}

void HttpsUpstream::delete_downstream() {
  if (downstream_ && downstream_->accesslog_ready()) {
    handler_->write_accesslog(downstream_.get());
  }

  downstream_.reset();
}

Downstream *HttpsUpstream::get_downstream() const { return downstream_.get(); }

std::unique_ptr<Downstream> HttpsUpstream::pop_downstream() {
  return std::unique_ptr<Downstream>(downstream_.release());
}

int HttpsUpstream::on_downstream_header_complete(Downstream *downstream) {
  if (LOG_ENABLED(INFO)) {
    if (downstream->get_non_final_response()) {
      DLOG(INFO, downstream) << "HTTP non-final response header";
    } else {
      DLOG(INFO, downstream) << "HTTP response header completed";
    }
  }

  const auto &req = downstream->request();
  auto &resp = downstream->response();
  auto &balloc = downstream->get_block_allocator();
  auto dconn = downstream->get_downstream_connection();
  // dconn might be nullptr if this is non-final response from mruby.

  if (downstream->get_non_final_response() &&
      !downstream->supports_non_final_response()) {
    resp.fs.clear_headers();
    return 0;
  }

#ifdef HAVE_MRUBY
  if (!downstream->get_non_final_response()) {
    assert(dconn);
    const auto &group = dconn->get_downstream_addr_group();
    if (group) {
      const auto &dmruby_ctx = group->shared_addr->mruby_ctx;

      if (dmruby_ctx->run_on_response_proc(downstream) != 0) {
        error_reply(500);
        return -1;
      }

      if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
        return -1;
      }
    }

    auto worker = handler_->get_worker();
    auto mruby_ctx = worker->get_mruby_context();

    if (mruby_ctx->run_on_response_proc(downstream) != 0) {
      error_reply(500);
      return -1;
    }

    if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
      return -1;
    }
  }
#endif // HAVE_MRUBY

  auto connect_method = req.method == HTTP_CONNECT;

  auto buf = downstream->get_response_buf();
  buf->append("HTTP/");
  buf->append('0' + req.http_major);
  buf->append('.');
  buf->append('0' + req.http_minor);
  buf->append(' ');
  if (req.connect_proto != ConnectProto::NONE && downstream->get_upgraded()) {
    buf->append(http2::stringify_status(balloc, 101));
    buf->append(' ');
    buf->append(http2::get_reason_phrase(101));
  } else {
    buf->append(http2::stringify_status(balloc, resp.http_status));
    buf->append(' ');
    buf->append(http2::get_reason_phrase(resp.http_status));
  }
  buf->append("\r\n");

  auto config = get_config();
  auto &httpconf = config->http;

  if (!config->http2_proxy && !httpconf.no_location_rewrite) {
    downstream->rewrite_location_response_header(
      get_client_handler()->get_upstream_scheme());
  }

  if (downstream->get_non_final_response()) {
    http2::build_http1_headers_from_headers(buf, resp.fs.headers(),
                                            http2::HDOP_STRIP_ALL);

    buf->append("\r\n");

    if (LOG_ENABLED(INFO)) {
      log_response_headers(buf);
    }

    resp.fs.clear_headers();

    return 0;
  }

  auto build_flags = (http2::HDOP_STRIP_ALL & ~http2::HDOP_STRIP_VIA) |
                     (!http2::legacy_http1(req.http_major, req.http_minor)
                        ? 0
                        : http2::HDOP_STRIP_TRANSFER_ENCODING);

  http2::build_http1_headers_from_headers(buf, resp.fs.headers(), build_flags);

  auto worker = handler_->get_worker();

  // after graceful shutdown commenced, add connection: close header
  // field.
  if (httpconf.max_requests <= num_requests_ ||
      worker->get_graceful_shutdown()) {
    resp.connection_close = true;
  }

  // We check downstream->get_response_connection_close() in case when
  // the Content-Length is not available.
  if (!req.connection_close && !resp.connection_close) {
    if (req.http_major <= 0 || req.http_minor <= 0) {
      // We add this header for HTTP/1.0 or HTTP/0.9 clients
      buf->append("Connection: Keep-Alive\r\n");
    }
  } else if (!downstream->get_upgraded()) {
    buf->append("Connection: close\r\n");
  }

  if (!connect_method && downstream->get_upgraded()) {
    if (req.connect_proto == ConnectProto::WEBSOCKET &&
        resp.http_status / 100 == 2) {
      buf->append("Upgrade: websocket\r\nConnection: Upgrade\r\n");
      auto key = req.fs.header(http2::HD_SEC_WEBSOCKET_KEY);
      if (!key || key->value.size() != base64::encode_length(16)) {
        return -1;
      }
      std::array<uint8_t, base64::encode_length(20)> out;
      auto accept = http2::make_websocket_accept_token(out.data(), key->value);
      if (accept.empty()) {
        return -1;
      }
      buf->append("Sec-WebSocket-Accept: ");
      buf->append(accept);
      buf->append("\r\n");
    } else {
      auto connection = resp.fs.header(http2::HD_CONNECTION);
      if (connection) {
        buf->append("Connection: ");
        buf->append((*connection).value);
        buf->append("\r\n");
      }

      auto upgrade = resp.fs.header(http2::HD_UPGRADE);
      if (upgrade) {
        buf->append("Upgrade: ");
        buf->append((*upgrade).value);
        buf->append("\r\n");
      }
    }
  }

  if (!resp.fs.header(http2::HD_ALT_SVC)) {
    // We won't change or alter alt-svc from backend for now
    if (!httpconf.altsvcs.empty()) {
      buf->append("Alt-Svc: ");
      buf->append(httpconf.altsvc_header_value);
      buf->append("\r\n");
    }
  }

  if (!config->http2_proxy && !httpconf.no_server_rewrite) {
    buf->append("Server: ");
    buf->append(httpconf.server_name);
    buf->append("\r\n");
  } else {
    auto server = resp.fs.header(http2::HD_SERVER);
    if (server) {
      buf->append("Server: ");
      buf->append((*server).value);
      buf->append("\r\n");
    }
  }

  if (req.method != HTTP_CONNECT || !downstream->get_upgraded()) {
    auto affinity_cookie = downstream->get_affinity_cookie_to_send();
    if (affinity_cookie) {
      auto &group = dconn->get_downstream_addr_group();
      auto &shared_addr = group->shared_addr;
      auto &cookieconf = shared_addr->affinity.cookie;
      auto secure =
        http::require_cookie_secure_attribute(cookieconf.secure, req.scheme);
      auto cookie_str = http::create_affinity_cookie(
        balloc, cookieconf.name, affinity_cookie, cookieconf.path, secure);
      buf->append("Set-Cookie: ");
      buf->append(cookie_str);
      buf->append("\r\n");
    }
  }

  auto via = resp.fs.header(http2::HD_VIA);
  if (httpconf.no_via) {
    if (via) {
      buf->append("Via: ");
      buf->append((*via).value);
      buf->append("\r\n");
    }
  } else {
    buf->append("Via: ");
    if (via) {
      buf->append((*via).value);
      buf->append(", ");
    }
    std::array<char, 16> viabuf;
    auto end = http::create_via_header_value(viabuf.data(), resp.http_major,
                                             resp.http_minor);
    buf->append(viabuf.data(), end - std::begin(viabuf));
    buf->append("\r\n");
  }

  for (auto &p : httpconf.add_response_headers) {
    buf->append(p.name);
    buf->append(": ");
    buf->append(p.value);
    buf->append("\r\n");
  }

  buf->append("\r\n");

  if (LOG_ENABLED(INFO)) {
    log_response_headers(buf);
  }

  return 0;
}

int HttpsUpstream::on_downstream_body(Downstream *downstream,
                                      const uint8_t *data, size_t len,
                                      bool flush) {
  if (len == 0) {
    return 0;
  }
  auto output = downstream->get_response_buf();
  if (downstream->get_chunked_response()) {
    output->append(util::utox(len));
    output->append("\r\n");
  }
  output->append(data, len);

  downstream->response_sent_body_length += len;

  if (downstream->get_chunked_response()) {
    output->append("\r\n");
  }
  return 0;
}

int HttpsUpstream::on_downstream_body_complete(Downstream *downstream) {
  const auto &req = downstream->request();
  auto &resp = downstream->response();

  if (downstream->get_chunked_response()) {
    auto output = downstream->get_response_buf();
    const auto &trailers = resp.fs.trailers();
    if (trailers.empty()) {
      output->append("0\r\n\r\n");
    } else {
      output->append("0\r\n");
      http2::build_http1_headers_from_headers(output, trailers,
                                              http2::HDOP_STRIP_ALL);
      output->append("\r\n");
    }
  }
  if (LOG_ENABLED(INFO)) {
    DLOG(INFO, downstream) << "HTTP response completed";
  }

  if (!downstream->validate_response_recv_body_length()) {
    resp.connection_close = true;
  }

  if (req.connection_close || resp.connection_close ||
      // To avoid to stall upload body
      downstream->get_request_state() != DownstreamState::MSG_COMPLETE) {
    auto handler = get_client_handler();
    handler->set_should_close_after_write(true);
  }
  return 0;
}

int HttpsUpstream::on_downstream_abort_request(Downstream *downstream,
                                               unsigned int status_code) {
  error_reply(status_code);
  handler_->signal_write();
  return 0;
}

int HttpsUpstream::on_downstream_abort_request_with_https_redirect(
  Downstream *downstream) {
  redirect_to_https(downstream);
  handler_->signal_write();
  return 0;
}

int HttpsUpstream::redirect_to_https(Downstream *downstream) {
  auto &req = downstream->request();
  if (req.method == HTTP_CONNECT || req.scheme != "http"_sr ||
      req.authority.empty()) {
    error_reply(400);
    return 0;
  }

  auto authority = util::extract_host(req.authority);
  if (authority.empty()) {
    error_reply(400);
    return 0;
  }

  auto &balloc = downstream->get_block_allocator();
  auto config = get_config();
  auto &httpconf = config->http;

  StringRef loc;
  if (httpconf.redirect_https_port == "443"_sr) {
    loc = concat_string_ref(balloc, "https://"_sr, authority, req.path);
  } else {
    loc = concat_string_ref(balloc, "https://"_sr, authority, ":"_sr,
                            httpconf.redirect_https_port, req.path);
  }

  auto &resp = downstream->response();
  resp.http_status = 308;
  resp.fs.add_header_token("location"_sr, loc, false, http2::HD_LOCATION);
  resp.fs.add_header_token("connection"_sr, "close"_sr, false,
                           http2::HD_CONNECTION);

  return send_reply(downstream, nullptr, 0);
}

void HttpsUpstream::log_response_headers(DefaultMemchunks *buf) const {
  std::string nhdrs;
  for (auto chunk = buf->head; chunk; chunk = chunk->next) {
    nhdrs.append(chunk->pos, chunk->last);
  }
  if (log_config()->errorlog_tty) {
    nhdrs = http::colorizeHeaders(nhdrs.c_str());
  }
  ULOG(INFO, this) << "HTTP response headers\n" << nhdrs;
}

void HttpsUpstream::on_handler_delete() {
  if (downstream_ && downstream_->accesslog_ready()) {
    handler_->write_accesslog(downstream_.get());
  }
}

int HttpsUpstream::on_downstream_reset(Downstream *downstream, bool no_retry) {
  int rv;
  std::unique_ptr<DownstreamConnection> dconn;

  assert(downstream == downstream_.get());

  downstream_->pop_downstream_connection();

  if (!downstream_->request_submission_ready()) {
    switch (downstream_->get_response_state()) {
    case DownstreamState::MSG_COMPLETE:
      // We have got all response body already.  Send it off.
      return 0;
    case DownstreamState::INITIAL:
      if (on_downstream_abort_request(downstream_.get(), 502) != 0) {
        return -1;
      }
      return 0;
    default:
      break;
    }
    // Return error so that caller can delete handler
    return -1;
  }

  downstream_->add_retry();

  rv = 0;

  if (no_retry || downstream_->no_more_retry()) {
    goto fail;
  }

  for (;;) {
    auto dconn = handler_->get_downstream_connection(rv, downstream_.get());
    if (!dconn) {
      goto fail;
    }

    rv = downstream_->attach_downstream_connection(std::move(dconn));
    if (rv == 0) {
      break;
    }
  }

  rv = downstream_->push_request_headers();
  if (rv != 0) {
    goto fail;
  }

  return 0;

fail:
  if (rv == SHRPX_ERR_TLS_REQUIRED) {
    rv = on_downstream_abort_request_with_https_redirect(downstream);
  } else {
    rv = on_downstream_abort_request(downstream_.get(), 502);
  }
  if (rv != 0) {
    return -1;
  }
  downstream_->pop_downstream_connection();

  return 0;
}

int HttpsUpstream::initiate_push(Downstream *downstream, const StringRef &uri) {
  return 0;
}

int HttpsUpstream::response_riovec(struct iovec *iov, int iovcnt) const {
  if (!downstream_) {
    return 0;
  }

  auto buf = downstream_->get_response_buf();

  return buf->riovec(iov, iovcnt);
}

void HttpsUpstream::response_drain(size_t n) {
  if (!downstream_) {
    return;
  }

  auto buf = downstream_->get_response_buf();

  buf->drain(n);
}

bool HttpsUpstream::response_empty() const {
  if (!downstream_) {
    return true;
  }

  auto buf = downstream_->get_response_buf();

  return buf->rleft() == 0;
}

Downstream *
HttpsUpstream::on_downstream_push_promise(Downstream *downstream,
                                          int32_t promised_stream_id) {
  return nullptr;
}

int HttpsUpstream::on_downstream_push_promise_complete(
  Downstream *downstream, Downstream *promised_downstream) {
  return -1;
}

bool HttpsUpstream::push_enabled() const { return false; }

void HttpsUpstream::cancel_premature_downstream(
  Downstream *promised_downstream) {}

} // namespace shrpx
