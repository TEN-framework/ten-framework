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
#include <private-lib-core.h>
#include <openssl/obj_mac.h>
#include <openssl/opensslv.h>
/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

int
lws_genhash_init(struct lws_genhash_ctx *ctx, enum lws_genhash_types type)
{
	ctx->type = (uint8_t)type;
	ctx->mdctx = EVP_MD_CTX_create();
	if (!ctx->mdctx)
		return 1;

	switch (ctx->type) {
	case LWS_GENHASH_TYPE_MD5:
		ctx->evp_type = EVP_md5();
		break;
	case LWS_GENHASH_TYPE_SHA1:
		ctx->evp_type = EVP_sha1();
		break;
	case LWS_GENHASH_TYPE_SHA256:
		ctx->evp_type = EVP_sha256();
		break;
	case LWS_GENHASH_TYPE_SHA384:
		ctx->evp_type = EVP_sha384();
		break;
	case LWS_GENHASH_TYPE_SHA512:
		ctx->evp_type = EVP_sha512();
		break;
	default:
		return 1;
	}

	if (EVP_DigestInit_ex(ctx->mdctx, ctx->evp_type, NULL) != 1) {
		EVP_MD_CTX_destroy(ctx->mdctx);

		return 1;
	}

	return 0;
}

int
lws_genhash_update(struct lws_genhash_ctx *ctx, const void *in, size_t len)
{
	if (!len)
		return 0;

	return EVP_DigestUpdate(ctx->mdctx, in, len) != 1;
}

int
lws_genhash_destroy(struct lws_genhash_ctx *ctx, void *result)
{
	unsigned int len;
	int ret = 0;

	if (!ctx->mdctx)
		return 0;

	if (result)
		ret = EVP_DigestFinal_ex(ctx->mdctx, result, &len) != 1;

	(void)len;

	EVP_MD_CTX_destroy(ctx->mdctx);
	ctx->mdctx = NULL;

	return ret;
}

#if defined(LWS_HAVE_EVP_PKEY_new_raw_private_key)

int
lws_genhmac_init(struct lws_genhmac_ctx *ctx, enum lws_genhmac_types type,
		 const uint8_t *key, size_t key_len)
{
	ctx->ctx = EVP_MD_CTX_create();
	if (!ctx->ctx)
		return -1;

	ctx->evp_type = 0;
	ctx->type = (uint8_t)type;

	switch (type) {
	case LWS_GENHMAC_TYPE_SHA256:
		ctx->evp_type = EVP_sha256();
		break;
	case LWS_GENHMAC_TYPE_SHA384:
		ctx->evp_type = EVP_sha384();
		break;
	case LWS_GENHMAC_TYPE_SHA512:
		ctx->evp_type = EVP_sha512();
		break;
	default:
		lwsl_err("%s: unknown HMAC type %d\n", __func__, type);
		goto bail;
	}

	ctx->key = EVP_PKEY_new_raw_private_key(EVP_PKEY_HMAC, NULL, key, key_len);
	if (!ctx->key)
		goto bail;

	if (EVP_DigestSignInit(ctx->ctx, NULL, ctx->evp_type, NULL, ctx->key) != 1)
		goto bail1;

	return 0;

bail1:
	EVP_PKEY_free(ctx->key);
bail:
	EVP_MD_CTX_free(ctx->ctx);

	return -1;
}

int
lws_genhmac_update(struct lws_genhmac_ctx *ctx, const void *in, size_t len)
{

	if (EVP_DigestSignUpdate(ctx->ctx, in, len) != 1)
		return -1;

	return 0;
}

int
lws_genhmac_destroy(struct lws_genhmac_ctx *ctx, void *result)
{
	size_t size = (size_t)lws_genhmac_size(ctx->type);
	int n;

	n = EVP_DigestSignFinal(ctx->ctx, result, &size);
	EVP_MD_CTX_free(ctx->ctx);
	EVP_PKEY_free(ctx->key);

	if (n != 1)
		return -1;

	return 0;
}

#else

int
lws_genhmac_init(struct lws_genhmac_ctx *ctx, enum lws_genhmac_types type,
		 const uint8_t *key, size_t key_len)
{
#if defined(LWS_HAVE_HMAC_CTX_new)
	ctx->ctx = HMAC_CTX_new();
	if (!ctx->ctx)
		return -1;
#else
	HMAC_CTX_init(&ctx->ctx);
#endif

	ctx->evp_type = 0;
	ctx->type = (uint8_t)type;

	switch (type) {
	case LWS_GENHMAC_TYPE_SHA256:
		ctx->evp_type = EVP_sha256();
		break;
	case LWS_GENHMAC_TYPE_SHA384:
		ctx->evp_type = EVP_sha384();
		break;
	case LWS_GENHMAC_TYPE_SHA512:
		ctx->evp_type = EVP_sha512();
		break;
	default:
		lwsl_err("%s: unknown HMAC type %d\n", __func__, type);
		goto bail;
	}

#if defined(LWS_HAVE_HMAC_CTX_new)
        if (HMAC_Init_ex(ctx->ctx, key, (int)key_len, ctx->evp_type, NULL) != 1)
#else
        if (HMAC_Init_ex(&ctx->ctx, key, (int)key_len, ctx->evp_type, NULL) != 1)
#endif
        	goto bail;

	return 0;

bail:
#if defined(LWS_HAVE_HMAC_CTX_new)
	HMAC_CTX_free(ctx->ctx);
#endif

	return -1;
}

int
lws_genhmac_update(struct lws_genhmac_ctx *ctx, const void *in, size_t len)
{
#if defined(LWS_HAVE_HMAC_CTX_new)
#if defined(LIBRESSL_VERSION_NUMBER)
	if (HMAC_Update(ctx->ctx, in, len) != 1)
#else
	if (HMAC_Update(ctx->ctx, in, (int)len) != 1)
#endif
#else /* HMAC_CTX_new */
	if (HMAC_Update(&ctx->ctx, in, len) != 1)
#endif
		return -1;

	return 0;
}

int
lws_genhmac_destroy(struct lws_genhmac_ctx *ctx, void *result)
{
	unsigned int size = (unsigned int)lws_genhmac_size(ctx->type);
#if defined(LWS_HAVE_HMAC_CTX_new)
	int n = HMAC_Final(ctx->ctx, result, &size);

	HMAC_CTX_free(ctx->ctx);
#else
	int n = HMAC_Final(&ctx->ctx, result, &size);
#endif

	if (n != 1)
		return -1;

	return 0;
}


#endif
