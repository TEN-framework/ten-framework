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
#ifndef SHRPX_API_DOWNSTREAM_CONNECTION_H
#define SHRPX_API_DOWNSTREAM_CONNECTION_H

#include "shrpx_downstream_connection.h"
#include "template.h"

using namespace nghttp2;

namespace shrpx {

class Worker;

// If new method is added, don't forget to update API_METHOD_STRING as
// well.
enum APIMethod {
  API_METHOD_GET,
  API_METHOD_POST,
  API_METHOD_PUT,
  API_METHOD_MAX,
};

// API status code, which is independent from HTTP status code.  But
// generally, 2xx code for SUCCESS, and otherwise FAILURE.
enum class APIStatusCode {
  SUCCESS,
  FAILURE,
};

class APIDownstreamConnection;

struct APIEndpoint {
  // Endpoint path.  It must start with "/api/".
  StringRef path;
  // true if we evaluate request body.
  bool require_body;
  // Allowed methods.  This is bitwise OR of one or more of (1 <<
  // APIMethod value).
  uint8_t allowed_methods;
  std::function<int(APIDownstreamConnection &)> handler;
};

class APIDownstreamConnection : public DownstreamConnection {
public:
  APIDownstreamConnection(Worker *worker);
  virtual ~APIDownstreamConnection();
  virtual int attach_downstream(Downstream *downstream);
  virtual void detach_downstream(Downstream *downstream);

  virtual int push_request_headers();
  virtual int push_upload_data_chunk(const uint8_t *data, size_t datalen);
  virtual int end_upload_data();

  virtual void pause_read(IOCtrlReason reason);
  virtual int resume_read(IOCtrlReason reason, size_t consumed);
  virtual void force_resume_read();

  virtual int on_read();
  virtual int on_write();

  virtual void on_upstream_change(Upstream *upstream);

  // true if this object is poolable.
  virtual bool poolable() const;

  virtual const std::shared_ptr<DownstreamAddrGroup> &
  get_downstream_addr_group() const;
  virtual DownstreamAddr *get_addr() const;

  int send_reply(unsigned int http_status, APIStatusCode api_status,
                 const StringRef &data = StringRef{});
  int error_method_not_allowed();

  // Handles backendconfig API request.
  int handle_backendconfig();
  // Handles configrevision API request.
  int handle_configrevision();

private:
  Worker *worker_;
  // This points to the requested APIEndpoint struct.
  const APIEndpoint *api_;
  // The file descriptor for temporary file to store request body.
  int fd_;
  // true if we stop reading request body.
  bool shutdown_read_;
};

} // namespace shrpx

#endif // SHRPX_API_DOWNSTREAM_CONNECTION_H
