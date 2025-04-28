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
 */

#include "private-lib-core.h"
#include "private-lib-cose.h"

lws_cose_sig_alg_t *
lws_cose_sign_alg_create(struct lws_context *cx, const lws_cose_key_t *ck,
		    cose_param_t cose_alg, int op)
{
	lws_cose_sig_alg_t *alg = lws_zalloc(sizeof(*alg), __func__);
	const struct lws_gencrypto_keyelem *ke;
	enum lws_genhmac_types ghm;
	enum lws_genhash_types gh;
	const char *crv;

	if (!alg)
		return NULL;

	alg->cose_alg = cose_alg;
	alg->cose_key = ck;

	switch (cose_alg) {

	/* ECDSA algs */

	case LWSCOSE_WKAECDSA_ALG_ES256: /* ECDSA w/ SHA-256 */
		crv = "P-256";
		gh = LWS_GENHASH_TYPE_SHA256;
		alg->keybits = 256;
		goto ecdsa;
	case LWSCOSE_WKAECDSA_ALG_ES384: /* ECDSA w/ SHA-384 */
		crv = "P-384";
		gh = LWS_GENHASH_TYPE_SHA384;
		alg->keybits = 384;
		goto ecdsa;
	case LWSCOSE_WKAECDSA_ALG_ES512: /* ECDSA w/ SHA-512 */
		crv = "P-521";
		gh = LWS_GENHASH_TYPE_SHA512;
		alg->keybits = 521;
ecdsa:

		/* the key is good for this? */

		if (lws_cose_key_checks(ck, LWSCOSE_WKKTV_EC2, cose_alg,
					op, crv))
			goto bail_ecdsa;

		if (lws_genhash_init(&alg->hash_ctx, gh))
			goto bail_ecdsa;

		if (lws_genecdsa_create(&alg->u.ecdsactx, cx, lws_ec_curves)) {
			lwsl_notice("%s: lws_genrsa_public_decrypt_create\n",
				    __func__);
			goto bail_ecdsa1;
		}

		if (lws_genecdsa_set_key(&alg->u.ecdsactx, ck->e)) {
			lwsl_notice("%s: ec key import fail\n", __func__);
			goto bail_ecdsa2;
		}

		break;

	/* HMAC algs */

	case LWSCOSE_WKAHMAC_256_64:
		ghm = LWS_GENHMAC_TYPE_SHA256;
		alg->keybits = 64;
		goto hmac;
	case LWSCOSE_WKAHMAC_256_256:
		ghm = LWS_GENHMAC_TYPE_SHA256;
		alg->keybits = 256;
		goto hmac;
	case LWSCOSE_WKAHMAC_384_384:
		ghm = LWS_GENHMAC_TYPE_SHA384;
		alg->keybits = 384;
		goto hmac;
	case LWSCOSE_WKAHMAC_512_512:
		ghm = LWS_GENHMAC_TYPE_SHA512;
		alg->keybits = 512;

hmac:
		if (lws_cose_key_checks(ck, LWSCOSE_WKKTV_SYMMETRIC,
					cose_alg, op, NULL))
			goto bail_hmac;

		ke = &ck->e[LWS_GENCRYPTO_OCT_KEYEL_K];
		if (lws_genhmac_init(&alg->u.hmacctx, ghm, ke->buf, ke->len))
			goto bail_hmac;

		break;

	/* RSASSA algs */

	case LWSCOSE_WKARSA_ALG_RS256:
		gh = LWS_GENHASH_TYPE_SHA256;
		goto rsassa;

	case LWSCOSE_WKARSA_ALG_RS384:
		gh = LWS_GENHASH_TYPE_SHA384;
		goto rsassa;

	case LWSCOSE_WKARSA_ALG_RS512:
		gh = LWS_GENHASH_TYPE_SHA512;

rsassa:
		if (lws_cose_key_checks(ck, LWSCOSE_WKKTV_RSA, cose_alg,
					op, NULL))
			goto bail_hmac;

		alg->keybits = (int)ck->e[LWS_GENCRYPTO_RSA_KEYEL_N].len * 8;

		if (lws_genhash_init(&alg->hash_ctx, gh))
			goto bail_hmac;

		if (lws_genrsa_create(&alg->u.rsactx, ck->e, cx,
				      LGRSAM_PKCS1_1_5, gh)) {
			lwsl_notice("%s: lws_genrsa_create fail\n", __func__);
			goto bail_hmac;
		}
		break;

	default:
		lwsl_warn("%s: unsupported alg %lld\n", __func__,
				(long long)cose_alg);
		goto bail_hmac;
	}

	return alg;

bail_ecdsa2:
	lws_genec_destroy(&alg->u.ecdsactx);
bail_ecdsa1:
	lws_genhash_destroy(&alg->hash_ctx, NULL);
bail_ecdsa:
	lws_free(alg);

	return NULL;

bail_hmac:
	lws_free(alg);

	return NULL;
}

int
lws_cose_sign_alg_hash(lws_cose_sig_alg_t *alg, const uint8_t *in, size_t in_len)
{
#if defined(VERBOSE)
	lwsl_hexdump_warn(in, in_len);
#endif

	switch (alg->cose_alg) {

	case LWSCOSE_WKAHMAC_256_64:
	case LWSCOSE_WKAHMAC_256_256:
	case LWSCOSE_WKAHMAC_384_384:
	case LWSCOSE_WKAHMAC_512_512:
		return lws_genhmac_update(&alg->u.hmacctx, in, in_len);
	}

	/* EC, rsa are just making the hash before signing */

	return lws_genhash_update(&alg->hash_ctx, in, in_len);
}

/*
 * We fill up alg-> rhash and rhash_len with the results, and destroy the
 * crypto pieces cleanly.  Call lws_cose_sign_alg_destroy() afterwards to
 * clean up the alg itself.
 */

void
lws_cose_sign_alg_complete(lws_cose_sig_alg_t *alg)
{
	uint8_t digest[LWS_GENHASH_LARGEST];
	unsigned int bytes;
	uint8_t htype;
	size_t hs;

	if (alg->completed)
		return;

	switch (alg->cose_alg) {
	case LWSCOSE_WKAECDSA_ALG_ES256: /* ECDSA w/ SHA-256 */
	case LWSCOSE_WKAECDSA_ALG_ES384: /* ECDSA w/ SHA-384 */
	case LWSCOSE_WKAECDSA_ALG_ES512: /* ECDSA w/ SHA-512 */
		hs = lws_genhash_size(alg->hash_ctx.type);
		bytes = (unsigned int)lws_gencrypto_bits_to_bytes(alg->keybits);
		lws_genhash_destroy(&alg->hash_ctx, digest);
		alg->rhash_len = 0;
		lwsl_notice("alg keybits %d hs %d\n", (int)alg->keybits, (int)hs);
		if (!alg->failed &&
		    lws_genecdsa_hash_sign_jws(&alg->u.ecdsactx, digest,
						alg->hash_ctx.type,
						(int)alg->keybits, alg->rhash,
						2u * bytes) >= 0)
			alg->rhash_len = (int)(2 * bytes);
		else
			alg->failed = 1;

		lws_genec_destroy(&alg->u.ecdsactx);
		break;

	case LWSCOSE_WKAHMAC_256_64:
	case LWSCOSE_WKAHMAC_256_256:
	case LWSCOSE_WKAHMAC_384_384:
	case LWSCOSE_WKAHMAC_512_512:
		alg->rhash_len = (int)lws_genhmac_size(alg->u.hmacctx.type);
		if (alg->cose_alg == LWSCOSE_WKAHMAC_256_64)
			alg->rhash_len = 8;

		if (lws_genhmac_destroy(&alg->u.hmacctx, alg->rhash)) {
			lwsl_err("%s: destroy failed\n", __func__);
			break;
		}
		break;

	case LWSCOSE_WKARSA_ALG_RS256:
	case LWSCOSE_WKARSA_ALG_RS384:
	case LWSCOSE_WKARSA_ALG_RS512:
		bytes = (unsigned int)lws_gencrypto_bits_to_bytes(alg->keybits);
		htype = alg->hash_ctx.type;

		if (!lws_genhash_destroy(&alg->hash_ctx, digest) &&
		    !alg->failed &&
		    lws_genrsa_hash_sign(&alg->u.rsactx, digest, htype,
					 alg->rhash, bytes) >= 0)
			alg->rhash_len = (int)bytes;
		else
			lwsl_err("%s: lws_genrsa_hash_sign\n", __func__);

		lws_genrsa_destroy(&alg->u.rsactx);
		break;

	default:
		break;
	}

	alg->completed = 1;
}

void
lws_cose_sign_alg_destroy(lws_cose_sig_alg_t **_alg)
{
	lws_dll2_remove(&(*_alg)->list);
	lws_cose_sign_alg_complete(*_alg);
	lws_free_set_NULL(*_alg);
}
