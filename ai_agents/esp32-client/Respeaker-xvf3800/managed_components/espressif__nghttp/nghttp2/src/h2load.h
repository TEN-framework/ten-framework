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
#ifndef H2LOAD_H
#define H2LOAD_H

#include "nghttp2_config.h"

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H
#include <sys/un.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <array>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#ifdef ENABLE_HTTP3
#  include <ngtcp2/ngtcp2.h>
#  include <ngtcp2/ngtcp2_crypto.h>
#endif // ENABLE_HTTP3

#include <ev.h>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include "http2.h"
#ifdef ENABLE_HTTP3
#  include "quic.h"
#endif // ENABLE_HTTP3
#include "memchunk.h"
#include "template.h"

using namespace nghttp2;

namespace h2load {

constexpr auto BACKOFF_WRITE_BUFFER_THRES = 16_k;

class Session;
struct Worker;

struct Config {
  std::vector<std::vector<nghttp2_nv>> nva;
  std::vector<std::string> h1reqs;
  std::vector<std::chrono::steady_clock::duration> timings;
  nghttp2::Headers custom_headers;
  std::string scheme;
  std::string host;
  std::string connect_to_host;
  std::string ifile;
  std::string ciphers;
  std::string tls13_ciphers;
  // supported groups (or curves).
  std::string groups;
  // length of upload data
  int64_t data_length;
  // memory mapped upload data
  uint8_t *data;
  addrinfo *addrs;
  size_t nreqs;
  size_t nclients;
  size_t nthreads;
  // The maximum number of concurrent streams per session.
  size_t max_concurrent_streams;
  size_t window_bits;
  size_t connection_window_bits;
  size_t max_frame_size;
  // rate at which connections should be made
  size_t rate;
  ev_tstamp rate_period;
  // amount of time for main measurements in timing-based test
  ev_tstamp duration;
  // amount of time to wait before starting measurements in timing-based test
  ev_tstamp warm_up_time;
  // amount of time to wait for activity on a given connection
  ev_tstamp conn_active_timeout;
  // amount of time to wait after the last request is made on a connection
  ev_tstamp conn_inactivity_timeout;
  enum { PROTO_HTTP2, PROTO_HTTP1_1 } no_tls_proto;
  uint32_t header_table_size;
  uint32_t encoder_header_table_size;
  // file descriptor for upload data
  int data_fd;
  // file descriptor to write per-request stats to.
  int log_fd;
  // base file name of qlog output files
  std::string qlog_file_base;
  uint16_t port;
  uint16_t default_port;
  uint16_t connect_to_port;
  bool verbose;
  bool timing_script;
  std::string base_uri;
  // true if UNIX domain socket is used.  In this case, base_uri is
  // not used in usual way.
  bool base_uri_unix;
  // used when UNIX domain socket is used (base_uri_unix is true).
  sockaddr_un unix_addr;
  // list of supported ALPN protocol strings in the order of
  // preference.
  std::vector<std::string> alpn_list;
  // The number of request per second for each client.
  double rps;
  // Disables GSO for UDP connections.
  bool no_udp_gso;
  // The maximum UDP datagram payload size to send.
  size_t max_udp_payload_size;
  // Enable ktls.
  bool ktls;
  // sni is the value sent in TLS SNI, overriding DNS name of the
  // remote host.
  std::string sni;

  Config();
  ~Config();

  bool is_rate_mode() const;
  bool is_timing_based_mode() const;
  bool has_base_uri() const;
  bool rps_enabled() const;
  bool is_quic() const;
};

struct RequestStat {
  // time point when request was sent
  std::chrono::steady_clock::time_point request_time;
  // same, but in wall clock reference frame
  std::chrono::system_clock::time_point request_wall_time;
  // time point when stream was closed
  std::chrono::steady_clock::time_point stream_close_time;
  // upload data length sent so far
  int64_t data_offset;
  // HTTP status code
  int status;
  // true if stream was successfully closed.  This means stream was
  // not reset, but it does not mean HTTP level error (e.g., 404).
  bool completed;
};

struct ClientStat {
  // time client started (i.e., first connect starts)
  std::chrono::steady_clock::time_point client_start_time;
  // time client end (i.e., client somehow processed all requests it
  // is responsible for, and disconnected)
  std::chrono::steady_clock::time_point client_end_time;
  // The number of requests completed successful, but not necessarily
  // means successful HTTP status code.
  size_t req_success;

  // The following 3 numbers are overwritten each time when connection
  // is made.

  // time connect starts
  std::chrono::steady_clock::time_point connect_start_time;
  // time to connect
  std::chrono::steady_clock::time_point connect_time;
  // time to first byte (TTFB)
  std::chrono::steady_clock::time_point ttfb;
};

struct SDStat {
  // min, max, mean and sd (standard deviation)
  double min, max, mean, sd;
  // percentage of samples inside mean -/+ sd
  double within_sd;
};

struct SDStats {
  // time for request
  SDStat request;
  // time for connect
  SDStat connect;
  // time to first byte (TTFB)
  SDStat ttfb;
  // request per second for each client
  SDStat rps;
};

struct Stats {
  Stats(size_t req_todo, size_t nclients);
  // The total number of requests
  size_t req_todo;
  // The number of requests issued so far
  size_t req_started;
  // The number of requests finished
  size_t req_done;
  // The number of requests completed successful, but not necessarily
  // means successful HTTP status code.
  size_t req_success;
  // The number of requests marked as success.  HTTP status code is
  // also considered as success. This is subset of req_done.
  size_t req_status_success;
  // The number of requests failed. This is subset of req_done.
  size_t req_failed;
  // The number of requests failed due to network errors. This is
  // subset of req_failed.
  size_t req_error;
  // The number of requests that failed due to timeout.
  size_t req_timedout;
  // The number of bytes received on the "wire". If SSL/TLS is used,
  // this is the number of decrypted bytes the application received.
  int64_t bytes_total;
  // The number of bytes received for header fields.  This is
  // compressed version.
  int64_t bytes_head;
  // The number of bytes received for header fields after they are
  // decompressed.
  int64_t bytes_head_decomp;
  // The number of bytes received in DATA frame.
  int64_t bytes_body;
  // The number of each HTTP status category, status[i] is status code
  // in the range [i*100, (i+1)*100).
  std::array<size_t, 6> status;
  // The statistics per request
  std::vector<RequestStat> req_stats;
  // The statistics per client
  std::vector<ClientStat> client_stats;
  // The number of UDP datagrams received.
  size_t udp_dgram_recv;
  // The number of UDP datagrams sent.
  size_t udp_dgram_sent;
};

enum ClientState { CLIENT_IDLE, CLIENT_CONNECTED };

// This type tells whether the client is in warmup phase or not or is over
enum class Phase {
  INITIAL_IDLE,  // Initial idle state before warm-up phase
  WARM_UP,       // Warm up phase when no measurements are done
  MAIN_DURATION, // Main measurement phase; if timing-based
                 // test is not run, this is the default phase
  DURATION_OVER  // This phase occurs after the measurements are over
};

struct Client;

// We use reservoir sampling method
struct Sampling {
  // maximum number of samples
  size_t max_samples;
  // number of samples seen, including discarded samples.
  size_t n;
};

struct Worker {
  MemchunkPool mcpool;
  std::mt19937 randgen;
  Stats stats;
  Sampling request_times_smp;
  Sampling client_smp;
  struct ev_loop *loop;
  SSL_CTX *ssl_ctx;
  Config *config;
  size_t progress_interval;
  uint32_t id;
  bool tls_info_report_done;
  bool app_info_report_done;
  size_t nconns_made;
  // number of clients this worker handles
  size_t nclients;
  // number of requests each client issues
  size_t nreqs_per_client;
  // at most nreqs_rem clients get an extra request
  size_t nreqs_rem;
  size_t rate;
  // maximum number of samples in this worker thread
  size_t max_samples;
  ev_timer timeout_watcher;
  // The next client ID this worker assigns
  uint32_t next_client_id;
  // Keeps track of the current phase (for timing-based experiment) for the
  // worker
  Phase current_phase;
  // We need to keep track of the clients in order to stop them when needed
  std::vector<Client *> clients;
  // This is only active when there is not a bounded number of requests
  // specified
  ev_timer duration_watcher;
  ev_timer warmup_watcher;

  Worker(uint32_t id, SSL_CTX *ssl_ctx, size_t nreq_todo, size_t nclients,
         size_t rate, size_t max_samples, Config *config);
  ~Worker();
  Worker(Worker &&o) = default;
  void run();
  void sample_req_stat(RequestStat *req_stat);
  void sample_client_stat(ClientStat *cstat);
  void report_progress();
  void report_rate_progress();
  // This function calls the destructors of all the clients.
  void stop_all_clients();
  // This function frees a client from the list of clients for this Worker.
  void free_client(Client *);
};

struct Stream {
  RequestStat req_stat;
  int status_success;
  Stream();
};

struct Client {
  DefaultMemchunks wb;
  std::unordered_map<int32_t, Stream> streams;
  ClientStat cstat;
  std::unique_ptr<Session> session;
  ev_io wev;
  ev_io rev;
  std::function<int(Client &)> readfn, writefn;
  Worker *worker;
  SSL *ssl;
#ifdef ENABLE_HTTP3
  struct {
    ngtcp2_crypto_conn_ref conn_ref;
    ev_timer pkt_timer;
    ngtcp2_conn *conn;
    ngtcp2_ccerr last_error;
    bool close_requested;
    FILE *qlog_file;

    struct {
      bool send_blocked;
      size_t num_blocked;
      size_t num_blocked_sent;
      struct {
        Address remote_addr;
        const uint8_t *data;
        size_t datalen;
        size_t gso_size;
      } blocked[2];
      std::unique_ptr<uint8_t[]> data;
    } tx;
  } quic;
#endif // ENABLE_HTTP3
  ev_timer request_timeout_watcher;
  addrinfo *next_addr;
  // Address for the current address.  When try_new_connection() is
  // used and current_addr is not nullptr, it is used instead of
  // trying next address though next_addr.  To try new address, set
  // nullptr to current_addr before calling connect().
  addrinfo *current_addr;
  size_t reqidx;
  ClientState state;
  // The number of requests this client has to issue.
  size_t req_todo;
  // The number of requests left to issue
  size_t req_left;
  // The number of requests currently have started, but not abandoned
  // or finished.
  size_t req_inflight;
  // The number of requests this client has issued so far.
  size_t req_started;
  // The number of requests this client has done so far.
  size_t req_done;
  // The client id per worker
  uint32_t id;
  int fd;
  Address local_addr;
  ev_timer conn_active_watcher;
  ev_timer conn_inactivity_watcher;
  std::string selected_proto;
  bool new_connection_requested;
  // true if the current connection will be closed, and no more new
  // request cannot be processed.
  bool final;
  // rps_watcher is a timer to invoke callback periodically to
  // generate a new request.
  ev_timer rps_watcher;
  // The timestamp that starts the period which contributes to the
  // next request generation.
  std::chrono::steady_clock::time_point rps_duration_started;
  // The number of requests allowed by rps, but limited by stream
  // concurrency.
  size_t rps_req_pending;
  // The number of in-flight streams.  req_inflight has similar value
  // but it only measures requests made during Phase::MAIN_DURATION.
  // rps_req_inflight measures the number of requests in all phases,
  // and it is only used if --rps is given.
  size_t rps_req_inflight;

  enum { ERR_CONNECT_FAIL = -100 };

  Client(uint32_t id, Worker *worker, size_t req_todo);
  ~Client();
  int make_socket(addrinfo *addr);
  int connect();
  void disconnect();
  void fail();
  // Call this function when do_read() returns -1.  This function
  // tries to connect to the remote host again if it is requested.  If
  // so, this function returns 0, and this object should be retained.
  // Otherwise, this function returns -1, and this object should be
  // deleted.
  int try_again_or_fail();
  void timeout();
  void restart_timeout();
  int submit_request();
  void process_request_failure();
  void process_timedout_streams();
  void process_abandoned_streams();
  void report_tls_info();
  void report_app_info();
  void terminate_session();
  // Asks client to create new connection, instead of just fail.
  void try_new_connection();

  int do_read();
  int do_write();

  // low-level I/O callback functions called by do_read/do_write
  int connected();
  int read_clear();
  int write_clear();
  int tls_handshake();
  int read_tls();
  int write_tls();

  int on_read(const uint8_t *data, size_t len);
  int on_write();

  int connection_made();

  void on_request(int32_t stream_id);
  void on_header(int32_t stream_id, const uint8_t *name, size_t namelen,
                 const uint8_t *value, size_t valuelen);
  void on_status_code(int32_t stream_id, uint16_t status);
  // |success| == true means that the request/response was exchanged
  // |successfully, but it does not mean response carried successful
  // |HTTP status code.
  void on_stream_close(int32_t stream_id, bool success, bool final = false);
  // Returns RequestStat for |stream_id|.  This function must be
  // called after on_request(stream_id), and before
  // on_stream_close(stream_id, ...).  Otherwise, this will return
  // nullptr.
  RequestStat *get_req_stat(int32_t stream_id);

  void record_request_time(RequestStat *req_stat);
  void record_connect_start_time();
  void record_connect_time();
  void record_ttfb();
  void clear_connect_times();
  void record_client_start_time();
  void record_client_end_time();

  void signal_write();

#ifdef ENABLE_HTTP3
  // QUIC
  int quic_init(const sockaddr *local_addr, socklen_t local_addrlen,
                const sockaddr *remote_addr, socklen_t remote_addrlen);
  void quic_free();
  int read_quic();
  int write_quic();
  int write_udp(const sockaddr *addr, socklen_t addrlen, const uint8_t *data,
                size_t datalen, size_t gso_size);
  void on_send_blocked(const ngtcp2_addr &remote_addr, const uint8_t *data,
                       size_t datalen, size_t gso_size);
  int send_blocked_packet();
  void quic_close_connection();

  int quic_handshake_completed();
  int quic_recv_stream_data(uint32_t flags, int64_t stream_id,
                            const uint8_t *data, size_t datalen);
  int quic_acked_stream_data_offset(int64_t stream_id, size_t datalen);
  int quic_stream_close(int64_t stream_id, uint64_t app_error_code);
  int quic_stream_reset(int64_t stream_id, uint64_t app_error_code);
  int quic_stream_stop_sending(int64_t stream_id, uint64_t app_error_code);
  int quic_extend_max_local_streams();
  int quic_extend_max_stream_data(int64_t stream_id);

  int quic_write_client_handshake(ngtcp2_encryption_level level,
                                  const uint8_t *data, size_t datalen);
  int quic_pkt_timeout();
  void quic_restart_pkt_timer();
  void quic_write_qlog(const void *data, size_t datalen);
  int quic_make_http3_session();
#endif // ENABLE_HTTP3
};

} // namespace h2load

#endif // H2LOAD_H
