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
 *  lws-gencrypto openssl-specific common code
 */

#include "private-lib-core.h"
#include "private-lib-tls-openssl.h"

/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

int
lws_gencrypto_openssl_hash_to_NID(enum lws_genhash_types hash_type)
{
	int h = -1;

	switch (hash_type) {
	case LWS_GENHASH_TYPE_UNKNOWN:
		break;
	case LWS_GENHASH_TYPE_MD5:
		h = NID_md5;
		break;
	case LWS_GENHASH_TYPE_SHA1:
		h = NID_sha1;
		break;
	case LWS_GENHASH_TYPE_SHA256:
		h = NID_sha256;
		break;
	case LWS_GENHASH_TYPE_SHA384:
		h = NID_sha384;
		break;
	case LWS_GENHASH_TYPE_SHA512:
		h = NID_sha512;
		break;
	}

	return h;
}

const EVP_MD *
lws_gencrypto_openssl_hash_to_EVP_MD(enum lws_genhash_types hash_type)
{
	const EVP_MD *h = NULL;

	switch (hash_type) {
	case LWS_GENHASH_TYPE_UNKNOWN:
		break;
	case LWS_GENHASH_TYPE_MD5:
		h = EVP_md5();
		break;
	case LWS_GENHASH_TYPE_SHA1:
		h = EVP_sha1();
		break;
	case LWS_GENHASH_TYPE_SHA256:
		h = EVP_sha256();
		break;
	case LWS_GENHASH_TYPE_SHA384:
		h = EVP_sha384();
		break;
	case LWS_GENHASH_TYPE_SHA512:
		h = EVP_sha512();
		break;
	}

	return h;
}
