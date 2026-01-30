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
#include "shrpx_connection_handler.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#include <sys/types.h>
#include <sys/wait.h>

#include <cerrno>
#include <thread>
#include <random>

#include "shrpx_client_handler.h"
#include "shrpx_tls.h"
#include "shrpx_worker.h"
#include "shrpx_config.h"
#include "shrpx_http2_session.h"
#include "shrpx_connect_blocker.h"
#include "shrpx_downstream_connection.h"
#include "shrpx_accept_handler.h"
#include "shrpx_memcached_dispatcher.h"
#include "shrpx_signal.h"
#include "shrpx_log.h"
#include "xsi_strerror.h"
#include "util.h"
#include "template.h"
#include "ssl_compat.h"

using namespace nghttp2;

namespace shrpx {

namespace {
void acceptor_disable_cb(struct ev_loop *loop, ev_timer *w, int revent) {
  auto h = static_cast<ConnectionHandler *>(w->data);

  // If we are in graceful shutdown period, we must not enable
  // acceptors again.
  if (h->get_graceful_shutdown()) {
    return;
  }

  h->enable_acceptor();
}
} // namespace

namespace {
void ocsp_cb(struct ev_loop *loop, ev_timer *w, int revent) {
  auto h = static_cast<ConnectionHandler *>(w->data);

  // If we are in graceful shutdown period, we won't do ocsp query.
  if (h->get_graceful_shutdown()) {
    return;
  }

  LOG(NOTICE) << "Start ocsp update";

  h->proceed_next_cert_ocsp();
}
} // namespace

namespace {
void ocsp_read_cb(struct ev_loop *loop, ev_io *w, int revent) {
  auto h = static_cast<ConnectionHandler *>(w->data);

  h->read_ocsp_chunk();
}
} // namespace

namespace {
void ocsp_chld_cb(struct ev_loop *loop, ev_child *w, int revent) {
  auto h = static_cast<ConnectionHandler *>(w->data);

  h->handle_ocsp_complete();
}
} // namespace

namespace {
void thread_join_async_cb(struct ev_loop *loop, ev_async *w, int revent) {
  ev_break(loop);
}
} // namespace

namespace {
void serial_event_async_cb(struct ev_loop *loop, ev_async *w, int revent) {
  auto h = static_cast<ConnectionHandler *>(w->data);

  h->handle_serial_event();
}
} // namespace

ConnectionHandler::ConnectionHandler(struct ev_loop *loop, std::mt19937 &gen)
  :
#ifdef ENABLE_HTTP3
    quic_ipc_fd_(-1),
#endif // ENABLE_HTTP3
    gen_(gen),
    single_worker_(nullptr),
    loop_(loop),
#ifdef HAVE_NEVERBLEED
    nb_(nullptr),
#endif // HAVE_NEVERBLEED
    tls_ticket_key_memcached_get_retry_count_(0),
    tls_ticket_key_memcached_fail_count_(0),
    worker_round_robin_cnt_(get_config()->api.enabled ? 1 : 0),
    graceful_shutdown_(false),
    enable_acceptor_on_ocsp_completion_(false) {
  ev_timer_init(&disable_acceptor_timer_, acceptor_disable_cb, 0., 0.);
  disable_acceptor_timer_.data = this;

  ev_timer_init(&ocsp_timer_, ocsp_cb, 0., 0.);
  ocsp_timer_.data = this;

  ev_io_init(&ocsp_.rev, ocsp_read_cb, -1, EV_READ);
  ocsp_.rev.data = this;

  ev_async_init(&thread_join_asyncev_, thread_join_async_cb);

  ev_async_init(&serial_event_asyncev_, serial_event_async_cb);
  serial_event_asyncev_.data = this;

  ev_async_start(loop_, &serial_event_asyncev_);

  ev_child_init(&ocsp_.chldev, ocsp_chld_cb, 0, 0);
  ocsp_.chldev.data = this;

  ocsp_.next = 0;
  ocsp_.proc.rfd = -1;

  reset_ocsp();
}

ConnectionHandler::~ConnectionHandler() {
  ev_child_stop(loop_, &ocsp_.chldev);
  ev_async_stop(loop_, &serial_event_asyncev_);
  ev_async_stop(loop_, &thread_join_asyncev_);
  ev_io_stop(loop_, &ocsp_.rev);
  ev_timer_stop(loop_, &ocsp_timer_);
  ev_timer_stop(loop_, &disable_acceptor_timer_);

#ifdef ENABLE_HTTP3
  for (auto ssl_ctx : quic_all_ssl_ctx_) {
    if (ssl_ctx == nullptr) {
      continue;
    }

    auto tls_ctx_data =
      static_cast<tls::TLSContextData *>(SSL_CTX_get_app_data(ssl_ctx));
    delete tls_ctx_data;
    SSL_CTX_free(ssl_ctx);
  }
#endif // ENABLE_HTTP3

  for (auto ssl_ctx : all_ssl_ctx_) {
    auto tls_ctx_data =
      static_cast<tls::TLSContextData *>(SSL_CTX_get_app_data(ssl_ctx));
    delete tls_ctx_data;
    SSL_CTX_free(ssl_ctx);
  }

  // Free workers before destroying ev_loop
  workers_.clear();

  for (auto loop : worker_loops_) {
    ev_loop_destroy(loop);
  }
}

void ConnectionHandler::set_ticket_keys_to_worker(
  const std::shared_ptr<TicketKeys> &ticket_keys) {
  for (auto &worker : workers_) {
    worker->set_ticket_keys(ticket_keys);
  }
}

void ConnectionHandler::worker_reopen_log_files() {
  for (auto &worker : workers_) {
    WorkerEvent wev{};

    wev.type = WorkerEventType::REOPEN_LOG;

    worker->send(std::move(wev));
  }
}

void ConnectionHandler::worker_replace_downstream(
  std::shared_ptr<DownstreamConfig> downstreamconf) {
  for (auto &worker : workers_) {
    WorkerEvent wev{};

    wev.type = WorkerEventType::REPLACE_DOWNSTREAM;
    wev.downstreamconf = downstreamconf;

    worker->send(std::move(wev));
  }
}

int ConnectionHandler::create_single_worker() {
  cert_tree_ = tls::create_cert_lookup_tree();
  auto sv_ssl_ctx = tls::setup_server_ssl_context(
    all_ssl_ctx_, indexed_ssl_ctx_, cert_tree_.get()
#ifdef HAVE_NEVERBLEED
                                      ,
    nb_
#endif // HAVE_NEVERBLEED
  );

#ifdef ENABLE_HTTP3
  quic_cert_tree_ = tls::create_cert_lookup_tree();
  auto quic_sv_ssl_ctx = tls::setup_quic_server_ssl_context(
    quic_all_ssl_ctx_, quic_indexed_ssl_ctx_, quic_cert_tree_.get()
#  ifdef HAVE_NEVERBLEED
                                                ,
    nb_
#  endif // HAVE_NEVERBLEED
  );
#endif // ENABLE_HTTP3

  auto cl_ssl_ctx = tls::setup_downstream_client_ssl_context(
#ifdef HAVE_NEVERBLEED
    nb_
#endif // HAVE_NEVERBLEED
  );

  if (cl_ssl_ctx) {
    all_ssl_ctx_.push_back(cl_ssl_ctx);
#ifdef ENABLE_HTTP3
    quic_all_ssl_ctx_.push_back(nullptr);
#endif // ENABLE_HTTP3
  }

  auto config = get_config();
  auto &tlsconf = config->tls;

  SSL_CTX *session_cache_ssl_ctx = nullptr;
  {
    auto &memcachedconf = config->tls.session_cache.memcached;
    if (memcachedconf.tls) {
      session_cache_ssl_ctx = tls::create_ssl_client_context(
#ifdef HAVE_NEVERBLEED
        nb_,
#endif // HAVE_NEVERBLEED
        tlsconf.cacert, memcachedconf.cert_file,
        memcachedconf.private_key_file);
      all_ssl_ctx_.push_back(session_cache_ssl_ctx);
#ifdef ENABLE_HTTP3
      quic_all_ssl_ctx_.push_back(nullptr);
#endif // ENABLE_HTTP3
    }
  }

#if defined(ENABLE_HTTP3) && defined(HAVE_LIBBPF)
  quic_bpf_refs_.resize(config->conn.quic_listener.addrs.size());
#endif // ENABLE_HTTP3 && HAVE_LIBBPF

#ifdef ENABLE_HTTP3
  assert(worker_ids_.size() == 1);
  const auto &wid = worker_ids_[0];
#endif // ENABLE_HTTP3

  single_worker_ = std::make_unique<Worker>(
    loop_, sv_ssl_ctx, cl_ssl_ctx, session_cache_ssl_ctx, cert_tree_.get(),
#ifdef ENABLE_HTTP3
    quic_sv_ssl_ctx, quic_cert_tree_.get(), wid,
#  ifdef HAVE_LIBBPF
    /* index = */ 0,
#  endif // HAVE_LIBBPF
#endif   // ENABLE_HTTP3
    ticket_keys_, this, config->conn.downstream);
#ifdef HAVE_MRUBY
  if (single_worker_->create_mruby_context() != 0) {
    return -1;
  }
#endif // HAVE_MRUBY

#ifdef ENABLE_HTTP3
  if (single_worker_->setup_quic_server_socket() != 0) {
    return -1;
  }
#endif // ENABLE_HTTP3

  return 0;
}

int ConnectionHandler::create_worker_thread(size_t num) {
#ifndef NOTHREADS
  assert(workers_.size() == 0);

  cert_tree_ = tls::create_cert_lookup_tree();
  auto sv_ssl_ctx = tls::setup_server_ssl_context(
    all_ssl_ctx_, indexed_ssl_ctx_, cert_tree_.get()
#  ifdef HAVE_NEVERBLEED
                                      ,
    nb_
#  endif // HAVE_NEVERBLEED
  );

#  ifdef ENABLE_HTTP3
  quic_cert_tree_ = tls::create_cert_lookup_tree();
  auto quic_sv_ssl_ctx = tls::setup_quic_server_ssl_context(
    quic_all_ssl_ctx_, quic_indexed_ssl_ctx_, quic_cert_tree_.get()
#    ifdef HAVE_NEVERBLEED
                                                ,
    nb_
#    endif // HAVE_NEVERBLEED
  );
#  endif // ENABLE_HTTP3

  auto cl_ssl_ctx = tls::setup_downstream_client_ssl_context(
#  ifdef HAVE_NEVERBLEED
    nb_
#  endif // HAVE_NEVERBLEED
  );

  if (cl_ssl_ctx) {
    all_ssl_ctx_.push_back(cl_ssl_ctx);
#  ifdef ENABLE_HTTP3
    quic_all_ssl_ctx_.push_back(nullptr);
#  endif // ENABLE_HTTP3
  }

  auto config = get_config();
  auto &tlsconf = config->tls;
  auto &apiconf = config->api;

#  if defined(ENABLE_HTTP3) && defined(HAVE_LIBBPF)
  quic_bpf_refs_.resize(config->conn.quic_listener.addrs.size());
#  endif // ENABLE_HTTP3 && HAVE_LIBBPF

  // We have dedicated worker for API request processing.
  if (apiconf.enabled) {
    ++num;
  }

  SSL_CTX *session_cache_ssl_ctx = nullptr;
  {
    auto &memcachedconf = config->tls.session_cache.memcached;

    if (memcachedconf.tls) {
      session_cache_ssl_ctx = tls::create_ssl_client_context(
#  ifdef HAVE_NEVERBLEED
        nb_,
#  endif // HAVE_NEVERBLEED
        tlsconf.cacert, memcachedconf.cert_file,
        memcachedconf.private_key_file);
      all_ssl_ctx_.push_back(session_cache_ssl_ctx);
#  ifdef ENABLE_HTTP3
      quic_all_ssl_ctx_.push_back(nullptr);
#  endif // ENABLE_HTTP3
    }
  }

#  ifdef ENABLE_HTTP3
  assert(worker_ids_.size() == num);
#  endif // ENABLE_HTTP3

  for (size_t i = 0; i < num; ++i) {
    auto loop = ev_loop_new(config->ev_loop_flags);

#  ifdef ENABLE_HTTP3
    const auto &wid = worker_ids_[i];
#  endif // ENABLE_HTTP3

    auto worker = std::make_unique<Worker>(
      loop, sv_ssl_ctx, cl_ssl_ctx, session_cache_ssl_ctx, cert_tree_.get(),
#  ifdef ENABLE_HTTP3
      quic_sv_ssl_ctx, quic_cert_tree_.get(), wid,
#    ifdef HAVE_LIBBPF
      i,
#    endif // HAVE_LIBBPF
#  endif   // ENABLE_HTTP3
      ticket_keys_, this, config->conn.downstream);
#  ifdef HAVE_MRUBY
    if (worker->create_mruby_context() != 0) {
      return -1;
    }
#  endif // HAVE_MRUBY

#  ifdef ENABLE_HTTP3
    if ((!apiconf.enabled || i != 0) &&
        worker->setup_quic_server_socket() != 0) {
      return -1;
    }
#  endif // ENABLE_HTTP3

    workers_.push_back(std::move(worker));
    worker_loops_.push_back(loop);

    LLOG(NOTICE, this) << "Created worker thread #" << workers_.size() - 1;
  }

  for (auto &worker : workers_) {
    worker->run_async();
  }

#endif // NOTHREADS

  return 0;
}

void ConnectionHandler::join_worker() {
#ifndef NOTHREADS
  int n = 0;

  if (LOG_ENABLED(INFO)) {
    LLOG(INFO, this) << "Waiting for worker thread to join: n="
                     << workers_.size();
  }

  for (auto &worker : workers_) {
    worker->wait();
    if (LOG_ENABLED(INFO)) {
      LLOG(INFO, this) << "Thread #" << n << " joined";
    }
    ++n;
  }
#endif // NOTHREADS
}

void ConnectionHandler::graceful_shutdown_worker() {
  if (single_worker_) {
    return;
  }

  if (LOG_ENABLED(INFO)) {
    LLOG(INFO, this) << "Sending graceful shutdown signal to worker";
  }

  for (auto &worker : workers_) {
    WorkerEvent wev{};
    wev.type = WorkerEventType::GRACEFUL_SHUTDOWN;

    worker->send(std::move(wev));
  }

#ifndef NOTHREADS
  ev_async_start(loop_, &thread_join_asyncev_);

  thread_join_fut_ = std::async(std::launch::async, [this]() {
    (void)reopen_log_files(get_config()->logging);
    join_worker();
    ev_async_send(get_loop(), &thread_join_asyncev_);
    delete_log_config();
  });
#endif // NOTHREADS
}

int ConnectionHandler::handle_connection(int fd, sockaddr *addr, int addrlen,
                                         const UpstreamAddr *faddr) {
  if (LOG_ENABLED(INFO)) {
    LLOG(INFO, this) << "Accepted connection from "
                     << util::numeric_name(addr, addrlen) << ", fd=" << fd;
  }

  auto config = get_config();

  if (single_worker_) {
    auto &upstreamconf = config->conn.upstream;
    if (single_worker_->get_worker_stat()->num_connections >=
        upstreamconf.worker_connections) {
      if (LOG_ENABLED(INFO)) {
        LLOG(INFO, this) << "Too many connections >="
                         << upstreamconf.worker_connections;
      }

      close(fd);
      return -1;
    }

    auto client =
      tls::accept_connection(single_worker_.get(), fd, addr, addrlen, faddr);
    if (!client) {
      LLOG(ERROR, this) << "ClientHandler creation failed";

      close(fd);
      return -1;
    }

    return 0;
  }

  Worker *worker;

  if (faddr->alt_mode == UpstreamAltMode::API) {
    worker = workers_[0].get();

    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Dispatch connection to API worker #0";
    }
  } else {
    worker = workers_[worker_round_robin_cnt_].get();

    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Dispatch connection to worker #" << worker_round_robin_cnt_;
    }

    if (++worker_round_robin_cnt_ == workers_.size()) {
      auto &apiconf = config->api;

      if (apiconf.enabled) {
        worker_round_robin_cnt_ = 1;
      } else {
        worker_round_robin_cnt_ = 0;
      }
    }
  }

  WorkerEvent wev{};
  wev.type = WorkerEventType::NEW_CONNECTION;
  wev.client_fd = fd;
  memcpy(&wev.client_addr, addr, addrlen);
  wev.client_addrlen = addrlen;
  wev.faddr = faddr;

  worker->send(std::move(wev));

  return 0;
}

struct ev_loop *ConnectionHandler::get_loop() const { return loop_; }

Worker *ConnectionHandler::get_single_worker() const {
  return single_worker_.get();
}

void ConnectionHandler::add_acceptor(std::unique_ptr<AcceptHandler> h) {
  acceptors_.push_back(std::move(h));
}

void ConnectionHandler::delete_acceptor() { acceptors_.clear(); }

void ConnectionHandler::enable_acceptor() {
  for (auto &a : acceptors_) {
    a->enable();
  }
}

void ConnectionHandler::disable_acceptor() {
  for (auto &a : acceptors_) {
    a->disable();
  }
}

void ConnectionHandler::sleep_acceptor(ev_tstamp t) {
  if (t == 0. || ev_is_active(&disable_acceptor_timer_)) {
    return;
  }

  disable_acceptor();

  ev_timer_set(&disable_acceptor_timer_, t, 0.);
  ev_timer_start(loop_, &disable_acceptor_timer_);
}

void ConnectionHandler::accept_pending_connection() {
  for (auto &a : acceptors_) {
    a->accept_connection();
  }
}

void ConnectionHandler::set_ticket_keys(
  std::shared_ptr<TicketKeys> ticket_keys) {
  ticket_keys_ = std::move(ticket_keys);
  if (single_worker_) {
    single_worker_->set_ticket_keys(ticket_keys_);
  }
}

const std::shared_ptr<TicketKeys> &ConnectionHandler::get_ticket_keys() const {
  return ticket_keys_;
}

void ConnectionHandler::set_graceful_shutdown(bool f) {
  graceful_shutdown_ = f;
  if (single_worker_) {
    single_worker_->set_graceful_shutdown(f);
  }
}

bool ConnectionHandler::get_graceful_shutdown() const {
  return graceful_shutdown_;
}

void ConnectionHandler::cancel_ocsp_update() {
  enable_acceptor_on_ocsp_completion_ = false;
  ev_timer_stop(loop_, &ocsp_timer_);

  if (ocsp_.proc.pid == 0) {
    return;
  }

  int rv;

  rv = kill(ocsp_.proc.pid, SIGTERM);
  if (rv != 0) {
    auto error = errno;
    LOG(ERROR) << "Could not send signal to OCSP query process: errno="
               << error;
  }

  while ((rv = waitpid(ocsp_.proc.pid, nullptr, 0)) == -1 && errno == EINTR)
    ;
  if (rv == -1) {
    auto error = errno;
    LOG(ERROR) << "Error occurred while we were waiting for the completion of "
                  "OCSP query process: errno="
               << error;
  }
}

// inspired by h2o_read_command function from h2o project:
// https://github.com/h2o/h2o
int ConnectionHandler::start_ocsp_update(const char *cert_file) {
  int rv;

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Start ocsp update for " << cert_file;
  }

  assert(!ev_is_active(&ocsp_.rev));
  assert(!ev_is_active(&ocsp_.chldev));

  char *const argv[] = {
    const_cast<char *>(get_config()->tls.ocsp.fetch_ocsp_response_file.data()),
    const_cast<char *>(cert_file), nullptr};

  Process proc;
  rv = exec_read_command(proc, argv);
  if (rv != 0) {
    return -1;
  }

  ocsp_.proc = proc;

  ev_io_set(&ocsp_.rev, ocsp_.proc.rfd, EV_READ);
  ev_io_start(loop_, &ocsp_.rev);

  ev_child_set(&ocsp_.chldev, ocsp_.proc.pid, 0);
  ev_child_start(loop_, &ocsp_.chldev);

  return 0;
}

void ConnectionHandler::read_ocsp_chunk() {
  std::array<uint8_t, 4_k> buf;
  for (;;) {
    ssize_t n;
    while ((n = read(ocsp_.proc.rfd, buf.data(), buf.size())) == -1 &&
           errno == EINTR)
      ;

    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      auto error = errno;
      LOG(WARN) << "Reading from ocsp query command failed: errno=" << error;
      ocsp_.error = error;

      break;
    }

    if (n == 0) {
      break;
    }

    std::copy_n(std::begin(buf), n, std::back_inserter(ocsp_.resp));
  }

  ev_io_stop(loop_, &ocsp_.rev);
}

void ConnectionHandler::handle_ocsp_complete() {
  ev_io_stop(loop_, &ocsp_.rev);
  ev_child_stop(loop_, &ocsp_.chldev);

  assert(ocsp_.next < all_ssl_ctx_.size());
#ifdef ENABLE_HTTP3
  assert(all_ssl_ctx_.size() == quic_all_ssl_ctx_.size());
#endif // ENABLE_HTTP3

  auto ssl_ctx = all_ssl_ctx_[ocsp_.next];
  auto tls_ctx_data =
    static_cast<tls::TLSContextData *>(SSL_CTX_get_app_data(ssl_ctx));

  auto rstatus = ocsp_.chldev.rstatus;
  auto status = WEXITSTATUS(rstatus);
  if (ocsp_.error || !WIFEXITED(rstatus) || status != 0) {
    LOG(WARN) << "ocsp query command for " << tls_ctx_data->cert_file
              << " failed: error=" << ocsp_.error << ", rstatus=" << log::hex
              << rstatus << log::dec << ", status=" << status;
    ++ocsp_.next;
    proceed_next_cert_ocsp();
    return;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "ocsp update for " << tls_ctx_data->cert_file
              << " finished successfully";
  }

  auto config = get_config();
  auto &tlsconf = config->tls;

  if (tlsconf.ocsp.no_verify ||
      tls::verify_ocsp_response(ssl_ctx, ocsp_.resp.data(),
                                ocsp_.resp.size()) == 0) {
#ifdef ENABLE_HTTP3
    // We have list of SSL_CTX with the same certificate in
    // quic_all_ssl_ctx_ as well.  Some SSL_CTXs are missing there in
    // that case we get nullptr.
    auto quic_ssl_ctx = quic_all_ssl_ctx_[ocsp_.next];
    if (quic_ssl_ctx) {
      auto quic_tls_ctx_data =
        static_cast<tls::TLSContextData *>(SSL_CTX_get_app_data(quic_ssl_ctx));
#  ifdef HAVE_ATOMIC_STD_SHARED_PTR
      quic_tls_ctx_data->ocsp_data.store(
        std::make_shared<std::vector<uint8_t>>(ocsp_.resp),
        std::memory_order_release);
#  else  // !HAVE_ATOMIC_STD_SHARED_PTR
      std::lock_guard<std::mutex> g(quic_tls_ctx_data->mu);
      quic_tls_ctx_data->ocsp_data =
        std::make_shared<std::vector<uint8_t>>(ocsp_.resp);
#  endif // !HAVE_ATOMIC_STD_SHARED_PTR
    }
#endif // ENABLE_HTTP3

#ifdef HAVE_ATOMIC_STD_SHARED_PTR
    tls_ctx_data->ocsp_data.store(
      std::make_shared<std::vector<uint8_t>>(std::move(ocsp_.resp)),
      std::memory_order_release);
#else  // !HAVE_ATOMIC_STD_SHARED_PTR
    std::lock_guard<std::mutex> g(tls_ctx_data->mu);
    tls_ctx_data->ocsp_data =
      std::make_shared<std::vector<uint8_t>>(std::move(ocsp_.resp));
#endif // !HAVE_ATOMIC_STD_SHARED_PTR
  }

  ++ocsp_.next;
  proceed_next_cert_ocsp();
}

void ConnectionHandler::reset_ocsp() {
  if (ocsp_.proc.rfd != -1) {
    close(ocsp_.proc.rfd);
  }

  ocsp_.proc.rfd = -1;
  ocsp_.proc.pid = 0;
  ocsp_.error = 0;
  ocsp_.resp = std::vector<uint8_t>();
}

void ConnectionHandler::proceed_next_cert_ocsp() {
  for (;;) {
    reset_ocsp();
    if (ocsp_.next == all_ssl_ctx_.size()) {
      ocsp_.next = 0;
      // We have updated all ocsp response, and schedule next update.
      ev_timer_set(&ocsp_timer_, get_config()->tls.ocsp.update_interval, 0.);
      ev_timer_start(loop_, &ocsp_timer_);

      if (enable_acceptor_on_ocsp_completion_) {
        enable_acceptor_on_ocsp_completion_ = false;
        enable_acceptor();
      }

      return;
    }

    auto ssl_ctx = all_ssl_ctx_[ocsp_.next];
    auto tls_ctx_data =
      static_cast<tls::TLSContextData *>(SSL_CTX_get_app_data(ssl_ctx));

    // client SSL_CTX is also included in all_ssl_ctx_, but has no
    // tls_ctx_data.
    if (!tls_ctx_data) {
      ++ocsp_.next;
      continue;
    }

    auto cert_file = tls_ctx_data->cert_file;

    if (start_ocsp_update(cert_file) != 0) {
      ++ocsp_.next;
      continue;
    }

    break;
  }
}

void ConnectionHandler::set_tls_ticket_key_memcached_dispatcher(
  std::unique_ptr<MemcachedDispatcher> dispatcher) {
  tls_ticket_key_memcached_dispatcher_ = std::move(dispatcher);
}

MemcachedDispatcher *
ConnectionHandler::get_tls_ticket_key_memcached_dispatcher() const {
  return tls_ticket_key_memcached_dispatcher_.get();
}

// Use the similar backoff algorithm described in
// https://github.com/grpc/grpc/blob/master/doc/connection-backoff.md
namespace {
constexpr size_t MAX_BACKOFF_EXP = 10;
constexpr auto MULTIPLIER = 3.2;
constexpr auto JITTER = 0.2;
} // namespace

void ConnectionHandler::on_tls_ticket_key_network_error(ev_timer *w) {
  if (++tls_ticket_key_memcached_get_retry_count_ >=
      get_config()->tls.ticket.memcached.max_retry) {
    LOG(WARN) << "Memcached: tls ticket get retry all failed "
              << tls_ticket_key_memcached_get_retry_count_ << " times.";

    on_tls_ticket_key_not_found(w);
    return;
  }

  auto base_backoff = util::int_pow(
    MULTIPLIER,
    std::min(MAX_BACKOFF_EXP, tls_ticket_key_memcached_get_retry_count_));
  auto dist = std::uniform_real_distribution<>(-JITTER * base_backoff,
                                               JITTER * base_backoff);

  auto backoff = base_backoff + dist(gen_);

  LOG(WARN)
    << "Memcached: tls ticket get failed due to network error, retrying in "
    << backoff << " seconds";

  ev_timer_set(w, backoff, 0.);
  ev_timer_start(loop_, w);
}

void ConnectionHandler::on_tls_ticket_key_not_found(ev_timer *w) {
  tls_ticket_key_memcached_get_retry_count_ = 0;

  if (++tls_ticket_key_memcached_fail_count_ >=
      get_config()->tls.ticket.memcached.max_fail) {
    LOG(WARN) << "Memcached: could not get tls ticket; disable tls ticket";

    tls_ticket_key_memcached_fail_count_ = 0;

    set_ticket_keys(nullptr);
    set_ticket_keys_to_worker(nullptr);
  }

  LOG(WARN) << "Memcached: tls ticket get failed, schedule next";
  schedule_next_tls_ticket_key_memcached_get(w);
}

void ConnectionHandler::on_tls_ticket_key_get_success(
  const std::shared_ptr<TicketKeys> &ticket_keys, ev_timer *w) {
  LOG(NOTICE) << "Memcached: tls ticket get success";

  tls_ticket_key_memcached_get_retry_count_ = 0;
  tls_ticket_key_memcached_fail_count_ = 0;

  schedule_next_tls_ticket_key_memcached_get(w);

  if (!ticket_keys || ticket_keys->keys.empty()) {
    LOG(WARN) << "Memcached: tls ticket keys are empty; tls ticket disabled";
    set_ticket_keys(nullptr);
    set_ticket_keys_to_worker(nullptr);
    return;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "ticket keys get done";
    LOG(INFO) << 0 << " enc+dec: "
              << util::format_hex(ticket_keys->keys[0].data.name);
    for (size_t i = 1; i < ticket_keys->keys.size(); ++i) {
      auto &key = ticket_keys->keys[i];
      LOG(INFO) << i << " dec: " << util::format_hex(key.data.name);
    }
  }

  set_ticket_keys(ticket_keys);
  set_ticket_keys_to_worker(ticket_keys);
}

void ConnectionHandler::schedule_next_tls_ticket_key_memcached_get(
  ev_timer *w) {
  ev_timer_set(w, get_config()->tls.ticket.memcached.interval, 0.);
  ev_timer_start(loop_, w);
}

SSL_CTX *ConnectionHandler::create_tls_ticket_key_memcached_ssl_ctx() {
  auto config = get_config();
  auto &tlsconf = config->tls;
  auto &memcachedconf = config->tls.ticket.memcached;

  auto ssl_ctx = tls::create_ssl_client_context(
#ifdef HAVE_NEVERBLEED
    nb_,
#endif // HAVE_NEVERBLEED
    tlsconf.cacert, memcachedconf.cert_file, memcachedconf.private_key_file);

  all_ssl_ctx_.push_back(ssl_ctx);
#ifdef ENABLE_HTTP3
  quic_all_ssl_ctx_.push_back(nullptr);
#endif // ENABLE_HTTP3

  return ssl_ctx;
}

#ifdef HAVE_NEVERBLEED
void ConnectionHandler::set_neverbleed(neverbleed_t *nb) { nb_ = nb; }
#endif // HAVE_NEVERBLEED

void ConnectionHandler::handle_serial_event() {
  std::vector<SerialEvent> q;
  {
    std::lock_guard<std::mutex> g(serial_event_mu_);
    q.swap(serial_events_);
  }

  for (auto &sev : q) {
    switch (sev.type) {
    case SerialEventType::REPLACE_DOWNSTREAM:
      // Mmake sure that none of worker uses
      // get_config()->conn.downstream
      mod_config()->conn.downstream = sev.downstreamconf;

      if (single_worker_) {
        single_worker_->replace_downstream_config(sev.downstreamconf);

        break;
      }

      worker_replace_downstream(sev.downstreamconf);

      break;
    default:
      break;
    }
  }
}

void ConnectionHandler::send_replace_downstream(
  const std::shared_ptr<DownstreamConfig> &downstreamconf) {
  send_serial_event(
    SerialEvent(SerialEventType::REPLACE_DOWNSTREAM, downstreamconf));
}

void ConnectionHandler::send_serial_event(SerialEvent ev) {
  {
    std::lock_guard<std::mutex> g(serial_event_mu_);

    serial_events_.push_back(std::move(ev));
  }

  ev_async_send(loop_, &serial_event_asyncev_);
}

SSL_CTX *ConnectionHandler::get_ssl_ctx(size_t idx) const {
  return all_ssl_ctx_[idx];
}

const std::vector<SSL_CTX *> &
ConnectionHandler::get_indexed_ssl_ctx(size_t idx) const {
  return indexed_ssl_ctx_[idx];
}

#ifdef ENABLE_HTTP3
const std::vector<SSL_CTX *> &
ConnectionHandler::get_quic_indexed_ssl_ctx(size_t idx) const {
  return quic_indexed_ssl_ctx_[idx];
}
#endif // ENABLE_HTTP3

void ConnectionHandler::set_enable_acceptor_on_ocsp_completion(bool f) {
  enable_acceptor_on_ocsp_completion_ = f;
}

#ifdef ENABLE_HTTP3
int ConnectionHandler::forward_quic_packet(const UpstreamAddr *faddr,
                                           const Address &remote_addr,
                                           const Address &local_addr,
                                           const ngtcp2_pkt_info &pi,
                                           const WorkerID &wid,
                                           std::span<const uint8_t> data) {
  assert(!get_config()->single_thread);

  auto worker = find_worker(wid);
  if (worker == nullptr) {
    return -1;
  }

  WorkerEvent wev{};
  wev.type = WorkerEventType::QUIC_PKT_FORWARD;
  wev.quic_pkt = std::make_unique<QUICPacket>(faddr->index, remote_addr,
                                              local_addr, pi, data);

  worker->send(std::move(wev));

  return 0;
}

void ConnectionHandler::set_quic_keying_materials(
  std::shared_ptr<QUICKeyingMaterials> qkms) {
  quic_keying_materials_ = std::move(qkms);
}

const std::shared_ptr<QUICKeyingMaterials> &
ConnectionHandler::get_quic_keying_materials() const {
  return quic_keying_materials_;
}

void ConnectionHandler::set_worker_ids(std::vector<WorkerID> worker_ids) {
  worker_ids_ = std::move(worker_ids);
}

namespace {
ssize_t find_worker_index(const std::vector<WorkerID> &worker_ids,
                          const WorkerID &wid) {
  assert(!worker_ids.empty());

  if (wid.server != worker_ids[0].server ||
      wid.worker_process != worker_ids[0].worker_process ||
      wid.thread >= worker_ids.size()) {
    return -1;
  }

  return wid.thread;
}
} // namespace

Worker *ConnectionHandler::find_worker(const WorkerID &wid) const {
  auto idx = find_worker_index(worker_ids_, wid);
  if (idx == -1) {
    return nullptr;
  }

  return workers_[idx].get();
}

QUICLingeringWorkerProcess *
ConnectionHandler::match_quic_lingering_worker_process_worker_id(
  const WorkerID &wid) {
  for (auto &lwps : quic_lingering_worker_processes_) {
    if (find_worker_index(lwps.worker_ids, wid) != -1) {
      return &lwps;
    }
  }

  return nullptr;
}

#  ifdef HAVE_LIBBPF
std::vector<BPFRef> &ConnectionHandler::get_quic_bpf_refs() {
  return quic_bpf_refs_;
}

void ConnectionHandler::unload_bpf_objects() {
  LOG(NOTICE) << "Unloading BPF objects";

  for (auto &ref : quic_bpf_refs_) {
    if (ref.obj == nullptr) {
      continue;
    }

    bpf_object__close(ref.obj);

    ref.obj = nullptr;
  }
}
#  endif // HAVE_LIBBPF

void ConnectionHandler::set_quic_ipc_fd(int fd) { quic_ipc_fd_ = fd; }

void ConnectionHandler::set_quic_lingering_worker_processes(
  const std::vector<QUICLingeringWorkerProcess> &quic_lwps) {
  quic_lingering_worker_processes_ = quic_lwps;
}

int ConnectionHandler::forward_quic_packet_to_lingering_worker_process(
  QUICLingeringWorkerProcess *quic_lwp, const Address &remote_addr,
  const Address &local_addr, const ngtcp2_pkt_info &pi,
  std::span<const uint8_t> data) {
  std::array<uint8_t, 512> header;

  assert(header.size() >= 1 + 1 + 1 + 1 + sizeof(sockaddr_storage) * 2);
  assert(remote_addr.len > 0);
  assert(local_addr.len > 0);

  auto p = header.data();

  *p++ = static_cast<uint8_t>(QUICIPCType::DGRAM_FORWARD);
  *p++ = static_cast<uint8_t>(remote_addr.len - 1);
  p = std::copy_n(reinterpret_cast<const uint8_t *>(&remote_addr.su),
                  remote_addr.len, p);
  *p++ = static_cast<uint8_t>(local_addr.len - 1);
  p = std::copy_n(reinterpret_cast<const uint8_t *>(&local_addr.su),
                  local_addr.len, p);
  *p++ = pi.ecn;

  iovec msg_iov[] = {
    {
      .iov_base = header.data(),
      .iov_len = static_cast<size_t>(p - header.data()),
    },
    {
      .iov_base = const_cast<uint8_t *>(data.data()),
      .iov_len = data.size(),
    },
  };

  msghdr msg{};
  msg.msg_iov = msg_iov;
  msg.msg_iovlen = array_size(msg_iov);

  ssize_t nwrite;

  while ((nwrite = sendmsg(quic_lwp->quic_ipc_fd, &msg, 0)) == -1 &&
         errno == EINTR)
    ;

  if (nwrite == -1) {
    std::array<char, STRERROR_BUFSIZE> errbuf;

    auto error = errno;
    LOG(ERROR) << "Failed to send QUIC IPC message: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    return -1;
  }

  return 0;
}

int ConnectionHandler::quic_ipc_read() {
  std::array<uint8_t, 65536> buf;

  ssize_t nread;

  while ((nread = recv(quic_ipc_fd_, buf.data(), buf.size(), 0)) == -1 &&
         errno == EINTR)
    ;

  if (nread == -1) {
    std::array<char, STRERROR_BUFSIZE> errbuf;

    auto error = errno;
    LOG(ERROR) << "Failed to read data from QUIC IPC channel: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    return -1;
  }

  if (nread == 0) {
    return 0;
  }

  size_t len = 1 + 1 + 1 + 1;

  // Wire format:
  // TYPE(1) REMOTE_ADDRLEN(1) REMOTE_ADDR(N) LOCAL_ADDRLEN(1) LOCAL_ADDR(N)
  // ECN(1) DGRAM_PAYLOAD(N)
  //
  // When encoding, REMOTE_ADDRLEN and LOCAL_ADDRLEN are decremented
  // by 1.
  if (static_cast<size_t>(nread) < len) {
    return 0;
  }

  auto p = buf.data();
  if (*p != static_cast<uint8_t>(QUICIPCType::DGRAM_FORWARD)) {
    LOG(ERROR) << "Unknown QUICIPCType: " << static_cast<uint32_t>(*p);

    return -1;
  }

  ++p;

  auto pkt = std::make_unique<QUICPacket>();

  auto remote_addrlen = static_cast<size_t>(*p++) + 1;
  if (remote_addrlen > sizeof(sockaddr_storage)) {
    LOG(ERROR) << "The length of remote address is too large: "
               << remote_addrlen;

    return -1;
  }

  len += remote_addrlen;

  if (static_cast<size_t>(nread) < len) {
    LOG(ERROR) << "Insufficient QUIC IPC message length";

    return -1;
  }

  pkt->remote_addr.len = remote_addrlen;
  memcpy(&pkt->remote_addr.su, p, remote_addrlen);

  p += remote_addrlen;

  auto local_addrlen = static_cast<size_t>(*p++) + 1;
  if (local_addrlen > sizeof(sockaddr_storage)) {
    LOG(ERROR) << "The length of local address is too large: " << local_addrlen;

    return -1;
  }

  len += local_addrlen;

  if (static_cast<size_t>(nread) < len) {
    LOG(ERROR) << "Insufficient QUIC IPC message length";

    return -1;
  }

  pkt->local_addr.len = local_addrlen;
  memcpy(&pkt->local_addr.su, p, local_addrlen);

  p += local_addrlen;

  pkt->pi.ecn = *p++;

  auto datalen = nread - (p - buf.data());

  pkt->data.assign(p, p + datalen);

  // At the moment, UpstreamAddr index is unknown.
  pkt->upstream_addr_index = static_cast<size_t>(-1);

  ngtcp2_version_cid vc;

  auto rv = ngtcp2_pkt_decode_version_cid(&vc, p, datalen, SHRPX_QUIC_SCIDLEN);
  if (rv < 0) {
    LOG(ERROR) << "ngtcp2_pkt_decode_version_cid: " << ngtcp2_strerror(rv);

    return -1;
  }

  if (vc.dcidlen != SHRPX_QUIC_SCIDLEN) {
    LOG(ERROR) << "DCID length is invalid";
    return -1;
  }

  if (single_worker_) {
    auto faddr = single_worker_->find_quic_upstream_addr(pkt->local_addr);
    if (faddr == nullptr) {
      LOG(ERROR) << "No suitable upstream address found";

      return 0;
    }

    auto quic_conn_handler = single_worker_->get_quic_connection_handler();

    // Ignore return value
    quic_conn_handler->handle_packet(faddr, pkt->remote_addr, pkt->local_addr,
                                     pkt->pi, pkt->data);

    return 0;
  }

  auto &qkm = quic_keying_materials_->keying_materials.front();

  ConnectionID decrypted_dcid;

  if (decrypt_quic_connection_id(decrypted_dcid,
                                 vc.dcid + SHRPX_QUIC_CID_WORKER_ID_OFFSET,
                                 qkm.cid_decryption_ctx) != 0) {
    return -1;
  }

  auto worker = find_worker(decrypted_dcid.worker);
  if (worker == nullptr) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "No worker to match Worker ID";
    }

    return 0;
  }

  WorkerEvent wev{
    .type = WorkerEventType::QUIC_PKT_FORWARD,
    .quic_pkt = std::move(pkt),
  };

  worker->send(std::move(wev));

  return 0;
}
#endif // ENABLE_HTTP3

} // namespace shrpx
