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
#include "private-lib-tls-openssl.h"

/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

void
lws_genrsa_destroy_elements(struct lws_gencrypto_keyelem *el)
{
	lws_gencrypto_destroy_elements(el, LWS_GENCRYPTO_RSA_KEYEL_COUNT);
}

static int mode_map_crypt[] = { RSA_PKCS1_PADDING, RSA_PKCS1_OAEP_PADDING },
	   mode_map_sig[]   = { RSA_PKCS1_PADDING, RSA_PKCS1_PSS_PADDING };

static int
rsa_pkey_wrap(struct lws_genrsa_ctx *ctx, RSA *rsa)
{
	EVP_PKEY *pkey;

	/* we have the RSA object filled up... wrap in a PKEY */

	pkey = EVP_PKEY_new();
	if (!pkey)
		return 1;

	/* bind the PKEY to the RSA key we just prepared */

	if (EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
		lwsl_err("%s: EVP_PKEY_assign_RSA_KEY failed\n", __func__);
		goto bail;
	}

	/* pepare our PKEY_CTX with the PKEY */

	ctx->ctx = EVP_PKEY_CTX_new(pkey, NULL);
	EVP_PKEY_free(pkey);
	pkey = NULL;
	if (!ctx->ctx)
		goto bail;

	return 0;

bail:
	if (pkey)
		EVP_PKEY_free(pkey);

	return 1;
}

int
lws_genrsa_create(struct lws_genrsa_ctx *ctx,
		  const struct lws_gencrypto_keyelem *el,
		  struct lws_context *context, enum enum_genrsa_mode mode,
		  enum lws_genhash_types oaep_hashid)
{
	int n;

	memset(ctx, 0, sizeof(*ctx));
	ctx->context = context;
	ctx->mode = mode;

	/* Step 1:
	 *
	 * convert the MPI for e and n to OpenSSL BIGNUMs
	 */

	for (n = 0; n < 5; n++) {
		ctx->bn[n] = BN_bin2bn(el[n].buf, (int)el[n].len, NULL);
		if (!ctx->bn[n]) {
			lwsl_notice("mpi load failed\n");
			goto bail;
		}
	}

	/* Step 2:
	 *
	 * assemble the OpenSSL RSA from the BIGNUMs
	 */

	ctx->rsa = RSA_new();
	if (!ctx->rsa) {
		lwsl_notice("Failed to create RSA\n");
		goto bail;
	}

#if defined(LWS_HAVE_RSA_SET0_KEY) && !defined(USE_WOLFSSL) 
	if (RSA_set0_key(ctx->rsa, ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_N],
			 ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_E],
			 ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_D]) != 1) {
		lwsl_notice("RSA_set0_key failed\n");
		goto bail;
	}
	RSA_set0_factors(ctx->rsa, ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_P],
				   ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_Q]);
#else
	ctx->rsa->e = ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_E];
	ctx->rsa->n = ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_N];
	ctx->rsa->d = ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_D];
	ctx->rsa->p = ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_P];
	ctx->rsa->q = ctx->bn[LWS_GENCRYPTO_RSA_KEYEL_Q];
#endif

	if (!rsa_pkey_wrap(ctx, ctx->rsa))
		return 0;

bail:
	for (n = 0; n < 5; n++)
		if (ctx->bn[n]) {
			BN_clear_free(ctx->bn[n]);
			ctx->bn[n] = NULL;
		}

	if (ctx->rsa) {
		RSA_free(ctx->rsa);
		ctx->rsa = NULL;
	}

	return 1;
}

int
lws_genrsa_new_keypair(struct lws_context *context, struct lws_genrsa_ctx *ctx,
		       enum enum_genrsa_mode mode, struct lws_gencrypto_keyelem *el,
		       int bits)
{
	BIGNUM *bn;
	int n;

	memset(ctx, 0, sizeof(*ctx));
	ctx->context = context;
	ctx->mode = mode;

	ctx->rsa = RSA_new();
	if (!ctx->rsa) {
		lwsl_notice("Failed to create RSA\n");
		return -1;
	}

	bn = BN_new();
	if (!bn)
		goto cleanup_1;
	if (BN_set_word(bn, RSA_F4) != 1) {
		BN_free(bn);
		goto cleanup_1;
	}

	n = RSA_generate_key_ex(ctx->rsa, bits, bn, NULL);
	BN_clear_free(bn);
	if (n != 1)
		goto cleanup_1;

#if defined(LWS_HAVE_RSA_SET0_KEY) && !defined(USE_WOLFSSL)
	{
		const BIGNUM *mpi[5];

		RSA_get0_key(ctx->rsa, &mpi[LWS_GENCRYPTO_RSA_KEYEL_N],
			     &mpi[LWS_GENCRYPTO_RSA_KEYEL_E], &mpi[LWS_GENCRYPTO_RSA_KEYEL_D]);
		RSA_get0_factors(ctx->rsa, &mpi[LWS_GENCRYPTO_RSA_KEYEL_P],
				 &mpi[LWS_GENCRYPTO_RSA_KEYEL_Q]);
#else
	{
		BIGNUM *mpi[5] = { ctx->rsa->e, ctx->rsa->n, ctx->rsa->d,
				   ctx->rsa->p, ctx->rsa->q, };
#endif
		for (n = 0; n < 5; n++)
			if (BN_num_bytes(mpi[n])) {
				el[n].buf = lws_malloc(
					(unsigned int)BN_num_bytes(mpi[n]), "genrsakey");
				if (!el[n].buf)
					goto cleanup;
				el[n].len = (unsigned int)BN_num_bytes(mpi[n]);
				BN_bn2bin(mpi[n], el[n].buf);
			}
	}

	if (!rsa_pkey_wrap(ctx, ctx->rsa))
		return 0;

cleanup:
	for (n = 0; n < LWS_GENCRYPTO_RSA_KEYEL_COUNT; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
cleanup_1:
	RSA_free(ctx->rsa);
	ctx->rsa = NULL;

	return -1;
}

/*
 * in_len must be less than RSA_size(rsa) - 11 for the PKCS #1 v1.5
 * based padding modes
 */

int
lws_genrsa_public_encrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			  size_t in_len, uint8_t *out)
{
	int n = RSA_public_encrypt((int)in_len, in, out, ctx->rsa,
				   mode_map_crypt[ctx->mode]);
	if (n < 0) {
		lwsl_err("%s: RSA_public_encrypt failed\n", __func__);
		lws_tls_err_describe_clear();
		return -1;
	}

	return n;
}

int
lws_genrsa_private_encrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   size_t in_len, uint8_t *out)
{
	int n = RSA_private_encrypt((int)in_len, in, out, ctx->rsa,
			        mode_map_crypt[ctx->mode]);
	if (n < 0) {
		lwsl_err("%s: RSA_private_encrypt failed\n", __func__);
		lws_tls_err_describe_clear();
		return -1;
	}

	return n;
}

int
lws_genrsa_public_decrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			  size_t in_len, uint8_t *out, size_t out_max)
{
	int n = RSA_public_decrypt((int)in_len, in, out, ctx->rsa,
			       mode_map_crypt[ctx->mode]);
	if (n < 0) {
		lwsl_err("%s: RSA_public_decrypt failed\n", __func__);
		return -1;
	}

	return n;
}

int
lws_genrsa_private_decrypt(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			   size_t in_len, uint8_t *out, size_t out_max)
{
	int n = RSA_private_decrypt((int)in_len, in, out, ctx->rsa,
			        mode_map_crypt[ctx->mode]);
	if (n < 0) {
		lwsl_err("%s: RSA_private_decrypt failed\n", __func__);
		lws_tls_err_describe_clear();
		return -1;
	}

	return n;
}

int
lws_genrsa_hash_sig_verify(struct lws_genrsa_ctx *ctx, const uint8_t *in,
			 enum lws_genhash_types hash_type, const uint8_t *sig,
			 size_t sig_len)
{
	int n = lws_gencrypto_openssl_hash_to_NID(hash_type),
	    h = (int)lws_genhash_size(hash_type);
	const EVP_MD *md = NULL;

	if (n < 0)
		return -1;

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		n = RSA_verify(n, in, (unsigned int)h, (uint8_t *)sig,
			       (unsigned int)sig_len, ctx->rsa);
		break;
	case LGRSAM_PKCS1_OAEP_PSS:
		md = lws_gencrypto_openssl_hash_to_EVP_MD(hash_type);
		if (!md)
			return -1;

#if defined(LWS_HAVE_RSA_verify_pss_mgf1)
		n = RSA_verify_pss_mgf1(ctx->rsa, in, h, md, NULL, -1,
					(uint8_t *)sig,
#else
		n = RSA_verify_PKCS1_PSS(ctx->rsa, in, md, (uint8_t *)sig,
#endif
					 (int)sig_len);
		break;
	default:
		return -1;
	}

	if (n != 1) {
		lwsl_notice("%s: fail\n", __func__);
		lws_tls_err_describe_clear();

		return -1;
	}

	return 0;
}

int
lws_genrsa_hash_sign(struct lws_genrsa_ctx *ctx, const uint8_t *in,
		       enum lws_genhash_types hash_type, uint8_t *sig,
		       size_t sig_len)
{
	int n = lws_gencrypto_openssl_hash_to_NID(hash_type),
	    h = (int)lws_genhash_size(hash_type);
	unsigned int used = 0;
	EVP_MD_CTX *mdctx = NULL;
	const EVP_MD *md = NULL;

	if (n < 0)
		return -1;

	switch(ctx->mode) {
	case LGRSAM_PKCS1_1_5:
		if (RSA_sign(n, in, (unsigned int)h, sig, &used, ctx->rsa) != 1) {
			lwsl_err("%s: RSA_sign failed\n", __func__);

			goto bail;
		}
		break;

	case LGRSAM_PKCS1_OAEP_PSS:

		md = lws_gencrypto_openssl_hash_to_EVP_MD(hash_type);
		if (!md)
			return -1;

		if (EVP_PKEY_CTX_set_rsa_padding(ctx->ctx,
						 mode_map_sig[ctx->mode]) != 1) {
			lwsl_err("%s: set_rsa_padding failed\n", __func__);

			goto bail;
		}

		mdctx = EVP_MD_CTX_create();
		if (!mdctx)
			goto bail;

		if (EVP_DigestSignInit(mdctx, NULL, md, NULL,
#if defined(USE_WOLFSSL)
					ctx->ctx->pkey)) {
#else
				       EVP_PKEY_CTX_get0_pkey(ctx->ctx))) {
#endif
			lwsl_err("%s: EVP_DigestSignInit failed\n", __func__);

			goto bail;
		}
		if (EVP_DigestSignUpdate(mdctx, in, (unsigned int)EVP_MD_size(md))) {
			lwsl_err("%s: EVP_DigestSignUpdate failed\n", __func__);

			goto bail;
		}
		if (EVP_DigestSignFinal(mdctx, sig, &sig_len)) {
			lwsl_err("%s: EVP_DigestSignFinal failed\n", __func__);

			goto bail;
		}
		EVP_MD_CTX_free(mdctx);
		used = (unsigned int)sig_len;
		break;

	default:
		return -1;
	}

	return (int)used;

bail:
	if (mdctx)
		EVP_MD_CTX_free(mdctx);

	return -1;
}

void
lws_genrsa_destroy(struct lws_genrsa_ctx *ctx)
{
	if (!ctx->ctx)
		return;

	EVP_PKEY_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	ctx->rsa = NULL;
}
