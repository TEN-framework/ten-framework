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
#include "shrpx_downstream.h"

#include <cassert>

#include "urlparse.h"

#include "shrpx_upstream.h"
#include "shrpx_client_handler.h"
#include "shrpx_config.h"
#include "shrpx_error.h"
#include "shrpx_downstream_connection.h"
#include "shrpx_downstream_queue.h"
#include "shrpx_worker.h"
#include "shrpx_http2_session.h"
#include "shrpx_log.h"
#ifdef HAVE_MRUBY
#  include "shrpx_mruby.h"
#endif // HAVE_MRUBY
#include "util.h"
#include "http2.h"

namespace shrpx {

namespace {
void header_timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto downstream = static_cast<Downstream *>(w->data);
  auto upstream = downstream->get_upstream();

  if (LOG_ENABLED(INFO)) {
    DLOG(INFO, downstream) << "request header timeout stream_id="
                           << downstream->get_stream_id();
  }

  downstream->disable_upstream_rtimer();
  downstream->disable_upstream_wtimer();

  upstream->on_timeout(downstream);
}
} // namespace

namespace {
void upstream_timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto downstream = static_cast<Downstream *>(w->data);
  auto upstream = downstream->get_upstream();

  auto which = revents == EV_READ ? "read" : "write";

  if (LOG_ENABLED(INFO)) {
    DLOG(INFO, downstream) << "upstream timeout stream_id="
                           << downstream->get_stream_id() << " event=" << which;
  }

  downstream->disable_upstream_rtimer();
  downstream->disable_upstream_wtimer();

  upstream->on_timeout(downstream);
}
} // namespace

namespace {
void upstream_rtimeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  upstream_timeoutcb(loop, w, EV_READ);
}
} // namespace

namespace {
void upstream_wtimeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  upstream_timeoutcb(loop, w, EV_WRITE);
}
} // namespace

namespace {
void downstream_timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto downstream = static_cast<Downstream *>(w->data);

  auto which = revents == EV_READ ? "read" : "write";

  if (LOG_ENABLED(INFO)) {
    DLOG(INFO, downstream) << "downstream timeout stream_id="
                           << downstream->get_downstream_stream_id()
                           << " event=" << which;
  }

  downstream->disable_downstream_rtimer();
  downstream->disable_downstream_wtimer();

  auto dconn = downstream->get_downstream_connection();

  if (dconn) {
    dconn->on_timeout();
  }
}
} // namespace

namespace {
void downstream_rtimeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  downstream_timeoutcb(loop, w, EV_READ);
}
} // namespace

namespace {
void downstream_wtimeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  downstream_timeoutcb(loop, w, EV_WRITE);
}
} // namespace

// upstream could be nullptr for unittests
Downstream::Downstream(Upstream *upstream, MemchunkPool *mcpool,
                       int64_t stream_id)
  : dlnext(nullptr),
    dlprev(nullptr),
    response_sent_body_length(0),
    balloc_(1024, 1024),
    req_(balloc_),
    resp_(balloc_),
    request_start_time_(std::chrono::high_resolution_clock::now()),
    blocked_request_buf_(mcpool),
    request_buf_(mcpool),
    response_buf_(mcpool),
    upstream_(upstream),
    blocked_link_(nullptr),
    addr_(nullptr),
    num_retry_(0),
    stream_id_(stream_id),
    assoc_stream_id_(-1),
    downstream_stream_id_(-1),
    response_rst_stream_error_code_(NGHTTP2_NO_ERROR),
    affinity_cookie_(0),
    request_state_(DownstreamState::INITIAL),
    response_state_(DownstreamState::INITIAL),
    dispatch_state_(DispatchState::NONE),
    upgraded_(false),
    chunked_request_(false),
    chunked_response_(false),
    expect_final_response_(false),
    request_pending_(false),
    request_header_sent_(false),
    accesslog_written_(false),
    new_affinity_cookie_(false),
    blocked_request_data_eof_(false),
    expect_100_continue_(false),
    stop_reading_(false) {
  auto config = get_config();
  auto &httpconf = config->http;

  ev_timer_init(&header_timer_, header_timeoutcb, 0., httpconf.timeout.header);

  auto &timeoutconf = config->http2.timeout;

  ev_timer_init(&upstream_rtimer_, &upstream_rtimeoutcb, 0.,
                timeoutconf.stream_read);
  ev_timer_init(&upstream_wtimer_, &upstream_wtimeoutcb, 0.,
                timeoutconf.stream_write);
  ev_timer_init(&downstream_rtimer_, &downstream_rtimeoutcb, 0.,
                timeoutconf.stream_read);
  ev_timer_init(&downstream_wtimer_, &downstream_wtimeoutcb, 0.,
                timeoutconf.stream_write);

  header_timer_.data = this;
  upstream_rtimer_.data = this;
  upstream_wtimer_.data = this;
  downstream_rtimer_.data = this;
  downstream_wtimer_.data = this;

  rcbufs_.reserve(32);
#ifdef ENABLE_HTTP3
  rcbufs3_.reserve(32);
#endif // ENABLE_HTTP3
}

Downstream::~Downstream() {
  if (LOG_ENABLED(INFO)) {
    DLOG(INFO, this) << "Deleting";
  }

  // check nullptr for unittest
  if (upstream_) {
    auto loop = upstream_->get_client_handler()->get_loop();

    ev_timer_stop(loop, &upstream_rtimer_);
    ev_timer_stop(loop, &upstream_wtimer_);
    ev_timer_stop(loop, &downstream_rtimer_);
    ev_timer_stop(loop, &downstream_wtimer_);
    ev_timer_stop(loop, &header_timer_);

#ifdef HAVE_MRUBY
    auto handler = upstream_->get_client_handler();
    auto worker = handler->get_worker();
    auto mruby_ctx = worker->get_mruby_context();

    mruby_ctx->delete_downstream(this);
#endif // HAVE_MRUBY
  }

#ifdef HAVE_MRUBY
  if (dconn_) {
    const auto &group = dconn_->get_downstream_addr_group();
    if (group) {
      const auto &mruby_ctx = group->shared_addr->mruby_ctx;
      mruby_ctx->delete_downstream(this);
    }
  }
#endif // HAVE_MRUBY

  // DownstreamConnection may refer to this object.  Delete it now
  // explicitly.
  dconn_.reset();

#ifdef ENABLE_HTTP3
  for (auto rcbuf : rcbufs3_) {
    nghttp3_rcbuf_decref(rcbuf);
  }
#endif // ENABLE_HTTP3

  for (auto rcbuf : rcbufs_) {
    nghttp2_rcbuf_decref(rcbuf);
  }

  if (LOG_ENABLED(INFO)) {
    DLOG(INFO, this) << "Deleted";
  }
}

int Downstream::attach_downstream_connection(
  std::unique_ptr<DownstreamConnection> dconn) {
  if (dconn->attach_downstream(this) != 0) {
    return -1;
  }

  dconn_ = std::move(dconn);

  return 0;
}

void Downstream::detach_downstream_connection() {
  if (!dconn_) {
    return;
  }

#ifdef HAVE_MRUBY
  const auto &group = dconn_->get_downstream_addr_group();
  if (group) {
    const auto &mruby_ctx = group->shared_addr->mruby_ctx;
    mruby_ctx->delete_downstream(this);
  }
#endif // HAVE_MRUBY

  dconn_->detach_downstream(this);

  auto handler = dconn_->get_client_handler();

  handler->pool_downstream_connection(
    std::unique_ptr<DownstreamConnection>(dconn_.release()));
}

DownstreamConnection *Downstream::get_downstream_connection() {
  return dconn_.get();
}

std::unique_ptr<DownstreamConnection> Downstream::pop_downstream_connection() {
#ifdef HAVE_MRUBY
  if (!dconn_) {
    return nullptr;
  }

  const auto &group = dconn_->get_downstream_addr_group();
  if (group) {
    const auto &mruby_ctx = group->shared_addr->mruby_ctx;
    mruby_ctx->delete_downstream(this);
  }
#endif // HAVE_MRUBY

  return std::unique_ptr<DownstreamConnection>(dconn_.release());
}

void Downstream::pause_read(IOCtrlReason reason) {
  if (dconn_) {
    dconn_->pause_read(reason);
  }
}

int Downstream::resume_read(IOCtrlReason reason, size_t consumed) {
  if (dconn_) {
    return dconn_->resume_read(reason, consumed);
  }

  return 0;
}

void Downstream::force_resume_read() {
  if (dconn_) {
    dconn_->force_resume_read();
  }
}

namespace {
const HeaderRefs::value_type *
search_header_linear_backwards(const HeaderRefs &headers,
                               const StringRef &name) {
  for (auto it = headers.rbegin(); it != headers.rend(); ++it) {
    auto &kv = *it;
    if (kv.name == name) {
      return &kv;
    }
  }
  return nullptr;
}
} // namespace

StringRef Downstream::assemble_request_cookie() {
  size_t len = 0;

  for (auto &kv : req_.fs.headers()) {
    if (kv.token != http2::HD_COOKIE || kv.value.empty()) {
      continue;
    }

    len += kv.value.size() + str_size("; ");
  }

  auto iov = make_byte_ref(balloc_, len + 1);
  auto p = std::begin(iov);

  for (auto &kv : req_.fs.headers()) {
    if (kv.token != http2::HD_COOKIE || kv.value.empty()) {
      continue;
    }

    auto end = std::end(kv.value);
    for (auto it = std::begin(kv.value) + kv.value.size();
         it != std::begin(kv.value); --it) {
      auto c = *(it - 1);
      if (c == ' ' || c == ';') {
        continue;
      }
      end = it;
      break;
    }

    p = std::copy(std::begin(kv.value), end, p);
    p = util::copy_lit(p, "; ");
  }

  // cut trailing "; "
  if (p - std::begin(iov) >= 2) {
    p -= 2;
  }

  return StringRef{std::span{std::begin(iov), p}};
}

uint32_t Downstream::find_affinity_cookie(const StringRef &name) {
  for (auto &kv : req_.fs.headers()) {
    if (kv.token != http2::HD_COOKIE) {
      continue;
    }

    for (auto it = std::begin(kv.value); it != std::end(kv.value);) {
      if (*it == '\t' || *it == ' ' || *it == ';') {
        ++it;
        continue;
      }

      auto end = std::find(it, std::end(kv.value), '=');
      if (end == std::end(kv.value)) {
        return 0;
      }

      if (name != StringRef{it, end}) {
        it = std::find(it, std::end(kv.value), ';');
        continue;
      }

      it = std::find(end + 1, std::end(kv.value), ';');
      auto val = StringRef{end + 1, it};
      if (val.size() != 8) {
        return 0;
      }
      uint32_t h = 0;
      for (auto c : val) {
        auto n = util::hex_to_uint(c);
        if (n == 256) {
          return 0;
        }
        h <<= 4;
        h += n;
      }
      affinity_cookie_ = h;
      return h;
    }
  }
  return 0;
}

size_t Downstream::count_crumble_request_cookie() {
  size_t n = 0;
  for (auto &kv : req_.fs.headers()) {
    if (kv.token != http2::HD_COOKIE) {
      continue;
    }

    for (auto it = std::begin(kv.value); it != std::end(kv.value);) {
      if (*it == '\t' || *it == ' ' || *it == ';') {
        ++it;
        continue;
      }

      it = std::find(it, std::end(kv.value), ';');

      ++n;
    }
  }
  return n;
}

void Downstream::crumble_request_cookie(std::vector<nghttp2_nv> &nva) {
  for (auto &kv : req_.fs.headers()) {
    if (kv.token != http2::HD_COOKIE) {
      continue;
    }

    for (auto it = std::begin(kv.value); it != std::end(kv.value);) {
      if (*it == '\t' || *it == ' ' || *it == ';') {
        ++it;
        continue;
      }

      auto first = it;

      it = std::find(it, std::end(kv.value), ';');

      nva.push_back({(uint8_t *)"cookie", (uint8_t *)first, str_size("cookie"),
                     (size_t)(it - first),
                     (uint8_t)(NGHTTP2_NV_FLAG_NO_COPY_NAME |
                               NGHTTP2_NV_FLAG_NO_COPY_VALUE |
                               (kv.no_index ? NGHTTP2_NV_FLAG_NO_INDEX : 0))});
    }
  }
}

namespace {
void add_header(size_t &sum, HeaderRefs &headers, const StringRef &name,
                const StringRef &value, bool no_index, int32_t token) {
  sum += name.size() + value.size();
  headers.emplace_back(name, value, no_index, token);
}
} // namespace

namespace {
StringRef alloc_header_name(BlockAllocator &balloc, const StringRef &name) {
  auto iov = make_byte_ref(balloc, name.size() + 1);
  auto p = std::copy(std::begin(name), std::end(name), std::begin(iov));
  util::inp_strlower(std::begin(iov), p);
  *p = '\0';

  return StringRef{std::span{std::begin(iov), p}};
}
} // namespace

namespace {
void append_last_header_key(BlockAllocator &balloc, bool &key_prev, size_t &sum,
                            HeaderRefs &headers, const char *data, size_t len) {
  assert(key_prev);
  sum += len;
  auto &item = headers.back();
  auto name =
    realloc_concat_string_ref(balloc, item.name, StringRef{data, len});

  auto p = const_cast<uint8_t *>(name.byte());
  util::inp_strlower(p + name.size() - len, p + name.size());

  item.name = name;
  item.token = http2::lookup_token(item.name);
}
} // namespace

namespace {
void append_last_header_value(BlockAllocator &balloc, bool &key_prev,
                              size_t &sum, HeaderRefs &headers,
                              const char *data, size_t len) {
  key_prev = false;
  sum += len;
  auto &item = headers.back();
  item.value =
    realloc_concat_string_ref(balloc, item.value, StringRef{data, len});
}
} // namespace

int FieldStore::parse_content_length() {
  content_length = -1;

  for (auto &kv : headers_) {
    if (kv.token != http2::HD_CONTENT_LENGTH) {
      continue;
    }

    auto len = util::parse_uint(kv.value);
    if (!len) {
      return -1;
    }
    if (content_length != -1) {
      return -1;
    }
    content_length = *len;
  }
  return 0;
}

const HeaderRefs::value_type *FieldStore::header(int32_t token) const {
  for (auto it = headers_.rbegin(); it != headers_.rend(); ++it) {
    auto &kv = *it;
    if (kv.token == token) {
      return &kv;
    }
  }
  return nullptr;
}

HeaderRefs::value_type *FieldStore::header(int32_t token) {
  for (auto it = headers_.rbegin(); it != headers_.rend(); ++it) {
    auto &kv = *it;
    if (kv.token == token) {
      return &kv;
    }
  }
  return nullptr;
}

const HeaderRefs::value_type *FieldStore::header(const StringRef &name) const {
  return search_header_linear_backwards(headers_, name);
}

void FieldStore::add_header_token(const StringRef &name, const StringRef &value,
                                  bool no_index, int32_t token) {
  shrpx::add_header(buffer_size_, headers_, name, value, no_index, token);
}

void FieldStore::alloc_add_header_name(const StringRef &name) {
  auto name_ref = alloc_header_name(balloc_, name);
  auto token = http2::lookup_token(name_ref);
  add_header_token(name_ref, StringRef{}, false, token);
  header_key_prev_ = true;
}

void FieldStore::append_last_header_key(const char *data, size_t len) {
  shrpx::append_last_header_key(balloc_, header_key_prev_, buffer_size_,
                                headers_, data, len);
}

void FieldStore::append_last_header_value(const char *data, size_t len) {
  shrpx::append_last_header_value(balloc_, header_key_prev_, buffer_size_,
                                  headers_, data, len);
}

void FieldStore::clear_headers() {
  headers_.clear();
  header_key_prev_ = false;
}

void FieldStore::add_trailer_token(const StringRef &name,
                                   const StringRef &value, bool no_index,
                                   int32_t token) {
  // Header size limit should be applied to all header and trailer
  // fields combined.
  shrpx::add_header(buffer_size_, trailers_, name, value, no_index, token);
}

void FieldStore::alloc_add_trailer_name(const StringRef &name) {
  auto name_ref = alloc_header_name(balloc_, name);
  auto token = http2::lookup_token(name_ref);
  add_trailer_token(name_ref, StringRef{}, false, token);
  trailer_key_prev_ = true;
}

void FieldStore::append_last_trailer_key(const char *data, size_t len) {
  shrpx::append_last_header_key(balloc_, trailer_key_prev_, buffer_size_,
                                trailers_, data, len);
}

void FieldStore::append_last_trailer_value(const char *data, size_t len) {
  shrpx::append_last_header_value(balloc_, trailer_key_prev_, buffer_size_,
                                  trailers_, data, len);
}

void FieldStore::erase_content_length_and_transfer_encoding() {
  for (auto &kv : headers_) {
    switch (kv.token) {
    case http2::HD_CONTENT_LENGTH:
    case http2::HD_TRANSFER_ENCODING:
      kv.name = StringRef{};
      kv.token = -1;
      break;
    }
  }
}

void Downstream::set_request_start_time(
  std::chrono::high_resolution_clock::time_point time) {
  request_start_time_ = std::move(time);
}

const std::chrono::high_resolution_clock::time_point &
Downstream::get_request_start_time() const {
  return request_start_time_;
}

void Downstream::reset_upstream(Upstream *upstream) {
  upstream_ = upstream;
  if (dconn_) {
    dconn_->on_upstream_change(upstream);
  }
}

Upstream *Downstream::get_upstream() const { return upstream_; }

void Downstream::set_stream_id(int64_t stream_id) { stream_id_ = stream_id; }

int64_t Downstream::get_stream_id() const { return stream_id_; }

void Downstream::set_request_state(DownstreamState state) {
  request_state_ = state;
}

DownstreamState Downstream::get_request_state() const { return request_state_; }

bool Downstream::get_chunked_request() const { return chunked_request_; }

void Downstream::set_chunked_request(bool f) { chunked_request_ = f; }

bool Downstream::request_buf_full() {
  auto handler = upstream_->get_client_handler();
  auto faddr = handler->get_upstream_addr();
  auto worker = handler->get_worker();

  // We don't check buffer size here for API endpoint.
  if (faddr->alt_mode == UpstreamAltMode::API) {
    return false;
  }

  if (dconn_) {
    auto &downstreamconf = *worker->get_downstream_config();
    return blocked_request_buf_.rleft() + request_buf_.rleft() >=
           downstreamconf.request_buffer_size;
  }

  return false;
}

DefaultMemchunks *Downstream::get_request_buf() { return &request_buf_; }

// Call this function after this object is attached to
// Downstream. Otherwise, the program will crash.
int Downstream::push_request_headers() {
  if (!dconn_) {
    DLOG(INFO, this) << "dconn_ is NULL";
    return -1;
  }
  return dconn_->push_request_headers();
}

int Downstream::push_upload_data_chunk(const uint8_t *data, size_t datalen) {
  req_.recv_body_length += datalen;

  if (!dconn_ && !request_header_sent_) {
    blocked_request_buf_.append(data, datalen);
    req_.unconsumed_body_length += datalen;
    return 0;
  }

  // Assumes that request headers have already been pushed to output
  // buffer using push_request_headers().
  if (!dconn_) {
    DLOG(INFO, this) << "dconn_ is NULL";
    return -1;
  }
  if (dconn_->push_upload_data_chunk(data, datalen) != 0) {
    return -1;
  }

  req_.unconsumed_body_length += datalen;

  return 0;
}

int Downstream::end_upload_data() {
  if (!dconn_ && !request_header_sent_) {
    blocked_request_data_eof_ = true;
    return 0;
  }
  if (!dconn_) {
    DLOG(INFO, this) << "dconn_ is NULL";
    return -1;
  }
  return dconn_->end_upload_data();
}

void Downstream::rewrite_location_response_header(
  const StringRef &upstream_scheme) {
  auto hd = resp_.fs.header(http2::HD_LOCATION);
  if (!hd) {
    return;
  }

  if (request_downstream_host_.empty() || req_.authority.empty()) {
    return;
  }

  urlparse_url u;
  auto rv = urlparse_parse_url(hd->value.data(), hd->value.size(), 0, &u);
  if (rv != 0) {
    return;
  }

  auto new_uri =
    http2::rewrite_location_uri(balloc_, hd->value, u, request_downstream_host_,
                                req_.authority, upstream_scheme);

  if (new_uri.empty()) {
    return;
  }

  hd->value = new_uri;
}

bool Downstream::get_chunked_response() const { return chunked_response_; }

void Downstream::set_chunked_response(bool f) { chunked_response_ = f; }

int Downstream::on_read() {
  if (!dconn_) {
    DLOG(INFO, this) << "dconn_ is NULL";
    return -1;
  }
  return dconn_->on_read();
}

void Downstream::set_response_state(DownstreamState state) {
  response_state_ = state;
}

DownstreamState Downstream::get_response_state() const {
  return response_state_;
}

DefaultMemchunks *Downstream::get_response_buf() { return &response_buf_; }

bool Downstream::response_buf_full() {
  if (dconn_) {
    auto handler = upstream_->get_client_handler();
    auto worker = handler->get_worker();
    auto &downstreamconf = *worker->get_downstream_config();

    return response_buf_.rleft() >= downstreamconf.response_buffer_size;
  }

  return false;
}

bool Downstream::validate_request_recv_body_length() const {
  if (req_.fs.content_length == -1) {
    return true;
  }

  if (req_.fs.content_length != req_.recv_body_length) {
    if (LOG_ENABLED(INFO)) {
      DLOG(INFO, this) << "request invalid bodylen: content-length="
                       << req_.fs.content_length
                       << ", received=" << req_.recv_body_length;
    }
    return false;
  }

  return true;
}

bool Downstream::validate_response_recv_body_length() const {
  if (!expect_response_body() || resp_.fs.content_length == -1) {
    return true;
  }

  if (resp_.fs.content_length != resp_.recv_body_length) {
    if (LOG_ENABLED(INFO)) {
      DLOG(INFO, this) << "response invalid bodylen: content-length="
                       << resp_.fs.content_length
                       << ", received=" << resp_.recv_body_length;
    }
    return false;
  }

  return true;
}

void Downstream::check_upgrade_fulfilled_http2() {
  // This handles nonzero req_.connect_proto and h1 frontend requests
  // WebSocket upgrade.
  upgraded_ = (req_.method == HTTP_CONNECT ||
               req_.connect_proto == ConnectProto::WEBSOCKET) &&
              resp_.http_status / 100 == 2;
}

void Downstream::check_upgrade_fulfilled_http1() {
  if (req_.method == HTTP_CONNECT) {
    if (req_.connect_proto == ConnectProto::WEBSOCKET) {
      if (resp_.http_status != 101) {
        return;
      }

      // This is done for HTTP/2 frontend only.
      auto accept = resp_.fs.header(http2::HD_SEC_WEBSOCKET_ACCEPT);
      if (!accept) {
        return;
      }

      std::array<uint8_t, base64::encode_length(20)> accept_buf;
      auto expected =
        http2::make_websocket_accept_token(accept_buf.data(), ws_key_);

      upgraded_ = !expected.empty() && expected == accept->value;
    } else {
      upgraded_ = resp_.http_status / 100 == 2;
    }

    return;
  }

  if (resp_.http_status == 101) {
    // TODO Do more strict checking for upgrade headers
    upgraded_ = req_.upgrade_request;

    return;
  }
}

void Downstream::inspect_http2_request() {
  if (req_.method == HTTP_CONNECT) {
    req_.upgrade_request = true;
  }
}

void Downstream::inspect_http1_request() {
  if (req_.method == HTTP_CONNECT) {
    req_.upgrade_request = true;
  } else if (req_.http_minor > 0) {
    auto upgrade = req_.fs.header(http2::HD_UPGRADE);
    if (upgrade) {
      const auto &val = upgrade->value;
      // TODO Perform more strict checking for upgrade headers
      if (NGHTTP2_CLEARTEXT_PROTO_VERSION_ID ""_sr == val) {
        req_.http2_upgrade_seen = true;
      } else {
        req_.upgrade_request = true;

        // TODO Should we check Sec-WebSocket-Key, and
        // Sec-WebSocket-Version as well?
        if (util::strieq("websocket"_sr, val)) {
          req_.connect_proto = ConnectProto::WEBSOCKET;
        }
      }
    }
  }
  auto transfer_encoding = req_.fs.header(http2::HD_TRANSFER_ENCODING);
  if (transfer_encoding) {
    req_.fs.content_length = -1;
  }

  auto expect = req_.fs.header(http2::HD_EXPECT);
  expect_100_continue_ =
    expect && util::strieq(expect->value, "100-continue"_sr);
}

void Downstream::inspect_http1_response() {
  auto transfer_encoding = resp_.fs.header(http2::HD_TRANSFER_ENCODING);
  if (transfer_encoding) {
    resp_.fs.content_length = -1;
  }
}

void Downstream::reset_response() {
  resp_.http_status = 0;
  resp_.http_major = 1;
  resp_.http_minor = 1;
}

bool Downstream::get_non_final_response() const {
  return !upgraded_ && resp_.http_status / 100 == 1;
}

bool Downstream::supports_non_final_response() const {
  return req_.http_major == 3 || req_.http_major == 2 ||
         (req_.http_major == 1 && req_.http_minor == 1);
}

bool Downstream::get_upgraded() const { return upgraded_; }

bool Downstream::get_http2_upgrade_request() const {
  return req_.http2_upgrade_seen && req_.fs.header(http2::HD_HTTP2_SETTINGS) &&
         response_state_ == DownstreamState::INITIAL;
}

StringRef Downstream::get_http2_settings() const {
  auto http2_settings = req_.fs.header(http2::HD_HTTP2_SETTINGS);
  if (!http2_settings) {
    return StringRef{};
  }
  return http2_settings->value;
}

void Downstream::set_downstream_stream_id(int64_t stream_id) {
  downstream_stream_id_ = stream_id;
}

int64_t Downstream::get_downstream_stream_id() const {
  return downstream_stream_id_;
}

uint32_t Downstream::get_response_rst_stream_error_code() const {
  return response_rst_stream_error_code_;
}

void Downstream::set_response_rst_stream_error_code(uint32_t error_code) {
  response_rst_stream_error_code_ = error_code;
}

void Downstream::set_expect_final_response(bool f) {
  expect_final_response_ = f;
}

bool Downstream::get_expect_final_response() const {
  return expect_final_response_;
}

bool Downstream::expect_response_body() const {
  return !resp_.headers_only &&
         http2::expect_response_body(req_.method, resp_.http_status);
}

bool Downstream::expect_response_trailer() const {
  // In HTTP/2, if final response HEADERS does not bear END_STREAM it
  // is possible trailer fields might come, regardless of request
  // method or status code.
  return !resp_.headers_only &&
         (resp_.http_major == 3 || resp_.http_major == 2);
}

void Downstream::repeat_header_timer() {
  auto loop = upstream_->get_client_handler()->get_loop();

  ev_timer_again(loop, &header_timer_);
}

void Downstream::stop_header_timer() {
  auto loop = upstream_->get_client_handler()->get_loop();

  ev_timer_stop(loop, &header_timer_);
}

namespace {
void reset_timer(struct ev_loop *loop, ev_timer *w) { ev_timer_again(loop, w); }
} // namespace

namespace {
void try_reset_timer(struct ev_loop *loop, ev_timer *w) {
  if (!ev_is_active(w)) {
    return;
  }
  ev_timer_again(loop, w);
}
} // namespace

namespace {
void ensure_timer(struct ev_loop *loop, ev_timer *w) {
  if (ev_is_active(w)) {
    return;
  }
  ev_timer_again(loop, w);
}
} // namespace

namespace {
void disable_timer(struct ev_loop *loop, ev_timer *w) {
  ev_timer_stop(loop, w);
}
} // namespace

void Downstream::reset_upstream_rtimer() {
  if (get_config()->http2.timeout.stream_read == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  reset_timer(loop, &upstream_rtimer_);
}

void Downstream::reset_upstream_wtimer() {
  auto loop = upstream_->get_client_handler()->get_loop();
  auto &timeoutconf = get_config()->http2.timeout;

  if (timeoutconf.stream_write != 0.) {
    reset_timer(loop, &upstream_wtimer_);
  }
  if (timeoutconf.stream_read != 0.) {
    try_reset_timer(loop, &upstream_rtimer_);
  }
}

void Downstream::ensure_upstream_wtimer() {
  if (get_config()->http2.timeout.stream_write == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  ensure_timer(loop, &upstream_wtimer_);
}

void Downstream::disable_upstream_rtimer() {
  if (get_config()->http2.timeout.stream_read == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  disable_timer(loop, &upstream_rtimer_);
}

void Downstream::disable_upstream_wtimer() {
  if (get_config()->http2.timeout.stream_write == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  disable_timer(loop, &upstream_wtimer_);
}

void Downstream::reset_downstream_rtimer() {
  if (get_config()->http2.timeout.stream_read == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  reset_timer(loop, &downstream_rtimer_);
}

void Downstream::reset_downstream_wtimer() {
  auto loop = upstream_->get_client_handler()->get_loop();
  auto &timeoutconf = get_config()->http2.timeout;

  if (timeoutconf.stream_write != 0.) {
    reset_timer(loop, &downstream_wtimer_);
  }
  if (timeoutconf.stream_read != 0.) {
    try_reset_timer(loop, &downstream_rtimer_);
  }
}

void Downstream::ensure_downstream_wtimer() {
  if (get_config()->http2.timeout.stream_write == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  ensure_timer(loop, &downstream_wtimer_);
}

void Downstream::disable_downstream_rtimer() {
  if (get_config()->http2.timeout.stream_read == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  disable_timer(loop, &downstream_rtimer_);
}

void Downstream::disable_downstream_wtimer() {
  if (get_config()->http2.timeout.stream_write == 0.) {
    return;
  }
  auto loop = upstream_->get_client_handler()->get_loop();
  disable_timer(loop, &downstream_wtimer_);
}

bool Downstream::accesslog_ready() const {
  return !accesslog_written_ && resp_.http_status > 0;
}

void Downstream::add_retry() { ++num_retry_; }

bool Downstream::no_more_retry() const { return num_retry_ > 50; }

void Downstream::set_request_downstream_host(const StringRef &host) {
  request_downstream_host_ = host;
}

void Downstream::set_request_pending(bool f) { request_pending_ = f; }

bool Downstream::get_request_pending() const { return request_pending_; }

void Downstream::set_request_header_sent(bool f) { request_header_sent_ = f; }

bool Downstream::get_request_header_sent() const {
  return request_header_sent_;
}

bool Downstream::request_submission_ready() const {
  return (request_state_ == DownstreamState::HEADER_COMPLETE ||
          request_state_ == DownstreamState::MSG_COMPLETE) &&
         (request_pending_ || !request_header_sent_) &&
         response_state_ == DownstreamState::INITIAL;
}

DispatchState Downstream::get_dispatch_state() const { return dispatch_state_; }

void Downstream::set_dispatch_state(DispatchState s) { dispatch_state_ = s; }

void Downstream::attach_blocked_link(BlockedLink *l) {
  assert(!blocked_link_);

  l->downstream = this;
  blocked_link_ = l;
}

BlockedLink *Downstream::detach_blocked_link() {
  auto link = blocked_link_;
  blocked_link_ = nullptr;
  return link;
}

bool Downstream::can_detach_downstream_connection() const {
  // We should check request and response buffer.  If request buffer
  // is not empty, then we might leave downstream connection in weird
  // state, especially for HTTP/1.1
  return dconn_ && response_state_ == DownstreamState::MSG_COMPLETE &&
         request_state_ == DownstreamState::MSG_COMPLETE && !upgraded_ &&
         !resp_.connection_close && request_buf_.rleft() == 0;
}

DefaultMemchunks Downstream::pop_response_buf() {
  return std::move(response_buf_);
}

void Downstream::set_assoc_stream_id(int64_t stream_id) {
  assoc_stream_id_ = stream_id;
}

int64_t Downstream::get_assoc_stream_id() const { return assoc_stream_id_; }

BlockAllocator &Downstream::get_block_allocator() { return balloc_; }

void Downstream::add_rcbuf(nghttp2_rcbuf *rcbuf) {
  nghttp2_rcbuf_incref(rcbuf);
  rcbufs_.push_back(rcbuf);
}

#ifdef ENABLE_HTTP3
void Downstream::add_rcbuf(nghttp3_rcbuf *rcbuf) {
  nghttp3_rcbuf_incref(rcbuf);
  rcbufs3_.push_back(rcbuf);
}
#endif // ENABLE_HTTP3

void Downstream::set_downstream_addr_group(
  const std::shared_ptr<DownstreamAddrGroup> &group) {
  group_ = group;
}

void Downstream::set_addr(const DownstreamAddr *addr) { addr_ = addr; }

const DownstreamAddr *Downstream::get_addr() const { return addr_; }

void Downstream::set_accesslog_written(bool f) { accesslog_written_ = f; }

void Downstream::renew_affinity_cookie(uint32_t h) {
  affinity_cookie_ = h;
  new_affinity_cookie_ = true;
}

uint32_t Downstream::get_affinity_cookie_to_send() const {
  if (new_affinity_cookie_) {
    return affinity_cookie_;
  }
  return 0;
}

DefaultMemchunks *Downstream::get_blocked_request_buf() {
  return &blocked_request_buf_;
}

bool Downstream::get_blocked_request_data_eof() const {
  return blocked_request_data_eof_;
}

void Downstream::set_blocked_request_data_eof(bool f) {
  blocked_request_data_eof_ = f;
}

void Downstream::set_ws_key(const StringRef &key) { ws_key_ = key; }

bool Downstream::get_expect_100_continue() const {
  return expect_100_continue_;
}

bool Downstream::get_stop_reading() const { return stop_reading_; }

void Downstream::set_stop_reading(bool f) { stop_reading_ = f; }

} // namespace shrpx
