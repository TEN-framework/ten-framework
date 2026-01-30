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
#include "tls.h"

#include <cassert>
#include <cstring>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>

#ifdef HAVE_LIBBROTLI
#  include <brotli/encode.h>
#  include <brotli/decode.h>
#endif // HAVE_LIBBROTLI

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/crypto.h>
#  include <wolfssl/openssl/conf.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/crypto.h>
#  include <openssl/conf.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

namespace nghttp2 {

namespace tls {

const char *get_tls_protocol(SSL *ssl) {
  switch (SSL_version(ssl)) {
  case SSL2_VERSION:
    return "SSLv2";
  case SSL3_VERSION:
    return "SSLv3";
#ifdef TLS1_3_VERSION
  case TLS1_3_VERSION:
    return "TLSv1.3";
#endif // TLS1_3_VERSION
  case TLS1_2_VERSION:
    return "TLSv1.2";
  case TLS1_1_VERSION:
    return "TLSv1.1";
  case TLS1_VERSION:
    return "TLSv1";
  default:
    return "unknown";
  }
}

TLSSessionInfo *get_tls_session_info(TLSSessionInfo *tls_info, SSL *ssl) {
  if (!ssl) {
    return nullptr;
  }

  auto session = SSL_get_session(ssl);
  if (!session) {
    return nullptr;
  }

  tls_info->cipher = SSL_get_cipher_name(ssl);
  tls_info->protocol = get_tls_protocol(ssl);
  tls_info->session_reused = SSL_session_reused(ssl);

  unsigned int session_id_length;
  tls_info->session_id = SSL_SESSION_get_id(session, &session_id_length);
  tls_info->session_id_length = session_id_length;

  return tls_info;
}

/* Conditional logic w/ lookup tables to check if id is one of the
   the block listed cipher suites for HTTP/2 described in RFC 7540.
   https://github.com/jay/http2_blacklisted_ciphers
*/
#define IS_CIPHER_BANNED_METHOD2(id)                                           \
  ((0x0000 <= id && id <= 0x00FF &&                                            \
    "\xFF\xFF\xFF\xCF\xFF\xFF\xFF\xFF\x7F\x00\x00\x00\x80\x3F\x00\x00"         \
    "\xF0\xFF\xFF\x3F\xF3\xF3\xFF\xFF\x3F\x00\x00\x00\x00\x00\x00\x80"         \
        [(id & 0xFF) / 8] &                                                    \
      (1 << (id % 8))) ||                                                      \
   (0xC000 <= id && id <= 0xC0FF &&                                            \
    "\xFE\xFF\xFF\xFF\xFF\x67\xFE\xFF\xFF\xFF\x33\xCF\xFC\xCF\xFF\xCF"         \
    "\x3C\xF3\xFC\x3F\x33\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"         \
        [(id & 0xFF) / 8] &                                                    \
      (1 << (id % 8))))

bool check_http2_cipher_block_list(SSL *ssl) {
  int id = SSL_CIPHER_get_id(SSL_get_current_cipher(ssl)) & 0xFFFFFF;

  return IS_CIPHER_BANNED_METHOD2(id);
}

bool check_http2_tls_version(SSL *ssl) {
  auto tls_ver = SSL_version(ssl);

  return tls_ver >= TLS1_2_VERSION;
}

bool check_http2_requirement(SSL *ssl) {
  return check_http2_tls_version(ssl) && !check_http2_cipher_block_list(ssl);
}

int ssl_ctx_set_proto_versions(SSL_CTX *ssl_ctx, int min, int max) {
  if (SSL_CTX_set_min_proto_version(ssl_ctx, min) != 1 ||
      SSL_CTX_set_max_proto_version(ssl_ctx, max) != 1) {
    return -1;
  }
  return 0;
}

#if defined(NGHTTP2_OPENSSL_IS_BORINGSSL) && defined(HAVE_LIBBROTLI)
int cert_compress(SSL *ssl, CBB *out, const uint8_t *in, size_t in_len) {
  uint8_t *dest;

  auto compressed_size = BrotliEncoderMaxCompressedSize(in_len);
  if (compressed_size == 0) {
    return 0;
  }

  if (!CBB_reserve(out, &dest, compressed_size)) {
    return 0;
  }

  if (BrotliEncoderCompress(BROTLI_MAX_QUALITY, BROTLI_DEFAULT_WINDOW,
                            BROTLI_MODE_GENERIC, in_len, in, &compressed_size,
                            dest) != BROTLI_TRUE) {
    return 0;
  }

  if (!CBB_did_write(out, compressed_size)) {
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
    CRYPTO_BUFFER_free(buf);

    return 0;
  }

  if (uncompressed_len != len) {
    CRYPTO_BUFFER_free(buf);

    return 0;
  }

  *out = buf;

  return 1;
}
#endif // NGHTTP2_OPENSSL_IS_BORINGSSL && HAVE_LIBBROTLI

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_BORINGSSL) ||                                     \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL) ||                                      \
  (defined(NGHTTP2_OPENSSL_IS_WOLFSSL) && defined(HAVE_SECRET_CALLBACK))
namespace {
std::ofstream keylog_file;

void keylog_callback(const SSL *ssl, const char *line) {
  keylog_file.write(line, strlen(line));
  keylog_file.put('\n');
  keylog_file.flush();
}
} // namespace

int setup_keylog_callback(SSL_CTX *ssl_ctx) {
  auto keylog_filename = getenv("SSLKEYLOGFILE");
  if (!keylog_filename) {
    return 0;
  }

  keylog_file.open(keylog_filename, std::ios_base::app);
  if (!keylog_file) {
    return -1;
  }

  SSL_CTX_set_keylog_callback(ssl_ctx, keylog_callback);

  return 0;
}
#else  // !NGHTTP2_GENUINE_OPENSSL && !NGHTTP2_OPENSSL_IS_BORINGSSL &&
       // !NGHTTP2_OPENSSL_IS_LIBRESSL && !(NGHTTP2_OPENSSL_IS_WOLFSSL &&
       // HAVE_SECRET_CALLBACK)
int setup_keylog_callback(SSL_CTX *ssl_ctx) { return 0; }
#endif // !NGHTTP2_GENUINE_OPENSSL && !NGHTTP2_OPENSSL_IS_BORINGSSL &&
       // !NGHTTP2_OPENSSL_IS_LIBRESSL && !(NGHTTP2_OPENSSL_IS_WOLFSSL &&
       // HAVE_SECRET_CALLBACK)

} // namespace tls

} // namespace nghttp2
