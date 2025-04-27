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
 *  gencrypto openssl-specific helper declarations
 */

#if !defined(__LWS_PRIVATE_LIB_TLS_OPENSSL_H__)
#define __LWS_PRIVATE_LIB_TLS_OPENSSL_H__

/*
 * one of these per different client context
 * cc_owner is in lws_context.lws_context_tls
 */

struct lws_tls_client_reuse {
	lws_tls_ctx *ssl_client_ctx;
	uint8_t hash[32];
	struct lws_dll2 cc_list;
	int refcount;
	int index;
};

typedef int (*next_proto_cb)(SSL *, const unsigned char **out,
                             unsigned char *outlen, const unsigned char *in,
                             unsigned int inlen, void *arg);

struct lws_x509_cert {
	X509 *cert; /* X509 is opaque, this has to be a pointer */
};

int
lws_gencrypto_openssl_hash_to_NID(enum lws_genhash_types hash_type);

const EVP_MD *
lws_gencrypto_openssl_hash_to_EVP_MD(enum lws_genhash_types hash_type);

#if !defined(LWS_HAVE_BN_bn2binpad)
int BN_bn2binpad(const BIGNUM *a, unsigned char *to, int tolen);
#endif

#endif

