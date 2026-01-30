/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2016 Tatsuhiro Tsujikawa
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
#include "shrpx_api_downstream_connection.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

#include "shrpx_client_handler.h"
#include "shrpx_upstream.h"
#include "shrpx_downstream.h"
#include "shrpx_worker.h"
#include "shrpx_connection_handler.h"
#include "shrpx_log.h"

namespace shrpx {

namespace {
const auto backendconfig_endpoint = APIEndpoint{
  "/api/v1beta1/backendconfig"_sr,
  true,
  (1 << API_METHOD_POST) | (1 << API_METHOD_PUT),
  &APIDownstreamConnection::handle_backendconfig,
};

const auto configrevision_endpoint = APIEndpoint{
  "/api/v1beta1/configrevision"_sr,
  true,
  (1 << API_METHOD_GET),
  &APIDownstreamConnection::handle_configrevision,
};
} // namespace

namespace {
// The method string.  This must be same order of APIMethod.
constexpr StringRef API_METHOD_STRING[] = {
  "GET"_sr,
  "POST"_sr,
  "PUT"_sr,
};
} // namespace

APIDownstreamConnection::APIDownstreamConnection(Worker *worker)
  : worker_(worker), api_(nullptr), fd_(-1), shutdown_read_(false) {}

APIDownstreamConnection::~APIDownstreamConnection() {
  if (fd_ != -1) {
    close(fd_);
  }
}

int APIDownstreamConnection::attach_downstream(Downstream *downstream) {
  if (LOG_ENABLED(INFO)) {
    DCLOG(INFO, this) << "Attaching to DOWNSTREAM:" << downstream;
  }

  downstream_ = downstream;

  return 0;
}

void APIDownstreamConnection::detach_downstream(Downstream *downstream) {
  if (LOG_ENABLED(INFO)) {
    DCLOG(INFO, this) << "Detaching from DOWNSTREAM:" << downstream;
  }
  downstream_ = nullptr;
}

int APIDownstreamConnection::send_reply(unsigned int http_status,
                                        APIStatusCode api_status,
                                        const StringRef &data) {
  shutdown_read_ = true;

  auto upstream = downstream_->get_upstream();

  auto &resp = downstream_->response();

  resp.http_status = http_status;

  auto &balloc = downstream_->get_block_allocator();

  StringRef api_status_str;

  switch (api_status) {
  case APIStatusCode::SUCCESS:
    api_status_str = "Success"_sr;
    break;
  case APIStatusCode::FAILURE:
    api_status_str = "Failure"_sr;
    break;
  default:
    assert(0);
  }

  constexpr auto M1 = "{\"status\":\""_sr;
  constexpr auto M2 = "\",\"code\":"_sr;
  constexpr auto M3 = "}"_sr;

  // 3 is the number of digits in http_status, assuming it is 3 digits
  // number.
  auto buflen =
    M1.size() + M2.size() + M3.size() + data.size() + api_status_str.size() + 3;

  auto buf = make_byte_ref(balloc, buflen);
  auto p = std::begin(buf);

  p = std::copy(std::begin(M1), std::end(M1), p);
  p = std::copy(std::begin(api_status_str), std::end(api_status_str), p);
  p = std::copy(std::begin(M2), std::end(M2), p);
  p = util::utos(p, http_status);
  p = std::copy(std::begin(data), std::end(data), p);
  p = std::copy(std::begin(M3), std::end(M3), p);

  buf = buf.subspan(0, p - std::begin(buf));

  auto content_length = util::make_string_ref_uint(balloc, buf.size());

  resp.fs.add_header_token("content-length"_sr, content_length, false,
                           http2::HD_CONTENT_LENGTH);

  switch (http_status) {
  case 400:
  case 405:
  case 413:
    resp.fs.add_header_token("connection"_sr, "close"_sr, false,
                             http2::HD_CONNECTION);
    break;
  }

  if (upstream->send_reply(downstream_, buf.data(), buf.size()) != 0) {
    return -1;
  }

  return 0;
}

namespace {
const APIEndpoint *lookup_api(const StringRef &path) {
  switch (path.size()) {
  case 26:
    switch (path[25]) {
    case 'g':
      if (util::streq("/api/v1beta1/backendconfi"_sr, path, 25)) {
        return &backendconfig_endpoint;
      }
      break;
    }
    break;
  case 27:
    switch (path[26]) {
    case 'n':
      if (util::streq("/api/v1beta1/configrevisio"_sr, path, 26)) {
        return &configrevision_endpoint;
      }
      break;
    }
    break;
  }
  return nullptr;
}
} // namespace

int APIDownstreamConnection::push_request_headers() {
  auto &req = downstream_->request();

  auto path =
    StringRef{std::begin(req.path),
              std::find(std::begin(req.path), std::end(req.path), '?')};

  api_ = lookup_api(path);

  if (!api_) {
    send_reply(404, APIStatusCode::FAILURE);

    return 0;
  }

  switch (req.method) {
  case HTTP_GET:
    if (!(api_->allowed_methods & (1 << API_METHOD_GET))) {
      error_method_not_allowed();
      return 0;
    }
    break;
  case HTTP_POST:
    if (!(api_->allowed_methods & (1 << API_METHOD_POST))) {
      error_method_not_allowed();
      return 0;
    }
    break;
  case HTTP_PUT:
    if (!(api_->allowed_methods & (1 << API_METHOD_PUT))) {
      error_method_not_allowed();
      return 0;
    }
    break;
  default:
    error_method_not_allowed();
    return 0;
  }

  // This works with req.fs.content_length == -1
  if (req.fs.content_length >
      static_cast<int64_t>(get_config()->api.max_request_body)) {
    send_reply(413, APIStatusCode::FAILURE);

    return 0;
  }

  switch (req.method) {
  case HTTP_POST:
  case HTTP_PUT: {
    char tempname[] = "/tmp/nghttpx-api.XXXXXX";
#ifdef HAVE_MKOSTEMP
    fd_ = mkostemp(tempname, O_CLOEXEC);
#else  // !HAVE_MKOSTEMP
    fd_ = mkstemp(tempname);
#endif // !HAVE_MKOSTEMP
    if (fd_ == -1) {
      send_reply(500, APIStatusCode::FAILURE);

      return 0;
    }
#ifndef HAVE_MKOSTEMP
    util::make_socket_closeonexec(fd_);
#endif // HAVE_MKOSTEMP
    unlink(tempname);
    break;
  }
  }

  downstream_->set_request_header_sent(true);
  auto src = downstream_->get_blocked_request_buf();
  auto dest = downstream_->get_request_buf();
  src->remove(*dest);

  return 0;
}

int APIDownstreamConnection::error_method_not_allowed() {
  auto &resp = downstream_->response();

  size_t len = 0;
  for (uint8_t i = 0; i < API_METHOD_MAX; ++i) {
    if (api_->allowed_methods & (1 << i)) {
      // The length of method + ", "
      len += API_METHOD_STRING[i].size() + 2;
    }
  }

  assert(len > 0);

  auto &balloc = downstream_->get_block_allocator();

  auto iov = make_byte_ref(balloc, len + 1);
  auto p = std::begin(iov);
  for (uint8_t i = 0; i < API_METHOD_MAX; ++i) {
    if (api_->allowed_methods & (1 << i)) {
      auto &s = API_METHOD_STRING[i];
      p = std::copy(std::begin(s), std::end(s), p);
      p = std::copy_n(", ", 2, p);
    }
  }

  p -= 2;
  *p = '\0';

  resp.fs.add_header_token("allow"_sr, StringRef{std::span{std::begin(iov), p}},
                           false, -1);
  return send_reply(405, APIStatusCode::FAILURE);
}

int APIDownstreamConnection::push_upload_data_chunk(const uint8_t *data,
                                                    size_t datalen) {
  if (shutdown_read_ || !api_->require_body) {
    return 0;
  }

  auto &req = downstream_->request();
  auto &apiconf = get_config()->api;

  if (static_cast<size_t>(req.recv_body_length) > apiconf.max_request_body) {
    send_reply(413, APIStatusCode::FAILURE);

    return 0;
  }

  ssize_t nwrite;
  while ((nwrite = write(fd_, data, datalen)) == -1 && errno == EINTR)
    ;
  if (nwrite == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not write API request body: errno=" << error;
    send_reply(500, APIStatusCode::FAILURE);

    return 0;
  }

  // We don't have to call Upstream::resume_read() here, because
  // request buffer is effectively unlimited.  Actually, we cannot
  // call it here since it could recursively call this function again.

  return 0;
}

int APIDownstreamConnection::end_upload_data() {
  if (shutdown_read_) {
    return 0;
  }

  return api_->handler(*this);
}

int APIDownstreamConnection::handle_backendconfig() {
  auto &req = downstream_->request();

  if (req.recv_body_length == 0) {
    send_reply(200, APIStatusCode::SUCCESS);

    return 0;
  }

  auto rp = mmap(nullptr, req.recv_body_length, PROT_READ, MAP_SHARED, fd_, 0);
  if (rp == reinterpret_cast<void *>(-1)) {
    send_reply(500, APIStatusCode::FAILURE);
    return 0;
  }

  auto unmapper = defer(munmap, rp, req.recv_body_length);

  Config new_config{};
  new_config.conn.downstream = std::make_shared<DownstreamConfig>();
  const auto &downstreamconf = new_config.conn.downstream;

  auto config = get_config();
  auto &src = config->conn.downstream;

  downstreamconf->timeout = src->timeout;
  downstreamconf->connections_per_host = src->connections_per_host;
  downstreamconf->connections_per_frontend = src->connections_per_frontend;
  downstreamconf->request_buffer_size = src->request_buffer_size;
  downstreamconf->response_buffer_size = src->response_buffer_size;
  downstreamconf->family = src->family;

  std::set<StringRef> include_set;
  std::map<StringRef, size_t> pattern_addr_indexer;

  for (auto first = reinterpret_cast<const uint8_t *>(rp),
            last = first + req.recv_body_length;
       first != last;) {
    auto eol = std::find(first, last, '\n');
    if (eol == last) {
      break;
    }

    if (first == eol || *first == '#') {
      first = ++eol;
      continue;
    }

    auto eq = std::find(first, eol, '=');
    if (eq == eol) {
      send_reply(400, APIStatusCode::FAILURE);
      return 0;
    }

    auto opt = StringRef{std::span{first, eq}};
    auto optval = StringRef{std::span{eq + 1, eol}};

    auto optid = option_lookup_token(opt);

    switch (optid) {
    case SHRPX_OPTID_BACKEND:
      break;
    default:
      first = ++eol;
      continue;
    }

    if (parse_config(&new_config, optid, opt, optval, include_set,
                     pattern_addr_indexer) != 0) {
      send_reply(400, APIStatusCode::FAILURE);
      return 0;
    }

    first = ++eol;
  }

  auto &tlsconf = config->tls;
  if (configure_downstream_group(&new_config, config->http2_proxy, true,
                                 tlsconf) != 0) {
    send_reply(400, APIStatusCode::FAILURE);
    return 0;
  }

  auto conn_handler = worker_->get_connection_handler();

  conn_handler->send_replace_downstream(downstreamconf);

  send_reply(200, APIStatusCode::SUCCESS);

  return 0;
}

int APIDownstreamConnection::handle_configrevision() {
  auto config = get_config();
  auto &balloc = downstream_->get_block_allocator();

  // Construct the following string:
  //   ,
  //   "data":{
  //     "configRevision": N
  //   }
  auto data = concat_string_ref(
    balloc, R"(,"data":{"configRevision":)"_sr,
    util::make_string_ref_uint(balloc, config->config_revision), "}"_sr);

  send_reply(200, APIStatusCode::SUCCESS, data);

  return 0;
}

void APIDownstreamConnection::pause_read(IOCtrlReason reason) {}

int APIDownstreamConnection::resume_read(IOCtrlReason reason, size_t consumed) {
  return 0;
}

void APIDownstreamConnection::force_resume_read() {}

int APIDownstreamConnection::on_read() { return 0; }

int APIDownstreamConnection::on_write() { return 0; }

void APIDownstreamConnection::on_upstream_change(Upstream *upstream) {}

bool APIDownstreamConnection::poolable() const { return false; }

const std::shared_ptr<DownstreamAddrGroup> &
APIDownstreamConnection::get_downstream_addr_group() const {
  static std::shared_ptr<DownstreamAddrGroup> s;
  return s;
}

DownstreamAddr *APIDownstreamConnection::get_addr() const { return nullptr; }

} // namespace shrpx
