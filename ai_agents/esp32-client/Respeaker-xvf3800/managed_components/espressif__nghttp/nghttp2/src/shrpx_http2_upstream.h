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
#ifndef SHRPX_HTTP2_UPSTREAM_H
#define SHRPX_HTTP2_UPSTREAM_H

#include "shrpx.h"

#include <memory>

#include <ev.h>

#include <nghttp2/nghttp2.h>

#include "shrpx_upstream.h"
#include "shrpx_downstream_queue.h"
#include "memchunk.h"
#include "buffer.h"

using namespace nghttp2;

namespace shrpx {

class ClientHandler;
class HttpsUpstream;

class Http2Upstream : public Upstream {
public:
  Http2Upstream(ClientHandler *handler);
  virtual ~Http2Upstream();
  virtual int on_read();
  virtual int on_write();
  virtual int on_timeout(Downstream *downstream);
  virtual int on_downstream_abort_request(Downstream *downstream,
                                          unsigned int status_code);
  virtual int
  on_downstream_abort_request_with_https_redirect(Downstream *downstream);
  virtual ClientHandler *get_client_handler() const;

  virtual int downstream_read(DownstreamConnection *dconn);
  virtual int downstream_write(DownstreamConnection *dconn);
  virtual int downstream_eof(DownstreamConnection *dconn);
  virtual int downstream_error(DownstreamConnection *dconn, int events);

  void add_pending_downstream(std::unique_ptr<Downstream> downstream);
  void remove_downstream(Downstream *downstream);

  int rst_stream(Downstream *downstream, uint32_t error_code);
  int terminate_session(uint32_t error_code);
  int error_reply(Downstream *downstream, unsigned int status_code);

  virtual void pause_read(IOCtrlReason reason);
  virtual int resume_read(IOCtrlReason reason, Downstream *downstream,
                          size_t consumed);

  virtual int on_downstream_header_complete(Downstream *downstream);
  virtual int on_downstream_body(Downstream *downstream, const uint8_t *data,
                                 size_t len, bool flush);
  virtual int on_downstream_body_complete(Downstream *downstream);

  virtual void on_handler_delete();
  virtual int on_downstream_reset(Downstream *downstream, bool no_retry);
  virtual int send_reply(Downstream *downstream, const uint8_t *body,
                         size_t bodylen);
  virtual int initiate_push(Downstream *downstream, const StringRef &uri);
  virtual int response_riovec(struct iovec *iov, int iovcnt) const;
  virtual void response_drain(size_t n);
  virtual bool response_empty() const;

  virtual Downstream *on_downstream_push_promise(Downstream *downstream,
                                                 int32_t promised_stream_id);
  virtual int
  on_downstream_push_promise_complete(Downstream *downstream,
                                      Downstream *promised_downstream);
  virtual bool push_enabled() const;
  virtual void cancel_premature_downstream(Downstream *promised_downstream);

  bool get_flow_control() const;
  // Perform HTTP/2 upgrade from |upstream|. On success, this object
  // takes ownership of the |upstream|. This function returns 0 if it
  // succeeds, or -1.
  int upgrade_upstream(HttpsUpstream *upstream);
  void start_settings_timer();
  void stop_settings_timer();
  int consume(int32_t stream_id, size_t len);
  void log_response_headers(Downstream *downstream,
                            const std::vector<nghttp2_nv> &nva) const;
  void start_downstream(Downstream *downstream);
  void initiate_downstream(Downstream *downstream);

  void submit_goaway();
  void check_shutdown();
  // Starts graceful shutdown period.
  void start_graceful_shutdown();

  int prepare_push_promise(Downstream *downstream);
  int submit_push_promise(const StringRef &scheme, const StringRef &authority,
                          const StringRef &path, Downstream *downstream);

  // Called when new request has started.
  void on_start_request(const nghttp2_frame *frame);
  int on_request_headers(Downstream *downstream, const nghttp2_frame *frame);

  DefaultMemchunks *get_response_buf();

  size_t get_max_buffer_size() const;

  int redirect_to_https(Downstream *downstream);

private:
  DefaultMemchunks wb_;
  std::unique_ptr<HttpsUpstream> pre_upstream_;
  DownstreamQueue downstream_queue_;
  ev_timer settings_timer_;
  ev_timer shutdown_timer_;
  ev_prepare prep_;
  ClientHandler *handler_;
  nghttp2_session *session_;
  size_t max_buffer_size_;
  // The number of requests seen so far.
  size_t num_requests_;
  bool flow_control_;
};

nghttp2_session_callbacks *create_http2_upstream_callbacks();

} // namespace shrpx

#endif // SHRPX_HTTP2_UPSTREAM_H
