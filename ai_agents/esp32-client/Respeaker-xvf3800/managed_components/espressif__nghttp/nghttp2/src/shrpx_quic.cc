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
#include "shrpx_quic.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/udp.h>

#include <array>
#include <chrono>

#include <ngtcp2/ngtcp2_crypto.h>

#include <nghttp3/nghttp3.h>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/rand.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/rand.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include "shrpx_config.h"
#include "shrpx_log.h"
#include "util.h"
#include "xsi_strerror.h"

bool operator==(const ngtcp2_cid &lhs, const ngtcp2_cid &rhs) {
  return ngtcp2_cid_eq(&lhs, &rhs);
}

namespace shrpx {

ngtcp2_tstamp quic_timestamp() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
           std::chrono::steady_clock::now().time_since_epoch())
    .count();
}

int quic_send_packet(const UpstreamAddr *faddr, const sockaddr *remote_sa,
                     size_t remote_salen, const sockaddr *local_sa,
                     size_t local_salen, const ngtcp2_pkt_info &pi,
                     std::span<const uint8_t> data, size_t gso_size) {
  assert(gso_size);

  iovec msg_iov = {const_cast<uint8_t *>(data.data()), data.size()};
  msghdr msg{};
  msg.msg_name = const_cast<sockaddr *>(remote_sa);
  msg.msg_namelen = remote_salen;
  msg.msg_iov = &msg_iov;
  msg.msg_iovlen = 1;

  uint8_t msg_ctrl[CMSG_SPACE(sizeof(int)) +
#ifdef UDP_SEGMENT
                   CMSG_SPACE(sizeof(uint16_t)) +
#endif // UDP_SEGMENT
                   CMSG_SPACE(sizeof(in6_pktinfo))];

  memset(msg_ctrl, 0, sizeof(msg_ctrl));

  msg.msg_control = msg_ctrl;
  msg.msg_controllen = sizeof(msg_ctrl);

  size_t controllen = 0;

  auto cm = CMSG_FIRSTHDR(&msg);

  switch (local_sa->sa_family) {
  case AF_INET: {
    controllen += CMSG_SPACE(sizeof(in_pktinfo));
    cm->cmsg_level = IPPROTO_IP;
    cm->cmsg_type = IP_PKTINFO;
    cm->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));
    in_pktinfo pktinfo{};
    auto addrin =
      reinterpret_cast<sockaddr_in *>(const_cast<sockaddr *>(local_sa));
    pktinfo.ipi_spec_dst = addrin->sin_addr;
    memcpy(CMSG_DATA(cm), &pktinfo, sizeof(pktinfo));

    break;
  }
  case AF_INET6: {
    controllen += CMSG_SPACE(sizeof(in6_pktinfo));
    cm->cmsg_level = IPPROTO_IPV6;
    cm->cmsg_type = IPV6_PKTINFO;
    cm->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));
    in6_pktinfo pktinfo{};
    auto addrin =
      reinterpret_cast<sockaddr_in6 *>(const_cast<sockaddr *>(local_sa));
    pktinfo.ipi6_addr = addrin->sin6_addr;
    memcpy(CMSG_DATA(cm), &pktinfo, sizeof(pktinfo));

    break;
  }
  default:
    assert(0);
  }

#ifdef UDP_SEGMENT
  if (data.size() > gso_size) {
    controllen += CMSG_SPACE(sizeof(uint16_t));
    cm = CMSG_NXTHDR(&msg, cm);
    cm->cmsg_level = SOL_UDP;
    cm->cmsg_type = UDP_SEGMENT;
    cm->cmsg_len = CMSG_LEN(sizeof(uint16_t));
    uint16_t n = gso_size;
    memcpy(CMSG_DATA(cm), &n, sizeof(n));
  }
#endif // UDP_SEGMENT

  controllen += CMSG_SPACE(sizeof(int));
  cm = CMSG_NXTHDR(&msg, cm);
  cm->cmsg_len = CMSG_LEN(sizeof(int));
  unsigned int tos = pi.ecn;
  memcpy(CMSG_DATA(cm), &tos, sizeof(tos));

  switch (local_sa->sa_family) {
  case AF_INET:
    cm->cmsg_level = IPPROTO_IP;
    cm->cmsg_type = IP_TOS;

    break;
  case AF_INET6:
    cm->cmsg_level = IPPROTO_IPV6;
    cm->cmsg_type = IPV6_TCLASS;

    break;
  default:
    assert(0);
  }

  msg.msg_controllen = controllen;

  ssize_t nwrite;

  do {
    nwrite = sendmsg(faddr->fd, &msg, 0);
  } while (nwrite == -1 && errno == EINTR);

  if (nwrite == -1) {
    if (LOG_ENABLED(INFO)) {
      auto error = errno;
      LOG(INFO) << "sendmsg failed: errno=" << error;
    }

    return -errno;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "QUIC sent packet: local="
              << util::to_numeric_addr(local_sa, local_salen)
              << " remote=" << util::to_numeric_addr(remote_sa, remote_salen)
              << " ecn=" << log::hex << pi.ecn << log::dec << " " << nwrite
              << " bytes";
  }

  assert(static_cast<size_t>(nwrite) == data.size());

  return 0;
}

int generate_quic_retry_connection_id(ngtcp2_cid &cid, uint32_t server_id,
                                      uint8_t km_id, EVP_CIPHER_CTX *ctx) {
  if (RAND_bytes(cid.data, SHRPX_QUIC_SCIDLEN) != 1) {
    return -1;
  }

  cid.datalen = SHRPX_QUIC_SCIDLEN;
  cid.data[0] = (cid.data[0] & (~SHRPX_QUIC_DCID_KM_ID_MASK)) | km_id;

  auto p = cid.data + SHRPX_QUIC_CID_WORKER_ID_OFFSET;

  std::copy_n(reinterpret_cast<uint8_t *>(&server_id), sizeof(server_id), p);

  return encrypt_quic_connection_id(p, p, ctx);
}

int generate_quic_connection_id(ngtcp2_cid &cid, const WorkerID &wid,
                                uint8_t km_id, EVP_CIPHER_CTX *ctx) {
  if (RAND_bytes(cid.data, SHRPX_QUIC_SCIDLEN) != 1) {
    return -1;
  }

  cid.datalen = SHRPX_QUIC_SCIDLEN;
  cid.data[0] = (cid.data[0] & (~SHRPX_QUIC_DCID_KM_ID_MASK)) | km_id;

  auto p = cid.data + SHRPX_QUIC_CID_WORKER_ID_OFFSET;

  std::copy_n(reinterpret_cast<const uint8_t *>(&wid), sizeof(wid), p);

  return encrypt_quic_connection_id(p, p, ctx);
}

int encrypt_quic_connection_id(uint8_t *dest, const uint8_t *src,
                               EVP_CIPHER_CTX *ctx) {
  int len;

  if (!EVP_EncryptUpdate(ctx, dest, &len, src, SHRPX_QUIC_DECRYPTED_DCIDLEN) ||
      !EVP_EncryptFinal_ex(ctx, dest + len, &len)) {
    return -1;
  }

  return 0;
}

int decrypt_quic_connection_id(ConnectionID &dest, const uint8_t *src,
                               EVP_CIPHER_CTX *ctx) {
  int len;
  auto p = reinterpret_cast<uint8_t *>(&dest);

  if (!EVP_DecryptUpdate(ctx, p, &len, src, SHRPX_QUIC_DECRYPTED_DCIDLEN) ||
      !EVP_DecryptFinal_ex(ctx, p + len, &len)) {
    return -1;
  }

  return 0;
}

int generate_quic_hashed_connection_id(ngtcp2_cid &dest,
                                       const Address &remote_addr,
                                       const Address &local_addr,
                                       const ngtcp2_cid &cid) {
  auto ctx = EVP_MD_CTX_new();
  auto d = defer(EVP_MD_CTX_free, ctx);

  std::array<uint8_t, 32> h;
  unsigned int hlen = EVP_MD_size(EVP_sha256());

  if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) ||
      !EVP_DigestUpdate(ctx, &remote_addr.su.sa, remote_addr.len) ||
      !EVP_DigestUpdate(ctx, &local_addr.su.sa, local_addr.len) ||
      !EVP_DigestUpdate(ctx, cid.data, cid.datalen) ||
      !EVP_DigestFinal_ex(ctx, h.data(), &hlen)) {
    return -1;
  }

  assert(hlen == h.size());

  std::copy_n(std::begin(h), sizeof(dest.data), std::begin(dest.data));
  dest.datalen = sizeof(dest.data);

  return 0;
}

int generate_quic_stateless_reset_token(uint8_t *token, const ngtcp2_cid &cid,
                                        const uint8_t *secret,
                                        size_t secretlen) {
  if (ngtcp2_crypto_generate_stateless_reset_token(token, secret, secretlen,
                                                   &cid) != 0) {
    return -1;
  }

  return 0;
}

std::optional<std::span<const uint8_t>>
generate_retry_token(std::span<uint8_t> token, uint32_t version,
                     const sockaddr *sa, socklen_t salen,
                     const ngtcp2_cid &retry_scid, const ngtcp2_cid &odcid,
                     std::span<const uint8_t> secret) {
  auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count();

  auto tokenlen = ngtcp2_crypto_generate_retry_token(
    token.data(), secret.data(), secret.size(), version, sa, salen, &retry_scid,
    &odcid, t);
  if (tokenlen < 0) {
    return {};
  }

  return {{std::begin(token), static_cast<size_t>(tokenlen)}};
}

int verify_retry_token(ngtcp2_cid &odcid, std::span<const uint8_t> token,
                       uint32_t version, const ngtcp2_cid &dcid,
                       const sockaddr *sa, socklen_t salen,
                       std::span<const uint8_t> secret) {
  auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count();

  if (ngtcp2_crypto_verify_retry_token(
        &odcid, token.data(), token.size(), secret.data(), secret.size(),
        version, sa, salen, &dcid, 10 * NGTCP2_SECONDS, t) != 0) {
    return -1;
  }

  return 0;
}

std::optional<std::span<const uint8_t>>
generate_token(std::span<uint8_t> token, const sockaddr *sa, size_t salen,
               std::span<const uint8_t> secret, uint8_t km_id) {
  auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count();

  auto tokenlen = ngtcp2_crypto_generate_regular_token(
    token.data(), secret.data(), secret.size(), sa, salen, t);
  if (tokenlen < 0) {
    return {};
  }

  token[tokenlen++] = km_id;

  return {{std::begin(token), static_cast<size_t>(tokenlen)}};
}

int verify_token(std::span<const uint8_t> token, const sockaddr *sa,
                 socklen_t salen, std::span<const uint8_t> secret) {
  if (token.empty()) {
    return -1;
  }

  auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count();

  if (ngtcp2_crypto_verify_regular_token(
        token.data(), token.size() - 1, secret.data(), secret.size(), sa, salen,
        3600 * NGTCP2_SECONDS, t) != 0) {
    return -1;
  }

  return 0;
}

int generate_quic_connection_id_encryption_key(std::span<uint8_t> key,
                                               std::span<const uint8_t> secret,
                                               std::span<const uint8_t> salt) {
  constexpr uint8_t info[] = "connection id encryption key";
  ngtcp2_crypto_md sha256;
  ngtcp2_crypto_md_init(
    &sha256, reinterpret_cast<void *>(const_cast<EVP_MD *>(EVP_sha256())));

  if (ngtcp2_crypto_hkdf(key.data(), key.size(), &sha256, secret.data(),
                         secret.size(), salt.data(), salt.size(), info,
                         str_size(info)) != 0) {
    return -1;
  }

  return 0;
}

const QUICKeyingMaterial *
select_quic_keying_material(const QUICKeyingMaterials &qkms, uint8_t km_id) {
  for (auto &qkm : qkms.keying_materials) {
    if (km_id == qkm.id) {
      return &qkm;
    }
  }

  return &qkms.keying_materials.front();
}

} // namespace shrpx
