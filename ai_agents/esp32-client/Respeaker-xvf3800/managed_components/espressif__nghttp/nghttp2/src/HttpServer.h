/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
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
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "nghttp2_config.h"

#include <sys/types.h>

#include <cinttypes>
#include <cstdlib>

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ev.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#include "http2.h"
#include "buffer.h"
#include "template.h"
#include "allocator.h"

namespace nghttp2 {

struct Config {
  std::map<std::string, std::vector<std::string>> push;
  std::map<std::string, std::string> mime_types;
  Headers trailer;
  std::string trailer_names;
  std::string htdocs;
  std::string host;
  std::string private_key_file;
  std::string cert_file;
  std::string dh_param_file;
  std::string address;
  std::string mime_types_file;
  ev_tstamp stream_read_timeout;
  ev_tstamp stream_write_timeout;
  void *data_ptr;
  size_t padding;
  size_t num_worker;
  size_t max_concurrent_streams;
  ssize_t header_table_size;
  ssize_t encoder_header_table_size;
  int window_bits;
  int connection_window_bits;
  uint16_t port;
  bool verbose;
  bool daemon;
  bool verify_client;
  bool no_tls;
  bool error_gzip;
  bool early_response;
  bool hexdump;
  bool echo_upload;
  bool no_content_length;
  bool ktls;
  Config();
  ~Config();
};

class Http2Handler;

struct FileEntry {
  FileEntry(std::string path, int64_t length, int64_t mtime, int fd,
            const std::string *content_type,
            const std::chrono::steady_clock::time_point &last_valid,
            bool stale = false)
    : path(std::move(path)),
      length(length),
      mtime(mtime),
      last_valid(last_valid),
      content_type(content_type),
      dlnext(nullptr),
      dlprev(nullptr),
      fd(fd),
      usecount(1),
      stale(stale) {}
  std::string path;
  std::multimap<std::string, std::unique_ptr<FileEntry>>::iterator it;
  int64_t length;
  int64_t mtime;
  std::chrono::steady_clock::time_point last_valid;
  const std::string *content_type;
  FileEntry *dlnext, *dlprev;
  int fd;
  int usecount;
  bool stale;
};

struct RequestHeader {
  StringRef method;
  StringRef scheme;
  StringRef authority;
  StringRef host;
  StringRef path;
  StringRef ims;
  StringRef expect;

  struct {
    nghttp2_rcbuf *method;
    nghttp2_rcbuf *scheme;
    nghttp2_rcbuf *authority;
    nghttp2_rcbuf *host;
    nghttp2_rcbuf *path;
    nghttp2_rcbuf *ims;
    nghttp2_rcbuf *expect;
  } rcbuf;
};

struct Stream {
  BlockAllocator balloc;
  RequestHeader header;
  Http2Handler *handler;
  FileEntry *file_ent;
  ev_timer rtimer;
  ev_timer wtimer;
  int64_t body_length;
  int64_t body_offset;
  // Total amount of bytes (sum of name and value length) used in
  // headers.
  size_t header_buffer_size;
  int32_t stream_id;
  bool echo_upload;
  Stream(Http2Handler *handler, int32_t stream_id);
  ~Stream();
};

class Sessions;

class Http2Handler {
public:
  Http2Handler(Sessions *sessions, int fd, SSL *ssl, int64_t session_id);
  ~Http2Handler();

  void remove_self();
  void start_settings_timer();
  int on_read();
  int on_write();
  int connection_made();
  int verify_alpn_result();

  int submit_file_response(const StringRef &status, Stream *stream,
                           time_t last_modified, off_t file_length,
                           const std::string *content_type,
                           nghttp2_data_provider2 *data_prd);

  int submit_response(const StringRef &status, int32_t stream_id,
                      nghttp2_data_provider2 *data_prd);

  int submit_response(const StringRef &status, int32_t stream_id,
                      const HeaderRefs &headers,
                      nghttp2_data_provider2 *data_prd);

  int submit_non_final_response(const std::string &status, int32_t stream_id);

  int submit_push_promise(Stream *stream, const StringRef &push_path);

  int submit_rst_stream(Stream *stream, uint32_t error_code);

  void add_stream(int32_t stream_id, std::unique_ptr<Stream> stream);
  void remove_stream(int32_t stream_id);
  Stream *get_stream(int32_t stream_id);
  int64_t session_id() const;
  Sessions *get_sessions() const;
  const Config *get_config() const;
  void remove_settings_timer();
  void terminate_session(uint32_t error_code);

  int fill_wb();

  int read_clear();
  int write_clear();
  int tls_handshake();
  int read_tls();
  int write_tls();

  struct ev_loop *get_loop() const;

  using WriteBuf = Buffer<64_k>;

  WriteBuf *get_wb();

private:
  ev_io wev_;
  ev_io rev_;
  ev_timer settings_timerev_;
  std::map<int32_t, std::unique_ptr<Stream>> id2stream_;
  WriteBuf wb_;
  std::function<int(Http2Handler &)> read_, write_;
  int64_t session_id_;
  nghttp2_session *session_;
  Sessions *sessions_;
  SSL *ssl_;
  const uint8_t *data_pending_;
  size_t data_pendinglen_;
  int fd_;
};

struct StatusPage {
  std::string status;
  FileEntry file_ent;
};

class HttpServer {
public:
  HttpServer(const Config *config);
  int listen();
  int run();
  const Config *get_config() const;
  const StatusPage *get_status_page(int status) const;

private:
  std::vector<StatusPage> status_pages_;
  const Config *config_;
};

nghttp2_ssize file_read_callback(nghttp2_session *session, int32_t stream_id,
                                 uint8_t *buf, size_t length,
                                 uint32_t *data_flags,
                                 nghttp2_data_source *source, void *user_data);

} // namespace nghttp2

#endif // HTTP_SERVER_H
