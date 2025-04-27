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

enum lws_jws_jose_hdr_indexes {
	LJJHI_ALG,	/* REQUIRED */
	LJJHI_JKU,	/* Optional: string */
	LJJHI_JWK,	/* Optional: jwk JSON object: public key: */
	LJJHI_KID,	/* Optional: string */
	LJJHI_X5U,	/* Optional: string: url of public key cert / chain */
	LJJHI_X5C,	/* Optional: base64 (NOT -url): actual cert */
	LJJHI_X5T,	/* Optional: base64url: SHA-1 of actual cert */
	LJJHI_X5T_S256, /* Optional: base64url: SHA-256 of actual cert */
	LJJHI_TYP,	/* Optional: string: media type */
	LJJHI_CTY,	/* Optional: string: content media type */
	LJJHI_CRIT,	/* Optional for send, REQUIRED: array of strings:
			 * mustn't contain standardized strings or null set */

	LJJHI_RECIPS_HDR,
	LJJHI_RECIPS_HDR_ALG,
	LJJHI_RECIPS_HDR_KID,
	LJJHI_RECIPS_EKEY,

	LJJHI_ENC,	/* JWE only: Optional: string */
	LJJHI_ZIP,	/* JWE only: Optional: string ("DEF" = deflate) */

	LJJHI_EPK,	/* Additional arg for JWE ECDH:  ephemeral public key */
	LJJHI_APU,	/* Additional arg for JWE ECDH:  base64url */
	LJJHI_APV,	/* Additional arg for JWE ECDH:  base64url */
	LJJHI_IV,	/* Additional arg for JWE AES:   base64url */
	LJJHI_TAG,	/* Additional arg for JWE AES:   base64url */
	LJJHI_P2S,	/* Additional arg for JWE PBES2: base64url: salt */
	LJJHI_P2C,	/* Additional arg for JWE PBES2: integer: count */

	LWS_COUNT_JOSE_HDR_ELEMENTS
};

enum lws_jose_algtype {
	LWS_JOSE_ENCTYPE_NONE,

	LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5,
	LWS_JOSE_ENCTYPE_RSASSA_PKCS1_OAEP,
	LWS_JOSE_ENCTYPE_RSASSA_PKCS1_PSS,

	LWS_JOSE_ENCTYPE_ECDSA,
	LWS_JOSE_ENCTYPE_ECDHES,

	LWS_JOSE_ENCTYPE_AES_CBC,
	LWS_JOSE_ENCTYPE_AES_CFB128,
	LWS_JOSE_ENCTYPE_AES_CFB8,
	LWS_JOSE_ENCTYPE_AES_CTR,
	LWS_JOSE_ENCTYPE_AES_ECB,
	LWS_JOSE_ENCTYPE_AES_OFB,
	LWS_JOSE_ENCTYPE_AES_XTS,	/* care: requires double-length key */
	LWS_JOSE_ENCTYPE_AES_GCM,
};

/* there's a table of these defined in lws-gencrypto-common.c */

struct lws_jose_jwe_alg {
	enum lws_genhash_types hash_type;
	enum lws_genhmac_types hmac_type;
	enum lws_jose_algtype algtype_signing; /* the signing cipher */
	enum lws_jose_algtype algtype_crypto; /* the encryption cipher */
	const char *alg; /* the JWA enc alg name, eg "ES512" */
	const char *curve_name; /* NULL, or, eg, "P-256" */
	unsigned short keybits_min, keybits_fixed;
	unsigned short ivbits;
};

/*
 * For JWS, "JOSE header" is defined to be the union of...
 *
 * o  JWS Protected Header
 * o  JWS Unprotected Header
 *
 * For JWE, the "JOSE header" is the union of...
 *
 * o  JWE Protected Header
 * o  JWE Shared Unprotected Header
 * o  JWE Per-Recipient Unprotected Header
 */

#define LWS_JWS_MAX_RECIPIENTS 3

struct lws_jws_recpient {
	/*
	 * JOSE per-recipient unprotected header... for JWS this contains
	 * protected / header / signature
	 */
	struct lws_gencrypto_keyelem unprot[LWS_COUNT_JOSE_HDR_ELEMENTS];
	struct lws_jwk jwk_ephemeral;	/* recipient ephemeral key if any */
	struct lws_jwk jwk;		/* recipient "jwk" key if any */
};

struct lws_jose {
	/* JOSE protected and unprotected header elements */
	struct lws_gencrypto_keyelem e[LWS_COUNT_JOSE_HDR_ELEMENTS];

	struct lws_jws_recpient recipient[LWS_JWS_MAX_RECIPIENTS];

	char typ[32];

	/* information from the protected header part */
	const struct lws_jose_jwe_alg *alg;
	const struct lws_jose_jwe_alg *enc_alg;

	int recipients; /* count of used recipient[] entries */
};

/**
 * lws_jose_init() - prepare a struct lws_jose for use
 *
 * \param jose: the jose header struct to prepare
 */
LWS_VISIBLE LWS_EXTERN void
lws_jose_init(struct lws_jose *jose);

/**
 * lws_jose_destroy() - retire a struct lws_jose from use
 *
 * \param jose: the jose header struct to destroy
 */
LWS_VISIBLE LWS_EXTERN void
lws_jose_destroy(struct lws_jose *jose);

/**
 * lws_gencrypto_jws_alg_to_definition() - look up a jws alg name
 *
 * \param alg: the jws alg name
 * \param jose: pointer to the pointer to the info struct to set on success
 *
 * Returns 0 if *jose set, else nonzero for failure
 */
LWS_VISIBLE LWS_EXTERN int
lws_gencrypto_jws_alg_to_definition(const char *alg,
				    const struct lws_jose_jwe_alg **jose);

/**
 * lws_gencrypto_jwe_alg_to_definition() - look up a jwe alg name
 *
 * \param alg: the jwe alg name
 * \param jose: pointer to the pointer to the info struct to set on success
 *
 * Returns 0 if *jose set, else nonzero for failure
 */
LWS_VISIBLE LWS_EXTERN int
lws_gencrypto_jwe_alg_to_definition(const char *alg,
				    const struct lws_jose_jwe_alg **jose);

/**
 * lws_gencrypto_jwe_enc_to_definition() - look up a jwe enc name
 *
 * \param alg: the jwe enc name
 * \param jose: pointer to the pointer to the info struct to set on success
 *
 * Returns 0 if *jose set, else nonzero for failure
 */
LWS_VISIBLE LWS_EXTERN int
lws_gencrypto_jwe_enc_to_definition(const char *enc,
				    const struct lws_jose_jwe_alg **jose);

/**
 * lws_jws_parse_jose() - parse a JWS JOSE header
 *
 * \param jose: the jose struct to set to parsing results
 * \param buf: the raw JOSE header
 * \param len: the length of the raw JOSE header
 * \param temp: parent-owned buffer to "allocate" elements into
 * \param temp_len: amount of space available in temp
 *
 * returns 0 for success, or -1 for error
 * *\p temp_len is updated to reflect the amount of \p temp used if successful.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jws_parse_jose(struct lws_jose *jose,
		   const char *buf, int len, char *temp, int *temp_len);

/**
 * lws_jwe_parse_jose() - parse a JWE JOSE header
 *
 * \param jose: the jose struct to set to parsing results
 * \param buf: the raw JOSE header
 * \param len: the length of the raw JOSE header
 * \param temp: parent-owned buffer to "allocate" elements into
 * \param temp_len: amount of space available in temp
 *
 * returns 0 for success, or -1 for error
 * *\p temp_len is updated to reflect the amount of \p temp used if successful.
 */
LWS_VISIBLE LWS_EXTERN int
lws_jwe_parse_jose(struct lws_jose *jose,
		   const char *buf, int len, char *temp, int *temp_len);

