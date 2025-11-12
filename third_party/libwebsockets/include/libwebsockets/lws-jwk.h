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

/*! \defgroup jwk JSON Web Keys
 * ## JSON Web Keys API
 *
 * Lws provides an API to parse JSON Web Keys into a struct lws_gencrypto_keyelem.
 *
 * "oct" and "RSA" type keys are supported.  For "oct" keys, they are held in
 * the "e" member of the struct lws_gencrypto_keyelem.
 *
 * Keys elements are allocated on the heap.  You must destroy the allocations
 * in the struct lws_gencrypto_keyelem by calling
 * lws_genrsa_destroy_elements() when you are finished with it.
 */
///@{

enum enum_jwk_meta_tok {
	JWK_META_KTY,
	JWK_META_KID,
	JWK_META_USE,
	JWK_META_KEY_OPS,
	JWK_META_X5C,
	JWK_META_ALG,

	LWS_COUNT_JWK_ELEMENTS
};

struct lws_jwk {
	/* key data elements */
	struct lws_gencrypto_keyelem e[LWS_GENCRYPTO_MAX_KEYEL_COUNT];
	/* generic meta key elements, like KID */
	struct lws_gencrypto_keyelem meta[LWS_COUNT_JWK_ELEMENTS];
	int kty;			/**< one of LWS_GENCRYPTO_KTY_ */
	char private_key; /* nonzero = has private key elements */
};

typedef int (*lws_jwk_key_import_callback)(struct lws_jwk *s, void *user);

struct lws_jwk_parse_state {
	struct lws_jwk *jwk;
	char b64[(((8192 / 8) * 4) / 3) + 1]; /* enough for 8Kb key */
	lws_jwk_key_import_callback per_key_cb;
	void *user;
	int pos;
	int cose_state;
	int seen;
	unsigned short possible;
};

/** lws_jwk_import() - Create a JSON Web key from the textual representation
 *
 * \param jwk: the JWK object to create
 * \param cb: callback for each jwk-processed key, or NULL if importing a single
 *	      key with no parent "keys" JSON
 * \param user: pointer to be passed to the callback, otherwise ignored by lws.
 *		NULL if importing a single key with no parent "keys" JSON
 * \param in: a single JWK JSON stanza in utf-8
 * \param len: the length of the JWK JSON stanza in bytes
 *
 * Creates an lws_jwk struct filled with data from the JSON representation.
 *
 * There are two ways to use this... with some protocols a single jwk is
 * delivered with no parent "keys": [] array.  If you call this with cb and
 * user as NULL, then the input will be interpreted like that and the results
 * placed in s.
 *
 * The second case is that you are dealing with a "keys":[] array with one or
 * more keys in it.  In this case, the function iterates through the keys using
 * s as a temporary jwk, and calls the user-provided callback for each key in
 * turn while it return 0 (nonzero return from the callback terminates the
 * iteration through any further keys).
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_import(struct lws_jwk *jwk, lws_jwk_key_import_callback cb, void *user,
	       const char *in, size_t len);

/** lws_jwk_destroy() - Destroy a JSON Web key
 *
 * \param jwk: the JWK object to destroy
 *
 * All allocations in the lws_jwk are destroyed
 */
LWS_VISIBLE LWS_EXTERN void
lws_jwk_destroy(struct lws_jwk *jwk);

/** lws_jwk_dup_oct() - Set a jwk to a dup'd binary OCT key
 *
 * \param jwk: the JWK object to set
 * \param key: the JWK object to destroy
 * \param len: the JWK object to destroy
 *
 * Sets the kty to OCT, allocates len bytes for K and copies len bytes of key
 * into the allocation.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_dup_oct(struct lws_jwk *jwk, const void *key, int len);

#define LWSJWKF_EXPORT_PRIVATE				(1 << 0)
#define LWSJWKF_EXPORT_NOCRLF				(1 << 1)

/** lws_jwk_export() - Export a JSON Web key to a textual representation
 *
 * \param jwk: the JWK object to export
 * \param flags: control export options
 * \param p: the buffer to write the exported JWK to
 * \param len: the length of the buffer \p p in bytes... reduced by used amount
 *
 * Returns length of the used part of the buffer if OK, or -1 for error.
 *
 * \p flags can be OR-ed together
 *
 * LWSJWKF_EXPORT_PRIVATE: default is only public part, set this to also export
 *			   the private part
 *
 * LWSJWKF_EXPORT_NOCRLF: normally adds a CRLF at the end of the export, if
 *			  you need to suppress it, set this flag
 *
 * Serializes the content of the JWK into a char buffer.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_export(struct lws_jwk *jwk, int flags, char *p, int *len);

/** lws_jwk_load() - Import a JSON Web key from a file
 *
 * \param jwk: the JWK object to load into
 * \param filename: filename to load from
 * \param cb: optional callback for each key
 * \param user: opaque user pointer passed to cb if given
 *
 * Returns 0 for OK or -1 for failure
 *
 * There are two ways to use this... with some protocols a single jwk is
 * delivered with no parent "keys": [] array.  If you call this with cb and
 * user as NULL, then the input will be interpreted like that and the results
 * placed in s.
 *
 * The second case is that you are dealing with a "keys":[] array with one or
 * more keys in it.  In this case, the function iterates through the keys using
 * s as a temporary jwk, and calls the user-provided callback for each key in
 * turn while it return 0 (nonzero return from the callback terminates the
 * iteration through any further keys, leaving the last one in s).
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_load(struct lws_jwk *jwk, const char *filename,
	     lws_jwk_key_import_callback cb, void *user);

/** lws_jwk_save() - Export a JSON Web key to a file
 *
 * \param jwk: the JWK object to save from
 * \param filename: filename to save to
 *
 * Returns 0 for OK or -1 for failure
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_save(struct lws_jwk *jwk, const char *filename);

/** lws_jwk_rfc7638_fingerprint() - jwk to RFC7638 compliant fingerprint
 *
 * \param jwk: the JWK object to fingerprint
 * \param digest32: buffer to take 32-byte digest
 *
 * Returns 0 for OK or -1 for failure
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_rfc7638_fingerprint(struct lws_jwk *jwk, char *digest32);

/** lws_jwk_strdup_meta() - allocate a duplicated string meta element
 *
 * \param jwk: the JWK object to fingerprint
 * \param idx: JWK_META_ element index
 * \param in: string to copy
 * \param len: length of string to copy
 *
 * Returns 0 for OK or nonzero for failure
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwk_strdup_meta(struct lws_jwk *jwk, enum enum_jwk_meta_tok idx,
		    const char *in, int len);


LWS_VISIBLE LWS_EXTERN int
lws_jwk_dump(struct lws_jwk *jwk);

/** lws_jwk_generate() - create a new key of given type and characteristics
 *
 * \param context: the struct lws_context used for RNG
 * \param jwk: the JWK object to fingerprint
 * \param kty: One of the LWS_GENCRYPTO_KTY_ key types
 * \param bits: for OCT and RSA keys, the number of bits
 * \param curve: for EC keys, the name of the curve
 *
 * Returns 0 for OK or nonzero for failure
 */
LWS_VISIBLE int
lws_jwk_generate(struct lws_context *context, struct lws_jwk *jwk,
	         enum lws_gencrypto_kty kty, int bits, const char *curve);

///@}
