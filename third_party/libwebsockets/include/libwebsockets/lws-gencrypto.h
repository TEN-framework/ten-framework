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

/*
 * These are gencrypto-level constants... they are used by both JOSE and direct
 * gencrypto code.  However while JWK relies on these, using gencrypto apis has
 * no dependency at all on any JOSE type.
 */

enum lws_gencrypto_kty {
	LWS_GENCRYPTO_KTY_UNKNOWN,

	LWS_GENCRYPTO_KTY_OCT,
	LWS_GENCRYPTO_KTY_RSA,
	LWS_GENCRYPTO_KTY_EC
};

/*
 * Keytypes where the same element name is reused must all agree to put the
 * same-named element at the same e[] index.  It's because when used with jwk,
 * we parse and store in incoming key data, but we may not be informed of the
 * definitive keytype until the end.
 */

enum lws_gencrypto_oct_tok {
	LWS_GENCRYPTO_OCT_KEYEL_K, /* note... same offset as AES K */

	LWS_GENCRYPTO_OCT_KEYEL_COUNT
};

enum lws_gencrypto_rsa_tok {
	LWS_GENCRYPTO_RSA_KEYEL_E,
	LWS_GENCRYPTO_RSA_KEYEL_N,
	LWS_GENCRYPTO_RSA_KEYEL_D, /* note... same offset as EC D */
	LWS_GENCRYPTO_RSA_KEYEL_P,
	LWS_GENCRYPTO_RSA_KEYEL_Q,
	LWS_GENCRYPTO_RSA_KEYEL_DP,
	LWS_GENCRYPTO_RSA_KEYEL_DQ,
	LWS_GENCRYPTO_RSA_KEYEL_QI,

	/* we don't actively use these if given, but may come from COSE */

	LWS_GENCRYPTO_RSA_KEYEL_OTHER,
	LWS_GENCRYPTO_RSA_KEYEL_RI,
	LWS_GENCRYPTO_RSA_KEYEL_DI,
	LWS_GENCRYPTO_RSA_KEYEL_TI,

	LWS_GENCRYPTO_RSA_KEYEL_COUNT
};

enum lws_gencrypto_ec_tok {
	LWS_GENCRYPTO_EC_KEYEL_CRV,
	LWS_GENCRYPTO_EC_KEYEL_X,
	/* note... same offset as RSA D */
	LWS_GENCRYPTO_EC_KEYEL_D = LWS_GENCRYPTO_RSA_KEYEL_D,
	LWS_GENCRYPTO_EC_KEYEL_Y,

	LWS_GENCRYPTO_EC_KEYEL_COUNT
};

enum lws_gencrypto_aes_tok {
	/* note... same offset as OCT K */
	LWS_GENCRYPTO_AES_KEYEL_K = LWS_GENCRYPTO_OCT_KEYEL_K,

	LWS_GENCRYPTO_AES_KEYEL_COUNT
};

/* largest number of key elements for any algorithm */
#define LWS_GENCRYPTO_MAX_KEYEL_COUNT LWS_GENCRYPTO_RSA_KEYEL_COUNT

/* this "stretchy" type holds individual key element data in binary form.
 * It's typcially used in an array with the layout mapping the element index to
 * the key element meaning defined by the enums above.  An array of these of
 * length LWS_GENCRYPTO_MAX_KEYEL_COUNT can define key elements for any key
 * type.
 */

typedef struct lws_gencrypto_keyelem {
	uint8_t *buf;
	uint32_t len;
} lws_gc_elem_t;


/**
 * lws_gencrypto_bits_to_bytes() - returns rounded up bytes needed for bits
 *
 * \param bits
 *
 * Returns the number of bytes needed to store the given number of bits.  If
 * a byte is partially used, the byte count is rounded up.
 */
LWS_VISIBLE LWS_EXTERN int
lws_gencrypto_bits_to_bytes(int bits);

/**
 * lws_base64_size() - returns estimated size of base64 encoding
 *
 * \param bytes
 *
 * Returns a slightly oversize estimate of the size of a base64 encoded version
 * of the given amount of unencoded data.
 */
LWS_VISIBLE LWS_EXTERN int
lws_base64_size(int bytes);

/**
 * lws_gencrypto_padded_length() - returns PKCS#5/#7 padded length
 *
 * @param blocksize - blocksize to pad to
 * @param len - Length of input to pad
 *
 * Returns the length of a buffer originally of size len after PKCS#5 or PKCS#7
 * padding has been applied to it.
 */
LWS_VISIBLE LWS_EXTERN size_t
lws_gencrypto_padded_length(size_t block_size, size_t len);
