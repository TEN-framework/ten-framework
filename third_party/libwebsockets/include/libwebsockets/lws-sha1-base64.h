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

/** \defgroup sha SHA and B64 helpers
 * ##SHA and B64 helpers
 *
 * These provide SHA-1 and B64 helper apis
 */
///@{
#ifdef LWS_SHA1_USE_OPENSSL_NAME
#define lws_SHA1 SHA1
#else
/**
 * lws_SHA1(): make a SHA-1 digest of a buffer
 *
 * \param d: incoming buffer
 * \param n: length of incoming buffer
 * \param md: buffer for message digest (must be >= 20 bytes)
 *
 * Reduces any size buffer into a 20-byte SHA-1 hash.
 */
LWS_VISIBLE LWS_EXTERN unsigned char *
lws_SHA1(const unsigned char *d, size_t n, unsigned char *md);
#endif
/**
 * lws_b64_encode_string(): encode a string into base 64
 *
 * \param in: incoming buffer
 * \param in_len: length of incoming buffer
 * \param out: result buffer
 * \param out_size: length of result buffer
 *
 * Encodes a string using b64
 */
LWS_VISIBLE LWS_EXTERN int
lws_b64_encode_string(const char *in, int in_len, char *out, int out_size);
/**
 * lws_b64_encode_string_url(): encode a string into base 64
 *
 * \param in: incoming buffer
 * \param in_len: length of incoming buffer
 * \param out: result buffer
 * \param out_size: length of result buffer
 *
 * Encodes a string using b64 with the "URL" variant (+ -> -, and / -> _)
 */
LWS_VISIBLE LWS_EXTERN int
lws_b64_encode_string_url(const char *in, int in_len, char *out, int out_size);
/**
 * lws_b64_decode_string(): decode a string from base 64
 *
 * \param in: incoming buffer
 * \param out: result buffer
 * \param out_size: length of result buffer
 *
 * Decodes a NUL-terminated string using b64
 */
LWS_VISIBLE LWS_EXTERN int
lws_b64_decode_string(const char *in, char *out, int out_size);
/**
 * lws_b64_decode_string_len(): decode a string from base 64
 *
 * \param in: incoming buffer
 * \param in_len: length of incoming buffer
 * \param out: result buffer
 * \param out_size: length of result buffer
 *
 * Decodes a range of chars using b64
 */
LWS_VISIBLE LWS_EXTERN int
lws_b64_decode_string_len(const char *in, int in_len, char *out, int out_size);

struct lws_b64state {
	unsigned char quad[4];
	size_t done;
	size_t len;
	int i;
	int c;
};

LWS_VISIBLE LWS_EXTERN void
lws_b64_decode_state_init(struct lws_b64state *state);

LWS_VISIBLE LWS_EXTERN int
lws_b64_decode_stateful(struct lws_b64state *s, const char *in, size_t *in_len,
			uint8_t *out, size_t *out_size, int final);
///@}

