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

/*! \defgroup genericRSA Generic RSA
 * ## Generic RSA related functions
 *
 * Lws provides generic RSA functions that abstract the ones
 * provided by whatever OpenSSL library you are linking against.
 *
 * It lets you use the same code if you build against mbedtls or OpenSSL
 * for example.
 */
///@{

/* include/libwebsockets/lws-jwk.h must be included before this */

enum enum_genrsa_mode {
	LGRSAM_PKCS1_1_5,
	LGRSAM_PKCS1_OAEP_PSS,

	LGRSAM_COUNT
};

struct lws_genrsa_ctx {
#if defined(LWS_WITH_MBEDTLS)
	mbedtls_rsa_context *ctx;
#else
	BIGNUM *bn[LWS_GENCRYPTO_RSA_KEYEL_COUNT];
	EVP_PKEY_CTX *ctx;
	RSA *rsa;
#endif
	struct lws_context *context;
	enum enum_genrsa_mode mode;
};

/** lws_genrsa_public_decrypt_create() - Create RSA public decrypt context
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param el: struct prepared with key element data
 * \param context: lws_context for RNG
 * \param mode: RSA mode, one of LGRSAM_ constants
 * \param oaep_hashid: the lws genhash id for the hash used in MFG1 hash
 *			used in OAEP mode - normally, SHA1
 *
 * Creates an RSA context with a public key associated with it, formed from
 * the key elements in \p el.
 *
 * Mode LGRSAM_PKCS1_1_5 is in widespread use but has weaknesses.  It's
 * recommended to use LGRSAM_PKCS1_OAEP_PSS for new implementations.
 *
 * Returns 0 for OK or nonzero for error.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_create(struct lws_genrsa_ctx *ctx,
		  const struct lws_gencrypto_keyelem *el,
		  struct lws_context *context, enum enum_genrsa_mode mode,
		  enum lws_genhash_types oaep_hashid);

/** lws_genrsa_destroy_elements() - Free allocations in genrsa_elements
 *
 * \param el: your struct lws_gencrypto_keyelem
 *
 * This is a helper for user code making use of struct lws_gencrypto_keyelem
 * where the elements are allocated on the heap, it frees any non-NULL
 * buf element and sets the buf to NULL.
 *
 * NB: lws_genrsa_public_... apis do not need this as they take care of the key
 * creation and destruction themselves.
 */
LWS_VISIBLE LWS_EXTERN void
lws_genrsa_destroy_elements(struct lws_gencrypto_keyelem *el);

/** lws_genrsa_new_keypair() - Create new RSA keypair
 *
 * \param context: your struct lws_context (may be used for RNG)
 * \param ctx: your struct lws_genrsa_ctx
 * \param mode: RSA mode, one of LGRSAM_ constants
 * \param el: struct to get the new key element data allocated into it
 * \param bits: key size, eg, 4096
 *
 * Creates a new RSA context and generates a new keypair into it, with \p bits
 * bits.
 *
 * Returns 0 for OK or nonzero for error.
 *
 * Mode LGRSAM_PKCS1_1_5 is in widespread use but has weaknesses.  It's
 * recommended to use LGRSAM_PKCS1_OAEP_PSS for new implementations.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_new_keypair(struct lws_context *context, struct lws_genrsa_ctx *ctx,
		       enum enum_genrsa_mode mode, struct lws_gencrypto_keyelem *el,
		       int bits);

/** lws_genrsa_public_encrypt() - Perform RSA public key encryption
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: plaintext input
 * \param in_len: length of plaintext input
 * \param out: encrypted output
 *
 * Performs PKCS1 v1.5 Encryption
 *
 * Returns <0 for error, or length of decrypted data.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_public_encrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			  size_t in_len, uint8_t *out);

/** lws_genrsa_private_encrypt() - Perform RSA private key encryption
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: plaintext input
 * \param in_len: length of plaintext input
 * \param out: encrypted output
 *
 * Performs PKCS1 v1.5 Encryption
 *
 * Returns <0 for error, or length of decrypted data.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_private_encrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   size_t in_len, uint8_t *out);

/** lws_genrsa_public_decrypt() - Perform RSA public key decryption
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: encrypted input
 * \param in_len: length of encrypted input
 * \param out: decrypted output
 * \param out_max: size of output buffer
 *
 * Performs PKCS1 v1.5 Decryption
 *
 * Returns <0 for error, or length of decrypted data.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_public_decrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			  size_t in_len, uint8_t *out, size_t out_max);

/** lws_genrsa_private_decrypt() - Perform RSA private key decryption
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: encrypted input
 * \param in_len: length of encrypted input
 * \param out: decrypted output
 * \param out_max: size of output buffer
 *
 * Performs PKCS1 v1.5 Decryption
 *
 * Returns <0 for error, or length of decrypted data.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_private_decrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   size_t in_len, uint8_t *out, size_t out_max);

/** lws_genrsa_hash_sig_verify() - Verifies RSA signature on a given hash
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: input to be hashed
 * \param hash_type: one of LWS_GENHASH_TYPE_
 * \param sig: pointer to the signature we received with the payload
 * \param sig_len: length of the signature we are checking in bytes
 *
 * Returns <0 for error, or 0 if signature matches the payload + key.
 *
 * This just looks at a hash... that's why there's no input length
 * parameter, it's decided by the choice of hash.   It's up to you to confirm
 * separately the actual payload matches the hash that was confirmed by this to
 * be validly signed.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_hash_sig_verify(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   enum lws_genhash_types hash_type,
			   const uint8_t *sig, size_t sig_len);

/** lws_genrsa_hash_sign() - Creates an ECDSA signature for a hash you provide
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: input to be hashed and signed
 * \param hash_type: one of LWS_GENHASH_TYPE_
 * \param sig: pointer to buffer to take signature
 * \param sig_len: length of the buffer (must be >= length of key N)
 *
 * Returns <0 for error, or \p sig_len for success.
 *
 * This creates an RSA signature for a hash you already computed and provide.
 * You should have created the hash before calling this by iterating over the
 * actual payload you need to confirm.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_hash_sign(struct lws_genrsa_ctx *ctx, const uint8_t *in,
		     enum lws_genhash_types hash_type,
		     uint8_t *sig, size_t sig_len);

/** lws_genrsa_public_decrypt_destroy() - Destroy RSA public decrypt context
 *
 * \param ctx: your struct lws_genrsa_ctx
 *
 * Destroys any allocations related to \p ctx.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN void
lws_genrsa_destroy(struct lws_genrsa_ctx *ctx);

/** lws_genrsa_render_pkey_asn1() - Exports public or private key to ASN1/DER
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param _private: 0 = public part only, 1 = all parts of the key
 * \param pkey_asn1: pointer to buffer to take the ASN1
 * \param pkey_asn1_len: max size of the pkey_asn1_len
 *
 * Returns length of pkey_asn1 written, or -1 for error.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genrsa_render_pkey_asn1(struct lws_genrsa_ctx *ctx, int _private,
			    uint8_t *pkey_asn1, size_t pkey_asn1_len);
///@}
