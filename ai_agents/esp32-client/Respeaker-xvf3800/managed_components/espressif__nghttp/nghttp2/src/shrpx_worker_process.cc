/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#include "shrpx_worker_process.h"

#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#include <sys/resource.h>
#include <sys/wait.h>
#include <grp.h>

#include <cinttypes>
#include <cstdlib>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/rand.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/rand.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ev.h>

#include <ares.h>

#include "shrpx_config.h"
#include "shrpx_connection_handler.h"
#include "shrpx_log_config.h"
#include "shrpx_worker.h"
#include "shrpx_accept_handler.h"
#include "shrpx_http2_upstream.h"
#include "shrpx_http2_session.h"
#include "shrpx_memcached_dispatcher.h"
#include "shrpx_memcached_request.h"
#include "shrpx_process.h"
#include "shrpx_tls.h"
#include "shrpx_log.h"
#include "util.h"
#include "app_helper.h"
#include "template.h"
#include "xsi_strerror.h"

using namespace nghttp2;

namespace shrpx {

namespace {
void drop_privileges(
#ifdef HAVE_NEVERBLEED
  neverbleed_t *nb
#endif // HAVE_NEVERBLEED
) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  auto config = get_config();

  if (getuid() == 0 && config->uid != 0) {
#ifdef HAVE_NEVERBLEED
    if (nb) {
      neverbleed_setuidgid(nb, config->user.data(), 1);
    }
#endif // HAVE_NEVERBLEED

    if (initgroups(config->user.data(), config->gid) != 0) {
      auto error = errno;
      LOG(FATAL) << "Could not change supplementary groups: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());
      exit(EXIT_FAILURE);
    }
    if (setgid(config->gid) != 0) {
      auto error = errno;
      LOG(FATAL) << "Could not change gid: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());
      exit(EXIT_FAILURE);
    }
    if (setuid(config->uid) != 0) {
      auto error = errno;
      LOG(FATAL) << "Could not change uid: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());
      exit(EXIT_FAILURE);
    }
    if (setuid(0) != -1) {
      LOG(FATAL) << "Still have root privileges?";
      exit(EXIT_FAILURE);
    }
  }
}
} // namespace

namespace {
void graceful_shutdown(ConnectionHandler *conn_handler) {
  if (conn_handler->get_graceful_shutdown()) {
    return;
  }

  LOG(NOTICE) << "Graceful shutdown signal received";

  conn_handler->set_graceful_shutdown(true);

  // TODO What happens for the connections not established in the
  // kernel?
  conn_handler->accept_pending_connection();
  conn_handler->delete_acceptor();

  conn_handler->graceful_shutdown_worker();

  auto single_worker = conn_handler->get_single_worker();
  if (single_worker) {
    auto worker_stat = single_worker->get_worker_stat();
    if (worker_stat->num_connections == 0 &&
        worker_stat->num_close_waits == 0) {
      ev_break(conn_handler->get_loop());
    }

    return;
  }
}
} // namespace

namespace {
void reopen_log(ConnectionHandler *conn_handler) {
  LOG(NOTICE) << "Reopening log files: worker process (thread main)";

  auto config = get_config();
  auto &loggingconf = config->logging;

  (void)reopen_log_files(loggingconf);
  redirect_stderr_to_errorlog(loggingconf);

  conn_handler->worker_reopen_log_files();
}
} // namespace

namespace {
void ipc_readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn_handler = static_cast<ConnectionHandler *>(w->data);
  std::array<uint8_t, 1024> buf;
  ssize_t nread;
  while ((nread = read(w->fd, buf.data(), buf.size())) == -1 && errno == EINTR)
    ;
  if (nread == -1) {
    auto error = errno;
    LOG(ERROR) << "Failed to read data from ipc channel: errno=" << error;
    return;
  }

  if (nread == 0) {
    // IPC socket closed.  Perform immediate shutdown.
    LOG(FATAL) << "IPC socket is closed.  Perform immediate shutdown.";
    nghttp2_Exit(EXIT_FAILURE);
  }

  for (ssize_t i = 0; i < nread; ++i) {
    switch (buf[i]) {
    case SHRPX_IPC_GRACEFUL_SHUTDOWN:
      graceful_shutdown(conn_handler);
      break;
    case SHRPX_IPC_REOPEN_LOG:
      reopen_log(conn_handler);
      break;
    }
  }
}
} // namespace

#ifdef ENABLE_HTTP3
namespace {
void quic_ipc_readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn_handler = static_cast<ConnectionHandler *>(w->data);

  if (conn_handler->quic_ipc_read() != 0) {
    LOG(ERROR) << "Failed to read data from QUIC IPC channel";

    return;
  }
}
} // namespace
#endif // ENABLE_HTTP3

namespace {
int generate_ticket_key(TicketKey &ticket_key) {
  ticket_key.cipher = get_config()->tls.ticket.cipher;
  ticket_key.hmac = EVP_sha256();
  ticket_key.hmac_keylen = EVP_MD_size(ticket_key.hmac);

  assert(static_cast<size_t>(EVP_CIPHER_key_length(ticket_key.cipher)) <=
         ticket_key.data.enc_key.size());
  assert(ticket_key.hmac_keylen <= ticket_key.data.hmac_key.size());

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "enc_keylen=" << EVP_CIPHER_key_length(ticket_key.cipher)
              << ", hmac_keylen=" << ticket_key.hmac_keylen;
  }

  if (RAND_bytes(reinterpret_cast<unsigned char *>(&ticket_key.data),
                 sizeof(ticket_key.data)) == 0) {
    return -1;
  }

  return 0;
}
} // namespace

namespace {
void renew_ticket_key_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto conn_handler = static_cast<ConnectionHandler *>(w->data);
  const auto &old_ticket_keys = conn_handler->get_ticket_keys();

  auto ticket_keys = std::make_shared<TicketKeys>();
  LOG(NOTICE) << "Renew new ticket keys";

  // If old_ticket_keys is not empty, it should contain at least 2
  // keys: one for encryption, and last one for the next encryption
  // key but decryption only.  The keys in between are old keys and
  // decryption only.  The next key is provided to ensure to mitigate
  // possible problem when one worker encrypt new key, but one worker,
  // which did not take the that key yet, and cannot decrypt it.
  //
  // We keep keys for get_config()->tls_session_timeout seconds.  The
  // default is 12 hours.  Thus the maximum ticket vector size is 12.
  if (old_ticket_keys) {
    auto &old_keys = old_ticket_keys->keys;
    auto &new_keys = ticket_keys->keys;

    assert(!old_keys.empty());

    auto max_tickets =
      static_cast<size_t>(std::chrono::duration_cast<std::chrono::hours>(
                            get_config()->tls.session_timeout)
                            .count());

    new_keys.resize(std::min(max_tickets, old_keys.size() + 1));
    std::copy_n(std::begin(old_keys), new_keys.size() - 1,
                std::begin(new_keys) + 1);
  } else {
    ticket_keys->keys.resize(1);
  }

  auto &new_key = ticket_keys->keys[0];

  if (generate_ticket_key(new_key) != 0) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "failed to generate ticket key";
    }
    conn_handler->set_ticket_keys(nullptr);
    conn_handler->set_ticket_keys_to_worker(nullptr);
    return;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "ticket keys generation done";
    assert(ticket_keys->keys.size() >= 1);
    LOG(INFO) << 0 << " enc+dec: "
              << util::format_hex(ticket_keys->keys[0].data.name);
    for (size_t i = 1; i < ticket_keys->keys.size(); ++i) {
      auto &key = ticket_keys->keys[i];
      LOG(INFO) << i << " dec: " << util::format_hex(key.data.name);
    }
  }

  conn_handler->set_ticket_keys(ticket_keys);
  conn_handler->set_ticket_keys_to_worker(ticket_keys);
}
} // namespace

namespace {
void memcached_get_ticket_key_cb(struct ev_loop *loop, ev_timer *w,
                                 int revents) {
  auto conn_handler = static_cast<ConnectionHandler *>(w->data);
  auto dispatcher = conn_handler->get_tls_ticket_key_memcached_dispatcher();

  auto req = std::make_unique<MemcachedRequest>();
  req->key = "nghttpx:tls-ticket-key";
  req->op = MemcachedOp::GET;
  req->cb = [conn_handler, w](MemcachedRequest *req, MemcachedResult res) {
    switch (res.status_code) {
    case MemcachedStatusCode::NO_ERROR:
      break;
    case MemcachedStatusCode::EXT_NETWORK_ERROR:
      conn_handler->on_tls_ticket_key_network_error(w);
      return;
    default:
      conn_handler->on_tls_ticket_key_not_found(w);
      return;
    }

    // |version (4bytes)|len (2bytes)|key (variable length)|...
    // (len, key) pairs are repeated as necessary.

    auto &value = res.value;
    if (value.size() < 4) {
      LOG(WARN) << "Memcached: tls ticket key value is too small: got "
                << value.size();
      conn_handler->on_tls_ticket_key_not_found(w);
      return;
    }
    auto p = value.data();
    auto version = util::get_uint32(p);
    // Currently supported version is 1.
    if (version != 1) {
      LOG(WARN) << "Memcached: tls ticket key version: want 1, got " << version;
      conn_handler->on_tls_ticket_key_not_found(w);
      return;
    }

    auto end = p + value.size();
    p += 4;

    auto &ticketconf = get_config()->tls.ticket;

    size_t expectedlen;
    size_t enc_keylen;
    size_t hmac_keylen;
    if (ticketconf.cipher == EVP_aes_128_cbc()) {
      expectedlen = 48;
      enc_keylen = 16;
      hmac_keylen = 16;
    } else if (ticketconf.cipher == EVP_aes_256_cbc()) {
      expectedlen = 80;
      enc_keylen = 32;
      hmac_keylen = 32;
    } else {
      return;
    }

    auto ticket_keys = std::make_shared<TicketKeys>();

    for (; p != end;) {
      if (end - p < 2) {
        LOG(WARN) << "Memcached: tls ticket key data is too small";
        conn_handler->on_tls_ticket_key_not_found(w);
        return;
      }
      auto len = util::get_uint16(p);
      p += 2;
      if (len != expectedlen) {
        LOG(WARN) << "Memcached: wrong tls ticket key size: want "
                  << expectedlen << ", got " << len;
        conn_handler->on_tls_ticket_key_not_found(w);
        return;
      }
      if (p + len > end) {
        LOG(WARN) << "Memcached: too short tls ticket key payload: want " << len
                  << ", got " << (end - p);
        conn_handler->on_tls_ticket_key_not_found(w);
        return;
      }
      auto key = TicketKey();
      key.cipher = ticketconf.cipher;
      key.hmac = EVP_sha256();
      key.hmac_keylen = hmac_keylen;

      std::copy_n(p, key.data.name.size(), std::begin(key.data.name));
      p += key.data.name.size();

      std::copy_n(p, enc_keylen, std::begin(key.data.enc_key));
      p += enc_keylen;

      std::copy_n(p, hmac_keylen, std::begin(key.data.hmac_key));
      p += hmac_keylen;

      ticket_keys->keys.push_back(std::move(key));
    }

    conn_handler->on_tls_ticket_key_get_success(ticket_keys, w);
  };

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Memcached: tls ticket key get request sent";
  }

  dispatcher->add_request(std::move(req));
}

} // namespace

#ifdef HAVE_NEVERBLEED
namespace {
void nb_child_cb(struct ev_loop *loop, ev_child *w, int revents) {
  log_chld(w->rpid, w->rstatus, "neverbleed process");

  ev_child_stop(loop, w);

  LOG(FATAL) << "neverbleed process exited; aborting now";

  nghttp2_Exit(EXIT_FAILURE);
}
} // namespace
#endif // HAVE_NEVERBLEED

namespace {
int send_ready_event(int ready_ipc_fd) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  auto pid = getpid();
  ssize_t nwrite;

  while ((nwrite = write(ready_ipc_fd, &pid, sizeof(pid))) == -1 &&
         errno == EINTR)
    ;

  if (nwrite < 0) {
    auto error = errno;

    LOG(ERROR) << "Writing PID to ready IPC channel failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    return -1;
  }

  return 0;
}
} // namespace

int worker_process_event_loop(WorkerProcessConfig *wpconf) {
  int rv;
  std::array<char, STRERROR_BUFSIZE> errbuf;
  (void)errbuf;

  auto config = get_config();

  if (reopen_log_files(config->logging) != 0) {
    LOG(FATAL) << "Failed to open log file";
    return -1;
  }

  rv = ares_library_init(ARES_LIB_INIT_ALL);
  if (rv != 0) {
    LOG(FATAL) << "ares_library_init failed: " << ares_strerror(rv);
    return -1;
  }

  auto loop = EV_DEFAULT;

  auto gen = util::make_mt19937();

#ifdef HAVE_NEVERBLEED
  std::array<char, NEVERBLEED_ERRBUF_SIZE> nb_errbuf;
  auto nb = std::make_unique<neverbleed_t>();
  if (neverbleed_init(nb.get(), nb_errbuf.data()) != 0) {
    LOG(FATAL) << "neverbleed_init failed: " << nb_errbuf.data();
    return -1;
  }

  LOG(NOTICE) << "neverbleed process [" << nb->daemon_pid << "] spawned";

  ev_child nb_childev;

  ev_child_init(&nb_childev, nb_child_cb, nb->daemon_pid, 0);
  nb_childev.data = nullptr;
  ev_child_start(loop, &nb_childev);
#endif // HAVE_NEVERBLEED

  auto conn_handler = std::make_unique<ConnectionHandler>(loop, gen);

#ifdef HAVE_NEVERBLEED
  conn_handler->set_neverbleed(nb.get());
#endif // HAVE_NEVERBLEED

#ifdef ENABLE_HTTP3
  conn_handler->set_quic_ipc_fd(wpconf->quic_ipc_fd);
  conn_handler->set_quic_lingering_worker_processes(
    wpconf->quic_lingering_worker_processes);
#endif // ENABLE_HTTP3

  for (auto &addr : config->conn.listener.addrs) {
    conn_handler->add_acceptor(
      std::make_unique<AcceptHandler>(&addr, conn_handler.get()));
  }

  MemchunkPool mcpool;

  ev_timer renew_ticket_key_timer;
  if (tls::upstream_tls_enabled(config->conn)) {
    auto &ticketconf = config->tls.ticket;
    auto &memcachedconf = ticketconf.memcached;

    if (!memcachedconf.host.empty()) {
      SSL_CTX *ssl_ctx = nullptr;

      if (memcachedconf.tls) {
        ssl_ctx = conn_handler->create_tls_ticket_key_memcached_ssl_ctx();
      }

      conn_handler->set_tls_ticket_key_memcached_dispatcher(
        std::make_unique<MemcachedDispatcher>(
          &ticketconf.memcached.addr, loop, ssl_ctx,
          StringRef{memcachedconf.host}, &mcpool, gen));

      ev_timer_init(&renew_ticket_key_timer, memcached_get_ticket_key_cb, 0.,
                    0.);
      renew_ticket_key_timer.data = conn_handler.get();
      // Get first ticket keys.
      memcached_get_ticket_key_cb(loop, &renew_ticket_key_timer, 0);
    } else {
      bool auto_tls_ticket_key = true;
      if (!ticketconf.files.empty()) {
        if (!ticketconf.cipher_given) {
          LOG(WARN)
            << "It is strongly recommended to specify "
               "--tls-ticket-key-cipher=aes-128-cbc (or "
               "tls-ticket-key-cipher=aes-128-cbc in configuration file) "
               "when --tls-ticket-key-file is used for the smooth "
               "transition when the default value of --tls-ticket-key-cipher "
               "becomes aes-256-cbc";
        }
        auto ticket_keys = read_tls_ticket_key_file(
          ticketconf.files, ticketconf.cipher, EVP_sha256());
        if (!ticket_keys) {
          LOG(WARN) << "Use internal session ticket key generator";
        } else {
          conn_handler->set_ticket_keys(std::move(ticket_keys));
          auto_tls_ticket_key = false;
        }
      }
      if (auto_tls_ticket_key) {
        // Generate new ticket key every 1hr.
        ev_timer_init(&renew_ticket_key_timer, renew_ticket_key_cb, 0., 1_h);
        renew_ticket_key_timer.data = conn_handler.get();
        ev_timer_again(loop, &renew_ticket_key_timer);

        // Generate first session ticket key before running workers.
        renew_ticket_key_cb(loop, &renew_ticket_key_timer, 0);
      }
    }
  }

#ifdef ENABLE_HTTP3
  auto &quicconf = config->quic;

  std::shared_ptr<QUICKeyingMaterials> qkms;

  if (!quicconf.upstream.secret_file.empty()) {
    qkms = read_quic_secret_file(quicconf.upstream.secret_file);
    if (!qkms) {
      LOG(WARN) << "Use QUIC keying materials generated internally";
    }
  }

  if (!qkms) {
    qkms = std::make_shared<QUICKeyingMaterials>();
    qkms->keying_materials.resize(1);

    auto &qkm = qkms->keying_materials.front();

    if (RAND_bytes(qkm.reserved.data(), qkm.reserved.size()) != 1) {
      LOG(ERROR) << "Failed to generate QUIC secret reserved data";
      return -1;
    }

    if (RAND_bytes(qkm.secret.data(), qkm.secret.size()) != 1) {
      LOG(ERROR) << "Failed to generate QUIC secret";
      return -1;
    }

    if (RAND_bytes(qkm.salt.data(), qkm.salt.size()) != 1) {
      LOG(ERROR) << "Failed to generate QUIC salt";
      return -1;
    }
  }

  for (auto &qkm : qkms->keying_materials) {
    if (generate_quic_connection_id_encryption_key(qkm.cid_encryption_key,
                                                   qkm.secret, qkm.salt) != 0) {
      LOG(ERROR) << "Failed to generate QUIC Connection ID encryption key";
      return -1;
    }

    qkm.cid_encryption_ctx = EVP_CIPHER_CTX_new();
    if (!EVP_EncryptInit_ex(qkm.cid_encryption_ctx, EVP_aes_128_ecb(), nullptr,
                            qkm.cid_encryption_key.data(), nullptr)) {
      LOG(ERROR)
        << "Failed to initialize QUIC Connection ID encryption context";
      return -1;
    }

    EVP_CIPHER_CTX_set_padding(qkm.cid_encryption_ctx, 0);

    qkm.cid_decryption_ctx = EVP_CIPHER_CTX_new();
    if (!EVP_DecryptInit_ex(qkm.cid_decryption_ctx, EVP_aes_128_ecb(), nullptr,
                            qkm.cid_encryption_key.data(), nullptr)) {
      LOG(ERROR)
        << "Failed to initialize QUIC Connection ID decryption context";
      return -1;
    }

    EVP_CIPHER_CTX_set_padding(qkm.cid_decryption_ctx, 0);
  }

  conn_handler->set_quic_keying_materials(std::move(qkms));

  conn_handler->set_worker_ids(wpconf->worker_ids);
  conn_handler->set_quic_lingering_worker_processes(
    wpconf->quic_lingering_worker_processes);
#endif // ENABLE_HTTP3

  if (config->single_thread) {
    rv = conn_handler->create_single_worker();
    if (rv != 0) {
      return -1;
    }
  } else {
#ifndef NOTHREADS
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);

    rv = pthread_sigmask(SIG_BLOCK, &set, nullptr);
    if (rv != 0) {
      LOG(ERROR) << "Blocking SIGCHLD failed: "
                 << xsi_strerror(rv, errbuf.data(), errbuf.size());
      return -1;
    }
#endif // !NOTHREADS

    rv = conn_handler->create_worker_thread(config->num_worker);
    if (rv != 0) {
      return -1;
    }

#ifndef NOTHREADS
    rv = pthread_sigmask(SIG_UNBLOCK, &set, nullptr);
    if (rv != 0) {
      LOG(ERROR) << "Unblocking SIGCHLD failed: "
                 << xsi_strerror(rv, errbuf.data(), errbuf.size());
      return -1;
    }
#endif // !NOTHREADS
  }

#if defined(ENABLE_HTTP3) && defined(HAVE_LIBBPF)
  conn_handler->unload_bpf_objects();
#endif // defined(ENABLE_HTTP3) && defined(HAVE_LIBBPF)

  drop_privileges(
#ifdef HAVE_NEVERBLEED
    nb.get()
#endif // HAVE_NEVERBLEED
  );

  ev_io ipcev;
  ev_io_init(&ipcev, ipc_readcb, wpconf->ipc_fd, EV_READ);
  ipcev.data = conn_handler.get();
  ev_io_start(loop, &ipcev);

#ifdef ENABLE_HTTP3
  ev_io quic_ipcev;
  ev_io_init(&quic_ipcev, quic_ipc_readcb, wpconf->quic_ipc_fd, EV_READ);
  quic_ipcev.data = conn_handler.get();
  ev_io_start(loop, &quic_ipcev);
#endif // ENABLE_HTTP3

  if (tls::upstream_tls_enabled(config->conn) && !config->tls.ocsp.disabled) {
    if (config->tls.ocsp.startup) {
      conn_handler->set_enable_acceptor_on_ocsp_completion(true);
      conn_handler->disable_acceptor();
    }

    conn_handler->proceed_next_cert_ocsp();
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Entering event loop";
  }

  if (send_ready_event(wpconf->ready_ipc_fd) != 0) {
    return -1;
  }

  ev_run(loop, 0);

  conn_handler->cancel_ocsp_update();

  // Destroy SSL_CTX held in conn_handler before killing neverbleed
  // daemon.  Otherwise priv_rsa_finish yields "write error" and
  // worker process aborts.
  conn_handler.reset();

#ifdef HAVE_NEVERBLEED
  assert(nb->daemon_pid > 0);

  rv = kill(nb->daemon_pid, SIGTERM);
  if (rv != 0) {
    auto error = errno;
    LOG(ERROR) << "Could not send signal to neverbleed daemon: errno=" << error;
  }

  while ((rv = waitpid(nb->daemon_pid, nullptr, 0)) == -1 && errno == EINTR)
    ;
  if (rv == -1) {
    auto error = errno;
    LOG(ERROR) << "Error occurred while we were waiting for the completion "
                  "of neverbleed process: errno="
               << error;
  }
#endif // HAVE_NEVERBLEED

  ares_library_cleanup();

  return 0;
}

} // namespace shrpx
