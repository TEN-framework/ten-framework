/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#include "h2load.h"

#include <getopt.h>
#include <signal.h>
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif // HAVE_NETINET_IN_H
#include <netinet/tcp.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif // HAVE_FCNTL_H
#include <sys/mman.h>
#include <netinet/udp.h>

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <future>
#include <random>
#include <string_view>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/err.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/err.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#ifdef ENABLE_HTTP3
#  ifdef HAVE_LIBNGTCP2_CRYPTO_QUICTLS
#    include <ngtcp2/ngtcp2_crypto_quictls.h>
#  endif // HAVE_LIBNGTCP2_CRYPTO_QUICTLS
#  ifdef HAVE_LIBNGTCP2_CRYPTO_BORINGSSL
#    include <ngtcp2/ngtcp2_crypto_boringssl.h>
#  endif // HAVE_LIBNGTCP2_CRYPTO_BORINGSSL
#  ifdef HAVE_LIBNGTCP2_CRYPTO_WOLFSSL
#    include <ngtcp2/ngtcp2_crypto_wolfssl.h>
#  endif // HAVE_LIBNGTCP2_CRYPTO_WOLFSSL
#endif   // ENABLE_HTTP3

#include "urlparse.h"

#include "h2load_http1_session.h"
#include "h2load_http2_session.h"
#ifdef ENABLE_HTTP3
#  include "h2load_http3_session.h"
#  include "h2load_quic.h"
#endif // ENABLE_HTTP3
#include "tls.h"
#include "http2.h"
#include "util.h"
#include "template.h"

#ifndef O_BINARY
#  define O_BINARY (0)
#endif // O_BINARY

using namespace nghttp2;

namespace h2load {

namespace {
bool recorded(const std::chrono::steady_clock::time_point &t) {
  return std::chrono::steady_clock::duration::zero() != t.time_since_epoch();
}
} // namespace

Config::Config()
  : ciphers(tls::DEFAULT_CIPHER_LIST),
    tls13_ciphers("TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_"
                  "CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256"),
    groups("X25519:P-256:P-384:P-521"),
    data_length(-1),
    data(nullptr),
    addrs(nullptr),
    nreqs(1),
    nclients(1),
    nthreads(1),
    max_concurrent_streams(1),
    window_bits(30),
    connection_window_bits(30),
    max_frame_size(16_k),
    rate(0),
    rate_period(1.0),
    duration(0.0),
    warm_up_time(0.0),
    conn_active_timeout(0.),
    conn_inactivity_timeout(0.),
    no_tls_proto(PROTO_HTTP2),
    header_table_size(4_k),
    encoder_header_table_size(4_k),
    data_fd(-1),
    log_fd(-1),
    qlog_file_base(),
    port(0),
    default_port(0),
    connect_to_port(0),
    verbose(false),
    timing_script(false),
    base_uri_unix(false),
    unix_addr{},
    rps(0.),
    no_udp_gso(false),
    max_udp_payload_size(0),
    ktls(false) {}

Config::~Config() {
  if (addrs) {
    if (base_uri_unix) {
      delete addrs;
    } else {
      freeaddrinfo(addrs);
    }
  }

  if (data_fd != -1) {
    close(data_fd);
  }
}

bool Config::is_rate_mode() const { return (this->rate != 0); }
bool Config::is_timing_based_mode() const { return (this->duration > 0); }
bool Config::has_base_uri() const { return (!this->base_uri.empty()); }
bool Config::rps_enabled() const { return this->rps > 0.0; }
bool Config::is_quic() const {
#ifdef ENABLE_HTTP3
  return !alpn_list.empty() &&
         (alpn_list[0] == NGHTTP3_ALPN_H3 || alpn_list[0] == "\x5h3-29");
#else  // !ENABLE_HTTP3
  return false;
#endif // !ENABLE_HTTP3
}
Config config;

namespace {
constexpr size_t MAX_SAMPLES = 1000000;
} // namespace

Stats::Stats(size_t req_todo, size_t nclients)
  : req_todo(req_todo),
    req_started(0),
    req_done(0),
    req_success(0),
    req_status_success(0),
    req_failed(0),
    req_error(0),
    req_timedout(0),
    bytes_total(0),
    bytes_head(0),
    bytes_head_decomp(0),
    bytes_body(0),
    status(),
    udp_dgram_recv(0),
    udp_dgram_sent(0) {}

Stream::Stream() : req_stat{}, status_success(-1) {}

namespace {
std::random_device rd;
} // namespace

namespace {
std::mt19937 gen(rd());
} // namespace

namespace {
void sampling_init(Sampling &smp, size_t max_samples) {
  smp.n = 0;
  smp.max_samples = max_samples;
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto client = static_cast<Client *>(w->data);
  client->restart_timeout();
  auto rv = client->do_write();
  if (rv == Client::ERR_CONNECT_FAIL) {
    client->disconnect();
    // Try next address
    client->current_addr = nullptr;
    rv = client->connect();
    if (rv != 0) {
      client->fail();
      client->worker->free_client(client);
      delete client;
      return;
    }
    return;
  }
  if (rv != 0) {
    client->fail();
    client->worker->free_client(client);
    delete client;
  }
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto client = static_cast<Client *>(w->data);
  client->restart_timeout();
  if (client->do_read() != 0) {
    if (client->try_again_or_fail() == 0) {
      return;
    }
    client->worker->free_client(client);
    delete client;
    return;
  }
  client->signal_write();
}
} // namespace

namespace {
// Called every rate_period when rate mode is being used
void rate_period_timeout_w_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto worker = static_cast<Worker *>(w->data);
  auto nclients_per_second = worker->rate;
  auto conns_remaining = worker->nclients - worker->nconns_made;
  auto nclients = std::min(nclients_per_second, conns_remaining);

  for (size_t i = 0; i < nclients; ++i) {
    auto req_todo = worker->nreqs_per_client;
    if (worker->nreqs_rem > 0) {
      ++req_todo;
      --worker->nreqs_rem;
    }
    auto client =
      std::make_unique<Client>(worker->next_client_id++, worker, req_todo);

    ++worker->nconns_made;

    if (client->connect() != 0) {
      std::cerr << "client could not connect to host" << std::endl;
      client->fail();
    } else {
      if (worker->config->is_timing_based_mode()) {
        worker->clients.push_back(client.release());
      } else {
        client.release();
      }
    }
    worker->report_rate_progress();
  }
  if (!worker->config->is_timing_based_mode()) {
    if (worker->nconns_made >= worker->nclients) {
      ev_timer_stop(worker->loop, w);
    }
  } else {
    // To check whether all created clients are pushed correctly
    assert(worker->nclients == worker->clients.size());
  }
}
} // namespace

namespace {
// Called when the duration for infinite number of requests are over
void duration_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto worker = static_cast<Worker *>(w->data);

  worker->current_phase = Phase::DURATION_OVER;

  std::cout << "Main benchmark duration is over for thread #" << worker->id
            << ". Stopping all clients." << std::endl;
  worker->stop_all_clients();
  std::cout << "Stopped all clients for thread #" << worker->id << std::endl;
}
} // namespace

namespace {
// Called when the warmup duration for infinite number of requests are over
void warmup_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto worker = static_cast<Worker *>(w->data);

  std::cout << "Warm-up phase is over for thread #" << worker->id << "."
            << std::endl;
  std::cout << "Main benchmark duration is started for thread #" << worker->id
            << "." << std::endl;
  assert(worker->stats.req_started == 0);
  assert(worker->stats.req_done == 0);

  for (auto client : worker->clients) {
    if (client) {
      assert(client->req_todo == 0);
      assert(client->req_left == 1);
      assert(client->req_inflight == 0);
      assert(client->req_started == 0);
      assert(client->req_done == 0);

      client->record_client_start_time();
      client->clear_connect_times();
      client->record_connect_start_time();
    }
  }

  worker->current_phase = Phase::MAIN_DURATION;

  ev_timer_start(worker->loop, &worker->duration_watcher);
}
} // namespace

namespace {
void rps_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto client = static_cast<Client *>(w->data);
  auto &session = client->session;

  assert(!config.timing_script);

  if (client->req_left == 0) {
    ev_timer_stop(loop, w);
    return;
  }

  auto now = std::chrono::steady_clock::now();
  auto d = now - client->rps_duration_started;
  auto n = static_cast<size_t>(
    round(std::chrono::duration<double>(d).count() * config.rps));
  client->rps_req_pending += n;
  client->rps_duration_started +=
    util::duration_from(static_cast<double>(n) / config.rps);

  if (client->rps_req_pending == 0) {
    return;
  }

  auto nreq = session->max_concurrent_streams() - client->rps_req_inflight;
  if (nreq == 0) {
    return;
  }

  nreq = config.is_timing_based_mode() ? std::max(nreq, client->req_left)
                                       : std::min(nreq, client->req_left);
  nreq = std::min(nreq, client->rps_req_pending);

  client->rps_req_inflight += nreq;
  client->rps_req_pending -= nreq;

  for (; nreq > 0; --nreq) {
    if (client->submit_request() != 0) {
      client->process_request_failure();
      break;
    }
  }

  client->signal_write();
}
} // namespace

namespace {
// Called when an a connection has been inactive for a set period of time
// or a fixed amount of time after all requests have been made on a
// connection
void conn_timeout_cb(EV_P_ ev_timer *w, int revents) {
  auto client = static_cast<Client *>(w->data);

  ev_timer_stop(client->worker->loop, &client->conn_inactivity_watcher);
  ev_timer_stop(client->worker->loop, &client->conn_active_watcher);

  if (util::check_socket_connected(client->fd)) {
    client->timeout();
  }
}
} // namespace

namespace {
bool check_stop_client_request_timeout(Client *client, ev_timer *w) {
  if (client->req_left == 0) {
    // no more requests to make, stop timer
    ev_timer_stop(client->worker->loop, w);
    return true;
  }

  return false;
}
} // namespace

namespace {
void client_request_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto client = static_cast<Client *>(w->data);

  if (client->streams.size() >= config.max_concurrent_streams) {
    ev_timer_stop(client->worker->loop, w);
    return;
  }

  if (client->submit_request() != 0) {
    ev_timer_stop(client->worker->loop, w);
    client->process_request_failure();
    return;
  }
  client->signal_write();

  if (check_stop_client_request_timeout(client, w)) {
    return;
  }

  auto duration =
    config.timings[client->reqidx] - config.timings[client->reqidx - 1];

  while (duration < std::chrono::duration<double>(1e-9)) {
    if (client->submit_request() != 0) {
      ev_timer_stop(client->worker->loop, w);
      client->process_request_failure();
      return;
    }
    client->signal_write();
    if (check_stop_client_request_timeout(client, w)) {
      return;
    }

    duration =
      config.timings[client->reqidx] - config.timings[client->reqidx - 1];
  }

  client->request_timeout_watcher.repeat = util::ev_tstamp_from(duration);
  ev_timer_again(client->worker->loop, &client->request_timeout_watcher);
}
} // namespace

Client::Client(uint32_t id, Worker *worker, size_t req_todo)
  : wb(&worker->mcpool),
    cstat{},
    worker(worker),
    ssl(nullptr),
#ifdef ENABLE_HTTP3
    quic{},
#endif // ENABLE_HTTP3
    next_addr(config.addrs),
    current_addr(nullptr),
    reqidx(0),
    state(CLIENT_IDLE),
    req_todo(req_todo),
    req_left(req_todo),
    req_inflight(0),
    req_started(0),
    req_done(0),
    id(id),
    fd(-1),
    local_addr{},
    new_connection_requested(false),
    final(false),
    rps_req_pending(0),
    rps_req_inflight(0) {
  if (req_todo == 0) { // this means infinite number of requests are to be made
    // This ensures that number of requests are unbounded
    // Just a positive number is fine, we chose the first positive number
    req_left = 1;
  }
  ev_io_init(&wev, writecb, 0, EV_WRITE);
  ev_io_init(&rev, readcb, 0, EV_READ);

  wev.data = this;
  rev.data = this;

  ev_timer_init(&conn_inactivity_watcher, conn_timeout_cb, 0.,
                worker->config->conn_inactivity_timeout);
  conn_inactivity_watcher.data = this;

  ev_timer_init(&conn_active_watcher, conn_timeout_cb,
                worker->config->conn_active_timeout, 0.);
  conn_active_watcher.data = this;

  ev_timer_init(&request_timeout_watcher, client_request_timeout_cb, 0., 0.);
  request_timeout_watcher.data = this;

  ev_timer_init(&rps_watcher, rps_cb, 0., 0.);
  rps_watcher.data = this;

#ifdef ENABLE_HTTP3
  ev_timer_init(&quic.pkt_timer, quic_pkt_timeout_cb, 0., 0.);
  quic.pkt_timer.data = this;

  if (config.is_quic()) {
    quic.tx.data = std::make_unique<uint8_t[]>(64_k);
  }

  ngtcp2_ccerr_default(&quic.last_error);
#endif // ENABLE_HTTP3
}

Client::~Client() {
  disconnect();

#ifdef ENABLE_HTTP3
  if (config.is_quic()) {
    quic_free();
  }
#endif // ENABLE_HTTP3

  if (ssl) {
    SSL_free(ssl);
  }

  worker->sample_client_stat(&cstat);
  ++worker->client_smp.n;
}

int Client::do_read() { return readfn(*this); }
int Client::do_write() { return writefn(*this); }

int Client::make_socket(addrinfo *addr) {
  int rv;

  if (config.is_quic()) {
#ifdef ENABLE_HTTP3
    fd = util::create_nonblock_udp_socket(addr->ai_family);
    if (fd == -1) {
      return -1;
    }

#  ifdef UDP_GRO
    int val = 1;
    if (setsockopt(fd, IPPROTO_UDP, UDP_GRO, &val, sizeof(val)) != 0) {
      std::cerr << "setsockopt UDP_GRO failed" << std::endl;
      return -1;
    }
#  endif // UDP_GRO

    rv = util::bind_any_addr_udp(fd, addr->ai_family);
    if (rv != 0) {
      close(fd);
      fd = -1;
      return -1;
    }

    socklen_t addrlen = sizeof(local_addr.su.storage);
    rv = getsockname(fd, &local_addr.su.sa, &addrlen);
    if (rv == -1) {
      return -1;
    }
    local_addr.len = addrlen;

    if (quic_init(&local_addr.su.sa, local_addr.len, addr->ai_addr,
                  addr->ai_addrlen) != 0) {
      std::cerr << "quic_init failed" << std::endl;
      return -1;
    }
#endif // ENABLE_HTTP3
  } else {
    fd = util::create_nonblock_socket(addr->ai_family);
    if (fd == -1) {
      return -1;
    }
    if (config.scheme == "https") {
      if (!ssl) {
        ssl = SSL_new(worker->ssl_ctx);
      }

      SSL_set_connect_state(ssl);
    }
  }

  if (ssl) {
    if (!config.sni.empty()) {
      SSL_set_tlsext_host_name(ssl, config.sni.c_str());
    } else if (!util::numeric_host(config.host.c_str())) {
      SSL_set_tlsext_host_name(ssl, config.host.c_str());
    }
  }

  if (config.is_quic()) {
    return 0;
  }

  rv = ::connect(fd, addr->ai_addr, addr->ai_addrlen);
  if (rv != 0 && errno != EINPROGRESS) {
    if (ssl) {
      SSL_free(ssl);
      ssl = nullptr;
    }
    close(fd);
    fd = -1;
    return -1;
  }
  return 0;
}

int Client::connect() {
  int rv;

  if (!worker->config->is_timing_based_mode() ||
      worker->current_phase == Phase::MAIN_DURATION) {
    record_client_start_time();
    clear_connect_times();
    record_connect_start_time();
  } else if (worker->current_phase == Phase::INITIAL_IDLE) {
    worker->current_phase = Phase::WARM_UP;
    std::cout << "Warm-up started for thread #" << worker->id << "."
              << std::endl;
    ev_timer_start(worker->loop, &worker->warmup_watcher);
  }

  if (worker->config->conn_inactivity_timeout > 0.) {
    ev_timer_again(worker->loop, &conn_inactivity_watcher);
  }

  if (current_addr) {
    rv = make_socket(current_addr);
    if (rv == -1) {
      return -1;
    }
  } else {
    addrinfo *addr = nullptr;
    while (next_addr) {
      addr = next_addr;
      next_addr = next_addr->ai_next;
      rv = make_socket(addr);
      if (rv == 0) {
        break;
      }
    }

    if (fd == -1) {
      return -1;
    }

    assert(addr);

    current_addr = addr;
  }

  ev_io_set(&rev, fd, EV_READ);
  ev_io_set(&wev, fd, EV_WRITE);

  ev_io_start(worker->loop, &wev);

  if (config.is_quic()) {
#ifdef ENABLE_HTTP3
    ev_io_start(worker->loop, &rev);

    readfn = &Client::read_quic;
    writefn = &Client::write_quic;
#endif // ENABLE_HTTP3
  } else {
    writefn = &Client::connected;
  }

  return 0;
}

void Client::timeout() {
  process_timedout_streams();

  disconnect();
}

void Client::restart_timeout() {
  if (worker->config->conn_inactivity_timeout > 0.) {
    ev_timer_again(worker->loop, &conn_inactivity_watcher);
  }
}

int Client::try_again_or_fail() {
  disconnect();

  if (new_connection_requested) {
    new_connection_requested = false;

    if (req_left) {
      if (worker->current_phase == Phase::MAIN_DURATION) {
        // At the moment, we don't have a facility to re-start request
        // already in in-flight.  Make them fail.
        worker->stats.req_failed += req_inflight;
        worker->stats.req_error += req_inflight;

        req_inflight = 0;
      } else if (worker->current_phase == Phase::DURATION_OVER) {
        // fix a race condition when h2load is sending connection: close over h1
        // prevents new clients from spawning after the test should have ended.
        return -1;
      }

      // Keep using current address
      if (connect() == 0) {
        return 0;
      }
      std::cerr << "client could not connect to host" << std::endl;
    }
  }

  process_abandoned_streams();

  return -1;
}

void Client::fail() {
  disconnect();

  process_abandoned_streams();
}

void Client::disconnect() {
  record_client_end_time();

#ifdef ENABLE_HTTP3
  if (config.is_quic()) {
    quic_close_connection();
  }
#endif // ENABLE_HTTP3

#ifdef ENABLE_HTTP3
  ev_timer_stop(worker->loop, &quic.pkt_timer);
#endif // ENABLE_HTTP3
  ev_timer_stop(worker->loop, &conn_inactivity_watcher);
  ev_timer_stop(worker->loop, &conn_active_watcher);
  ev_timer_stop(worker->loop, &rps_watcher);
  ev_timer_stop(worker->loop, &request_timeout_watcher);
  streams.clear();
  session.reset();
  wb.reset();
  state = CLIENT_IDLE;
  ev_io_stop(worker->loop, &wev);
  ev_io_stop(worker->loop, &rev);
  if (ssl) {
    if (config.is_quic()) {
      SSL_free(ssl);
      ssl = nullptr;
    } else {
      SSL_set_shutdown(ssl, SSL_get_shutdown(ssl) | SSL_RECEIVED_SHUTDOWN);
      ERR_clear_error();

      if (SSL_shutdown(ssl) != 1) {
        SSL_free(ssl);
        ssl = nullptr;
      }
    }
  }
  if (fd != -1) {
    shutdown(fd, SHUT_WR);
    close(fd);
    fd = -1;
  }

  final = false;
}

int Client::submit_request() {
  if (session->submit_request() != 0) {
    return -1;
  }

  if (worker->current_phase != Phase::MAIN_DURATION) {
    return 0;
  }

  ++worker->stats.req_started;
  ++req_started;
  ++req_inflight;
  if (!worker->config->is_timing_based_mode()) {
    --req_left;
  }
  // if an active timeout is set and this is the last request to be submitted
  // on this connection, start the active timeout.
  if (worker->config->conn_active_timeout > 0. && req_left == 0) {
    ev_timer_start(worker->loop, &conn_active_watcher);
  }

  return 0;
}

void Client::process_timedout_streams() {
  if (worker->current_phase != Phase::MAIN_DURATION) {
    return;
  }

  for (auto &p : streams) {
    auto &req_stat = p.second.req_stat;
    if (!req_stat.completed) {
      req_stat.stream_close_time = std::chrono::steady_clock::now();
    }
  }

  worker->stats.req_timedout += req_inflight;

  process_abandoned_streams();
}

void Client::process_abandoned_streams() {
  if (worker->current_phase != Phase::MAIN_DURATION) {
    return;
  }

  auto req_abandoned = req_inflight + req_left;

  worker->stats.req_failed += req_abandoned;
  worker->stats.req_error += req_abandoned;

  req_inflight = 0;
  req_left = 0;
}

void Client::process_request_failure() {
  if (worker->current_phase != Phase::MAIN_DURATION) {
    return;
  }

  worker->stats.req_failed += req_left;
  worker->stats.req_error += req_left;

  req_left = 0;

  if (req_inflight == 0) {
    terminate_session();
  }
  std::cout << "Process Request Failure:" << worker->stats.req_failed
            << std::endl;
}

#ifndef NGHTTP2_OPENSSL_IS_BORINGSSL
namespace {
void print_server_tmp_key(SSL *ssl) {
  EVP_PKEY *key;

  if (!SSL_get_server_tmp_key(ssl, &key)) {
    return;
  }

  auto key_del = defer(EVP_PKEY_free, key);

  std::cout << "Server Temp Key: ";

  auto pkey_id = EVP_PKEY_id(key);
  switch (pkey_id) {
  case EVP_PKEY_RSA:
    std::cout << "RSA " << EVP_PKEY_bits(key) << " bits" << std::endl;
    break;
  case EVP_PKEY_DH:
    std::cout << "DH " << EVP_PKEY_bits(key) << " bits" << std::endl;
    break;
  case EVP_PKEY_EC: {
#  if OPENSSL_3_0_0_API
    std::array<char, 64> curve_name;
    const char *cname;
    if (!EVP_PKEY_get_utf8_string_param(key, "group", curve_name.data(),
                                        curve_name.size(), nullptr)) {
      cname = "<unknown>";
    } else {
      cname = curve_name.data();
    }
#  else  // !OPENSSL_3_0_0_API
    auto ec = EVP_PKEY_get1_EC_KEY(key);
    auto ec_del = defer(EC_KEY_free, ec);
    auto nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
    auto cname = EC_curve_nid2nist(nid);
    if (!cname) {
      cname = OBJ_nid2sn(nid);
    }
#  endif // !OPENSSL_3_0_0_API

    std::cout << "ECDH " << cname << " " << EVP_PKEY_bits(key) << " bits"
              << std::endl;
    break;
  }
  default:
    std::cout << OBJ_nid2sn(pkey_id) << " " << EVP_PKEY_bits(key) << " bits"
              << std::endl;
    break;
  }
}
} // namespace
#endif // !NGHTTP2_OPENSSL_IS_BORINGSSL

void Client::report_tls_info() {
  if (worker->id == 0 && !worker->tls_info_report_done) {
    worker->tls_info_report_done = true;
    auto cipher = SSL_get_current_cipher(ssl);
    std::cout << "TLS Protocol: " << tls::get_tls_protocol(ssl) << "\n"
              << "Cipher: " << SSL_CIPHER_get_name(cipher) << std::endl;
#ifndef NGHTTP2_OPENSSL_IS_BORINGSSL
    print_server_tmp_key(ssl);
#endif // !NGHTTP2_OPENSSL_IS_BORINGSSL
  }
}

void Client::report_app_info() {
  if (worker->id == 0 && !worker->app_info_report_done) {
    worker->app_info_report_done = true;
    std::cout << "Application protocol: " << selected_proto << std::endl;
  }
}

void Client::terminate_session() {
#ifdef ENABLE_HTTP3
  if (config.is_quic()) {
    quic.close_requested = true;
  }
#endif // ENABLE_HTTP3
  if (session) {
    session->terminate();
  }
  // http1 session needs writecb to tear down session.
  signal_write();
}

void Client::on_request(int32_t stream_id) { streams[stream_id] = Stream(); }

void Client::on_header(int32_t stream_id, const uint8_t *name, size_t namelen,
                       const uint8_t *value, size_t valuelen) {
  auto itr = streams.find(stream_id);
  if (itr == std::end(streams)) {
    return;
  }
  auto &stream = (*itr).second;

  if (worker->current_phase != Phase::MAIN_DURATION) {
    // If the stream is for warm-up phase, then mark as a success
    // But we do not update the count for 2xx, 3xx, etc status codes
    // Same has been done in on_status_code function
    stream.status_success = 1;
    return;
  }

  if (stream.status_success == -1 && namelen == 7 &&
      ":status"_sr == StringRef{name, namelen}) {
    int status = 0;
    for (size_t i = 0; i < valuelen; ++i) {
      if ('0' <= value[i] && value[i] <= '9') {
        status *= 10;
        status += value[i] - '0';
        if (status > 999) {
          stream.status_success = 0;
          return;
        }
      } else {
        break;
      }
    }

    if (status < 200) {
      return;
    }

    stream.req_stat.status = status;
    if (status >= 200 && status < 300) {
      ++worker->stats.status[2];
      stream.status_success = 1;
    } else if (status < 400) {
      ++worker->stats.status[3];
      stream.status_success = 1;
    } else if (status < 600) {
      ++worker->stats.status[status / 100];
      stream.status_success = 0;
    } else {
      stream.status_success = 0;
    }
  }
}

void Client::on_status_code(int32_t stream_id, uint16_t status) {
  auto itr = streams.find(stream_id);
  if (itr == std::end(streams)) {
    return;
  }
  auto &stream = (*itr).second;

  if (worker->current_phase != Phase::MAIN_DURATION) {
    stream.status_success = 1;
    return;
  }

  stream.req_stat.status = status;
  if (status >= 200 && status < 300) {
    ++worker->stats.status[2];
    stream.status_success = 1;
  } else if (status < 400) {
    ++worker->stats.status[3];
    stream.status_success = 1;
  } else if (status < 600) {
    ++worker->stats.status[status / 100];
    stream.status_success = 0;
  } else {
    stream.status_success = 0;
  }
}

void Client::on_stream_close(int32_t stream_id, bool success, bool final) {
  if (worker->current_phase == Phase::MAIN_DURATION) {
    if (req_inflight > 0) {
      --req_inflight;
    }
    auto req_stat = get_req_stat(stream_id);
    if (!req_stat) {
      return;
    }

    req_stat->stream_close_time = std::chrono::steady_clock::now();
    if (success) {
      req_stat->completed = true;
      ++worker->stats.req_success;
      ++cstat.req_success;

      if (streams[stream_id].status_success == 1) {
        ++worker->stats.req_status_success;
      } else {
        ++worker->stats.req_failed;
      }

      worker->sample_req_stat(req_stat);

      // Count up in successful cases only
      ++worker->request_times_smp.n;
    } else {
      ++worker->stats.req_failed;
      ++worker->stats.req_error;
    }
    ++worker->stats.req_done;
    ++req_done;

    if (worker->config->log_fd != -1) {
      auto start = std::chrono::duration_cast<std::chrono::microseconds>(
        req_stat->request_wall_time.time_since_epoch());
      auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
        req_stat->stream_close_time - req_stat->request_time);

      std::array<uint8_t, 256> buf;
      auto p = std::begin(buf);
      p = util::utos(p, start.count());
      *p++ = '\t';
      if (success) {
        p = util::utos(p, req_stat->status);
      } else {
        *p++ = '-';
        *p++ = '1';
      }
      *p++ = '\t';
      p = util::utos(p, delta.count());
      *p++ = '\n';

      auto nwrite = static_cast<size_t>(std::distance(std::begin(buf), p));
      assert(nwrite <= buf.size());
      while (write(worker->config->log_fd, buf.data(), nwrite) == -1 &&
             errno == EINTR)
        ;
    }
  }

  worker->report_progress();
  streams.erase(stream_id);
  if (req_left == 0 && req_inflight == 0) {
    terminate_session();
    return;
  }

  if (!final && req_left > 0) {
    if (config.timing_script) {
      if (!ev_is_active(&request_timeout_watcher)) {
        ev_feed_event(worker->loop, &request_timeout_watcher, EV_TIMER);
      }
    } else if (!config.rps_enabled()) {
      if (submit_request() != 0) {
        process_request_failure();
      }
    } else if (rps_req_pending) {
      --rps_req_pending;
      if (submit_request() != 0) {
        process_request_failure();
      }
    } else {
      assert(rps_req_inflight);
      --rps_req_inflight;
    }
  }
}

RequestStat *Client::get_req_stat(int32_t stream_id) {
  auto it = streams.find(stream_id);
  if (it == std::end(streams)) {
    return nullptr;
  }

  return &(*it).second.req_stat;
}

int Client::connection_made() {
  if (ssl) {
    report_tls_info();

    const unsigned char *next_proto = nullptr;
    unsigned int next_proto_len;

    SSL_get0_alpn_selected(ssl, &next_proto, &next_proto_len);

    if (next_proto) {
      auto proto = StringRef{next_proto, next_proto_len};
      if (config.is_quic()) {
#ifdef ENABLE_HTTP3
        assert(session);
        if ("h3"_sr != proto && "h3-29"_sr != proto) {
          return -1;
        }
#endif // ENABLE_HTTP3
      } else if (util::check_h2_is_selected(proto)) {
        session = std::make_unique<Http2Session>(this);
      } else if (NGHTTP2_H1_1 == proto) {
        session = std::make_unique<Http1Session>(this);
      }

      // Just assign next_proto to selected_proto anyway to show the
      // negotiation result.
      selected_proto = proto;
    } else if (config.is_quic()) {
      std::cerr << "QUIC requires ALPN negotiation" << std::endl;
      return -1;
    } else {
      std::cout << "No protocol negotiated. Fallback behaviour may be activated"
                << std::endl;

      for (const auto &proto : config.alpn_list) {
        if (NGHTTP2_H1_1_ALPN == proto) {
          std::cout << "Server does not support ALPN. Falling back to HTTP/1.1."
                    << std::endl;
          session = std::make_unique<Http1Session>(this);
          selected_proto = NGHTTP2_H1_1;
          break;
        }
      }
    }

    if (!selected_proto.empty()) {
      report_app_info();
    }

    if (!session) {
      std::cout
        << "No supported protocol was negotiated. Supported protocols were:"
        << std::endl;
      for (const auto &proto : config.alpn_list) {
        std::cout << proto.substr(1) << std::endl;
      }
      disconnect();
      return -1;
    }
  } else {
    switch (config.no_tls_proto) {
    case Config::PROTO_HTTP2:
      session = std::make_unique<Http2Session>(this);
      selected_proto = NGHTTP2_CLEARTEXT_PROTO_VERSION_ID;
      break;
    case Config::PROTO_HTTP1_1:
      session = std::make_unique<Http1Session>(this);
      selected_proto = NGHTTP2_H1_1;
      break;
    default:
      // unreachable
      assert(0);
    }

    report_app_info();
  }

  state = CLIENT_CONNECTED;

  session->on_connect();

  record_connect_time();

  if (config.rps_enabled()) {
    rps_watcher.repeat = std::max(0.01, 1. / config.rps);
    ev_timer_again(worker->loop, &rps_watcher);
    rps_duration_started = std::chrono::steady_clock::now();
  }

  if (config.rps_enabled()) {
    assert(req_left);

    ++rps_req_inflight;

    if (submit_request() != 0) {
      process_request_failure();
    }
  } else if (!config.timing_script) {
    auto nreq = config.is_timing_based_mode()
                  ? std::max(req_left, session->max_concurrent_streams())
                  : std::min(req_left, session->max_concurrent_streams());

    for (; nreq > 0; --nreq) {
      if (submit_request() != 0) {
        process_request_failure();
        break;
      }
    }
  } else {
    auto duration = config.timings[reqidx];

    while (duration < std::chrono::duration<double>(1e-9)) {
      if (submit_request() != 0) {
        process_request_failure();
        break;
      }
      duration = config.timings[reqidx];
      if (reqidx == 0) {
        // if reqidx wraps around back to 0, we uses up all lines and
        // should break
        break;
      }
    }

    if (duration >= std::chrono::duration<double>(1e-9)) {
      // double check since we may have break due to reqidx wraps
      // around back to 0
      request_timeout_watcher.repeat = util::ev_tstamp_from(duration);
      ev_timer_again(worker->loop, &request_timeout_watcher);
    }
  }
  signal_write();

  return 0;
}

int Client::on_read(const uint8_t *data, size_t len) {
  auto rv = session->on_read(data, len);
  if (worker->current_phase == Phase::MAIN_DURATION) {
    worker->stats.bytes_total += len;
  }
  if (rv != 0) {
    return -1;
  }
  signal_write();
  return 0;
}

int Client::on_write() {
  if (wb.rleft() >= BACKOFF_WRITE_BUFFER_THRES) {
    return 0;
  }

  if (session->on_write() != 0) {
    return -1;
  }
  return 0;
}

int Client::read_clear() {
  uint8_t buf[8_k];

  for (;;) {
    ssize_t nread;
    while ((nread = read(fd, buf, sizeof(buf))) == -1 && errno == EINTR)
      ;
    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
      }
      return -1;
    }

    if (nread == 0) {
      return -1;
    }

    if (on_read(buf, nread) != 0) {
      return -1;
    }
  }

  return 0;
}

int Client::write_clear() {
  std::array<struct iovec, 2> iov;

  for (;;) {
    if (on_write() != 0) {
      return -1;
    }

    auto iovcnt = wb.riovec(iov.data(), iov.size());

    if (iovcnt == 0) {
      break;
    }

    ssize_t nwrite;
    while ((nwrite = writev(fd, iov.data(), iovcnt)) == -1 && errno == EINTR)
      ;

    if (nwrite == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        ev_io_start(worker->loop, &wev);
        return 0;
      }
      return -1;
    }

    wb.drain(nwrite);
  }

  ev_io_stop(worker->loop, &wev);

  return 0;
}

int Client::connected() {
  if (!util::check_socket_connected(fd)) {
    return ERR_CONNECT_FAIL;
  }
  ev_io_start(worker->loop, &rev);
  ev_io_stop(worker->loop, &wev);

  if (ssl) {
    SSL_set_fd(ssl, fd);

    readfn = &Client::tls_handshake;
    writefn = &Client::tls_handshake;

    return do_write();
  }

  readfn = &Client::read_clear;
  writefn = &Client::write_clear;

  if (connection_made() != 0) {
    return -1;
  }

  return 0;
}

int Client::tls_handshake() {
  ERR_clear_error();

  auto rv = SSL_do_handshake(ssl);

  if (rv <= 0) {
    auto err = SSL_get_error(ssl, rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
      ev_io_stop(worker->loop, &wev);
      return 0;
    case SSL_ERROR_WANT_WRITE:
      ev_io_start(worker->loop, &wev);
      return 0;
    default:
      return -1;
    }
  }

  ev_io_stop(worker->loop, &wev);

  readfn = &Client::read_tls;
  writefn = &Client::write_tls;

  if (connection_made() != 0) {
    return -1;
  }

  return 0;
}

int Client::read_tls() {
  uint8_t buf[8_k];

  ERR_clear_error();

  for (;;) {
    auto rv = SSL_read(ssl, buf, sizeof(buf));

    if (rv <= 0) {
      auto err = SSL_get_error(ssl, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
        return 0;
      case SSL_ERROR_WANT_WRITE:
        // renegotiation started
        return -1;
      default:
        return -1;
      }
    }

    if (on_read(buf, rv) != 0) {
      return -1;
    }
  }
}

int Client::write_tls() {
  ERR_clear_error();

  struct iovec iov;

  for (;;) {
    if (on_write() != 0) {
      return -1;
    }

    auto iovcnt = wb.riovec(&iov, 1);

    if (iovcnt == 0) {
      break;
    }

    auto rv = SSL_write(ssl, iov.iov_base, iov.iov_len);

    if (rv <= 0) {
      auto err = SSL_get_error(ssl, rv);
      switch (err) {
      case SSL_ERROR_WANT_READ:
        // renegotiation started
        return -1;
      case SSL_ERROR_WANT_WRITE:
        ev_io_start(worker->loop, &wev);
        return 0;
      default:
        return -1;
      }
    }

    wb.drain(rv);
  }

  ev_io_stop(worker->loop, &wev);

  return 0;
}

#ifdef ENABLE_HTTP3
// Returns 1 if sendmsg is blocked.
int Client::write_udp(const sockaddr *addr, socklen_t addrlen,
                      const uint8_t *data, size_t datalen, size_t gso_size) {
  iovec msg_iov;
  msg_iov.iov_base = const_cast<uint8_t *>(data);
  msg_iov.iov_len = datalen;

  msghdr msg{};
  msg.msg_name = const_cast<sockaddr *>(addr);
  msg.msg_namelen = addrlen;
  msg.msg_iov = &msg_iov;
  msg.msg_iovlen = 1;

#  ifdef UDP_SEGMENT
  std::array<uint8_t, CMSG_SPACE(sizeof(uint16_t))> msg_ctrl{};
  if (gso_size && datalen > gso_size) {
    msg.msg_control = msg_ctrl.data();
    msg.msg_controllen = msg_ctrl.size();

    auto cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_UDP;
    cm->cmsg_type = UDP_SEGMENT;
    cm->cmsg_len = CMSG_LEN(sizeof(uint16_t));
    uint16_t n = gso_size;
    memcpy(CMSG_DATA(cm), &n, sizeof(n));
  }
#  endif // UDP_SEGMENT

  auto nwrite = sendmsg(fd, &msg, 0);
  if (nwrite < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 1;
    }

    std::cerr << "sendmsg: errno=" << errno << std::endl;
  } else if (gso_size) {
    worker->stats.udp_dgram_sent += (datalen + gso_size - 1) / gso_size;
  } else {
    ++worker->stats.udp_dgram_sent;
  }

  ev_io_stop(worker->loop, &wev);

  return 0;
}
#endif // ENABLE_HTTP3

void Client::record_request_time(RequestStat *req_stat) {
  req_stat->request_time = std::chrono::steady_clock::now();
  req_stat->request_wall_time = std::chrono::system_clock::now();
}

void Client::record_connect_start_time() {
  cstat.connect_start_time = std::chrono::steady_clock::now();
}

void Client::record_connect_time() {
  cstat.connect_time = std::chrono::steady_clock::now();
}

void Client::record_ttfb() {
  if (recorded(cstat.ttfb)) {
    return;
  }

  cstat.ttfb = std::chrono::steady_clock::now();
}

void Client::clear_connect_times() {
  cstat.connect_start_time = std::chrono::steady_clock::time_point();
  cstat.connect_time = std::chrono::steady_clock::time_point();
  cstat.ttfb = std::chrono::steady_clock::time_point();
}

void Client::record_client_start_time() {
  // Record start time only once at the very first connection is going
  // to be made.
  if (recorded(cstat.client_start_time)) {
    return;
  }

  cstat.client_start_time = std::chrono::steady_clock::now();
}

void Client::record_client_end_time() {
  // Unlike client_start_time, we overwrite client_end_time.  This
  // handles multiple connect/disconnect for HTTP/1.1 benchmark.
  cstat.client_end_time = std::chrono::steady_clock::now();
}

void Client::signal_write() { ev_io_start(worker->loop, &wev); }

void Client::try_new_connection() { new_connection_requested = true; }

namespace {
int get_ev_loop_flags() {
  if (ev_supported_backends() & ~ev_recommended_backends() & EVBACKEND_KQUEUE) {
    return ev_recommended_backends() | EVBACKEND_KQUEUE;
  }

  return 0;
}
} // namespace

Worker::Worker(uint32_t id, SSL_CTX *ssl_ctx, size_t req_todo, size_t nclients,
               size_t rate, size_t max_samples, Config *config)
  : randgen(util::make_mt19937()),
    stats(req_todo, nclients),
    loop(ev_loop_new(get_ev_loop_flags())),
    ssl_ctx(ssl_ctx),
    config(config),
    id(id),
    tls_info_report_done(false),
    app_info_report_done(false),
    nconns_made(0),
    nclients(nclients),
    nreqs_per_client(req_todo / nclients),
    nreqs_rem(req_todo % nclients),
    rate(rate),
    max_samples(max_samples),
    next_client_id(0) {
  if (!config->is_rate_mode() && !config->is_timing_based_mode()) {
    progress_interval = std::max(static_cast<size_t>(1), req_todo / 10);
  } else {
    progress_interval = std::max(static_cast<size_t>(1), nclients / 10);
  }

  // Below timeout is not needed in case of timing-based benchmarking
  // create timer that will go off every rate_period
  ev_timer_init(&timeout_watcher, rate_period_timeout_w_cb, 0.,
                config->rate_period);
  timeout_watcher.data = this;

  if (config->is_timing_based_mode()) {
    stats.req_stats.reserve(std::max(req_todo, max_samples));
    stats.client_stats.reserve(std::max(nclients, max_samples));
  } else {
    stats.req_stats.reserve(std::min(req_todo, max_samples));
    stats.client_stats.reserve(std::min(nclients, max_samples));
  }

  sampling_init(request_times_smp, max_samples);
  sampling_init(client_smp, max_samples);

  ev_timer_init(&duration_watcher, duration_timeout_cb, config->duration, 0.);
  duration_watcher.data = this;

  ev_timer_init(&warmup_watcher, warmup_timeout_cb, config->warm_up_time, 0.);
  warmup_watcher.data = this;

  if (config->is_timing_based_mode()) {
    current_phase = Phase::INITIAL_IDLE;
  } else {
    current_phase = Phase::MAIN_DURATION;
  }
}

Worker::~Worker() {
  ev_timer_stop(loop, &timeout_watcher);
  ev_timer_stop(loop, &duration_watcher);
  ev_timer_stop(loop, &warmup_watcher);
  ev_loop_destroy(loop);
}

void Worker::stop_all_clients() {
  for (auto client : clients) {
    if (client) {
      client->terminate_session();
    }
  }
}

void Worker::free_client(Client *deleted_client) {
  for (auto &client : clients) {
    if (client == deleted_client) {
      client->req_todo = client->req_done;
      stats.req_todo += client->req_todo;
      auto index = &client - &clients[0];
      clients[index] = nullptr;
      return;
    }
  }
}

void Worker::run() {
  if (!config->is_rate_mode() && !config->is_timing_based_mode()) {
    for (size_t i = 0; i < nclients; ++i) {
      auto req_todo = nreqs_per_client;
      if (nreqs_rem > 0) {
        ++req_todo;
        --nreqs_rem;
      }

      auto client = std::make_unique<Client>(next_client_id++, this, req_todo);
      if (client->connect() != 0) {
        std::cerr << "client could not connect to host" << std::endl;
        client->fail();
      } else {
        client.release();
      }
    }
  } else if (config->is_rate_mode()) {
    ev_timer_again(loop, &timeout_watcher);

    // call callback so that we don't waste the first rate_period
    rate_period_timeout_w_cb(loop, &timeout_watcher, 0);
  } else {
    // call the callback to start for one single time
    rate_period_timeout_w_cb(loop, &timeout_watcher, 0);
  }
  ev_run(loop, 0);
}

namespace {
template <typename Stats, typename Stat>
void sample(Sampling &smp, Stats &stats, Stat *s) {
  ++smp.n;
  if (stats.size() < smp.max_samples) {
    stats.push_back(*s);
    return;
  }
  auto d = std::uniform_int_distribution<unsigned long>(0, smp.n - 1);
  auto i = d(gen);
  if (i < smp.max_samples) {
    stats[i] = *s;
  }
}
} // namespace

void Worker::sample_req_stat(RequestStat *req_stat) {
  sample(request_times_smp, stats.req_stats, req_stat);
}

void Worker::sample_client_stat(ClientStat *cstat) {
  sample(client_smp, stats.client_stats, cstat);
}

void Worker::report_progress() {
  if (id != 0 || config->is_rate_mode() || stats.req_done % progress_interval ||
      config->is_timing_based_mode()) {
    return;
  }

  std::cout << "progress: " << stats.req_done * 100 / stats.req_todo << "% done"
            << std::endl;
}

void Worker::report_rate_progress() {
  if (id != 0 || nconns_made % progress_interval) {
    return;
  }

  std::cout << "progress: " << nconns_made * 100 / nclients
            << "% of clients started" << std::endl;
}

namespace {
// Returns percentage of number of samples within mean +/- sd.
double within_sd(const std::vector<double> &samples, double mean, double sd) {
  if (samples.size() == 0) {
    return 0.0;
  }
  auto lower = mean - sd;
  auto upper = mean + sd;
  auto m = std::count_if(
    std::begin(samples), std::end(samples),
    [&lower, &upper](double t) { return lower <= t && t <= upper; });
  return (m / static_cast<double>(samples.size())) * 100;
}
} // namespace

namespace {
// Computes statistics using |samples|. The min, max, mean, sd, and
// percentage of number of samples within mean +/- sd are computed.
// If |sampling| is true, this computes sample variance.  Otherwise,
// population variance.
SDStat compute_time_stat(const std::vector<double> &samples,
                         bool sampling = false) {
  if (samples.empty()) {
    return {0.0, 0.0, 0.0, 0.0, 0.0};
  }
  // standard deviation calculated using Rapid calculation method:
  // https://en.wikipedia.org/wiki/Standard_deviation#Rapid_calculation_methods
  double a = 0, q = 0;
  size_t n = 0;
  double sum = 0;
  auto res = SDStat{std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::min()};
  for (const auto &t : samples) {
    ++n;
    res.min = std::min(res.min, t);
    res.max = std::max(res.max, t);
    sum += t;

    auto na = a + (t - a) / n;
    q += (t - a) * (t - na);
    a = na;
  }

  assert(n > 0);
  res.mean = sum / n;
  res.sd = sqrt(q / (sampling && n > 1 ? n - 1 : n));
  res.within_sd = within_sd(samples, res.mean, res.sd);

  return res;
}
} // namespace

namespace {
SDStats
process_time_stats(const std::vector<std::unique_ptr<Worker>> &workers) {
  auto request_times_sampling = false;
  auto client_times_sampling = false;
  size_t nrequest_times = 0;
  size_t nclient_times = 0;
  for (const auto &w : workers) {
    nrequest_times += w->stats.req_stats.size();
    request_times_sampling = w->request_times_smp.n > w->stats.req_stats.size();

    nclient_times += w->stats.client_stats.size();
    client_times_sampling = w->client_smp.n > w->stats.client_stats.size();
  }

  std::vector<double> request_times;
  request_times.reserve(nrequest_times);

  std::vector<double> connect_times, ttfb_times, rps_values;
  connect_times.reserve(nclient_times);
  ttfb_times.reserve(nclient_times);
  rps_values.reserve(nclient_times);

  for (const auto &w : workers) {
    for (const auto &req_stat : w->stats.req_stats) {
      if (!req_stat.completed) {
        continue;
      }
      request_times.push_back(
        std::chrono::duration_cast<std::chrono::duration<double>>(
          req_stat.stream_close_time - req_stat.request_time)
          .count());
    }

    const auto &stat = w->stats;

    for (const auto &cstat : stat.client_stats) {
      if (recorded(cstat.client_start_time) &&
          recorded(cstat.client_end_time)) {
        auto t = std::chrono::duration_cast<std::chrono::duration<double>>(
                   cstat.client_end_time - cstat.client_start_time)
                   .count();
        if (t > 1e-9) {
          rps_values.push_back(cstat.req_success / t);
        }
      }

      // We will get connect event before FFTB.
      if (!recorded(cstat.connect_start_time) ||
          !recorded(cstat.connect_time)) {
        continue;
      }

      connect_times.push_back(
        std::chrono::duration_cast<std::chrono::duration<double>>(
          cstat.connect_time - cstat.connect_start_time)
          .count());

      if (!recorded(cstat.ttfb)) {
        continue;
      }

      ttfb_times.push_back(
        std::chrono::duration_cast<std::chrono::duration<double>>(
          cstat.ttfb - cstat.connect_start_time)
          .count());
    }
  }

  return {compute_time_stat(request_times, request_times_sampling),
          compute_time_stat(connect_times, client_times_sampling),
          compute_time_stat(ttfb_times, client_times_sampling),
          compute_time_stat(rps_values, client_times_sampling)};
}
} // namespace

namespace {
void resolve_host() {
  if (config.base_uri_unix) {
    auto res = std::make_unique<addrinfo>();
    res->ai_family = config.unix_addr.sun_family;
    res->ai_socktype = SOCK_STREAM;
    res->ai_addrlen = sizeof(config.unix_addr);
    res->ai_addr =
      static_cast<struct sockaddr *>(static_cast<void *>(&config.unix_addr));

    config.addrs = res.release();
    return;
  };

  int rv;
  addrinfo hints{}, *res;

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_ADDRCONFIG;

  const auto &resolve_host =
    config.connect_to_host.empty() ? config.host : config.connect_to_host;
  auto port =
    config.connect_to_port == 0 ? config.port : config.connect_to_port;

  rv =
    getaddrinfo(resolve_host.c_str(), util::utos(port).c_str(), &hints, &res);
  if (rv != 0) {
    std::cerr << "getaddrinfo() failed: " << gai_strerror(rv) << std::endl;
    exit(EXIT_FAILURE);
  }
  if (res == nullptr) {
    std::cerr << "No address returned" << std::endl;
    exit(EXIT_FAILURE);
  }
  config.addrs = res;
}
} // namespace

namespace {
std::string get_reqline(const char *uri, const urlparse_url &u) {
  std::string reqline;

  if (util::has_uri_field(u, URLPARSE_PATH)) {
    reqline = util::get_uri_field(uri, u, URLPARSE_PATH);
  } else {
    reqline = "/";
  }

  if (util::has_uri_field(u, URLPARSE_QUERY)) {
    reqline += '?';
    reqline += util::get_uri_field(uri, u, URLPARSE_QUERY);
  }

  return reqline;
}
} // namespace

namespace {
constexpr auto UNIX_PATH_PREFIX = "unix:"_sr;
} // namespace

namespace {
bool parse_base_uri(const StringRef &base_uri) {
  urlparse_url u;
  if (urlparse_parse_url(base_uri.data(), base_uri.size(), 0, &u) != 0 ||
      !util::has_uri_field(u, URLPARSE_SCHEMA) ||
      !util::has_uri_field(u, URLPARSE_HOST)) {
    return false;
  }

  config.scheme = util::get_uri_field(base_uri.data(), u, URLPARSE_SCHEMA);
  config.host = util::get_uri_field(base_uri.data(), u, URLPARSE_HOST);
  config.default_port = util::get_default_port(base_uri.data(), u);
  if (util::has_uri_field(u, URLPARSE_PORT)) {
    config.port = u.port;
  } else {
    config.port = config.default_port;
  }

  return true;
}
} // namespace
namespace {
// Use std::vector<std::string>::iterator explicitly, without that,
// urlparse_url u{} fails with clang-3.4.
std::vector<std::string> parse_uris(std::vector<std::string>::iterator first,
                                    std::vector<std::string>::iterator last) {
  std::vector<std::string> reqlines;

  if (first == last) {
    std::cerr << "no URI available" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!config.has_base_uri()) {
    if (!parse_base_uri(StringRef{*first})) {
      std::cerr << "invalid URI: " << *first << std::endl;
      exit(EXIT_FAILURE);
    }

    config.base_uri = *first;
  }

  for (; first != last; ++first) {
    urlparse_url u;

    auto uri = (*first).c_str();

    if (urlparse_parse_url(uri, (*first).size(), 0, &u) != 0) {
      std::cerr << "invalid URI: " << uri << std::endl;
      exit(EXIT_FAILURE);
    }

    reqlines.push_back(get_reqline(uri, u));
  }

  return reqlines;
}
} // namespace

namespace {
std::vector<std::string> read_uri_from_file(std::istream &infile) {
  std::vector<std::string> uris;
  std::string line_uri;
  while (std::getline(infile, line_uri)) {
    uris.push_back(line_uri);
  }

  return uris;
}
} // namespace

namespace {
void read_script_from_file(
  std::istream &infile,
  std::vector<std::chrono::steady_clock::duration> &timings,
  std::vector<std::string> &uris) {
  std::string script_line;
  int line_count = 0;
  while (std::getline(infile, script_line)) {
    line_count++;
    if (script_line.empty()) {
      std::cerr << "Empty line detected at line " << line_count
                << ". Ignoring and continuing." << std::endl;
      continue;
    }

    std::size_t pos = script_line.find("\t");
    if (pos == std::string::npos) {
      std::cerr << "Invalid line format detected, no tab character at line "
                << line_count << ". \n\t" << script_line << std::endl;
      exit(EXIT_FAILURE);
    }

    const char *start = script_line.c_str();
    char *end;
    auto v = std::strtod(start, &end);

    errno = 0;
    if (v < 0.0 || !std::isfinite(v) || end == start || errno != 0) {
      auto error = errno;
      std::cerr << "Time value error at line " << line_count << ". \n\t"
                << "value = " << script_line.substr(0, pos) << std::endl;
      if (error != 0) {
        std::cerr << "\t" << strerror(error) << std::endl;
      }
      exit(EXIT_FAILURE);
    }

    timings.emplace_back(
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double, std::milli>(v)));
    uris.push_back(script_line.substr(pos + 1, script_line.size()));
  }
}
} // namespace

namespace {
std::unique_ptr<Worker> create_worker(uint32_t id, SSL_CTX *ssl_ctx,
                                      size_t nreqs, size_t nclients,
                                      size_t rate, size_t max_samples) {
  std::stringstream rate_report;
  if (config.is_rate_mode() && nclients > rate) {
    rate_report << "Up to " << rate << " client(s) will be created every "
                << util::duration_str(config.rate_period) << " ";
  }

  if (config.is_timing_based_mode()) {
    std::cout << "spawning thread #" << id << ": " << nclients
              << " total client(s). Timing-based test with "
              << config.warm_up_time << "s of warm-up time and "
              << config.duration << "s of main duration for measurements."
              << std::endl;
  } else {
    std::cout << "spawning thread #" << id << ": " << nclients
              << " total client(s). " << rate_report.str() << nreqs
              << " total requests" << std::endl;
  }

  if (config.is_rate_mode()) {
    return std::make_unique<Worker>(id, ssl_ctx, nreqs, nclients, rate,
                                    max_samples, &config);
  } else {
    // Here rate is same as client because the rate_timeout callback
    // will be called only once
    return std::make_unique<Worker>(id, ssl_ctx, nreqs, nclients, nclients,
                                    max_samples, &config);
  }
}
} // namespace

namespace {
int parse_header_table_size(uint32_t &dst, const char *opt,
                            const char *optarg) {
  auto n = util::parse_uint_with_unit(optarg);
  if (!n) {
    std::cerr << "--" << opt << ": Bad option value: " << optarg << std::endl;
    return -1;
  }
  if (n > std::numeric_limits<uint32_t>::max()) {
    std::cerr << "--" << opt
              << ": Value too large.  It should be less than or equal to "
              << std::numeric_limits<uint32_t>::max() << std::endl;
    return -1;
  }

  dst = *n;

  return 0;
}
} // namespace

namespace {
std::string make_http_authority(const Config &config) {
  std::string host;

  if (util::numeric_host(config.host.c_str(), AF_INET6)) {
    host += '[';
    host += config.host;
    host += ']';
  } else {
    host = config.host;
  }

  if (config.port != config.default_port) {
    host += ':';
    host += util::utos(config.port);
  }

  return host;
}
} // namespace

namespace {
void print_version(std::ostream &out) {
  out << "h2load nghttp2/" NGHTTP2_VERSION << std::endl;
}
} // namespace

namespace {
void print_usage(std::ostream &out) {
  out << R"(Usage: h2load [OPTIONS]... [URI]...
benchmarking tool for HTTP/2 server)"
      << std::endl;
}
} // namespace

namespace {
constexpr auto DEFAULT_ALPN_LIST = "h2,h2-16,h2-14,http/1.1"_sr;
} // namespace

namespace {
void print_help(std::ostream &out) {
  print_usage(out);

  auto config = Config();

  out << R"(
  <URI>       Specify URI to access.   Multiple URIs can be specified.
              URIs are used  in this order for each  client.  All URIs
              are used, then  first URI is used and then  2nd URI, and
              so  on.  The  scheme, host  and port  in the  subsequent
              URIs, if present,  are ignored.  Those in  the first URI
              are used solely.  Definition of a base URI overrides all
              scheme, host or port values.
Options:
  -n, --requests=<N>
              Number of  requests across all  clients.  If it  is used
              with --timing-script-file option,  this option specifies
              the number of requests  each client performs rather than
              the number of requests  across all clients.  This option
              is ignored if timing-based  benchmarking is enabled (see
              --duration option).
              Default: )"
      << config.nreqs << R"(
  -c, --clients=<N>
              Number  of concurrent  clients.   With  -r option,  this
              specifies the maximum number of connections to be made.
              Default: )"
      << config.nclients << R"(
  -t, --threads=<N>
              Number of native threads.
              Default: )"
      << config.nthreads << R"(
  -i, --input-file=<PATH>
              Path of a file with multiple URIs are separated by EOLs.
              This option will disable URIs getting from command-line.
              If '-' is given as <PATH>, URIs will be read from stdin.
              URIs are used  in this order for each  client.  All URIs
              are used, then  first URI is used and then  2nd URI, and
              so  on.  The  scheme, host  and port  in the  subsequent
              URIs, if present,  are ignored.  Those in  the first URI
              are used solely.  Definition of a base URI overrides all
              scheme, host or port values.
  -m, --max-concurrent-streams=<N>
              Max  concurrent  streams  to issue  per  session.   When
              http/1.1  is used,  this  specifies the  number of  HTTP
              pipelining requests in-flight.
              Default: 1
  -f, --max-frame-size=<SIZE>
              Maximum frame size that the local endpoint is willing to
              receive.
              Default: )"
      << util::utos_unit(config.max_frame_size) << R"(
  -w, --window-bits=<N>
              Sets the stream level initial window size to (2**<N>)-1.
              For QUIC, <N> is capped to 26 (roughly 64MiB).
              Default: )"
      << config.window_bits << R"(
  -W, --connection-window-bits=<N>
              Sets  the  connection  level   initial  window  size  to
              (2**<N>)-1.
              Default: )"
      << config.connection_window_bits << R"(
  -H, --header=<HEADER>
              Add/Override a header to the requests.
  --ciphers=<SUITE>
              Set  allowed cipher  list  for TLSv1.2  or earlier.   The
              format of the string is described in OpenSSL ciphers(1).
              Default: )"
      << config.ciphers << R"(
  --tls13-ciphers=<SUITE>
              Set allowed cipher list for  TLSv1.3.  The format of the
              string is described in OpenSSL ciphers(1).
              Default: )"
      << config.tls13_ciphers << R"(
  -p, --no-tls-proto=<PROTOID>
              Specify ALPN identifier of the  protocol to be used when
              accessing http URI without SSL/TLS.
              Available protocols: )"
      << NGHTTP2_CLEARTEXT_PROTO_VERSION_ID << R"( and )" << NGHTTP2_H1_1 << R"(
              Default: )"
      << NGHTTP2_CLEARTEXT_PROTO_VERSION_ID << R"(
  -d, --data=<PATH>
              Post FILE to  server.  The request method  is changed to
              POST.   For  http/1.1 connection,  if  -d  is used,  the
              maximum number of in-flight pipelined requests is set to
              1.
  -r, --rate=<N>
              Specifies  the  fixed  rate  at  which  connections  are
              created.   The   rate  must   be  a   positive  integer,
              representing the  number of  connections to be  made per
              rate period.   The maximum  number of connections  to be
              made  is  given  in  -c   option.   This  rate  will  be
              distributed among  threads as  evenly as  possible.  For
              example,  with   -t2  and   -r4,  each  thread   gets  2
              connections per period.  When the rate is 0, the program
              will run  as it  normally does, creating  connections at
              whatever variable rate it  wants.  The default value for
              this option is 0.  -r and -D are mutually exclusive.
  --rate-period=<DURATION>
              Specifies the time  period between creating connections.
              The period  must be a positive  number, representing the
              length of the period in time.  This option is ignored if
              the rate option is not used.  The default value for this
              option is 1s.
  -D, --duration=<DURATION>
              Specifies the main duration for the measurements in case
              of timing-based  benchmarking.  -D  and -r  are mutually
              exclusive.
  --warm-up-time=<DURATION>
              Specifies the  time  period  before  starting the actual
              measurements, in  case  of  timing-based benchmarking.
              Needs to provided along with -D option.
  -T, --connection-active-timeout=<DURATION>
              Specifies  the maximum  time that  h2load is  willing to
              keep a  connection open,  regardless of the  activity on
              said connection.  <DURATION> must be a positive integer,
              specifying the amount of time  to wait.  When no timeout
              value is  set (either  active or inactive),  h2load will
              keep  a  connection  open indefinitely,  waiting  for  a
              response.
  -N, --connection-inactivity-timeout=<DURATION>
              Specifies the amount  of time that h2load  is willing to
              wait to see activity  on a given connection.  <DURATION>
              must  be a  positive integer,  specifying the  amount of
              time  to wait.   When no  timeout value  is set  (either
              active or inactive), h2load  will keep a connection open
              indefinitely, waiting for a response.
  --timing-script-file=<PATH>
              Path of a file containing one or more lines separated by
              EOLs.  Each script line is composed of two tab-separated
              fields.  The first field represents the time offset from
              the start of execution, expressed as a positive value of
              milliseconds  with microsecond  resolution.  The  second
              field represents the URI.  This option will disable URIs
              getting from  command-line.  If '-' is  given as <PATH>,
              script lines will be read  from stdin.  Script lines are
              used in order for each client.   If -n is given, it must
              be less  than or  equal to the  number of  script lines,
              larger values are clamped to the number of script lines.
              If -n is not given,  the number of requests will default
              to the  number of  script lines.   The scheme,  host and
              port defined in  the first URI are  used solely.  Values
              contained  in  other  URIs,  if  present,  are  ignored.
              Definition of a  base URI overrides all  scheme, host or
              port   values.   --timing-script-file   and  --rps   are
              mutually exclusive.
  -B, --base-uri=(<URI>|unix:<PATH>)
              Specify URI from which the scheme, host and port will be
              used  for  all requests.   The  base  URI overrides  all
              values  defined either  at  the command  line or  inside
              input files.  If argument  starts with "unix:", then the
              rest  of the  argument will  be treated  as UNIX  domain
              socket path.   The connection is made  through that path
              instead of TCP.   In this case, scheme  is inferred from
              the first  URI appeared  in the  command line  or inside
              input files as usual.
  --alpn-list=<LIST>
              Comma delimited list of  ALPN protocol identifier sorted
              in the  order of preference.  That  means most desirable
              protocol comes  first.  The parameter must  be delimited
              by a single comma only  and any white spaces are treated
              as a part of protocol string.
              Default: )"
      << DEFAULT_ALPN_LIST << R"(
  --h1        Short        hand        for        --alpn-list=http/1.1
              --no-tls-proto=http/1.1,    which   effectively    force
              http/1.1 for both http and https URI.
  --header-table-size=<SIZE>
              Specify decoder header table size.
              Default: )"
      << util::utos_unit(config.header_table_size) << R"(
  --encoder-header-table-size=<SIZE>
              Specify encoder header table size.  The decoder (server)
              specifies  the maximum  dynamic table  size it  accepts.
              Then the negotiated dynamic table size is the minimum of
              this option value and the value which server specified.
              Default: )"
      << util::utos_unit(config.encoder_header_table_size) << R"(
  --log-file=<PATH>
              Write per-request information to a file as tab-separated
              columns: start  time as  microseconds since  epoch; HTTP
              status code;  microseconds until end of  response.  More
              columns may be added later.  Rows are ordered by end-of-
              response  time when  using  one worker  thread, but  may
              appear slightly  out of order with  multiple threads due
              to buffering.  Status code is -1 for failed streams.
  --qlog-file-base=<PATH>
              Enable qlog output and specify base file name for qlogs.
              Qlog is emitted  for each connection.  For  a given base
              name   "base",    each   output   file    name   becomes
              "base.M.N.sqlog" where M is worker ID and N is client ID
              (e.g. "base.0.3.sqlog").  Only effective in QUIC runs.
  --connect-to=<HOST>[:<PORT>]
              Host and port to connect  instead of using the authority
              in <URI>.
  --rps=<N>   Specify request  per second for each  client.  --rps and
              --timing-script-file are mutually exclusive.
  --groups=<GROUPS>
              Specify the supported groups.
              Default: )"
      << config.groups << R"(
  --no-udp-gso
              Disable UDP GSO.
  --max-udp-payload-size=<SIZE>
              Specify the maximum outgoing UDP datagram payload size.
  --ktls      Enable ktls.
  --sni=<DNSNAME>
              Send  <DNSNAME> in  TLS  SNI, overriding  the host  name
              specified in URI.
  -v, --verbose
              Output debug information.
  --version   Display version information and exit.
  -h, --help  Display this help and exit.

--

  The <SIZE> argument is an integer and an optional unit (e.g., 10K is
  10 * 1024).  Units are K, M and G (powers of 1024).

  The <DURATION> argument is an integer and an optional unit (e.g., 1s
  is 1 second and 500ms is 500 milliseconds).  Units are h, m, s or ms
  (hours, minutes, seconds and milliseconds, respectively).  If a unit
  is omitted, a second is used as unit.)"
      << std::endl;
}
} // namespace

int main(int argc, char **argv) {
  std::string datafile;
  std::string logfile;
  bool nreqs_set_manually = false;
  while (1) {
    static int flag = 0;
    constexpr static option long_options[] = {
      {"requests", required_argument, nullptr, 'n'},
      {"clients", required_argument, nullptr, 'c'},
      {"data", required_argument, nullptr, 'd'},
      {"threads", required_argument, nullptr, 't'},
      {"max-concurrent-streams", required_argument, nullptr, 'm'},
      {"window-bits", required_argument, nullptr, 'w'},
      {"max-frame-size", required_argument, nullptr, 'f'},
      {"connection-window-bits", required_argument, nullptr, 'W'},
      {"input-file", required_argument, nullptr, 'i'},
      {"header", required_argument, nullptr, 'H'},
      {"no-tls-proto", required_argument, nullptr, 'p'},
      {"verbose", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {"version", no_argument, &flag, 1},
      {"ciphers", required_argument, &flag, 2},
      {"rate", required_argument, nullptr, 'r'},
      {"connection-active-timeout", required_argument, nullptr, 'T'},
      {"connection-inactivity-timeout", required_argument, nullptr, 'N'},
      {"duration", required_argument, nullptr, 'D'},
      {"timing-script-file", required_argument, &flag, 3},
      {"base-uri", required_argument, nullptr, 'B'},
      {"npn-list", required_argument, &flag, 4},
      {"rate-period", required_argument, &flag, 5},
      {"h1", no_argument, &flag, 6},
      {"header-table-size", required_argument, &flag, 7},
      {"encoder-header-table-size", required_argument, &flag, 8},
      {"warm-up-time", required_argument, &flag, 9},
      {"log-file", required_argument, &flag, 10},
      {"connect-to", required_argument, &flag, 11},
      {"rps", required_argument, &flag, 12},
      {"groups", required_argument, &flag, 13},
      {"tls13-ciphers", required_argument, &flag, 14},
      {"no-udp-gso", no_argument, &flag, 15},
      {"qlog-file-base", required_argument, &flag, 16},
      {"max-udp-payload-size", required_argument, &flag, 17},
      {"ktls", no_argument, &flag, 18},
      {"alpn-list", required_argument, &flag, 19},
      {"sni", required_argument, &flag, 20},
      {nullptr, 0, nullptr, 0}};
    int option_index = 0;
    auto c = getopt_long(argc, argv,
                         "hvW:c:d:m:n:p:t:w:f:H:i:r:T:N:D:B:", long_options,
                         &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'n': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-n: bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.nreqs = *n;
      nreqs_set_manually = true;
      break;
    }
    case 'c': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-c: bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.nclients = *n;
      break;
    }
    case 'd':
      datafile = optarg;
      break;
    case 't': {
#ifdef NOTHREADS
      std::cerr << "-t: WARNING: Threading disabled at build time, "
                << "no threads created." << std::endl;
#else
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-t: bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.nthreads = *n;
#endif // NOTHREADS
      break;
    }
    case 'm': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-m: bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.max_concurrent_streams = *n;
      break;
    }
    case 'w':
    case 'W': {
      auto n = util::parse_uint(optarg);
      if (!n || n > 30) {
        std::cerr << "-" << static_cast<char>(c)
                  << ": specify the integer in the range [0, 30], inclusive"
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      if (c == 'w') {
        config.window_bits = *n;
      } else {
        config.connection_window_bits = *n;
      }
      break;
    }
    case 'f': {
      auto n = util::parse_uint_with_unit(optarg);
      if (!n) {
        std::cerr << "--max-frame-size: bad option value: " << optarg
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      if (static_cast<uint64_t>(*n) < 16_k) {
        std::cerr << "--max-frame-size: minimum 16384" << std::endl;
        exit(EXIT_FAILURE);
      }
      if (static_cast<uint64_t>(*n) > 16_m - 1) {
        std::cerr << "--max-frame-size: maximum 16777215" << std::endl;
        exit(EXIT_FAILURE);
      }
      config.max_frame_size = *n;
      break;
    }
    case 'H': {
      char *header = optarg;
      // Skip first possible ':' in the header name
      char *value = strchr(optarg + 1, ':');
      if (!value || (header[0] == ':' && header + 1 == value)) {
        std::cerr << "-H: invalid header: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      *value = 0;
      value++;
      while (isspace(*value)) {
        value++;
      }
      if (*value == 0) {
        // This could also be a valid case for suppressing a header
        // similar to curl
        std::cerr << "-H: invalid header - value missing: " << optarg
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      // Note that there is no processing currently to handle multiple
      // message-header fields with the same field name
      config.custom_headers.emplace_back(header, value);
      util::inp_strlower(config.custom_headers.back().name);
      break;
    }
    case 'i':
      config.ifile = optarg;
      break;
    case 'p': {
      auto proto = StringRef{optarg};
      if (util::strieq(NGHTTP2_CLEARTEXT_PROTO_VERSION_ID ""_sr, proto)) {
        config.no_tls_proto = Config::PROTO_HTTP2;
      } else if (util::strieq(NGHTTP2_H1_1, proto)) {
        config.no_tls_proto = Config::PROTO_HTTP1_1;
      } else {
        std::cerr << "-p: unsupported protocol " << proto << std::endl;
        exit(EXIT_FAILURE);
      }
      break;
    }
    case 'r': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-r: bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      if (n == 0) {
        std::cerr << "-r: the rate at which connections are made "
                  << "must be positive." << std::endl;
        exit(EXIT_FAILURE);
      }
      config.rate = *n;
      break;
    }
    case 'T': {
      auto d = util::parse_duration_with_unit(optarg);
      if (!d) {
        std::cerr << "-T: bad value for the conn_active_timeout wait time: "
                  << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.conn_active_timeout = *d;
      break;
    }
    case 'N': {
      auto d = util::parse_duration_with_unit(optarg);
      if (!d) {
        std::cerr << "-N: bad value for the conn_inactivity_timeout wait time: "
                  << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.conn_inactivity_timeout = *d;
      break;
    }
    case 'B': {
      auto arg = StringRef{optarg};
      config.base_uri = "";
      config.base_uri_unix = false;

      if (util::istarts_with(arg, UNIX_PATH_PREFIX)) {
        // UNIX domain socket path
        sockaddr_un un;

        auto path =
          StringRef{std::begin(arg) + UNIX_PATH_PREFIX.size(), std::end(arg)};

        if (path.size() == 0 || path.size() + 1 > sizeof(un.sun_path)) {
          std::cerr << "--base-uri: invalid UNIX domain socket path: " << arg
                    << std::endl;
          exit(EXIT_FAILURE);
        }

        config.base_uri_unix = true;

        auto &unix_addr = config.unix_addr;
        std::copy(std::begin(path), std::end(path), unix_addr.sun_path);
        unix_addr.sun_path[path.size()] = '\0';
        unix_addr.sun_family = AF_UNIX;

        break;
      }

      if (!parse_base_uri(arg)) {
        std::cerr << "--base-uri: invalid base URI: " << arg << std::endl;
        exit(EXIT_FAILURE);
      }

      config.base_uri = arg;
      break;
    }
    case 'D': {
      auto d = util::parse_duration_with_unit(optarg);
      if (!d) {
        std::cerr << "-D: value error " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.duration = *d;
      break;
    }
    case 'v':
      config.verbose = true;
      break;
    case 'h':
      print_help(std::cout);
      exit(EXIT_SUCCESS);
    case '?':
      util::show_candidates(argv[optind - 1], long_options);
      exit(EXIT_FAILURE);
    case 0:
      switch (flag) {
      case 1:
        // version option
        print_version(std::cout);
        exit(EXIT_SUCCESS);
      case 2:
        // ciphers option
        config.ciphers = optarg;
        break;
      case 3:
        // timing-script option
        config.ifile = optarg;
        config.timing_script = true;
        break;
      case 5: {
        // rate-period
        auto d = util::parse_duration_with_unit(optarg);
        if (!d) {
          std::cerr << "--rate-period: value error " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        config.rate_period = *d;
        break;
      }
      case 6:
        // --h1
        config.alpn_list = util::parse_config_str_list("http/1.1"_sr);
        config.no_tls_proto = Config::PROTO_HTTP1_1;
        break;
      case 7:
        // --header-table-size
        if (parse_header_table_size(config.header_table_size,
                                    "header-table-size", optarg) != 0) {
          exit(EXIT_FAILURE);
        }
        break;
      case 8:
        // --encoder-header-table-size
        if (parse_header_table_size(config.encoder_header_table_size,
                                    "encoder-header-table-size", optarg) != 0) {
          exit(EXIT_FAILURE);
        }
        break;
      case 9: {
        // --warm-up-time
        auto d = util::parse_duration_with_unit(optarg);
        if (!d) {
          std::cerr << "--warm-up-time: value error " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        config.warm_up_time = *d;
        break;
      }
      case 10:
        // --log-file
        logfile = optarg;
        break;
      case 11: {
        // --connect-to
        auto p = util::split_hostport(StringRef{optarg});
        int64_t port = 0;
        if (p.first.empty() ||
            (!p.second.empty() &&
             (port = util::parse_uint(p.second).value_or(-1)) == -1)) {
          std::cerr << "--connect-to: Invalid value " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        config.connect_to_host = p.first;
        config.connect_to_port = port;
        break;
      }
      case 12: {
        char *end;
        auto v = std::strtod(optarg, &end);
        if (end == optarg || *end != '\0' || !std::isfinite(v) ||
            1. / v < 1e-6) {
          std::cerr << "--rps: Invalid value " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        config.rps = v;
        break;
      }
      case 13:
        // --groups
        config.groups = optarg;
        break;
      case 14:
        // --tls13-ciphers
        config.tls13_ciphers = optarg;
        break;
      case 15:
        // --no-udp-gso
        config.no_udp_gso = true;
        break;
      case 16:
        // --qlog-file-base
        config.qlog_file_base = optarg;
        break;
      case 17: {
        // --max-udp-payload-size
        auto n = util::parse_uint_with_unit(optarg);
        if (!n) {
          std::cerr << "--max-udp-payload-size: bad option value: " << optarg
                    << std::endl;
          exit(EXIT_FAILURE);
        }
        if (static_cast<uint64_t>(*n) > 64_k) {
          std::cerr << "--max-udp-payload-size: must not exceed 65536"
                    << std::endl;
          exit(EXIT_FAILURE);
        }
        config.max_udp_payload_size = *n;
        break;
      }
      case 18:
        // --ktls
        config.ktls = true;
        break;
      case 4:
        // npn-list option
        std::cerr << "--npn-list: deprecated.  Use --alpn-list instead."
                  << std::endl;
        // fall through
      case 19:
        // alpn-list option
        config.alpn_list = util::parse_config_str_list(StringRef{optarg});
        break;
      case 20:
        // --sni
        config.sni = optarg;
        break;
      }
      break;
    default:
      break;
    }
  }

  if (argc == optind) {
    if (config.ifile.empty()) {
      std::cerr << "no URI or input file given" << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  if (config.nclients == 0) {
    std::cerr << "-c: the number of clients must be strictly greater than 0."
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.alpn_list.empty()) {
    config.alpn_list = util::parse_config_str_list(DEFAULT_ALPN_LIST);
  }

  // serialize the APLN tokens
  for (auto &proto : config.alpn_list) {
    proto.insert(proto.begin(), static_cast<unsigned char>(proto.size()));
  }

  std::vector<std::string> reqlines;

  if (config.ifile.empty()) {
    std::vector<std::string> uris;
    std::copy(&argv[optind], &argv[argc], std::back_inserter(uris));
    reqlines = parse_uris(std::begin(uris), std::end(uris));
  } else {
    std::vector<std::string> uris;
    if (!config.timing_script) {
      if (config.ifile == "-") {
        uris = read_uri_from_file(std::cin);
      } else {
        std::ifstream infile(config.ifile);
        if (!infile) {
          std::cerr << "cannot read input file: " << config.ifile << std::endl;
          exit(EXIT_FAILURE);
        }

        uris = read_uri_from_file(infile);
      }
    } else {
      if (config.ifile == "-") {
        read_script_from_file(std::cin, config.timings, uris);
      } else {
        std::ifstream infile(config.ifile);
        if (!infile) {
          std::cerr << "cannot read input file: " << config.ifile << std::endl;
          exit(EXIT_FAILURE);
        }

        read_script_from_file(infile, config.timings, uris);
      }

      if (nreqs_set_manually) {
        if (config.nreqs > uris.size()) {
          std::cerr << "-n: the number of requests must be less than or equal "
                       "to the number of timing script entries. Setting number "
                       "of requests to "
                    << uris.size() << std::endl;

          config.nreqs = uris.size();
        }
      } else {
        config.nreqs = uris.size();
      }
    }

    reqlines = parse_uris(std::begin(uris), std::end(uris));
  }

  if (reqlines.empty()) {
    std::cerr << "No URI given" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.is_timing_based_mode() && config.is_rate_mode()) {
    std::cerr << "-r, -D: they are mutually exclusive." << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.timing_script && config.rps_enabled()) {
    std::cerr << "--timing-script-file, --rps: they are mutually exclusive."
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.nreqs == 0 && !config.is_timing_based_mode()) {
    std::cerr << "-n: the number of requests must be strictly greater than 0 "
                 "if timing-based test is not being run."
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.max_concurrent_streams == 0) {
    std::cerr << "-m: the max concurrent streams must be strictly greater "
              << "than 0." << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.nthreads == 0) {
    std::cerr << "-t: the number of threads must be strictly greater than 0."
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.nthreads > std::thread::hardware_concurrency()) {
    std::cerr << "-t: warning: the number of threads is greater than hardware "
              << "cores." << std::endl;
  }

  // With timing script, we don't distribute config.nreqs to each
  // client or thread.
  if (!config.timing_script && config.nreqs < config.nclients &&
      !config.is_timing_based_mode()) {
    std::cerr << "-n, -c: the number of requests must be greater than or "
              << "equal to the clients." << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.nclients < config.nthreads) {
    std::cerr << "-c, -t: the number of clients must be greater than or equal "
              << "to the number of threads." << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.is_timing_based_mode()) {
    config.nreqs = 0;
  }

  if (config.is_rate_mode()) {
    if (config.rate < config.nthreads) {
      std::cerr << "-r, -t: the connection rate must be greater than or equal "
                << "to the number of threads." << std::endl;
      exit(EXIT_FAILURE);
    }

    if (config.rate > config.nclients) {
      std::cerr << "-r, -c: the connection rate must be smaller than or equal "
                   "to the number of clients."
                << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  if (!datafile.empty()) {
    config.data_fd = open(datafile.c_str(), O_RDONLY | O_BINARY);
    if (config.data_fd == -1) {
      std::cerr << "-d: Could not open file " << datafile << std::endl;
      exit(EXIT_FAILURE);
    }
    struct stat data_stat;
    if (fstat(config.data_fd, &data_stat) == -1) {
      std::cerr << "-d: Could not stat file " << datafile << std::endl;
      exit(EXIT_FAILURE);
    }
    config.data_length = data_stat.st_size;
    auto addr = mmap(nullptr, config.data_length, PROT_READ, MAP_SHARED,
                     config.data_fd, 0);
    if (addr == MAP_FAILED) {
      std::cerr << "-d: Could not mmap file " << datafile << std::endl;
      exit(EXIT_FAILURE);
    }
    config.data = static_cast<uint8_t *>(addr);
  }

  if (!logfile.empty()) {
    config.log_fd = open(logfile.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                         S_IRUSR | S_IWUSR | S_IRGRP);
    if (config.log_fd == -1) {
      std::cerr << "--log-file: Could not open file " << logfile << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  if (!config.qlog_file_base.empty() && !config.is_quic()) {
    std::cerr << "Warning: --qlog-file-base: only effective in quic, ignoring."
              << std::endl;
  }

  struct sigaction act {};
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, nullptr);

  auto ssl_ctx = SSL_CTX_new(TLS_client_method());
  if (!ssl_ctx) {
    std::cerr << "Failed to create SSL_CTX: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(EXIT_FAILURE);
  }

  auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                  SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                  SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

#ifdef SSL_OP_ENABLE_KTLS
  if (config.ktls) {
    ssl_opts |= SSL_OP_ENABLE_KTLS;
  }
#endif // SSL_OP_ENABLE_KTLS

  SSL_CTX_set_options(ssl_ctx, ssl_opts);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

  if (config.is_quic()) {
#ifdef ENABLE_HTTP3
#  ifdef HAVE_LIBNGTCP2_CRYPTO_QUICTLS
    if (ngtcp2_crypto_quictls_configure_client_context(ssl_ctx) != 0) {
      std::cerr << "ngtcp2_crypto_quictls_configure_client_context failed"
                << std::endl;
      exit(EXIT_FAILURE);
    }
#  endif // HAVE_LIBNGTCP2_CRYPTO_QUICTLS
#  ifdef HAVE_LIBNGTCP2_CRYPTO_BORINGSSL
    if (ngtcp2_crypto_boringssl_configure_client_context(ssl_ctx) != 0) {
      std::cerr << "ngtcp2_crypto_boringssl_configure_client_context failed"
                << std::endl;
      exit(EXIT_FAILURE);
    }
#  endif // HAVE_LIBNGTCP2_CRYPTO_BORINGSSL
#  ifdef HAVE_LIBNGTCP2_CRYPTO_WOLFSSL
    if (ngtcp2_crypto_wolfssl_configure_client_context(ssl_ctx) != 0) {
      std::cerr << "ngtcp2_crypto_wolfssl_configure_client_context failed"
                << std::endl;
      exit(EXIT_FAILURE);
    }
#  endif // HAVE_LIBNGTCP2_CRYPTO_WOLFSSL
#endif   // ENABLE_HTTP3
  } else if (nghttp2::tls::ssl_ctx_set_proto_versions(
               ssl_ctx, nghttp2::tls::NGHTTP2_TLS_MIN_VERSION,
               nghttp2::tls::NGHTTP2_TLS_MAX_VERSION) != 0) {
    std::cerr << "Could not set TLS versions" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (SSL_CTX_set_cipher_list(ssl_ctx, config.ciphers.c_str()) == 0) {
    std::cerr << "SSL_CTX_set_cipher_list with " << config.ciphers
              << " failed: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    exit(EXIT_FAILURE);
  }

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL) || defined(NGHTTP2_OPENSSL_IS_WOLFSSL)
  if (SSL_CTX_set_ciphersuites(ssl_ctx, config.tls13_ciphers.c_str()) == 0) {
    std::cerr << "SSL_CTX_set_ciphersuites with " << config.tls13_ciphers
              << " failed: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    exit(EXIT_FAILURE);
  }
#endif // NGHTTP2_GENUINE_OPENSSL || NGHTTP2_OPENSSL_IS_LIBRESSL ||
       // NGHTTP2_OPENSSL_IS_WOLFSSL

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
  // Passing X25519 to SSL_CTX_set1_groups_list fails for some reason.
  if (SSL_CTX_set1_curves_list(
        ssl_ctx, const_cast<char *>(config.groups.c_str())) != 1) {
    std::cerr << "SSL_CTX_set1_curves_list failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(EXIT_FAILURE);
  }
#else  // !NGHTTP2_OPENSSL_IS_WOLFSSL
  if (SSL_CTX_set1_groups_list(ssl_ctx, config.groups.c_str()) != 1) {
    std::cerr << "SSL_CTX_set1_groups_list failed" << std::endl;
    exit(EXIT_FAILURE);
  }
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

  std::vector<unsigned char> proto_list;
  for (const auto &proto : config.alpn_list) {
    std::copy_n(proto.c_str(), proto.size(), std::back_inserter(proto_list));
  }

  SSL_CTX_set_alpn_protos(ssl_ctx, proto_list.data(), proto_list.size());

  if (tls::setup_keylog_callback(ssl_ctx) != 0) {
    std::cerr << "Failed to setup keylog" << std::endl;

    exit(EXIT_FAILURE);
  }

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
  if (!SSL_CTX_add_cert_compression_alg(
        ssl_ctx, nghttp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
        nghttp2::tls::cert_compress, nghttp2::tls::cert_decompress)) {
    std::cerr << "SSL_CTX_add_cert_compression_alg failed" << std::endl;
    exit(EXIT_FAILURE);
  }
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

  std::string user_agent = "h2load nghttp2/" NGHTTP2_VERSION;
  Headers shared_nva;
  shared_nva.emplace_back(":scheme", config.scheme);
  shared_nva.emplace_back(":authority", make_http_authority(config));
  shared_nva.emplace_back(":method", config.data_fd == -1 ? "GET" : "POST");
  shared_nva.emplace_back("user-agent", user_agent);

  // list header fields that can be overridden.
  auto override_hdrs = std::to_array<std::string_view>(
    {":authority", "host", ":method", ":scheme", "user-agent"});

  for (auto &kv : config.custom_headers) {
    if (std::find(std::begin(override_hdrs), std::end(override_hdrs),
                  kv.name) != std::end(override_hdrs)) {
      // override header
      for (auto &nv : shared_nva) {
        if ((nv.name == ":authority" && kv.name == "host") ||
            (nv.name == kv.name)) {
          nv.value = kv.value;
        }
      }
    } else {
      // add additional headers
      shared_nva.push_back(kv);
    }
  }

  std::string content_length_str;
  if (config.data_fd != -1) {
    content_length_str = util::utos(config.data_length);
  }

  auto method_it =
    std::find_if(std::begin(shared_nva), std::end(shared_nva),
                 [](const Header &nv) { return nv.name == ":method"; });
  assert(method_it != std::end(shared_nva));

  config.h1reqs.reserve(reqlines.size());
  config.nva.reserve(reqlines.size());

  for (auto &req : reqlines) {
    // For HTTP/1.1
    auto h1req = (*method_it).value;
    h1req += ' ';
    h1req += req;
    h1req += " HTTP/1.1\r\n";
    for (auto &nv : shared_nva) {
      if (nv.name == ":authority") {
        h1req += "Host: ";
        h1req += nv.value;
        h1req += "\r\n";
        continue;
      }
      if (nv.name[0] == ':') {
        continue;
      }
      h1req += nv.name;
      h1req += ": ";
      h1req += nv.value;
      h1req += "\r\n";
    }

    if (!content_length_str.empty()) {
      h1req += "Content-Length: ";
      h1req += content_length_str;
      h1req += "\r\n";
    }
    h1req += "\r\n";

    config.h1reqs.push_back(std::move(h1req));

    // For nghttp2
    std::vector<nghttp2_nv> nva;
    // 2 for :path, and possible content-length
    nva.reserve(2 + shared_nva.size());

    nva.push_back(http2::make_field_v(":path"_sr, req));

    for (auto &nv : shared_nva) {
      nva.push_back(http2::make_field_nv(nv.name, nv.value));
    }

    if (!content_length_str.empty()) {
      nva.push_back(
        http2::make_field_nv("content-length"_sr, content_length_str));
    }

    config.nva.push_back(std::move(nva));
  }

  // Don't DOS our server!
  if (config.host == "nghttp2.org") {
    std::cerr << "Using h2load against public server " << config.host
              << " should be prohibited." << std::endl;
    exit(EXIT_FAILURE);
  }

  resolve_host();

  std::cout << "starting benchmark..." << std::endl;

  std::vector<std::unique_ptr<Worker>> workers;
  workers.reserve(config.nthreads);

#ifndef NOTHREADS
  size_t nreqs_per_thread = 0;
  size_t nreqs_rem = 0;

  if (!config.timing_script) {
    nreqs_per_thread = config.nreqs / config.nthreads;
    nreqs_rem = config.nreqs % config.nthreads;
  }

  auto nclients_per_thread = config.nclients / config.nthreads;
  auto nclients_rem = config.nclients % config.nthreads;

  auto rate_per_thread = config.rate / config.nthreads;
  auto rate_per_thread_rem = config.rate % config.nthreads;

  size_t max_samples_per_thread =
    std::max(static_cast<size_t>(256), MAX_SAMPLES / config.nthreads);

  std::mutex mu;
  std::condition_variable cv;
  auto ready = false;

  std::vector<std::future<void>> futures;
  for (size_t i = 0; i < config.nthreads; ++i) {
    auto rate = rate_per_thread;
    if (rate_per_thread_rem > 0) {
      --rate_per_thread_rem;
      ++rate;
    }
    auto nclients = nclients_per_thread;
    if (nclients_rem > 0) {
      --nclients_rem;
      ++nclients;
    }

    size_t nreqs;
    if (config.timing_script) {
      // With timing script, each client issues config.nreqs requests.
      // We divide nreqs by number of clients in Worker ctor to
      // distribute requests to those clients evenly, so multiply
      // config.nreqs here by config.nclients.
      nreqs = config.nreqs * nclients;
    } else {
      nreqs = nreqs_per_thread;
      if (nreqs_rem > 0) {
        --nreqs_rem;
        ++nreqs;
      }
    }

    workers.push_back(
      create_worker(i, ssl_ctx, nreqs, nclients, rate, max_samples_per_thread));
    auto &worker = workers.back();
    futures.push_back(
      std::async(std::launch::async, [&worker, &mu, &cv, &ready]() {
        {
          std::unique_lock<std::mutex> ulk(mu);
          cv.wait(ulk, [&ready] { return ready; });
        }
        worker->run();

#  ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
        wc_ecc_fp_free();
#  endif // NGHTTP2_OPENSSL_IS_WOLFSSL
      }));
  }

  {
    std::lock_guard<std::mutex> lg(mu);
    ready = true;
    cv.notify_all();
  }

  auto start = std::chrono::steady_clock::now();

  for (auto &fut : futures) {
    fut.get();
  }

#else  // NOTHREADS
  auto rate = config.rate;
  auto nclients = config.nclients;
  auto nreqs =
    config.timing_script ? config.nreqs * config.nclients : config.nreqs;

  workers.push_back(
    create_worker(0, ssl_ctx, nreqs, nclients, rate, MAX_SAMPLES));

  auto start = std::chrono::steady_clock::now();

  workers.back()->run();
#endif // NOTHREADS

  auto end = std::chrono::steady_clock::now();
  auto duration =
    std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  Stats stats(0, 0);
  for (const auto &w : workers) {
    const auto &s = w->stats;

    stats.req_todo += s.req_todo;
    stats.req_started += s.req_started;
    stats.req_done += s.req_done;
    stats.req_timedout += s.req_timedout;
    stats.req_success += s.req_success;
    stats.req_status_success += s.req_status_success;
    stats.req_failed += s.req_failed;
    stats.req_error += s.req_error;
    stats.bytes_total += s.bytes_total;
    stats.bytes_head += s.bytes_head;
    stats.bytes_head_decomp += s.bytes_head_decomp;
    stats.bytes_body += s.bytes_body;
    stats.udp_dgram_recv += s.udp_dgram_recv;
    stats.udp_dgram_sent += s.udp_dgram_sent;

    for (size_t i = 0; i < stats.status.size(); ++i) {
      stats.status[i] += s.status[i];
    }
  }

  auto ts = process_time_stats(workers);

  // Requests which have not been issued due to connection errors, are
  // counted towards req_failed and req_error.
  auto req_not_issued =
    (stats.req_todo - stats.req_status_success - stats.req_failed);
  stats.req_failed += req_not_issued;
  stats.req_error += req_not_issued;

  // UI is heavily inspired by weighttp[1] and wrk[2]
  //
  // [1] https://github.com/lighttpd/weighttp
  // [2] https://github.com/wg/wrk
  double rps = 0;
  int64_t bps = 0;
  if (duration.count() > 0) {
    if (config.is_timing_based_mode()) {
      // we only want to consider the main duration if warm-up is given
      rps = stats.req_success / config.duration;
      bps = stats.bytes_total / config.duration;
    } else {
      auto secd = std::chrono::duration_cast<
        std::chrono::duration<double, std::chrono::seconds::period>>(duration);
      rps = stats.req_success / secd.count();
      bps = stats.bytes_total / secd.count();
    }
  }

  double header_space_savings = 0.;
  if (stats.bytes_head_decomp > 0) {
    header_space_savings =
      1. - static_cast<double>(stats.bytes_head) / stats.bytes_head_decomp;
  }

  std::cout << std::fixed << std::setprecision(2) << R"(
finished in )"
            << util::format_duration(duration) << ", " << rps << " req/s, "
            << util::utos_funit(bps) << R"(B/s
requests: )" << stats.req_todo
            << " total, " << stats.req_started << " started, " << stats.req_done
            << " done, " << stats.req_status_success << " succeeded, "
            << stats.req_failed << " failed, " << stats.req_error
            << " errored, " << stats.req_timedout << R"( timeout
status codes: )"
            << stats.status[2] << " 2xx, " << stats.status[3] << " 3xx, "
            << stats.status[4] << " 4xx, " << stats.status[5] << R"( 5xx
traffic: )" << util::utos_funit(stats.bytes_total)
            << "B (" << stats.bytes_total << ") total, "
            << util::utos_funit(stats.bytes_head) << "B (" << stats.bytes_head
            << ") headers (space savings " << header_space_savings * 100
            << "%), " << util::utos_funit(stats.bytes_body) << "B ("
            << stats.bytes_body << R"() data)" << std::endl;
#ifdef ENABLE_HTTP3
  if (config.is_quic()) {
    std::cout << "UDP datagram: " << stats.udp_dgram_sent << " sent, "
              << stats.udp_dgram_recv << " received" << std::endl;
  }
#endif // ENABLE_HTTP3
  std::cout
    << R"(                     min         max         mean         sd        +/- sd
time for request: )"
    << std::setw(10) << util::format_duration(ts.request.min) << "  "
    << std::setw(10) << util::format_duration(ts.request.max) << "  "
    << std::setw(10) << util::format_duration(ts.request.mean) << "  "
    << std::setw(10) << util::format_duration(ts.request.sd) << std::setw(9)
    << util::dtos(ts.request.within_sd) << "%"
    << "\ntime for connect: " << std::setw(10)
    << util::format_duration(ts.connect.min) << "  " << std::setw(10)
    << util::format_duration(ts.connect.max) << "  " << std::setw(10)
    << util::format_duration(ts.connect.mean) << "  " << std::setw(10)
    << util::format_duration(ts.connect.sd) << std::setw(9)
    << util::dtos(ts.connect.within_sd) << "%"
    << "\ntime to 1st byte: " << std::setw(10)
    << util::format_duration(ts.ttfb.min) << "  " << std::setw(10)
    << util::format_duration(ts.ttfb.max) << "  " << std::setw(10)
    << util::format_duration(ts.ttfb.mean) << "  " << std::setw(10)
    << util::format_duration(ts.ttfb.sd) << std::setw(9)
    << util::dtos(ts.ttfb.within_sd) << "%"
    << "\nreq/s           : " << std::setw(10) << ts.rps.min << "  "
    << std::setw(10) << ts.rps.max << "  " << std::setw(10) << ts.rps.mean
    << "  " << std::setw(10) << ts.rps.sd << std::setw(9)
    << util::dtos(ts.rps.within_sd) << "%" << std::endl;

  SSL_CTX_free(ssl_ctx);

  if (config.log_fd != -1) {
    close(config.log_fd);
  }

  return 0;
}

} // namespace h2load

int main(int argc, char **argv) { return h2load::main(argc, argv); }
