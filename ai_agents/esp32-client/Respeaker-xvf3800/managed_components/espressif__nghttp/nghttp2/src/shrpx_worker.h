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
#ifndef SHRPX_WORKER_H
#define SHRPX_WORKER_H

#include "shrpx.h"

#include <mutex>
#include <vector>
#include <random>
#include <unordered_map>
#include <deque>
#include <thread>
#include <queue>
#ifndef NOTHREADS
#  include <future>
#endif // NOTHREADS

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#  include <wolfssl/openssl/err.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ev.h>

#include "shrpx_config.h"
#include "shrpx_downstream_connection_pool.h"
#include "memchunk.h"
#include "shrpx_tls.h"
#include "shrpx_live_check.h"
#include "shrpx_connect_blocker.h"
#include "shrpx_dns_tracker.h"
#ifdef ENABLE_HTTP3
#  include "shrpx_quic_connection_handler.h"
#  include "shrpx_quic.h"
#endif // ENABLE_HTTP3
#include "allocator.h"

using namespace nghttp2;

namespace shrpx {

class Http2Session;
class ConnectBlocker;
class MemcachedDispatcher;
struct UpstreamAddr;
class ConnectionHandler;
#ifdef ENABLE_HTTP3
class QUICListener;
#endif // ENABLE_HTTP3

#ifdef HAVE_MRUBY
namespace mruby {

class MRubyContext;

} // namespace mruby
#endif // HAVE_MRUBY

namespace tls {
class CertLookupTree;
} // namespace tls

struct WeightGroup;

struct DownstreamAddr {
  Address addr;
  // backend address.  If |host_unix| is true, this is UNIX domain
  // socket path.
  StringRef host;
  StringRef hostport;
  // backend port.  0 if |host_unix| is true.
  uint16_t port;
  // true if |host| contains UNIX domain socket path.
  bool host_unix;

  // sni field to send remote server if TLS is enabled.
  StringRef sni;

  std::unique_ptr<ConnectBlocker> connect_blocker;
  std::unique_ptr<LiveCheck> live_check;
  // Connection pool for this particular address if session affinity
  // is enabled
  std::unique_ptr<DownstreamConnectionPool> dconn_pool;
  size_t fall;
  size_t rise;
  // Client side TLS session cache
  tls::TLSSessionCache tls_session_cache;
  // List of Http2Session which is not fully utilized (i.e., the
  // server advertised maximum concurrency is not reached).  We will
  // coalesce as much stream as possible in one Http2Session to fully
  // utilize TCP connection.
  DList<Http2Session> http2_extra_freelist;
  WeightGroup *wg;
  // total number of streams created in HTTP/2 connections for this
  // address.
  size_t num_dconn;
  // the sequence number of this address to randomize the order access
  // threads.
  size_t seq;
  // Application protocol used in this backend
  Proto proto;
  // cycle is used to prioritize this address.  Lower value takes
  // higher priority.
  uint32_t cycle;
  // penalty which is applied to the next cycle calculation.
  uint32_t pending_penalty;
  // Weight of this address inside a weight group.  Its range is [1,
  // 256], inclusive.
  uint32_t weight;
  // name of group which this address belongs to.
  StringRef group;
  // Weight of the weight group which this address belongs to.  Its
  // range is [1, 256], inclusive.
  uint32_t group_weight;
  // affinity hash for this address.  It is assigned when strict
  // stickiness is enabled.
  uint32_t affinity_hash;
  // true if TLS is used in this backend
  bool tls;
  // true if dynamic DNS is enabled
  bool dns;
  // true if :scheme pseudo header field should be upgraded to secure
  // variant (e.g., "https") when forwarding request to a backend
  // connected by TLS connection.
  bool upgrade_scheme;
  // true if this address is queued.
  bool queued;
};

constexpr uint32_t MAX_DOWNSTREAM_ADDR_WEIGHT = 256;

struct DownstreamAddrEntry {
  DownstreamAddr *addr;
  size_t seq;
  uint32_t cycle;
};

struct DownstreamAddrEntryGreater {
  bool operator()(const DownstreamAddrEntry &lhs,
                  const DownstreamAddrEntry &rhs) const {
    auto d = lhs.cycle - rhs.cycle;
    if (d == 0) {
      return rhs.seq < lhs.seq;
    }
    return d <= 2 * MAX_DOWNSTREAM_ADDR_WEIGHT - 1;
  }
};

struct WeightGroup {
  std::priority_queue<DownstreamAddrEntry, std::vector<DownstreamAddrEntry>,
                      DownstreamAddrEntryGreater>
    pq;
  size_t seq;
  uint32_t weight;
  uint32_t cycle;
  uint32_t pending_penalty;
  // true if this object is queued.
  bool queued;
};

struct WeightGroupEntry {
  WeightGroup *wg;
  size_t seq;
  uint32_t cycle;
};

struct WeightGroupEntryGreater {
  bool operator()(const WeightGroupEntry &lhs,
                  const WeightGroupEntry &rhs) const {
    auto d = lhs.cycle - rhs.cycle;
    if (d == 0) {
      return rhs.seq < lhs.seq;
    }
    return d <= 2 * MAX_DOWNSTREAM_ADDR_WEIGHT - 1;
  }
};

struct SharedDownstreamAddr {
  SharedDownstreamAddr()
    : balloc(1024, 1024),
      affinity{SessionAffinity::NONE},
      redirect_if_not_tls{false},
      dnf{false},
      timeout{} {}

  SharedDownstreamAddr(const SharedDownstreamAddr &) = delete;
  SharedDownstreamAddr(SharedDownstreamAddr &&) = delete;
  SharedDownstreamAddr &operator=(const SharedDownstreamAddr &) = delete;
  SharedDownstreamAddr &operator=(SharedDownstreamAddr &&) = delete;

  BlockAllocator balloc;
  std::vector<DownstreamAddr> addrs;
  std::vector<WeightGroup> wgs;
  std::priority_queue<WeightGroupEntry, std::vector<WeightGroupEntry>,
                      WeightGroupEntryGreater>
    pq;
  // Bunch of session affinity hash.  Only used if affinity ==
  // SessionAffinity::IP.
  std::vector<AffinityHash> affinity_hash;
  // Maps affinity hash of each DownstreamAddr to its index in addrs.
  // It is only assigned when strict stickiness is enabled.
  std::unordered_map<uint32_t, size_t> affinity_hash_map;
#ifdef HAVE_MRUBY
  std::shared_ptr<mruby::MRubyContext> mruby_ctx;
#endif // HAVE_MRUBY
  // Configuration for session affinity
  AffinityConfig affinity;
  // Session affinity
  // true if this group requires that client connection must be TLS,
  // and the request must be redirected to https URI.
  bool redirect_if_not_tls;
  // true if a request should not be forwarded to a backend.
  bool dnf;
  // Timeouts for backend connection.
  struct {
    ev_tstamp read;
    ev_tstamp write;
  } timeout;
};

struct DownstreamAddrGroup {
  DownstreamAddrGroup();
  ~DownstreamAddrGroup();

  DownstreamAddrGroup(const DownstreamAddrGroup &) = delete;
  DownstreamAddrGroup(DownstreamAddrGroup &&) = delete;
  DownstreamAddrGroup &operator=(const DownstreamAddrGroup &) = delete;
  DownstreamAddrGroup &operator=(DownstreamAddrGroup &&) = delete;

  ImmutableString pattern;
  std::shared_ptr<SharedDownstreamAddr> shared_addr;
  // true if this group is no longer used for new request.  If this is
  // true, the connection made using one of address in shared_addr
  // must not be pooled.
  bool retired;
};

struct WorkerStat {
  size_t num_connections;
  size_t num_close_waits;
};

#ifdef ENABLE_HTTP3
struct QUICPacket {
  QUICPacket(size_t upstream_addr_index, const Address &remote_addr,
             const Address &local_addr, const ngtcp2_pkt_info &pi,
             std::span<const uint8_t> data)
    : upstream_addr_index{upstream_addr_index},
      remote_addr{remote_addr},
      local_addr{local_addr},
      pi{pi},
      data{std::begin(data), std::end(data)} {}
  QUICPacket() : upstream_addr_index{}, remote_addr{}, local_addr{}, pi{} {}
  size_t upstream_addr_index;
  Address remote_addr;
  Address local_addr;
  ngtcp2_pkt_info pi;
  std::vector<uint8_t> data;
};
#endif // ENABLE_HTTP3

enum class WorkerEventType {
  NEW_CONNECTION = 0x01,
  REOPEN_LOG = 0x02,
  GRACEFUL_SHUTDOWN = 0x03,
  REPLACE_DOWNSTREAM = 0x04,
#ifdef ENABLE_HTTP3
  QUIC_PKT_FORWARD = 0x05,
#endif // ENABLE_HTTP3
};

struct WorkerEvent {
  WorkerEventType type;
  struct {
    sockaddr_union client_addr;
    size_t client_addrlen;
    int client_fd;
    const UpstreamAddr *faddr;
  };
  std::shared_ptr<TicketKeys> ticket_keys;
  std::shared_ptr<DownstreamConfig> downstreamconf;
#ifdef ENABLE_HTTP3
  std::unique_ptr<QUICPacket> quic_pkt;
#endif // ENABLE_HTTP3
};

class Worker {
public:
  Worker(struct ev_loop *loop, SSL_CTX *sv_ssl_ctx, SSL_CTX *cl_ssl_ctx,
         SSL_CTX *tls_session_cache_memcached_ssl_ctx,
         tls::CertLookupTree *cert_tree,
#ifdef ENABLE_HTTP3
         SSL_CTX *quic_sv_ssl_ctx, tls::CertLookupTree *quic_cert_tree,
         WorkerID wid,
#  ifdef HAVE_LIBBPF
         size_t index,
#  endif // HAVE_LIBBPF
#endif   // ENABLE_HTTP3
         const std::shared_ptr<TicketKeys> &ticket_keys,
         ConnectionHandler *conn_handler,
         std::shared_ptr<DownstreamConfig> downstreamconf);
  ~Worker();
  void run_async();
  void wait();
  void process_events();
  void send(WorkerEvent event);

  tls::CertLookupTree *get_cert_lookup_tree() const;
#ifdef ENABLE_HTTP3
  tls::CertLookupTree *get_quic_cert_lookup_tree() const;
#endif // ENABLE_HTTP3

  // These 2 functions make a lock m_ to get/set ticket keys
  // atomically.
  std::shared_ptr<TicketKeys> get_ticket_keys();
  void set_ticket_keys(std::shared_ptr<TicketKeys> ticket_keys);

  WorkerStat *get_worker_stat();
  struct ev_loop *get_loop() const;
  SSL_CTX *get_sv_ssl_ctx() const;
  SSL_CTX *get_cl_ssl_ctx() const;
#ifdef ENABLE_HTTP3
  SSL_CTX *get_quic_sv_ssl_ctx() const;
#endif // ENABLE_HTTP3

  void set_graceful_shutdown(bool f);
  bool get_graceful_shutdown() const;

  MemchunkPool *get_mcpool();
  void schedule_clear_mcpool();

  MemcachedDispatcher *get_session_cache_memcached_dispatcher();

  std::mt19937 &get_randgen();

#ifdef HAVE_MRUBY
  int create_mruby_context();

  mruby::MRubyContext *get_mruby_context() const;
#endif // HAVE_MRUBY

  std::vector<std::shared_ptr<DownstreamAddrGroup>> &
  get_downstream_addr_groups();

  ConnectBlocker *get_connect_blocker() const;

  const DownstreamConfig *get_downstream_config() const;

  void
  replace_downstream_config(std::shared_ptr<DownstreamConfig> downstreamconf);

  ConnectionHandler *get_connection_handler() const;

#ifdef ENABLE_HTTP3
  QUICConnectionHandler *get_quic_connection_handler();

  int setup_quic_server_socket();

  const WorkerID &get_worker_id() const;

#  ifdef HAVE_LIBBPF
  bool should_attach_bpf() const;

  bool should_update_bpf_map() const;

  uint32_t compute_sk_index() const;
#  endif // HAVE_LIBBPF

  int create_quic_server_socket(UpstreamAddr &addr);

  // Returns a pointer to UpstreamAddr which matches |local_addr|.
  const UpstreamAddr *find_quic_upstream_addr(const Address &local_addr);
#endif // ENABLE_HTTP3

  DNSTracker *get_dns_tracker();

private:
#ifndef NOTHREADS
  std::future<void> fut_;
#endif // NOTHREADS
#if defined(ENABLE_HTTP3) && defined(HAVE_LIBBPF)
  // Unique index of this worker.
  size_t index_;
#endif // ENABLE_HTTP3 && HAVE_LIBBPF
  std::mutex m_;
  std::deque<WorkerEvent> q_;
  std::mt19937 randgen_;
  ev_async w_;
  ev_timer mcpool_clear_timer_;
  ev_timer proc_wev_timer_;
  MemchunkPool mcpool_;
  WorkerStat worker_stat_;
  DNSTracker dns_tracker_;

#ifdef ENABLE_HTTP3
  WorkerID worker_id_;
  std::vector<UpstreamAddr> quic_upstream_addrs_;
  std::vector<std::unique_ptr<QUICListener>> quic_listeners_;
#endif // ENABLE_HTTP3

  std::shared_ptr<DownstreamConfig> downstreamconf_;
  std::unique_ptr<MemcachedDispatcher> session_cache_memcached_dispatcher_;
#ifdef HAVE_MRUBY
  std::unique_ptr<mruby::MRubyContext> mruby_ctx_;
#endif // HAVE_MRUBY
  struct ev_loop *loop_;

  // Following fields are shared across threads if
  // get_config()->tls_ctx_per_worker == true.
  SSL_CTX *sv_ssl_ctx_;
  SSL_CTX *cl_ssl_ctx_;
  tls::CertLookupTree *cert_tree_;
  ConnectionHandler *conn_handler_;
#ifdef ENABLE_HTTP3
  SSL_CTX *quic_sv_ssl_ctx_;
  tls::CertLookupTree *quic_cert_tree_;

  QUICConnectionHandler quic_conn_handler_;
#endif // ENABLE_HTTP3

#ifdef HAVE_ATOMIC_STD_SHARED_PTR
  std::atomic<std::shared_ptr<TicketKeys>> ticket_keys_;
#else  // !HAVE_ATOMIC_STD_SHARED_PTR
  std::mutex ticket_keys_m_;
  std::shared_ptr<TicketKeys> ticket_keys_;
#endif // !HAVE_ATOMIC_STD_SHARED_PTR
  std::vector<std::shared_ptr<DownstreamAddrGroup>> downstream_addr_groups_;
  // Worker level blocker for downstream connection.  For example,
  // this is used when file descriptor is exhausted.
  std::unique_ptr<ConnectBlocker> connect_blocker_;

  bool graceful_shutdown_;
};

// Selects group based on request's |hostport| and |path|.  |hostport|
// is the value taken from :authority or host header field, and may
// contain port.  The |path| may contain query part.  We require the
// catch-all pattern in place, so this function always selects one
// group.  The catch-all group index is given in |catch_all|.  All
// patterns are given in |groups|.
size_t match_downstream_addr_group(
  const RouterConfig &routerconfig, const StringRef &hostport,
  const StringRef &path,
  const std::vector<std::shared_ptr<DownstreamAddrGroup>> &groups,
  size_t catch_all, BlockAllocator &balloc);

// Calls this function if connecting to backend failed.  |raddr| is
// the actual address used to connect to backend, and it could be
// nullptr.  This function may schedule live check.
void downstream_failure(DownstreamAddr *addr, const Address *raddr);

} // namespace shrpx

#endif // SHRPX_WORKER_H
