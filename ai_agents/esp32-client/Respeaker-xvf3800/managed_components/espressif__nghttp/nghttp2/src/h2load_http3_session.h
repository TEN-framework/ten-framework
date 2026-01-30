/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2019 nghttp2 contributors
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
#ifndef H2LOAD_HTTP3_SESSION_H
#define H2LOAD_HTTP3_SESSION_H

#include "h2load_session.h"

#include <nghttp3/nghttp3.h>

namespace h2load {

struct Client;

class Http3Session : public Session {
public:
  Http3Session(Client *client);
  virtual ~Http3Session();
  virtual void on_connect();
  virtual int submit_request();
  virtual int on_read(const uint8_t *data, size_t len);
  virtual int on_write();
  virtual void terminate();
  virtual size_t max_concurrent_streams();

  int init_conn();
  int stream_close(int64_t stream_id, uint64_t app_error_code);
  int end_stream(int64_t stream_id);
  void recv_data(int64_t stream_id, const uint8_t *data, size_t datalen);
  void consume(int64_t stream_id, size_t nconsumed);
  void begin_headers(int64_t stream_id);
  void recv_header(int64_t stream_id, const nghttp3_vec *name,
                   const nghttp3_vec *value);
  int stop_sending(int64_t stream_id, uint64_t app_error_code);
  int reset_stream(int64_t stream_id, uint64_t app_error_code);

  int close_stream(int64_t stream_id, uint64_t app_error_code);
  int shutdown_stream_read(int64_t stream_id);
  int extend_max_local_streams();
  int64_t submit_request_internal();

  ssize_t read_stream(uint32_t flags, int64_t stream_id, const uint8_t *data,
                      size_t datalen);
  ssize_t write_stream(int64_t &stream_id, int &fin, nghttp3_vec *vec,
                       size_t veccnt);
  void block_stream(int64_t stream_id);
  int unblock_stream(int64_t stream_id);
  void shutdown_stream_write(int64_t stream_id);
  int add_write_offset(int64_t stream_id, size_t ndatalen);
  int add_ack_offset(int64_t stream_id, size_t datalen);

  void read_data(nghttp3_vec *vec, size_t veccnt, uint32_t *pflags);

private:
  Client *client_;
  nghttp3_conn *conn_;
  size_t npending_request_;
  size_t reqidx_;
};

} // namespace h2load

#endif // H2LOAD_HTTP3_SESSION_H
