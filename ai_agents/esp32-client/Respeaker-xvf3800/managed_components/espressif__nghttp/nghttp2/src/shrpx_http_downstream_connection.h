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
#ifndef SHRPX_HTTP_DOWNSTREAM_CONNECTION_H
#define SHRPX_HTTP_DOWNSTREAM_CONNECTION_H

#include "shrpx.h"

#include "llhttp.h"

#include "shrpx_downstream_connection.h"
#include "shrpx_io_control.h"
#include "shrpx_connection.h"

namespace shrpx {

class DownstreamConnectionPool;
class Worker;
struct DownstreamAddrGroup;
struct DownstreamAddr;
struct DNSQuery;

class HttpDownstreamConnection : public DownstreamConnection {
public:
  HttpDownstreamConnection(const std::shared_ptr<DownstreamAddrGroup> &group,
                           DownstreamAddr *addr, struct ev_loop *loop,
                           Worker *worker);
  virtual ~HttpDownstreamConnection();
  virtual int attach_downstream(Downstream *downstream);
  virtual void detach_downstream(Downstream *downstream);

  virtual int push_request_headers();
  virtual int push_upload_data_chunk(const uint8_t *data, size_t datalen);
  virtual int end_upload_data();
  void end_upload_data_chunk();

  virtual void pause_read(IOCtrlReason reason);
  virtual int resume_read(IOCtrlReason reason, size_t consumed);
  virtual void force_resume_read();

  virtual int on_read();
  virtual int on_write();

  virtual void on_upstream_change(Upstream *upstream);

  virtual bool poolable() const;

  virtual const std::shared_ptr<DownstreamAddrGroup> &
  get_downstream_addr_group() const;
  virtual DownstreamAddr *get_addr() const;

  int initiate_connection();

  int write_first();
  int read_clear();
  int write_clear();
  int read_tls();
  int write_tls();

  int process_input(const uint8_t *data, size_t datalen);
  int tls_handshake();

  int connected();
  void signal_write();
  int actual_signal_write();

  // Returns address used to connect to backend.  Could be nullptr.
  const Address *get_raddr() const;

  int noop();

  int process_blocked_request_buf();

private:
  Connection conn_;
  std::function<int(HttpDownstreamConnection &)> on_read_, on_write_,
    signal_write_;
  Worker *worker_;
  // nullptr if TLS is not used.
  SSL_CTX *ssl_ctx_;
  std::shared_ptr<DownstreamAddrGroup> group_;
  // Address of remote endpoint
  DownstreamAddr *addr_;
  // Actual remote address used to contact backend.  This is initially
  // nullptr, and may point to either &addr_->addr, or
  // resolved_addr_.get().
  const Address *raddr_;
  // Resolved IP address if dns parameter is used
  std::unique_ptr<Address> resolved_addr_;
  std::unique_ptr<DNSQuery> dns_query_;
  IOControl ioctrl_;
  llhttp_t response_htp_;
  // true if first write succeeded.
  bool first_write_done_;
  // true if this object can be reused
  bool reusable_;
  // true if request header is written to request buffer.
  bool request_header_written_;
};

} // namespace shrpx

#endif // SHRPX_HTTP_DOWNSTREAM_CONNECTION_H
