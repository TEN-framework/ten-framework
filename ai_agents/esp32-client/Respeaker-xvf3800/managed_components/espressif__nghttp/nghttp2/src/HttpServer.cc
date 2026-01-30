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
#include "HttpServer.h"

#include <sys/stat.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif // HAVE_FCNTL_H
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif // HAVE_NETINET_IN_H
#include <netinet/tcp.h>
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif // HAVE_ARPA_INET_H

#include <cassert>
#include <set>
#include <iostream>
#include <thread>
#include <mutex>
#include <deque>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/err.h>
#  include <wolfssl/openssl/dh.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/err.h>
#  include <openssl/dh.h>
#  if OPENSSL_3_0_0_API
#    include <openssl/decoder.h>
#  endif // OPENSSL_3_0_0_API
#endif   // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <zlib.h>

#include "app_helper.h"
#include "http2.h"
#include "util.h"
#include "tls.h"
#include "template.h"

#ifndef O_BINARY
#  define O_BINARY (0)
#endif // O_BINARY

using namespace std::chrono_literals;

namespace nghttp2 {

namespace {
// TODO could be constexpr
constexpr auto DEFAULT_HTML = "index.html"_sr;
constexpr auto NGHTTPD_SERVER = "nghttpd nghttp2/" NGHTTP2_VERSION ""_sr;
} // namespace

namespace {
void delete_handler(Http2Handler *handler) {
  handler->remove_self();
  delete handler;
}
} // namespace

namespace {
void print_session_id(int64_t id) { std::cout << "[id=" << id << "] "; }
} // namespace

Config::Config()
  : mime_types_file("/etc/mime.types"),
    stream_read_timeout(1_min),
    stream_write_timeout(1_min),
    data_ptr(nullptr),
    padding(0),
    num_worker(1),
    max_concurrent_streams(100),
    header_table_size(-1),
    encoder_header_table_size(-1),
    window_bits(-1),
    connection_window_bits(-1),
    port(0),
    verbose(false),
    daemon(false),
    verify_client(false),
    no_tls(false),
    error_gzip(false),
    early_response(false),
    hexdump(false),
    echo_upload(false),
    no_content_length(false),
    ktls(false) {}

Config::~Config() {}

namespace {
void stream_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  int rv;
  auto stream = static_cast<Stream *>(w->data);
  auto hd = stream->handler;
  auto config = hd->get_config();

  ev_timer_stop(hd->get_loop(), &stream->rtimer);
  ev_timer_stop(hd->get_loop(), &stream->wtimer);

  if (config->verbose) {
    print_session_id(hd->session_id());
    print_timer();
    std::cout << " timeout stream_id=" << stream->stream_id << std::endl;
  }

  hd->submit_rst_stream(stream, NGHTTP2_INTERNAL_ERROR);

  rv = hd->on_write();
  if (rv == -1) {
    delete_handler(hd);
  }
}
} // namespace

namespace {
void add_stream_read_timeout(Stream *stream) {
  auto hd = stream->handler;
  ev_timer_again(hd->get_loop(), &stream->rtimer);
}
} // namespace

namespace {
void add_stream_read_timeout_if_pending(Stream *stream) {
  auto hd = stream->handler;
  if (ev_is_active(&stream->rtimer)) {
    ev_timer_again(hd->get_loop(), &stream->rtimer);
  }
}
} // namespace

namespace {
void add_stream_write_timeout(Stream *stream) {
  auto hd = stream->handler;
  ev_timer_again(hd->get_loop(), &stream->wtimer);
}
} // namespace

namespace {
void remove_stream_read_timeout(Stream *stream) {
  auto hd = stream->handler;
  ev_timer_stop(hd->get_loop(), &stream->rtimer);
}
} // namespace

namespace {
void remove_stream_write_timeout(Stream *stream) {
  auto hd = stream->handler;
  ev_timer_stop(hd->get_loop(), &stream->wtimer);
}
} // namespace

namespace {
void fill_callback(nghttp2_session_callbacks *callbacks, const Config *config);
} // namespace

namespace {
constexpr ev_tstamp RELEASE_FD_TIMEOUT = 2.;
} // namespace

namespace {
void release_fd_cb(struct ev_loop *loop, ev_timer *w, int revents);
} // namespace

namespace {
constexpr auto FILE_ENTRY_MAX_AGE = 10s;
} // namespace

namespace {
constexpr size_t FILE_ENTRY_EVICT_THRES = 2048;
} // namespace

namespace {
bool need_validation_file_entry(
  const FileEntry *ent, const std::chrono::steady_clock::time_point &now) {
  return ent->last_valid + FILE_ENTRY_MAX_AGE < now;
}
} // namespace

namespace {
bool validate_file_entry(FileEntry *ent,
                         const std::chrono::steady_clock::time_point &now) {
  struct stat stbuf;
  int rv;

  rv = fstat(ent->fd, &stbuf);
  if (rv != 0) {
    ent->stale = true;
    return false;
  }

  if (stbuf.st_nlink == 0 || ent->mtime != stbuf.st_mtime) {
    ent->stale = true;
    return false;
  }

  ent->mtime = stbuf.st_mtime;
  ent->last_valid = now;

  return true;
}
} // namespace

class Sessions {
public:
  Sessions(HttpServer *sv, struct ev_loop *loop, const Config *config,
           SSL_CTX *ssl_ctx)
    : sv_(sv),
      loop_(loop),
      config_(config),
      ssl_ctx_(ssl_ctx),
      callbacks_(nullptr),
      option_(nullptr),
      next_session_id_(1),
      tstamp_cached_(ev_now(loop)),
      cached_date_(util::http_date(tstamp_cached_)) {
    nghttp2_session_callbacks_new(&callbacks_);

    fill_callback(callbacks_, config_);

    nghttp2_option_new(&option_);

    if (config_->encoder_header_table_size != -1) {
      nghttp2_option_set_max_deflate_dynamic_table_size(
        option_, config_->encoder_header_table_size);
    }

    ev_timer_init(&release_fd_timer_, release_fd_cb, 0., RELEASE_FD_TIMEOUT);
    release_fd_timer_.data = this;
  }
  ~Sessions() {
    ev_timer_stop(loop_, &release_fd_timer_);
    for (auto handler : handlers_) {
      delete handler;
    }
    nghttp2_option_del(option_);
    nghttp2_session_callbacks_del(callbacks_);
  }
  void add_handler(Http2Handler *handler) { handlers_.insert(handler); }
  void remove_handler(Http2Handler *handler) {
    handlers_.erase(handler);
    if (handlers_.empty() && !fd_cache_.empty()) {
      ev_timer_again(loop_, &release_fd_timer_);
    }
  }
  SSL_CTX *get_ssl_ctx() const { return ssl_ctx_; }
  SSL *ssl_session_new(int fd) {
    SSL *ssl = SSL_new(ssl_ctx_);
    if (!ssl) {
      std::cerr << "SSL_new() failed" << std::endl;
      return nullptr;
    }
    if (SSL_set_fd(ssl, fd) == 0) {
      std::cerr << "SSL_set_fd() failed" << std::endl;
      SSL_free(ssl);
      return nullptr;
    }
    return ssl;
  }
  const Config *get_config() const { return config_; }
  struct ev_loop *get_loop() const { return loop_; }
  int64_t get_next_session_id() {
    auto session_id = next_session_id_;
    if (next_session_id_ == std::numeric_limits<int64_t>::max()) {
      next_session_id_ = 1;
    } else {
      ++next_session_id_;
    }
    return session_id;
  }
  const nghttp2_session_callbacks *get_callbacks() const { return callbacks_; }
  const nghttp2_option *get_option() const { return option_; }
  void accept_connection(int fd) {
    util::make_socket_nodelay(fd);
    SSL *ssl = nullptr;
    if (ssl_ctx_) {
      ssl = ssl_session_new(fd);
      if (!ssl) {
        close(fd);
        return;
      }
    }
    auto handler =
      std::make_unique<Http2Handler>(this, fd, ssl, get_next_session_id());
    if (!ssl) {
      if (handler->connection_made() != 0) {
        return;
      }
    }
    add_handler(handler.release());
  }
  void update_cached_date() { cached_date_ = util::http_date(tstamp_cached_); }
  const std::string &get_cached_date() {
    auto t = ev_now(loop_);
    if (t != tstamp_cached_) {
      tstamp_cached_ = t;
      update_cached_date();
    }
    return cached_date_;
  }
  FileEntry *get_cached_fd(const std::string &path) {
    auto range = fd_cache_.equal_range(path);
    if (range.first == range.second) {
      return nullptr;
    }

    auto now = std::chrono::steady_clock::now();

    for (auto it = range.first; it != range.second;) {
      auto &ent = (*it).second;
      if (ent->stale) {
        ++it;
        continue;
      }
      if (need_validation_file_entry(ent.get(), now) &&
          !validate_file_entry(ent.get(), now)) {
        if (ent->usecount == 0) {
          fd_cache_lru_.remove(ent.get());
          close(ent->fd);
          it = fd_cache_.erase(it);
          continue;
        }
        ++it;
        continue;
      }

      fd_cache_lru_.remove(ent.get());
      fd_cache_lru_.append(ent.get());

      ++ent->usecount;
      return ent.get();
    }
    return nullptr;
  }
  FileEntry *cache_fd(const std::string &path, const FileEntry &ent) {
#ifdef HAVE_STD_MAP_EMPLACE
    auto rv = fd_cache_.emplace(path, std::make_unique<FileEntry>(ent));
#else  // !HAVE_STD_MAP_EMPLACE
    // for gcc-4.7
    auto rv =
      fd_cache_.insert(std::make_pair(path, std::make_unique<FileEntry>(ent)));
#endif // !HAVE_STD_MAP_EMPLACE
    auto &res = (*rv).second;
    res->it = rv;
    fd_cache_lru_.append(res.get());

    while (fd_cache_.size() > FILE_ENTRY_EVICT_THRES) {
      auto ent = fd_cache_lru_.head;
      if (ent->usecount) {
        break;
      }
      fd_cache_lru_.remove(ent);
      close(ent->fd);
      fd_cache_.erase(ent->it);
    }

    return res.get();
  }
  void release_fd(FileEntry *target) {
    --target->usecount;

    if (target->usecount == 0 && target->stale) {
      fd_cache_lru_.remove(target);
      close(target->fd);
      fd_cache_.erase(target->it);
      return;
    }

    // We use timer to close file descriptor and delete the entry from
    // cache.  The timer will be started when there is no handler.
  }
  void release_unused_fd() {
    for (auto i = std::begin(fd_cache_); i != std::end(fd_cache_);) {
      auto &ent = (*i).second;
      if (ent->usecount != 0) {
        ++i;
        continue;
      }

      fd_cache_lru_.remove(ent.get());
      close(ent->fd);
      i = fd_cache_.erase(i);
    }
  }
  const HttpServer *get_server() const { return sv_; }
  bool handlers_empty() const { return handlers_.empty(); }

private:
  std::set<Http2Handler *> handlers_;
  // cache for file descriptors to read file.
  std::multimap<std::string, std::unique_ptr<FileEntry>> fd_cache_;
  DList<FileEntry> fd_cache_lru_;
  HttpServer *sv_;
  struct ev_loop *loop_;
  const Config *config_;
  SSL_CTX *ssl_ctx_;
  nghttp2_session_callbacks *callbacks_;
  nghttp2_option *option_;
  ev_timer release_fd_timer_;
  int64_t next_session_id_;
  ev_tstamp tstamp_cached_;
  std::string cached_date_;
};

namespace {
void release_fd_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto sessions = static_cast<Sessions *>(w->data);

  ev_timer_stop(loop, w);

  if (!sessions->handlers_empty()) {
    return;
  }

  sessions->release_unused_fd();
}
} // namespace

Stream::Stream(Http2Handler *handler, int32_t stream_id)
  : balloc(1024, 1024),
    header{},
    handler(handler),
    file_ent(nullptr),
    body_length(0),
    body_offset(0),
    header_buffer_size(0),
    stream_id(stream_id),
    echo_upload(false) {
  auto config = handler->get_config();
  ev_timer_init(&rtimer, stream_timeout_cb, 0., config->stream_read_timeout);
  ev_timer_init(&wtimer, stream_timeout_cb, 0., config->stream_write_timeout);
  rtimer.data = this;
  wtimer.data = this;
}

Stream::~Stream() {
  if (file_ent != nullptr) {
    auto sessions = handler->get_sessions();
    sessions->release_fd(file_ent);
  }

  auto &rcbuf = header.rcbuf;
  nghttp2_rcbuf_decref(rcbuf.method);
  nghttp2_rcbuf_decref(rcbuf.scheme);
  nghttp2_rcbuf_decref(rcbuf.authority);
  nghttp2_rcbuf_decref(rcbuf.host);
  nghttp2_rcbuf_decref(rcbuf.path);
  nghttp2_rcbuf_decref(rcbuf.ims);
  nghttp2_rcbuf_decref(rcbuf.expect);

  auto loop = handler->get_loop();
  ev_timer_stop(loop, &rtimer);
  ev_timer_stop(loop, &wtimer);
}

namespace {
void on_session_closed(Http2Handler *hd, int64_t session_id) {
  if (hd->get_config()->verbose) {
    print_session_id(session_id);
    print_timer();
    std::cout << " closed" << std::endl;
  }
}
} // namespace

namespace {
void settings_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  int rv;
  auto hd = static_cast<Http2Handler *>(w->data);
  hd->terminate_session(NGHTTP2_SETTINGS_TIMEOUT);
  rv = hd->on_write();
  if (rv == -1) {
    delete_handler(hd);
  }
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  int rv;
  auto handler = static_cast<Http2Handler *>(w->data);

  rv = handler->on_read();
  if (rv == -1) {
    delete_handler(handler);
  }
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  int rv;
  auto handler = static_cast<Http2Handler *>(w->data);

  rv = handler->on_write();
  if (rv == -1) {
    delete_handler(handler);
  }
}
} // namespace

Http2Handler::Http2Handler(Sessions *sessions, int fd, SSL *ssl,
                           int64_t session_id)
  : session_id_(session_id),
    session_(nullptr),
    sessions_(sessions),
    ssl_(ssl),
    data_pending_(nullptr),
    data_pendinglen_(0),
    fd_(fd) {
  ev_timer_init(&settings_timerev_, settings_timeout_cb, 10., 0.);
  ev_io_init(&wev_, writecb, fd, EV_WRITE);
  ev_io_init(&rev_, readcb, fd, EV_READ);

  settings_timerev_.data = this;
  wev_.data = this;
  rev_.data = this;

  auto loop = sessions_->get_loop();
  ev_io_start(loop, &rev_);

  if (ssl) {
    SSL_set_accept_state(ssl);
    read_ = &Http2Handler::tls_handshake;
    write_ = &Http2Handler::tls_handshake;
  } else {
    read_ = &Http2Handler::read_clear;
    write_ = &Http2Handler::write_clear;
  }
}

Http2Handler::~Http2Handler() {
  on_session_closed(this, session_id_);
  nghttp2_session_del(session_);
  if (ssl_) {
    SSL_set_shutdown(ssl_, SSL_get_shutdown(ssl_) | SSL_RECEIVED_SHUTDOWN);
    ERR_clear_error();
    SSL_shutdown(ssl_);
  }
  auto loop = sessions_->get_loop();
  ev_timer_stop(loop, &settings_timerev_);
  ev_io_stop(loop, &rev_);
  ev_io_stop(loop, &wev_);
  if (ssl_) {
    SSL_free(ssl_);
  }
  shutdown(fd_, SHUT_WR);
  close(fd_);
}

void Http2Handler::remove_self() { sessions_->remove_handler(this); }

struct ev_loop *Http2Handler::get_loop() const { return sessions_->get_loop(); }

Http2Handler::WriteBuf *Http2Handler::get_wb() { return &wb_; }

void Http2Handler::start_settings_timer() {
  ev_timer_start(sessions_->get_loop(), &settings_timerev_);
}

int Http2Handler::fill_wb() {
  if (data_pending_) {
    auto n = std::min(wb_.wleft(), data_pendinglen_);
    wb_.write(data_pending_, n);
    if (n < data_pendinglen_) {
      data_pending_ += n;
      data_pendinglen_ -= n;
      return 0;
    }

    data_pending_ = nullptr;
    data_pendinglen_ = 0;
  }

  for (;;) {
    const uint8_t *data;
    auto datalen = nghttp2_session_mem_send2(session_, &data);

    if (datalen < 0) {
      std::cerr << "nghttp2_session_mem_send2() returned error: "
                << nghttp2_strerror(datalen) << std::endl;
      return -1;
    }
    if (datalen == 0) {
      break;
    }
    auto n = wb_.write(data, datalen);
    if (n < static_cast<decltype(n)>(datalen)) {
      data_pending_ = data + n;
      data_pendinglen_ = datalen - n;
      break;
    }
  }
  return 0;
}

int Http2Handler::read_clear() {
  int rv;
  std::array<uint8_t, 8_k> buf;

  ssize_t nread;
  while ((nread = read(fd_, buf.data(), buf.size())) == -1 && errno == EINTR)
    ;
  if (nread == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return write_(*this);
    }
    return -1;
  }
  if (nread == 0) {
    return -1;
  }

  if (get_config()->hexdump) {
    util::hexdump(stdout, buf.data(), nread);
  }

  rv = nghttp2_session_mem_recv2(session_, buf.data(), nread);
  if (rv < 0) {
    if (rv != NGHTTP2_ERR_BAD_CLIENT_MAGIC) {
      std::cerr << "nghttp2_session_mem_recv2() returned error: "
                << nghttp2_strerror(rv) << std::endl;
    }
    return -1;
  }

  return write_(*this);
}

int Http2Handler::write_clear() {
  auto loop = sessions_->get_loop();
  for (;;) {
    if (wb_.rleft() > 0) {
      ssize_t nwrite;
      while ((nwrite = write(fd_, wb_.pos, wb_.rleft())) == -1 &&
             errno == EINTR)
        ;
      if (nwrite == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          ev_io_start(loop, &wev_);
          return 0;
        }
        return -1;
      }
      wb_.drain(nwrite);
      continue;
    }
    wb_.reset();
    if (fill_wb() != 0) {
      return -1;
    }
    if (wb_.rleft() == 0) {
      break;
    }
  }

  if (wb_.rleft() == 0) {
    ev_io_stop(loop, &wev_);
  } else {
    ev_io_start(loop, &wev_);
  }

  if (nghttp2_session_want_read(session_) == 0 &&
      nghttp2_session_want_write(session_) == 0 && wb_.rleft() == 0) {
    return -1;
  }

  return 0;
}

int Http2Handler::tls_handshake() {
  ev_io_stop(sessions_->get_loop(), &wev_);

  ERR_clear_error();

  auto rv = SSL_do_handshake(ssl_);

  if (rv <= 0) {
    auto err = SSL_get_error(ssl_, rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
      return 0;
    case SSL_ERROR_WANT_WRITE:
      ev_io_start(sessions_->get_loop(), &wev_);
      return 0;
    default:
      return -1;
    }
  }

  if (sessions_->get_config()->verbose) {
    std::cerr << "SSL/TLS handshake completed" << std::endl;
  }

  if (verify_alpn_result() != 0) {
    return -1;
  }

  read_ = &Http2Handler::read_tls;
  write_ = &Http2Handler::write_tls;

  if (connection_made() != 0) {
    return -1;
  }

  if (sessions_->get_config()->verbose) {
    if (SSL_session_reused(ssl_)) {
      std::cerr << "SSL/TLS session reused" << std::endl;
    }
  }

  return 0;
}

int Http2Handler::read_tls() {
  std::array<uint8_t, 8_k> buf;

  ERR_clear_error();

  for (;;) {
    auto rv = SSL_read(ssl_, buf.data(), buf.size());

    if (rv <= 0) {
      auto err = SSL_get_error(ssl_, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
        return write_(*this);
      case SSL_ERROR_WANT_WRITE:
        // renegotiation started
        return -1;
      default:
        return -1;
      }
    }

    auto nread = rv;

    if (get_config()->hexdump) {
      util::hexdump(stdout, buf.data(), nread);
    }

    rv = nghttp2_session_mem_recv2(session_, buf.data(), nread);
    if (rv < 0) {
      if (rv != NGHTTP2_ERR_BAD_CLIENT_MAGIC) {
        std::cerr << "nghttp2_session_mem_recv2() returned error: "
                  << nghttp2_strerror(rv) << std::endl;
      }
      return -1;
    }

    if (SSL_pending(ssl_) == 0) {
      break;
    }
  }

  return write_(*this);
}

int Http2Handler::write_tls() {
  auto loop = sessions_->get_loop();

  ERR_clear_error();

  for (;;) {
    if (wb_.rleft() > 0) {
      auto rv = SSL_write(ssl_, wb_.pos, wb_.rleft());

      if (rv <= 0) {
        auto err = SSL_get_error(ssl_, rv);
        switch (err) {
        case SSL_ERROR_WANT_READ:
          // renegotiation started
          return -1;
        case SSL_ERROR_WANT_WRITE:
          ev_io_start(sessions_->get_loop(), &wev_);
          return 0;
        default:
          return -1;
        }
      }

      wb_.drain(rv);
      continue;
    }
    wb_.reset();
    if (fill_wb() != 0) {
      return -1;
    }
    if (wb_.rleft() == 0) {
      break;
    }
  }

  if (wb_.rleft() == 0) {
    ev_io_stop(loop, &wev_);
  } else {
    ev_io_start(loop, &wev_);
  }

  if (nghttp2_session_want_read(session_) == 0 &&
      nghttp2_session_want_write(session_) == 0 && wb_.rleft() == 0) {
    return -1;
  }

  return 0;
}

int Http2Handler::on_read() { return read_(*this); }

int Http2Handler::on_write() { return write_(*this); }

int Http2Handler::connection_made() {
  int r;

  r = nghttp2_session_server_new2(&session_, sessions_->get_callbacks(), this,
                                  sessions_->get_option());

  if (r != 0) {
    return r;
  }

  auto config = sessions_->get_config();
  std::array<nghttp2_settings_entry, 4> entry;
  size_t niv = 2;

  entry[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  entry[0].value = config->max_concurrent_streams;

  entry[1].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  entry[1].value = 1;

  if (config->header_table_size >= 0) {
    entry[niv].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
    entry[niv].value = config->header_table_size;
    ++niv;
  }

  if (config->window_bits != -1) {
    entry[niv].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
    entry[niv].value = (1 << config->window_bits) - 1;
    ++niv;
  }

  r = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, entry.data(), niv);
  if (r != 0) {
    return r;
  }

  if (config->connection_window_bits != -1) {
    r = nghttp2_session_set_local_window_size(
      session_, NGHTTP2_FLAG_NONE, 0,
      (1 << config->connection_window_bits) - 1);
    if (r != 0) {
      return r;
    }
  }

  if (ssl_ && !nghttp2::tls::check_http2_requirement(ssl_)) {
    terminate_session(NGHTTP2_INADEQUATE_SECURITY);
  }

  return on_write();
}

int Http2Handler::verify_alpn_result() {
  const unsigned char *next_proto = nullptr;
  unsigned int next_proto_len;
  // Check the negotiated protocol in ALPN
  SSL_get0_alpn_selected(ssl_, &next_proto, &next_proto_len);
  if (next_proto) {
    auto proto = StringRef{next_proto, next_proto_len};
    if (sessions_->get_config()->verbose) {
      std::cout << "The negotiated protocol: " << proto << std::endl;
    }
    if (util::check_h2_is_selected(proto)) {
      return 0;
    }
  }
  if (sessions_->get_config()->verbose) {
    std::cerr << "Client did not advertise HTTP/2 protocol."
              << " (nghttp2 expects " << NGHTTP2_PROTO_VERSION_ID << ")"
              << std::endl;
  }
  return -1;
}

int Http2Handler::submit_file_response(const StringRef &status, Stream *stream,
                                       time_t last_modified, off_t file_length,
                                       const std::string *content_type,
                                       nghttp2_data_provider2 *data_prd) {
  std::string last_modified_str;
  auto nva = std::to_array({
    http2::make_field(":status"_sr, status),
    http2::make_field("server"_sr, NGHTTPD_SERVER),
    http2::make_field("cache-control"_sr, "max-age=3600"_sr),
    http2::make_field_v("date"_sr, sessions_->get_cached_date()),
    {},
    {},
    {},
    {},
  });
  size_t nvlen = 4;
  if (!get_config()->no_content_length) {
    nva[nvlen++] = http2::make_field(
      "content-length"_sr,
      util::make_string_ref_uint(stream->balloc, file_length));
  }
  if (last_modified != 0) {
    last_modified_str = util::http_date(last_modified);
    nva[nvlen++] = http2::make_field_v("last-modified"_sr, last_modified_str);
  }
  if (content_type) {
    nva[nvlen++] = http2::make_field_v("content-type"_sr, *content_type);
  }
  auto &trailer_names = get_config()->trailer_names;
  if (!trailer_names.empty()) {
    nva[nvlen++] = http2::make_field("trailer"_sr, trailer_names);
  }
  return nghttp2_submit_response2(session_, stream->stream_id, nva.data(),
                                  nvlen, data_prd);
}

int Http2Handler::submit_response(const StringRef &status, int32_t stream_id,
                                  const HeaderRefs &headers,
                                  nghttp2_data_provider2 *data_prd) {
  auto nva = std::vector<nghttp2_nv>();
  nva.reserve(4 + headers.size());
  nva.push_back(http2::make_field(":status"_sr, status));
  nva.push_back(http2::make_field("server"_sr, NGHTTPD_SERVER));
  nva.push_back(http2::make_field_v("date"_sr, sessions_->get_cached_date()));

  if (data_prd) {
    auto &trailer_names = get_config()->trailer_names;
    if (!trailer_names.empty()) {
      nva.push_back(http2::make_field("trailer"_sr, trailer_names));
    }
  }

  for (auto &nv : headers) {
    nva.push_back(
      http2::make_field(nv.name, nv.value, http2::no_index(nv.no_index)));
  }
  int r = nghttp2_submit_response2(session_, stream_id, nva.data(), nva.size(),
                                   data_prd);
  return r;
}

int Http2Handler::submit_response(const StringRef &status, int32_t stream_id,
                                  nghttp2_data_provider2 *data_prd) {
  auto nva = std::to_array({
    http2::make_field(":status"_sr, status),
    http2::make_field("server"_sr, NGHTTPD_SERVER),
    http2::make_field_v("date"_sr, sessions_->get_cached_date()),
    {},
  });
  size_t nvlen = 3;

  if (data_prd) {
    auto &trailer_names = get_config()->trailer_names;
    if (!trailer_names.empty()) {
      nva[nvlen++] = http2::make_field("trailer"_sr, trailer_names);
    }
  }

  return nghttp2_submit_response2(session_, stream_id, nva.data(), nvlen,
                                  data_prd);
}

int Http2Handler::submit_non_final_response(const std::string &status,
                                            int32_t stream_id) {
  auto nva = std::to_array({http2::make_field_v(":status"_sr, status)});
  return nghttp2_submit_headers(session_, NGHTTP2_FLAG_NONE, stream_id, nullptr,
                                nva.data(), nva.size(), nullptr);
}

int Http2Handler::submit_push_promise(Stream *stream,
                                      const StringRef &push_path) {
  auto authority = stream->header.authority;

  if (authority.empty()) {
    authority = stream->header.host;
  }

  auto scheme = get_config()->no_tls ? "http"_sr : "https"_sr;

  auto nva = std::to_array({http2::make_field(":method"_sr, "GET"_sr),
                            http2::make_field(":path"_sr, push_path),
                            http2::make_field(":scheme"_sr, scheme),
                            http2::make_field(":authority"_sr, authority)});

  auto promised_stream_id = nghttp2_submit_push_promise(
    session_, NGHTTP2_FLAG_END_HEADERS, stream->stream_id, nva.data(),
    nva.size(), nullptr);

  if (promised_stream_id < 0) {
    return promised_stream_id;
  }

  auto promised_stream = std::make_unique<Stream>(this, promised_stream_id);

  auto &promised_header = promised_stream->header;
  promised_header.method = "GET"_sr;
  promised_header.path = push_path;
  promised_header.scheme = scheme;
  promised_header.authority =
    make_string_ref(promised_stream->balloc, authority);

  add_stream(promised_stream_id, std::move(promised_stream));

  return 0;
}

int Http2Handler::submit_rst_stream(Stream *stream, uint32_t error_code) {
  remove_stream_read_timeout(stream);
  remove_stream_write_timeout(stream);

  return nghttp2_submit_rst_stream(session_, NGHTTP2_FLAG_NONE,
                                   stream->stream_id, error_code);
}

void Http2Handler::add_stream(int32_t stream_id,
                              std::unique_ptr<Stream> stream) {
  id2stream_[stream_id] = std::move(stream);
}

void Http2Handler::remove_stream(int32_t stream_id) {
  id2stream_.erase(stream_id);
}

Stream *Http2Handler::get_stream(int32_t stream_id) {
  auto itr = id2stream_.find(stream_id);
  if (itr == std::end(id2stream_)) {
    return nullptr;
  } else {
    return (*itr).second.get();
  }
}

int64_t Http2Handler::session_id() const { return session_id_; }

Sessions *Http2Handler::get_sessions() const { return sessions_; }

const Config *Http2Handler::get_config() const {
  return sessions_->get_config();
}

void Http2Handler::remove_settings_timer() {
  ev_timer_stop(sessions_->get_loop(), &settings_timerev_);
}

void Http2Handler::terminate_session(uint32_t error_code) {
  nghttp2_session_terminate_session(session_, error_code);
}

nghttp2_ssize file_read_callback(nghttp2_session *session, int32_t stream_id,
                                 uint8_t *buf, size_t length,
                                 uint32_t *data_flags,
                                 nghttp2_data_source *source, void *user_data) {
  int rv;
  auto hd = static_cast<Http2Handler *>(user_data);
  auto stream = hd->get_stream(stream_id);

  auto nread = std::min(stream->body_length - stream->body_offset,
                        static_cast<int64_t>(length));

  *data_flags |= NGHTTP2_DATA_FLAG_NO_COPY;

  if (nread == 0 || stream->body_length == stream->body_offset + nread) {
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;

    auto config = hd->get_config();
    if (!config->trailer.empty()) {
      std::vector<nghttp2_nv> nva;
      nva.reserve(config->trailer.size());
      for (auto &kv : config->trailer) {
        nva.push_back(http2::make_field_nv(kv.name, kv.value,
                                           http2::no_index(kv.no_index)));
      }
      rv = nghttp2_submit_trailer(session, stream_id, nva.data(), nva.size());
      if (rv != 0) {
        if (nghttp2_is_fatal(rv)) {
          return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
      } else {
        *data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
      }
    }

    if (nghttp2_session_get_stream_remote_close(session, stream_id) == 0) {
      remove_stream_read_timeout(stream);
      remove_stream_write_timeout(stream);

      hd->submit_rst_stream(stream, NGHTTP2_NO_ERROR);
    }
  }

  return nread;
}

namespace {
void prepare_status_response(Stream *stream, Http2Handler *hd, int status) {
  auto sessions = hd->get_sessions();
  auto status_page = sessions->get_server()->get_status_page(status);
  auto file_ent = &status_page->file_ent;

  // we don't set stream->file_ent since we don't want to expire it.
  stream->body_length = file_ent->length;
  nghttp2_data_provider2 data_prd;
  data_prd.source.fd = file_ent->fd;
  data_prd.read_callback = file_read_callback;

  HeaderRefs headers;
  headers.reserve(2);
  headers.emplace_back("content-type"_sr, "text/html; charset=UTF-8"_sr);
  headers.emplace_back(
    "content-length"_sr,
    util::make_string_ref_uint(stream->balloc, file_ent->length));
  hd->submit_response(StringRef{status_page->status}, stream->stream_id,
                      headers, &data_prd);
}
} // namespace

namespace {
void prepare_echo_response(Stream *stream, Http2Handler *hd) {
  auto length = lseek(stream->file_ent->fd, 0, SEEK_END);
  if (length == -1) {
    hd->submit_rst_stream(stream, NGHTTP2_INTERNAL_ERROR);
    return;
  }
  stream->body_length = length;
  if (lseek(stream->file_ent->fd, 0, SEEK_SET) == -1) {
    hd->submit_rst_stream(stream, NGHTTP2_INTERNAL_ERROR);
    return;
  }
  nghttp2_data_provider2 data_prd;
  data_prd.source.fd = stream->file_ent->fd;
  data_prd.read_callback = file_read_callback;

  HeaderRefs headers;
  headers.emplace_back("nghttpd-response"_sr, "echo"_sr);
  if (!hd->get_config()->no_content_length) {
    headers.emplace_back("content-length"_sr,
                         util::make_string_ref_uint(stream->balloc, length));
  }

  hd->submit_response("200"_sr, stream->stream_id, headers, &data_prd);
}
} // namespace

namespace {
bool prepare_upload_temp_store(Stream *stream, Http2Handler *hd) {
  auto sessions = hd->get_sessions();

  char tempfn[] = "/tmp/nghttpd.temp.XXXXXX";
  auto fd = mkstemp(tempfn);
  if (fd == -1) {
    return false;
  }
  unlink(tempfn);
  // Ordinary request never start with "echo:".  The length is 0 for
  // now.  We will update it when we get whole request body.
  auto path = std::string("echo:") + tempfn;
  stream->file_ent =
    sessions->cache_fd(path, FileEntry(path, 0, 0, fd, nullptr, {}, true));
  stream->echo_upload = true;
  return true;
}
} // namespace

namespace {
void prepare_redirect_response(Stream *stream, Http2Handler *hd,
                               const StringRef &path, int status) {
  auto scheme = stream->header.scheme;

  auto authority = stream->header.authority;
  if (authority.empty()) {
    authority = stream->header.host;
  }

  auto location =
    concat_string_ref(stream->balloc, scheme, "://"_sr, authority, path);

  auto headers = HeaderRefs{{"location"_sr, location}};

  auto sessions = hd->get_sessions();
  auto status_page = sessions->get_server()->get_status_page(status);

  hd->submit_response(StringRef{status_page->status}, stream->stream_id,
                      headers, nullptr);
}
} // namespace

namespace {
void prepare_response(Stream *stream, Http2Handler *hd,
                      bool allow_push = true) {
  int rv;
  auto reqpath = stream->header.path;
  if (reqpath.empty()) {
    prepare_status_response(stream, hd, 405);
    return;
  }

  auto ims = stream->header.ims;

  time_t last_mod = 0;
  bool last_mod_found = false;
  if (!ims.empty()) {
    last_mod_found = true;
    last_mod = util::parse_http_date(ims);
  }

  StringRef raw_path, raw_query;
  auto query_pos = std::find(std::begin(reqpath), std::end(reqpath), '?');
  if (query_pos != std::end(reqpath)) {
    // Do not response to this request to allow clients to test timeouts.
    if ("nghttpd_do_not_respond_to_req=yes"_sr ==
        StringRef{query_pos, std::end(reqpath)}) {
      return;
    }
    raw_path = StringRef{std::begin(reqpath), query_pos};
    raw_query = StringRef{query_pos, std::end(reqpath)};
  } else {
    raw_path = reqpath;
  }

  auto sessions = hd->get_sessions();

  StringRef path;
  if (std::find(std::begin(raw_path), std::end(raw_path), '%') ==
      std::end(raw_path)) {
    path = raw_path;
  } else {
    path = util::percent_decode(stream->balloc, raw_path);
  }

  path = http2::path_join(stream->balloc, StringRef{}, StringRef{}, path,
                          StringRef{});

  if (std::find(std::begin(path), std::end(path), '\\') != std::end(path)) {
    if (stream->file_ent) {
      sessions->release_fd(stream->file_ent);
      stream->file_ent = nullptr;
    }
    prepare_status_response(stream, hd, 404);
    return;
  }

  if (!hd->get_config()->push.empty()) {
    auto push_itr = hd->get_config()->push.find(std::string{path});
    if (allow_push && push_itr != std::end(hd->get_config()->push)) {
      for (auto &push_path : (*push_itr).second) {
        rv = hd->submit_push_promise(stream, StringRef{push_path});
        if (rv != 0) {
          std::cerr << "nghttp2_submit_push_promise() returned error: "
                    << nghttp2_strerror(rv) << std::endl;
        }
      }
    }
  }

  std::string file_path;
  {
    auto len = hd->get_config()->htdocs.size() + path.size();

    auto trailing_slash = path[path.size() - 1] == '/';
    if (trailing_slash) {
      len += DEFAULT_HTML.size();
    }

    file_path.resize(len);

    auto p = &file_path[0];

    auto &htdocs = hd->get_config()->htdocs;
    p = std::copy(std::begin(htdocs), std::end(htdocs), p);
    p = std::copy(std::begin(path), std::end(path), p);
    if (trailing_slash) {
      std::copy(std::begin(DEFAULT_HTML), std::end(DEFAULT_HTML), p);
    }
  }

  if (stream->echo_upload) {
    assert(stream->file_ent);
    prepare_echo_response(stream, hd);
    return;
  }

  auto file_ent = sessions->get_cached_fd(file_path);

  if (file_ent == nullptr) {
    int file = open(file_path.c_str(), O_RDONLY | O_BINARY);
    if (file == -1) {
      prepare_status_response(stream, hd, 404);

      return;
    }

    struct stat buf;

    if (fstat(file, &buf) == -1) {
      close(file);
      prepare_status_response(stream, hd, 404);

      return;
    }

    if (buf.st_mode & S_IFDIR) {
      close(file);

      auto reqpath =
        concat_string_ref(stream->balloc, raw_path, "/"_sr, raw_query);

      prepare_redirect_response(stream, hd, reqpath, 301);

      return;
    }

    const std::string *content_type = nullptr;

    auto ext = file_path.c_str() + file_path.size() - 1;
    for (; file_path.c_str() < ext && *ext != '.' && *ext != '/'; --ext)
      ;
    if (*ext == '.') {
      ++ext;

      const auto &mime_types = hd->get_config()->mime_types;
      auto content_type_itr = mime_types.find(ext);
      if (content_type_itr != std::end(mime_types)) {
        content_type = &(*content_type_itr).second;
      }
    }

    file_ent = sessions->cache_fd(
      file_path, FileEntry(file_path, buf.st_size, buf.st_mtime, file,
                           content_type, std::chrono::steady_clock::now()));
  }

  stream->file_ent = file_ent;

  if (last_mod_found && file_ent->mtime <= last_mod) {
    hd->submit_response("304"_sr, stream->stream_id, nullptr);

    return;
  }

  auto method = stream->header.method;
  if (method == "HEAD"_sr) {
    hd->submit_file_response("200"_sr, stream, file_ent->mtime,
                             file_ent->length, file_ent->content_type, nullptr);
    return;
  }

  stream->body_length = file_ent->length;

  nghttp2_data_provider2 data_prd;

  data_prd.source.fd = file_ent->fd;
  data_prd.read_callback = file_read_callback;

  hd->submit_file_response("200"_sr, stream, file_ent->mtime, file_ent->length,
                           file_ent->content_type, &data_prd);
}
} // namespace

namespace {
int on_header_callback2(nghttp2_session *session, const nghttp2_frame *frame,
                        nghttp2_rcbuf *name, nghttp2_rcbuf *value,
                        uint8_t flags, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);

  auto namebuf = nghttp2_rcbuf_get_buf(name);
  auto valuebuf = nghttp2_rcbuf_get_buf(value);

  if (hd->get_config()->verbose) {
    print_session_id(hd->session_id());
    verbose_on_header_callback(session, frame, namebuf.base, namebuf.len,
                               valuebuf.base, valuebuf.len, flags, user_data);
  }
  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }
  auto stream = hd->get_stream(frame->hd.stream_id);
  if (!stream) {
    return 0;
  }

  if (stream->header_buffer_size + namebuf.len + valuebuf.len > 64_k) {
    hd->submit_rst_stream(stream, NGHTTP2_INTERNAL_ERROR);
    return 0;
  }

  stream->header_buffer_size += namebuf.len + valuebuf.len;

  auto token = http2::lookup_token(StringRef{namebuf.base, namebuf.len});

  auto &header = stream->header;

  switch (token) {
  case http2::HD__METHOD:
    header.method = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.method = value;
    nghttp2_rcbuf_incref(value);
    break;
  case http2::HD__SCHEME:
    header.scheme = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.scheme = value;
    nghttp2_rcbuf_incref(value);
    break;
  case http2::HD__AUTHORITY:
    header.authority = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.authority = value;
    nghttp2_rcbuf_incref(value);
    break;
  case http2::HD_HOST:
    header.host = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.host = value;
    nghttp2_rcbuf_incref(value);
    break;
  case http2::HD__PATH:
    header.path = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.path = value;
    nghttp2_rcbuf_incref(value);
    break;
  case http2::HD_IF_MODIFIED_SINCE:
    header.ims = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.ims = value;
    nghttp2_rcbuf_incref(value);
    break;
  case http2::HD_EXPECT:
    header.expect = StringRef{valuebuf.base, valuebuf.len};
    header.rcbuf.expect = value;
    nghttp2_rcbuf_incref(value);
    break;
  }

  return 0;
}
} // namespace

namespace {
int on_begin_headers_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);

  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  auto stream = std::make_unique<Stream>(hd, frame->hd.stream_id);

  add_stream_read_timeout(stream.get());

  hd->add_stream(frame->hd.stream_id, std::move(stream));

  return 0;
}
} // namespace

namespace {
int hd_on_frame_recv_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);
  if (hd->get_config()->verbose) {
    print_session_id(hd->session_id());
    verbose_on_frame_recv_callback(session, frame, user_data);
  }
  switch (frame->hd.type) {
  case NGHTTP2_DATA: {
    // TODO Handle POST
    auto stream = hd->get_stream(frame->hd.stream_id);
    if (!stream) {
      return 0;
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      remove_stream_read_timeout(stream);
      if (stream->echo_upload || !hd->get_config()->early_response) {
        prepare_response(stream, hd);
      }
    } else {
      add_stream_read_timeout(stream);
    }

    break;
  }
  case NGHTTP2_HEADERS: {
    auto stream = hd->get_stream(frame->hd.stream_id);
    if (!stream) {
      return 0;
    }

    if (frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
      auto expect100 = stream->header.expect;

      if (util::strieq("100-continue"_sr, expect100)) {
        hd->submit_non_final_response("100", frame->hd.stream_id);
      }

      auto method = stream->header.method;
      if (hd->get_config()->echo_upload &&
          (method == "POST"_sr || method == "PUT"_sr)) {
        if (!prepare_upload_temp_store(stream, hd)) {
          hd->submit_rst_stream(stream, NGHTTP2_INTERNAL_ERROR);
          return 0;
        }
      } else if (hd->get_config()->early_response) {
        prepare_response(stream, hd);
      }
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      remove_stream_read_timeout(stream);
      if (stream->echo_upload || !hd->get_config()->early_response) {
        prepare_response(stream, hd);
      }
    } else {
      add_stream_read_timeout(stream);
    }

    break;
  }
  case NGHTTP2_SETTINGS:
    if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
      hd->remove_settings_timer();
    }
    break;
  default:
    break;
  }
  return 0;
}
} // namespace

namespace {
int hd_on_frame_send_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);

  if (hd->get_config()->verbose) {
    print_session_id(hd->session_id());
    verbose_on_frame_send_callback(session, frame, user_data);
  }

  switch (frame->hd.type) {
  case NGHTTP2_DATA:
  case NGHTTP2_HEADERS: {
    auto stream = hd->get_stream(frame->hd.stream_id);

    if (!stream) {
      return 0;
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      remove_stream_write_timeout(stream);
    } else if (std::min(nghttp2_session_get_stream_remote_window_size(
                          session, frame->hd.stream_id),
                        nghttp2_session_get_remote_window_size(session)) <= 0) {
      // If stream is blocked by flow control, enable write timeout.
      add_stream_read_timeout_if_pending(stream);
      add_stream_write_timeout(stream);
    } else {
      add_stream_read_timeout_if_pending(stream);
      remove_stream_write_timeout(stream);
    }

    break;
  }
  case NGHTTP2_SETTINGS: {
    if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
      return 0;
    }

    hd->start_settings_timer();

    break;
  }
  case NGHTTP2_PUSH_PROMISE: {
    auto promised_stream_id = frame->push_promise.promised_stream_id;
    auto promised_stream = hd->get_stream(promised_stream_id);
    auto stream = hd->get_stream(frame->hd.stream_id);

    if (!stream || !promised_stream) {
      return 0;
    }

    add_stream_read_timeout_if_pending(stream);
    add_stream_write_timeout(stream);

    prepare_response(promised_stream, hd, /*allow_push */ false);
  }
  }
  return 0;
}
} // namespace

namespace {
int send_data_callback(nghttp2_session *session, nghttp2_frame *frame,
                       const uint8_t *framehd, size_t length,
                       nghttp2_data_source *source, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);
  auto wb = hd->get_wb();
  auto padlen = frame->data.padlen;
  auto stream = hd->get_stream(frame->hd.stream_id);

  if (wb->wleft() < 9 + length + padlen) {
    return NGHTTP2_ERR_WOULDBLOCK;
  }

  int fd = source->fd;

  auto p = wb->last;

  p = std::copy_n(framehd, 9, p);

  if (padlen) {
    *p++ = padlen - 1;
  }

  while (length) {
    ssize_t nread;
    while ((nread = pread(fd, p, length, stream->body_offset)) == -1 &&
           errno == EINTR)
      ;

    if (nread == -1) {
      remove_stream_read_timeout(stream);
      remove_stream_write_timeout(stream);

      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    stream->body_offset += nread;
    length -= nread;
    p += nread;
  }

  if (padlen) {
    std::fill(p, p + padlen - 1, 0);
    p += padlen - 1;
  }

  wb->last = p;

  return 0;
}
} // namespace

namespace {
nghttp2_ssize select_padding_callback(nghttp2_session *session,
                                      const nghttp2_frame *frame,
                                      size_t max_payload, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);
  return std::min(max_payload, frame->hd.length + hd->get_config()->padding);
}
} // namespace

namespace {
int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags,
                                int32_t stream_id, const uint8_t *data,
                                size_t len, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);
  auto stream = hd->get_stream(stream_id);

  if (!stream) {
    return 0;
  }

  if (stream->echo_upload) {
    assert(stream->file_ent);
    while (len) {
      ssize_t n;
      while ((n = write(stream->file_ent->fd, data, len)) == -1 &&
             errno == EINTR)
        ;
      if (n == -1) {
        hd->submit_rst_stream(stream, NGHTTP2_INTERNAL_ERROR);
        return 0;
      }
      len -= n;
      data += n;
    }
  }
  // TODO Handle POST

  add_stream_read_timeout(stream);

  return 0;
}
} // namespace

namespace {
int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                             uint32_t error_code, void *user_data) {
  auto hd = static_cast<Http2Handler *>(user_data);
  hd->remove_stream(stream_id);
  if (hd->get_config()->verbose) {
    print_session_id(hd->session_id());
    print_timer();
    printf(" stream_id=%d closed\n", stream_id);
    fflush(stdout);
  }
  return 0;
}
} // namespace

namespace {
void fill_callback(nghttp2_session_callbacks *callbacks, const Config *config) {
  nghttp2_session_callbacks_set_on_stream_close_callback(
    callbacks, on_stream_close_callback);

  nghttp2_session_callbacks_set_on_frame_recv_callback(
    callbacks, hd_on_frame_recv_callback);

  nghttp2_session_callbacks_set_on_frame_send_callback(
    callbacks, hd_on_frame_send_callback);

  if (config->verbose) {
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(
      callbacks, verbose_on_invalid_frame_recv_callback);

    nghttp2_session_callbacks_set_error_callback2(callbacks,
                                                  verbose_error_callback);
  }

  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
    callbacks, on_data_chunk_recv_callback);

  nghttp2_session_callbacks_set_on_header_callback2(callbacks,
                                                    on_header_callback2);

  nghttp2_session_callbacks_set_on_begin_headers_callback(
    callbacks, on_begin_headers_callback);

  nghttp2_session_callbacks_set_send_data_callback(callbacks,
                                                   send_data_callback);

  if (config->padding) {
    nghttp2_session_callbacks_set_select_padding_callback2(
      callbacks, select_padding_callback);
  }
}
} // namespace

struct ClientInfo {
  int fd;
};

struct Worker {
  std::unique_ptr<Sessions> sessions;
  ev_async w;
  // protects q
  std::mutex m;
  std::deque<ClientInfo> q;
};

namespace {
void worker_acceptcb(struct ev_loop *loop, ev_async *w, int revents) {
  auto worker = static_cast<Worker *>(w->data);
  auto &sessions = worker->sessions;

  std::deque<ClientInfo> q;
  {
    std::lock_guard<std::mutex> lock(worker->m);
    q.swap(worker->q);
  }

  for (const auto &c : q) {
    sessions->accept_connection(c.fd);
  }
}
} // namespace

namespace {
void run_worker(Worker *worker) {
  auto loop = worker->sessions->get_loop();

  ev_run(loop, 0);

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
  wc_ecc_fp_free();
#endif // NGHTTP2_OPENSSL_IS_WOLFSSL
}
} // namespace

namespace {
int get_ev_loop_flags() {
  if (ev_supported_backends() & ~ev_recommended_backends() & EVBACKEND_KQUEUE) {
    return ev_recommended_backends() | EVBACKEND_KQUEUE;
  }

  return 0;
}
} // namespace

class AcceptHandler {
public:
  AcceptHandler(HttpServer *sv, Sessions *sessions, const Config *config)
    : sessions_(sessions), config_(config), next_worker_(0) {
    if (config_->num_worker == 1) {
      return;
    }
    for (size_t i = 0; i < config_->num_worker; ++i) {
      if (config_->verbose) {
        std::cerr << "spawning thread #" << i << std::endl;
      }
      auto worker = std::make_unique<Worker>();
      auto loop = ev_loop_new(get_ev_loop_flags());
      worker->sessions =
        std::make_unique<Sessions>(sv, loop, config_, sessions_->get_ssl_ctx());
      ev_async_init(&worker->w, worker_acceptcb);
      worker->w.data = worker.get();
      ev_async_start(loop, &worker->w);

      auto t = std::thread(run_worker, worker.get());
      t.detach();
      workers_.push_back(std::move(worker));
    }
  }
  void accept_connection(int fd) {
    if (config_->num_worker == 1) {
      sessions_->accept_connection(fd);
      return;
    }

    // Dispatch client to the one of the worker threads, in a round
    // robin manner.
    auto &worker = workers_[next_worker_];
    if (next_worker_ == config_->num_worker - 1) {
      next_worker_ = 0;
    } else {
      ++next_worker_;
    }
    {
      std::lock_guard<std::mutex> lock(worker->m);
      worker->q.push_back({fd});
    }
    ev_async_send(worker->sessions->get_loop(), &worker->w);
  }

private:
  std::vector<std::unique_ptr<Worker>> workers_;
  Sessions *sessions_;
  const Config *config_;
  // In multi threading mode, this points to the next thread that
  // client will be dispatched.
  size_t next_worker_;
};

namespace {
void acceptcb(struct ev_loop *loop, ev_io *w, int revents);
} // namespace

class ListenEventHandler {
public:
  ListenEventHandler(Sessions *sessions, int fd,
                     std::shared_ptr<AcceptHandler> acceptor)
    : acceptor_(std::move(acceptor)), sessions_(sessions), fd_(fd) {
    ev_io_init(&w_, acceptcb, fd, EV_READ);
    w_.data = this;
    ev_io_start(sessions_->get_loop(), &w_);
  }
  void accept_connection() {
    for (;;) {
#ifdef HAVE_ACCEPT4
      auto fd = accept4(fd_, nullptr, nullptr, SOCK_NONBLOCK);
#else  // !HAVE_ACCEPT4
      auto fd = accept(fd_, nullptr, nullptr);
#endif // !HAVE_ACCEPT4
      if (fd == -1) {
        break;
      }
#ifndef HAVE_ACCEPT4
      util::make_socket_nonblocking(fd);
#endif // !HAVE_ACCEPT4
      acceptor_->accept_connection(fd);
    }
  }

private:
  ev_io w_;
  std::shared_ptr<AcceptHandler> acceptor_;
  Sessions *sessions_;
  int fd_;
};

namespace {
void acceptcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto handler = static_cast<ListenEventHandler *>(w->data);
  handler->accept_connection();
}
} // namespace

namespace {
FileEntry make_status_body(int status, uint16_t port) {
  BlockAllocator balloc(1024, 1024);

  auto status_string = http2::stringify_status(balloc, status);
  auto reason_pharase = http2::get_reason_phrase(status);

  std::string body;
  body = "<html><head><title>";
  body += status_string;
  body += ' ';
  body += reason_pharase;
  body += "</title></head><body><h1>";
  body += status_string;
  body += ' ';
  body += reason_pharase;
  body += "</h1><hr><address>";
  body += NGHTTPD_SERVER;
  body += " at port ";
  body += util::utos(port);
  body += "</address>";
  body += "</body></html>";

  char tempfn[] = "/tmp/nghttpd.temp.XXXXXX";
  int fd = mkstemp(tempfn);
  if (fd == -1) {
    auto error = errno;
    std::cerr << "Could not open status response body file: errno=" << error;
    assert(0);
  }
  unlink(tempfn);
  ssize_t nwrite;
  while ((nwrite = write(fd, body.c_str(), body.size())) == -1 &&
         errno == EINTR)
    ;
  if (nwrite == -1) {
    auto error = errno;
    std::cerr << "Could not write status response body into file: errno="
              << error;
    assert(0);
  }

  return FileEntry(util::utos(status), nwrite, 0, fd, nullptr, {});
}
} // namespace

// index into HttpServer::status_pages_
enum {
  IDX_200,
  IDX_301,
  IDX_400,
  IDX_404,
  IDX_405,
};

HttpServer::HttpServer(const Config *config) : config_(config) {
  status_pages_ = std::vector<StatusPage>{
    {"200", make_status_body(200, config_->port)},
    {"301", make_status_body(301, config_->port)},
    {"400", make_status_body(400, config_->port)},
    {"404", make_status_body(404, config_->port)},
    {"405", make_status_body(405, config_->port)},
  };
}

namespace {
int verify_callback(int preverify_ok, X509_STORE_CTX *ctx) {
  // We don't verify the client certificate. Just request it for the
  // testing purpose.
  return 1;
}
} // namespace

namespace {
int start_listen(HttpServer *sv, struct ev_loop *loop, Sessions *sessions,
                 const Config *config) {
  int r;
  bool ok = false;
  const char *addr = nullptr;

  std::shared_ptr<AcceptHandler> acceptor;
  auto service = util::utos(config->port);

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
#ifdef AI_ADDRCONFIG
  hints.ai_flags |= AI_ADDRCONFIG;
#endif // AI_ADDRCONFIG

  if (!config->address.empty()) {
    addr = config->address.c_str();
  }

  addrinfo *res, *rp;
  r = getaddrinfo(addr, service.c_str(), &hints, &res);
  if (r != 0) {
    std::cerr << "getaddrinfo() failed: " << gai_strerror(r) << std::endl;
    return -1;
  }

  for (rp = res; rp; rp = rp->ai_next) {
    int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd == -1) {
      continue;
    }
    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      close(fd);
      continue;
    }
    (void)util::make_socket_nonblocking(fd);
#ifdef IPV6_V6ONLY
    if (rp->ai_family == AF_INET6) {
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
                     static_cast<socklen_t>(sizeof(val))) == -1) {
        close(fd);
        continue;
      }
    }
#endif // IPV6_V6ONLY
    if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0 && listen(fd, 1000) == 0) {
      if (!acceptor) {
        acceptor = std::make_shared<AcceptHandler>(sv, sessions, config);
      }
      new ListenEventHandler(sessions, fd, acceptor);

      if (config->verbose) {
        std::string s = util::numeric_name(rp->ai_addr, rp->ai_addrlen);
        std::cout << (rp->ai_family == AF_INET ? "IPv4" : "IPv6") << ": listen "
                  << s << ":" << config->port << std::endl;
      }
      ok = true;
      continue;
    } else {
      std::cerr << strerror(errno) << std::endl;
    }
    close(fd);
  }
  freeaddrinfo(res);

  if (!ok) {
    return -1;
  }
  return 0;
}
} // namespace

namespace {
int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                         unsigned char *outlen, const unsigned char *in,
                         unsigned int inlen, void *arg) {
  auto config = static_cast<HttpServer *>(arg)->get_config();
  if (config->verbose) {
    std::cout << "[ALPN] client offers:" << std::endl;
  }
  if (config->verbose) {
    for (unsigned int i = 0; i < inlen; i += in[i] + 1) {
      std::cout << " * ";
      std::cout.write(reinterpret_cast<const char *>(&in[i + 1]), in[i]);
      std::cout << std::endl;
    }
  }
  if (!util::select_h2(out, outlen, in, inlen)) {
    return SSL_TLSEXT_ERR_NOACK;
  }
  return SSL_TLSEXT_ERR_OK;
}
} // namespace

int HttpServer::run() {
  SSL_CTX *ssl_ctx = nullptr;
  std::vector<unsigned char> next_proto;

  if (!config_->no_tls) {
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
      std::cerr << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
      return -1;
    }

    auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                    SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
                    SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET |
                    SSL_OP_CIPHER_SERVER_PREFERENCE;

#ifdef SSL_OP_ENABLE_KTLS
    if (config_->ktls) {
      ssl_opts |= SSL_OP_ENABLE_KTLS;
    }
#endif // SSL_OP_ENABLE_KTLS

    SSL_CTX_set_options(ssl_ctx, ssl_opts);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

    if (nghttp2::tls::ssl_ctx_set_proto_versions(
          ssl_ctx, nghttp2::tls::NGHTTP2_TLS_MIN_VERSION,
          nghttp2::tls::NGHTTP2_TLS_MAX_VERSION) != 0) {
      std::cerr << "Could not set TLS versions" << std::endl;
      return -1;
    }

    if (SSL_CTX_set_cipher_list(ssl_ctx, tls::DEFAULT_CIPHER_LIST.data()) ==
        0) {
      std::cerr << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
      return -1;
    }

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
    if (SSL_CTX_set_ciphersuites(ssl_ctx,
                                 tls::DEFAULT_TLS13_CIPHER_LIST.data()) == 0) {
      std::cerr << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
      return -1;
    }
#endif // NGHTTP2_OPENSSL_IS_WOLFSSL

    const unsigned char sid_ctx[] = "nghttpd";
    SSL_CTX_set_session_id_context(ssl_ctx, sid_ctx, sizeof(sid_ctx) - 1);
    SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_SERVER);

#ifndef OPENSSL_NO_EC
    if (SSL_CTX_set1_curves_list(ssl_ctx, "P-256") != 1) {
      std::cerr << "SSL_CTX_set1_curves_list failed: "
                << ERR_error_string(ERR_get_error(), nullptr);
      return -1;
    }
#endif // OPENSSL_NO_EC

    if (!config_->dh_param_file.empty()) {
      // Read DH parameters from file
      auto bio = BIO_new_file(config_->dh_param_file.c_str(), "rb");
      if (bio == nullptr) {
        std::cerr << "BIO_new_file() failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return -1;
      }

#if OPENSSL_3_0_0_API
      EVP_PKEY *dh = nullptr;
      auto dctx = OSSL_DECODER_CTX_new_for_pkey(
        &dh, "PEM", nullptr, "DH", OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS,
        nullptr, nullptr);

      if (!OSSL_DECODER_from_bio(dctx, bio)) {
        std::cerr << "OSSL_DECODER_from_bio() failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return -1;
      }

      if (SSL_CTX_set0_tmp_dh_pkey(ssl_ctx, dh) != 1) {
        std::cerr << "SSL_CTX_set0_tmp_dh_pkey failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return -1;
      }
#else  // !OPENSSL_3_0_0_API
      auto dh = PEM_read_bio_DHparams(bio, nullptr, nullptr, nullptr);

      if (dh == nullptr) {
        std::cerr << "PEM_read_bio_DHparams() failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return -1;
      }

      SSL_CTX_set_tmp_dh(ssl_ctx, dh);
      DH_free(dh);
#endif // !OPENSSL_3_0_0_API
      BIO_free(bio);
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, config_->private_key_file.c_str(),
                                    SSL_FILETYPE_PEM) != 1) {
      std::cerr << "SSL_CTX_use_PrivateKey_file failed." << std::endl;
      return -1;
    }
    if (SSL_CTX_use_certificate_chain_file(ssl_ctx,
                                           config_->cert_file.c_str()) != 1) {
      std::cerr << "SSL_CTX_use_certificate_file failed." << std::endl;
      return -1;
    }
    if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
      std::cerr << "SSL_CTX_check_private_key failed." << std::endl;
      return -1;
    }
    if (config_->verify_client) {
      SSL_CTX_set_verify(ssl_ctx,
                         SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE |
                           SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                         verify_callback);
    }

    next_proto = util::get_default_alpn();

    // ALPN selection callback
    SSL_CTX_set_alpn_select_cb(ssl_ctx, alpn_select_proto_cb, this);

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
    if (!SSL_CTX_add_cert_compression_alg(
          ssl_ctx, nghttp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
          nghttp2::tls::cert_compress, nghttp2::tls::cert_decompress)) {
      std::cerr << "SSL_CTX_add_cert_compression_alg failed." << std::endl;
      return -1;
    }
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI
  }

  auto loop = EV_DEFAULT;

  Sessions sessions(this, loop, config_, ssl_ctx);
  if (start_listen(this, loop, &sessions, config_) != 0) {
    std::cerr << "Could not listen" << std::endl;
    if (ssl_ctx) {
      SSL_CTX_free(ssl_ctx);
    }
    return -1;
  }

  ev_run(loop, 0);

  SSL_CTX_free(ssl_ctx);

  return 0;
}

const Config *HttpServer::get_config() const { return config_; }

const StatusPage *HttpServer::get_status_page(int status) const {
  switch (status) {
  case 200:
    return &status_pages_[IDX_200];
  case 301:
    return &status_pages_[IDX_301];
  case 400:
    return &status_pages_[IDX_400];
  case 404:
    return &status_pages_[IDX_404];
  case 405:
    return &status_pages_[IDX_405];
  default:
    assert(0);
  }
  return nullptr;
}

} // namespace nghttp2
