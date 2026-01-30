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
#include "shrpx_tls.h"

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/types.h>

#include <vector>
#include <string>
#include <iomanip>

#include <iostream>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/crypto.h>
#  include <wolfssl/openssl/x509.h>
#  include <wolfssl/openssl/x509v3.h>
#  include <wolfssl/openssl/rand.h>
#  include <wolfssl/openssl/dh.h>
#  include <wolfssl/openssl/ocsp.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/crypto.h>
#  include <openssl/x509.h>
#  include <openssl/x509v3.h>
#  include <openssl/rand.h>
#  include <openssl/dh.h>
#  ifndef OPENSSL_NO_OCSP
#    include <openssl/ocsp.h>
#  endif // OPENSSL_NO_OCSP
#  if OPENSSL_3_0_0_API
#    include <openssl/params.h>
#    include <openssl/core_names.h>
#    include <openssl/decoder.h>
#  endif // OPENSSL_3_0_0_API
#endif   // !NGHTTP2_OPENSSL_IS_WOLFSSL
#ifdef NGHTTP2_OPENSSL_IS_BORINGSSL
#  include <openssl/hmac.h>
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL

#include <nghttp2/nghttp2.h>

#ifdef ENABLE_HTTP3
#  include <ngtcp2/ngtcp2.h>
#  include <ngtcp2/ngtcp2_crypto.h>
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

#ifdef HAVE_LIBBROTLI
#  include <brotli/encode.h>
#  include <brotli/decode.h>
#endif // HAVE_LIBBROTLI

#include "shrpx_log.h"
#include "shrpx_client_handler.h"
#include "shrpx_config.h"
#include "shrpx_worker.h"
#include "shrpx_downstream_connection_pool.h"
#include "shrpx_http2_session.h"
#include "shrpx_memcached_request.h"
#include "shrpx_memcached_dispatcher.h"
#include "shrpx_connection_handler.h"
#ifdef ENABLE_HTTP3
#  include "shrpx_http3_upstream.h"
#endif // ENABLE_HTTP3
#include "util.h"
#include "tls.h"
#include "template.h"
#include "timegm.h"

using namespace nghttp2;
using namespace std::chrono_literals;

namespace shrpx {

namespace tls {

namespace {
int verify_callback(int preverify_ok, X509_STORE_CTX *ctx) {
  if (!preverify_ok) {
    int err = X509_STORE_CTX_get_error(ctx);
    int depth = X509_STORE_CTX_get_error_depth(ctx);
    if (err == X509_V_ERR_CERT_HAS_EXPIRED && depth == 0 &&
        get_config()->tls.client_verify.tolerate_expired) {
      LOG(INFO) << "The client certificate has expired, but is accepted by "
                   "configuration";
      return 1;
    }
    LOG(ERROR) << "client certificate verify error:num=" << err << ":"
               << X509_verify_cert_error_string(err) << ":depth=" << depth;
  }
  return preverify_ok;
}
} // namespace

int set_alpn_prefs(std::vector<unsigned char> &out,
                   const std::vector<StringRef> &protos) {
  size_t len = 0;

  for (const auto &proto : protos) {
    if (proto.size() > 255) {
      LOG(FATAL) << "Too long ALPN identifier: " << proto.size();
      return -1;
    }

    len += 1 + proto.size();
  }

  if (len > (1 << 16) - 1) {
    LOG(FATAL) << "Too long ALPN identifier list: " << len;
    return -1;
  }

  out.resize(len);
  auto ptr = out.data();

  for (const auto &proto : protos) {
    *ptr++ = proto.size();
    ptr = std::copy(std::begin(proto), std::end(proto), ptr);
  }

  return 0;
}

namespace {
int ssl_pem_passwd_cb(char *buf, int size, int rwflag, void *user_data) {
  auto config = static_cast<Config *>(user_data);
  auto len = static_cast<int>(config->tls.private_key_passwd.size());
  if (size < len + 1) {
    LOG(ERROR) << "ssl_pem_passwd_cb: buf is too small " << size;
    return 0;
  }
  // Copy string including last '\0'.
  memcpy(buf, config->tls.private_key_passwd.data(), len + 1);
  return len;
}
} // namespace

namespace {
std::shared_ptr<std::vector<uint8_t>>
get_ocsp_data(TLSContextData *tls_ctx_data) {
#ifdef HAVE_ATOMIC_STD_SHARED_PTR
  return tls_ctx_data->ocsp_data.load(std::memory_order_acquire);
#else  // !HAVE_ATOMIC_STD_SHARED_PTR
  std::lock_guard<std::mutex> g(tls_ctx_data->mu);
  return tls_ctx_data->ocsp_data;
#endif // !HAVE_ATOMIC_STD_SHARED_PTR
}
} // namespace

namespace {
void set_ocsp_response(SSL *ssl) {
#ifdef NGHTTP2_OPENSSL_IS_BORINGSSL
  auto tls_ctx_data =
    static_cast<TLSContextData *>(SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));
  auto data = get_ocsp_data(tls_ctx_data);

  if (!data) {
    return;
  }

  SSL_set_ocsp_response(ssl, data->data(), data->size());
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL
}
} // namespace

namespace {
// *al is set to SSL_AD_UNRECOGNIZED_NAME by openssl, so we don't have
// to set it explicitly.
int servername_callback(SSL *ssl, int *al, void *arg) {
  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  auto handler = static_cast<ClientHandler *>(conn->data);
  auto worker = handler->get_worker();

  auto rawhost = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  if (rawhost == nullptr) {
    set_ocsp_response(ssl);

    return SSL_TLSEXT_ERR_NOACK;
  }

  auto len = strlen(rawhost);
  // NI_MAXHOST includes terminal NULL.
  if (len == 0 || len + 1 > NI_MAXHOST) {
    set_ocsp_response(ssl);

    return SSL_TLSEXT_ERR_NOACK;
  }

  std::array<char, NI_MAXHOST> buf;

  auto end_buf = std::copy_n(rawhost, len, std::begin(buf));

  util::inp_strlower(std::begin(buf), end_buf);

  auto hostname = StringRef{std::begin(buf), end_buf};

#ifdef ENABLE_HTTP3
  auto cert_tree = conn->proto == Proto::HTTP3
                     ? worker->get_quic_cert_lookup_tree()
                     : worker->get_cert_lookup_tree();
#else  // !ENABLE_HTTP3
  auto cert_tree = worker->get_cert_lookup_tree();
#endif // !ENABLE_HTTP3

  auto idx = cert_tree->lookup(hostname);
  if (idx == -1) {
    set_ocsp_response(ssl);

    return SSL_TLSEXT_ERR_NOACK;
  }

  handler->set_tls_sni(hostname);

  auto conn_handler = worker->get_connection_handler();

#ifdef ENABLE_HTTP3
  const auto &ssl_ctx_list = conn->proto == Proto::HTTP3
                               ? conn_handler->get_quic_indexed_ssl_ctx(idx)
                               : conn_handler->get_indexed_ssl_ctx(idx);
#else  // !ENABLE_HTTP3
  const auto &ssl_ctx_list = conn_handler->get_indexed_ssl_ctx(idx);
#endif // !ENABLE_HTTP3

  assert(!ssl_ctx_list.empty());

#ifdef NGHTTP2_GENUINE_OPENSSL
  auto num_sigalgs =
    SSL_get_sigalgs(ssl, 0, nullptr, nullptr, nullptr, nullptr, nullptr);

  for (idx = 0; idx < num_sigalgs; ++idx) {
    int signhash;

    SSL_get_sigalgs(ssl, idx, nullptr, nullptr, &signhash, nullptr, nullptr);
    switch (signhash) {
    case NID_ecdsa_with_SHA256:
    case NID_ecdsa_with_SHA384:
    case NID_ecdsa_with_SHA512:
      break;
    default:
      continue;
    }

    break;
  }

  if (idx == num_sigalgs) {
    SSL_set_SSL_CTX(ssl, ssl_ctx_list[0]);

    return SSL_TLSEXT_ERR_OK;
  }

  auto num_shared_curves = SSL_get_shared_curve(ssl, -1);

  for (auto i = 0; i < num_shared_curves; ++i) {
    auto shared_curve = SSL_get_shared_curve(ssl, i);
#  if OPENSSL_3_0_0_API
    // It looks like only short name is defined in OpenSSL.  No idea
    // which one to use because it is unknown that which one
    // EVP_PKEY_get_utf8_string_param("group") returns.
    auto shared_curve_name = OBJ_nid2sn(shared_curve);
    if (shared_curve_name == nullptr) {
      continue;
    }
#  endif // OPENSSL_3_0_0_API

    for (auto ssl_ctx : ssl_ctx_list) {
      auto cert = SSL_CTX_get0_certificate(ssl_ctx);
      auto pubkey = X509_get0_pubkey(cert);

      if (EVP_PKEY_base_id(pubkey) != EVP_PKEY_EC) {
        continue;
      }

#  if OPENSSL_3_0_0_API
      std::array<char, 64> curve_name;
      if (!EVP_PKEY_get_utf8_string_param(pubkey, "group", curve_name.data(),
                                          curve_name.size(), nullptr)) {
        continue;
      }

      if (strcmp(shared_curve_name, curve_name.data()) == 0) {
        SSL_set_SSL_CTX(ssl, ssl_ctx);
        return SSL_TLSEXT_ERR_OK;
      }
#  else  // !OPENSSL_3_0_0_API
      auto eckey = EVP_PKEY_get0_EC_KEY(pubkey);
      if (eckey == nullptr) {
        continue;
      }

      auto ecgroup = EC_KEY_get0_group(eckey);
      auto cert_curve = EC_GROUP_get_curve_name(ecgroup);

      if (shared_curve == cert_curve) {
        SSL_set_SSL_CTX(ssl, ssl_ctx);
        return SSL_TLSEXT_ERR_OK;
      }
#  endif // !OPENSSL_3_0_0_API
    }
  }
#endif // NGHTTP2_GENUINE_OPENSSL

  SSL_set_SSL_CTX(ssl, ssl_ctx_list[0]);

  set_ocsp_response(ssl);

  return SSL_TLSEXT_ERR_OK;
}
} // namespace

#ifndef NGHTTP2_OPENSSL_IS_BORINGSSL
namespace {
int ocsp_resp_cb(SSL *ssl, void *arg) {
  auto ssl_ctx = SSL_get_SSL_CTX(ssl);
  auto tls_ctx_data =
    static_cast<TLSContextData *>(SSL_CTX_get_app_data(ssl_ctx));

  auto data = get_ocsp_data(tls_ctx_data);

  if (!data) {
    return SSL_TLSEXT_ERR_OK;
  }

  auto buf = static_cast<uint8_t *>(
    CRYPTO_malloc(data->size(), NGHTTP2_FILE_NAME, __LINE__));

  if (!buf) {
    return SSL_TLSEXT_ERR_OK;
  }

  std::copy(std::begin(*data), std::end(*data), buf);

  SSL_set_tlsext_status_ocsp_resp(ssl, buf, data->size());

  return SSL_TLSEXT_ERR_OK;
}
} // namespace
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL

constexpr auto MEMCACHED_SESSION_CACHE_KEY_PREFIX =
  "nghttpx:tls-session-cache:"_sr;

namespace {
int tls_session_client_new_cb(SSL *ssl, SSL_SESSION *session) {
  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  if (conn->tls.client_session_cache == nullptr) {
    return 0;
  }

  try_cache_tls_session(conn->tls.client_session_cache, session,
                        std::chrono::steady_clock::now());

  return 0;
}
} // namespace

namespace {
int tls_session_new_cb(SSL *ssl, SSL_SESSION *session) {
  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  auto handler = static_cast<ClientHandler *>(conn->data);
  auto worker = handler->get_worker();
  auto dispatcher = worker->get_session_cache_memcached_dispatcher();
  auto &balloc = handler->get_block_allocator();

#ifdef TLS1_3_VERSION
  if (SSL_version(ssl) == TLS1_3_VERSION) {
    return 0;
  }
#endif // TLS1_3_VERSION

  const unsigned char *id;
  unsigned int idlen;

  id = SSL_SESSION_get_id(session, &idlen);

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Memcached: cache session, id="
              << util::format_hex(std::span{id, idlen});
  }

  auto req = std::make_unique<MemcachedRequest>();
  req->op = MemcachedOp::ADD;
  req->key = MEMCACHED_SESSION_CACHE_KEY_PREFIX;
  req->key +=
    util::format_hex(balloc, std::span{id, static_cast<size_t>(idlen)});

  auto sessionlen = i2d_SSL_SESSION(session, nullptr);
  req->value.resize(sessionlen);
  auto buf = &req->value[0];
  i2d_SSL_SESSION(session, &buf);
  req->expiry = 12_h;
  req->cb = [](MemcachedRequest *req, MemcachedResult res) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Memcached: session cache done.  key=" << req->key
                << ", status_code=" << static_cast<uint16_t>(res.status_code)
                << ", value="
                << std::string(std::begin(res.value), std::end(res.value));
    }
    if (res.status_code != MemcachedStatusCode::NO_ERROR) {
      LOG(WARN) << "Memcached: failed to cache session key=" << req->key
                << ", status_code=" << static_cast<uint16_t>(res.status_code)
                << ", value="
                << std::string(std::begin(res.value), std::end(res.value));
    }
  };
  assert(!req->canceled);

  dispatcher->add_request(std::move(req));

  return 0;
}
} // namespace

namespace {
SSL_SESSION *tls_session_get_cb(SSL *ssl, const unsigned char *id, int idlen,
                                int *copy) {
  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  auto handler = static_cast<ClientHandler *>(conn->data);
  auto worker = handler->get_worker();
  auto dispatcher = worker->get_session_cache_memcached_dispatcher();
  auto &balloc = handler->get_block_allocator();

  if (idlen == 0) {
    return nullptr;
  }

  if (conn->tls.cached_session) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Memcached: found cached session, id="
                << util::format_hex(std::span{id, static_cast<size_t>(idlen)});
    }

    // This is required, without this, memory leak occurs.
    *copy = 0;

    auto session = conn->tls.cached_session;
    conn->tls.cached_session = nullptr;
    return session;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Memcached: get cached session, id="
              << util::format_hex(std::span{id, static_cast<size_t>(idlen)});
  }

  auto req = std::make_unique<MemcachedRequest>();
  req->op = MemcachedOp::GET;
  req->key = MEMCACHED_SESSION_CACHE_KEY_PREFIX;
  req->key +=
    util::format_hex(balloc, std::span{id, static_cast<size_t>(idlen)});
  req->cb = [conn](MemcachedRequest *, MemcachedResult res) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Memcached: returned status code "
                << static_cast<uint16_t>(res.status_code);
    }

    // We might stop reading, so start it again
    conn->rlimit.startw();
    ev_timer_again(conn->loop, &conn->rt);

    conn->wlimit.startw();
    ev_timer_again(conn->loop, &conn->wt);

    conn->tls.cached_session_lookup_req = nullptr;
    if (res.status_code != MemcachedStatusCode::NO_ERROR) {
      conn->tls.handshake_state = TLSHandshakeState::CANCEL_SESSION_CACHE;
      return;
    }

    const uint8_t *p = res.value.data();

    auto session = d2i_SSL_SESSION(nullptr, &p, res.value.size());
    if (!session) {
      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "cannot materialize session";
      }
      conn->tls.handshake_state = TLSHandshakeState::CANCEL_SESSION_CACHE;
      return;
    }

    conn->tls.cached_session = session;
    conn->tls.handshake_state = TLSHandshakeState::GOT_SESSION_CACHE;
  };

  conn->tls.handshake_state = TLSHandshakeState::WAIT_FOR_SESSION_CACHE;
  conn->tls.cached_session_lookup_req = req.get();

  dispatcher->add_request(std::move(req));

  return nullptr;
}
} // namespace

namespace {
int ticket_key_cb(SSL *ssl, unsigned char *key_name, unsigned char *iv,
                  EVP_CIPHER_CTX *ctx,
#if OPENSSL_3_0_0_API
                  EVP_MAC_CTX *hctx,
#else  // !OPENSSL_3_0_0_API
                  HMAC_CTX *hctx,
#endif // !OPENSSL_3_0_0_API
                  int enc) {
  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  auto handler = static_cast<ClientHandler *>(conn->data);
  auto worker = handler->get_worker();
  auto ticket_keys = worker->get_ticket_keys();

  if (!ticket_keys) {
    // No ticket keys available.
    return -1;
  }

  auto &keys = ticket_keys->keys;
  assert(!keys.empty());

  if (enc) {
    if (RAND_bytes(iv, EVP_MAX_IV_LENGTH) == 0) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, handler) << "session ticket key: RAND_bytes failed";
      }
      return -1;
    }

    auto &key = keys[0];

    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, handler) << "encrypt session ticket key: "
                          << util::format_hex(key.data.name);
    }

    std::copy(std::begin(key.data.name), std::end(key.data.name), key_name);

    EVP_EncryptInit_ex(ctx, get_config()->tls.ticket.cipher, nullptr,
                       key.data.enc_key.data(), iv);
#if OPENSSL_3_0_0_API
    auto params = std::to_array({
      OSSL_PARAM_construct_octet_string(
        OSSL_MAC_PARAM_KEY, key.data.hmac_key.data(), key.hmac_keylen),
      OSSL_PARAM_construct_utf8_string(
        OSSL_MAC_PARAM_DIGEST, const_cast<char *>(EVP_MD_get0_name(key.hmac)),
        0),
      OSSL_PARAM_construct_end(),
    });
    if (!EVP_MAC_CTX_set_params(hctx, params.data())) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, handler) << "EVP_MAC_CTX_set_params failed";
      }
      return -1;
    }
#else  // !OPENSSL_3_0_0_API
    HMAC_Init_ex(hctx, key.data.hmac_key.data(), key.hmac_keylen, key.hmac,
                 nullptr);
#endif // !OPENSSL_3_0_0_API
    return 1;
  }

  size_t i;
  for (i = 0; i < keys.size(); ++i) {
    auto &key = keys[i];
    if (std::equal(std::begin(key.data.name), std::end(key.data.name),
                   key_name)) {
      break;
    }
  }

  if (i == keys.size()) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, handler) << "session ticket key "
                          << util::format_hex(std::span{key_name, 16})
                          << " not found";
    }
    return 0;
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, handler) << "decrypt session ticket key: "
                        << util::format_hex(std::span{key_name, 16});
  }

  auto &key = keys[i];
#if OPENSSL_3_0_0_API
  auto params = std::to_array({
    OSSL_PARAM_construct_octet_string(
      OSSL_MAC_PARAM_KEY, key.data.hmac_key.data(), key.hmac_keylen),
    OSSL_PARAM_construct_utf8_string(
      OSSL_MAC_PARAM_DIGEST, const_cast<char *>(EVP_MD_get0_name(key.hmac)), 0),
    OSSL_PARAM_construct_end(),
  });
  if (!EVP_MAC_CTX_set_params(hctx, params.data())) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, handler) << "EVP_MAC_CTX_set_params failed";
    }
    return -1;
  }
#else  // !OPENSSL_3_0_0_API
  HMAC_Init_ex(hctx, key.data.hmac_key.data(), key.hmac_keylen, key.hmac,
               nullptr);
#endif // !OPENSSL_3_0_0_API
  EVP_DecryptInit_ex(ctx, key.cipher, nullptr, key.data.enc_key.data(), iv);

#ifdef TLS1_3_VERSION
  // If ticket_key_cb is not set, OpenSSL always renew ticket for
  // TLSv1.3.
  if (SSL_version(ssl) == TLS1_3_VERSION) {
    return 2;
  }
#endif // TLS1_3_VERSION

  return i == 0 ? 1 : 2;
}
} // namespace

namespace {
void info_callback(const SSL *ssl, int where, int ret) {
#ifdef TLS1_3_VERSION
  // TLSv1.3 has no renegotiation.
  if (SSL_version(ssl) == TLS1_3_VERSION) {
    return;
  }
#endif // TLS1_3_VERSION

  // To mitigate possible DOS attack using lots of renegotiations, we
  // disable renegotiation. Since OpenSSL does not provide an easy way
  // to disable it, we check that renegotiation is started in this
  // callback.
  if (where & SSL_CB_HANDSHAKE_START) {
    auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
    if (conn && conn->tls.initial_handshake_done) {
      auto handler = static_cast<ClientHandler *>(conn->data);
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, handler) << "TLS renegotiation started";
      }
      handler->start_immediate_shutdown();
    }
  }
}
} // namespace

namespace {
int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                         unsigned char *outlen, const unsigned char *in,
                         unsigned int inlen, void *arg) {
  // We assume that get_config()->alpn_list contains ALPN protocol
  // identifier sorted by preference order.  So we just break when we
  // found the first overlap.
  for (const auto &target_proto_id : get_config()->tls.alpn_list) {
    for (auto p = in, end = in + inlen; p < end;) {
      auto proto_id = p + 1;
      auto proto_len = *p;

      if (proto_id + proto_len <= end &&
          target_proto_id == StringRef{proto_id, proto_len}) {
        *out = reinterpret_cast<const unsigned char *>(proto_id);
        *outlen = proto_len;

        return SSL_TLSEXT_ERR_OK;
      }

      p += 1 + proto_len;
    }
  }

  return SSL_TLSEXT_ERR_NOACK;
}
} // namespace

#ifdef ENABLE_HTTP3
namespace {
int quic_alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                              unsigned char *outlen, const unsigned char *in,
                              unsigned int inlen, void *arg) {
  constexpr StringRef alpnlist[] = {
    "h3"_sr,
    "h3-29"_sr,
  };

  for (auto &alpn : alpnlist) {
    for (auto p = in, end = in + inlen; p < end;) {
      auto proto_id = p + 1;
      auto proto_len = *p;

      if (alpn.size() == proto_len &&
          memcmp(alpn.byte(), proto_id, alpn.size()) == 0) {
        *out = proto_id;
        *outlen = proto_len;

        return SSL_TLSEXT_ERR_OK;
      }

      p += 1 + proto_len;
    }
  }

  return SSL_TLSEXT_ERR_ALERT_FATAL;
}
} // namespace
#endif // ENABLE_HTTP3

#ifdef NGHTTP2_GENUINE_OPENSSL
namespace {
int sct_add_cb(SSL *ssl, unsigned int ext_type, unsigned int context,
               const unsigned char **out, size_t *outlen, X509 *x,
               size_t chainidx, int *al, void *add_arg) {
  assert(ext_type == TLSEXT_TYPE_signed_certificate_timestamp);

  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  if (!conn->tls.sct_requested) {
    return 0;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "sct_add_cb is called, chainidx=" << chainidx << ", x=" << x
              << ", context=" << log::hex << context;
  }

  // We only have SCTs for leaf certificate.
  if (chainidx != 0) {
    return 0;
  }

  auto ssl_ctx = SSL_get_SSL_CTX(ssl);
  auto tls_ctx_data =
    static_cast<TLSContextData *>(SSL_CTX_get_app_data(ssl_ctx));

  *out = tls_ctx_data->sct_data.data();
  *outlen = tls_ctx_data->sct_data.size();

  return 1;
}
} // namespace

namespace {
void sct_free_cb(SSL *ssl, unsigned int ext_type, unsigned int context,
                 const unsigned char *out, void *add_arg) {
  assert(ext_type == TLSEXT_TYPE_signed_certificate_timestamp);
}
} // namespace

namespace {
int sct_parse_cb(SSL *ssl, unsigned int ext_type, unsigned int context,
                 const unsigned char *in, size_t inlen, X509 *x,
                 size_t chainidx, int *al, void *parse_arg) {
  assert(ext_type == TLSEXT_TYPE_signed_certificate_timestamp);
  // client SHOULD send 0 length extension_data, but it is still
  // SHOULD, and not MUST.

  // For TLSv1.3 Certificate message, sct_add_cb is called even if
  // client has not sent signed_certificate_timestamp extension in its
  // ClientHello.  Explicitly remember that client has included it
  // here.
  auto conn = static_cast<Connection *>(SSL_get_app_data(ssl));
  conn->tls.sct_requested = true;

  return 1;
}
} // namespace

#endif // NGHTTP2_GENUINE_OPENSSL

#ifndef OPENSSL_NO_PSK
namespace {
unsigned int psk_server_cb(SSL *ssl, const char *identity, unsigned char *psk,
                           unsigned int max_psk_len) {
  auto config = get_config();
  auto &tlsconf = config->tls;

  auto it = tlsconf.psk_secrets.find(StringRef{identity});
  if (it == std::end(tlsconf.psk_secrets)) {
    return 0;
  }

  auto &secret = (*it).second;
  if (secret.size() > max_psk_len) {
    LOG(ERROR) << "The size of PSK secret is " << secret.size()
               << ", but the acceptable maximum size is" << max_psk_len;
    return 0;
  }

  std::copy(std::begin(secret), std::end(secret), psk);

  return static_cast<unsigned int>(secret.size());
}
} // namespace
#endif // !OPENSSL_NO_PSK

#ifndef OPENSSL_NO_PSK
namespace {
unsigned int psk_client_cb(SSL *ssl, const char *hint, char *identity_out,
                           unsigned int max_identity_len, unsigned char *psk,
                           unsigned int max_psk_len) {
  auto config = get_config();
  auto &tlsconf = config->tls;

  auto &identity = tlsconf.client.psk.identity;
  auto &secret = tlsconf.client.psk.secret;

  if (identity.empty()) {
    return 0;
  }

  if (identity.size() + 1 > max_identity_len) {
    LOG(ERROR) << "The size of PSK identity is " << identity.size()
               << ", but the acceptable maximum size is " << max_identity_len;
    return 0;
  }

  if (secret.size() > max_psk_len) {
    LOG(ERROR) << "The size of PSK secret is " << secret.size()
               << ", but the acceptable maximum size is " << max_psk_len;
    return 0;
  }

  *std::copy(std::begin(identity), std::end(identity), identity_out) = '\0';
  std::copy(std::begin(secret), std::end(secret), psk);

  return static_cast<unsigned int>(secret.size());
}
} // namespace
#endif // !OPENSSL_NO_PSK

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
namespace {
int cert_compress(SSL *ssl, CBB *out, const uint8_t *in, size_t in_len) {
  uint8_t *dest;

  size_t compressed_size = BrotliEncoderMaxCompressedSize(in_len);
  if (compressed_size == 0) {
    LOG(ERROR) << "BrotliEncoderMaxCompressedSize failed";

    return 0;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Maximum compressed size is " << compressed_size
              << " bytes against input " << in_len << " bytes";
  }

  if (!CBB_reserve(out, &dest, compressed_size)) {
    LOG(ERROR) << "CBB_reserve failed";

    return 0;
  }

  if (BrotliEncoderCompress(BROTLI_MAX_QUALITY, BROTLI_DEFAULT_WINDOW,
                            BROTLI_MODE_GENERIC, in_len, in, &compressed_size,
                            dest) != BROTLI_TRUE) {
    LOG(ERROR) << "BrotliEncoderCompress failed";

    return 0;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "BrotliEncoderCompress succeeded, produced " << compressed_size
              << " bytes, " << (in_len - compressed_size) * 100 / in_len
              << "% reduction";
  }

  if (!CBB_did_write(out, compressed_size)) {
    LOG(ERROR) << "CBB_did_write failed";

    return 0;
  }

  return 1;
}

int cert_decompress(SSL *ssl, CRYPTO_BUFFER **out, size_t uncompressed_len,
                    const uint8_t *in, size_t in_len) {
  uint8_t *dest;
  auto buf = CRYPTO_BUFFER_alloc(&dest, uncompressed_len);
  auto len = uncompressed_len;

  if (BrotliDecoderDecompress(in_len, in, &len, dest) !=
      BROTLI_DECODER_RESULT_SUCCESS) {
    LOG(ERROR) << "BrotliDecoderDecompress failed";

    CRYPTO_BUFFER_free(buf);

    return 0;
  }

  if (uncompressed_len != len) {
    LOG(ERROR) << "Unexpected uncompressed length: expected "
               << uncompressed_len << " bytes, actual " << len << " bytes";

    CRYPTO_BUFFER_free(buf);

    return 0;
  }

  *out = buf;

  return 1;
}
} // namespace
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

struct TLSProtocol {
  StringRef name;
  long int mask;
};

constexpr TLSProtocol TLS_PROTOS[] = {
  TLSProtocol{"TLSv1.2"_sr, SSL_OP_NO_TLSv1_2},
  TLSProtocol{"TLSv1.1"_sr, SSL_OP_NO_TLSv1_1},
  TLSProtocol{"TLSv1.0"_sr, SSL_OP_NO_TLSv1}};

long int create_tls_proto_mask(const std::vector<StringRef> &tls_proto_list) {
  long int res = 0;

  for (auto &supported : TLS_PROTOS) {
    auto ok = false;
    for (auto &name : tls_proto_list) {
      if (util::strieq(supported.name, name)) {
        ok = true;
        break;
      }
    }
    if (!ok) {
      res |= supported.mask;
    }
  }
  return res;
}

SSL_CTX *create_ssl_context(const char *private_key_file, const char *cert_file,
                            const std::vector<uint8_t> &sct_data
#ifdef HAVE_NEVERBLEED
                            ,
                            neverbleed_t *nb
#endif // HAVE_NEVERBLEED
) {
  auto ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!ssl_ctx) {
    LOG(FATAL) << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }

  auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                  SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                  SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
                  SSL_OP_SINGLE_ECDH_USE | SSL_OP_SINGLE_DH_USE |
                  SSL_OP_CIPHER_SERVER_PREFERENCE
#ifdef NGHTTP2_GENUINE_OPENSSL
                  // The reason for disabling built-in anti-replay in
                  // OpenSSL is that it only works if client gets back
                  // to the same server.  The freshness check
                  // described in
                  // https://tools.ietf.org/html/rfc8446#section-8.3
                  // is still performed.
                  | SSL_OP_NO_ANTI_REPLAY
#endif // NGHTTP2_GENUINE_OPENSSL
    ;

  auto config = mod_config();
  auto &tlsconf = config->tls;

#ifdef SSL_OP_ENABLE_KTLS
  if (tlsconf.ktls) {
    ssl_opts |= SSL_OP_ENABLE_KTLS;
  }
#endif // SSL_OP_ENABLE_KTLS

  SSL_CTX_set_options(ssl_ctx, ssl_opts | tlsconf.tls_proto_mask);

  if (nghttp2::tls::ssl_ctx_set_proto_versions(
        ssl_ctx, tlsconf.min_proto_version, tlsconf.max_proto_version) != 0) {
    LOG(FATAL) << "Could not set TLS protocol version";
    DIE();
  }

  const unsigned char sid_ctx[] = "shrpx";
  SSL_CTX_set_session_id_context(ssl_ctx, sid_ctx, sizeof(sid_ctx) - 1);
  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_SERVER);

  if (!tlsconf.session_cache.memcached.host.empty()) {
    SSL_CTX_sess_set_new_cb(ssl_ctx, tls_session_new_cb);
    SSL_CTX_sess_set_get_cb(ssl_ctx, tls_session_get_cb);
  }

  SSL_CTX_set_timeout(ssl_ctx, tlsconf.session_timeout.count());

  if (SSL_CTX_set_cipher_list(ssl_ctx, tlsconf.ciphers.data()) == 0) {
    LOG(FATAL) << "SSL_CTX_set_cipher_list " << tlsconf.ciphers
               << " failed: " << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL) || defined(NGHTTP2_OPENSSL_IS_WOLFSSL)
  if (SSL_CTX_set_ciphersuites(ssl_ctx, tlsconf.tls13_ciphers.data()) == 0) {
    LOG(FATAL) << "SSL_CTX_set_ciphersuites " << tlsconf.tls13_ciphers
               << " failed: " << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#endif // NGHTTP2_GENUINE_OPENSSL || NGHTTP2_OPENSSL_IS_LIBRESSL ||
       // NGHTTP2_OPENSSL_IS_WOLFSSL

#ifndef OPENSSL_NO_EC
  if (SSL_CTX_set1_curves_list(ssl_ctx, tlsconf.ecdh_curves.data()) != 1) {
    LOG(FATAL) << "SSL_CTX_set1_curves_list " << tlsconf.ecdh_curves
               << " failed";
    DIE();
  }
#endif // OPENSSL_NO_EC

  if (!tlsconf.dh_param_file.empty()) {
    // Read DH parameters from file
    auto bio = BIO_new_file(tlsconf.dh_param_file.data(), "rb");
    if (bio == nullptr) {
      LOG(FATAL) << "BIO_new_file() failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
#if OPENSSL_3_0_0_API
    EVP_PKEY *dh = nullptr;
    auto dctx = OSSL_DECODER_CTX_new_for_pkey(
      &dh, "PEM", nullptr, "DH", OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS, nullptr,
      nullptr);

    if (!OSSL_DECODER_from_bio(dctx, bio)) {
      LOG(FATAL) << "OSSL_DECODER_from_bio() failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }

    if (SSL_CTX_set0_tmp_dh_pkey(ssl_ctx, dh) != 1) {
      LOG(FATAL) << "SSL_CTX_set0_tmp_dh_pkey failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
#else  // !OPENSSL_3_0_0_API
    auto dh = PEM_read_bio_DHparams(bio, nullptr, nullptr, nullptr);
    if (dh == nullptr) {
      LOG(FATAL) << "PEM_read_bio_DHparams() failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
    SSL_CTX_set_tmp_dh(ssl_ctx, dh);
    DH_free(dh);
#endif // !OPENSSL_3_0_0_API
    BIO_free(bio);
  }

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

  if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
    LOG(WARN) << "Could not load system trusted ca certificates: "
              << ERR_error_string(ERR_get_error(), nullptr);
  }

  if (!tlsconf.cacert.empty()) {
    if (SSL_CTX_load_verify_locations(ssl_ctx, tlsconf.cacert.data(),
                                      nullptr) != 1) {
      LOG(FATAL) << "Could not load trusted ca certificates from "
                 << tlsconf.cacert << ": "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
  }

  if (!tlsconf.private_key_passwd.empty()) {
    SSL_CTX_set_default_passwd_cb(ssl_ctx, ssl_pem_passwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, config);
  }

#ifndef HAVE_NEVERBLEED
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file,
                                  SSL_FILETYPE_PEM) != 1) {
    LOG(FATAL) << "SSL_CTX_use_PrivateKey_file failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#else  // HAVE_NEVERBLEED
  std::array<char, NEVERBLEED_ERRBUF_SIZE> errbuf;
  if (neverbleed_load_private_key_file(nb, ssl_ctx, private_key_file,
                                       errbuf.data()) != 1) {
    LOG(FATAL) << "neverbleed_load_private_key_file failed: " << errbuf.data();
    DIE();
  }
#endif // HAVE_NEVERBLEED

  if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1) {
    LOG(FATAL) << "SSL_CTX_use_certificate_file failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
  if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
    LOG(FATAL) << "SSL_CTX_check_private_key failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
  if (tlsconf.client_verify.enabled) {
    if (!tlsconf.client_verify.cacert.empty()) {
      if (SSL_CTX_load_verify_locations(
            ssl_ctx, tlsconf.client_verify.cacert.data(), nullptr) != 1) {
        LOG(FATAL) << "Could not load trusted ca certificates from "
                   << tlsconf.client_verify.cacert << ": "
                   << ERR_error_string(ERR_get_error(), nullptr);
        DIE();
      }
      // It is heard that SSL_CTX_load_verify_locations() may leave
      // error even though it returns success. See
      // http://forum.nginx.org/read.php?29,242540
      ERR_clear_error();
      auto list = SSL_load_client_CA_file(tlsconf.client_verify.cacert.data());
      if (!list) {
        LOG(FATAL) << "Could not load ca certificates from "
                   << tlsconf.client_verify.cacert << ": "
                   << ERR_error_string(ERR_get_error(), nullptr);
        DIE();
      }
      SSL_CTX_set_client_CA_list(ssl_ctx, list);
    }
    SSL_CTX_set_verify(ssl_ctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE |
                         SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       verify_callback);
  }
  SSL_CTX_set_tlsext_servername_callback(ssl_ctx, servername_callback);
#if OPENSSL_3_0_0_API
  SSL_CTX_set_tlsext_ticket_key_evp_cb(ssl_ctx, ticket_key_cb);
#else  // !OPENSSL_3_0_0_API
  SSL_CTX_set_tlsext_ticket_key_cb(ssl_ctx, ticket_key_cb);
#endif // !OPENSSL_3_0_0_API
#ifndef NGHTTP2_OPENSSL_IS_BORINGSSL
  SSL_CTX_set_tlsext_status_cb(ssl_ctx, ocsp_resp_cb);
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL
  SSL_CTX_set_info_callback(ssl_ctx, info_callback);

#ifdef NGHTTP2_OPENSSL_IS_BORINGSSL
  SSL_CTX_set_early_data_enabled(ssl_ctx, 1);
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL

  // ALPN selection callback
  SSL_CTX_set_alpn_select_cb(ssl_ctx, alpn_select_proto_cb, nullptr);

  auto tls_ctx_data = new TLSContextData();
  tls_ctx_data->cert_file = cert_file;
  tls_ctx_data->sct_data = sct_data;

  SSL_CTX_set_app_data(ssl_ctx, tls_ctx_data);

#ifdef NGHTTP2_GENUINE_OPENSSL
  // SSL_extension_supported(TLSEXT_TYPE_signed_certificate_timestamp)
  // returns 1, which means OpenSSL internally handles it.  But
  // OpenSSL handles signed_certificate_timestamp extension specially,
  // and it lets custom handler to process the extension.
  if (!sct_data.empty()) {
    // It is not entirely clear to me that SSL_EXT_CLIENT_HELLO is
    // required here.  sct_parse_cb is called without
    // SSL_EXT_CLIENT_HELLO being set.  But the passed context value
    // is SSL_EXT_CLIENT_HELLO.
    if (SSL_CTX_add_custom_ext(
          ssl_ctx, TLSEXT_TYPE_signed_certificate_timestamp,
          SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO |
            SSL_EXT_TLS1_3_CERTIFICATE | SSL_EXT_IGNORE_ON_RESUMPTION,
          sct_add_cb, sct_free_cb, nullptr, sct_parse_cb, nullptr) != 1) {
      LOG(FATAL) << "SSL_CTX_add_custom_ext failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
  }
#elif defined(NGHTTP2_OPENSSL_IS_BORINGSSL)
  if (!tls_ctx_data->sct_data.empty() &&
      SSL_CTX_set_signed_cert_timestamp_list(
        ssl_ctx, tls_ctx_data->sct_data.data(),
        tls_ctx_data->sct_data.size()) != 1) {
    LOG(FATAL) << "SSL_CTX_set_signed_cert_timestamp_list failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  (defined(NGHTTP2_OPENSSL_IS_WOLFSSL) && defined(WOLFSSL_EARLY_DATA))
  if (SSL_CTX_set_max_early_data(ssl_ctx, tlsconf.max_early_data) != 1) {
    LOG(FATAL) << "SSL_CTX_set_max_early_data failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#endif // NGHTTP2_GENUINE_OPENSSL || (NGHTTP2_OPENSSL_IS_WOLFSSL &&
       // WOLFSSL_EARLY_DATA)
#ifdef NGHTTP2_GENUINE_OPENSSL
  if (SSL_CTX_set_recv_max_early_data(ssl_ctx, tlsconf.max_early_data) != 1) {
    LOG(FATAL) << "SSL_CTX_set_recv_max_early_data failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#endif // NGHTTP2_GENUINE_OPENSSL

#ifndef OPENSSL_NO_PSK
  SSL_CTX_set_psk_server_callback(ssl_ctx, psk_server_cb);
#endif // !LIBRESSL_NO_PSK

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
  if (!SSL_CTX_add_cert_compression_alg(
        ssl_ctx, nghttp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
        cert_compress, cert_decompress)) {
    LOG(FATAL) << "SSL_CTX_add_cert_compression_alg failed";
    DIE();
  }
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

  return ssl_ctx;
}

#ifdef ENABLE_HTTP3
SSL_CTX *create_quic_ssl_context(const char *private_key_file,
                                 const char *cert_file,
                                 const std::vector<uint8_t> &sct_data
#  ifdef HAVE_NEVERBLEED
                                 ,
                                 neverbleed_t *nb
#  endif // HAVE_NEVERBLEED
) {
  auto ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!ssl_ctx) {
    LOG(FATAL) << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }

  constexpr auto ssl_opts =
    (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_SINGLE_ECDH_USE |
    SSL_OP_SINGLE_DH_USE |
    SSL_OP_CIPHER_SERVER_PREFERENCE
#  ifdef NGHTTP2_GENUINE_OPENSSL
    // The reason for disabling built-in anti-replay in OpenSSL is
    // that it only works if client gets back to the same server.
    // The freshness check described in
    // https://tools.ietf.org/html/rfc8446#section-8.3 is still
    // performed.
    | SSL_OP_NO_ANTI_REPLAY
#  endif // NGHTTP2_GENUINE_OPENSSL
    ;

  auto config = mod_config();
  auto &tlsconf = config->tls;

  SSL_CTX_set_options(ssl_ctx, ssl_opts);

#  ifdef HAVE_LIBNGTCP2_CRYPTO_QUICTLS
  if (ngtcp2_crypto_quictls_configure_server_context(ssl_ctx) != 0) {
    LOG(FATAL) << "ngtcp2_crypto_quictls_configure_server_context failed";
    DIE();
  }
#  endif // HAVE_LIBNGTCP2_CRYPTO_QUICTLS
#  ifdef HAVE_LIBNGTCP2_CRYPTO_BORINGSSL
  if (ngtcp2_crypto_boringssl_configure_server_context(ssl_ctx) != 0) {
    LOG(FATAL) << "ngtcp2_crypto_boringssl_configure_server_context failed";
    DIE();
  }
#  endif // HAVE_LIBNGTCP2_CRYPTO_BORINGSSL
#  ifdef HAVE_LIBNGTCP2_CRYPTO_WOLFSSL
  if (ngtcp2_crypto_wolfssl_configure_server_context(ssl_ctx) != 0) {
    LOG(FATAL) << "ngtcp2_crypto_wolfssl_configure_server_context failed";
    DIE();
  }
#  endif // HAVE_LIBNGTCP2_CRYPTO_WOLFSSL

  const unsigned char sid_ctx[] = "shrpx";
  SSL_CTX_set_session_id_context(ssl_ctx, sid_ctx, sizeof(sid_ctx) - 1);
  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);

  SSL_CTX_set_timeout(ssl_ctx, tlsconf.session_timeout.count());

  if (SSL_CTX_set_cipher_list(ssl_ctx, tlsconf.ciphers.data()) == 0) {
    LOG(FATAL) << "SSL_CTX_set_cipher_list " << tlsconf.ciphers
               << " failed: " << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }

#  if defined(NGHTTP2_GENUINE_OPENSSL) ||                                      \
    defined(NGHTTP2_OPENSSL_IS_LIBRESSL) ||                                    \
    defined(NGHTTP2_OPENSSL_IS_WOLFSSL)
  if (SSL_CTX_set_ciphersuites(ssl_ctx, tlsconf.tls13_ciphers.data()) == 0) {
    LOG(FATAL) << "SSL_CTX_set_ciphersuites " << tlsconf.tls13_ciphers
               << " failed: " << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#  endif // NGHTTP2_GENUINE_OPENSSL || NGHTTP2_OPENSSL_IS_LIBRESSL ||
         // NGHTTP2_OPENSSL_IS_WOLFSSL

#  ifndef OPENSSL_NO_EC
  if (SSL_CTX_set1_curves_list(ssl_ctx, tlsconf.ecdh_curves.data()) != 1) {
    LOG(FATAL) << "SSL_CTX_set1_curves_list " << tlsconf.ecdh_curves
               << " failed";
    DIE();
  }
#  endif // OPENSSL_NO_EC

  if (!tlsconf.dh_param_file.empty()) {
    // Read DH parameters from file
    auto bio = BIO_new_file(tlsconf.dh_param_file.data(), "rb");
    if (bio == nullptr) {
      LOG(FATAL) << "BIO_new_file() failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
#  if OPENSSL_3_0_0_API
    EVP_PKEY *dh = nullptr;
    auto dctx = OSSL_DECODER_CTX_new_for_pkey(
      &dh, "PEM", nullptr, "DH", OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS, nullptr,
      nullptr);

    if (!OSSL_DECODER_from_bio(dctx, bio)) {
      LOG(FATAL) << "OSSL_DECODER_from_bio() failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }

    if (SSL_CTX_set0_tmp_dh_pkey(ssl_ctx, dh) != 1) {
      LOG(FATAL) << "SSL_CTX_set0_tmp_dh_pkey failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
#  else  // !OPENSSL_3_0_0_API
    auto dh = PEM_read_bio_DHparams(bio, nullptr, nullptr, nullptr);
    if (dh == nullptr) {
      LOG(FATAL) << "PEM_read_bio_DHparams() failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
    SSL_CTX_set_tmp_dh(ssl_ctx, dh);
    DH_free(dh);
#  endif // !OPENSSL_3_0_0_API
    BIO_free(bio);
  }

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

  if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
    LOG(WARN) << "Could not load system trusted ca certificates: "
              << ERR_error_string(ERR_get_error(), nullptr);
  }

  if (!tlsconf.cacert.empty()) {
    if (SSL_CTX_load_verify_locations(ssl_ctx, tlsconf.cacert.data(),
                                      nullptr) != 1) {
      LOG(FATAL) << "Could not load trusted ca certificates from "
                 << tlsconf.cacert << ": "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
  }

  if (!tlsconf.private_key_passwd.empty()) {
    SSL_CTX_set_default_passwd_cb(ssl_ctx, ssl_pem_passwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, config);
  }

#  ifndef HAVE_NEVERBLEED
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file,
                                  SSL_FILETYPE_PEM) != 1) {
    LOG(FATAL) << "SSL_CTX_use_PrivateKey_file failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#  else  // HAVE_NEVERBLEED
  std::array<char, NEVERBLEED_ERRBUF_SIZE> errbuf;
  if (neverbleed_load_private_key_file(nb, ssl_ctx, private_key_file,
                                       errbuf.data()) != 1) {
    LOG(FATAL) << "neverbleed_load_private_key_file failed: " << errbuf.data();
    DIE();
  }
#  endif // HAVE_NEVERBLEED

  if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1) {
    LOG(FATAL) << "SSL_CTX_use_certificate_file failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
  if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
    LOG(FATAL) << "SSL_CTX_check_private_key failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
  if (tlsconf.client_verify.enabled) {
    if (!tlsconf.client_verify.cacert.empty()) {
      if (SSL_CTX_load_verify_locations(
            ssl_ctx, tlsconf.client_verify.cacert.data(), nullptr) != 1) {
        LOG(FATAL) << "Could not load trusted ca certificates from "
                   << tlsconf.client_verify.cacert << ": "
                   << ERR_error_string(ERR_get_error(), nullptr);
        DIE();
      }
      // It is heard that SSL_CTX_load_verify_locations() may leave
      // error even though it returns success. See
      // http://forum.nginx.org/read.php?29,242540
      ERR_clear_error();
      auto list = SSL_load_client_CA_file(tlsconf.client_verify.cacert.data());
      if (!list) {
        LOG(FATAL) << "Could not load ca certificates from "
                   << tlsconf.client_verify.cacert << ": "
                   << ERR_error_string(ERR_get_error(), nullptr);
        DIE();
      }
      SSL_CTX_set_client_CA_list(ssl_ctx, list);
    }
    SSL_CTX_set_verify(ssl_ctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE |
                         SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       verify_callback);
  }
  SSL_CTX_set_tlsext_servername_callback(ssl_ctx, servername_callback);
#  if OPENSSL_3_0_0_API
  SSL_CTX_set_tlsext_ticket_key_evp_cb(ssl_ctx, ticket_key_cb);
#  else  // !OPENSSL_3_0_0_API
  SSL_CTX_set_tlsext_ticket_key_cb(ssl_ctx, ticket_key_cb);
#  endif // !OPENSSL_3_0_0_API
#  ifndef NGHTTP2_OPENSSL_IS_BORINGSSL
  SSL_CTX_set_tlsext_status_cb(ssl_ctx, ocsp_resp_cb);
#  endif // NGHTTP2_OPENSSL_IS_BORINGSSL

  // ALPN selection callback
  SSL_CTX_set_alpn_select_cb(ssl_ctx, quic_alpn_select_proto_cb, nullptr);

  auto tls_ctx_data = new TLSContextData();
  tls_ctx_data->cert_file = cert_file;
  tls_ctx_data->sct_data = sct_data;

  SSL_CTX_set_app_data(ssl_ctx, tls_ctx_data);

#  ifdef NGHTTP2_GENUINE_OPENSSL
  // SSL_extension_supported(TLSEXT_TYPE_signed_certificate_timestamp)
  // returns 1, which means OpenSSL internally handles it.  But
  // OpenSSL handles signed_certificate_timestamp extension specially,
  // and it lets custom handler to process the extension.
  if (!sct_data.empty()) {
    // It is not entirely clear to me that SSL_EXT_CLIENT_HELLO is
    // required here.  sct_parse_cb is called without
    // SSL_EXT_CLIENT_HELLO being set.  But the passed context value
    // is SSL_EXT_CLIENT_HELLO.
    if (SSL_CTX_add_custom_ext(
          ssl_ctx, TLSEXT_TYPE_signed_certificate_timestamp,
          SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO |
            SSL_EXT_TLS1_3_CERTIFICATE | SSL_EXT_IGNORE_ON_RESUMPTION,
          sct_add_cb, sct_free_cb, nullptr, sct_parse_cb, nullptr) != 1) {
      LOG(FATAL) << "SSL_CTX_add_custom_ext failed: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
  }
#  elif defined(NGHTTP2_OPENSSL_IS_BORINGSSL)
  if (!tls_ctx_data->sct_data.empty() &&
      SSL_CTX_set_signed_cert_timestamp_list(
        ssl_ctx, tls_ctx_data->sct_data.data(),
        tls_ctx_data->sct_data.size()) != 1) {
    LOG(FATAL) << "SSL_CTX_set_signed_cert_timestamp_list failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#  endif // NGHTTP2_OPENSSL_IS_BORINGSSL

#  if defined(NGHTTP2_GENUINE_OPENSSL) ||                                      \
    (defined(NGHTTP2_OPENSSL_IS_WOLFSSL) && defined(WOLFSSL_EARLY_DATA))
  auto &quicconf = config->quic;

  if (quicconf.upstream.early_data &&
      SSL_CTX_set_max_early_data(ssl_ctx,
                                 std::numeric_limits<uint32_t>::max()) != 1) {
    LOG(FATAL) << "SSL_CTX_set_max_early_data failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#  endif // NGHTTP2_GENUINE_OPENSSL || (NGHTTP2_OPENSSL_IS_WOLFSSL &&
         // WOLFSSL_EARLY_DATA)

#  ifndef OPENSSL_NO_PSK
  SSL_CTX_set_psk_server_callback(ssl_ctx, psk_server_cb);
#  endif // !LIBRESSL_NO_PSK

#  if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
  if (!SSL_CTX_add_cert_compression_alg(
        ssl_ctx, nghttp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
        cert_compress, cert_decompress)) {
    LOG(FATAL) << "SSL_CTX_add_cert_compression_alg failed";
    DIE();
  }
#  endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

  return ssl_ctx;
}
#endif // ENABLE_HTTP3

SSL_CTX *create_ssl_client_context(
#ifdef HAVE_NEVERBLEED
  neverbleed_t *nb,
#endif // HAVE_NEVERBLEED
  const StringRef &cacert, const StringRef &cert_file,
  const StringRef &private_key_file) {
  auto ssl_ctx = SSL_CTX_new(TLS_client_method());
  if (!ssl_ctx) {
    LOG(FATAL) << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }

  auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                  SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                  SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

  auto &tlsconf = get_config()->tls;

#ifdef SSL_OP_ENABLE_KTLS
  if (tlsconf.ktls) {
    ssl_opts |= SSL_OP_ENABLE_KTLS;
  }
#endif // SSL_OP_ENABLE_KTLS

  SSL_CTX_set_options(ssl_ctx, ssl_opts | tlsconf.tls_proto_mask);

  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_CLIENT |
                                            SSL_SESS_CACHE_NO_INTERNAL_STORE);
  SSL_CTX_sess_set_new_cb(ssl_ctx, tls_session_client_new_cb);

  if (nghttp2::tls::ssl_ctx_set_proto_versions(
        ssl_ctx, tlsconf.min_proto_version, tlsconf.max_proto_version) != 0) {
    LOG(FATAL) << "Could not set TLS protocol version";
    DIE();
  }

  if (SSL_CTX_set_cipher_list(ssl_ctx, tlsconf.client.ciphers.data()) == 0) {
    LOG(FATAL) << "SSL_CTX_set_cipher_list " << tlsconf.client.ciphers
               << " failed: " << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL) || defined(NGHTTP2_OPENSSL_IS_WOLFSSL)
  if (SSL_CTX_set_ciphersuites(ssl_ctx, tlsconf.client.tls13_ciphers.data()) ==
      0) {
    LOG(FATAL) << "SSL_CTX_set_ciphersuites " << tlsconf.client.tls13_ciphers
               << " failed: " << ERR_error_string(ERR_get_error(), nullptr);
    DIE();
  }
#endif // NGHTTP2_GENUINE_OPENSSL || NGHTTP2_OPENSSL_IS_LIBRESSL ||
       // NGHTTP2_OPENSSL_IS_WOLFSSL

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

  if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
    LOG(WARN) << "Could not load system trusted ca certificates: "
              << ERR_error_string(ERR_get_error(), nullptr);
  }

  if (!cacert.empty()) {
    if (SSL_CTX_load_verify_locations(ssl_ctx, cacert.data(), nullptr) != 1) {
      LOG(FATAL) << "Could not load trusted ca certificates from " << cacert
                 << ": " << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
  }

  if (!tlsconf.insecure) {
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
  }

  if (!cert_file.empty()) {
    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file.data()) != 1) {
      LOG(FATAL) << "Could not load client certificate from " << cert_file
                 << ": " << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
  }

  if (!private_key_file.empty()) {
#ifndef HAVE_NEVERBLEED
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file.data(),
                                    SSL_FILETYPE_PEM) != 1) {
      LOG(FATAL) << "Could not load client private key from "
                 << private_key_file << ": "
                 << ERR_error_string(ERR_get_error(), nullptr);
      DIE();
    }
#else  // HAVE_NEVERBLEED
    std::array<char, NEVERBLEED_ERRBUF_SIZE> errbuf;
    if (neverbleed_load_private_key_file(nb, ssl_ctx, private_key_file.data(),
                                         errbuf.data()) != 1) {
      LOG(FATAL) << "neverbleed_load_private_key_file: could not load client "
                    "private key from "
                 << private_key_file << ": " << errbuf.data();
      DIE();
    }
#endif // HAVE_NEVERBLEED
  }

#ifndef OPENSSL_NO_PSK
  SSL_CTX_set_psk_client_callback(ssl_ctx, psk_client_cb);
#endif // !OPENSSL_NO_PSK

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
  if (!SSL_CTX_add_cert_compression_alg(
        ssl_ctx, nghttp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
        cert_compress, cert_decompress)) {
    LOG(FATAL) << "SSL_CTX_add_cert_compression_alg failed";
    DIE();
  }
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

  return ssl_ctx;
}

SSL *create_ssl(SSL_CTX *ssl_ctx) {
  auto ssl = SSL_new(ssl_ctx);
  if (!ssl) {
    LOG(ERROR) << "SSL_new() failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    return nullptr;
  }

  return ssl;
}

ClientHandler *accept_connection(Worker *worker, int fd, sockaddr *addr,
                                 int addrlen, const UpstreamAddr *faddr) {
  std::array<char, NI_MAXHOST> host;
  std::array<char, NI_MAXSERV> service;
  int rv;

  if (addr->sa_family == AF_UNIX) {
    std::copy_n("localhost", sizeof("localhost"), std::begin(host));
    service[0] = '\0';
  } else {
    rv = getnameinfo(addr, addrlen, host.data(), host.size(), service.data(),
                     service.size(), NI_NUMERICHOST | NI_NUMERICSERV);
    if (rv != 0) {
      LOG(ERROR) << "getnameinfo() failed: " << gai_strerror(rv);

      return nullptr;
    }

    rv = util::make_socket_nodelay(fd);
    if (rv == -1) {
      LOG(WARN) << "Setting option TCP_NODELAY failed: errno=" << errno;
    }
  }
  SSL *ssl = nullptr;
  if (faddr->tls) {
    auto ssl_ctx = worker->get_sv_ssl_ctx();

    assert(ssl_ctx);

    ssl = create_ssl(ssl_ctx);
    if (!ssl) {
      return nullptr;
    }
    // Disable TLS session ticket if we don't have working ticket
    // keys.
    if (!worker->get_ticket_keys()) {
      SSL_set_options(ssl, SSL_OP_NO_TICKET);
    }
  }

  return new ClientHandler(worker, fd, ssl, StringRef{host.data()},
                           StringRef{service.data()}, addr->sa_family, faddr);
}

bool tls_hostname_match(const StringRef &pattern, const StringRef &hostname) {
  auto ptWildcard = std::find(std::begin(pattern), std::end(pattern), '*');
  if (ptWildcard == std::end(pattern)) {
    return util::strieq(pattern, hostname);
  }

  auto ptLeftLabelEnd = std::find(std::begin(pattern), std::end(pattern), '.');
  auto wildcardEnabled = true;
  // Do case-insensitive match. At least 2 dots are required to enable
  // wildcard match. Also wildcard must be in the left-most label.
  // Don't attempt to match a presented identifier where the wildcard
  // character is embedded within an A-label.
  if (ptLeftLabelEnd == std::end(pattern) ||
      std::find(ptLeftLabelEnd + 1, std::end(pattern), '.') ==
        std::end(pattern) ||
      ptLeftLabelEnd < ptWildcard || util::istarts_with(pattern, "xn--"_sr)) {
    wildcardEnabled = false;
  }

  if (!wildcardEnabled) {
    return util::strieq(pattern, hostname);
  }

  auto hnLeftLabelEnd =
    std::find(std::begin(hostname), std::end(hostname), '.');
  if (hnLeftLabelEnd == std::end(hostname) ||
      !util::strieq(StringRef{ptLeftLabelEnd, std::end(pattern)},
                    StringRef{hnLeftLabelEnd, std::end(hostname)})) {
    return false;
  }
  // Perform wildcard match. Here '*' must match at least one
  // character.
  if (hnLeftLabelEnd - std::begin(hostname) <
      ptLeftLabelEnd - std::begin(pattern)) {
    return false;
  }
  return util::istarts_with(StringRef{std::begin(hostname), hnLeftLabelEnd},
                            StringRef{std::begin(pattern), ptWildcard}) &&
         util::iends_with(StringRef{std::begin(hostname), hnLeftLabelEnd},
                          StringRef{ptWildcard + 1, ptLeftLabelEnd});
}

namespace {
// if return value is not empty, StringRef.c_str() must be freed using
// OPENSSL_free().
StringRef get_common_name(X509 *cert) {
  auto subjectname = X509_get_subject_name(cert);
  if (!subjectname) {
    LOG(WARN) << "Could not get X509 name object from the certificate.";
    return StringRef{};
  }
  int lastpos = -1;
  for (;;) {
    lastpos = X509_NAME_get_index_by_NID(subjectname, NID_commonName, lastpos);
    if (lastpos == -1) {
      break;
    }
    auto entry = X509_NAME_get_entry(subjectname, lastpos);

    unsigned char *p;
    auto plen = ASN1_STRING_to_UTF8(&p, X509_NAME_ENTRY_get_data(entry));
    if (plen < 0) {
      continue;
    }
    if (std::find(p, p + plen, '\0') != p + plen) {
      // Embedded NULL is not permitted.
      continue;
    }
    if (plen == 0) {
      LOG(WARN) << "X509 name is empty";
      OPENSSL_free(p);
      continue;
    }

    return StringRef{p, static_cast<size_t>(plen)};
  }
  return StringRef{};
}
} // namespace

int verify_numeric_hostname(X509 *cert, const StringRef &hostname,
                            const Address *addr) {
  const void *saddr;
  size_t saddrlen;
  switch (addr->su.storage.ss_family) {
  case AF_INET:
    saddr = &addr->su.in.sin_addr;
    saddrlen = sizeof(addr->su.in.sin_addr);
    break;
  case AF_INET6:
    saddr = &addr->su.in6.sin6_addr;
    saddrlen = sizeof(addr->su.in6.sin6_addr);
    break;
  default:
    return -1;
  }

  auto altnames = static_cast<GENERAL_NAMES *>(
    X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
  if (altnames) {
    auto altnames_deleter = defer(GENERAL_NAMES_free, altnames);
    size_t n = sk_GENERAL_NAME_num(altnames);
    auto ip_found = false;
    for (size_t i = 0; i < n; ++i) {
      auto altname = sk_GENERAL_NAME_value(altnames, i);
      if (altname->type != GEN_IPADD) {
        continue;
      }

      auto ip_addr = altname->d.iPAddress->data;
      if (!ip_addr) {
        continue;
      }
      size_t ip_addrlen = altname->d.iPAddress->length;

      ip_found = true;
      if (saddrlen == ip_addrlen && memcmp(saddr, ip_addr, ip_addrlen) == 0) {
        return 0;
      }
    }

    if (ip_found) {
      return -1;
    }
  }

  auto cn = get_common_name(cert);
  if (cn.empty()) {
    return -1;
  }

  // cn is not NULL terminated
  auto rv = hostname == cn;
  OPENSSL_free(const_cast<char *>(cn.data()));

  if (rv) {
    return 0;
  }

  return -1;
}

int verify_dns_hostname(X509 *cert, const StringRef &hostname) {
  auto altnames = static_cast<GENERAL_NAMES *>(
    X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
  if (altnames) {
    auto dns_found = false;
    auto altnames_deleter = defer(GENERAL_NAMES_free, altnames);
    size_t n = sk_GENERAL_NAME_num(altnames);
    for (size_t i = 0; i < n; ++i) {
      auto altname = sk_GENERAL_NAME_value(altnames, i);
      if (altname->type != GEN_DNS) {
        continue;
      }

      auto name = ASN1_STRING_get0_data(altname->d.ia5);
      if (!name) {
        continue;
      }

      auto len = ASN1_STRING_length(altname->d.ia5);
      if (len == 0) {
        continue;
      }
      if (std::find(name, name + len, '\0') != name + len) {
        // Embedded NULL is not permitted.
        continue;
      }

      if (name[len - 1] == '.') {
        --len;
        if (len == 0) {
          continue;
        }
      }

      dns_found = true;

      if (tls_hostname_match(StringRef{name, static_cast<size_t>(len)},
                             hostname)) {
        return 0;
      }
    }

    // RFC 6125, section 6.4.4. says that client MUST not seek a match
    // for CN if a dns dNSName is found.
    if (dns_found) {
      return -1;
    }
  }

  auto cn = get_common_name(cert);
  if (cn.empty()) {
    return -1;
  }

  if (cn[cn.size() - 1] == '.') {
    if (cn.size() == 1) {
      OPENSSL_free(const_cast<char *>(cn.data()));

      return -1;
    }
    cn = StringRef{cn.data(), cn.size() - 1};
  }

  auto rv = tls_hostname_match(cn, hostname);
  OPENSSL_free(const_cast<char *>(cn.data()));

  return rv ? 0 : -1;
}

namespace {
int verify_hostname(X509 *cert, const StringRef &hostname,
                    const Address *addr) {
  if (util::numeric_host(hostname.data())) {
    return verify_numeric_hostname(cert, hostname, addr);
  }

  return verify_dns_hostname(cert, hostname);
}
} // namespace

int check_cert(SSL *ssl, const Address *addr, const StringRef &host) {
#if OPENSSL_3_0_0_API
  auto cert = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto cert = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!cert) {
    // By the protocol definition, TLS server always sends certificate
    // if it has.  If certificate cannot be retrieved, authentication
    // without certificate is used, such as PSK.
    return 0;
  }
#if !OPENSSL_3_0_0_API
  auto cert_deleter = defer(X509_free, cert);
#endif // !OPENSSL_3_0_0_API

  if (verify_hostname(cert, host, addr) != 0) {
    LOG(ERROR) << "Certificate verification failed: hostname does not match";
    return -1;
  }
  return 0;
}

int check_cert(SSL *ssl, const DownstreamAddr *addr, const Address *raddr) {
  auto hostname =
    addr->sni.empty() ? StringRef{addr->host} : StringRef{addr->sni};
  return check_cert(ssl, raddr, hostname);
}

CertLookupTree::CertLookupTree() {}

ssize_t CertLookupTree::add_cert(const StringRef &hostname, size_t idx) {
  std::array<char, NI_MAXHOST> buf;

  // NI_MAXHOST includes terminal NULL byte
  if (hostname.empty() || hostname.size() + 1 > buf.size()) {
    return -1;
  }

  auto wildcard_it = std::find(std::begin(hostname), std::end(hostname), '*');
  if (wildcard_it != std::end(hostname) &&
      wildcard_it + 1 != std::end(hostname)) {
    auto wildcard_prefix = StringRef{std::begin(hostname), wildcard_it};
    auto wildcard_suffix = StringRef{wildcard_it + 1, std::end(hostname)};

    auto rev_suffix =
      StringRef{std::begin(buf),
                std::reverse_copy(std::begin(wildcard_suffix),
                                  std::end(wildcard_suffix), std::begin(buf))};

    WildcardPattern *wpat;

    if (wildcard_patterns_.size() !=
        rev_wildcard_router_.add_route(rev_suffix, wildcard_patterns_.size())) {
      auto wcidx = rev_wildcard_router_.match(rev_suffix);

      assert(wcidx != -1);

      wpat = &wildcard_patterns_[wcidx];
    } else {
      wildcard_patterns_.emplace_back();
      wpat = &wildcard_patterns_.back();
    }

    auto rev_prefix =
      StringRef{std::begin(buf),
                std::reverse_copy(std::begin(wildcard_prefix),
                                  std::end(wildcard_prefix), std::begin(buf))};

    for (auto &p : wpat->rev_prefix) {
      if (p.prefix == rev_prefix) {
        return p.idx;
      }
    }

    wpat->rev_prefix.emplace_back(rev_prefix, idx);

    return idx;
  }

  return router_.add_route(hostname, idx);
}

ssize_t CertLookupTree::lookup(const StringRef &hostname) {
  std::array<char, NI_MAXHOST> buf;

  // NI_MAXHOST includes terminal NULL byte
  if (hostname.empty() || hostname.size() + 1 > buf.size()) {
    return -1;
  }

  // Always prefer exact match
  auto idx = router_.match(hostname);
  if (idx != -1) {
    return idx;
  }

  if (wildcard_patterns_.empty()) {
    return -1;
  }

  ssize_t best_idx = -1;
  size_t best_prefixlen = 0;
  const RNode *last_node = nullptr;

  auto rev_host = StringRef{
    std::begin(buf), std::reverse_copy(std::begin(hostname), std::end(hostname),
                                       std::begin(buf))};

  for (;;) {
    size_t nread = 0;

    auto wcidx =
      rev_wildcard_router_.match_prefix(&nread, &last_node, rev_host);
    if (wcidx == -1) {
      return best_idx;
    }

    // '*' must match at least one byte
    if (nread == rev_host.size()) {
      return best_idx;
    }

    rev_host = StringRef{std::begin(rev_host) + nread, std::end(rev_host)};

    auto rev_prefix = StringRef{std::begin(rev_host) + 1, std::end(rev_host)};

    auto &wpat = wildcard_patterns_[wcidx];
    for (auto &wprefix : wpat.rev_prefix) {
      if (!util::ends_with(rev_prefix, wprefix.prefix)) {
        continue;
      }

      auto prefixlen = wprefix.prefix.size() + (&rev_host[0] - &buf[0]);

      // Breaking a tie with longer suffix
      if (prefixlen < best_prefixlen) {
        continue;
      }

      best_idx = wprefix.idx;
      best_prefixlen = prefixlen;
    }
  }
}

void CertLookupTree::dump() const {
  std::cerr << "exact:" << std::endl;
  router_.dump();
  std::cerr << "wildcard suffix (reversed):" << std::endl;
  rev_wildcard_router_.dump();
}

int cert_lookup_tree_add_ssl_ctx(
  CertLookupTree *lt, std::vector<std::vector<SSL_CTX *>> &indexed_ssl_ctx,
  SSL_CTX *ssl_ctx) {
  std::array<char, NI_MAXHOST> buf;

  auto cert = SSL_CTX_get0_certificate(ssl_ctx);
  auto altnames = static_cast<GENERAL_NAMES *>(
    X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
  if (altnames) {
    auto altnames_deleter = defer(GENERAL_NAMES_free, altnames);
    size_t n = sk_GENERAL_NAME_num(altnames);
    auto dns_found = false;
    for (size_t i = 0; i < n; ++i) {
      auto altname = sk_GENERAL_NAME_value(altnames, i);
      if (altname->type != GEN_DNS) {
        continue;
      }

      auto name = ASN1_STRING_get0_data(altname->d.ia5);
      if (!name) {
        continue;
      }

      auto len = ASN1_STRING_length(altname->d.ia5);
      if (len == 0) {
        continue;
      }
      if (std::find(name, name + len, '\0') != name + len) {
        // Embedded NULL is not permitted.
        continue;
      }

      if (name[len - 1] == '.') {
        --len;
        if (len == 0) {
          continue;
        }
      }

      dns_found = true;

      if (static_cast<size_t>(len) + 1 > buf.size()) {
        continue;
      }

      auto end_buf = std::copy_n(name, len, std::begin(buf));
      util::inp_strlower(std::begin(buf), end_buf);

      auto idx = lt->add_cert(StringRef{std::begin(buf), end_buf},
                              indexed_ssl_ctx.size());
      if (idx == -1) {
        continue;
      }

      if (static_cast<size_t>(idx) < indexed_ssl_ctx.size()) {
        indexed_ssl_ctx[idx].push_back(ssl_ctx);
      } else {
        assert(static_cast<size_t>(idx) == indexed_ssl_ctx.size());
        indexed_ssl_ctx.emplace_back(std::vector<SSL_CTX *>{ssl_ctx});
      }
    }

    // Don't bother CN if we have dNSName.
    if (dns_found) {
      return 0;
    }
  }

  auto cn = get_common_name(cert);
  if (cn.empty()) {
    return 0;
  }

  if (cn[cn.size() - 1] == '.') {
    if (cn.size() == 1) {
      OPENSSL_free(const_cast<char *>(cn.data()));

      return 0;
    }

    cn = StringRef{cn.data(), cn.size() - 1};
  }

  auto end_buf = std::copy(std::begin(cn), std::end(cn), std::begin(buf));

  OPENSSL_free(const_cast<char *>(cn.data()));

  util::inp_strlower(std::begin(buf), end_buf);

  auto idx =
    lt->add_cert(StringRef{std::begin(buf), end_buf}, indexed_ssl_ctx.size());
  if (idx == -1) {
    return 0;
  }

  if (static_cast<size_t>(idx) < indexed_ssl_ctx.size()) {
    indexed_ssl_ctx[idx].push_back(ssl_ctx);
  } else {
    assert(static_cast<size_t>(idx) == indexed_ssl_ctx.size());
    indexed_ssl_ctx.emplace_back(std::vector<SSL_CTX *>{ssl_ctx});
  }

  return 0;
}

bool in_proto_list(const std::vector<StringRef> &protos,
                   const StringRef &needle) {
  for (auto &proto : protos) {
    if (proto == needle) {
      return true;
    }
  }
  return false;
}

bool upstream_tls_enabled(const ConnectionConfig &connconf) {
#ifdef ENABLE_HTTP3
  if (connconf.quic_listener.addrs.size()) {
    return true;
  }
#endif // ENABLE_HTTP3

  const auto &faddrs = connconf.listener.addrs;
  return std::any_of(std::begin(faddrs), std::end(faddrs),
                     [](const UpstreamAddr &faddr) { return faddr.tls; });
}

X509 *load_certificate(const char *filename) {
  auto bio = BIO_new(BIO_s_file());
  if (!bio) {
    fprintf(stderr, "BIO_new() failed\n");
    return nullptr;
  }
  auto bio_deleter = defer(BIO_vfree, bio);
  if (!BIO_read_filename(bio, filename)) {
    fprintf(stderr, "Could not read certificate file '%s'\n", filename);
    return nullptr;
  }
  auto cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  if (!cert) {
    fprintf(stderr, "Could not read X509 structure from file '%s'\n", filename);
    return nullptr;
  }

  return cert;
}

SSL_CTX *
setup_server_ssl_context(std::vector<SSL_CTX *> &all_ssl_ctx,
                         std::vector<std::vector<SSL_CTX *>> &indexed_ssl_ctx,
                         CertLookupTree *cert_tree
#ifdef HAVE_NEVERBLEED
                         ,
                         neverbleed_t *nb
#endif // HAVE_NEVERBLEED
) {
  auto config = get_config();

  if (!upstream_tls_enabled(config->conn)) {
    return nullptr;
  }

  auto &tlsconf = config->tls;

  auto ssl_ctx = create_ssl_context(tlsconf.private_key_file.data(),
                                    tlsconf.cert_file.data(), tlsconf.sct_data
#ifdef HAVE_NEVERBLEED
                                    ,
                                    nb
#endif // HAVE_NEVERBLEED
  );

  all_ssl_ctx.push_back(ssl_ctx);

  assert(cert_tree);

  if (cert_lookup_tree_add_ssl_ctx(cert_tree, indexed_ssl_ctx, ssl_ctx) == -1) {
    LOG(FATAL) << "Failed to add default certificate.";
    DIE();
  }

  for (auto &c : tlsconf.subcerts) {
    auto ssl_ctx = create_ssl_context(c.private_key_file.data(),
                                      c.cert_file.data(), c.sct_data
#ifdef HAVE_NEVERBLEED
                                      ,
                                      nb
#endif // HAVE_NEVERBLEED
    );
    all_ssl_ctx.push_back(ssl_ctx);

    if (cert_lookup_tree_add_ssl_ctx(cert_tree, indexed_ssl_ctx, ssl_ctx) ==
        -1) {
      LOG(FATAL) << "Failed to add sub certificate.";
      DIE();
    }
  }

  return ssl_ctx;
}

#ifdef ENABLE_HTTP3
SSL_CTX *setup_quic_server_ssl_context(
  std::vector<SSL_CTX *> &all_ssl_ctx,
  std::vector<std::vector<SSL_CTX *>> &indexed_ssl_ctx,
  CertLookupTree *cert_tree
#  ifdef HAVE_NEVERBLEED
  ,
  neverbleed_t *nb
#  endif // HAVE_NEVERBLEED
) {
  auto config = get_config();

  if (!upstream_tls_enabled(config->conn)) {
    return nullptr;
  }

  auto &tlsconf = config->tls;

  auto ssl_ctx = create_quic_ssl_context(
    tlsconf.private_key_file.data(), tlsconf.cert_file.data(), tlsconf.sct_data
#  ifdef HAVE_NEVERBLEED
    ,
    nb
#  endif // HAVE_NEVERBLEED
  );

  all_ssl_ctx.push_back(ssl_ctx);

  assert(cert_tree);

  if (cert_lookup_tree_add_ssl_ctx(cert_tree, indexed_ssl_ctx, ssl_ctx) == -1) {
    LOG(FATAL) << "Failed to add default certificate.";
    DIE();
  }

  for (auto &c : tlsconf.subcerts) {
    auto ssl_ctx = create_quic_ssl_context(c.private_key_file.data(),
                                           c.cert_file.data(), c.sct_data
#  ifdef HAVE_NEVERBLEED
                                           ,
                                           nb
#  endif // HAVE_NEVERBLEED
    );
    all_ssl_ctx.push_back(ssl_ctx);

    if (cert_lookup_tree_add_ssl_ctx(cert_tree, indexed_ssl_ctx, ssl_ctx) ==
        -1) {
      LOG(FATAL) << "Failed to add sub certificate.";
      DIE();
    }
  }

  return ssl_ctx;
}
#endif // ENABLE_HTTP3

SSL_CTX *setup_downstream_client_ssl_context(
#ifdef HAVE_NEVERBLEED
  neverbleed_t *nb
#endif // HAVE_NEVERBLEED
) {
  auto &tlsconf = get_config()->tls;

  return create_ssl_client_context(
#ifdef HAVE_NEVERBLEED
    nb,
#endif // HAVE_NEVERBLEED
    tlsconf.cacert, tlsconf.client.cert_file, tlsconf.client.private_key_file);
}

void setup_downstream_http2_alpn(SSL *ssl) {
  // ALPN advertisement
  auto alpn = util::get_default_alpn();
  SSL_set_alpn_protos(ssl, alpn.data(), alpn.size());
}

void setup_downstream_http1_alpn(SSL *ssl) {
  // ALPN advertisement
  SSL_set_alpn_protos(ssl, NGHTTP2_H1_1_ALPN.byte(), NGHTTP2_H1_1_ALPN.size());
}

std::unique_ptr<CertLookupTree> create_cert_lookup_tree() {
  auto config = get_config();
  if (!upstream_tls_enabled(config->conn)) {
    return nullptr;
  }
  return std::make_unique<CertLookupTree>();
}

namespace {
std::vector<uint8_t> serialize_ssl_session(SSL_SESSION *session) {
  auto len = i2d_SSL_SESSION(session, nullptr);
  auto buf = std::vector<uint8_t>(len);
  auto p = buf.data();
  i2d_SSL_SESSION(session, &p);

  return buf;
}
} // namespace

void try_cache_tls_session(TLSSessionCache *cache, SSL_SESSION *session,
                           const std::chrono::steady_clock::time_point &t) {
  if (cache->last_updated + 1min > t) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Client session cache entry is still fresh.";
    }
    return;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Update client cache entry "
              << "timestamp = " << t.time_since_epoch().count();
  }

  cache->session_data = serialize_ssl_session(session);
  cache->last_updated = t;
}

SSL_SESSION *reuse_tls_session(const TLSSessionCache &cache) {
  if (cache.session_data.empty()) {
    return nullptr;
  }

  auto p = cache.session_data.data();
  return d2i_SSL_SESSION(nullptr, &p, cache.session_data.size());
}

int proto_version_from_string(const StringRef &v) {
#ifdef TLS1_3_VERSION
  if (util::strieq("TLSv1.3"_sr, v)) {
    return TLS1_3_VERSION;
  }
#endif // TLS1_3_VERSION
  if (util::strieq("TLSv1.2"_sr, v)) {
    return TLS1_2_VERSION;
  }
  if (util::strieq("TLSv1.1"_sr, v)) {
    return TLS1_1_VERSION;
  }
  if (util::strieq("TLSv1.0"_sr, v)) {
    return TLS1_VERSION;
  }
  return -1;
}

int verify_ocsp_response(SSL_CTX *ssl_ctx, const uint8_t *ocsp_resp,
                         size_t ocsp_resplen) {
#ifndef OPENSSL_NO_OCSP
  int rv;

  STACK_OF(X509) * chain_certs;
  SSL_CTX_get0_chain_certs(ssl_ctx, &chain_certs);

  auto resp = d2i_OCSP_RESPONSE(nullptr, &ocsp_resp, ocsp_resplen);
  if (resp == nullptr) {
    LOG(ERROR) << "d2i_OCSP_RESPONSE failed";
    return -1;
  }
  auto resp_deleter = defer(OCSP_RESPONSE_free, resp);

  if (OCSP_response_status(resp) != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
    LOG(ERROR) << "OCSP response status is not successful";
    return -1;
  }

  ERR_clear_error();

  auto bs = OCSP_response_get1_basic(resp);
  if (bs == nullptr) {
    LOG(ERROR) << "OCSP_response_get1_basic failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    return -1;
  }
  auto bs_deleter = defer(OCSP_BASICRESP_free, bs);

  auto store = SSL_CTX_get_cert_store(ssl_ctx);

  ERR_clear_error();

  rv = OCSP_basic_verify(bs, chain_certs, store, 0);

  if (rv != 1) {
    LOG(ERROR) << "OCSP_basic_verify failed: "
               << ERR_error_string(ERR_get_error(), nullptr);
    return -1;
  }

  auto sresp = OCSP_resp_get0(bs, 0);
  if (sresp == nullptr) {
    LOG(ERROR) << "OCSP response verification failed: no single response";
    return -1;
  }

  auto certid = OCSP_SINGLERESP_get0_id(sresp);
  assert(certid != nullptr);

  ASN1_INTEGER *serial;
  rv = OCSP_id_get0_info(nullptr, nullptr, nullptr, &serial,
                         const_cast<OCSP_CERTID *>(certid));
  if (rv != 1) {
    LOG(ERROR) << "OCSP_id_get0_info failed";
    return -1;
  }

  if (serial == nullptr) {
    LOG(ERROR) << "OCSP response does not contain serial number";
    return -1;
  }

  auto cert = SSL_CTX_get0_certificate(ssl_ctx);
  auto cert_serial = X509_get_serialNumber(cert);

  if (ASN1_INTEGER_cmp(cert_serial, serial)) {
    LOG(ERROR) << "OCSP verification serial numbers do not match";
    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "OCSP verification succeeded";
  }
#endif // !OPENSSL_NO_OCSP

  return 0;
}

ssize_t get_x509_fingerprint(uint8_t *dst, size_t dstlen, const X509 *x,
                             const EVP_MD *md) {
  unsigned int len = dstlen;
  if (X509_digest(x, md, dst, &len) != 1) {
    return -1;
  }
  return len;
}

namespace {
StringRef get_x509_name(BlockAllocator &balloc, X509_NAME *nm) {
  auto b = BIO_new(BIO_s_mem());
  if (!b) {
    return StringRef{};
  }

  auto b_deleter = defer(BIO_free, b);

  // Not documented, but it seems that X509_NAME_print_ex returns the
  // number of bytes written into b.
  auto slen = X509_NAME_print_ex(b, nm, 0, XN_FLAG_RFC2253);
  if (slen <= 0) {
    return StringRef{};
  }

  auto iov = make_byte_ref(balloc, slen + 1);
  BIO_read(b, iov.data(), slen);
  iov[slen] = '\0';
  return StringRef{iov.data(), static_cast<size_t>(slen)};
}
} // namespace

StringRef get_x509_subject_name(BlockAllocator &balloc, X509 *x) {
  return get_x509_name(balloc, X509_get_subject_name(x));
}

StringRef get_x509_issuer_name(BlockAllocator &balloc, X509 *x) {
  return get_x509_name(balloc, X509_get_issuer_name(x));
}

StringRef get_x509_serial(BlockAllocator &balloc, X509 *x) {
  auto sn = X509_get_serialNumber(x);
  auto bn = BN_new();
  auto bn_d = defer(BN_free, bn);
  if (!ASN1_INTEGER_to_BN(sn, bn) || BN_num_bytes(bn) > 20) {
    return StringRef{};
  }

  std::array<uint8_t, 20> b;
  auto n = BN_bn2bin(bn, b.data());
  assert(n <= 20);

  return util::format_hex(balloc, std::span{b.data(), static_cast<size_t>(n)});
}

namespace {
// Performs conversion from |at| to time_t.  The result is stored in
// |t|.  This function returns 0 if it succeeds, or -1.
int time_t_from_asn1_time(time_t &t, const ASN1_TIME *at) {
  int rv;

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL) || defined(NGHTTP2_OPENSSL_IS_WOLFSSL)
  struct tm tm;
  rv = ASN1_TIME_to_tm(at, &tm);
  if (rv != 1) {
    return -1;
  }

  t = nghttp2_timegm(&tm);
#else // !NGHTTP2_GENUINE_OPENSSL && !NGHTTP2_OPENSSL_IS_LIBRESSL &&
      // !NGHTTP2_OPENSSL_IS_WOLFSSL
  auto b = BIO_new(BIO_s_mem());
  if (!b) {
    return -1;
  }

  auto bio_deleter = defer(BIO_free, b);

  rv = ASN1_TIME_print(b, at);
  if (rv != 1) {
    return -1;
  }

#  ifdef NGHTTP2_OPENSSL_IS_BORINGSSL
  char *s;
#  else
  unsigned char *s;
#  endif
  auto slen = BIO_get_mem_data(b, &s);
  auto tt = util::parse_openssl_asn1_time_print(
    StringRef{s, static_cast<size_t>(slen)});
  if (tt == 0) {
    return -1;
  }

  t = tt;
#endif // !NGHTTP2_GENUINE_OPENSSL && !NGHTTP2_OPENSSL_IS_LIBRESSL &&
       // !NGHTTP2_OPENSSL_IS_WOLFSSL

  return 0;
}
} // namespace

int get_x509_not_before(time_t &t, X509 *x) {
  auto at = X509_get0_notBefore(x);
  if (!at) {
    return -1;
  }

  return time_t_from_asn1_time(t, at);
}

int get_x509_not_after(time_t &t, X509 *x) {
  auto at = X509_get0_notAfter(x);
  if (!at) {
    return -1;
  }

  return time_t_from_asn1_time(t, at);
}

} // namespace tls

} // namespace shrpx
