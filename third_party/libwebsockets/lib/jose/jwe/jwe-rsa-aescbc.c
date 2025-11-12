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
 * Requirements on entry:
 *
 *  - jwe->jws.map LJWE_JOSE contains the ASCII JOSE header
 *  - jwe->jws.map LJWE_EKEY contains cek of enc_alg hmac length
 *  - jwe->jws.map LJWE_CTXT contains the plaintext
 *
 * On successful exit:
 *
 *  - jwe->jws.map LJWE_ATAG contains the tag
 *  - jwe->jws.map LJWE_IV contains the new random IV that was used
 *  - jwe->jws.map LJWE_EKEY contains the encrypted CEK
 *  - jwe->jws.map LJWE_CTXT contains the ciphertext
 *
 *  Return the amount of temp used, or -1
 */

int
lws_jwe_encrypt_rsa_aes_cbc_hs(struct lws_jwe *jwe,
			       char *temp, int *temp_len)
{
	int n, hlen = (int)lws_genhmac_size(jwe->jose.enc_alg->hmac_type),
	    ot = *temp_len;
	char ekey[LWS_GENHASH_LARGEST];
	struct lws_genrsa_ctx rsactx;

	if (jwe->jws.jwk->kty != LWS_GENCRYPTO_KTY_RSA) {
		lwsl_err("%s: unexpected kty %d\n", __func__, jwe->jws.jwk->kty);

		return -1;
	}

	/*
	 * Notice that the unencrypted EKEY coming in is smaller than the
	 * RSA-encrypted EKEY going out, which is going to be the RSA key size
	 *
	 * Create a b64 version of the JOSE header, needed as aad
	 */
	if (lws_jws_encode_b64_element(&jwe->jws.map_b64, LJWE_JOSE,
				       temp, temp_len,
				       jwe->jws.map.buf[LJWE_JOSE],
				       jwe->jws.map.len[LJWE_JOSE]))
		return -1;

	if (lws_jws_alloc_element(&jwe->jws.map, LJWE_ATAG, temp + (ot - *temp_len),
				  temp_len, (unsigned int)hlen / 2, 0))
		return -1;

	if (lws_jws_alloc_element(&jwe->jws.map, LJWE_IV, temp + (ot - *temp_len),
				  temp_len, LWS_JWE_AES_IV_BYTES, 0))
		return -1;

	/*
	 * Without changing the unencrypted CEK in EKEY, reallocate enough
	 * space to write the RSA-encrypted version in-situ.
	 */
	if (lws_jws_dup_element(&jwe->jws.map, LJWE_EKEY, temp + (ot - *temp_len),
				temp_len, jwe->jws.map.buf[LJWE_EKEY],
				jwe->jws.map.len[LJWE_EKEY],
				jwe->jws.jwk->e[LWS_GENCRYPTO_RSA_KEYEL_N].len))
		return -1;

	/* Encrypt using the raw CEK (treated as MAC KEY | ENC KEY) */

	n = lws_jwe_encrypt_cbc_hs(jwe, (uint8_t *)jwe->jws.map.buf[LJWE_EKEY],
				     (uint8_t *)jwe->jws.map_b64.buf[LJWE_JOSE],
				     (int)jwe->jws.map_b64.len[LJWE_JOSE]);
	if (n < 0) {
		lwsl_err("%s: lws_jwe_encrypt_cbc_hs failed\n", __func__);
		return -1;
	}

	if (lws_genrsa_create(&rsactx, jwe->jws.jwk->e, jwe->jws.context,
			!strcmp(jwe->jose.alg->alg,   "RSA-OAEP") ?
					LGRSAM_PKCS1_OAEP_PSS : LGRSAM_PKCS1_1_5,
					LWS_GENHASH_TYPE_UNKNOWN)) {
		lwsl_notice("%s: lws_genrsa_create\n",
			    __func__);
		return -1;
	}

	/* encrypt the CEK using RSA, mbedtls can't handle both in and out are
	 * the EKEY, so copy the unencrypted ekey out temporarily */

	memcpy(ekey, jwe->jws.map.buf[LJWE_EKEY], (unsigned int)hlen);

	n = lws_genrsa_public_encrypt(&rsactx, (uint8_t *)ekey, (unsigned int)hlen,
				      (uint8_t *)jwe->jws.map.buf[LJWE_EKEY]);
	lws_genrsa_destroy(&rsactx);
	lws_explicit_bzero(ekey, (unsigned int)hlen); /* cleanse the temp CEK copy */
	if (n < 0) {
		lwsl_err("%s: encrypt cek fail\n", __func__);
		return -1;
	}
	jwe->jws.map.len[LJWE_EKEY] = (unsigned int)n; /* update to encrypted EKEY size */

	/*
	 * We end up with IV, ATAG, set, EKEY encrypted and CTXT is ciphertext,
	 * and b64u version of ATAG in map_b64.
	 */

	return 0;
}

int
lws_jwe_auth_and_decrypt_rsa_aes_cbc_hs(struct lws_jwe *jwe)
{
	int n;
	struct lws_genrsa_ctx rsactx;
	uint8_t enc_cek[512];

	if (jwe->jws.jwk->kty != LWS_GENCRYPTO_KTY_RSA) {
		lwsl_err("%s: unexpected kty %d\n", __func__, jwe->jws.jwk->kty);

		return -1;
	}

	if (jwe->jws.map.len[LJWE_EKEY] < 40) {
		lwsl_err("%s: EKEY length too short %d\n", __func__,
				jwe->jws.map.len[LJWE_EKEY]);

		return -1;
	}

	/* Decrypt the JWE Encrypted Key to get the raw MAC || CEK */

	if (lws_genrsa_create(&rsactx, jwe->jws.jwk->e, jwe->jws.context,
			!strcmp(jwe->jose.alg->alg,   "RSA-OAEP") ?
				LGRSAM_PKCS1_OAEP_PSS : LGRSAM_PKCS1_1_5,
				LWS_GENHASH_TYPE_UNKNOWN)) {
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

	n = lws_jwe_auth_and_decrypt_cbc_hs(jwe, enc_cek,
			     (uint8_t *)jwe->jws.map_b64.buf[LJWE_JOSE],
			     (int)jwe->jws.map_b64.len[LJWE_JOSE]);
	if (n < 0) {
		lwsl_err("%s: lws_jwe_auth_and_decrypt_cbc_hs failed\n",
			 __func__);
		return -1;
	}

#if defined(LWS_WITH_MBEDTLS) && defined(LWS_PLAT_OPTEE)

	/* strip padding */

	n = jwe->jws.map.buf[LJWE_CTXT][jwe->jws.map.len[LJWE_CTXT] - 1];
	if (n > 16) {
		lwsl_err("%s: n == %d, plen %d\n", __func__, n,
				(int)jwe->jws.map.len[LJWE_CTXT]);
		return -1;
	}
	jwe->jws.map.len[LJWE_CTXT] -= n;
#endif

	return (int)jwe->jws.map.len[LJWE_CTXT];
}
