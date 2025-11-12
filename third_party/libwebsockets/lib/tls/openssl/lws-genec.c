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
#include "private-lib-tls-openssl.h"

#if !defined(OPENSSL_NO_EC) && defined(LWS_HAVE_EC_KEY_new_by_curve_name) && \
    (OPENSSL_VERSION_NUMBER >= 0x30000000l) && \
     !defined(LWS_SUPPRESS_DEPRECATED_API_WARNINGS)
/* msvc doesn't have #warning... */
#error "You probably need LWS_SUPPRESS_DEPRECATED_API_WARNINGS"
#endif

#if defined(USE_WOLFSSL)
#include "openssl/ecdh.h"
#endif

/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

#if defined(USE_WOLFSSL)
EVP_PKEY * EVP_PKEY_CTX_get0_pkey(EVP_PKEY_CTX *p)
{
	return p->pkey;
}
#endif

#if !defined(LWS_HAVE_ECDSA_SIG_set0)
static void
ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps)
{
    if (pr != NULL)
        *pr = sig->r;
    if (ps != NULL)
        *ps = sig->s;
}

static int
ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
	if (r == NULL || s == NULL)
		return 0;
	BN_clear_free(sig->r);
	BN_clear_free(sig->s);
	sig->r = r;
	sig->s = s;

	return 1;
}
#endif
#if !defined(LWS_HAVE_BN_bn2binpad)
int BN_bn2binpad(const BIGNUM *a, unsigned char *to, int tolen)
{
    int i;
#if !defined(USE_WOLFSSL)
    BN_ULONG l;
#endif

#if !defined(LIBRESSL_VERSION_NUMBER) && !defined(USE_WOLFSSL)
    bn_check_top(a);
#endif
    i = BN_num_bytes(a);

    /* Add leading zeroes if necessary */
    if (tolen > i) {
        memset(to, 0, (size_t)(tolen - i));
        to += tolen - i;
    }
#if defined(USE_WOLFSSL)
    BN_bn2bin(a, to);
#else
    while (i--) {
        l = a->d[i / BN_BYTES];
        *(to++) = (unsigned char)(l >> (8 * (i % BN_BYTES))) & 0xff;
    }
#endif
    return tolen;
}
#endif

const struct lws_ec_curves lws_ec_curves[4] = {
	/*
	 * These are the curves we are willing to use by default...
	 *
	 * The 3 recommended+ (P-256) and optional curves in RFC7518 7.6
	 *
	 * Specific keys lengths from RFC8422 p20
	 */
	{ "P-256", NID_X9_62_prime256v1, 32 },
	{ "P-384", NID_secp384r1,	 48 },
	{ "P-521", NID_secp521r1,	 66 },

	{ NULL, 0, 0 }
};

static int
lws_genec_eckey_import(int nid, EVP_PKEY *pkey,
		       const struct lws_gencrypto_keyelem *el)
{
	EC_KEY *ec = EC_KEY_new_by_curve_name(nid);
	BIGNUM *bn_d, *bn_x, *bn_y;
	int n;

	if (!ec)
		return -1;

	/*
	 * EC_KEY contains
	 *
	 * EC_GROUP * 	group
	 * EC_POINT * 	pub_key
	 * BIGNUM * 	priv_key  (ie, d)
	 */

	bn_x = BN_bin2bn(el[LWS_GENCRYPTO_EC_KEYEL_X].buf,
			 (int)el[LWS_GENCRYPTO_EC_KEYEL_X].len, NULL);
	if (!bn_x) {
		lwsl_err("%s: BN_bin2bn (x) fail\n", __func__);
		goto bail;
	}
	bn_y = BN_bin2bn(el[LWS_GENCRYPTO_EC_KEYEL_Y].buf,
			(int)el[LWS_GENCRYPTO_EC_KEYEL_Y].len, NULL);
	if (!bn_y) {
		lwsl_err("%s: BN_bin2bn (y) fail\n", __func__);
		goto bail1;
	}

	/*
	 * EC_KEY_set_public_key_affine_coordinates sets the public key for
	 * key based on its affine co-ordinates, i.e. it constructs an
	 * EC_POINT object based on the supplied x and y values and sets
	 * the public key to be this EC_POINT. It will also performs
	 * certain sanity checks on the key to confirm that it is valid.
	 */

#if defined(USE_WOLFSSL)
	n = wolfSSL_EC_POINT_set_affine_coordinates_GFp(ec->group,
                                                ec->pub_key,
                                                bn_x, bn_y,
                                                NULL);
#else
	n = EC_KEY_set_public_key_affine_coordinates(ec, bn_x, bn_y);
#endif
	BN_free(bn_x);
	BN_free(bn_y);
	if (n != 1) {
		lwsl_err("%s: EC_KEY_set_public_key_affine_coordinates fail:\n",
			 __func__);
		lws_tls_err_describe_clear();
		goto bail;
	}

	if (el[LWS_GENCRYPTO_EC_KEYEL_D].len) {
		bn_d = BN_bin2bn(el[LWS_GENCRYPTO_EC_KEYEL_D].buf,
				(int)el[LWS_GENCRYPTO_EC_KEYEL_D].len, NULL);
		if (!bn_d) {
			lwsl_err("%s: BN_bin2bn (d) fail\n", __func__);
			goto bail;
		}

		n = EC_KEY_set_private_key(ec, bn_d);
		BN_clear_free(bn_d);
		if (n != 1) {
			lwsl_err("%s: EC_KEY_set_private_key fail\n", __func__);
			goto bail;
		}
	}

	/* explicitly confirm the key pieces are consistent */

#if !defined(USE_WOLFSSL)
	if (EC_KEY_check_key(ec) != 1) {
		lwsl_err("%s: EC_KEY_set_private_key fail\n", __func__);
		goto bail;
	}
#endif

	n = EVP_PKEY_assign_EC_KEY(pkey, ec);
	if (n != 1) {
		lwsl_err("%s: EVP_PKEY_set1_EC_KEY failed\n", __func__);
		return -1;
	}

	return 0;

bail1:
	BN_free(bn_x);
bail:
	EC_KEY_free(ec);

	return -1;
}

static int
lws_genec_keypair_import(struct lws_genec_ctx *ctx,
			 const struct lws_ec_curves *curve_table,
			 EVP_PKEY_CTX **pctx,
			 const struct lws_gencrypto_keyelem *el)
{
	EVP_PKEY *pkey = NULL;
	const struct lws_ec_curves *curve;

	if (el[LWS_GENCRYPTO_EC_KEYEL_CRV].len < 4)
		return -2;

	curve = lws_genec_curve(curve_table,
				(char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf);
	if (!curve)
		return -3;

	if ((el[LWS_GENCRYPTO_EC_KEYEL_D].len &&
	     el[LWS_GENCRYPTO_EC_KEYEL_D].len != curve->key_bytes) ||
	    el[LWS_GENCRYPTO_EC_KEYEL_X].len != curve->key_bytes ||
	    el[LWS_GENCRYPTO_EC_KEYEL_Y].len != curve->key_bytes)
		return -4;

	ctx->has_private = !!el[LWS_GENCRYPTO_EC_KEYEL_D].len;

	pkey = EVP_PKEY_new();
	if (!pkey)
		return -7;

	if (lws_genec_eckey_import(curve->tls_lib_nid, pkey, el)) {
		lwsl_err("%s: lws_genec_eckey_import fail\n", __func__);
		goto bail;
	}

	*pctx = EVP_PKEY_CTX_new(pkey, NULL);
	EVP_PKEY_free(pkey);
	pkey = NULL;

	if (!*pctx)
		goto bail;

	return 0;

bail:
	if (pkey)
		EVP_PKEY_free(pkey);

	if (*pctx) {
		EVP_PKEY_CTX_free(*pctx);
		*pctx = NULL;
	}

	return -9;
}

int
lws_genecdh_create(struct lws_genec_ctx *ctx, struct lws_context *context,
		   const struct lws_ec_curves *curve_table)
{
	ctx->context = context;
	ctx->ctx[0] = NULL;
	ctx->ctx[1] = NULL;
	ctx->curve_table = curve_table;
	ctx->genec_alg = LEGENEC_ECDH;

	return 0;
}

int
lws_genecdsa_create(struct lws_genec_ctx *ctx, struct lws_context *context,
		    const struct lws_ec_curves *curve_table)
{
	ctx->context = context;
	ctx->ctx[0] = NULL;
	ctx->ctx[1] = NULL;
	ctx->curve_table = curve_table;
	ctx->genec_alg = LEGENEC_ECDSA;

	return 0;
}

int
lws_genecdh_set_key(struct lws_genec_ctx *ctx, struct lws_gencrypto_keyelem *el,
		    enum enum_lws_dh_side side)
{
	if (ctx->genec_alg != LEGENEC_ECDH)
		return -1;

	return lws_genec_keypair_import(ctx, ctx->curve_table, &ctx->ctx[side], el);
}

int
lws_genecdsa_set_key(struct lws_genec_ctx *ctx,
		     const struct lws_gencrypto_keyelem *el)
{
	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	return lws_genec_keypair_import(ctx, ctx->curve_table, &ctx->ctx[0], el);
}

static void
lws_genec_keypair_destroy(EVP_PKEY_CTX **pctx)
{
	if (!*pctx)
		return;

//	lwsl_err("%p\n", EVP_PKEY_get1_EC_KEY(EVP_PKEY_CTX_get0_pkey(*pctx)));

//	EC_KEY_free(EVP_PKEY_get1_EC_KEY(EVP_PKEY_CTX_get0_pkey(*pctx)));

	EVP_PKEY_CTX_free(*pctx);
	*pctx = NULL;
}

void
lws_genec_destroy(struct lws_genec_ctx *ctx)
{
	if (ctx->ctx[0])
		lws_genec_keypair_destroy(&ctx->ctx[0]);
	if (ctx->ctx[1])
		lws_genec_keypair_destroy(&ctx->ctx[1]);
}

static int
lws_genec_new_keypair(struct lws_genec_ctx *ctx, enum enum_lws_dh_side side,
		      const char *curve_name, struct lws_gencrypto_keyelem *el)
{
	const struct lws_ec_curves *curve;
	const EC_POINT *pubkey;
	EVP_PKEY *pkey = NULL;
	int ret = -29, n, m;
	BIGNUM *bn[3];
	EC_KEY *ec;

	curve = lws_genec_curve(ctx->curve_table, curve_name);
	if (!curve) {
		lwsl_err("%s: curve '%s' not supported\n",
			 __func__, curve_name);

		return -22;
	}

	ec = EC_KEY_new_by_curve_name(curve->tls_lib_nid);
	if (!ec) {
		lwsl_err("%s: unknown nid %d\n", __func__, curve->tls_lib_nid);
		return -23;
	}

	if (EC_KEY_generate_key(ec) != 1) {
		lwsl_err("%s: EC_KEY_generate_key failed\n", __func__);
		goto bail;
	}

	pkey = EVP_PKEY_new();
	if (!pkey)
		goto bail;

	if (EVP_PKEY_set1_EC_KEY(pkey, ec) != 1) {
		lwsl_err("%s: EVP_PKEY_assign_EC_KEY failed\n", __func__);
		goto bail1;
	}

	ctx->ctx[side] = EVP_PKEY_CTX_new(pkey, NULL);
	if (!ctx->ctx[side]) {
		lwsl_err("%s: EVP_PKEY_CTX_new failed\n", __func__);
		goto bail1;
	}

	/*
	 * we need to capture the individual element BIGNUMs into
	 * lws_gencrypto_keyelem, so they can be serialized, used in jwk etc
	 */

	pubkey = EC_KEY_get0_public_key(ec);
	if (!pubkey) {
		lwsl_err("%s: EC_KEY_get0_public_key failed\n", __func__);
		goto bail1;
	}

	bn[0] = BN_new();
	bn[1] = (BIGNUM *)EC_KEY_get0_private_key(ec);
	bn[2] = BN_new();

#if defined(LWS_HAVE_EC_POINT_get_affine_coordinates)
	if (EC_POINT_get_affine_coordinates(EC_KEY_get0_group(ec),
#else
	if (EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec),
#endif
		        pubkey, bn[0], bn[2], NULL) != 1) {
		lwsl_err("%s: EC_POINT_get_affine_coordinates_GFp failed\n",
			 __func__);
		goto bail2;
	}

	el[LWS_GENCRYPTO_EC_KEYEL_CRV].len = (uint32_t)strlen(curve_name) + 1;
	el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf =
			lws_malloc(el[LWS_GENCRYPTO_EC_KEYEL_CRV].len, "ec");
	if (!el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf) {
		lwsl_err("%s: OOM\n", __func__);
		goto bail2;
	}

	strcpy((char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf, curve_name);

	for (n = LWS_GENCRYPTO_EC_KEYEL_X; n < LWS_GENCRYPTO_EC_KEYEL_COUNT;
	     n++) {
		el[n].len = curve->key_bytes;
		el[n].buf = lws_malloc(curve->key_bytes, "ec");
		if (!el[n].buf)
			goto bail2;

		m = BN_bn2binpad(bn[n - 1], el[n].buf, (int32_t)el[n].len);
		if ((uint32_t)m != el[n].len)
			goto bail2;
	}

	ctx->has_private = 1;

	ret = 0;

bail2:
	BN_clear_free(bn[0]);
	BN_clear_free(bn[2]);
bail1:
	EVP_PKEY_free(pkey);
bail:
	EC_KEY_free(ec);

	return ret;
}

int
lws_genecdh_new_keypair(struct lws_genec_ctx *ctx, enum enum_lws_dh_side side,
			const char *curve_name,
			struct lws_gencrypto_keyelem *el)
{
	if (ctx->genec_alg != LEGENEC_ECDH)
		return -1;

	return lws_genec_new_keypair(ctx, side, curve_name, el);
}

int
lws_genecdsa_new_keypair(struct lws_genec_ctx *ctx, const char *curve_name,
			 struct lws_gencrypto_keyelem *el)
{
	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	return lws_genec_new_keypair(ctx, LDHS_OURS, curve_name, el);
}

#if 0
int
lws_genecdsa_hash_sign(struct lws_genec_ctx *ctx, const uint8_t *in,
		       enum lws_genhash_types hash_type,
		       uint8_t *sig, size_t sig_len)
{
	const EVP_MD *md = lws_gencrypto_openssl_hash_to_EVP_MD(hash_type);
	EVP_MD_CTX *mdctx = NULL;

	if (ctx->genec_alg != LEGENEC_ECDSA)
		return -1;

	if (!md)
		return -1;

	mdctx = EVP_MD_CTX_create();
	if (!mdctx)
		goto bail;

	if (EVP_DigestSignInit(mdctx, NULL, md, NULL,
			       EVP_PKEY_CTX_get0_pkey(ctx->ctx))) {
		lwsl_err("%s: EVP_DigestSignInit failed\n", __func__);

		goto bail;
	}
	if (EVP_DigestSignUpdate(mdctx, in, EVP_MD_size(md))) {
		lwsl_err("%s: EVP_DigestSignUpdate failed\n", __func__);

		goto bail;
	}
	if (EVP_DigestSignFinal(mdctx, sig, &sig_len)) {
		lwsl_err("%s: EVP_DigestSignFinal failed\n", __func__);

		goto bail;
	}

	EVP_MD_CTX_free(mdctx);

	return (int)sig_len;
bail:
	if (mdctx)
		EVP_MD_CTX_free(mdctx);

	return -1;
}
#endif

int
lws_genecdsa_hash_sign_jws(struct lws_genec_ctx *ctx, const uint8_t *in,
			   enum lws_genhash_types hash_type, int keybits,
			   uint8_t *sig, size_t sig_len)
{
	int ret = -1, n, keybytes = lws_gencrypto_bits_to_bytes(keybits);
	size_t hs = lws_genhash_size(hash_type);
	const BIGNUM *r = NULL, *s = NULL;
	ECDSA_SIG *ecdsasig;
	EC_KEY *eckey;

	if (ctx->genec_alg != LEGENEC_ECDSA) {
		lwsl_notice("%s: ctx alg %d\n", __func__, ctx->genec_alg);
		return -1;
	}

	if (!ctx->has_private)
		return -1;

	if ((int)sig_len != (int)(keybytes * 2)) {
		lwsl_notice("%s: sig buff %d < %d\n", __func__,
			    (int)sig_len, (int)(hs * 2));
		return -1;
	}

	eckey = EVP_PKEY_get1_EC_KEY(EVP_PKEY_CTX_get0_pkey(ctx->ctx[0]));

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

	ecdsasig = ECDSA_do_sign(in, (int)hs, eckey);
	EC_KEY_free(eckey);
	if (!ecdsasig) {
		lwsl_notice("%s: ECDSA_do_sign fail\n", __func__);
		goto bail;
	}

	ECDSA_SIG_get0(ecdsasig, &r, &s);

	/*
	 * in the 521-bit case, we have to pad the last byte as it only
	 * generates 65 bytes
	 */

	n = BN_bn2binpad(r, sig, keybytes);
	if (n != keybytes) {
		lwsl_notice("%s: bignum r fail %d %d\n", __func__, n, keybytes);
		goto bail;
	}

	n = BN_bn2binpad(s, sig + keybytes, keybytes);
	if (n != keybytes) {
		lwsl_notice("%s: bignum s fail %d %d\n", __func__, n, keybytes);
		goto bail;
	}

	ret = 0;

bail:
	if (ecdsasig)
		ECDSA_SIG_free(ecdsasig);

	return ret;
}

/* in is the JWS Signing Input hash */

int
lws_genecdsa_hash_sig_verify_jws(struct lws_genec_ctx *ctx, const uint8_t *in,
				 enum lws_genhash_types hash_type, int keybits,
				 const uint8_t *sig, size_t sig_len)
{
	int ret = -1, n, hlen = (int)lws_genhash_size(hash_type),
			keybytes = lws_gencrypto_bits_to_bytes(keybits);
	ECDSA_SIG *ecsig = ECDSA_SIG_new();
	BIGNUM *r = NULL, *s = NULL;
	EC_KEY *eckey;

	if (!ecsig)
		return -1;

	if (ctx->genec_alg != LEGENEC_ECDSA)
		goto bail;

	if ((int)sig_len != keybytes * 2) {
		lwsl_err("%s: sig buf size %d vs %d\n", __func__,
			 (int)sig_len, keybytes * 2);
		goto bail;
	}
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

	r = BN_bin2bn(sig, keybytes, NULL);
	if (!r) {
		lwsl_err("%s: BN_bin2bn (r) fail\n", __func__);
		goto bail;
	}

	s = BN_bin2bn(sig + keybytes, keybytes, NULL);
	if (!s) {
		lwsl_err("%s: BN_bin2bn (s) fail\n", __func__);
		goto bail1;
	}

	if (ECDSA_SIG_set0(ecsig, r, s) != 1) {
		lwsl_err("%s: ECDSA_SIG_set0 fail\n", __func__);
		goto bail1;
	}

	eckey = EVP_PKEY_get1_EC_KEY(EVP_PKEY_CTX_get0_pkey(ctx->ctx[0]));

	n = ECDSA_do_verify(in, hlen, ecsig, eckey);
	EC_KEY_free(eckey);
	if (n != 1) {
		lwsl_err("%s: ECDSA_do_verify fail, hlen %d\n", __func__, (int)hlen);
		lws_tls_err_describe_clear();
		goto bail;
	}

	ret = 0;
	goto bail;

bail1:
	if (r)
		BN_free(r);
	if (s)
		BN_free(s);

bail:
	ECDSA_SIG_free(ecsig);

	return ret;
}

int
lws_genecdh_compute_shared_secret(struct lws_genec_ctx *ctx, uint8_t *ss,
				  int *ss_len)
{
	int len, ret = -1;
	EC_KEY *eckey[2];

	if (!ctx->ctx[LDHS_OURS] || !ctx->ctx[LDHS_THEIRS]) {
		lwsl_err("%s: both sides must be set up\n", __func__);

		return -1;
	}

	eckey[LDHS_OURS] = EVP_PKEY_get1_EC_KEY(
				EVP_PKEY_CTX_get0_pkey(ctx->ctx[LDHS_OURS]));
	eckey[LDHS_THEIRS] = EVP_PKEY_get1_EC_KEY(
				EVP_PKEY_CTX_get0_pkey(ctx->ctx[LDHS_THEIRS]));

	len = (EC_GROUP_get_degree(EC_KEY_get0_group(eckey[LDHS_OURS])) + 7) / 8;
	if (len <= *ss_len) {
#if defined(USE_WOLFSSL)
		*ss_len = wolfSSL_ECDH_compute_key(
#else
		*ss_len = ECDH_compute_key(
#endif
				ss, (unsigned int)len,
				EC_KEY_get0_public_key(eckey[LDHS_THEIRS]),
				eckey[LDHS_OURS], NULL);
		ret = -(*ss_len < 0);
	}

	EC_KEY_free(eckey[LDHS_OURS]);
	EC_KEY_free(eckey[LDHS_THEIRS]);

	return ret;
}
