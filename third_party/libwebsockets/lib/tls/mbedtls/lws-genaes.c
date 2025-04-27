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
 *  lws_genaes provides an abstraction api for AES in lws that works the
 *  same whether you are using openssl or mbedtls hash functions underneath.
 */
#include "private-lib-core.h"
#if defined(LWS_WITH_JOSE)
#include "private-lib-jose.h"
#endif

static int operation_map[] = { MBEDTLS_AES_ENCRYPT, MBEDTLS_AES_DECRYPT };

static unsigned int
_write_pkcs7_pad(uint8_t *p, int len)
{
	unsigned int n = 0, padlen = LWS_AES_CBC_BLOCKLEN * ((unsigned int)len /
					LWS_AES_CBC_BLOCKLEN + 1) - (unsigned int)len;

	p += len;

	while (n++ < padlen)
		*p++ = (uint8_t)padlen;

	return padlen;
}

int
lws_genaes_create(struct lws_genaes_ctx *ctx, enum enum_aes_operation op,
		  enum enum_aes_modes mode, struct lws_gencrypto_keyelem *el,
		  enum enum_aes_padding padding, void *engine)
{
	int n = 0;

	ctx->mode = mode;
	ctx->k = el;
	ctx->op = (enum enum_aes_operation)operation_map[op];
	ctx->underway = 0;
	ctx->padding = padding == LWS_GAESP_WITH_PADDING;

	switch (ctx->mode) {
	case LWS_GAESM_XTS:
#if defined(MBEDTLS_CIPHER_MODE_XTS)
		mbedtls_aes_xts_init(&ctx->u.ctx_xts);
		break;
#else
		return -1;
#endif
	case LWS_GAESM_GCM:
		mbedtls_gcm_init(&ctx->u.ctx_gcm);
		n = mbedtls_gcm_setkey(&ctx->u.ctx_gcm, MBEDTLS_CIPHER_ID_AES,
				       ctx->k->buf, ctx->k->len * 8);
		if (n) {
			lwsl_notice("%s: mbedtls_gcm_setkey: -0x%x\n",
				    __func__, -n);
			return n;
		}
		return n;
	default:
		mbedtls_aes_init(&ctx->u.ctx);
		break;
	}

	switch (op) {
	case LWS_GAESO_ENC:
		if (ctx->mode == LWS_GAESM_XTS)
#if defined(MBEDTLS_CIPHER_MODE_XTS)
			n = mbedtls_aes_xts_setkey_enc(&ctx->u.ctx_xts,
						       ctx->k->buf,
						       ctx->k->len * 8);
#else
			return -1;
#endif
		else
			n = mbedtls_aes_setkey_enc(&ctx->u.ctx, ctx->k->buf,
						   ctx->k->len * 8);
		break;
	case LWS_GAESO_DEC:
		switch (ctx->mode) {
		case LWS_GAESM_XTS:
#if defined(MBEDTLS_CIPHER_MODE_XTS)
			n = mbedtls_aes_xts_setkey_dec(&ctx->u.ctx_xts,
						       ctx->k->buf,
						       ctx->k->len * 8);
			break;
#else
			return -1;
#endif

		case LWS_GAESM_CFB128:
		case LWS_GAESM_CFB8:
		case LWS_GAESM_CTR:
		case LWS_GAESM_OFB:
			n = mbedtls_aes_setkey_enc(&ctx->u.ctx, ctx->k->buf,
						   ctx->k->len * 8);
			break;
		default:
			n = mbedtls_aes_setkey_dec(&ctx->u.ctx, ctx->k->buf,
						   ctx->k->len * 8);
			break;
		}
		break;
	}

	if (n)
		lwsl_notice("%s: setting key: -0x%x\n", __func__, -n);

	return n;
}

int
lws_genaes_destroy(struct lws_genaes_ctx *ctx, unsigned char *tag, size_t tlen)
{
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
	size_t last_len = 0;
	uint8_t last[16];
#endif
	int n;

	if (ctx->mode == LWS_GAESM_GCM) {
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
		n = mbedtls_gcm_finish(&ctx->u.ctx_gcm, last, sizeof(last),
					&last_len, tag, tlen);
#else
		n = mbedtls_gcm_finish(&ctx->u.ctx_gcm, tag, tlen);
#endif

		if (n)
			lwsl_notice("%s: mbedtls_gcm_finish: -0x%x\n",
				    __func__, -n);
		if (tag && ctx->op == MBEDTLS_AES_DECRYPT && !n) {
			if (lws_timingsafe_bcmp(ctx->tag, tag, (unsigned int)ctx->taglen)) {
				lwsl_err("%s: lws_genaes_crypt tag "
					 "mismatch (bad first)\n",
						__func__);
				lwsl_hexdump_notice(tag, tlen);
				lwsl_hexdump_notice(ctx->tag, (unsigned int)ctx->taglen);
				n = -1;
			}
		}
		mbedtls_gcm_free(&ctx->u.ctx_gcm);
		return n;
	}
	if (ctx->mode == LWS_GAESM_XTS)
#if defined(MBEDTLS_CIPHER_MODE_XTS)
		mbedtls_aes_xts_free(&ctx->u.ctx_xts);
#else
		return -1;
#endif
	else
		mbedtls_aes_free(&ctx->u.ctx);

	return 0;
}

#if defined(LWS_HAVE_mbedtls_internal_aes_encrypt)
static int
lws_genaes_rfc3394_wrap(int wrap, int cek_bits, const uint8_t *kek,
			int kek_bits, const uint8_t *in, uint8_t *out)
{
	int n, m, ret = -1, c64 = cek_bits / 64;
	mbedtls_aes_context ctx;
	uint8_t a[8], b[16];

	/*
	 * notice the KEK key used to perform the wrapping or unwrapping is
	 * always the size of the AES key used, eg, A128KW == 128 bits.  The
	 * key being wrapped or unwrapped may be larger and is set by the
	 * 'bits' parameter.
	 *
	 * If it's larger than the KEK key size bits, we iterate over it
	 */

	mbedtls_aes_init(&ctx);

	if (wrap) {
		/*
		 * The inputs to the key wrapping process are the KEK and the
		 * plaintext to be wrapped.  The plaintext consists of n 64-bit
		 * blocks, containing the key data being wrapped.
		 *
		 * Inputs:      Plaintext, n 64-bit values {P1, P2, ..., Pn},
		 *		and Key, K (the KEK).
		 * Outputs:     Ciphertext, (n+1) 64-bit values
		 *		{C0, C1, ..., Cn}.
		 *
		 * The default initial value (IV) is defined to be the
		 * hexadecimal constant:
		 *
		 * A[0] = IV = A6A6A6A6A6A6A6A6
		 */
		memset(out, 0xa6, 8);
		memcpy(out + 8, in, 8 * (unsigned int)c64);
		n = mbedtls_aes_setkey_enc(&ctx, kek, (unsigned int)kek_bits);
	} else {
		/*
		 * 2.2.2 Key Unwrap
		 *
		 * The inputs to the unwrap process are the KEK and (n+1)
		 * 64-bit blocks of ciphertext consisting of previously
		 * wrapped key.  It returns n blocks of plaintext consisting
		 * of the n 64-bit blocks of the decrypted key data.
		 *
		 * Inputs:  Ciphertext, (n+1) 64-bit values {C0, C1, ..., Cn},
		 * and Key, K (the KEK).
		 *
		 * Outputs: Plaintext, n 64-bit values {P1, P2, ..., Pn}.
		 */
		memcpy(a, in, 8);
		memcpy(out, in + 8, 8 * (unsigned int)c64);
		n = mbedtls_aes_setkey_dec(&ctx, kek, (unsigned int)kek_bits);
	}

	if (n < 0) {
		lwsl_err("%s: setkey failed\n", __func__);
		goto bail;
	}

	if (wrap) {
		for (n = 0; n <= 5; n++) {
			uint8_t *r = out + 8;
			for (m = 1; m <= c64; m++) {
				memcpy(b, out, 8);
				memcpy(b + 8, r, 8);
				if (mbedtls_internal_aes_encrypt(&ctx, b, b))
					goto bail;

				memcpy(out, b, 8);
				out[7] ^= (uint8_t)(c64 * n + m);
				memcpy(r, b + 8, 8);
				r += 8;
			}
		}
		ret = 0;
	} else {
		/*
		 *
		 */
		for (n = 5; n >= 0; n--) {
			uint8_t *r = out + (c64 - 1) * 8;
			for (m = c64; m >= 1; m--) {
				memcpy(b, a, 8);
				b[7] ^= (uint8_t)(c64 * n + m);
				memcpy(b + 8, r, 8);
				if (mbedtls_internal_aes_decrypt(&ctx, b, b))
					goto bail;

				memcpy(a, b, 8);
				memcpy(r, b + 8, 8);
				r -= 8;
			}
		}

		ret = 0;
		for (n = 0; n < 8; n++)
			if (a[n] != 0xa6)
				ret = -1;
	}

bail:
	if (ret)
		lwsl_notice("%s: failed\n", __func__);
	mbedtls_aes_free(&ctx);

	return ret;
}
#endif

int
lws_genaes_crypt(struct lws_genaes_ctx *ctx, const uint8_t *in, size_t len,
		 uint8_t *out, uint8_t *iv_or_nonce_ctr_or_data_unit_16,
		 uint8_t *stream_block_16, size_t *nc_or_iv_off, int taglen)
{
	uint8_t iv[LWS_JWE_AES_IV_BYTES], sb[16];
	int n = 0;

	switch (ctx->mode) {
	case LWS_GAESM_KW:
#if defined(LWS_HAVE_mbedtls_internal_aes_encrypt)
		/* a key of length ctx->k->len is wrapped by a 128-bit KEK */
		n = lws_genaes_rfc3394_wrap(ctx->op == MBEDTLS_AES_ENCRYPT,
				(ctx->op == MBEDTLS_AES_ENCRYPT ? (int)len * 8 :
						((int)len - 8) * 8), ctx->k->buf,
						(int)ctx->k->len * 8,
				in, out);
		break;
#else
		lwsl_err("%s: your mbedtls is too old\n", __func__);
		return -1;
#endif
	case LWS_GAESM_CBC:
		memcpy(iv, iv_or_nonce_ctr_or_data_unit_16, 16);

		/*
		 * If encrypting, we do the PKCS#7 padding.
		 * During decryption, the caller will need to unpad.
		 */
		if (ctx->padding && ctx->op == MBEDTLS_AES_ENCRYPT) {
			/*
			 * Since we don't want to burden the caller with
			 * the over-allocation at the end of the input,
			 * we have to allocate a temp with space for it
			 */
			uint8_t *padin = (uint8_t *)lws_malloc(
				lws_gencrypto_padded_length(LWS_AES_CBC_BLOCKLEN, len),
								__func__);

			if (!padin)
				return -1;

			memcpy(padin, in, len);
			len += _write_pkcs7_pad((uint8_t *)padin, (int)len);
			n = mbedtls_aes_crypt_cbc(&ctx->u.ctx, (int)ctx->op, len, iv,
						  padin, out);
			lws_free(padin);
		} else
			n = mbedtls_aes_crypt_cbc(&ctx->u.ctx, (int)ctx->op, len, iv,
                                      in, out);

		break;

	case LWS_GAESM_CFB128:
		memcpy(iv, iv_or_nonce_ctr_or_data_unit_16, 16);
		n = mbedtls_aes_crypt_cfb128(&ctx->u.ctx, (int)ctx->op, len,
					     nc_or_iv_off, iv, in, out);
		break;

	case LWS_GAESM_CFB8:
		memcpy(iv, iv_or_nonce_ctr_or_data_unit_16, 16);
		n = mbedtls_aes_crypt_cfb8(&ctx->u.ctx, (int)ctx->op, len, iv,
					   in, out);
		break;

	case LWS_GAESM_CTR:
		memcpy(iv, iv_or_nonce_ctr_or_data_unit_16, 16);
		memcpy(sb, stream_block_16, 16);
		n = mbedtls_aes_crypt_ctr(&ctx->u.ctx, len, nc_or_iv_off,
					  iv, sb, in, out);
		memcpy(iv_or_nonce_ctr_or_data_unit_16, iv, 16);
		memcpy(stream_block_16, sb, 16);
		break;

	case LWS_GAESM_ECB:
		n = mbedtls_aes_crypt_ecb(&ctx->u.ctx, (int)ctx->op, in, out);
		break;

	case LWS_GAESM_OFB:
#if defined(MBEDTLS_CIPHER_MODE_OFB)
		memcpy(iv, iv_or_nonce_ctr_or_data_unit_16, 16);
		n = mbedtls_aes_crypt_ofb(&ctx->u.ctx, len, nc_or_iv_off, iv,
					  in, out);
		break;
#else
		return -1;
#endif

	case LWS_GAESM_XTS:
#if defined(MBEDTLS_CIPHER_MODE_XTS)
		memcpy(iv, iv_or_nonce_ctr_or_data_unit_16, 16);
		n = mbedtls_aes_crypt_xts(&ctx->u.ctx_xts, (int)ctx->op, len, iv,
					  in, out);
		break;
#else
		return -1;
#endif
	case LWS_GAESM_GCM:
		if (!ctx->underway) {
			ctx->underway = 1;

			memcpy(ctx->tag, stream_block_16, (unsigned int)taglen);
			ctx->taglen = taglen;

			/*
			 * iv:                   iv_or_nonce_ctr_or_data_unit_16
			 * iv_len:               *nc_or_iv_off
			 * stream_block_16:      pointer to tag
			 * additional data:      in
			 * additional data len:  len
			 */

#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
			n = mbedtls_gcm_starts(&ctx->u.ctx_gcm, (int)ctx->op,
					       iv_or_nonce_ctr_or_data_unit_16,
					       *nc_or_iv_off);
			if (!n)
				n = mbedtls_gcm_update_ad(&ctx->u.ctx_gcm,
							  in, len);
#else
			n = mbedtls_gcm_starts(&ctx->u.ctx_gcm, (int)ctx->op,
					       iv_or_nonce_ctr_or_data_unit_16,
					       *nc_or_iv_off, in, len);
#endif
			if (n) {
				lwsl_notice("%s: mbedtls_gcm_starts: -0x%x\n",
					    __func__, -n);

				return -1;
			}
			break;
		}

#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
		{
			size_t al;

			n = mbedtls_gcm_update(&ctx->u.ctx_gcm, in, len, out, len, &al);
		}
#else
		n = mbedtls_gcm_update(&ctx->u.ctx_gcm, len, in, out);
#endif
		if (n) {
			lwsl_notice("%s: mbedtls_gcm_update: -0x%x\n",
				    __func__, -n);

			return -1;
		}
		break;
	}

	if (n) {
		lwsl_notice("%s: failed: -0x%x, len %d\n", __func__, -n, (int)len);

		return -1;
	}

	return 0;
}
