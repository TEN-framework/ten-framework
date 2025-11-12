/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
 * Shared JWK handling that's the same whether JOSE or COSE
 */

#include "private-lib-core.h"
#include "private-lib-jose.h"

static const char *meta_names[] = {
	"kty", "kid", "use", "key_ops", "x5c", "alg"
};

static const char meta_b64[] = { 0, 0, 0, 0, 1, 0 };

static const char *oct_names[] = {
	"k"
};
static const char oct_b64[] = { 1 };

static const char *rsa_names[] = {
	"e", "n", "d", "p", "q", "dp", "dq", "qi"
};
static const char rsa_b64[] = { 1, 1, 1, 1, 1, 1, 1, 1 };

static const char *ec_names[] = {
	"crv", "x", "d", "y",
};
static const char ec_b64[] = { 0, 1, 1, 1 };

int
lws_jwk_dump(struct lws_jwk *jwk)
{
	const char **enames, *b64;
	int elems;
	int n;

	(void)enames;
	(void)meta_names;

	switch (jwk->kty) {
	default:
	case LWS_GENCRYPTO_KTY_UNKNOWN:
		lwsl_err("%s: jwk %p: unknown type\n", __func__, jwk);

		return 1;
	case LWS_GENCRYPTO_KTY_OCT:
		elems = LWS_GENCRYPTO_OCT_KEYEL_COUNT;
		enames = oct_names;
		b64 = oct_b64;
		break;
	case LWS_GENCRYPTO_KTY_RSA:
		elems = LWS_GENCRYPTO_RSA_KEYEL_COUNT;
		enames = rsa_names;
		b64 = rsa_b64;
		break;
	case LWS_GENCRYPTO_KTY_EC:
		elems = LWS_GENCRYPTO_EC_KEYEL_COUNT;
		enames = ec_names;
		b64 = ec_b64;
		break;
	}

	lwsl_info("%s: jwk %p\n", __func__, jwk);

	for (n = 0; n < LWS_COUNT_JWK_ELEMENTS; n++) {
		if (jwk->meta[n].buf && meta_b64[n]) {
			lwsl_info("  meta: %s\n", meta_names[n]);
			lwsl_hexdump_info(jwk->meta[n].buf, jwk->meta[n].len);
		}
		if (jwk->meta[n].buf && !meta_b64[n])
			lwsl_info("  meta: %s: '%s'\n", meta_names[n],
					jwk->meta[n].buf);
	}

	for (n = 0; n < elems; n++) {
		if (jwk->e[n].buf && b64[n]) {
			lwsl_info("  e: %s\n", enames[n]);
			lwsl_hexdump_info(jwk->e[n].buf, jwk->e[n].len);
		}
		if (jwk->e[n].buf && !b64[n])
			lwsl_info("  e: %s: '%s'\n", enames[n], jwk->e[n].buf);
	}

	return 0;
}

int
_lws_jwk_set_el_jwk(struct lws_gencrypto_keyelem *e, char *in, size_t len)
{
	e->buf = lws_malloc(len + 1, "jwk");
	if (!e->buf)
		return -1;

	memcpy(e->buf, in, len);
	e->buf[len] = '\0';
	e->len = (uint32_t)len;

	return 0;
}

void
lws_jwk_destroy_elements(struct lws_gencrypto_keyelem *el, int m)
{
	int n;

	for (n = 0; n < m; n++)
		if (el[n].buf) {
			/* wipe all key material when it goes out of scope */
			lws_explicit_bzero(el[n].buf, el[n].len);
			lws_free_set_NULL(el[n].buf);
			el[n].len = 0;
		}
}

void
lws_jwk_destroy(struct lws_jwk *jwk)
{
	lws_jwk_destroy_elements(jwk->e, LWS_ARRAY_SIZE(jwk->e));
	lws_jwk_destroy_elements(jwk->meta, LWS_ARRAY_SIZE(jwk->meta));
}

void
lws_jwk_init_jps(struct lws_jwk_parse_state *jps,
		 struct lws_jwk *jwk, lws_jwk_key_import_callback cb,
		 void *user)
{
	if (jwk)
		memset(jwk, 0, sizeof(*jwk));

	jps->jwk		= jwk;
	jps->possible		= F_RSA | F_EC | F_OCT;
	jps->per_key_cb		= cb;
	jps->user		= user;
	jps->pos		= 0;
	jps->seen		= 0;
	jps->cose_state		= 0;
}

int
lws_jwk_dup_oct(struct lws_jwk *jwk, const void *key, int len)
{
	unsigned int ulen = (unsigned int)len;

	jwk->e[LWS_GENCRYPTO_KTY_OCT].buf = lws_malloc(ulen, __func__);
	if (!jwk->e[LWS_GENCRYPTO_KTY_OCT].buf)
		return -1;

	jwk->kty = LWS_GENCRYPTO_KTY_OCT;
	jwk->e[LWS_GENCRYPTO_OCT_KEYEL_K].len = ulen;

	memcpy(jwk->e[LWS_GENCRYPTO_KTY_OCT].buf, key, ulen);

	return 0;
}

int
lws_jwk_generate(struct lws_context *context, struct lws_jwk *jwk,
	         enum lws_gencrypto_kty kty, int bits, const char *curve)
{
	size_t sn;
	int n;

	memset(jwk, 0, sizeof(*jwk));

	jwk->kty = (int)kty;
	jwk->private_key = 1;

	switch (kty) {
	case LWS_GENCRYPTO_KTY_RSA:
	{
		struct lws_genrsa_ctx ctx;

		lwsl_notice("%s: generating %d bit RSA key\n", __func__, bits);
		n = lws_genrsa_new_keypair(context, &ctx, LGRSAM_PKCS1_1_5,
					    jwk->e, bits);
		lws_genrsa_destroy(&ctx);
		if (n) {
			lwsl_err("%s: problem generating RSA key\n", __func__);
			return 1;
		}
	}
		break;
	case LWS_GENCRYPTO_KTY_OCT:
		sn = (unsigned int)lws_gencrypto_bits_to_bytes(bits);
		jwk->e[LWS_GENCRYPTO_OCT_KEYEL_K].buf = lws_malloc(sn, "oct");
		if (!jwk->e[LWS_GENCRYPTO_OCT_KEYEL_K].buf)
			return 1;
		jwk->e[LWS_GENCRYPTO_OCT_KEYEL_K].len = (uint32_t)sn;
		if (lws_get_random(context,
			     jwk->e[LWS_GENCRYPTO_OCT_KEYEL_K].buf, sn) != sn) {
			lwsl_err("%s: problem getting random\n", __func__);
			return 1;
		}
		break;
	case LWS_GENCRYPTO_KTY_EC:
	{
		struct lws_genec_ctx ctx;

		if (!curve) {
			lwsl_err("%s: must have a named curve\n", __func__);

			return 1;
		}

		if (lws_genecdsa_create(&ctx, context, NULL))
			return 1;

		lwsl_notice("%s: generating ECDSA key on curve %s\n", __func__,
				curve);

		n = lws_genecdsa_new_keypair(&ctx, curve, jwk->e);
		lws_genec_destroy(&ctx);
		if (n) {
			lwsl_err("%s: problem generating ECDSA key\n", __func__);
			return 1;
		}
	}
		break;

	case LWS_GENCRYPTO_KTY_UNKNOWN:
	default:
		lwsl_err("%s: unknown kty\n", __func__);
		return 1;
	}

	return 0;
}

int
lws_jwk_rfc7638_fingerprint(struct lws_jwk *jwk, char *digest32)
{
	struct lws_genhash_ctx hash_ctx;
	size_t tmpsize = 2536;
	char *tmp;
	int n, m = (int)tmpsize;

	tmp = lws_malloc(tmpsize, "rfc7638 tmp");

	n = lws_jwk_export(jwk, LWSJWKF_EXPORT_NOCRLF, tmp, &m);
	if (n < 0)
		goto bail;

	if (lws_genhash_init(&hash_ctx, LWS_GENHASH_TYPE_SHA256))
		goto bail;

	if (lws_genhash_update(&hash_ctx, tmp, (unsigned int)n)) {
		lws_genhash_destroy(&hash_ctx, NULL);

		goto bail;
	}
	lws_free(tmp);

	if (lws_genhash_destroy(&hash_ctx, digest32))
		return -1;

	return 0;

bail:
	lws_free(tmp);

	return -1;
}

int
lws_jwk_strdup_meta(struct lws_jwk *jwk, enum enum_jwk_meta_tok idx,
		    const char *in, int len)
{
	jwk->meta[idx].buf = lws_malloc((unsigned int)len, __func__);
	if (!jwk->meta[idx].buf)
		return 1;
	jwk->meta[idx].len = (uint32_t)(unsigned int)len;
	memcpy(jwk->meta[idx].buf, in, (unsigned int)len);

	return 0;
}

