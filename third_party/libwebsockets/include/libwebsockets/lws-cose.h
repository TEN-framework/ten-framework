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

/** \defgroup cose COSE apis
 * ##COSE related functions
 * \ingroup lwsaoi
 *
 * COSE RFC 8152 relates to signed and encrypted CBOR
 */
//@{

enum {
	/*  RFC8152: Table 2: Common Header Parameters
	 * https://www.iana.org/assignments/cose/cose.xhtml#header-parameters
	 */

	LWSCOSE_WKL_ALG				= 1,   /* int / tstr */
	LWSCOSE_WKL_CRIT,			       /* [+ label ] */
	LWSCOSE_WKL_CONTENT_TYPE,		       /* tstr / uint */
	LWSCOSE_WKL_KID,			       /* bstr */
	LWSCOSE_WKL_IV,				       /* bstr */
	LWSCOSE_WKL_IV_PARTIAL,			       /* bstr */
	LWSCOSE_WKL_COUNTERSIG,			       /* COSE sig(s) */
	LWSCOSE_WKL_COUNTERSIG0			= 9,   /* bstr */
	LWSCOSE_WKL_KID_CONTEXT,		       /* bstr */
	LWSCOSE_WKL_CUPH_NONCE			= 256, /* bstr */
	LWSCOSE_WKL_CUPH_OWNER_PUBKEY		= 257, /* array */

	/*  RFC8152: Table 3: key map labels */

	LWSCOSE_WKK_KTY				= 1, /* int / tstr */
	LWSCOSE_WKK_KID,			     /* bstr */
	LWSCOSE_WKK_ALG,			     /* int / tstr */
	LWSCOSE_WKK_KEY_OPS,			     /* [ + (int / tstr) ] */
	LWSCOSE_WKK_BASE_IV,			     /* bstr */

	/*  RFC8152: Table 4: Key Operation Values */

	LWSCOSE_WKKO_SIGN			= 1,
	LWSCOSE_WKKO_VERIFY,
	LWSCOSE_WKKO_ENCRYPT,
	LWSCOSE_WKKO_DECRYPT,
	LWSCOSE_WKKO_WRAP_KEY,
	LWSCOSE_WKKO_UNWRAP_KEY,
	LWSCOSE_WKKO_DERIVE_KEY,
	LWSCOSE_WKKO_DERIVE_BITS,
	LWSCOSE_WKKO_MAC_CREATE,
	LWSCOSE_WKKO_MAC_VERIFY,

	/*  RFC8152: Table 5: ECDSA algs */

	LWSCOSE_WKAECDSA_ALG_ES256		= -7,
	LWSCOSE_WKAECDSA_ALG_ES384		= -35,
	LWSCOSE_WKAECDSA_ALG_ES512		= -36,

	/*  RFC8152: Table 6: EDDSA algs */

	LWSCOSE_WKAEDDSA_ALG_EDDSA		= -8,

	/*  RFC8152: Table 7: HMAC algs */

	LWSCOSE_WKAHMAC_256_64			= 4,
	LWSCOSE_WKAHMAC_256_256,
	LWSCOSE_WKAHMAC_384_384,
	LWSCOSE_WKAHMAC_512_512,

	/*  RFC8152: Table 8: AES algs */

	LWSCOSE_WKAAES_128_64			= 14,
	LWSCOSE_WKAAES_256_64,
	LWSCOSE_WKAAES_128_128			= 25,
	LWSCOSE_WKAAES_256_128,

	/*  RFC8152: Table 9: AES GCM algs */

	LWSCOSE_WKAAESGCM_128			= 1,
	LWSCOSE_WKAAESGCM_192,
	LWSCOSE_WKAAESGCM_256,

	/*  RFC8152: Table 10: AES CCM algs */

	LWSCOSE_WKAAESCCM_16_64_128		= 10,
	LWSCOSE_WKAAESCCM_16_64_256,
	LWSCOSE_WKAAESCCM_64_64_128,
	LWSCOSE_WKAAESCCM_64_64_256,
	LWSCOSE_WKAAESCCM_16_128_128,
	LWSCOSE_WKAAESCCM_16_128_256,
	LWSCOSE_WKAAESCCM_64_128_128,
	LWSCOSE_WKAAESCCM_64_128_256,

	/*  RFC8152: Table 11: CHACHA20 / Poly1305 */

	LWSCOSE_WKACHACHA_POLY1305		= 24,

	/*  RFC8152: Table 13: HKDF param */

	LWSCOSE_WKAPHKDF_SALT			= -20,

	/* RFC8152: Table 14: Context Algorithm Parameters */

	LWSCOSE_WKAPCTX_PARTY_U_IDENTITY	= -21,
	LWSCOSE_WKAPCTX_PARTY_U_NONCE		= -22,
	LWSCOSE_WKAPCTX_PARTY_U_OTHER		= -23,
	LWSCOSE_WKAPCTX_PARTY_V_IDENTITY	= -24,
	LWSCOSE_WKAPCTX_PARTY_V_NONCE		= -25,
	LWSCOSE_WKAPCTX_PARTY_V_OTHER		= -26,

	/* RFC8152: Table 15: Direct key */

	LWSCOSE_WKK_DIRECT_CEK			= -6,

	/* RFC8152: Table 16: Direct key with KDF */

	LWSCOSE_WKK_DIRECT_HKDF_SHA_256		= -10,
	LWSCOSE_WKK_DIRECT_HKDF_SHA_512		= -11,
	LWSCOSE_WKK_DIRECT_HKDF_AES_128		= -12,
	LWSCOSE_WKK_DIRECT_HKDF_AES_256		= -13,

	/* RFC8152: Table 17: AES Key Wrap Algorithm Values */

	LWSCOSE_WKK_DIRECT_HKDFKW_SHA_256	= -3,
	LWSCOSE_WKK_DIRECT_HKDFKW_SHA_512	= -4,
	LWSCOSE_WKK_DIRECT_HKDFKW_AES_128	= -5,

	/* RFC8152: Table 18: ECDH Algorithm Values */

	LWSCOSE_WKAECDH_ALG_ES_HKDF_256		= -25,
	LWSCOSE_WKAECDH_ALG_ES_HKDF_512		= -26,
	LWSCOSE_WKAECDH_ALG_SS_HKDF_256		= -27,
	LWSCOSE_WKAECDH_ALG_SS_HKDF_512		= -28,

	/* RFC8152: Table 19: ECDH Algorithm Parameters */

	LWSCOSE_WKAPECDH_EPHEMERAL_KEY		= -1,
	LWSCOSE_WKAPECDH_STATIC_KEY		= -2,
	LWSCOSE_WKAPECDH_STATIC_KEY_ID		= -3,

	/* RFC8152: Table 20: ECDH Algorithm Parameters with key wrap */

	LWSCOSE_WKAPECDH_ES_A128KW		= -29,
	LWSCOSE_WKAPECDH_ES_A192KW		= -30,
	LWSCOSE_WKAPECDH_ES_A256KW		= -31,
	LWSCOSE_WKAPECDH_SS_A128KW		= -32,
	LWSCOSE_WKAPECDH_SS_A192KW		= -33,
	LWSCOSE_WKAPECDH_SS_A256KW		= -34,

	/* RFC8152: Table 21: Key Type Values
	 *  https://www.iana.org/assignments/cose/cose.xhtml#key-type
	 */

	LWSCOSE_WKKTV_OKP			= 1,
	LWSCOSE_WKKTV_EC2			= 2,
	LWSCOSE_WKKTV_RSA			= 3,
	LWSCOSE_WKKTV_SYMMETRIC			= 4,
	LWSCOSE_WKKTV_HSS_LMS			= 5,
	LWSCOSE_WKKTV_WALNUTDSA			= 6,


	/* RFC8152: Table 22: Elliptic Curves
	 * https://www.iana.org/assignments/cose/cose.xhtml#elliptic-curves
	 */

	LWSCOSE_WKEC_P256			= 1,
	LWSCOSE_WKEC_P384,
	LWSCOSE_WKEC_P521,
	LWSCOSE_WKEC_X25519,
	LWSCOSE_WKEC_X448,
	LWSCOSE_WKEC_ED25519,
	LWSCOSE_WKEC_ED448,
	LWSCOSE_WKEC_SECP256K1,

	/* RFC8152: Table 23: EC Key Parameters */

	LWSCOSE_WKECKP_CRV			= -1,
	LWSCOSE_WKECKP_X			= -2,
	LWSCOSE_WKECKP_Y			= -3,
	LWSCOSE_WKECKP_D			= -4,

	/* RFC8152: Table 24: Octet Key Pair (OKP) Parameters */

	LWSCOSE_WKOKP_CRV			= -1,
	LWSCOSE_WKOKP_X				= -2,
	LWSCOSE_WKOKP_D				= -4,

	/* Additional from
	 * https://www.iana.org/assignments/cose/cose.xhtml#key-type-parameters
	 */

	LWSCOSE_WKKPRSA_N			= -1,
	LWSCOSE_WKKPRSA_E			= -2,
	LWSCOSE_WKKPRSA_D			= -3,
	LWSCOSE_WKKPRSA_P			= -4,
	LWSCOSE_WKKPRSA_Q			= -5,
	LWSCOSE_WKKPRSA_DP			= -6,
	LWSCOSE_WKKPRSA_DQ			= -7,
	LWSCOSE_WKKPRSA_QINV			= -8,
	LWSCOSE_WKKPRSA_OTHER			= -9,
	LWSCOSE_WKKPRSA_RI			= -10,
	LWSCOSE_WKKPRSA_DI			= -11,
	LWSCOSE_WKKPRSA_TI			= -12,

	/* RFC8152: Table 25: Symmetric Key Parameters */

	LWSCOSE_WKSYMKP_KEY_VALUE		= 4,

	/* RFC8152: Table 26: CoAP Content-Formats for COSE */

	LWSCOAP_CONTENTFORMAT_COSE_SIGN		= 98,
	LWSCOAP_CONTENTFORMAT_COSE_SIGN1	= 18,
	LWSCOAP_CONTENTFORMAT_COSE_ENCRYPT	= 96,
	LWSCOAP_CONTENTFORMAT_COSE_ENCRYPT0	= 16,
	LWSCOAP_CONTENTFORMAT_COSE_MAC		= 97,
	LWSCOAP_CONTENTFORMAT_COSE_MAC0		= 17,
	LWSCOAP_CONTENTFORMAT_COSE_KEY		= 101,
	LWSCOAP_CONTENTFORMAT_COSE_KEY_SET	= 102,

	/* RFC8152: Table 27: Header Parameter for CounterSignature0 */

	LWSCOSE_WKL_COUNTERSIGNATURE0		= 9, /* bstr */

	/* RFC8812: Table 1: RSASSA-PKCS1-v1_5 Algorithm Values */

	LWSCOSE_WKARSA_ALG_RS256		= -257, /* + SHA-256 */
	LWSCOSE_WKARSA_ALG_RS384		= -258, /* + SHA-384 */
	LWSCOSE_WKARSA_ALG_RS512		= -259, /* + SHA-512 */
};

enum enum_cose_key_meta_tok {
	COSEKEY_META_KTY,
	COSEKEY_META_KID,
	COSEKEY_META_KEY_OPS,
	COSEKEY_META_BASE_IV,
	COSEKEY_META_ALG,

	LWS_COUNT_COSE_KEY_ELEMENTS
};

typedef int64_t cose_param_t;

LWS_VISIBLE LWS_EXTERN const char *
lws_cose_alg_to_name(cose_param_t alg);

LWS_VISIBLE LWS_EXTERN cose_param_t
lws_cose_name_to_alg(const char *name);

/*
 * cose_key
 */

typedef struct lws_cose_key {
	/* key data elements */
	struct lws_gencrypto_keyelem	e[LWS_GENCRYPTO_MAX_KEYEL_COUNT];
	/* generic meta key elements, like KID */
	struct lws_gencrypto_keyelem 	meta[LWS_COUNT_COSE_KEY_ELEMENTS];
	lws_dll2_t			list; /* used when part of a set */
	int				gencrypto_kty;	/**< one of LWS_GENCRYPTO_KTY_ */
	cose_param_t			kty;
	cose_param_t			cose_alg;
	cose_param_t			cose_curve;
	char 				private_key; /* nonzero = has private key elements */
} lws_cose_key_t;

typedef int (*lws_cose_key_import_callback)(struct lws_cose_key *s, void *user);

/** lws_cose_jwk_import() - Create an lws_cose_key_t object from cose_key CBOR
 *
 * \param pkey_set: NULL, or a pointer to an lws_dll2_owner_t for a cose_key set
 * \param cb: callback for each jwk-processed key, or NULL if importing a single
 *	      key with no parent "keys" JSON
 * \param user: pointer to be passed to the callback, otherwise ignored by lws.
 *		NULL if importing a single key with no parent "keys" JSON
 * \param in: a single cose_key
 * \param len: the length of the cose_key in bytes
 *
 * Creates a single lws_cose_key_t if \p pkey_set is NULL or if the incoming
 * CBOR doesn't start with an array, otherwise expects a CBOR array containing
 * zero or more cose_key CBOR, and adds each to the \p pkey_set
 * lws_dll2_owner_t struct.  Created lws_cose_key_t are filled with data from
 * the COSE representation and can be used with other COSE crypto ops.
 */
LWS_VISIBLE LWS_EXTERN lws_cose_key_t *
lws_cose_key_import(lws_dll2_owner_t *pkey_set, lws_cose_key_import_callback cb,
		    void *user, const uint8_t *in, size_t len);

/** lws_cose_key_export() - Create cose_key CBOR from an lws_cose_key_t
 *
 * \param ck: the lws_cose_key_t to export to CBOR
 * \param ctx: the CBOR writing context (same as for lws_lec_printf())
 * \param flags: 0 to export only public elements, or LWSJWKF_EXPORT_PRIVATE
 *
 * Creates an lws_jwk struct filled with data from the COSE representation.
 */
LWS_VISIBLE LWS_EXTERN enum lws_lec_pctx_ret
lws_cose_key_export(lws_cose_key_t *ck, lws_lec_pctx_t *ctx, int flags);

/**
 * lws_cose_key_generate() - generate a fresh key
 *
 * \param context: the lws_context used to get random
 * \param cose_kty: one of LWSCOSE_WKKTV_ indicating the well-known key type
 * \param use_mask: 0, or a bitfield where (1 << LWSCOSE_WKKO_...) set means valid for use
 * \param bits: key bits for RSA
 * \param curve: for EC keys, one of "P-256", "P-384" or "P-521" currently
 * \param kid: string describing the key, or NULL
 *
 * Create an lws_cose_key_t of the specified type and return it
 */
LWS_VISIBLE LWS_EXTERN lws_cose_key_t *
lws_cose_key_generate(struct lws_context *context, cose_param_t cose_kty,
		      int use_mask, int bits, const char *curve,
		      const uint8_t *kid, size_t kl);

LWS_VISIBLE LWS_EXTERN lws_cose_key_t *
lws_cose_key_from_set(lws_dll2_owner_t *set, const uint8_t *kid, size_t kl);

LWS_VISIBLE LWS_EXTERN void
lws_cose_key_destroy(lws_cose_key_t **ck);

LWS_VISIBLE LWS_EXTERN void
lws_cose_key_set_destroy(lws_dll2_owner_t *o);

/* only available in _DEBUG build */

LWS_VISIBLE LWS_EXTERN void
lws_cose_key_dump(const lws_cose_key_t *ck);

/*
 * cose_sign
 */

struct lws_cose_validate_context;


enum lws_cose_sig_types {
	SIGTYPE_UNKNOWN,
	SIGTYPE_MULTI,
	SIGTYPE_SINGLE,
	SIGTYPE_COUNTERSIGNED, /* not yet supported */
	SIGTYPE_MAC, /* only supported for validation */
	SIGTYPE_MAC0,
};

/* a list of these result objects is the output of the validation process */

typedef struct {
	lws_dll2_t		list;

	const lws_cose_key_t	*cose_key;
	cose_param_t		cose_alg;

	int			result; /* 0 = validated */

} lws_cose_validate_res_t;

enum {
	LCOSESIGEXTCB_RET_FINISHED,
	LCOSESIGEXTCB_RET_AGAIN,
	LCOSESIGEXTCB_RET_ERROR		= -1
};

typedef struct {
	struct lws_cose_validate_context *cps;
	const uint8_t			 *ext;
	size_t				 xl;
} lws_cose_sig_ext_pay_t;

typedef int (*lws_cose_sign_ext_pay_cb_t)(lws_cose_sig_ext_pay_t *x);
typedef int (*lws_cose_validate_pay_cb_t)(struct lws_cose_validate_context *cps,
					  void *opaque, const uint8_t *paychunk,
					  size_t paychunk_len);

typedef struct lws_cose_validate_create_info {
	struct lws_context		*cx;
	/**< REQUIRED: the lws context */
	lws_dll2_owner_t		*keyset;
	/**< REQUIRED: one or more cose_keys */

	enum lws_cose_sig_types		sigtype;
	/**<  0 if a CBOR tag is in the sig, else one of SIGTYPE_MULTI,
	 * SIGTYPE_SINGLE, etc*/

	lws_cose_validate_pay_cb_t	pay_cb;
	/**< optional: called back with unvalidated payload pieces */
	void				*pay_opaque;
	/**< optional: passed into pay_cb callback along with payload chunk */

	lws_cose_sign_ext_pay_cb_t	ext_cb;
	/**< optional extra application data provision callback */
	void				*ext_opaque;
	/**< optional extra application data provision callback opaque */
	size_t				ext_len;
	/**< if we have extra app data, this must be set to the length of it */
} lws_cose_validate_create_info_t;

/**
 * lws_cose_validate_create() - create a signature validation context
 *
 * \param info: struct describing the validation context to create
 *
 * Creates a signature validation context set up as described in \p info.
 *
 * You can then pass the signature cbor chunks to it using
 * lws_cose_validate_chunk(), finialize and get the results list using
 * lws_cose_validate_results() and destroy with lws_cose_validate_destroy().
 */
LWS_VISIBLE LWS_EXTERN struct lws_cose_validate_context *
lws_cose_validate_create(const lws_cose_validate_create_info_t *info);

/**
 * lws_cose_validate_chunk() - passes chunks of CBOR into the signature validator
 *
 * \param cps: the validation context
 * \param in: the chunk of CBOR (does not have to be logically complete)
 * \param in_len: number of bytes available at \p in
 *
 * Parses signature CBOR to produce a list of result objects.
 *
 *
 */
LWS_VISIBLE LWS_EXTERN int
lws_cose_validate_chunk(struct lws_cose_validate_context *cps,
			const uint8_t *in, size_t in_len, size_t *used_in);

LWS_VISIBLE LWS_EXTERN lws_dll2_owner_t *
lws_cose_validate_results(struct lws_cose_validate_context *cps);

LWS_VISIBLE LWS_EXTERN void
lws_cose_validate_destroy(struct lws_cose_validate_context **cps);

struct lws_cose_sign_context;

#define LCSC_FL_ADD_CBOR_TAG		(1 << 0)
#define LCSC_FL_ADD_CBOR_PREFER_MAC0	(1 << 1)

typedef struct lws_cose_sign_create_info {
	struct lws_context		*cx;
	/**< REQUIRED: the lws context */
	lws_dll2_owner_t		*keyset;
	/**< REQUIRED: one or more cose_keys */

	lws_lec_pctx_t			*lec;
	/**< REQUIRED: the cbor output context to emit to, user must
	 * initialize with lws_lec_init() beforehand */

	lws_cose_sign_ext_pay_cb_t	ext_cb;
	/**< optional extra application data provision callback */
	void				*ext_opaque;
	/**< optional extra application data provision callback opaque */
	size_t				ext_len;
	/**< if we have extra app data, this must be set to the length of it */

	size_t				inline_payload_len;
	/**< REQUIRED: size of the inline payload we will provide */

	int				flags;
	/**< bitmap of  LCSC_FL_* */
	enum lws_cose_sig_types		sigtype;
	/**< 0, or sign type hint */
} lws_cose_sign_create_info_t;

/**
 * lws_cose_sign_create() - Create a signing context
 *
 * \param info: a structure describing the signing context you want to create
 *
 * This allocates and returns a signing context created according to what is in
 * the \p info parameter.
 *
 * \p info must be prepared with the lws_context, a keyset to use, a CBOR
 * output context, and the inline payload length.
 *
 * Returns NULL on failure or the created signing context ready to add alg(s)
 * to.
 */

LWS_VISIBLE LWS_EXTERN struct lws_cose_sign_context *
lws_cose_sign_create(const lws_cose_sign_create_info_t *info);

LWS_VISIBLE LWS_EXTERN int
lws_cose_sign_add(struct lws_cose_sign_context *csc, cose_param_t alg,
		  const lws_cose_key_t *ck);

LWS_VISIBLE LWS_EXTERN enum lws_lec_pctx_ret
lws_cose_sign_payload_chunk(struct lws_cose_sign_context *csc,
			    const uint8_t *in, size_t in_len);

LWS_VISIBLE LWS_EXTERN void
lws_cose_sign_destroy(struct lws_cose_sign_context **csc);

//@}
