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
 *  lws_genec provides an EC abstraction api in lws that works the
 *  same whether you are using openssl or mbedtls crypto functions underneath.
 */
#include "private-lib-core.h"
#include "private-lib-tls-mbedtls.h"

#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
#define ECDHCTX(_c, _ins) _c->u.ctx_ecdh->MBEDTLS_PRIVATE(ctx).\
			MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(_ins)
#define ECDSACTX(_c, _ins) _c->u.ctx_ecdsa->MBEDTLS_PRIVATE(_ins)
#else
#define ECDHCTX(_c, _ins) _c->u.ctx_ecdh->_ins
#define ECDSACTX(_c, _ins) _c->u.ctx_ecdsa->_ins
#endif

const struct lws_ec_curves lws_ec_curves[] = {
	/*
	 * These are the curves we are willing to use by default...
	 *
	 * The 3 recommended+ (P-256) and optional curves in RFC7518 7.6
	 *
	 * Specific keys lengths from RFC8422 p20
	 */
	{ "P-256", MBEDTLS_ECP_DP_SECP256R1, 32 },
	{ "P-384", MBEDTLS_ECP_DP_SECP384R1, 48 },
	{ "P-521", MBEDTLS_ECP_DP_SECP521R1, 66 },

	{ NULL, 0, 0 }
};

static int
lws_genec_keypair_import(struct lws_genec_ctx *ctx, enum enum_lws_dh_side side,
			 const struct lws_gencrypto_keyelem *el)
{
	const struct lws_ec_curves *curve;
	mbedtls_ecp_keypair kp;
	int ret = -1;

	if (el[LWS_GENCRYPTO_EC_KEYEL_CRV].len < 4) {
		lwsl_notice("%s: crv '%s' (%d)\n", __func__,
			    el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf ?
				    (char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf :
					    "null",
			    el[LWS_GENCRYPTO_EC_KEYEL_CRV].len);
		return -21;
	}

	curve = lws_genec_curve(ctx->curve_table,
				(char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf);
	if (!curve)
		return -22;

	/*
	 * d (the private part) may be missing, otherwise it and everything
	 * else must match the expected bignum size
	 */

	if ((el[LWS_GENCRYPTO_EC_KEYEL_D].len &&
	     el[LWS_GENCRYPTO_EC_KEYEL_D].len != curve->key_bytes) ||
	    el[LWS_GENCRYPTO_EC_KEYEL_X].len != curve->key_bytes ||
	    el[LWS_GENCRYPTO_EC_KEYEL_Y].len != curve->key_bytes)
		return -23;

	mbedtls_ecp_keypair_init(&kp);
	if (mbedtls_ecp_group_load(&kp.MBEDTLS_PRIVATE(grp),
				   (mbedtls_ecp_group_id)curve->tls_lib_nid))
		goto bail1;

	ctx->has_private = !!el[LWS_GENCRYPTO_EC_KEYEL_D].len;

	/* d (the private key) is directly an mpi */

	if (ctx->has_private &&
	    mbedtls_mpi_read_binary(&kp.MBEDTLS_PRIVATE(d),
				    el[LWS_GENCRYPTO_EC_KEYEL_D].buf,
				    el[LWS_GENCRYPTO_EC_KEYEL_D].len))
		goto bail1;

	mbedtls_ecp_set_zero(&kp.MBEDTLS_PRIVATE(Q));

	if (mbedtls_mpi_read_binary(&kp.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X),
				    el[LWS_GENCRYPTO_EC_KEYEL_X].buf,
				    el[LWS_GENCRYPTO_EC_KEYEL_X].len))
		goto bail1;

	if (mbedtls_mpi_read_binary(&kp.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y),
				    el[LWS_GENCRYPTO_EC_KEYEL_Y].buf,
				    el[LWS_GENCRYPTO_EC_KEYEL_Y].len))
		goto bail1;

	mbedtls_mpi_lset(&kp.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Z), 1);

	switch (ctx->genec_alg) {
	case LEGENEC_ECDH:
		if (mbedtls_ecdh_get_params(ctx->u.ctx_ecdh, &kp,
					    (mbedtls_ecdh_side)side))
			goto bail1;
		/* verify the key is consistent with the claimed curve */
		if (ctx->has_private &&
		    mbedtls_ecp_check_privkey(&ECDHCTX(ctx, grp),
					      &ECDHCTX(ctx, d)))
			goto bail1;
		if (mbedtls_ecp_check_pubkey(&ECDHCTX(ctx, grp),
					     &ECDHCTX(ctx, Q)))
			goto bail1;
		break;
	case LEGENEC_ECDSA:
		if (mbedtls_ecdsa_from_keypair(ctx->u.ctx_ecdsa, &kp))
			goto bail1;
		/* verify the key is consistent with the claimed curve */
		if (ctx->has_private &&
		    mbedtls_ecp_check_privkey(&ECDSACTX(ctx, grp),
					      &ECDSACTX(ctx, d)))
			goto bail1;
		if (mbedtls_ecp_check_pubkey(&ECDSACTX(ctx, grp),
					     &ECDSACTX(ctx, Q)))
			goto bail1;
		break;
	default:
		goto bail1;
	}

	ret = 0;

bail1:
	mbedtls_ecp_keypair_free(&kp);

	return ret;
}

int
lws_genecdh_create(struct lws_genec_ctx *ctx, struct lws_context *context,
		   const struct lws_ec_curves *curve_table)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->context = context;
	ctx->curve_table = curve_table;
	ctx->genec_alg = LEGENEC_ECDH;

	ctx->u.ctx_ecdh = lws_zalloc(sizeof(*ctx->u.ctx_ecdh), "genecdh");
	if (!ctx->u.ctx_ecdh)
		return 1;

	mbedtls_ecdh_init(ctx->u.ctx_ecdh);

	return 0;
}

int
lws_genecdsa_create(struct lws_genec_ctx *ctx, struct lws_context *context,
		    const struct lws_ec_curves *curve_table)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->context = context;
	ctx->curve_table = curve_table;
	ctx->genec_alg = LEGENEC_ECDSA;

	ctx->u.ctx_ecdsa = lws_zalloc(sizeof(*ctx->u.ctx_ecdsa), "genecdsa");
	if (!ctx->u.ctx_ecdsa)
		return 1;

	mbedtls_ecdsa_init(ctx->u.ctx_ecdsa);

	return 0;
}


int
lws_genecdh_set_key(struct lws_genec_ctx *ctx, struct lws_gencrypto_keyelem *el,
		    enum enum_lws_dh_side side)
{
	if (ctx->genec_alg != LEGENEC_ECDH)
		return -1;

	return lws_genec_keypair_import(ctx, side, el);
}

int
lws_genecdsa_set_key(struct lws_genec_ctx *ctx,
		     const struct lws_gencrypto_keyelem *el)
{
	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	return lws_genec_keypair_import(ctx, 0, el);
}

void
lws_genec_destroy(struct lws_genec_ctx *ctx)
{
	switch (ctx->genec_alg) {
	case LEGENEC_ECDH:
		if (ctx->u.ctx_ecdh) {
			mbedtls_ecdh_free(ctx->u.ctx_ecdh);
			lws_free(ctx->u.ctx_ecdh);
			ctx->u.ctx_ecdh = NULL;
		}
		break;
	case LEGENEC_ECDSA:
		if (ctx->u.ctx_ecdsa) {
			mbedtls_ecdsa_free(ctx->u.ctx_ecdsa);
			lws_free(ctx->u.ctx_ecdsa);
			ctx->u.ctx_ecdsa = NULL;
		}
		break;
	default:
		break;
	}
}

int
lws_genecdh_new_keypair(struct lws_genec_ctx *ctx, enum enum_lws_dh_side side,
			const char *curve_name,
			struct lws_gencrypto_keyelem *el)
{
	const struct lws_ec_curves *curve;
	mbedtls_ecdsa_context ecdsa;
	mbedtls_ecp_keypair *kp;
	mbedtls_mpi *mpi[3];
	int n;

	if (ctx->genec_alg != LEGENEC_ECDH)
		return -1;

	curve = lws_genec_curve(ctx->curve_table, curve_name);
	if (!curve) {
		lwsl_err("%s: curve '%s' not supported\n",
			 __func__, curve_name);

		return -22;
	}

	mbedtls_ecdsa_init(&ecdsa);
	n = mbedtls_ecdsa_genkey(&ecdsa, (mbedtls_ecp_group_id)curve->tls_lib_nid,
				 lws_gencrypto_mbedtls_rngf,
				 ctx->context);
	if (n) {
		lwsl_err("mbedtls_ecdsa_genkey failed 0x%x\n", -n);
		goto bail1;
	}

	kp = (mbedtls_ecp_keypair *)&ecdsa;

	n = mbedtls_ecdh_get_params(ctx->u.ctx_ecdh, kp,
				    (mbedtls_ecdh_side)side);
	if (n) {
		lwsl_err("mbedtls_ecdh_get_params failed 0x%x\n", -n);
		goto bail1;
	}

	/*
	 * we need to capture the individual element BIGNUMs into
	 * lws_gencrypto_keyelem, so they can be serialized, used in jwk etc
	 */

	mpi[0] = &kp->MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X);
	mpi[1] = &kp->MBEDTLS_PRIVATE(d);
	mpi[2] = &kp->MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y);

	el[LWS_GENCRYPTO_EC_KEYEL_CRV].len = (uint32_t)strlen(curve_name) + 1;
	el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf =
			lws_malloc(el[LWS_GENCRYPTO_EC_KEYEL_CRV].len, "ec");
	if (!el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf)
		goto bail1;
	strcpy((char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf, curve_name);

	for (n = LWS_GENCRYPTO_EC_KEYEL_X; n < LWS_GENCRYPTO_EC_KEYEL_COUNT;
	     n++) {
		el[n].len = curve->key_bytes;
		el[n].buf = lws_malloc(curve->key_bytes, "ec");
		if (!el[n].buf)
			goto bail2;

		if (mbedtls_mpi_write_binary(mpi[n - 1], el[n].buf,
					     curve->key_bytes))
			goto bail2;
	}

	mbedtls_ecdsa_free(&ecdsa);

	return 0;

bail2:
	for (n = 0; n < LWS_GENCRYPTO_EC_KEYEL_COUNT; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
bail1:
	mbedtls_ecdsa_free(&ecdsa);

	lws_free_set_NULL(ctx->u.ctx_ecdh);

	return -1;
}

int
lws_genecdsa_new_keypair(struct lws_genec_ctx *ctx, const char *curve_name,
			 struct lws_gencrypto_keyelem *el)
{
	const struct lws_ec_curves *curve;
	mbedtls_ecp_keypair *kp;
	mbedtls_mpi *mpi[3];
	int n;

	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	curve = lws_genec_curve(ctx->curve_table, curve_name);
	if (!curve) {
		lwsl_err("%s: curve '%s' not supported\n",
			 __func__, curve_name);

		return -22;
	}

	//mbedtls_ecdsa_init(ctx->u.ctx_ecdsa);
	n = mbedtls_ecdsa_genkey(ctx->u.ctx_ecdsa, (mbedtls_ecp_group_id)curve->tls_lib_nid,
				 lws_gencrypto_mbedtls_rngf, ctx->context);
	if (n) {
		lwsl_err("mbedtls_ecdsa_genkey failed 0x%x\n", -n);
		goto bail1;
	}

	/*
	 * we need to capture the individual element BIGNUMs into
	 * lws_gencrypto_keyelems, so they can be serialized, used in jwk etc
	 */

	kp = (mbedtls_ecp_keypair *)ctx->u.ctx_ecdsa;

	mpi[0] = &kp->MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X);
	mpi[1] = &kp->MBEDTLS_PRIVATE(d);
	mpi[2] = &kp->MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y);

	el[LWS_GENCRYPTO_EC_KEYEL_CRV].len = (uint32_t)strlen(curve_name) + 1;
	el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf =
			lws_malloc(el[LWS_GENCRYPTO_EC_KEYEL_CRV].len, "ec");
	if (!el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf)
		goto bail1;
	strcpy((char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf, curve_name);

	for (n = LWS_GENCRYPTO_EC_KEYEL_X; n < LWS_GENCRYPTO_EC_KEYEL_COUNT;
	     n++) {
		el[n].len = curve->key_bytes;
		el[n].buf = lws_malloc(curve->key_bytes, "ec");
		if (!el[n].buf)
			goto bail2;


		if (mbedtls_mpi_write_binary(mpi[n - 1], el[n].buf, el[n].len)) {
			lwsl_err("%s: mbedtls_mpi_write_binary failed\n", __func__);
			goto bail2;
		}
	}

	return 0;

bail2:
	for (n = 0; n < LWS_GENCRYPTO_EC_KEYEL_COUNT; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
bail1:

	lws_free_set_NULL(ctx->u.ctx_ecdsa);

	return -1;
}

int
lws_genecdsa_hash_sign_jws(struct lws_genec_ctx *ctx, const uint8_t *in,
			   enum lws_genhash_types hash_type, int keybits,
			   uint8_t *sig, size_t sig_len)
{
	int n, keybytes = lws_gencrypto_bits_to_bytes(keybits);
	size_t hlen = lws_genhash_size(hash_type);
	mbedtls_mpi mpi_r, mpi_s;
	size_t slen = sig_len;

	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	/*
	 * The ECDSA P-256 SHA-256 digital signature is generated as follows:
	 *
	 * 1.  Generate a digital signature of the JWS Signing Input using ECDSA
	 *     P-256 SHA-256 with the desired private key.  The output will be
	 *     the pair (R, S), where R and S are 256-bit unsigned integers.
	 *
	 * 2.  Turn R and S into octet sequences in big-endian order, with each
	 *     array being be 32 octets long.  The octet sequence
	 *     representations MUST NOT be shortened to omit any leading zero
	 *     octets contained in the values.
	 *
	 * 3.  Concatenate the two octet sequences in the order R and then S.
	 *     (Note that many ECDSA implementations will directly produce this
	 *     concatenation as their output.)
	 *
	 * 4.  The resulting 64-octet sequence is the JWS Signature value.
	 */

	mbedtls_mpi_init(&mpi_r);
	mbedtls_mpi_init(&mpi_s);

	n = mbedtls_ecdsa_sign(&ECDSACTX(ctx, grp), &mpi_r, &mpi_s,
			       &ECDSACTX(ctx, d), in, hlen,
			lws_gencrypto_mbedtls_rngf, ctx->context);
	if (n) {
		lwsl_err("%s: mbedtls_ecdsa_sign failed: -0x%x\n",
			 __func__, -n);

		goto bail2;
	}

	if (mbedtls_mpi_write_binary(&mpi_r, sig, (unsigned int)keybytes))
		goto bail2;
	mbedtls_mpi_free(&mpi_r);
	if (mbedtls_mpi_write_binary(&mpi_s, sig + keybytes, (unsigned int)keybytes))
		goto bail1;
	mbedtls_mpi_free(&mpi_s);

	return (int)slen;

bail2:
	mbedtls_mpi_free(&mpi_r);
bail1:
	mbedtls_mpi_free(&mpi_s);

	return -3;
}

int
lws_genecdsa_hash_sig_verify_jws(struct lws_genec_ctx *ctx, const uint8_t *in,
				 enum lws_genhash_types hash_type, int keybits,
				 const uint8_t *sig, size_t sig_len)
{
	int n, keybytes = lws_gencrypto_bits_to_bytes(keybits);
	size_t hlen = lws_genhash_size(hash_type);
	mbedtls_mpi mpi_r, mpi_s;

	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	if ((int)sig_len != keybytes * 2)
		return -1;

	/*
	 * 1.  The JWS Signature value MUST be a 64-octet sequence.  If it is
	 *     not a 64-octet sequence, the validation has failed.
	 *
	 * 2.  Split the 64-octet sequence into two 32-octet sequences.  The
	 *     first octet sequence represents R and the second S.  The values R
	 *     and S are represented as octet sequences using the Integer-to-
	 *     OctetString Conversion defined in Section 2.3.7 of SEC1 [SEC1]
	 *     (in big-endian octet order).
	 *
	 * 3.  Submit the JWS Signing Input, R, S, and the public key (x, y) to
	 *     the ECDSA P-256 SHA-256 validator.
	 */

	mbedtls_mpi_init(&mpi_r);
	mbedtls_mpi_init(&mpi_s);

	if (mbedtls_mpi_read_binary(&mpi_r, sig, (unsigned int)keybytes))
		return -1;
	if (mbedtls_mpi_read_binary(&mpi_s, sig + keybytes, (unsigned int)keybytes))
		goto bail1;

	n = mbedtls_ecdsa_verify(&ECDSACTX(ctx, grp), in, hlen,
				 &ECDSACTX(ctx, Q), &mpi_r, &mpi_s);

	mbedtls_mpi_free(&mpi_s);
	mbedtls_mpi_free(&mpi_r);

	if (n) {
		lwsl_err("%s: mbedtls_ecdsa_verify failed: -0x%x\n",
			 __func__, -n);

		goto bail;
	}

	return 0;
bail1:
	mbedtls_mpi_free(&mpi_r);

bail:

	return -3;
}

int
lws_genecdh_compute_shared_secret(struct lws_genec_ctx *ctx, uint8_t *ss,
				  int *ss_len)
{
	int n;
	size_t st;
	if (mbedtls_ecp_check_pubkey(&ECDHCTX(ctx, grp), &ECDHCTX(ctx, Q)) ||
	    mbedtls_ecp_check_pubkey(&ECDHCTX(ctx, grp), &ECDHCTX(ctx, Qp))) {
		lwsl_err("%s: both sides must be set up\n", __func__);

		return -1;
	}

	n = mbedtls_ecdh_calc_secret(ctx->u.ctx_ecdh, &st, ss, (size_t)*ss_len,
			lws_gencrypto_mbedtls_rngf, ctx->context);
	if (n)
		return -1;

	*ss_len = (int)st;

	return 0;
}
