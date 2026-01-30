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
#ifndef SHRPX_UPSTREAM_H
#define SHRPX_UPSTREAM_H

#include "shrpx.h"
#include "shrpx_io_control.h"
#include "memchunk.h"

using namespace nghttp2;

namespace shrpx {

class ClientHandler;
class Downstream;
class DownstreamConnection;

class Upstream {
public:
  virtual ~Upstream() {}
  virtual int on_read() = 0;
  virtual int on_write() = 0;
  virtual int on_timeout(Downstream *downstream) { return 0; }
  virtual int on_downstream_abort_request(Downstream *downstream,
                                          unsigned int status_code) = 0;
  // Called when the current request is aborted without forwarding it
  // to backend, and it should be redirected to https URI.
  virtual int
  on_downstream_abort_request_with_https_redirect(Downstream *downstream) = 0;
  virtual int downstream_read(DownstreamConnection *dconn) = 0;
  virtual int downstream_write(DownstreamConnection *dconn) = 0;
  virtual int downstream_eof(DownstreamConnection *dconn) = 0;
  virtual int downstream_error(DownstreamConnection *dconn, int events) = 0;
  virtual ClientHandler *get_client_handler() const = 0;

  virtual int on_downstream_header_complete(Downstream *downstream) = 0;
  virtual int on_downstream_body(Downstream *downstream, const uint8_t *data,
                                 size_t len, bool flush) = 0;
  virtual int on_downstream_body_complete(Downstream *downstream) = 0;

  virtual void on_handler_delete() = 0;
  // Called when downstream connection for |downstream| is reset.
  // Currently this is only used by Http2Session.  If |no_retry| is
  // true, another connection attempt using new DownstreamConnection
  // is not allowed.
  virtual int on_downstream_reset(Downstream *downstream, bool no_retry) = 0;

  virtual void pause_read(IOCtrlReason reason) = 0;
  virtual int resume_read(IOCtrlReason reason, Downstream *downstream,
                          size_t consumed) = 0;
  virtual int send_reply(Downstream *downstream, const uint8_t *body,
                         size_t bodylen) = 0;

  // Starts server push.  The |downstream| is an associated stream for
  // the pushed resource.  This function returns 0 if it succeeds,
  // otherwise -1.
  virtual int initiate_push(Downstream *downstream, const StringRef &uri) = 0;

  // Fills response data in |iov| whose capacity is |iovcnt|.  Returns
  // the number of iovs filled.
  virtual int response_riovec(struct iovec *iov, int iovcnt) const = 0;
  virtual void response_drain(size_t n) = 0;
  virtual bool response_empty() const = 0;

  // Called when PUSH_PROMISE was started in downstream.  The
  // associated downstream is given as |downstream|.  The promised
  // stream ID is given as |promised_stream_id|.  If upstream supports
  // server push for the corresponding upstream connection, it should
  // return Downstream object for pushed stream.  Otherwise, returns
  // nullptr.
  virtual Downstream *
  on_downstream_push_promise(Downstream *downstream,
                             int32_t promised_stream_id) = 0;
  // Called when PUSH_PROMISE frame was completely received in
  // downstream.  The associated downstream is given as |downstream|.
  // This function returns 0 if it succeeds, or -1.
  virtual int
  on_downstream_push_promise_complete(Downstream *downstream,
                                      Downstream *promised_downstream) = 0;
  // Returns true if server push is enabled in upstream connection.
  virtual bool push_enabled() const = 0;
  // Cancels promised downstream.  This function is called when
  // PUSH_PROMISE for |promised_downstream| is not submitted to
  // upstream session.
  virtual void cancel_premature_downstream(Downstream *promised_downstream) = 0;
};

} // namespace shrpx

#endif // SHRPX_UPSTREAM_H
