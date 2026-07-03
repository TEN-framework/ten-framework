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
 *  lws_genrsa provides an RSA abstraction api in lws that works the
 *  same whether you are using openssl or mbedtls crypto functions underneath.
 */
#include "private-lib-core.h"
#include "private-lib-tls-mbedtls.h"
#include <mbedtls/rsa.h>

void
lws_genrsa_destroy_elements(struct lws_gencrypto_keyelem *el)
{
	int n;

	for (n = 0; n < LWS_GENCRYPTO_RSA_KEYEL_COUNT; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
}

static int mode_map[] = { MBEDTLS_RSA_PKCS_V15, MBEDTLS_RSA_PKCS_V21 };

int
lws_genrsa_create(struct lws_genrsa_ctx *ctx,
		  const struct lws_gencrypto_keyelem *el,
		  struct lws_context *context, enum enum_genrsa_mode mode,
		  enum lws_genhash_types oaep_hashid)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx = lws_zalloc(sizeof(*ctx->ctx), "genrsa");
	if (!ctx->ctx)
		return 1;

	ctx->context = context;
	ctx->mode = mode;

	if (mode >= LGRSAM_COUNT)
		return -1;

#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
	mbedtls_rsa_init(ctx->ctx, mode_map[mode], 0);
#else
	mbedtls_rsa_init(ctx->ctx);
	mbedtls_rsa_set_padding(ctx->ctx, mode_map[mode], 0);
#endif

	ctx->ctx->MBEDTLS_PRIVATE(padding) = mode_map[mode];
	ctx->ctx->MBEDTLS_PRIVATE(hash_id) =
			(int)lws_gencrypto_mbedtls_hash_to_MD_TYPE(oaep_hashid);

	{
		int n;

		mbedtls_mpi *mpi[LWS_GENCRYPTO_RSA_KEYEL_COUNT] = {
			&ctx->ctx->MBEDTLS_PRIVATE(E),
			&ctx->ctx->MBEDTLS_PRIVATE(N),
			&ctx->ctx->MBEDTLS_PRIVATE(D),
			&ctx->ctx->MBEDTLS_PRIVATE(P),
			&ctx->ctx->MBEDTLS_PRIVATE(Q),
			&ctx->ctx->MBEDTLS_PRIVATE(DP),
			&ctx->ctx->MBEDTLS_PRIVATE(DQ),
			&ctx->ctx->MBEDTLS_PRIVATE(QP),
		};

		for (n = 0; n < LWS_GENCRYPTO_RSA_KEYEL_COUNT; n++)
			if (el[n].buf &&
			    mbedtls_mpi_read_binary(mpi[n], el[n].buf,
					    	    el[n].len)) {
				lwsl_notice("mpi load failed\n");
				lws_free_set_NULL(ctx->ctx);

				return -1;
			}

		/* mbedtls... compute missing P & Q */

		if ( el[LWS_GENCRYPTO_RSA_KEYEL_D].len &&
		    !el[LWS_GENCRYPTO_RSA_KEYEL_P].len &&
		    !el[LWS_GENCRYPTO_RSA_KEYEL_Q].len) {
#if defined(LWS_HAVE_mbedtls_rsa_complete)
			if (mbedtls_rsa_complete(ctx->ctx)) {
				lwsl_notice("mbedtls_rsa_complete failed\n");
#else
			{
				lwsl_notice("%s: you have to provide P and Q\n", __func__);
#endif
				lws_free_set_NULL(ctx->ctx);

				return -1;
			}

		}
	}

	ctx->ctx->MBEDTLS_PRIVATE(len) = el[LWS_GENCRYPTO_RSA_KEYEL_N].len;

	return 0;
}

static int
_rngf(void *context, unsigned char *buf, size_t len)
{
	if ((size_t)lws_get_random(context, buf, len) == len)
		return 0;

	return -1;
}

int
lws_genrsa_new_keypair(struct lws_context *context, struct lws_genrsa_ctx *ctx,
		       enum enum_genrsa_mode mode, struct lws_gencrypto_keyelem *el,
		       int bits)
{
	int n;

	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx = lws_zalloc(sizeof(*ctx->ctx), "genrsa");
	if (!ctx->ctx)
		return -1;

	ctx->context = context;
	ctx->mode = mode;

	if (mode >= LGRSAM_COUNT)
		return -1;

#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
	mbedtls_rsa_init(ctx->ctx, mode_map[mode], 0);
#else
	mbedtls_rsa_init(ctx->ctx);
	mbedtls_rsa_set_padding(ctx->ctx, mode_map[mode], 0);
#endif

	n = mbedtls_rsa_gen_key(ctx->ctx, _rngf, context, (unsigned int)bits, 65537);
	if (n) {
		lwsl_err("mbedtls_rsa_gen_key failed 0x%x\n", -n);
		goto cleanup_1;
	}

	{
		mbedtls_mpi *mpi[LWS_GENCRYPTO_RSA_KEYEL_COUNT] = {
			&ctx->ctx->MBEDTLS_PRIVATE(E),
			&ctx->ctx->MBEDTLS_PRIVATE(N),
			&ctx->ctx->MBEDTLS_PRIVATE(D),
			&ctx->ctx->MBEDTLS_PRIVATE(P),
			&ctx->ctx->MBEDTLS_PRIVATE(Q),
			&ctx->ctx->MBEDTLS_PRIVATE(DP),
			&ctx->ctx->MBEDTLS_PRIVATE(DQ),
			&ctx->ctx->MBEDTLS_PRIVATE(QP),
		};

		for (n = 0; n < LWS_GENCRYPTO_RSA_KEYEL_COUNT; n++)
			if (mpi[n] && mbedtls_mpi_size(mpi[n])) {
				el[n].buf = lws_malloc(
					mbedtls_mpi_size(mpi[n]), "genrsakey");
				if (!el[n].buf)
					goto cleanup;
				el[n].len = (uint32_t)mbedtls_mpi_size(mpi[n]);
				if (mbedtls_mpi_write_binary(mpi[n], el[n].buf,
							 el[n].len))
					goto cleanup;
			}
	}

	return 0;

cleanup:
	for (n = 0; n < LWS_GENCRYPTO_RSA_KEYEL_COUNT; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
cleanup_1:
	lws_free(ctx->ctx);

	return -1;
}

int
lws_genrsa_public_decrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			  size_t in_len, uint8_t *out, size_t out_max)
{
	size_t olen = 0;
	int n;

	ctx->ctx->MBEDTLS_PRIVATE(len) = in_len;

#if defined(LWS_HAVE_mbedtls_rsa_complete)
	mbedtls_rsa_complete(ctx->ctx);
#endif

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = mbedtls_rsa_rsaes_pkcs1_v15_decrypt(ctx->ctx, _rngf,
							ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
							MBEDTLS_RSA_PUBLIC,
#endif
							&olen, in, out,
							out_max);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		n = mbedtls_rsa_rsaes_oaep_decrypt(ctx->ctx, _rngf,
						   ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
							MBEDTLS_RSA_PUBLIC,
#endif
						   NULL, 0,
						   &olen, in, out, out_max);
		break;
	default:
		return -1;
	}
	if (n) {
		lwsl_notice("%s: -0x%x\n", __func__, -n);

		return -1;
	}

	return (int)olen;
}

int
lws_genrsa_private_decrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   size_t in_len, uint8_t *out, size_t out_max)
{
	size_t olen = 0;
	int n;

	ctx->ctx->MBEDTLS_PRIVATE(len) = in_len;

#if defined(LWS_HAVE_mbedtls_rsa_complete)
	mbedtls_rsa_complete(ctx->ctx);
#endif

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = mbedtls_rsa_rsaes_pkcs1_v15_decrypt(ctx->ctx, _rngf,
							ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
							MBEDTLS_RSA_PRIVATE,
#endif
							&olen, in, out,
							out_max);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		n = mbedtls_rsa_rsaes_oaep_decrypt(ctx->ctx, _rngf,
						   ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
						   MBEDTLS_RSA_PRIVATE,
#endif
						   NULL, 0,
						   &olen, in, out, out_max);
		break;
	default:
		return -1;
	}
	if (n) {
		lwsl_notice("%s: -0x%x\n", __func__, -n);

		return -1;
	}

	return (int)olen;
}

int
lws_genrsa_public_encrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			  size_t in_len, uint8_t *out)
{
	int n;

#if defined(LWS_HAVE_mbedtls_rsa_complete)
	mbedtls_rsa_complete(ctx->ctx);
#endif

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = mbedtls_rsa_rsaes_pkcs1_v15_encrypt(ctx->ctx, _rngf,
							ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
							MBEDTLS_RSA_PUBLIC,
#endif
							in_len, in, out);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		n = mbedtls_rsa_rsaes_oaep_encrypt(ctx->ctx, _rngf,
						   ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
						   MBEDTLS_RSA_PUBLIC,
#endif
						   NULL, 0,
						   in_len, in, out);
		break;
	default:
		return -1;
	}
	if (n < 0) {
		lwsl_notice("%s: -0x%x: in_len: %d\n", __func__, -n,
				(int)in_len);

		return -1;
	}

	return (int)mbedtls_mpi_size(&ctx->ctx->MBEDTLS_PRIVATE(N));
}

int
lws_genrsa_private_encrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   size_t in_len, uint8_t *out)
{
	int n;

#if defined(LWS_HAVE_mbedtls_rsa_complete)
	mbedtls_rsa_complete(ctx->ctx);
#endif

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = mbedtls_rsa_rsaes_pkcs1_v15_encrypt(ctx->ctx, _rngf,
							ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
							MBEDTLS_RSA_PRIVATE,
#endif
							in_len, in, out);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		n = mbedtls_rsa_rsaes_oaep_encrypt(ctx->ctx, _rngf,
						   ctx->context,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
						   MBEDTLS_RSA_PRIVATE,
#endif
						   NULL, 0,
						   in_len, in, out);
		break;
	default:
		return -1;
	}
	if (n) {
		lwsl_notice("%s: -0x%x: in_len: %d\n", __func__, -n,
				(int)in_len);

		return -1;
	}

	return (int)mbedtls_mpi_size(&ctx->ctx->MBEDTLS_PRIVATE(N));
}

int
lws_genrsa_hash_sig_verify(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			 enum lws_genhash_types hash_type, const uint8_t *sig,
			 size_t sig_len)
{
	int n, h = (int)lws_gencrypto_mbedtls_hash_to_MD_TYPE(hash_type);

	if (h < 0)
		return -1;

#if defined(LWS_HAVE_mbedtls_rsa_complete)
	mbedtls_rsa_complete(ctx->ctx);
#endif

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = mbedtls_rsa_rsassa_pkcs1_v15_verify(ctx->ctx,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
							NULL, NULL,
							MBEDTLS_RSA_PUBLIC,
#endif
							(mbedtls_md_type_t)h,
							(unsigned int)lws_genhash_size(hash_type),
							in, sig);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		n = mbedtls_rsa_rsassa_pss_verify(ctx->ctx,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
						  NULL, NULL,
						  MBEDTLS_RSA_PUBLIC,
#endif
						  (mbedtls_md_type_t)h,
						  (unsigned int)lws_genhash_size(hash_type),
						  in, sig);
		break;
	default:
		return -1;
	}
	if (n < 0) {
		lwsl_notice("%s: (mode %d) -0x%x\n", __func__, ctx->mode, -n);

		return -1;
	}

	return n;
}

int
lws_genrsa_hash_sign(struct lws_genrsa_ctx *ctx, const uint8_t *in,
		       enum lws_genhash_types hash_type, uint8_t *sig,
		       size_t sig_len)
{
	int n, h = (int)lws_gencrypto_mbedtls_hash_to_MD_TYPE(hash_type);

	if (h < 0)
		return -1;

#if defined(LWS_HAVE_mbedtls_rsa_complete)
	mbedtls_rsa_complete(ctx->ctx);
#endif

	/*
	 * The "sig" buffer must be as large as the size of ctx->N
	 * (eg. 128 bytes if RSA-1024 is used).
	 */
	if (sig_len < ctx->ctx->MBEDTLS_PRIVATE(len))
		return -1;

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = mbedtls_rsa_rsassa_pkcs1_v15_sign(ctx->ctx,
						      mbedtls_ctr_drbg_random,
						      &ctx->context->mcdc,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
						      MBEDTLS_RSA_PRIVATE,
#endif
						      (mbedtls_md_type_t)h,
						      (unsigned int)lws_genhash_size(hash_type),
						      in, sig);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		n = mbedtls_rsa_rsassa_pss_sign(ctx->ctx,
						mbedtls_ctr_drbg_random,
						&ctx->context->mcdc,
#if !defined(MBEDTLS_VERSION_NUMBER) || MBEDTLS_VERSION_NUMBER < 0x03000000
						MBEDTLS_RSA_PRIVATE,
#endif
						(mbedtls_md_type_t)h,
						(unsigned int)lws_genhash_size(hash_type),
						in, sig);
		break;
	default:
		return -1;
	}

	if (n < 0) {
		lwsl_notice("%s: -0x%x\n", __func__, -n);

		return -1;
	}

	return (int)ctx->ctx->MBEDTLS_PRIVATE(len);
}

int
lws_genrsa_render_pkey_asn1(struct lws_genrsa_ctx *ctx, int _private,
			    uint8_t *pkey_asn1, size_t pkey_asn1_len)
{
	uint8_t *p = pkey_asn1, *totlen, *end = pkey_asn1 + pkey_asn1_len - 1;
	mbedtls_mpi *mpi[LWS_GENCRYPTO_RSA_KEYEL_COUNT] = {
		&ctx->ctx->MBEDTLS_PRIVATE(N),
		&ctx->ctx->MBEDTLS_PRIVATE(E),
		&ctx->ctx->MBEDTLS_PRIVATE(D),
		&ctx->ctx->MBEDTLS_PRIVATE(P),
		&ctx->ctx->MBEDTLS_PRIVATE(Q),
		&ctx->ctx->MBEDTLS_PRIVATE(DP),
		&ctx->ctx->MBEDTLS_PRIVATE(DQ),
		&ctx->ctx->MBEDTLS_PRIVATE(QP),
	};
	int n;

	/* 30 82  - sequence
	 *   09 29  <-- length(0x0929) less 4 bytes
	 * 02 01 <- length (1)
	 *  00
	 * 02 82
	 *  02 01 <- length (513)  N
	 *  ...
	 *
	 *  02 03 <- length (3) E
	 *    01 00 01
	 *
	 * 02 82
	 *   02 00 <- length (512) D P Q EXP1 EXP2 COEFF
	 *
	 *  */

	*p++ = 0x30;
	*p++ = 0x82;
	totlen = p;
	p += 2;

	*p++ = 0x02;
	*p++ = 0x01;
	*p++ = 0x00;

	for (n = 0; n < LWS_GENCRYPTO_RSA_KEYEL_COUNT; n++) {
		int m = (int)mbedtls_mpi_size(mpi[n]);
		uint8_t *elen;

		*p++ = 0x02;
		elen = p;
		if (m < 0x7f)
			*p++ = (uint8_t)m;
		else {
			*p++ = 0x82;
			*p++ = (uint8_t)(m >> 8);
			*p++ = (uint8_t)(m & 0xff);
		}

		if (p + m > end)
			return -1;

		if (mbedtls_mpi_write_binary(mpi[n], p, (unsigned int)m))
			return -1;
		if (p[0] & 0x80) {
			p[0] = 0x00;
			if (mbedtls_mpi_write_binary(mpi[n], &p[1], (unsigned int)m))
				return -1;
			m++;
		}
		if (m < 0x7f)
			*elen = (uint8_t)m;
		else {
			*elen++ = 0x82;
			*elen++ = (uint8_t)(m >> 8);
			*elen = (uint8_t)(m & 0xff);
		}
		p += m;
	}

	n = lws_ptr_diff(p, pkey_asn1);

	*totlen++ = (uint8_t)((n - 4) >> 8);
	*totlen = (uint8_t)((n - 4) & 0xff);

	return n;
}

void
lws_genrsa_destroy(struct lws_genrsa_ctx *ctx)
{
	if (!ctx->ctx)
		return;
	mbedtls_rsa_free(ctx->ctx);
	lws_free(ctx->ctx);
	ctx->ctx = NULL;
}
