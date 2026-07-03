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
 * From RFC7518 JWA
 *
 * 4.6.  Key Agreement with Elliptic Curve Diffie-Hellman Ephemeral Static
 *    (ECDH-ES)
 *
 * This section defines the specifics of key agreement with Elliptic
 * Curve Diffie-Hellman Ephemeral Static [RFC6090], in combination with
 * the Concat KDF, as defined in Section 5.8.1 of [NIST.800-56A].  The
 * key agreement result can be used in one of two ways:
 *
 * 1.  directly as the Content Encryption Key (CEK) for the "enc"
 *     algorithm, in the Direct Key Agreement mode, or
 *
 * 2.  as a symmetric key used to wrap the CEK with the "A128KW",
 *     "A192KW", or "A256KW" algorithms, in the Key Agreement with Key
 *     Wrapping mode.
 *
 * A new ephemeral public key value MUST be generated for each key
 * agreement operation.
 *
 * In Direct Key Agreement mode, the output of the Concat KDF MUST be a
 * key of the same length as that used by the "enc" algorithm.  In this
 * case, the empty octet sequence is used as the JWE Encrypted Key
 * value.  The "alg" (algorithm) Header Parameter value "ECDH-ES" is
 * used in the Direct Key Agreement mode.
 *
 * In Key Agreement with Key Wrapping mode, the output of the Concat KDF
 * MUST be a key of the length needed for the specified key wrapping
 * algorithm.  In this case, the JWE Encrypted Key is the CEK wrapped
 * with the agreed-upon key.
 *
 * The following "alg" (algorithm) Header Parameter values are used to
 * indicate that the JWE Encrypted Key is the result of encrypting the
 * CEK using the result of the key agreement algorithm as the key
 * encryption key for the corresponding key wrapping algorithm:
 *
 * +-----------------+-------------------------------------------------+
 * | "alg" Param     | Key Management Algorithm                        |
 * | Value           |                                                 |
 * +-----------------+-------------------------------------------------+
 * | ECDH-ES+A128KW  | ECDH-ES using Concat KDF and CEK wrapped with   |
 * |                 | "A128KW"                                        |
 * | ECDH-ES+A192KW  | ECDH-ES using Concat KDF and CEK wrapped with   |
 * |                 | "A192KW"                                        |
 * | ECDH-ES+A256KW  | ECDH-ES using Concat KDF and CEK wrapped with   |
 * |                 | "A256KW"                                        |
 * +-----------------+-------------------------------------------------+
 *
 * 4.6.1.  Header Parameters Used for ECDH Key Agreement
 *
 * The following Header Parameter names are used for key agreement as
 * defined below.
 *
 * 4.6.1.1.  "epk" (Ephemeral Public Key) Header Parameter
 *
 * The "epk" (ephemeral public key) value created by the originator for
 * the use in key agreement algorithms.  This key is represented as a
 * JSON Web Key [JWK] public key value.  It MUST contain only public key
 * parameters and SHOULD contain only the minimum JWK parameters
 * necessary to represent the key; other JWK parameters included can be
 * checked for consistency and honored, or they can be ignored.  This
 * Header Parameter MUST be present and MUST be understood and processed
 * by implementations when these algorithms are used.
 *
 * 4.6.1.2.  "apu" (Agreement PartyUInfo) Header Parameter
 *
 * The "apu" (agreement PartyUInfo) value for key agreement algorithms
 * using it (such as "ECDH-ES"), represented as a base64url-encoded
 * string.  When used, the PartyUInfo value contains information about
 * the producer.  Use of this Header Parameter is OPTIONAL.  This Header
 * Parameter MUST be understood and processed by implementations when
 * these algorithms are used.
 *
 * 4.6.1.3.  "apv" (Agreement PartyVInfo) Header Parameter
 *
 * The "apv" (agreement PartyVInfo) value for key agreement algorithms
 * using it (such as "ECDH-ES"), represented as a base64url encoded
 * string.  When used, the PartyVInfo value contains information about
 * the recipient.  Use of this Header Parameter is OPTIONAL.  This
 * Header Parameter MUST be understood and processed by implementations
 * when these algorithms are used.
 *
 * 4.6.2.  Key Derivation for ECDH Key Agreement
 *
 * The key derivation process derives the agreed-upon key from the
 * shared secret Z established through the ECDH algorithm, per
 * Section 6.2.2.2 of [NIST.800-56A].
 *
 * Key derivation is performed using the Concat KDF, as defined in
 * Section 5.8.1 of [NIST.800-56A], where the Digest Method is SHA-256.
 * The Concat KDF parameters are set as follows:
 *
 * Z
 *    This is set to the representation of the shared secret Z as an
 *    octet sequence.
 *
 * keydatalen
 *    This is set to the number of bits in the desired output key.  For
 *    "ECDH-ES", this is length of the key used by the "enc" algorithm.
 *    For "ECDH-ES+A128KW", "ECDH-ES+A192KW", and "ECDH-ES+A256KW", this
 *    is 128, 192, and 256, respectively.
 *
 * AlgorithmID
 *    The AlgorithmID value is of the form Datalen || Data, where Data
 *    is a variable-length string of zero or more octets, and Datalen is
 *    a fixed-length, big-endian 32-bit counter that indicates the
 *    length (in octets) of Data.  In the Direct Key Agreement case,
 *    Data is set to the octets of the ASCII representation of the "enc"
 *    Header Parameter value.  In the Key Agreement with Key Wrapping
 *    case, Data is set to the octets of the ASCII representation of the
 *    "alg" (algorithm) Header Parameter value.
 *
 * PartyUInfo
 *    The PartyUInfo value is of the form Datalen || Data, where Data is
 *    a variable-length string of zero or more octets, and Datalen is a
 *    fixed-length, big-endian 32-bit counter that indicates the length
 *    (in octets) of Data.  If an "apu" (agreement PartyUInfo) Header
 *    Parameter is present, Data is set to the result of base64url
 *    decoding the "apu" value and Datalen is set to the number of
 *    octets in Data.  Otherwise, Datalen is set to 0 and Data is set to
 *    the empty octet sequence.
 *
 * PartyVInfo
 *    The PartyVInfo value is of the form Datalen || Data, where Data is
 *    a variable-length string of zero or more octets, and Datalen is a
 *    fixed-length, big-endian 32-bit counter that indicates the length
 *    (in octets) of Data.  If an "apv" (agreement PartyVInfo) Header
 *    Parameter is present, Data is set to the result of base64url
 *    decoding the "apv" value and Datalen is set to the number of
 *    octets in Data.  Otherwise, Datalen is set to 0 and Data is set to
 *    the empty octet sequence.
 *
 * SuppPubInfo
 *    This is set to the keydatalen represented as a 32-bit big-endian
 *    integer.
 *
 * SuppPrivInfo
 *    This is set to the empty octet sequence.
 *
 * Applications need to specify how the "apu" and "apv" Header
 * Parameters are used for that application.  The "apu" and "apv" values
 * MUST be distinct, when used.  Applications wishing to conform to
 * [NIST.800-56A] need to provide values that meet the requirements of
 * that document, e.g., by using values that identify the producer and
 * consumer.  Alternatively, applications MAY conduct key derivation in
 * a manner similar to "Diffie-Hellman Key Agreement Method" [RFC2631]:
 * in that case, the "apu" parameter MAY either be omitted or represent
 * a random 512-bit value (analogous to PartyAInfo in Ephemeral-Static
 * mode in RFC 2631) and the "apv" parameter SHOULD NOT be present.
 *
 */


/*
 * - ECDH-ES[-variant] comes in the jose "alg" and just covers key agreement.
 *   The "enc" action is completely separate and handled elsewhere.  However
 *   the key size throughout is determined by the needs of the "enc" action.
 *
 * - The jwe->jws.jwk is the PEER - the encryption consumer's - public key.
 *
 * - The public part of the ephemeral key comes out in jose.jwk_ephemeral
 *
 * - Return shared secret length or < 0 for error
 *
 * - Unwrapped CEK in EKEY.  If any, wrapped CEK in "wrapped".
 *
 * - Caller responsibility to cleanse EKEY.
 */

static int
lws_jwe_encrypt_ecdh(struct lws_jwe *jwe, char *temp, int *temp_len,
		     uint8_t *cek)
{
	uint8_t shared_secret[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES],
		derived[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES];
	int m, n, ret = -1, ot = *temp_len, ss_len = sizeof(shared_secret),
	  //  kw_hlen = lws_genhash_size(jwe->jose.alg->hash_type),
	    enc_hlen = (int)lws_genhmac_size(jwe->jose.enc_alg->hmac_type),
	    ekbytes = 32; //jwe->jose.alg->keybits_fixed / 8;
	struct lws_genec_ctx ecctx;
	struct lws_jwk *ephem = &jwe->jose.recipient[jwe->recip].jwk_ephemeral;

	if (jwe->jws.jwk->kty != LWS_GENCRYPTO_KTY_EC) {
		lwsl_err("%s: unexpected kty %d\n", __func__, jwe->jws.jwk->kty);

		return -1;
	}

	ephem->kty = LWS_GENCRYPTO_KTY_EC;
	ephem->private_key = 1;

	/* Generate jose.jwk_ephemeral on the peer public key curve */

	if (lws_genecdh_create(&ecctx, jwe->jws.context, NULL))
		goto bail;

	/* ephemeral context gets random key on same curve as recip pubkey */
	if (lws_genecdh_new_keypair(&ecctx, LDHS_OURS, (const char *)
				jwe->jws.jwk->e[LWS_GENCRYPTO_EC_KEYEL_CRV].buf,
				ephem->e))
		goto bail;

	/* peer context gets js->jwk key */
	if (lws_genecdh_set_key(&ecctx, jwe->jws.jwk->e, LDHS_THEIRS)) {
		lwsl_err("%s: setting peer pubkey failed\n", __func__);
		goto bail;
	}

	/* combine our ephemeral key and the peer pubkey to get the secret */

	if (lws_genecdh_compute_shared_secret(&ecctx, shared_secret, &ss_len)) {
		lwsl_notice("%s: lws_genecdh_compute_shared_secret failed\n",
				__func__);

		goto bail;
	}

	/*
	 * The private part of the ephemeral key is finished with...
	 * cleanse and free it.  We need to keep the public part around so we
	 * can publish it with the JWE as "epk".
	 */

	lws_explicit_bzero(ephem->e[LWS_GENCRYPTO_EC_KEYEL_D].buf,
			   ephem->e[LWS_GENCRYPTO_EC_KEYEL_D].len);
	lws_free_set_NULL(ephem->e[LWS_GENCRYPTO_EC_KEYEL_D].buf);
	ephem->e[LWS_GENCRYPTO_EC_KEYEL_D].len = 0;
	ephem->private_key = 0;

	/*
	 * Derive the CEK from the shared secret... amount of bytes written to
	 * derived matches bitcount in jwe->jose.enc_alg->keybits_fixed
	 *
	 * In Direct Key Agreement mode, the output of the Concat KDF MUST be a
	 * key of the same length as that used by the "enc" algorithm.
	 */

	if (lws_jwa_concat_kdf(jwe,
			jwe->jose.alg->algtype_crypto == LWS_JOSE_ENCTYPE_NONE,
			derived, shared_secret, ss_len)) {
		lwsl_notice("%s: lws_jwa_concat_kdf failed\n", __func__);

		goto bail;
	}

	/* in P-521 case, we get a 66-byte shared secret for a 64-byte key */
	if (ss_len < enc_hlen) {
		lwsl_err("%s: concat KDF bad derived key len %d\n", __func__,
			 ss_len);
		goto bail;
	}

	/*
	 * For "ECDH-ES", that was it, and we use what we just wrapped in
	 * wrapped as the CEK without publishing it.
	 *
	 * For "ECDH-ES-AES[128,192,256]KW", we generate a new, random CEK and
	 * then wrap it using the key we just wrapped, and make the wrapped
	 * version available in EKEY.
	 */

	if (jwe->jose.alg->algtype_crypto != LWS_JOSE_ENCTYPE_NONE) {
		struct lws_gencrypto_keyelem el;
		struct lws_genaes_ctx aesctx;

		/* generate the actual CEK in cek */

		if (lws_get_random(jwe->jws.context, cek, (unsigned int)enc_hlen) !=
							(size_t)enc_hlen) {
			lwsl_err("Problem getting random\n");
			goto bail;
		}

		/* wrap with the derived key */

		el.buf = derived;
		el.len = (unsigned int)enc_hlen / 2;

		if (lws_genaes_create(&aesctx, LWS_GAESO_ENC, LWS_GAESM_KW, &el,
					1, NULL)) {

			lwsl_notice("%s: lws_genaes_create\n", __func__);
			goto bail;
		}

		/* wrap CEK into EKEY */

		n = lws_genaes_crypt(&aesctx, cek, (unsigned int)enc_hlen,
				     (void *)jwe->jws.map.buf[LJWE_EKEY],
				     NULL, NULL, NULL, 0);
		m = lws_genaes_destroy(&aesctx, NULL, 0);
		if (n < 0) {
			lwsl_err("%s: encrypt cek fail\n", __func__);
			goto bail;
		}
		if (m < 0) {
			lwsl_err("%s: lws_genaes_destroy fail\n", __func__);
			goto bail;
		}

		jwe->jws.map.len[LJWE_EKEY] = (unsigned int)enc_hlen + 8;

		/* Wrapped CEK is in EKEY. Random CEK is in cek. */

	} else /* direct derived CEK is in cek */
		memcpy(cek, derived, (unsigned int)enc_hlen);

	/* rewrite the protected JOSE header to have the epk pieces */

	jwe->jws.map.buf[LJWE_JOSE] = temp;

	m = n = lws_snprintf(temp, (size_t)*temp_len,
			     "{\"alg\":\"%s\", \"enc\":\"%s\", \"epk\":",
			     jwe->jose.alg->alg, jwe->jose.enc_alg->alg);
	*temp_len -= n;

	n = lws_jwk_export(ephem, 0, temp + (ot - *temp_len), temp_len);
	if (n < 0) {
		lwsl_err("%s: ephemeral export failed\n", __func__);
		goto bail;
	}
	m += n;

	n = lws_snprintf(temp + (ot - *temp_len), (size_t)*temp_len, "}");
	*temp_len -= n + 1;
	m += n;
	jwe->jws.map.len[LJWE_JOSE] = (unsigned int)m;

	/* create a b64 version of the JOSE header, needed later for AAD */

	if (lws_jws_encode_b64_element(&jwe->jws.map_b64, LJWE_JOSE,
				       temp + (ot - *temp_len), temp_len,
				       jwe->jws.map.buf[LJWE_JOSE],
				       jwe->jws.map.len[LJWE_JOSE]))
		return -1;

	ret = enc_hlen;

bail:
	lws_genec_destroy(&ecctx);

	/* cleanse the shared secret (watch out for cek at parent too) */
	lws_explicit_bzero(shared_secret, (unsigned int)ekbytes);
	lws_explicit_bzero(derived, (unsigned int)ekbytes);

	return ret;
}

int
lws_jwe_encrypt_ecdh_cbc_hs(struct lws_jwe *jwe, char *temp, int *temp_len)
{
	int ss_len, // kw_hlen = lws_genhash_size(jwe->jose.alg->hash_type),
	    enc_hlen = (int)lws_genhmac_size(jwe->jose.enc_alg->hmac_type);
	uint8_t cek[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES];
	int ekbytes = jwe->jose.alg->keybits_fixed / 8;
	int n, ot = *temp_len, ret = -1;

	/* if we will produce an EKEY, make space for it */

	if (jwe->jose.alg->algtype_crypto != LWS_JOSE_ENCTYPE_NONE) {
		if (lws_jws_alloc_element(&jwe->jws.map, LJWE_EKEY,
					  temp + (ot - *temp_len), temp_len,
					  (unsigned int)enc_hlen + 8, 0))
			goto bail;
	}

	/* decrypt the CEK */

	ss_len = lws_jwe_encrypt_ecdh(jwe, temp + (ot - *temp_len), temp_len, cek);
	if (ss_len < 0) {
		lwsl_err("%s: lws_jwe_encrypt_ecdh failed\n", __func__);
		return -1;
	}

	/* cek contains the unwrapped CEK.  EKEY may contain wrapped CEK */

	/* make space for the payload encryption pieces */

	if (lws_jws_alloc_element(&jwe->jws.map, LJWE_ATAG,
				  temp + (ot - *temp_len),
				  temp_len, (unsigned int)enc_hlen / 2, 0))
		goto bail;

	if (lws_jws_alloc_element(&jwe->jws.map, LJWE_IV,
				  temp + (ot - *temp_len),
				  temp_len, LWS_JWE_AES_IV_BYTES, 0))
		goto bail;

	/* Perform the authenticated encryption on CTXT...
	 * ...the AAD is b64u(protected JOSE header) */

	n = lws_jwe_encrypt_cbc_hs(jwe, cek,
				   (uint8_t *)jwe->jws.map_b64.buf[LJWE_JOSE],
				   (int)jwe->jws.map_b64.len[LJWE_JOSE]);
	if (n < 0) {
		lwsl_notice("%s: lws_jwe_encrypt_cbc_hs failed\n", __func__);
		goto bail;
	}

	ret = 0;

bail:
	/* if fail or direct CEK, cleanse and remove EKEY */
	if (ret || jwe->jose.enc_alg->algtype_crypto == LWS_JOSE_ENCTYPE_NONE) {
		if (jwe->jws.map.len[LJWE_EKEY])
			lws_explicit_bzero((void *)jwe->jws.map.buf[LJWE_EKEY],
					   jwe->jws.map.len[LJWE_EKEY]);
		jwe->jws.map.len[LJWE_EKEY] = 0;
	}

	lws_explicit_bzero(cek, (unsigned int)ekbytes);

	return ret;
}

/*
 * jwe->jws.jwk is recipient private key
 *
 * If kw mode, then EKEY is the wrapped CEK
 *
 *
 */

static int
lws_jwe_auth_and_decrypt_ecdh(struct lws_jwe *jwe)
{
	uint8_t shared_secret[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES],
		derived[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES];
	int ekbytes = jwe->jose.enc_alg->keybits_fixed / 8,
		      enc_hlen = (int)lws_genhmac_size(jwe->jose.enc_alg->hmac_type);
	struct lws_genec_ctx ecctx;
	int n, ret = -1, ss_len = sizeof(shared_secret);

	if (jwe->jws.jwk->kty != LWS_GENCRYPTO_KTY_EC) {
		lwsl_err("%s: unexpected kty %d\n", __func__, jwe->jws.jwk->kty);

		return -1;
	}

	if (jwe->jose.recipient[jwe->recip].jwk_ephemeral.kty !=
			LWS_GENCRYPTO_KTY_EC) {
		lwsl_err("%s: missing epk\n", __func__);

		return -1;
	}

	/*
	 * Recompute the shared secret...
	 *
	 * - direct:  it's the CEK
	 *
	 * - aeskw: apply it as AES keywrap to EKEY to get the CEK
	 */

	/* Generate jose.jwk_ephemeral on the peer public key curve */

	if (lws_genecdh_create(&ecctx, jwe->jws.context, NULL))
		goto bail;

	/* Load our private key into our side of the ecdh context */

	if (lws_genecdh_set_key(&ecctx, jwe->jws.jwk->e, LDHS_OURS)) {
		lwsl_err("%s: setting our private key failed\n", __func__);
		goto bail;
	}

	/* Import the ephemeral public key into the peer side */
	if (lws_genecdh_set_key(&ecctx,
			jwe->jose.recipient[jwe->recip].jwk_ephemeral.e,
			LDHS_THEIRS)) {
		lwsl_err("%s: setting epk pubkey failed\n", __func__);
		goto bail;
	}

	/* combine their ephemeral key and our private key to get the secret */

	if (lws_genecdh_compute_shared_secret(&ecctx, shared_secret, &ss_len)) {
		lwsl_notice("%s: lws_genecdh_compute_shared_secret failed\n",
				__func__);

		goto bail;
	}

	lws_genec_destroy(&ecctx);

	if (ss_len < enc_hlen) {
		lwsl_err("%s: ss_len %d ekbytes %d\n", __func__, ss_len, enc_hlen);
		goto bail;
	}

	/*
	 * Derive the CEK from the shared secret... amount of bytes written to
	 * cek[] matches bitcount in jwe->jose.enc_alg->keybits_fixed
	 */

	if (lws_jwa_concat_kdf(jwe,
			jwe->jose.alg->algtype_crypto == LWS_JOSE_ENCTYPE_NONE,
			derived, shared_secret, ss_len)) {
		lwsl_notice("%s: lws_jwa_concat_kdf failed\n", __func__);

		goto bail;
	}

	/*
	 * "ECDH-ES": derived is the CEK
	 * "ECDH-ES-AES[128,192,256]KW": wrapped key is in EKEY,
	 *				 "derived" contains KEK
	 */

	if (jwe->jose.alg->algtype_crypto != LWS_JOSE_ENCTYPE_NONE) {
		struct lws_gencrypto_keyelem el;
		struct lws_genaes_ctx aesctx;
		int m;

		/* Confirm space for EKEY */

		if (jwe->jws.map.len[LJWE_EKEY] < (unsigned int)enc_hlen) {
			lwsl_err("%s: missing EKEY\n", __func__);

			goto bail;
		}

		/* unwrap with the KEK we derived */

		el.buf = derived;
		el.len = (unsigned int)enc_hlen / 2;

		if (lws_genaes_create(&aesctx, LWS_GAESO_DEC, LWS_GAESM_KW,
				      &el, 1, NULL)) {

			lwsl_notice("%s: lws_genaes_create\n", __func__);
			goto bail;
		}

		/* decrypt the EKEY to end up with CEK in "shared_secret" */

		n = lws_genaes_crypt(&aesctx,
				     (const uint8_t *)jwe->jws.map.buf[LJWE_EKEY],
				     jwe->jws.map.len[LJWE_EKEY],
				     (uint8_t *)shared_secret,
				     NULL, NULL, NULL, 0);
		m = lws_genaes_destroy(&aesctx, NULL, 0);
		if (n < 0) {
			lwsl_err("%s: decrypt cek fail\n", __func__);
			goto bail;
		}
		if (m < 0) {
			lwsl_err("%s: lws_genaes_destroy fail\n", __func__);
			goto bail;
		}
	} else
		memcpy(shared_secret, derived, (unsigned int)enc_hlen);

	/* either way, the recovered CEK is in shared_secret */

	if (lws_jwe_auth_and_decrypt_cbc_hs(jwe, shared_secret,
			(uint8_t *)jwe->jws.map_b64.buf[LJWE_JOSE],
			(int)jwe->jws.map_b64.len[LJWE_JOSE]) < 0) {
		lwsl_err("%s: lws_jwe_auth_and_decrypt_cbc_hs fail\n", __func__);
		goto bail;
	}

	/* if all went well, then CTXT is now the plaintext */
	ret = 0;

bail:
	/* cleanse wrapped on stack that contained the CEK / wrapped key */
	lws_explicit_bzero(derived, (unsigned int)ekbytes);
	/* cleanse the shared secret */
	lws_explicit_bzero(shared_secret, (unsigned int)ekbytes);

	return ret;
}

int
lws_jwe_auth_and_decrypt_ecdh_cbc_hs(struct lws_jwe *jwe,
				     char *temp, int *temp_len)
{
	/* create a b64 version of the JOSE header, needed later for AAD */

	if (lws_jws_encode_b64_element(&jwe->jws.map_b64, LJWE_JOSE,
				       temp, temp_len,
				       jwe->jws.map.buf[LJWE_JOSE],
				       jwe->jws.map.len[LJWE_JOSE]))
		return -1;

	return lws_jwe_auth_and_decrypt_ecdh(jwe);
}
