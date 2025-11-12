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
 *  lws_genhash provides a hash / hmac abstraction api in lws that works the
 *  same whether you are using openssl or mbedtls hash functions underneath.
 */
#include "libwebsockets.h"
#include <mbedtls/version.h>

#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER >= 0x03000000)
#define mbedtls_md5_starts_ret mbedtls_md5_starts
#define mbedtls_md5_update_ret mbedtls_md5_update
#define mbedtls_md5_finish_ret mbedtls_md5_finish
#define mbedtls_sha1_finish_ret mbedtls_sha1_finish
#define mbedtls_sha1_update_ret mbedtls_sha1_update
#define mbedtls_sha1_starts_ret mbedtls_sha1_starts
#define mbedtls_sha256_starts_ret mbedtls_sha256_starts
#define mbedtls_sha256_update_ret mbedtls_sha256_update
#define mbedtls_sha256_finish_ret mbedtls_sha256_finish
#define mbedtls_sha512_starts_ret mbedtls_sha512_starts
#define mbedtls_sha512_update_ret mbedtls_sha512_update
#define mbedtls_sha512_finish_ret mbedtls_sha512_finish
#endif

#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER >= 0x02070000)

/*
 * We have the _ret variants available, check the return codes on everything
 */

int
lws_genhash_init(struct lws_genhash_ctx *ctx, enum lws_genhash_types type)
{
	ctx->type = (uint8_t)type;

	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		mbedtls_md5_init(&ctx->u.md5);
		if (mbedtls_md5_starts_ret(&ctx->u.md5))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA1:
		mbedtls_sha1_init(&ctx->u.sha1);
		if (mbedtls_sha1_starts_ret(&ctx->u.sha1))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA256:
		mbedtls_sha256_init(&ctx->u.sha256);
		if (mbedtls_sha256_starts_ret(&ctx->u.sha256, 0))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA384:
		mbedtls_sha512_init(&ctx->u.sha512);
		if (mbedtls_sha512_starts_ret(&ctx->u.sha512, 1 /* is384 */))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA512:
		mbedtls_sha512_init(&ctx->u.sha512);
		if (mbedtls_sha512_starts_ret(&ctx->u.sha512, 0))
			return 1;
		break;
	default:
		return 1;
	}

	return 0;
}

int
lws_genhash_update(struct lws_genhash_ctx *ctx, const void *in, size_t len)
{
	if (!len)
		return 0;

	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		if (mbedtls_md5_update_ret(&ctx->u.md5, in, len))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA1:
		if (mbedtls_sha1_update_ret(&ctx->u.sha1, in, len))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA256:
		if (mbedtls_sha256_update_ret(&ctx->u.sha256, in, len))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA384:
		if (mbedtls_sha512_update_ret(&ctx->u.sha512, in, len))
			return 1;
		break;
	case LWS_GENHASH_TYPE_SHA512:
		if (mbedtls_sha512_update_ret(&ctx->u.sha512, in, len))
			return 1;
		break;
	}

	return 0;
}

int
lws_genhash_destroy(struct lws_genhash_ctx *ctx, void *result)
{
	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		if (mbedtls_md5_finish_ret(&ctx->u.md5, result))
			return 1;
		mbedtls_md5_free(&ctx->u.md5);
		break;
	case LWS_GENHASH_TYPE_SHA1:
		if (mbedtls_sha1_finish_ret(&ctx->u.sha1, result))
			return 1;
		mbedtls_sha1_free(&ctx->u.sha1);
		break;
	case LWS_GENHASH_TYPE_SHA256:
		if (mbedtls_sha256_finish_ret(&ctx->u.sha256, result))
			return 1;
		mbedtls_sha256_free(&ctx->u.sha256);
		break;
	case LWS_GENHASH_TYPE_SHA384:
		if (mbedtls_sha512_finish_ret(&ctx->u.sha512, result))
			return 1;
		mbedtls_sha512_free(&ctx->u.sha512);
		break;
	case LWS_GENHASH_TYPE_SHA512:
		if (mbedtls_sha512_finish_ret(&ctx->u.sha512, result))
			return 1;
		mbedtls_sha512_free(&ctx->u.sha512);
		break;
	}

	return 0;
}

#else

/*
 * mbedtls is too old to have the _ret variants
 */

int
lws_genhash_init(struct lws_genhash_ctx *ctx, enum lws_genhash_types type)
{
	ctx->type = type;

	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		mbedtls_md5_init(&ctx->u.md5);
		mbedtls_md5_starts(&ctx->u.md5);
		break;
	case LWS_GENHASH_TYPE_SHA1:
		mbedtls_sha1_init(&ctx->u.sha1);
		mbedtls_sha1_starts(&ctx->u.sha1);
		break;
	case LWS_GENHASH_TYPE_SHA256:
		mbedtls_sha256_init(&ctx->u.sha256);
		mbedtls_sha256_starts(&ctx->u.sha256, 0);
		break;
	case LWS_GENHASH_TYPE_SHA384:
		mbedtls_sha512_init(&ctx->u.sha512);
		mbedtls_sha512_starts(&ctx->u.sha512, 1 /* is384 */);
		break;
	case LWS_GENHASH_TYPE_SHA512:
		mbedtls_sha512_init(&ctx->u.sha512);
		mbedtls_sha512_starts(&ctx->u.sha512, 0);
		break;
	default:
		return 1;
	}

	return 0;
}

int
lws_genhash_update(struct lws_genhash_ctx *ctx, const void *in, size_t len)
{
	if (!len)
		return 0;

	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		mbedtls_md5_update(&ctx->u.md5, in, len);
		break;
	case LWS_GENHASH_TYPE_SHA1:
		mbedtls_sha1_update(&ctx->u.sha1, in, len);
		break;
	case LWS_GENHASH_TYPE_SHA256:
		mbedtls_sha256_update(&ctx->u.sha256, in, len);
		break;
	case LWS_GENHASH_TYPE_SHA384:
		mbedtls_sha512_update(&ctx->u.sha512, in, len);
		break;
	case LWS_GENHASH_TYPE_SHA512:
		mbedtls_sha512_update(&ctx->u.sha512, in, len);
		break;
	}

	return 0;
}

int
lws_genhash_destroy(struct lws_genhash_ctx *ctx, void *result)
{
	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		mbedtls_md5_finish(&ctx->u.md5, result);
		mbedtls_md5_free(&ctx->u.md5);
		break;
	case LWS_GENHASH_TYPE_SHA1:
		mbedtls_sha1_finish(&ctx->u.sha1, result);
		mbedtls_sha1_free(&ctx->u.sha1);
		break;
	case LWS_GENHASH_TYPE_SHA256:
		mbedtls_sha256_finish(&ctx->u.sha256, result);
		mbedtls_sha256_free(&ctx->u.sha256);
		break;
	case LWS_GENHASH_TYPE_SHA384:
		mbedtls_sha512_finish(&ctx->u.sha512, result);
		mbedtls_sha512_free(&ctx->u.sha512);
		break;
	case LWS_GENHASH_TYPE_SHA512:
		mbedtls_sha512_finish(&ctx->u.sha512, result);
		mbedtls_sha512_free(&ctx->u.sha512);
		break;
	}

	return 0;
}

#endif

int
lws_genhmac_init(struct lws_genhmac_ctx *ctx, enum lws_genhmac_types type,
		 const uint8_t *key, size_t key_len)
{
	int t;

	ctx->type = (uint8_t)type;

	switch (type) {
	case LWS_GENHMAC_TYPE_SHA256:
		t = MBEDTLS_MD_SHA256;
		break;
	case LWS_GENHMAC_TYPE_SHA384:
		t = MBEDTLS_MD_SHA384;
		break;
	case LWS_GENHMAC_TYPE_SHA512:
		t = MBEDTLS_MD_SHA512;
		break;
	default:
		return -1;
	}

	ctx->hmac = mbedtls_md_info_from_type((mbedtls_md_type_t)t);
	if (!ctx->hmac)
		return -1;

#if !defined(LWS_HAVE_mbedtls_md_setup)
	if (mbedtls_md_init_ctx(&ctx->ctx, ctx->hmac))
		return -1;
#else
	if (mbedtls_md_setup(&ctx->ctx, ctx->hmac, 1))
		return -1;
#endif

	if (mbedtls_md_hmac_starts(&ctx->ctx, key, key_len)) {
		mbedtls_md_free(&ctx->ctx);
		ctx->hmac = NULL;

		return -1;
	}

	return 0;
}

int
lws_genhmac_update(struct lws_genhmac_ctx *ctx, const void *in, size_t len)
{
	if (!len)
		return 0;

	if (mbedtls_md_hmac_update(&ctx->ctx, in, len))
		return -1;

	return 0;
}

int
lws_genhmac_destroy(struct lws_genhmac_ctx *ctx, void *result)
{
	int n = 0;

	if (result)
		n = mbedtls_md_hmac_finish(&ctx->ctx, result);

	mbedtls_md_free(&ctx->ctx);
	ctx->hmac = NULL;
	if (n)
		return -1;

	return 0;
}
