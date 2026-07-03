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
#include "private-lib-tls-mbedtls.h"

mbedtls_md_type_t
lws_gencrypto_mbedtls_hash_to_MD_TYPE(enum lws_genhash_types hash_type)
{
	mbedtls_md_type_t h = (mbedtls_md_type_t)-1;

	switch (hash_type) {
	case LWS_GENHASH_TYPE_MD5:
		h = MBEDTLS_MD_MD5;
		break;
	case LWS_GENHASH_TYPE_SHA1:
		h = MBEDTLS_MD_SHA1;
		break;
	case LWS_GENHASH_TYPE_SHA256:
		h = MBEDTLS_MD_SHA256;
		break;
	case LWS_GENHASH_TYPE_SHA384:
		h = MBEDTLS_MD_SHA384;
		break;
	case LWS_GENHASH_TYPE_SHA512:
		h = MBEDTLS_MD_SHA512;
		break;
	default:
		break;
	}

	return h;
}

int
lws_gencrypto_mbedtls_rngf(void *context, unsigned char *buf, size_t len)
{
	if ((size_t)lws_get_random(context, buf, len) == len) {
		// lwsl_hexdump_err(buf, len);
		return 0;
	}
	lwsl_err("%s: rng failed\n", __func__);
	return -1;
}
