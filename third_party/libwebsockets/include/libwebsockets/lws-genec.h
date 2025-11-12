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

enum enum_genec_alg {
	LEGENEC_UNKNOWN,

	LEGENEC_ECDH,
	LEGENEC_ECDSA
};

struct lws_genec_ctx {
#if defined(LWS_WITH_MBEDTLS)
	union {
		mbedtls_ecdh_context *ctx_ecdh;
		mbedtls_ecdsa_context *ctx_ecdsa;
	} u;
#else
	EVP_PKEY_CTX *ctx[2];
#endif
	struct lws_context *context;
	const struct lws_ec_curves *curve_table;
	enum enum_genec_alg genec_alg;

	char has_private;
};

#if defined(LWS_WITH_MBEDTLS)
enum enum_lws_dh_side {
	LDHS_OURS = MBEDTLS_ECDH_OURS,
	LDHS_THEIRS = MBEDTLS_ECDH_THEIRS
};
#else
enum enum_lws_dh_side {
	LDHS_OURS,
	LDHS_THEIRS
};
#endif

struct lws_ec_curves {
	const char *name;
	int tls_lib_nid;
	uint16_t key_bytes;
};


/* ECDH-specific apis */

/** lws_genecdh_create() - Create a genecdh
 *
 * \param ctx: your genec context
 * \param context: your lws_context (for RNG access)
 * \param curve_table: NULL, enabling P-256, P-384 and P-521, or a replacement
 *		       struct lws_ec_curves array, terminated by an entry with
 *		       .name = NULL, of curves you want to allow
 *
 * Initializes a genecdh
 */
LWS_VISIBLE int
lws_genecdh_create(struct lws_genec_ctx *ctx, struct lws_context *context,
		   const struct lws_ec_curves *curve_table);

/** lws_genecdh_set_key() - Apply an EC key to our or theirs side
 *
 * \param ctx: your genecdh context
 * \param el: your key elements
 * \param side: LDHS_OURS or LDHS_THEIRS
 *
 * Applies an EC key to one side or the other of an ECDH ctx
 */
LWS_VISIBLE LWS_EXTERN int
lws_genecdh_set_key(struct lws_genec_ctx *ctx, struct lws_gencrypto_keyelem *el,
		    enum enum_lws_dh_side side);

/** lws_genecdh_new_keypair() - Create a genec with a new public / private key
 *
 * \param ctx: your genec context
 * \param side: LDHS_OURS or LDHS_THEIRS
 * \param curve_name: an EC curve name, like "P-256"
 * \param el: array pf LWS_GENCRYPTO_EC_KEYEL_COUNT key elems to take the new key
 *
 * Creates a genecdh with a newly minted EC public / private key
 */
LWS_VISIBLE LWS_EXTERN int
lws_genecdh_new_keypair(struct lws_genec_ctx *ctx, enum enum_lws_dh_side side,
		        const char *curve_name, struct lws_gencrypto_keyelem *el);

LWS_VISIBLE LWS_EXTERN int
lws_genecdh_compute_shared_secret(struct lws_genec_ctx *ctx, uint8_t *ss,
		  int *ss_len);


/* ECDSA-specific apis */

/** lws_genecdsa_create() - Create a genecdsa and
 *
 * \param ctx: your genec context
 * \param context: your lws_context (for RNG access)
 * \param curve_table: NULL, enabling P-256, P-384 and P-521, or a replacement
 *		       struct lws_ec_curves array, terminated by an entry with
 *		       .name = NULL, of curves you want to allow
 *
 * Initializes a genecdh
 */
LWS_VISIBLE int
lws_genecdsa_create(struct lws_genec_ctx *ctx, struct lws_context *context,
		    const struct lws_ec_curves *curve_table);

/** lws_genecdsa_new_keypair() - Create a genecdsa with a new public / private key
 *
 * \param ctx: your genec context
 * \param curve_name: an EC curve name, like "P-256"
 * \param el: array pf LWS_GENCRYPTO_EC_KEYEL_COUNT key elements to take the new key
 *
 * Creates a genecdsa with a newly minted EC public / private key
 */
LWS_VISIBLE LWS_EXTERN int
lws_genecdsa_new_keypair(struct lws_genec_ctx *ctx, const char *curve_name,
			 struct lws_gencrypto_keyelem *el);

/** lws_genecdsa_set_key() - Apply an EC key to an ecdsa context
 *
 * \param ctx: your genecdsa context
 * \param el: your key elements
 *
 * Applies an EC key to an ecdsa context
 */
LWS_VISIBLE LWS_EXTERN int
lws_genecdsa_set_key(struct lws_genec_ctx *ctx,
		     const struct lws_gencrypto_keyelem *el);

/** lws_genecdsa_hash_sig_verify_jws() - Verifies a JWS ECDSA signature on a given hash
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: unencrypted payload (usually a recomputed hash)
 * \param hash_type: one of LWS_GENHASH_TYPE_
 * \param keybits: number of bits in the crypto key
 * \param sig: pointer to the signature we received with the payload
 * \param sig_len: length of the signature we are checking in bytes
 *
 * This just looks at the signed hash... that's why there's no input length
 * parameter, it's decided by the choice of hash.  It's up to you to confirm
 * separately the actual payload matches the hash that was confirmed by this to
 * be validly signed.
 *
 * Returns <0 for error, or 0 if signature matches the hash + key..
 *
 * The JWS ECDSA signature verification algorithm differs to generic ECDSA
 * signatures and they're not interoperable.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genecdsa_hash_sig_verify_jws(struct lws_genec_ctx *ctx, const uint8_t *in,
				 enum lws_genhash_types hash_type, int keybits,
				 const uint8_t *sig, size_t sig_len);

/** lws_genecdsa_hash_sign_jws() - Creates a JWS ECDSA signature for a hash you provide
 *
 * \param ctx: your struct lws_genrsa_ctx
 * \param in: precomputed hash
 * \param hash_type: one of LWS_GENHASH_TYPE_
 * \param keybits: number of bits in the crypto key
 * \param sig: pointer to buffer to take signature
 * \param sig_len: length of the buffer (must be >= length of key N)
 *
 * Returns <0 for error, or >=0 for success.
 *
 * This creates a JWS ECDSA signature for a hash you already computed and provide.
 *
 * The JWS ECDSA signature generation algorithm differs to generic ECDSA
 * signatures and they're not interoperable.
 *
 * This and related APIs operate identically with OpenSSL or mbedTLS backends.
 */
LWS_VISIBLE LWS_EXTERN int
lws_genecdsa_hash_sign_jws(struct lws_genec_ctx *ctx, const uint8_t *in,
			   enum lws_genhash_types hash_type, int keybits,
			   uint8_t *sig, size_t sig_len);


/* Apis that apply to both ECDH and ECDSA */

LWS_VISIBLE LWS_EXTERN void
lws_genec_destroy(struct lws_genec_ctx *ctx);

LWS_VISIBLE LWS_EXTERN void
lws_genec_destroy_elements(struct lws_gencrypto_keyelem *el);

LWS_VISIBLE LWS_EXTERN int
lws_genec_dump(struct lws_gencrypto_keyelem *el);
