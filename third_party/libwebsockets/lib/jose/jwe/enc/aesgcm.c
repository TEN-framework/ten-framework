/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
#include "private-lib-jose-jwe.h"

/*
 * NOTICE this is AESGCM content encryption, it's not AES GCM key wrapping
 *
 *
 * This section defines the specifics of performing authenticated
 * encryption with AES in Galois/Counter Mode (GCM) ([AES] and
 * [NIST.800-38D]).
 *
 * The CEK is used as the encryption key.
 *
 * Use of an IV of size 96 bits is REQUIRED with this algorithm.
 *
 * The requested size of the Authentication Tag output MUST be 128 bits,
 * regardless of the key size.
 *
 * For decrypt: decrypt the KEK, then decrypt the payload
 *
 * For encrypt: encrypt the payload, then encrypt the KEK
 */

/*
 * encrypting... enc_cek is unencrypted
 */

int
lws_jwe_encrypt_gcm(struct lws_jwe *jwe,
		    uint8_t *enc_cek, uint8_t *aad, int aad_len)
{
	struct lws_gencrypto_keyelem el;
	struct lws_genaes_ctx aesctx;
	size_t ivs = LWS_AESGCM_IV;
	int n;

	/* Some sanity checks on what came in */

	/* MUST be 128-bit for all sizes */
	if (jwe->jws.map.len[LJWE_ATAG] != LWS_AESGCM_TAG) {
		lwsl_notice("%s: AESGCM tag size must be 128b, got %d\n",
				__func__, jwe->jws.map.len[LJWE_ATAG]);
		return -1;
	}

	if (jwe->jws.map.len[LJWE_IV] != LWS_AESGCM_IV) { /* MUST be 96-bit */
		lwsl_notice("%s: AESGCM IV must be 128b, got %d\n", __func__,
				jwe->jws.map.len[LJWE_IV]);
		return -1;
	}

	/* EKEY is directly the CEK KEY */
	el.buf = enc_cek;
	el.len = jwe->jose.enc_alg->keybits_fixed / 8;

	if (lws_genaes_create(&aesctx, LWS_GAESO_ENC, LWS_GAESM_GCM,
			      &el, LWS_GAESP_NO_PADDING, NULL)) {
		lwsl_err("%s: lws_genaes_create failed\n", __func__);

		return -1;
	}

	/* aad */

	n = lws_genaes_crypt(&aesctx, aad, (unsigned int)aad_len, NULL,
			     (uint8_t *)jwe->jws.map.buf[LJWE_IV],
			     (uint8_t *)jwe->jws.map.buf[LJWE_ATAG], &ivs,
			     LWS_AESGCM_TAG);
	if (n) {
		lwsl_err("%s: lws_genaes_crypt aad failed\n", __func__);
		return -1;
	}

	/* payload */
	n = lws_genaes_crypt(&aesctx, (uint8_t *)jwe->jws.map.buf[LJWE_CTXT],
			     jwe->jws.map.len[LJWE_CTXT],
			     (uint8_t *)jwe->jws.map.buf[LJWE_CTXT],
			     (uint8_t *)jwe->jws.map.buf[LJWE_IV],
			     NULL, &ivs,
			     LWS_AESGCM_TAG);

	n |= lws_genaes_destroy(&aesctx, (uint8_t *)jwe->jws.map.buf[LJWE_ATAG],
				LWS_AESGCM_TAG);
	if (n) {
		lwsl_err("%s: lws_genaes_crypt failed\n", __func__);
		return -1;
	}

	return (int)jwe->jws.map.len[LJWE_CTXT];
}

int
lws_jwe_auth_and_decrypt_gcm(struct lws_jwe *jwe,
			     uint8_t *enc_cek, uint8_t *aad, int aad_len)
{
	struct lws_gencrypto_keyelem el;
	struct lws_genaes_ctx aesctx;
	size_t ivs = LWS_AESGCM_IV;
	uint8_t tag[LWS_AESGCM_TAG];
	int n;

	/* Some sanity checks on what came in */

	/* Tag MUST be 128-bit for all sizes */
	if (jwe->jws.map.len[LJWE_ATAG] != LWS_AESGCM_TAG) {
		lwsl_notice("%s: AESGCM tag size must be 128b, got %d\n",
				__func__, jwe->jws.map.len[LJWE_ATAG]);
		return -1;
	}

	if (jwe->jws.map.len[LJWE_IV] != LWS_AESGCM_IV) { /* MUST be 96-bit */
		lwsl_notice("%s: AESGCM IV must be 128b, got %d\n", __func__,
				jwe->jws.map.len[LJWE_IV]);
		return -1;
	}

	/* EKEY is directly the CEK KEY */
	el.buf = enc_cek;
	el.len = jwe->jose.enc_alg->keybits_fixed / 8;

	if (lws_genaes_create(&aesctx, LWS_GAESO_DEC, LWS_GAESM_GCM,
			      &el, LWS_GAESP_NO_PADDING, NULL)) {
		lwsl_err("%s: lws_genaes_create failed\n", __func__);

		return -1;
	}

	n = lws_genaes_crypt(&aesctx, aad, (unsigned int)aad_len,
			     NULL,
			     (uint8_t *)jwe->jws.map.buf[LJWE_IV],
			     (uint8_t *)jwe->jws.map.buf[LJWE_ATAG], &ivs, 16);
	if (n) {
		lwsl_err("%s: lws_genaes_crypt aad failed\n", __func__);
		return -1;
	}
	n = lws_genaes_crypt(&aesctx, (uint8_t *)jwe->jws.map.buf[LJWE_CTXT],
			     jwe->jws.map.len[LJWE_CTXT],
			     (uint8_t *)jwe->jws.map.buf[LJWE_CTXT],
			     (uint8_t *)jwe->jws.map.buf[LJWE_IV],
			     (uint8_t *)jwe->jws.map.buf[LJWE_ATAG], &ivs, 16);

	n |= lws_genaes_destroy(&aesctx, tag, sizeof(tag));
	if (n) {
		lwsl_err("%s: lws_genaes_crypt failed\n", __func__);
		return -1;
	}

	return (int)jwe->jws.map.len[LJWE_CTXT];
}
