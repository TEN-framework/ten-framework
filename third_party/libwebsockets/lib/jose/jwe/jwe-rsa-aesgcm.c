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

#define LWS_AESGCM_IV 12


int
lws_jwe_encrypt_rsa_aes_gcm(struct lws_jwe *jwe, char *temp, int *temp_len)
{
	int ekbytes = jwe->jose.enc_alg->keybits_fixed / 8;
	struct lws_genrsa_ctx rsactx;
	int n, ret = -1, ot = *temp_len;

	if (jwe->jws.jwk->kty != LWS_GENCRYPTO_KTY_RSA) {
		lwsl_err("%s: wrong kty %d\n", __func__, jwe->jws.jwk->kty);

		return -1;
	}

	/* create the IV + CEK */

	if (lws_jws_randomize_element(jwe->jws.context, &jwe->jws.map, LJWE_IV,
				      temp, temp_len,
				      LWS_AESGCM_IV, 0))
		return -1;

	if (lws_jws_alloc_element(&jwe->jws.map, LJWE_ATAG,
				  temp + (ot - *temp_len),
				  temp_len, LWS_AESGCM_TAG, 0))
		return -1;

	/* create a b64 version of the JOSE header, needed as aad */

	if (lws_jws_encode_b64_element(&jwe->jws.map_b64, LJWE_JOSE,
				       temp + (ot - *temp_len), temp_len,
				       jwe->jws.map.buf[LJWE_JOSE],
				       jwe->jws.map.len[LJWE_JOSE]))
		return -1;

	/*
	 * If none already, create a new, random CEK in the JWE (so it can be
	 * reused for other recipients on same payload).  If it already exists,
	 * just reuse it.  It will be cleansed in the JWE destroy.
	 */
	if (!jwe->cek_valid) {
		if (lws_get_random(jwe->jws.context, jwe->cek, (unsigned int)ekbytes) !=
							      (size_t)ekbytes) {
			lwsl_err("%s: Problem getting random\n", __func__);
			return -1;
		}
		jwe->cek_valid = 1;
	}

	if (lws_jws_dup_element(&jwe->jws.map, LJWE_EKEY,
			        temp + (ot - *temp_len), temp_len,
			        jwe->cek, (unsigned int)ekbytes, 0))
		return -1;

	/* encrypt the payload */

	n = lws_jwe_encrypt_gcm(jwe, (uint8_t *)jwe->jws.map.buf[LJWE_EKEY],
				(uint8_t *)jwe->jws.map_b64.buf[LJWE_JOSE],
				(int)jwe->jws.map_b64.len[LJWE_JOSE]);
	if (n < 0) {
		lwsl_err("%s: lws_jwe_encrypt_gcm failed\n",
			 __func__);
		goto bail;
	}

	/* Encrypt the CEK into EKEY to make the JWE Encrypted Key */

	if (lws_genrsa_create(&rsactx, jwe->jws.jwk->e, jwe->jws.context,
			!strcmp(jwe->jose.alg->alg,   "RSA-OAEP") ?
				LGRSAM_PKCS1_OAEP_PSS : LGRSAM_PKCS1_1_5,
			LWS_GENHASH_TYPE_SHA1 /* !!! */)) {
		lwsl_notice("%s: lws_genrsa_public_decrypt_create\n",
			    __func__);
		goto bail;
	}

	n = lws_genrsa_public_encrypt(&rsactx, jwe->cek, (unsigned int)ekbytes,
				      (uint8_t *)jwe->jws.map.buf[LJWE_EKEY]);
	lws_genrsa_destroy(&rsactx);
	if (n < 0) {
		lwsl_err("%s: encrypt cek fail: \n", __func__);
		goto bail;
	}

	/* set the EKEY length to the actual enciphered length */
	jwe->jws.map.len[LJWE_EKEY] = (unsigned int)n;

	ret = (int32_t)jwe->jws.map.len[LJWE_CTXT];

bail:

	return ret;
}

int
lws_jwe_auth_and_decrypt_rsa_aes_gcm(struct lws_jwe *jwe)
{
	int n;
	struct lws_genrsa_ctx rsactx;
	uint8_t enc_cek[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES];

	if (jwe->jws.jwk->kty != LWS_GENCRYPTO_KTY_RSA) {
		lwsl_err("%s: unexpected kty %d\n", __func__, jwe->jws.jwk->kty);

		return -1;
	}

	if (jwe->jws.map.len[LJWE_EKEY] < 32) {
		lwsl_err("%s: EKEY length too short %d\n", __func__,
				jwe->jws.map.len[LJWE_EKEY]);

		return -1;
	}

	/* Decrypt the JWE Encrypted Key to get the direct CEK */

	if (lws_genrsa_create(&rsactx, jwe->jws.jwk->e, jwe->jws.context,
			!strcmp(jwe->jose.alg->alg,   "RSA-OAEP") ?
				LGRSAM_PKCS1_OAEP_PSS : LGRSAM_PKCS1_1_5,
			LWS_GENHASH_TYPE_SHA1 /* !!! */)) {
		lwsl_notice("%s: lws_genrsa_public_decrypt_create\n",
			    __func__);
		return -1;
	}

	n = lws_genrsa_private_decrypt(&rsactx,
				       (uint8_t *)jwe->jws.map.buf[LJWE_EKEY],
				       jwe->jws.map.len[LJWE_EKEY], enc_cek,
				       sizeof(enc_cek));
	lws_genrsa_destroy(&rsactx);
	if (n < 0) {
		lwsl_err("%s: decrypt cek fail: \n", __func__);
		return -1;
	}

	n = lws_jwe_auth_and_decrypt_gcm(jwe, enc_cek,
			(uint8_t *)jwe->jws.map_b64.buf[LJWE_JOSE],
				(int)jwe->jws.map_b64.len[LJWE_JOSE]);
	if (n < 0) {
		lwsl_err("%s: lws_jwe_auth_and_decrypt_gcm_hs failed\n",
			 __func__);
		return -1;
	}

#if defined(LWS_WITH_MBEDTLS) && defined(LWS_PLAT_OPTEE)
	/* strip padding */

	n = jwe->jws.map.buf[LJWE_CTXT][jwe->jws.map.len[LJWE_CTXT] - 1];
	if (n > 16)
		return -1;
	jwe->jws.map.len[LJWE_CTXT] -= n;
#endif

	return (int)jwe->jws.map.len[LJWE_CTXT];
}
