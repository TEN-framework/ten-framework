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
#ifndef TLS_H
#define TLS_H

#include "nghttp2_config.h"

#include <cinttypes>
#include <string_view>

#include "ssl_compat.h"

using namespace std::literals;

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

namespace nghttp2 {

namespace tls {

// Recommended general purpose "Intermediate compatibility" cipher
// suites for TLSv1.2 by mozilla.
//
// https://wiki.mozilla.org/Security/Server_Side_TLS
constexpr auto DEFAULT_CIPHER_LIST =
  "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-"
  "AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-"
  "POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
  "AES256-GCM-SHA384"sv;

// Recommended general purpose "Modern compatibility" cipher suites
// for TLSv1.3 by mozilla.
//
// https://wiki.mozilla.org/Security/Server_Side_TLS
constexpr auto DEFAULT_TLS13_CIPHER_LIST =
#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL) || defined(NGHTTP2_OPENSSL_IS_WOLFSSL)
  "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:"
  "TLS_CHACHA20_POLY1305_SHA256"sv
#else  // !NGHTTP2_GENUINE_OPENSSL && !NGHTTP2_OPENSSL_IS_LIBRESSL
  ""
#endif // !NGHTTP2_GENUINE_OPENSSL && !NGHTTP2_OPENSSL_IS_LIBRESSL
  ;

constexpr auto NGHTTP2_TLS_MIN_VERSION = TLS1_VERSION;
#ifdef TLS1_3_VERSION
constexpr auto NGHTTP2_TLS_MAX_VERSION = TLS1_3_VERSION;
#else  // !TLS1_3_VERSION
constexpr auto NGHTTP2_TLS_MAX_VERSION = TLS1_2_VERSION;
#endif // !TLS1_3_VERSION

const char *get_tls_protocol(SSL *ssl);

struct TLSSessionInfo {
  const char *cipher;
  const char *protocol;
  const uint8_t *session_id;
  bool session_reused;
  size_t session_id_length;
};

TLSSessionInfo *get_tls_session_info(TLSSessionInfo *tls_info, SSL *ssl);

// Returns true iff the negotiated protocol is TLSv1.2.
bool check_http2_tls_version(SSL *ssl);

// Returns true iff the negotiated cipher suite is in HTTP/2 cipher
// block list.
bool check_http2_cipher_block_list(SSL *ssl);

// Returns true if SSL/TLS requirement for HTTP/2 is fulfilled.
// To fulfill the requirement, the following 2 terms must be hold:
//
// 1. The negotiated protocol must be TLSv1.2.
// 2. The negotiated cipher cuite is not listed in the block list
//    described in RFC 7540.
bool check_http2_requirement(SSL *ssl);

// Sets TLS min and max versions to |ssl_ctx|.  This function returns
// 0 if it succeeds, or -1.
int ssl_ctx_set_proto_versions(SSL_CTX *ssl_ctx, int min, int max);

constexpr uint16_t CERTIFICATE_COMPRESSION_ALGO_BROTLI = 2;

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
int cert_compress(SSL *ssl, CBB *out, const uint8_t *in, size_t in_len);

int cert_decompress(SSL *ssl, CRYPTO_BUFFER **out, size_t uncompressed_len,
                    const uint8_t *in, size_t in_len);
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

// Setup keylog callback.  It returns 0 if it succeeds, or -1.
int setup_keylog_callback(SSL_CTX *ssl_ctx);

} // namespace tls

} // namespace nghttp2

#endif // TLS_H
