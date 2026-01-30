/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 British Broadcasting Corporation
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
#include "h2load_http1_session.h"

#include <cassert>
#include <cerrno>

#include "h2load.h"
#include "util.h"
#include "template.h"

#include <iostream>
#include <fstream>

using namespace nghttp2;

namespace h2load {

namespace {
// HTTP response message begin
int htp_msg_begincb(llhttp_t *htp) {
  auto session = static_cast<Http1Session *>(htp->data);

  if (session->stream_resp_counter_ > session->stream_req_counter_) {
    return -1;
  }

  return 0;
}
} // namespace

namespace {
// HTTP response status code
int htp_statuscb(llhttp_t *htp, const char *at, size_t length) {
  auto session = static_cast<Http1Session *>(htp->data);
  auto client = session->get_client();

  if (htp->status_code / 100 == 1) {
    return 0;
  }

  client->on_status_code(session->stream_resp_counter_, htp->status_code);

  return 0;
}
} // namespace

namespace {
// HTTP response message complete
int htp_msg_completecb(llhttp_t *htp) {
  auto session = static_cast<Http1Session *>(htp->data);
  auto client = session->get_client();

  if (htp->status_code / 100 == 1) {
    return 0;
  }

  client->final = llhttp_should_keep_alive(htp) == 0;
  auto req_stat = client->get_req_stat(session->stream_resp_counter_);

  assert(req_stat);

  auto config = client->worker->config;
  if (req_stat->data_offset >= config->data_length) {
    client->on_stream_close(session->stream_resp_counter_, true, client->final);
  }

  session->stream_resp_counter_ += 2;

  if (client->final) {
    session->stream_req_counter_ = session->stream_resp_counter_;

    // Connection is going down.  If we have still request to do,
    // create new connection and keep on doing the job.
    if (client->req_left) {
      client->try_new_connection();
    }

    return HPE_PAUSED;
  }

  return 0;
}
} // namespace

namespace {
int htp_hdr_keycb(llhttp_t *htp, const char *data, size_t len) {
  auto session = static_cast<Http1Session *>(htp->data);
  auto client = session->get_client();

  client->worker->stats.bytes_head += len;
  client->worker->stats.bytes_head_decomp += len;
  return 0;
}
} // namespace

namespace {
int htp_hdr_valcb(llhttp_t *htp, const char *data, size_t len) {
  auto session = static_cast<Http1Session *>(htp->data);
  auto client = session->get_client();

  client->worker->stats.bytes_head += len;
  client->worker->stats.bytes_head_decomp += len;
  return 0;
}
} // namespace

namespace {
int htp_hdrs_completecb(llhttp_t *htp) {
  return !http2::expect_response_body(htp->status_code);
}
} // namespace

namespace {
int htp_body_cb(llhttp_t *htp, const char *data, size_t len) {
  auto session = static_cast<Http1Session *>(htp->data);
  auto client = session->get_client();

  client->record_ttfb();
  client->worker->stats.bytes_body += len;

  return 0;
}
} // namespace

namespace {
constexpr llhttp_settings_t htp_hooks = {
  htp_msg_begincb,     // llhttp_cb      on_message_begin;
  nullptr,             // llhttp_data_cb on_url;
  htp_statuscb,        // llhttp_data_cb on_status;
  nullptr,             // llhttp_data_cb on_method;
  nullptr,             // llhttp_data_cb on_version;
  htp_hdr_keycb,       // llhttp_data_cb on_header_field;
  htp_hdr_valcb,       // llhttp_data_cb on_header_value;
  nullptr,             // llhttp_data_cb on_chunk_extension_name;
  nullptr,             // llhttp_data_cb on_chunk_extension_value;
  htp_hdrs_completecb, // llhttp_cb      on_headers_complete;
  htp_body_cb,         // llhttp_data_cb on_body;
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

Http1Session::Http1Session(Client *client)
  : stream_req_counter_(1),
    stream_resp_counter_(1),
    client_(client),
    htp_(),
    complete_(false) {
  llhttp_init(&htp_, HTTP_RESPONSE, &htp_hooks);
  htp_.data = this;
}

Http1Session::~Http1Session() {}

void Http1Session::on_connect() { client_->signal_write(); }

int Http1Session::submit_request() {
  auto config = client_->worker->config;
  const auto &req = config->h1reqs[client_->reqidx];
  client_->reqidx++;

  if (client_->reqidx == config->h1reqs.size()) {
    client_->reqidx = 0;
  }

  client_->on_request(stream_req_counter_);

  auto req_stat = client_->get_req_stat(stream_req_counter_);

  client_->record_request_time(req_stat);
  client_->wb.append(req);

  if (config->data_fd == -1 || config->data_length == 0) {
    // increment for next request
    stream_req_counter_ += 2;

    return 0;
  }

  return on_write();
}

int Http1Session::on_read(const uint8_t *data, size_t len) {
  auto htperr =
    llhttp_execute(&htp_, reinterpret_cast<const char *>(data), len);
  auto nread = htperr == HPE_OK
                 ? len
                 : static_cast<size_t>(reinterpret_cast<const uint8_t *>(
                                         llhttp_get_error_pos(&htp_)) -
                                       data);

  if (client_->worker->config->verbose) {
    std::cout.write(reinterpret_cast<const char *>(data), nread);
  }

  if (htperr == HPE_PAUSED) {
    // pause is done only when connection: close is requested
    return -1;
  }

  if (htperr != HPE_OK) {
    std::cerr << "[ERROR] HTTP parse error: "
              << "(" << llhttp_errno_name(htperr) << ") "
              << llhttp_get_error_reason(&htp_) << std::endl;
    return -1;
  }

  return 0;
}

int Http1Session::on_write() {
  if (complete_) {
    return -1;
  }

  auto config = client_->worker->config;
  auto req_stat = client_->get_req_stat(stream_req_counter_);
  if (!req_stat) {
    return 0;
  }

  if (req_stat->data_offset < config->data_length) {
    auto req_stat = client_->get_req_stat(stream_req_counter_);
    auto &wb = client_->wb;

    // TODO unfortunately, wb has no interface to use with read(2)
    // family functions.
    std::array<uint8_t, 16_k> buf;

    ssize_t nread;
    while ((nread = pread(config->data_fd, buf.data(), buf.size(),
                          req_stat->data_offset)) == -1 &&
           errno == EINTR)
      ;

    if (nread == -1) {
      return -1;
    }

    req_stat->data_offset += nread;

    wb.append(buf.data(), nread);

    if (client_->worker->config->verbose) {
      std::cout << "[send " << nread << " byte(s)]" << std::endl;
    }

    if (req_stat->data_offset == config->data_length) {
      // increment for next request
      stream_req_counter_ += 2;

      if (stream_resp_counter_ == stream_req_counter_) {
        // Response has already been received
        client_->on_stream_close(stream_resp_counter_ - 2, true,
                                 client_->final);
      }
    }
  }

  return 0;
}

void Http1Session::terminate() { complete_ = true; }

Client *Http1Session::get_client() { return client_; }

size_t Http1Session::max_concurrent_streams() {
  auto config = client_->worker->config;

  return config->data_fd == -1 ? config->max_concurrent_streams : 1;
}

} // namespace h2load
