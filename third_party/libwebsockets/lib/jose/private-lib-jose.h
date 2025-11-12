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
 */

/* information about each token declared above */

#define F_M	(1 <<  9)	/* Mandatory for key type */
#define F_B64	(1 << 10)	/* Base64 coded octets */
#define F_B64U	(1 << 11)	/* Base64 Url coded octets */
#define F_META	(1 << 12)	/* JWK key metainformation */
#define F_RSA	(1 << 13)	/* RSA key */
#define F_EC	(1 << 14)	/* Elliptic curve key */
#define F_OCT	(1 << 15)	/* octet key */

void
lws_jwk_destroy_elements(struct lws_gencrypto_keyelem *el, int m);

int
lws_jose_render(struct lws_jose *jose, struct lws_jwk *aux_jwk,
		char *out, size_t out_len);

int
_lws_jwk_set_el_jwk(struct lws_gencrypto_keyelem *e, char *in, size_t len);

void
lws_jwk_init_jps(struct lws_jwk_parse_state *jps,
		 struct lws_jwk *jwk, lws_jwk_key_import_callback cb,
		 void *user);

signed char
cb_jwk(struct lejp_ctx *ctx, char reason);

extern const char * const jwk_tok[19], * const jwk_outer_tok[19];
