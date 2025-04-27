/*
 * lws-api-test-jose - RFC7515 jws tests
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>

/*
 * JSON Web Signature is defined in RFC7515
 *
 * https://tools.ietf.org/html/rfc7515
 *
 * It's basically a way to wrap some JSON with a JSON "header" describing the
 * crypto, and a signature, all in a BASE64 wrapper with elided terminating '='.
 *
 * The signature stays with the content, it serves a different purpose than eg
 * a TLS tunnel to transfer it.
 *
 */

/* for none, the compact serialization format is b64u(jose hdr).b64u(payload) */

static const char *none_cser =
	  "eyJhbGciOiJub25lIn0"
	  "."
	  "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFt"
	  "cGxlLmNvbS9pc19yb290Ijp0cnVlfQ",
	  *none_jose = "{\"alg\":\"none\"}",
	  *none_payload	= "{\"iss\":\"joe\",\r\n \"exp\":1300819380,\r\n"
			  " \"http://example.com/is_root\":true}";

int
test_jws_none(struct lws_context *context)
{
	struct lws_jws_map map;
	struct lws_jose jose;
	char temp[2048];
	int n, temp_len = sizeof(temp), ret = -1;

	lws_jose_init(&jose);

	/* A.5 Unsecured JSON "none" RFC7515 worked example */

	/* decode the b64.b64[.b64] compact serialization blocks */
	n = lws_jws_compact_decode(none_cser, (int)strlen(none_cser), &map, NULL,
				   temp, &temp_len);
	if (n != 2) {
		lwsl_err("%s: concat_map failed\n", __func__);
		goto bail;
	}

		/* confirm the decoded JOSE header is exactly what we expect */
		if (strncmp(none_jose, map.buf[LJWS_JOSE], map.len[LJWS_JOSE])) {
			lwsl_err("%s: jose b64 decode wrong\n", __func__);
			goto bail;
		}

	/* parse the JOSE header */
	if (lws_jws_parse_jose(&jose, map.buf[LJWS_JOSE],
			       (int)map.len[LJWS_JOSE],
			       (char *)lws_concat_temp(temp, temp_len),
			       &temp_len) < 0 || !jose.alg) {
		lwsl_err("%s: JOSE parse failed\n", __func__);
		goto bail;
	}

		/* confirm we used the "none" alg as expected from JOSE hdr */
		if (strcmp(jose.alg->alg, "none")) {
			lwsl_err("%s: JOSE header has wrong alg\n", __func__);
			goto bail;
		}

		/* confirm the payload is literally what we expect */
		if (strncmp(none_payload, map.buf[LJWS_PYLD],
					  map.len[LJWS_PYLD])) {
			lwsl_err("%s: payload b64 decode wrong\n", __func__);
			goto bail;
		}

	/* end */

	ret = 0;

bail:
	lws_jose_destroy(&jose);

	if (ret)
		lwsl_err("%s: selftest failed ++++++++++++++++++++\n", __func__);
	else
		lwsl_notice("%s: selftest OK\n", __func__);

	return ret;
}



static const char
	   *test1	= "{\"typ\":\"JWT\",\r\n \"alg\":\"HS256\"}",
	   *test1_enc	= "eyJ0eXAiOiJKV1QiLA0KICJhbGciOiJIUzI1NiJ9",
	   *test2	= "{\"iss\":\"joe\",\r\n \"exp\":1300819380,\r\n"
			  " \"http://example.com/is_root\":true}",
	   *test2_enc	= "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQ"
			  "ogImh0dHA6Ly9leGFtcGxlLmNvbS9pc19yb290Ijp0cnVlfQ",
	   *key_jwk	= "{\"kty\":\"oct\",\r\n"
			  " \"k\":\"AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQ"
			  "Lr_T-1qS0gZH75aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow\"}",
	   *hash_enc	= "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
;

int
test_jws_HS256(struct lws_context *context)
{
	char buf[2048], temp[256], *p = buf, *end = buf + sizeof(buf) - 1, *enc_ptr;
	uint8_t digest[LWS_GENHASH_LARGEST];
	struct lws_jws_map map;
	int temp_len = sizeof(temp);
	struct lws_genhmac_ctx ctx;
	struct lws_jose jose;
	struct lws_jwk jwk;
	struct lws_jws jws;
	int n;

	lws_jose_init(&jose);
	lws_jws_init(&jws, &jwk, context);

	/* Test 1: SHA256 on RFC7515 worked example */

	/* parse the JOSE header */

	if (lws_jws_parse_jose(&jose, test1, (int)strlen(test1), temp,
			       &temp_len) < 0 || !jose.alg) {
		lwsl_err("%s: JOSE parse failed\n", __func__);
		goto bail;
	}

		/* confirm we used the "none" alg as expected from JOSE hdr */
		if (strcmp(jose.alg->alg, "HS256")) {
			lwsl_err("%s: JOSE header has wrong alg\n", __func__);
			goto bail;
		}

	/* 1.1: import the JWK oct key */

	if (lws_jwk_import(&jwk, NULL, NULL, key_jwk, strlen(key_jwk)) < 0) {
		lwsl_notice("Failed to decode JWK test key\n");
		return -1;
	}
		if (jwk.kty != LWS_GENCRYPTO_KTY_OCT) {
			lwsl_err("%s: unexpected kty %d\n", __func__, jwk.kty);

			return -1;
		}

	/* 1.2: create JWS known hdr + known payload */

	n = lws_jws_encode_section(test1, strlen(test1), 1, &p, end);
	if (n < 0) {
		goto bail;
	}

		if (strcmp(buf, test1_enc))
			goto bail;

	enc_ptr = p + 1; /* + 1 skips the . */
	n = lws_jws_encode_section(test2, strlen(test2), 0, &p, end);
	if (n < 0) {
		goto bail;
	}

		if (strcmp(enc_ptr, test2_enc))
			goto bail;

	/* 1.3: use HMAC SHA-256 with known key on the hdr . payload */

	if (lws_genhmac_init(&ctx, jose.alg->hmac_type,
			     jwk.e[LWS_GENCRYPTO_OCT_KEYEL_K].buf,
			     jwk.e[LWS_GENCRYPTO_OCT_KEYEL_K].len))
		goto bail;
	if (lws_genhmac_update(&ctx, (uint8_t *)buf, lws_ptr_diff_size_t(p, buf)))
		goto bail_destroy_hmac;
	lws_genhmac_destroy(&ctx, digest);

	/* 1.4: append a base64 encode of the computed HMAC digest */

	enc_ptr = p + 1; /* + 1 skips the . */
	n = lws_jws_encode_section((const char *)digest, 32, 0, &p, end);
	if (n < 0)
		goto bail;
	if (strcmp(enc_ptr, hash_enc)) { /* check against known B64URL hash */
		lwsl_err("%s: b64 enc of computed HMAC mismatches '%s' '%s'\n",
			 __func__, enc_ptr, hash_enc);
		goto bail;
	}

	/* 1.5: Check we can agree the signature matches the payload */

	if (lws_jws_sig_confirm_compact_b64(buf, lws_ptr_diff_size_t(p, buf), &map, &jwk, context,
			lws_concat_temp(temp, temp_len), &temp_len) < 0) {
		lwsl_notice("%s: confirm sig failed\n", __func__);
		goto bail;
	}

	lws_jws_destroy(&jws);
	lws_jwk_destroy(&jwk);
	lws_jose_destroy(&jose);

	/* end */

	lwsl_notice("%s: selftest OK\n", __func__);

	return 0;

bail_destroy_hmac:
	lws_genhmac_destroy(&ctx, NULL);

bail:
	lws_jws_destroy(&jws);
	lws_jwk_destroy(&jwk);
	lws_jose_destroy(&jose);
	lwsl_err("%s: selftest failed ++++++++++++++++++++\n", __func__);

	return 1;
}


static const char
	/* the key from worked example in RFC7515 A-2, as a JWK */
	*rfc7515_rsa_key =
	"{\"kty\":\"RSA\","
	" \"n\":\"ofgWCuLjybRlzo0tZWJjNiuSfb4p4fAkd_wWJcyQoTbji9k0l8W26mPddx"
		 "HmfHQp-Vaw-4qPCJrcS2mJPMEzP1Pt0Bm4d4QlL-yRT-SFd2lZS-pCgNMs"
		 "D1W_YpRPEwOWvG6b32690r2jZ47soMZo9wGzjb_7OMg0LOL-bSf63kpaSH"
		 "SXndS5z5rexMdbBYUsLA9e-KXBdQOS-UTo7WTBEMa2R2CapHg665xsmtdV"
		 "MTBQY4uDZlxvb3qCo5ZwKh9kG4LT6_I5IhlJH7aGhyxXFvUK-DWNmoudF8"
		 "NAco9_h9iaGNj8q2ethFkMLs91kzk2PAcDTW9gb54h4FRWyuXpoQ\","
	"\"e\":\"AQAB\","
	"\"d\":\"Eq5xpGnNCivDflJsRQBXHx1hdR1k6Ulwe2JZD50LpXyWPEAeP88vLNO97I"
		"jlA7_GQ5sLKMgvfTeXZx9SE-7YwVol2NXOoAJe46sui395IW_GO-pWJ1O0"
		"BkTGoVEn2bKVRUCgu-GjBVaYLU6f3l9kJfFNS3E0QbVdxzubSu3Mkqzjkn"
		"439X0M_V51gfpRLI9JYanrC4D4qAdGcopV_0ZHHzQlBjudU2QvXt4ehNYT"
		"CBr6XCLQUShb1juUO1ZdiYoFaFQT5Tw8bGUl_x_jTj3ccPDVZFD9pIuhLh"
		"BOneufuBiB4cS98l2SR_RQyGWSeWjnczT0QU91p1DhOVRuOopznQ\","
	"\"p\":\"4BzEEOtIpmVdVEZNCqS7baC4crd0pqnRH_5IB3jw3bcxGn6QLvnEtfdUdi"
		"YrqBdss1l58BQ3KhooKeQTa9AB0Hw_Py5PJdTJNPY8cQn7ouZ2KKDcmnPG"
		"BY5t7yLc1QlQ5xHdwW1VhvKn-nXqhJTBgIPgtldC-KDV5z-y2XDwGUc\","
	"\"q\":\"uQPEfgmVtjL0Uyyx88GZFF1fOunH3-7cepKmtH4pxhtCoHqpWmT8YAmZxa"
		"ewHgHAjLYsp1ZSe7zFYHj7C6ul7TjeLQeZD_YwD66t62wDmpe_HlB-TnBA"
		"-njbglfIsRLtXlnDzQkv5dTltRJ11BKBBypeeF6689rjcJIDEz9RWdc\","
	"\"dp\":\"BwKfV3Akq5_MFZDFZCnW-wzl-CCo83WoZvnLQwCTeDv8uzluRSnm71I3Q"
		"CLdhrqE2e9YkxvuxdBfpT_PI7Yz-FOKnu1R6HsJeDCjn12Sk3vmAktV2zb"
		"34MCdy7cpdTh_YVr7tss2u6vneTwrA86rZtu5Mbr1C1XsmvkxHQAdYo0\","
	"\"dq\":\"h_96-mK1R_7glhsum81dZxjTnYynPbZpHziZjeeHcXYsXaaMwkOlODsWa"
		"7I9xXDoRwbKgB719rrmI2oKr6N3Do9U0ajaHF-NKJnwgjMd2w9cjz3_-ky"
		"NlxAr2v4IKhGNpmM5iIgOS1VZnOZ68m6_pbLBSp3nssTdlqvd0tIiTHU\","
	"\"qi\":\"IYd7DHOhrWvxkwPQsRM2tOgrjbcrfvtQJipd-DlcxyVuuM9sQLdgjVk2o"
		"y26F0EmpScGLq2MowX7fhd_QJQ3ydy5cY7YIBi87w93IKLEdfnbJtoOPLU"
		"W0ITrJReOgo1cq9SbsxYawBgfp_gh6A5603k2-ZQwVK0JKSHuLFkuQ3U\""
	"}",
	*rfc7515_rsa_a1 = /* the signed worked example in RFC7515 A-1 */
	"eyJhbGciOiJSUzI1NiJ9"
	".eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFt"
	"cGxlLmNvbS9pc19yb290Ijp0cnVlfQ"
	".cC4hiUPoj9Eetdgtv3hF80EGrhuB__dzERat0XF9g2VtQgr9PJbu3XOiZj5RZmh7"
	"AAuHIm4Bh-0Qc_lF5YKt_O8W2Fp5jujGbds9uJdbF9CUAr7t1dnZcAcQjbKBYNX4"
	"BAynRFdiuB--f_nZLgrnbyTyWzO75vRK5h6xBArLIARNPvkSjtQBMHlb1L07Qe7K"
	"0GarZRmB_eSN9383LcOLn6_dO--xi12jzDwusC-eOkHWEsqtFZESc6BfI7noOPqv"
	"hJ1phCnvWh6IeYI2w9QOYEUipUTI8np6LbgGY9Fs98rqVt5AXLIhWkWywlVmtVrB"
	"p0igcN_IoypGlUPQGe77Rw"
;

int
test_jws_RS256(struct lws_context *context)
{
	struct lws_jws_map map;
	struct lws_jose jose;
	struct lws_jwk jwk;
	struct lws_jws jws;
	char temp[2048], *in;
	int n, l, temp_len = sizeof(temp);

	lws_jose_init(&jose);
	lws_jws_init(&jws, &jwk, context);

	/* Test 2: RS256 on RFC7515 worked example */

	if (lws_gencrypto_jws_alg_to_definition("RS256", &jose.alg)) {
		lwsl_err("%s: RS256 not supported\n", __func__);
		goto bail;
	}

	/* 2.1: import the jwk */

	if (lws_jwk_import(&jwk, NULL, NULL,
			   rfc7515_rsa_key, strlen(rfc7515_rsa_key))) {
		lwsl_notice("%s: 2.2: Failed to read JWK key\n", __func__);
		goto bail2;
	}

	if (jwk.kty != LWS_GENCRYPTO_KTY_RSA) {
		lwsl_err("%s: 2.2: kty: %d instead of RSA\n", __func__, jwk.kty);
		goto bail;
	}

	/* 2.2: check the signature on the test packet from RFC7515 A-1 */

	if (lws_jws_sig_confirm_compact_b64(rfc7515_rsa_a1,
					    strlen(rfc7515_rsa_a1), &map,
					    &jwk, context, temp, &temp_len) < 0) {
		lwsl_notice("%s: 2.2: confirm rsa sig failed\n", __func__);
		goto bail;
	}

	if (lws_jws_b64_compact_map(rfc7515_rsa_a1, (int)strlen(rfc7515_rsa_a1),
				   &jws.map_b64) != 3) {
		lwsl_notice("%s: lws_jws_b64_compact_map failed\n", __func__);
		goto bail;
	}

	/* 2.3: generate our own signature for a copy of the test packet */

	in = lws_concat_temp(temp, temp_len);
	l = (int)strlen(rfc7515_rsa_a1);
	if (temp_len < l + 1)
		goto bail;
	memcpy(in, rfc7515_rsa_a1, (unsigned int)l + 1);
	temp_len -= l + 1;

	if (lws_jws_b64_compact_map(in, l, &jws.map_b64) != 3) {
		lwsl_notice("%s: lws_jws_b64_compact_map failed\n", __func__);
		goto bail;
	}

	/* overwrite the copy of the known b64 sig (it's all placed inside temp) */
	n = lws_jws_sign_from_b64(&jose, &jws,
				  (char *)jws.map_b64.buf[LJWS_SIG],
				  jws.map_b64.len[LJWS_SIG] + 8);
	if (n < 0) {
		lwsl_err("%s: failed signing test packet\n", __func__);
		goto bail;
	}
	jws.map_b64.len[LJWS_SIG] = (unsigned int)n;

	/* 2.4: confirm our signature can be verified */

	in[l] = '\0';
	if (lws_jws_sig_confirm_compact_b64(in, (unsigned int)l, &map, &jwk,
			context, lws_concat_temp(temp, temp_len), &temp_len) < 0) {
		lwsl_notice("%s: 2.2: confirm rsa sig failed\n", __func__);
		goto bail;
	}

	lws_jwk_destroy(&jwk);

	/* end */

	lwsl_notice("%s: selftest OK\n", __func__);

	return 0;

bail:
	lws_jwk_destroy(&jwk);
bail2:
	lws_jws_destroy(&jws);
	lwsl_err("%s: selftest failed ++++++++++++++++++++\n", __func__);

	return 1;
}

static const char
	*es256_jose = "{\"alg\":\"ES256\"}",
	*es256_payload	= "{\"iss\":\"joe\",\r\n \"exp\":1300819380,\r\n"
			  " \"http://example.com/is_root\":true}",
	*es256_cser =
	    "eyJhbGciOiJFUzI1NiJ9"
	    "."
	    "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFt"
	    "cGxlLmNvbS9pc19yb290Ijp0cnVlfQ"
	    "."
	    "DtEhU3ljbEg8L38VWAfUAqOyKAM6-Xx-F4GawxaepmXFCgfTjDxw5djxLa8ISlSA"
	    "pmWQxfKTUJqPP3-Kg6NU1Q",
	*es256_jwk =
	"{"
		"\"kty\":\"EC\","
		"\"crv\":\"P-256\","
		"\"x\":\"f83OJ3D2xF1Bg8vub9tLe1gHMzV76e8Tus9uPHvRVEU\","
		"\"y\":\"x_FEzRu9m36HLN_tue659LNpXW6pCyStikYjKIWI5a0\","
		"\"d\":\"jpsQnnGQmL-YBIffH1136cspYG6-0iY7X1fCE9-E9LI\""
	"}"
#if 0
			,
	rfc7515_ec_a3_R[] = {
		 14, 209,  33,  83, 121,  99, 108,  72,  60,  47, 127,  21,  88,
		  7, 212,   2, 163, 178,  40,   3,  58, 249, 124, 126,  23, 129,
		154, 195,  22, 158, 166, 101
	},
	rfc7515_ec_a3_S[] = {
		197,  10,   7, 211, 140,  60, 112, 229, 216, 241,  45, 175,
		  8,  74,  84, 128, 166, 101, 144, 197, 242, 147,  80, 154,
		143,  63, 127, 138, 131, 163,  84, 213
	}
#endif
;

int
test_jws_ES256(struct lws_context *context)
{
	uint8_t digest[LWS_GENHASH_LARGEST];
	struct lws_genhash_ctx hash_ctx;
	struct lws_jws_map map;
	struct lws_jose jose;
	struct lws_jwk jwk;
	struct lws_jws jws;
	char temp[2048], *p;
	int ret = -1, l, n, temp_len = sizeof(temp);

	/* A.3 "ES256" RFC7515 worked example - verify */

	lws_jose_init(&jose);

	/* decode the b64.b64[.b64] compact serialization blocks */
	if (lws_jws_compact_decode(es256_cser, (int)strlen(es256_cser),
				   &jws.map, &jws.map_b64,
				   temp, &temp_len) != 3) {
		lwsl_err("%s: concat_map failed\n", __func__);
		goto bail;
	}

		/* confirm the decoded JOSE header is exactly what we expect */
		if (jws.map.len[LJWS_JOSE] != strlen(es256_jose) ||
		    strncmp(es256_jose, jws.map.buf[LJWS_JOSE],
				    jws.map.len[LJWS_JOSE])) {
			lwsl_err("%s: jose b64 decode wrong\n", __func__);
			goto bail;
		}

		/* confirm the decoded payload is exactly what we expect */
		if (jws.map.len[LJWS_PYLD] != strlen(es256_payload) ||
		    strncmp(es256_payload, jws.map.buf[LJWS_PYLD],
					    jws.map.len[LJWS_PYLD])) {
			lwsl_err("%s: payload b64 decode wrong\n", __func__);
			goto bail;
		}

	/* parse the JOSE header */
	if (lws_jws_parse_jose(&jose, jws.map.buf[LJWS_JOSE],
			       (int)jws.map.len[LJWS_JOSE],
			       (char *)lws_concat_temp(temp, temp_len), &temp_len) < 0) {
		lwsl_err("%s: JOSE parse failed\n", __func__);
		goto bail;
	}

		/* confirm we used "ES256" alg we expect from the JOSE hdr */
		if (strcmp(jose.alg->alg, "ES256")) {
			lwsl_err("%s: JOSE header has wrong alg\n", __func__);
			goto bail;
		}

	jws.jwk = &jwk;
	jws.context = context;

	/* import the ES256 jwk */
	if (lws_jwk_import(&jwk, NULL, NULL, es256_jwk, strlen(es256_jwk))) {
		lwsl_notice("%s: Failed to read JWK key\n", __func__);
		goto bail;
	}

		/* sanity */
		if (jwk.kty != LWS_GENCRYPTO_KTY_EC) {
			lwsl_err("%s: kty: %d instead of EC\n",
					__func__, jwk.kty);
			goto bail1;
		}

	if (lws_jws_sig_confirm(&jws.map_b64, &jws.map, &jwk, context) < 0) {
		lwsl_notice("%s: confirm EC sig failed\n", __func__);
		goto bail1;
	}

	/* A.3 "ES256" RFC7515 worked example - sign */

	l = (int)strlen(es256_cser);
	if (temp_len < l + 1)
		goto bail1;
	p = lws_concat_temp(temp, temp_len);
	memcpy(p, es256_cser, (unsigned int)l + 1);
	temp_len -= l + 1;

	/* scan the b64 compact serialization string to map the blocks */
	if (lws_jws_b64_compact_map(p, l, &jws.map_b64) != 3)
		goto bail1;

	/* create the hash of the protected b64 part */
	if (lws_genhash_init(&hash_ctx, jose.alg->hash_type) ||
	    lws_genhash_update(&hash_ctx, jws.map_b64.buf[LJWS_JOSE],
			    jws.map_b64.len[LJWS_JOSE]) ||
	    lws_genhash_update(&hash_ctx, ".", 1) ||
	    lws_genhash_update(&hash_ctx, jws.map_b64.buf[LJWS_PYLD],
			    jws.map_b64.len[LJWS_PYLD]) ||
	    lws_genhash_destroy(&hash_ctx, digest)) {
		lws_genhash_destroy(&hash_ctx, NULL);

		goto bail1;
	}

	lwsl_hexdump(jws.map_b64.buf[LJWS_SIG], jws.map_b64.len[LJWS_SIG]);

	/* overwrite the copy of the known b64 sig (it's placed inside buf) */
	n = lws_jws_sign_from_b64(&jose, &jws,
				  (char *)jws.map_b64.buf[LJWS_SIG],
				  jws.map_b64.len[LJWS_SIG] + 8);
	if (n < 0) {
		lwsl_err("%s: failed signing test packet\n", __func__);
		goto bail1;
	}
	jws.map_b64.len[LJWS_SIG] = (unsigned int)n;

	lwsl_hexdump(jws.map_b64.buf[LJWS_SIG], jws.map_b64.len[LJWS_SIG]);

	/* 2.4: confirm our generated signature can be verified */

//	lwsl_err("p %p, l %d\n", p, (int)l);
	p[l] = '\0';
	if (lws_jws_sig_confirm_compact_b64(p, (unsigned int)l, &map, &jwk,
			context, lws_concat_temp(temp, temp_len), &temp_len) < 0) {
		lwsl_notice("%s: confirm our EC sig failed\n", __func__);
		goto bail1;
	}

	/* end */
	ret =  0;

bail1:
	lws_jwk_destroy(&jwk);
	lws_jose_destroy(&jose);

bail:
	lwsl_notice("%s: selftest %s\n", __func__, ret ? "FAIL" : "OK");

	return ret;
}

static const char
	*es512_jose = "{\"alg\":\"ES512\"}",
	*es512_payload	= "Payload",
	*es512_cser =
	     "eyJhbGciOiJFUzUxMiJ9"
	     "."
	     "UGF5bG9hZA"
	     "."
	     "AdwMgeerwtHoh-l192l60hp9wAHZFVJbLfD_UxMi70cwnZOYaRI1bKPWROc-mZZq"
	     "wqT2SI-KGDKB34XO0aw_7XdtAG8GaSwFKdCAPZgoXD2YBJZCPEX3xKpRwcdOO8Kp"
	     "EHwJjyqOgzDO7iKvU8vcnwNrmxYbSW9ERBXukOXolLzeO_Jn",
	*es512_jwk =
	   "{"
	      "\"kty\":\"EC\","
	      "\"crv\":\"P-521\","
	      "\"x\":\"AekpBQ8ST8a8VcfVOTNl353vSrDCLLJXmPk06wTjxrrjcBpXp5EOnYG_"
	           "NjFZ6OvLFV1jSfS9tsz4qUxcWceqwQGk\","
	      "\"y\":\"ADSmRA43Z1DSNx_RvcLI87cdL07l6jQyyBXMoxVg_l2Th-x3S1WDhjDl"
	           "y79ajL4Kkd0AZMaZmh9ubmf63e3kyMj2\","
	      "\"d\":\"AY5pb7A0UFiB3RELSD64fTLOSV_jazdF7fLYyuTw8lOfRhWg6Y6rUrPA"
	           "xerEzgdRhajnu0ferB0d53vM9mE15j2C\""
	   "}"
;

int
test_jws_ES512(struct lws_context *context)
{
	uint8_t digest[LWS_GENHASH_LARGEST];
	struct lws_genhash_ctx hash_ctx;
	struct lws_jws_map map;
	struct lws_jose jose;
	struct lws_jwk jwk;
	struct lws_jws jws;
	char temp[2048], *p;
	int ret = -1, l, n, temp_len = sizeof(temp);

	/* A.4 "ES512" RFC7515 worked example - verify */

	lws_jose_init(&jose);

	/* decode the b64.b64[.b64] compact serialization blocks */
	if (lws_jws_compact_decode(es512_cser, (int)strlen(es512_cser),
				   &jws.map, &jws.map_b64, temp,
				   &temp_len) != 3) {
		lwsl_err("%s: concat_map failed\n", __func__);
		goto bail;
	}

		/* confirm the decoded JOSE header is exactly what we expect */
		if (jws.map.len[LJWS_JOSE] != strlen(es512_jose) ||
		    strncmp(es512_jose, jws.map.buf[LJWS_JOSE],
				        jws.map.len[LJWS_JOSE])) {
			lwsl_err("%s: jose b64 decode wrong\n", __func__);
			goto bail;
		}

		/* confirm the decoded payload is exactly what we expect */
		if (jws.map.len[LJWS_PYLD] != strlen(es512_payload) ||
		    strncmp(es512_payload, jws.map.buf[LJWS_PYLD],
					   jws.map.len[LJWS_PYLD])) {
			lwsl_err("%s: payload b64 decode wrong\n", __func__);
			goto bail;
		}

	/* parse the JOSE header */
	if (lws_jws_parse_jose(&jose, jws.map.buf[LJWS_JOSE],
			      (int)jws.map.len[LJWS_JOSE],
			      lws_concat_temp(temp, temp_len), &temp_len) < 0) {
		lwsl_err("%s: JOSE parse failed\n", __func__);
		goto bail;
	}

		/* confirm we used "es512" alg we expect from the JOSE hdr */
		if (strcmp(jose.alg->alg, "ES512")) {
			lwsl_err("%s: JOSE header has wrong alg\n", __func__);
			goto bail;
		}

	jws.jwk = &jwk;
	jws.context = context;

	/* import the es512 jwk */
	if (lws_jwk_import(&jwk, NULL, NULL, es512_jwk, strlen(es512_jwk))) {
		lwsl_notice("%s: Failed to read JWK key\n", __func__);
		goto bail;
	}

		/* sanity */
		if (jwk.kty != LWS_GENCRYPTO_KTY_EC) {
			lwsl_err("%s: kty: %d instead of EC\n",
					__func__, jwk.kty);
			goto bail1;
		}

	if (lws_jws_sig_confirm(&jws.map_b64, &jws.map, &jwk, context) < 0) {
		lwsl_notice("%s: confirm EC sig failed\n", __func__);
		goto bail1;
	}

	/* A.3 "es512" RFC7515 worked example - sign */

	l = (int)strlen(es512_cser);
	if (temp_len < l)
		goto bail1;
	p = lws_concat_temp(temp, temp_len);
	memcpy(p, es512_cser, (unsigned int)l + 1);
	temp_len -= (l + 1);

	/* scan the b64 compact serialization string to map the blocks */
	if (lws_jws_b64_compact_map(p, l, &jws.map_b64) != 3)
		goto bail1;

	/* create the hash of the protected b64 part */
	if (lws_genhash_init(&hash_ctx, jose.alg->hash_type) ||
	    lws_genhash_update(&hash_ctx, jws.map_b64.buf[LJWS_JOSE],
			       jws.map_b64.len[LJWS_JOSE]) ||
	    lws_genhash_update(&hash_ctx, ".", 1) ||
	    lws_genhash_update(&hash_ctx, jws.map_b64.buf[LJWS_PYLD],
			       jws.map_b64.len[LJWS_PYLD]) ||
	    lws_genhash_destroy(&hash_ctx, digest)) {
		lws_genhash_destroy(&hash_ctx, NULL);

		goto bail1;
	}

	/* overwrite the copy of the known b64 sig (it's placed inside buf) */
	n = lws_jws_sign_from_b64(&jose, &jws,
				  (char *)jws.map_b64.buf[LJWS_SIG], 1024);
	if (n < 0) {
		lwsl_err("%s: failed signing test packet\n", __func__);
		goto bail1;
	}
	jws.map_b64.len[LJWS_SIG] = (unsigned int)n;

	/* 2.4: confirm our generated signature can be verified */

	p[l] = '\0';

	if (lws_jws_sig_confirm_compact_b64(p, (unsigned int)l, &map, &jwk, context,
			lws_concat_temp(temp, temp_len), &temp_len) < 0) {
		lwsl_notice("%s: confirm our ECDSA sig failed\n", __func__);
		goto bail1;
	}

	/* jwt test */

	{
		unsigned long long ull = lws_now_secs();
		char buf[8192];
		size_t cml = 2048, cml2 = 2048;

		if (lws_jwt_sign_compact(context, &jwk, "ES512",
					(char *)buf, &cml2,
					(char *)buf + 2048, 4096,
					"{\"iss\":\"warmcat.com\",\"aud\":"
					"\"https://libwebsockets.org/sai\","
					"\"iat\":%llu,"
					"\"nbf\":%llu,"
					"\"exp\":%llu,"
					"\"sub\":\"manage\"}", ull,
					ull - 60, ull + (30 * 24 * 3600)
				     )) {
			lwsl_err("%s: failed to create JWT\n", __func__);
			goto bail1;
		}

		lwsl_notice("%s: jwt test '%s'\n", __func__, buf);

		if (lws_jwt_signed_validate(context, &jwk, "ES512",
					     (const char *)buf, cml2,
					     (char *)buf + 2048, 2048,
					     (char *)buf + 4096, &cml)) {
			lwsl_err("%s: failed to parse JWT\n", __func__);

			goto bail1;
		}

		lwsl_notice("%s: jwt valid, payload '%s'\n",
				__func__, buf + 4096);
	}

	/* end */
	ret =  0;

bail1:
	lws_jwk_destroy(&jwk);
	lws_jose_destroy(&jose);

bail:
	lwsl_notice("%s: selftest %s\n", __func__, ret ? "FAIL" : "OK");

	return ret;
}

static char
	rsa_cert[] = "-----BEGIN CERTIFICATE-----\n"
	     "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\n"
	     "e2Hl/7M0c/lbZ4FvXgARlex2rkgS0Ka06HE=\n"
	     "-----END CERTIFICATE-----\n",
	rsa_key[] = "-----BEGIN PRIVATE KEY-----\n"
	    "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\n"
	    "fBrpEY1IATtPq1taBZZogRqI3rOkkPk=\n"
	    "-----END PRIVATE KEY-----\n";

int
test_jwt_RS256(struct lws_context *context)
{
	struct lws_jwk jwk;
	struct lws_x509_cert *pub = NULL;
	int ret = -1;
	int ret_encode;
	char sha1_fingerprint[30];
	uint8_t sha1sum[20];
	char der_buf[LWS_ARRAY_SIZE(rsa_cert)];
	union lws_tls_cert_info_results *der_info =
			(union lws_tls_cert_info_results *)der_buf;

	if (lws_x509_create(&pub)) {
		lwsl_err("%s: failed to create x509 public key\n", __func__);
		goto bail;
	}

	if (lws_x509_parse_from_pem(pub, rsa_cert, LWS_ARRAY_SIZE(rsa_cert))) {
		lwsl_err("%s: failed to parse x509 public key\n", __func__);
		goto bail;
	}

	if (lws_x509_public_to_jwk(&jwk, pub, NULL, 2048)) {
		lwsl_err("%s: failed to copy public key to jwk\n", __func__);
		goto bail;
	}

	if (lws_x509_jwk_privkey_pem(context, &jwk, (char *)rsa_key,
				     LWS_ARRAY_SIZE(rsa_key), NULL)) {
		lwsl_err("%s: failed to copy private key to jwk\n", __func__);
		goto bail;
	}

	if (lws_x509_info(pub, LWS_TLS_CERT_INFO_DER_RAW, der_info,
			  LWS_ARRAY_SIZE(der_buf) - sizeof(*der_info) +
			  sizeof(der_info->ns.name)) ||
	    der_info->ns.len <= 0) {
		lwsl_err("%s: failed to parse x509 public key\n", __func__);
		goto bail;
	}

	if (!lws_SHA1((unsigned char *)der_info->ns.name,
		      (size_t)der_info->ns.len, sha1sum)) {
		lwsl_err("%s: sha1sum of public key failed\n", __func__);
		goto bail;
	}

	ret_encode = lws_b64_encode_string_url((char *)sha1sum,
				LWS_ARRAY_SIZE(sha1sum), sha1_fingerprint,
				LWS_ARRAY_SIZE(sha1_fingerprint));
	if (ret_encode < 0) {
		lwsl_err("%s: failed to encode sha1sum to base64url\n", __func__);
		goto bail;
	}

	while (sha1_fingerprint[--ret_encode] == '=')
		sha1_fingerprint[ret_encode] = '\0';

	lwsl_notice("%s: cert fingerprint '%s'\n", __func__, sha1_fingerprint);

	/* now produce jwt with some additional header fields */
	{
		unsigned long long ull = lws_now_secs();
		char buf[8192];
		size_t cml = 2048, cml2 = 2048;
		const char hdr_fmt[] = "{\"alg\":\"RS256\", \"typ\":\"JWT\", \"x5t\":\"%s\"}";
		char jose_hdr[LWS_ARRAY_SIZE(hdr_fmt) + LWS_ARRAY_SIZE(sha1_fingerprint)];

		struct lws_jwt_sign_info info = {
			.alg = NULL,
			.jose_hdr = jose_hdr,
			.jose_hdr_len = (size_t)lws_snprintf(jose_hdr, LWS_ARRAY_SIZE(jose_hdr), hdr_fmt, sha1_fingerprint),
			.out = buf,
			.out_len = &cml2,
			.temp = buf + cml2,
			.tl = 4096
		};

		lwsl_notice("%s: jose_hdr of len %zu: '%s'\n", __func__, info.jose_hdr_len, info.jose_hdr);
		if (lws_jwt_sign_via_info(context, &jwk, &info,
					"{\"iss\":\"warmcat.com\",\"aud\":"
					"\"https://libwebsockets.org/sai\","
					"\"iat\":%llu,"
					"\"nbf\":%llu,"
					"\"exp\":%llu,"
					"\"sub\":\"manage\"}", ull,
					ull - 60, ull + (30 * 24 * 3600)
						 )) {
			lwsl_err("%s: failed to create JWT\n", __func__);
			goto bail1;
		}

		lwsl_notice("%s: jwt test '%s'\n", __func__, buf);

		if (lws_jwt_signed_validate(context, &jwk, "RS256",
							 (const char *)buf, cml2,
							 (char *)buf + 2048, 2048,
							 (char *)buf + 4096, &cml)) {
			lwsl_err("%s: failed to parse JWT\n", __func__);

			goto bail1;
		}

		lwsl_notice("%s: jwt valid, payload '%s'\n",
				__func__, buf + 4096);
	}

	/* end */
	ret =	0;

bail1:
	lws_jwk_destroy(&jwk);
	lws_x509_destroy(&pub);

bail:
	lwsl_notice("%s: selftest %s\n", __func__, ret ? "FAIL" : "OK");

	return ret;
}

int
test_jws(struct lws_context *context)
{
	int n = 0;

	n |= test_jws_none(context);
	n |= test_jws_HS256(context);
	n |= test_jws_RS256(context);
	n |= test_jws_ES256(context);
	n |= test_jws_ES512(context);
	n |= test_jwt_RS256(context);

	return n;
}
