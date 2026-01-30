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
#ifndef SHRPX_QUIC_H
#define SHRPX_QUIC_H

#include "shrpx.h"

#include <stdint.h>

#include <functional>
#include <optional>
#include <span>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/evp.h>
#  include <wolfssl/openssl/rand.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/evp.h>
#  include <openssl/rand.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ngtcp2/ngtcp2.h>

#include "siphash.h"
#include "template.h"
#include "network.h"

using namespace nghttp2;

namespace std {
template <> struct hash<ngtcp2_cid> {
  hash() {
    auto s = as_writable_uint8_span(std::span{key});
    assert(RAND_bytes(s.data(), s.size()) == 1);
  }

  std::size_t operator()(const ngtcp2_cid &cid) const noexcept {
    return static_cast<size_t>(siphash24(key, {cid.data, cid.datalen}));
  }

  std::array<uint64_t, 2> key;
};
} // namespace std

bool operator==(const ngtcp2_cid &lhs, const ngtcp2_cid &rhs);

namespace shrpx {

struct UpstreamAddr;
struct QUICKeyingMaterials;
struct QUICKeyingMaterial;

constexpr size_t SHRPX_QUIC_CID_WORKER_ID_OFFSET = 1;
constexpr size_t SHRPX_QUIC_SERVER_IDLEN = 4;
constexpr size_t SHRPX_QUIC_SOCK_IDLEN = 4;
constexpr size_t SHRPX_QUIC_WORKER_IDLEN =
  SHRPX_QUIC_SERVER_IDLEN + SHRPX_QUIC_SOCK_IDLEN;
constexpr size_t SHRPX_QUIC_CLIENT_IDLEN = 8;
constexpr size_t SHRPX_QUIC_DECRYPTED_DCIDLEN =
  SHRPX_QUIC_WORKER_IDLEN + SHRPX_QUIC_CLIENT_IDLEN;
constexpr size_t SHRPX_QUIC_SCIDLEN =
  SHRPX_QUIC_CID_WORKER_ID_OFFSET + SHRPX_QUIC_DECRYPTED_DCIDLEN;
constexpr size_t SHRPX_QUIC_CID_ENCRYPTION_KEYLEN = 16;
constexpr size_t SHRPX_QUIC_CONN_CLOSE_PKTLEN = 256;
constexpr size_t SHRPX_QUIC_STATELESS_RESET_BURST = 100;
constexpr size_t SHRPX_QUIC_SECRET_RESERVEDLEN = 4;
constexpr size_t SHRPX_QUIC_SECRETLEN = 32;
constexpr size_t SHRPX_QUIC_SALTLEN = 32;
constexpr uint8_t SHRPX_QUIC_DCID_KM_ID_MASK = 0xe0;

struct WorkerID {
  union {
    struct {
      uint32_t server;
      uint16_t worker_process;
      uint16_t thread;
    };
    uint64_t worker;
  };
};

static_assert(sizeof(WorkerID) == SHRPX_QUIC_WORKER_IDLEN,
              "WorkerID length assertion failure");

inline bool operator==(const WorkerID &lhd, const WorkerID &rhd) {
  return lhd.worker == rhd.worker;
}

inline bool operator!=(const WorkerID &lhd, const WorkerID &rhd) {
  return lhd.worker != rhd.worker;
}

struct ConnectionID {
  WorkerID worker;
  uint64_t client;
};

ngtcp2_tstamp quic_timestamp();

int quic_send_packet(const UpstreamAddr *faddr, const sockaddr *remote_sa,
                     size_t remote_salen, const sockaddr *local_sa,
                     size_t local_salen, const ngtcp2_pkt_info &pi,
                     std::span<const uint8_t> data, size_t gso_size);

int generate_quic_retry_connection_id(ngtcp2_cid &cid, uint32_t server_id,
                                      uint8_t km_id, EVP_CIPHER_CTX *ctx);

int generate_quic_connection_id(ngtcp2_cid &cid, const WorkerID &wid,
                                uint8_t km_id, EVP_CIPHER_CTX *ctx);

int encrypt_quic_connection_id(uint8_t *dest, const uint8_t *src,
                               EVP_CIPHER_CTX *ctx);

int decrypt_quic_connection_id(ConnectionID &dest, const uint8_t *src,
                               EVP_CIPHER_CTX *ctx);

int generate_quic_hashed_connection_id(ngtcp2_cid &dest,
                                       const Address &remote_addr,
                                       const Address &local_addr,
                                       const ngtcp2_cid &cid);

int generate_quic_stateless_reset_token(uint8_t *token, const ngtcp2_cid &cid,
                                        const uint8_t *secret,
                                        size_t secretlen);

std::optional<std::span<const uint8_t>>
generate_retry_token(std::span<uint8_t> token, uint32_t version,
                     const sockaddr *sa, socklen_t salen,
                     const ngtcp2_cid &retry_scid, const ngtcp2_cid &odcid,
                     std::span<const uint8_t> secret);

int verify_retry_token(ngtcp2_cid &odcid, std::span<const uint8_t> token,
                       uint32_t version, const ngtcp2_cid &dcid,
                       const sockaddr *sa, socklen_t salen,
                       std::span<const uint8_t> secret);

std::optional<std::span<const uint8_t>>
generate_token(std::span<uint8_t> token, const sockaddr *sa, size_t salen,
               std::span<const uint8_t> secret, uint8_t km_id);

int verify_token(std::span<const uint8_t> token, const sockaddr *sa,
                 socklen_t salen, std::span<const uint8_t> secret);

int generate_quic_connection_id_encryption_key(std::span<uint8_t> key,
                                               std::span<const uint8_t> secret,
                                               std::span<const uint8_t> salt);

const QUICKeyingMaterial *
select_quic_keying_material(const QUICKeyingMaterials &qkms, uint8_t km_id);

} // namespace shrpx

#endif // SHRPX_QUIC_H
