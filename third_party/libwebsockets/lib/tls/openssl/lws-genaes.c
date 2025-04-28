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
 *  lws_genaes provides an AES abstraction api in lws that works the
 *  same whether you are using openssl or mbedtls hash functions underneath.
 */
#include "private-lib-core.h"
#if defined(LWS_WITH_JOSE)
#include "private-lib-jose.h"
#endif

/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

int
lws_genaes_create(struct lws_genaes_ctx *ctx, enum enum_aes_operation op,
		  enum enum_aes_modes mode, struct lws_gencrypto_keyelem *el,
		  enum enum_aes_padding padding, void *engine)
{
	int n = 0;

	ctx->ctx = EVP_CIPHER_CTX_new();
	if (!ctx->ctx)
		return -1;

	ctx->mode = mode;
	ctx->k = el;
	ctx->engine = engine;
	ctx->init = 0;
	ctx->op = op;
	ctx->padding = padding;

	switch (ctx->k->len) {
	case 128 / 8:
		switch (mode) {
		case LWS_GAESM_KW:
#if defined(LWS_HAVE_EVP_aes_128_wrap)
			EVP_CIPHER_CTX_set_flags(ctx->ctx,
						EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
			ctx->cipher = EVP_aes_128_wrap();
			break;
#else
			lwsl_err("%s: your OpenSSL lacks AES wrap apis, update it\n",
				 __func__);
			return -1;
#endif
		case LWS_GAESM_CBC:
			ctx->cipher = EVP_aes_128_cbc();
			break;
#if defined(LWS_HAVE_EVP_aes_128_cfb128)
		case LWS_GAESM_CFB128:
			ctx->cipher = EVP_aes_128_cfb128();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_cfb8)
		case LWS_GAESM_CFB8:
			ctx->cipher = EVP_aes_128_cfb8();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ctr)
		case LWS_GAESM_CTR:
			ctx->cipher = EVP_aes_128_ctr();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ecb)
		case LWS_GAESM_ECB:
			ctx->cipher = EVP_aes_128_ecb();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ofb)
		case LWS_GAESM_OFB:
			ctx->cipher = EVP_aes_128_ofb();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_xts)
		case LWS_GAESM_XTS:
			lwsl_err("%s: AES XTS requires double-length key\n",
				 __func__);
			break;
#endif
		case LWS_GAESM_GCM:
			ctx->cipher = EVP_aes_128_gcm();
			break;
		default:
			goto bail;
		}
		break;

	case 192 / 8:
		switch (mode) {
		case LWS_GAESM_KW:
#if defined(LWS_HAVE_EVP_aes_128_wrap)
			EVP_CIPHER_CTX_set_flags(ctx->ctx,
						EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
			ctx->cipher = EVP_aes_192_wrap();
			break;
#else
                        lwsl_err("%s: your OpenSSL lacks AES wrap apis, update it\n",
                                 __func__);
                        return -1;
#endif
		case LWS_GAESM_CBC:
			ctx->cipher = EVP_aes_192_cbc();
			break;
#if defined(LWS_HAVE_EVP_aes_192_cfb128)
		case LWS_GAESM_CFB128:
			ctx->cipher = EVP_aes_192_cfb128();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_192_cfb8)
		case LWS_GAESM_CFB8:
			ctx->cipher = EVP_aes_192_cfb8();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ctr)
		case LWS_GAESM_CTR:
			ctx->cipher = EVP_aes_192_ctr();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ecb)
		case LWS_GAESM_ECB:
			ctx->cipher = EVP_aes_192_ecb();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ofb)
		case LWS_GAESM_OFB:
			ctx->cipher = EVP_aes_192_ofb();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_xts)
		case LWS_GAESM_XTS:
			lwsl_err("%s: AES XTS 192 invalid\n", __func__);
			goto bail;
#endif
		case LWS_GAESM_GCM:
			ctx->cipher = EVP_aes_192_gcm();
			break;
		default:
			goto bail;
		}
		break;

	case 256 / 8:
		switch (mode) {
		case LWS_GAESM_KW:
#if defined(LWS_HAVE_EVP_aes_128_wrap)
			EVP_CIPHER_CTX_set_flags(ctx->ctx,
						EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
			ctx->cipher = EVP_aes_256_wrap();
			break;
#else
                        lwsl_err("%s: your OpenSSL lacks AES wrap apis, update it\n",
                                 __func__);
                        return -1;
#endif
		case LWS_GAESM_CBC:
			ctx->cipher = EVP_aes_256_cbc();
			break;
#if defined(LWS_HAVE_EVP_aes_256_cfb128)
		case LWS_GAESM_CFB128:
			ctx->cipher = EVP_aes_256_cfb128();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_256_cfb8)
		case LWS_GAESM_CFB8:
			ctx->cipher = EVP_aes_256_cfb8();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ctr)
		case LWS_GAESM_CTR:
			ctx->cipher = EVP_aes_256_ctr();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ecb)
		case LWS_GAESM_ECB:
			ctx->cipher = EVP_aes_256_ecb();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_ofb)
		case LWS_GAESM_OFB:
			ctx->cipher = EVP_aes_256_ofb();
			break;
#endif
#if defined(LWS_HAVE_EVP_aes_128_xts)
		case LWS_GAESM_XTS:
			ctx->cipher = EVP_aes_128_xts();
			break;
#endif
		case LWS_GAESM_GCM:
			ctx->cipher = EVP_aes_256_gcm();
			break;
		default:
			goto bail;
		}
		break;

	case 512 / 8:
		switch (mode) {
#if defined(LWS_HAVE_EVP_aes_128_xts)
		case LWS_GAESM_XTS:
			ctx->cipher = EVP_aes_256_xts();
#endif
			break;
		default:
			goto bail;
		}
	break;

	default:
		lwsl_err("%s: unsupported AES size %d bits\n", __func__,
			 ctx->k->len * 8);
		goto bail;
	}

	switch (ctx->op) {
	case LWS_GAESO_ENC:
		n = EVP_EncryptInit_ex(ctx->ctx, ctx->cipher, ctx->engine,
				       NULL, NULL);
		EVP_CIPHER_CTX_set_padding(ctx->ctx, (int)padding);
		break;
	case LWS_GAESO_DEC:
		n = EVP_DecryptInit_ex(ctx->ctx, ctx->cipher, ctx->engine,
				       NULL, NULL);
		EVP_CIPHER_CTX_set_padding(ctx->ctx, (int)padding);
		break;
	}
	if (!n) {
		lwsl_err("%s: cipher init failed (cipher %p)\n", __func__,
			 ctx->cipher);
		lws_tls_err_describe_clear();
		goto bail;
	}

	return 0;
bail:
	EVP_CIPHER_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	return -1;
}

int
lws_genaes_destroy(struct lws_genaes_ctx *ctx, unsigned char *tag, size_t tlen)
{
	uint8_t buf[256];
	int outl = sizeof(buf), n = 0;

	if (!ctx->ctx)
		return 0;

	if (ctx->init) {
		switch (ctx->op) {
		case LWS_GAESO_ENC:

			if (EVP_EncryptFinal_ex(ctx->ctx, buf, &outl) != 1) {
				lwsl_err("%s: enc final failed\n", __func__);
				n = -1;
			}

			if (ctx->mode == LWS_GAESM_GCM) {
				if (EVP_CIPHER_CTX_ctrl(ctx->ctx,
						EVP_CTRL_GCM_GET_TAG,
						    ctx->taglen, tag) != 1) {
					lwsl_err("get tag ctrl failed\n");
					//lws_tls_err_describe_clear();
					n = 1;
				}
			}
			if (ctx->mode == LWS_GAESM_CBC)
				memcpy(tag, buf, (unsigned int)outl);

			break;

		case LWS_GAESO_DEC:
			if (EVP_DecryptFinal_ex(ctx->ctx, buf, &outl) != 1) {
				lwsl_err("%s: dec final failed\n", __func__);
				lws_tls_err_describe_clear();
				n = -1;
			}

			break;
		}
		if (outl)
			lwsl_debug("%s: final len %d\n", __func__, outl);
	}

	ctx->k = NULL;
	EVP_CIPHER_CTX_free(ctx->ctx);
	ctx->ctx = NULL;

	return n;
}

int
lws_genaes_crypt(struct lws_genaes_ctx *ctx,
		 const uint8_t *in, size_t len, uint8_t *out,
		 uint8_t *iv_or_nonce_ctr_or_data_unit_16,
		 uint8_t *stream_block_16, size_t *nc_or_iv_off, int taglen)
{
	int n = 0, outl, olen;

	if (!ctx->init) {

		EVP_CIPHER_CTX_set_key_length(ctx->ctx, (int)ctx->k->len);

		if (ctx->mode == LWS_GAESM_GCM) {
			n = EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_IVLEN,
					   (int)*nc_or_iv_off, NULL);
			if (n != 1) {
				lwsl_err("%s: SET_IVLEN failed\n", __func__);
				return -1;
			}
			memcpy(ctx->tag, stream_block_16, (unsigned int)taglen);
			ctx->taglen = taglen;
		}

		switch (ctx->op) {
		case LWS_GAESO_ENC:
			n = EVP_EncryptInit_ex(ctx->ctx, NULL, NULL,
					       ctx->k->buf,
					       iv_or_nonce_ctr_or_data_unit_16);
			break;
		case LWS_GAESO_DEC:
			if (ctx->mode == LWS_GAESM_GCM)
				EVP_CIPHER_CTX_ctrl(ctx->ctx,
						    EVP_CTRL_GCM_SET_TAG,
						    ctx->taglen, ctx->tag);
			n = EVP_DecryptInit_ex(ctx->ctx, NULL, NULL,
					       ctx->k->buf,
					       iv_or_nonce_ctr_or_data_unit_16);
			break;
		}

		if (!n) {
			lws_tls_err_describe_clear();
			lwsl_err("%s: init failed (cipher %p)\n",
				 __func__, ctx->cipher);

			return -1;
		}
		ctx->init = 1;
	}

	if (ctx->mode == LWS_GAESM_GCM && !out) {
		/* AAD */

		if (!len)
			return 0;

		switch (ctx->op) {
		case LWS_GAESO_ENC:
			n = EVP_EncryptUpdate(ctx->ctx, NULL, &olen, in, (int)len);
			break;
		case LWS_GAESO_DEC:
			n = EVP_DecryptUpdate(ctx->ctx, NULL, &olen, in, (int)len);
			break;
		default:
			return -1;
		}
		if (n != 1) {
			lwsl_err("%s: set AAD failed\n",  __func__);
			lws_tls_err_describe_clear();
			lwsl_hexdump_err(in, len);
			return -1;
		}

		return 0;
	}

	switch (ctx->op) {
	case LWS_GAESO_ENC:
		n = EVP_EncryptUpdate(ctx->ctx, out, &outl, in, (int)len);
		break;
	case LWS_GAESO_DEC:
		n = EVP_DecryptUpdate(ctx->ctx, out, &outl, in, (int)len);
		break;
	default:
		return -1;
	}

	// lwsl_notice("discarding outl %d\n", (int)outl);

	if (!n) {
		lwsl_notice("%s: update failed\n", __func__);
		lws_tls_err_describe_clear();

		return -1;
	}

	return 0;
}
