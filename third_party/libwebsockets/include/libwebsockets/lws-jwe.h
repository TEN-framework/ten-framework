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
 * JWE Compact Serialization consists of
 *
 *     BASE64URL(UTF8(JWE Protected Header)) || '.' ||
 *     BASE64URL(JWE Encrypted Key)	     || '.' ||
 *     BASE64URL(JWE Initialization Vector)  || '.' ||
 *     BASE64URL(JWE Ciphertext)	     || '.' ||
 *     BASE64URL(JWE Authentication Tag)
 */

#define LWS_JWE_RFC3394_OVERHEAD_BYTES 8
#define LWS_JWE_AES_IV_BYTES 16

#define LWS_JWE_LIMIT_RSA_KEY_BITS 4096
#define LWS_JWE_LIMIT_AES_KEY_BITS (512 + 64) /* RFC3394 Key Wrap adds 64b */
#define LWS_JWE_LIMIT_EC_KEY_BITS  528 /* 521 rounded to byte boundary */
#define LWS_JWE_LIMIT_HASH_BITS    (LWS_GENHASH_LARGEST * 8)

/* the largest key element for any cipher */
#define LWS_JWE_LIMIT_KEY_ELEMENT_BYTES (LWS_JWE_LIMIT_RSA_KEY_BITS / 8)


struct lws_jwe {
	struct lws_jose jose;
	struct lws_jws jws;
	struct lws_jwk jwk;

	/*
	 * We have to keep a copy of the CEK so we can reuse it with later
	 * key encryptions for the multiple recipient case.
	 */
	uint8_t cek[LWS_JWE_LIMIT_KEY_ELEMENT_BYTES];
	unsigned int cek_valid:1;

	int recip;
};

LWS_VISIBLE LWS_EXTERN void
lws_jwe_init(struct lws_jwe *jwe, struct lws_context *context);

LWS_VISIBLE LWS_EXTERN void
lws_jwe_destroy(struct lws_jwe *jwe);

LWS_VISIBLE LWS_EXTERN void
lws_jwe_be64(uint64_t c, uint8_t *p8);

/*
 * JWE Compact Serialization consists of
 *
 *     BASE64URL(UTF8(JWE Protected Header)) || '.' ||
 *     BASE64URL(JWE Encrypted Key)	     || '.' ||
 *     BASE64URL(JWE Initialization Vector)  || '.' ||
 *     BASE64URL(JWE Ciphertext)	     || '.' ||
 *     BASE64URL(JWE Authentication Tag)
 */

LWS_VISIBLE LWS_EXTERN int
lws_jwe_render_compact(struct lws_jwe *jwe, char *out, size_t out_len);

LWS_VISIBLE int
lws_jwe_render_flattened(struct lws_jwe *jwe, char *out, size_t out_len);

LWS_VISIBLE LWS_EXTERN int
lws_jwe_json_parse(struct lws_jwe *jwe, const uint8_t *buf, int len,
		   char *temp, int *temp_len);

/**
 * lws_jwe_auth_and_decrypt() - confirm and decrypt JWE
 *
 * \param jose: jose context
 * \param jws: jws / jwe context... .map and .map_b64 must be filled already
 *
 * This is a high level JWE decrypt api that takes a jws with the maps
 * already processed, and if the authentication passes, returns the decrypted
 * plaintext in jws.map.buf[LJWE_CTXT] and its length in jws.map.len[LJWE_CTXT].
 *
 * In the jws, the following fields must have been set by the caller
 *
 * .context
 * .jwk (the key encryption key)
 * .map
 * .map_b64
 *
 * Having the b64 and decoded maps filled externally makes it flexible where
 * the data was picked from, eg, from a Complete JWE JSON serialization, a
 * flattened one, or a Compact Serialization.
 *
 * Returns decrypt length, or -1 for failure.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwe_auth_and_decrypt(struct lws_jwe *jwe, char *temp, int *temp_len);

/**
 * lws_jwe_encrypt() - perform JWE encryption
 *
 * \param jose: the JOSE header information (encryption types, etc)
 * \param jws: the JWE elements, pointer to jwk etc
 * \param temp: parent-owned buffer to "allocate" elements into
 * \param temp_len: amount of space available in temp
 *
 * May be called up to LWS_JWS_MAX_RECIPIENTS times to encrypt the same CEK
 * multiple ways on the same JWE payload.
 *
 * returns the amount of temp used, or -1 for error.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwe_encrypt(struct lws_jwe *jwe, char *temp, int *temp_len);

/**
 * lws_jwe_create_packet() - add b64 sig to b64 hdr + payload
 *
 * \param jwe: the struct lws_jwe we are trying to render
 * \param payload: unencoded payload JSON
 * \param len: length of unencoded payload JSON
 * \param nonce: Nonse string to include in protected header
 * \param out: buffer to take signed packet
 * \param out_len: size of \p out buffer
 * \param conext: lws_context to get random from
 *
 * This creates a "flattened" JWS packet from the jwk and the plaintext
 * payload, and signs it.  The packet is written into \p out.
 *
 * This does the whole packet assembly and signing, calling through to
 * lws_jws_sign_from_b64() as part of the process.
 *
 * Returns the length written to \p out, or -1.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwe_create_packet(struct lws_jwe *jwe,
		      const char *payload, size_t len, const char *nonce,
		      char *out, size_t out_len, struct lws_context *context);


/* only exposed because we have test vectors that need it */
LWS_VISIBLE LWS_EXTERN int
lws_jwe_auth_and_decrypt_cbc_hs(struct lws_jwe *jwe, uint8_t *enc_cek,
					uint8_t *aad, int aad_len);

/* only exposed because we have test vectors that need it */
LWS_VISIBLE LWS_EXTERN int
lws_jwa_concat_kdf(struct lws_jwe *jwe, int direct,
		   uint8_t *out, const uint8_t *shared_secret, int sslen);
