/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2021 Tatsuhiro Tsujikawa
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
#include "shrpx_null_downstream_connection.h"
#include "shrpx_upstream.h"
#include "shrpx_downstream.h"
#include "shrpx_log.h"

namespace shrpx {

NullDownstreamConnection::NullDownstreamConnection(
  const std::shared_ptr<DownstreamAddrGroup> &group)
  : group_(group) {}

NullDownstreamConnection::~NullDownstreamConnection() {}

int NullDownstreamConnection::attach_downstream(Downstream *downstream) {
  if (LOG_ENABLED(INFO)) {
    DCLOG(INFO, this) << "Attaching to DOWNSTREAM:" << downstream;
  }

  downstream_ = downstream;

  return 0;
}

void NullDownstreamConnection::detach_downstream(Downstream *downstream) {
  if (LOG_ENABLED(INFO)) {
    DCLOG(INFO, this) << "Detaching from DOWNSTREAM:" << downstream;
  }
  downstream_ = nullptr;
}

int NullDownstreamConnection::push_request_headers() { return 0; }

int NullDownstreamConnection::push_upload_data_chunk(const uint8_t *data,
                                                     size_t datalen) {
  return 0;
}

int NullDownstreamConnection::end_upload_data() { return 0; }

void NullDownstreamConnection::pause_read(IOCtrlReason reason) {}

int NullDownstreamConnection::resume_read(IOCtrlReason reason,
                                          size_t consumed) {
  return 0;
}

void NullDownstreamConnection::force_resume_read() {}

int NullDownstreamConnection::on_read() { return 0; }

int NullDownstreamConnection::on_write() { return 0; }

void NullDownstreamConnection::on_upstream_change(Upstream *upstream) {}

bool NullDownstreamConnection::poolable() const { return false; }

const std::shared_ptr<DownstreamAddrGroup> &
NullDownstreamConnection::get_downstream_addr_group() const {
  return group_;
}

DownstreamAddr *NullDownstreamConnection::get_addr() const { return nullptr; }

} // namespace shrpx
