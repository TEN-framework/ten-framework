 /*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 *  This is included from private-lib-core.h if LWS_WITH_TLS
 */

#if !defined(__LWS_TLS_PRIVATE_H__)
#define __LWS_TLS_PRIVATE_H__


#if defined(LWS_WITH_TLS)

#include "private-jit-trust.h"

#if defined(USE_WOLFSSL)
 #if defined(USE_OLD_CYASSL)
  #if defined(_WIN32)
   #include <IDE/WIN/user_settings.h>
   #include <cyassl/ctaocrypt/settings.h>
  #else
   #include <cyassl/options.h>
  #endif
  #include <cyassl/openssl/ssl.h>
  #include <cyassl/error-ssl.h>
 #else
  #if defined(_WIN32)
   #include <IDE/WIN/user_settings.h>
   #include <wolfssl/wolfcrypt/settings.h>
  #else
   #include <wolfssl/options.h>
  #endif
  #include <wolfssl/openssl/ssl.h>
  #include <wolfssl/error-ssl.h>
  #define OPENSSL_NO_TLSEXT
 #endif /* not USE_OLD_CYASSL */
#else /* WOLFSSL */
 #if defined(LWS_PLAT_FREERTOS)
  #define OPENSSL_NO_TLSEXT
  #if !defined(LWS_AMAZON_RTOS)
   /* AMAZON RTOS has its own setting via MTK_MBEDTLS_CONFIG_FILE */
   #undef MBEDTLS_CONFIG_FILE
   #define MBEDTLS_CONFIG_FILE <mbedtls/esp_config.h>
  #endif
  #include <mbedtls/ssl.h>
  #include <mbedtls/aes.h>
  #include <mbedtls/gcm.h>
  #include <mbedtls/x509_crt.h>
  #include "ssl.h" /* wrapper !!!! */
 #else /* not esp32 */
  #if defined(LWS_WITH_MBEDTLS)
   #include <mbedtls/ssl.h>
   #include <mbedtls/aes.h>
   #include <mbedtls/gcm.h>
   #include <mbedtls/x509_crt.h>
   #include <mbedtls/x509_csr.h>
   #include <mbedtls/ecp.h>
   #include <mbedtls/ecdsa.h>
  #if defined(LWS_AMAZON_LINUX)
   #include "ssl.h" /* wrapper !!!! */
  #else
   #include "openssl/ssl.h" /* wrapper !!!! */
  #endif
  #else
   #include <openssl/ssl.h>
   #include <openssl/evp.h>
   #include <openssl/err.h>
   #include <openssl/md5.h>
   #include <openssl/sha.h>
   #include <openssl/rsa.h>
   #include <openssl/bn.h>
   #include <openssl/aes.h>
   #ifdef LWS_HAVE_OPENSSL_ECDH_H
    #include <openssl/ecdh.h>
   #endif
   #if !defined(LWS_HAVE_EVP_MD_CTX_free) && !defined(USE_WOLFSSL)
    #define EVP_MD_CTX_free EVP_MD_CTX_destroy
   #endif
   #include <openssl/x509v3.h>
  #endif /* not mbedtls */
  #if defined(OPENSSL_VERSION_NUMBER)
   #if (OPENSSL_VERSION_NUMBER < 0x0009080afL)
/*
 * later openssl defines this to negate the presence of tlsext... but it was
 * only introduced at 0.9.8j.  Earlier versions don't know it exists so don't
 * define it... making it look like the feature exists...
 */
    #define OPENSSL_NO_TLSEXT
   #endif
  #endif
 #endif /* not ESP32 */
#endif /* not USE_WOLFSSL */

#endif /* LWS_WITH_TLS */

enum lws_tls_extant {
	LWS_TLS_EXTANT_NO,
	LWS_TLS_EXTANT_YES,
	LWS_TLS_EXTANT_ALTERNATIVE
};

#if defined(LWS_WITH_TLS)

#if defined(LWS_WITH_TLS_SESSIONS) && defined(LWS_WITH_CLIENT) && \
	(defined(LWS_WITH_MBEDTLS) || defined(OPENSSL_IS_BORINGSSL))
#define LWS_TLS_SYNTHESIZE_CB 1
#endif

int
lws_tls_restrict_borrow(struct lws *wsi);

void
lws_tls_restrict_return(struct lws *wsi);

void
lws_tls_restrict_return_handshake(struct lws *wsi);

typedef SSL lws_tls_conn;
typedef SSL_CTX lws_tls_ctx;
typedef BIO lws_tls_bio;
typedef X509 lws_tls_x509;

#if defined(LWS_WITH_NETWORK)
#include "private-network.h"
#endif

int
lws_context_init_ssl_library(struct lws_context *cx,
			     const struct lws_context_creation_info *info);
void
lws_context_deinit_ssl_library(struct lws_context *context);
#define LWS_SSL_ENABLED(vh) (vh && vh->tls.use_ssl)

extern const struct lws_tls_ops tls_ops_openssl, tls_ops_mbedtls;

struct lws_ec_valid_curves {
	int id;
	const char *jwa_name; /* list terminates with NULL jwa_name */
};

enum lws_tls_extant
lws_tls_use_any_upgrade_check_extant(const char *name);
extern int openssl_websocket_private_data_index;

void
lws_tls_err_describe_clear(void);

int
lws_tls_openssl_cert_info(X509 *x509, enum lws_tls_cert_info type,
			  union lws_tls_cert_info_results *buf, size_t len);
int
lws_tls_check_all_cert_lifetimes(struct lws_context *context);

int
lws_tls_alloc_pem_to_der_file(struct lws_context *context, const char *filename,
			      const char *inbuf, lws_filepos_t inlen,
			      uint8_t **buf, lws_filepos_t *amount);

int
lws_gencrypto_bits_to_bytes(int bits);

void
lws_gencrypto_destroy_elements(struct lws_gencrypto_keyelem *el, int m);

/* genec */

struct lws_gencrypto_keyelem;
struct lws_ec_curves;

extern const struct lws_ec_curves lws_ec_curves[4];
const struct lws_ec_curves *
lws_genec_curve(const struct lws_ec_curves *table, const char *name);
LWS_VISIBLE void
lws_genec_destroy_elements(struct lws_gencrypto_keyelem *el);
int
lws_gencrypto_mbedtls_rngf(void *context, unsigned char *buf, size_t len);

int
lws_genec_confirm_curve_allowed_by_tls_id(const char *allowed, int id,
					  struct lws_jwk *jwk);

void
lws_tls_reuse_session(struct lws *wsi);

void
lws_tls_session_cache(struct lws_vhost *vh, uint32_t ttl);

int
lws_tls_session_name_from_wsi(struct lws *wsi, char *buf, size_t len);

/**
 * lws_tls_session_name_discrete() - form an lws session tag name from pieces
 *
 * \param vhname: name of the vhost
 * \param host: name of the host we are connecting to, like warmcat.com
 * \param port: the port we connected to
 * \param buf: the destination buffer for the tag
 * \param len: the max available size of the destination buffer
 *
 * Creates a tag string representing a specific host, for use with serializing
 * sessions made with the host.
 */
void
lws_tls_session_tag_discrete(const char *vhname, const char *host,
			     uint16_t port, char *buf, size_t len);

/**
 * lws_tls_session_name_from_wsi() - form an lws session tag name from a client wsi
 *
 * \param wsi: the wsi whose vhost, host and port we should use for the tag
 * \param buf: the destination buffer for the tag
 * \param len: the max available size of the destination buffer
 *
 * Creates a tag string representing a specific host, for use with serializing
 * sessions made with the host.
 */
int
lws_tls_session_tag_from_wsi(struct lws *wsi, char *buf, size_t len);

#else /* ! WITH_TLS */

#define lws_tls_restrict_borrow(xxx) (0)
#define lws_tls_restrict_return(xxx)

#endif
#endif
