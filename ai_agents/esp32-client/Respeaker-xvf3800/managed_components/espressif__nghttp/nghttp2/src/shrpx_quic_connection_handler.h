/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2021 Tatsuhiro Tsujikawa
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
#ifndef SHRPX_QUIC_CONNECTION_HANDLER_H
#define SHRPX_QUIC_CONNECTION_HANDLER_H

#include "shrpx.h"

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

#include <ngtcp2/ngtcp2.h>

#include <ev.h>

#include "shrpx_quic.h"
#include "network.h"

using namespace nghttp2;

namespace shrpx {

struct UpstreamAddr;
class ClientHandler;
class Worker;

// CloseWait handles packets received in close-wait (draining or
// closing period).
struct CloseWait {
  CloseWait(Worker *worker, std::vector<ngtcp2_cid> scids,
            std::vector<uint8_t> pkt, ev_tstamp period);
  ~CloseWait();

  int handle_packet(const UpstreamAddr *faddr, const Address &remote_addr,
                    const Address &local_addr, const ngtcp2_pkt_info &pi,
                    std::span<const uint8_t> data);

  Worker *worker;
  // Source Connection IDs of the connection.
  std::vector<ngtcp2_cid> scids;
  // QUIC packet which is sent in response to the incoming packet.  It
  // might be empty.
  std::vector<uint8_t> pkt;
  // Close-wait (draining or closing period) timer.
  ev_timer timer;
  // The number of bytes received during close-wait period.
  size_t bytes_recv;
  // The number of bytes sent during close-wait period.
  size_t bytes_sent;
  // The number of packets received during close-wait period.
  size_t num_pkts_recv;
  // If the number of packets received reaches this number, send a
  // QUIC packet.
  size_t next_pkts_recv;
};

class QUICConnectionHandler {
public:
  QUICConnectionHandler(Worker *worker);
  ~QUICConnectionHandler();
  int handle_packet(const UpstreamAddr *faddr, const Address &remote_addr,
                    const Address &local_addr, const ngtcp2_pkt_info &pi,
                    std::span<const uint8_t> data);
  // Send Retry packet.  |ini_dcid| is the destination Connection ID
  // which appeared in Client Initial packet.  |ini_scid| is the
  // source Connection ID which appeared in Client Initial packet.
  int send_retry(const UpstreamAddr *faddr, uint32_t version,
                 std::span<const uint8_t> ini_dcid,
                 std::span<const uint8_t> ini_scid, const Address &remote_addr,
                 const Address &local_addr, size_t max_pktlen);
  // Send Version Negotiation packet.  |ini_dcid| is the destination
  // Connection ID which appeared in Client Initial packet.
  // |ini_scid| is the source Connection ID which appeared in Client
  // Initial packet.
  int send_version_negotiation(const UpstreamAddr *faddr, uint32_t version,
                               std::span<const uint8_t> ini_dcid,
                               std::span<const uint8_t> ini_scid,
                               const Address &remote_addr,
                               const Address &local_addr);
  int send_stateless_reset(const UpstreamAddr *faddr, size_t pktlen,
                           std::span<const uint8_t> dcid,
                           const Address &remote_addr,
                           const Address &local_addr);
  // Send Initial CONNECTION_CLOSE.  |ini_dcid| is the destination
  // Connection ID which appeared in Client Initial packet.
  // |ini_scid| is the source Connection ID which appeared in Client
  // Initial packet.
  int send_connection_close(const UpstreamAddr *faddr, uint32_t version,
                            const ngtcp2_cid &ini_dcid,
                            const ngtcp2_cid &ini_scid,
                            const Address &remote_addr,
                            const Address &local_addr, uint64_t error_code,
                            size_t max_pktlen);
  ClientHandler *
  handle_new_connection(const UpstreamAddr *faddr, const Address &remote_addr,
                        const Address &local_addr, const ngtcp2_pkt_hd &hd,
                        const ngtcp2_cid *odcid, std::span<const uint8_t> token,
                        ngtcp2_token_type token_type);
  void add_connection_id(const ngtcp2_cid &cid, ClientHandler *handler);
  void remove_connection_id(const ngtcp2_cid &cid);

  void add_close_wait(CloseWait *cw);
  void remove_close_wait(const CloseWait *cw);

  void on_stateless_reset_bucket_regen();

private:
  Worker *worker_;
  std::unordered_map<ngtcp2_cid, ClientHandler *> connections_;
  std::unordered_map<ngtcp2_cid, CloseWait *> close_waits_;
  ev_timer stateless_reset_bucket_regen_timer_;
  size_t stateless_reset_bucket_;
};

} // namespace shrpx

#endif // SHRPX_QUIC_CONNECTION_HANDLER_H
