/*
 * lws-api-test-gencrypto - lws-genec
 *
 * Written in 2010-2018 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>

static const uint8_t
	*jwk_ec1 = (uint8_t *)
		"{\"kty\":\"EC\","
		  "\"crv\":\"P-256\","
		  "\"x\":\"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4\","
		  "\"y\":\"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM\","
		  "\"d\":\"870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE\","
		  "\"use\":\"enc\","
		  "\"kid\":\"rfc7517-A.2-example private key\"}"
;

static int
test_genec1(struct lws_context *context)
{
	struct lws_genec_ctx ctx;
	struct lws_jwk jwk;
	struct lws_gencrypto_keyelem el[LWS_GENCRYPTO_EC_KEYEL_COUNT];
	//uint8_t res[32], res1[32];
	int n;

	memset(el, 0, sizeof(el));

	if (lws_genecdh_create(&ctx, context, NULL))
		return 1;

	/* let's create a new key */

	if (lws_genecdh_new_keypair(&ctx, LDHS_OURS, "P-256", el)) {
		lwsl_err("%s: lws_genec_new_keypair failed\n", __func__);
		return 1;
	}

	lws_genec_dump(el);
	lws_genec_destroy_elements(el);

	lws_genec_destroy(&ctx);

	if (lws_jwk_import(&jwk, NULL, NULL, (char *)jwk_ec1,
			   strlen((char *)jwk_ec1)) < 0) {
		lwsl_notice("Failed to decode JWK test key\n");
		return 1;
	}

	lws_jwk_dump(&jwk);

	if (jwk.kty != LWS_GENCRYPTO_KTY_EC) {
		lws_jwk_destroy(&jwk);
		lwsl_err("%s: jwk is not an EC key\n", __func__);
		return 1;
	}

	if (lws_genecdh_create(&ctx, context, NULL))
		return 1;

	n = lws_genecdh_set_key(&ctx, jwk.e, LDHS_OURS);
	if (n) {
		lws_jwk_destroy(&jwk);
		lwsl_err("%s: lws_genec_create failed: %d\n", __func__, n);
		return 1;
	}
#if 0
	if (lws_genec_crypt(&ctx, cbc256, 16, res, (uint8_t *)cbc256_iv,
			     NULL, NULL)) {
		lwsl_err("%s: lws_genec_crypt failed\n", __func__);
		goto bail;
	}

	if (lws_timingsafe_bcmp(cbc256_enc, res, 16)) {
		lwsl_err("%s: lws_genec_crypt encoding mismatch\n", __func__);
		lwsl_hexdump_notice(res, 16);
		goto bail;
	}

	lws_genec_destroy(&ctx);

	if (lws_genec_create(&ctx, LWS_GAESO_DEC, LWS_GAESM_CBC, &e, NULL)) {
		lwsl_err("%s: lws_genec_create dec failed\n", __func__);
		return -1;
	}

	if (lws_genec_crypt(&ctx, res, 16, res1, (uint8_t *)cbc256_iv,
			     NULL, NULL)) {
		lwsl_err("%s: lws_genec_crypt dec failed\n", __func__);
		goto bail;
	}

	if (lws_timingsafe_bcmp(cbc256, res1, 16)) {
		lwsl_err("%s: lws_genec_crypt decoding mismatch\n", __func__);
		lwsl_hexdump_notice(res, 16);
		goto bail;
	}
#endif
	lws_genec_destroy(&ctx);

	lws_jwk_destroy(&jwk);

	return 0;

//bail:
//	lws_genec_destroy(&ctx);

//	return -1;
}

int
test_genec(struct lws_context *context)
{
	if (test_genec1(context))
		goto bail;

	/* end */

	lwsl_notice("%s: selftest OK\n", __func__);

	return 0;

bail:
	lwsl_err("%s: selftest failed ++++++++++++++++++++\n", __func__);

	return 1;
}
