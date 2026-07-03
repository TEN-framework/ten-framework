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
 */

#include "private-lib-core.h"
#include "private-lib-jose.h"
#include "private-lib-jose-jwe.h"

/*
 * Currently only support flattened or compact (implicitly single signature)
 */

static const char * const jwe_json[] = {
	"protected",
	"iv",
	"ciphertext",
	"tag",
	"encrypted_key"
};

enum enum_jwe_complete_tokens {
	LWS_EJCT_PROTECTED,
	LWS_EJCT_IV,
	LWS_EJCT_CIPHERTEXT,
	LWS_EJCT_TAG,
	LWS_EJCT_RECIP_ENC_KEY,
};

/* parse a JWS complete or flattened JSON object */

struct jwe_cb_args {
	struct lws_jws *jws;

	char *temp;
	int *temp_len;
};

static signed char
lws_jwe_json_cb(struct lejp_ctx *ctx, char reason)
{
	struct jwe_cb_args *args = (struct jwe_cb_args *)ctx->user;
	int n, m;

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {

	/* strings */

	case LWS_EJCT_PROTECTED:  /* base64u: JOSE: must contain 'alg' */
		m = LJWS_JOSE;
		goto append_string;
	case LWS_EJCT_IV:	/* base64u */
		m = LJWE_IV;
		goto append_string;
	case LWS_EJCT_CIPHERTEXT:  /* base64u */
		m = LJWE_CTXT;
		goto append_string;
	case LWS_EJCT_TAG:  /* base64u */
		m = LJWE_ATAG;
		goto append_string;
	case LWS_EJCT_RECIP_ENC_KEY:  /* base64u */
		m = LJWE_EKEY;
		goto append_string;

	default:
		return -1;
	}

	return 0;

append_string:

	if (*args->temp_len < ctx->npos) {
		lwsl_err("%s: out of parsing space\n", __func__);
		return -1;
	}

	/*
	 * We keep both b64u and decoded in temp mapped using map / map_b64,
	 * the jws signature is actually over the b64 content not the plaintext,
	 * and we can't do it until we see the protected alg.
	 */

	if (!args->jws->map_b64.buf[m]) {
		args->jws->map_b64.buf[m] = args->temp;
		args->jws->map_b64.len[m] = 0;
	}

	memcpy(args->temp, ctx->buf, ctx->npos);
	args->temp += ctx->npos;
	*args->temp_len -= ctx->npos;
	args->jws->map_b64.len[m] += ctx->npos;

	if (reason == LEJPCB_VAL_STR_END) {
		args->jws->map.buf[m] = args->temp;

		n = lws_b64_decode_string_len(
			(const char *)args->jws->map_b64.buf[m],
			(int)args->jws->map_b64.len[m],
			(char *)args->temp, *args->temp_len);
		if (n < 0) {
			lwsl_err("%s: b64 decode failed\n", __func__);
			return -1;
		}

		args->temp += n;
		*args->temp_len -= n;
		args->jws->map.len[m] = (uint32_t)n;
	}

	return 0;
}

int
lws_jwe_json_parse(struct lws_jwe *jwe, const uint8_t *buf, int len,
		   char *temp, int *temp_len)
{
	struct jwe_cb_args args;
	struct lejp_ctx jctx;
	int m = 0;

	args.jws = &jwe->jws;
	args.temp = temp;
	args.temp_len = temp_len;

	lejp_construct(&jctx, lws_jwe_json_cb, &args, jwe_json,
		       LWS_ARRAY_SIZE(jwe_json));

	m = lejp_parse(&jctx, (uint8_t *)buf, len);
	lejp_destruct(&jctx);
	if (m < 0) {
		lwsl_notice("%s: parse returned %d\n", __func__, m);
		return -1;
	}

	return 0;
}

void
lws_jwe_init(struct lws_jwe *jwe, struct lws_context *context)
{
	lws_jose_init(&jwe->jose);
	lws_jws_init(&jwe->jws, &jwe->jwk, context);
	memset(&jwe->jwk, 0, sizeof(jwe->jwk));
	jwe->recip = 0;
	jwe->cek_valid = 0;
}

void
lws_jwe_destroy(struct lws_jwe *jwe)
{
	lws_jws_destroy(&jwe->jws);
	lws_jose_destroy(&jwe->jose);
	lws_jwk_destroy(&jwe->jwk);
	/* cleanse the CEK we held on to in case of further encryptions of it */
	lws_explicit_bzero(jwe->cek, sizeof(jwe->cek));
	jwe->cek_valid = 0;
}

static uint8_t *
be32(uint32_t i, uint32_t *p32)
{
	uint8_t *p = (uint8_t *)p32;

	*p++ = (uint8_t)((i >> 24) & 0xff);
	*p++ = (uint8_t)((i >> 16) & 0xff);
	*p++ = (uint8_t)((i >> 8) & 0xff);
	*p++ = (uint8_t)(i & 0xff);

	return (uint8_t *)p32;
}

/*
 * The key derivation process derives the agreed-upon key from the
 * shared secret Z established through the ECDH algorithm, per
 * Section 6.2.2.2 of [NIST.800-56A].
 *
 *
 * Key derivation is performed using the Concat KDF, as defined in
 * Section 5.8.1 of [NIST.800-56A], where the Digest Method is SHA-256.
 *
 * out must be prepared to take at least 32 bytes or the encrypted key size,
 * whichever is larger.
 */

int
lws_jwa_concat_kdf(struct lws_jwe *jwe, int direct, uint8_t *out,
		   const uint8_t *shared_secret, int sslen)
{
	int hlen = (int)lws_genhash_size(LWS_GENHASH_TYPE_SHA256), aidlen;
	struct lws_genhash_ctx hash_ctx;
	uint32_t ctr = 1, t;
	const char *aid;

	if (!jwe->jose.enc_alg || !jwe->jose.alg)
		return -1;

	/*
	 * Hash
	 *
	 * AlgorithmID || PartyUInfo || PartyVInfo
	 * 	{|| SuppPubInfo }{|| SuppPrivInfo }
	 *
	 * AlgorithmID
	 *
	 * The AlgorithmID value is of the form Datalen || Data, where Data
	 * is a variable-length string of zero or more octets, and Datalen is
	 * a fixed-length, big-endian 32-bit counter that indicates the
	 * length (in octets) of Data.  In the Direct Key Agreement case,
	 * Data is set to the octets of the ASCII representation of the "enc"
	 * Header Parameter value.  In the Key Agreement with Key Wrapping
	 * case, Data is set to the octets of the ASCII representation of the
	 * "alg" (algorithm) Header Parameter value.
	 */

	aid = direct ? jwe->jose.enc_alg->alg : jwe->jose.alg->alg;
	aidlen = (int)strlen(aid);

	/*
	 *   PartyUInfo (PartyVInfo is the same deal)
	 *
	 *    The PartyUInfo value is of the form Datalen || Data, where Data is
	 *    a variable-length string of zero or more octets, and Datalen is a
	 *    fixed-length, big-endian 32-bit counter that indicates the length
	 *    (in octets) of Data.  If an "apu" (agreement PartyUInfo) Header
	 *    Parameter is present, Data is set to the result of base64url
	 *    decoding the "apu" value and Datalen is set to the number of
	 *    octets in Data.  Otherwise, Datalen is set to 0 and Data is set to
	 *    the empty octet sequence
	 *
	 *   SuppPubInfo
	 *
	 *    This is set to the keydatalen represented as a 32-bit big-endian
	 *    integer.
	 *
	 *   keydatalen
	 *
	 *    This is set to the number of bits in the desired output key.  For
	 *    "ECDH-ES", this is length of the key used by the "enc" algorithm.
	 *    For "ECDH-ES+A128KW", "ECDH-ES+A192KW", and "ECDH-ES+A256KW", this
	 *    is 128, 192, and 256, respectively.
	 *
	 *    Compute Hash i = H(counter || Z || OtherInfo).
	 *
	 *    We must iteratively hash over key material that's larger than
	 *    one hash output size (256b for SHA-256)
	 */

	while (ctr <= (uint32_t)((jwe->jose.enc_alg->keybits_fixed + (hlen - 1)) / hlen)) {

		/*
		 * Key derivation is performed using the Concat KDF, as defined
		 * in Section 5.8.1 of [NIST.800-56A], where the Digest Method
		 * is SHA-256.
		 */

		if (lws_genhash_init(&hash_ctx, LWS_GENHASH_TYPE_SHA256))
			return -1;

		if (/* counter */
		    lws_genhash_update(&hash_ctx, be32(ctr++, &t), 4) ||
		    /* Z */
		    lws_genhash_update(&hash_ctx, shared_secret, (unsigned int)sslen) ||
		    /* other info */
		    lws_genhash_update(&hash_ctx, be32((uint32_t)strlen(aid), &t), 4) ||
		    lws_genhash_update(&hash_ctx, aid, (unsigned int)aidlen) ||
		    lws_genhash_update(&hash_ctx,
				       be32(jwe->jose.e[LJJHI_APU].len, &t), 4) ||
		    lws_genhash_update(&hash_ctx, jwe->jose.e[LJJHI_APU].buf,
						  jwe->jose.e[LJJHI_APU].len) ||
		    lws_genhash_update(&hash_ctx,
				       be32(jwe->jose.e[LJJHI_APV].len, &t), 4) ||
		    lws_genhash_update(&hash_ctx, jwe->jose.e[LJJHI_APV].buf,
						  jwe->jose.e[LJJHI_APV].len) ||
		    lws_genhash_update(&hash_ctx,
				       be32(jwe->jose.enc_alg->keybits_fixed, &t),
					    4) ||
		    lws_genhash_destroy(&hash_ctx, out)) {
			lwsl_err("%s: fail\n", __func__);
			lws_genhash_destroy(&hash_ctx, NULL);

			return -1;
		}

		out += hlen;
	}

	return 0;
}

void
lws_jwe_be64(uint64_t c, uint8_t *p8)
{
	int n;

	for (n = 56; n >= 0; n -= 8)
		*p8++ = (uint8_t)((c >> n) & 0xff);
}

int
lws_jwe_auth_and_decrypt(struct lws_jwe *jwe, char *temp, int *temp_len)
{
	int valid_aescbc_hmac, valid_aesgcm;
	char dotstar[96];

	if (lws_jwe_parse_jose(&jwe->jose, jwe->jws.map.buf[LJWS_JOSE],
			       (int)jwe->jws.map.len[LJWS_JOSE],
			       temp, temp_len) < 0) {
		lws_strnncpy(dotstar, jwe->jws.map.buf[LJWS_JOSE],
			     jwe->jws.map.len[LJWS_JOSE], sizeof(dotstar));
		lwsl_err("%s: JOSE parse '%s' failed\n", __func__, dotstar);
		return -1;
	}

	if (!jwe->jose.alg) {
		lws_strnncpy(dotstar, jwe->jws.map.buf[LJWS_JOSE],
			     jwe->jws.map.len[LJWS_JOSE], sizeof(dotstar));
		lwsl_err("%s: no jose.alg: %s\n", __func__, dotstar);

		return -1;
	}

	valid_aescbc_hmac = jwe->jose.enc_alg &&
		jwe->jose.enc_alg->algtype_crypto == LWS_JOSE_ENCTYPE_AES_CBC &&
		(jwe->jose.enc_alg->hmac_type == LWS_GENHMAC_TYPE_SHA256 ||
		 jwe->jose.enc_alg->hmac_type == LWS_GENHMAC_TYPE_SHA384 ||
		 jwe->jose.enc_alg->hmac_type == LWS_GENHMAC_TYPE_SHA512);

	valid_aesgcm = jwe->jose.enc_alg &&
		jwe->jose.enc_alg->algtype_crypto == LWS_JOSE_ENCTYPE_AES_GCM;

	if ((jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5 ||
	     jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_RSASSA_PKCS1_OAEP)) {
		/* RSA + AESCBC */
		if (valid_aescbc_hmac)
			return lws_jwe_auth_and_decrypt_rsa_aes_cbc_hs(jwe);
		/* RSA + AESGCM */
		if (valid_aesgcm)
			return lws_jwe_auth_and_decrypt_rsa_aes_gcm(jwe);
	}

	/* AESKW */

	if (jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_AES_ECB &&
	    valid_aescbc_hmac)
		return lws_jwe_auth_and_decrypt_aeskw_cbc_hs(jwe);

	/* ECDH-ES + AESKW */

	if (jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_ECDHES &&
	    valid_aescbc_hmac)
		return lws_jwe_auth_and_decrypt_ecdh_cbc_hs(jwe,
							    temp, temp_len);

	lwsl_err("%s: unknown cipher alg combo %s / %s\n", __func__,
			jwe->jose.alg->alg, jwe->jose.enc_alg ?
					jwe->jose.enc_alg->alg : "NULL");

	return -1;
}
int
lws_jwe_encrypt(struct lws_jwe *jwe, char *temp, int *temp_len)
{
	int valid_aescbc_hmac, valid_aesgcm, ot = *temp_len, ret = -1;

	if (jwe->jose.recipients >= (int)LWS_ARRAY_SIZE(jwe->jose.recipient)) {
		lwsl_err("%s: max recipients reached\n", __func__);

		return -1;
	}

	valid_aesgcm = jwe->jose.enc_alg &&
		jwe->jose.enc_alg->algtype_crypto == LWS_JOSE_ENCTYPE_AES_GCM;

	if (lws_jwe_parse_jose(&jwe->jose, jwe->jws.map.buf[LJWS_JOSE],
			       (int)jwe->jws.map.len[LJWS_JOSE], temp, temp_len) < 0) {
		lwsl_err("%s: JOSE parse failed\n", __func__);
		goto bail;
	}

	temp += ot - *temp_len;

	valid_aescbc_hmac = jwe->jose.enc_alg &&
		jwe->jose.enc_alg->algtype_crypto == LWS_JOSE_ENCTYPE_AES_CBC &&
		(jwe->jose.enc_alg->hmac_type == LWS_GENHMAC_TYPE_SHA256 ||
		 jwe->jose.enc_alg->hmac_type == LWS_GENHMAC_TYPE_SHA384 ||
		 jwe->jose.enc_alg->hmac_type == LWS_GENHMAC_TYPE_SHA512);

	if ((jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5 ||
	     jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_RSASSA_PKCS1_OAEP)) {
		/* RSA + AESCBC */
		if (valid_aescbc_hmac) {
			ret = lws_jwe_encrypt_rsa_aes_cbc_hs(jwe, temp, temp_len);
			goto bail;
		}
		/* RSA + AESGCM */
		if (valid_aesgcm) {
			ret = lws_jwe_encrypt_rsa_aes_gcm(jwe, temp, temp_len);
			goto bail;
		}
	}

	/* AESKW */

	if (jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_AES_ECB &&
	    valid_aescbc_hmac) {
		ret = lws_jwe_encrypt_aeskw_cbc_hs(jwe, temp, temp_len);
		goto bail;
	}

	/* ECDH-ES + AESKW */

	if (jwe->jose.alg->algtype_signing == LWS_JOSE_ENCTYPE_ECDHES &&
	    valid_aescbc_hmac) {
		ret = lws_jwe_encrypt_ecdh_cbc_hs(jwe, temp, temp_len);
		goto bail;
	}

	lwsl_err("%s: unknown cipher alg combo %s / %s\n", __func__,
			jwe->jose.alg->alg, jwe->jose.enc_alg ?
					jwe->jose.enc_alg->alg : "NULL");

bail:
	if (ret)
		memset(&jwe->jose.recipient[jwe->jose.recipients], 0,
			sizeof(jwe->jose.recipient[0]));
	else
		jwe->jose.recipients++;

	return ret;
}

/*
 * JWE Compact Serialization consists of
 *
 *     BASE64URL(UTF8(JWE Protected Header)) || '.' ||
 *     BASE64URL(JWE Encrypted Key)	     || '.' ||
 *     BASE64URL(JWE Initialization Vector)  || '.' ||
 *     BASE64URL(JWE Ciphertext)	     || '.' ||
 *     BASE64URL(JWE Authentication Tag)
 *
 *
 * In the JWE Compact Serialization, no JWE Shared Unprotected Header or
 * JWE Per-Recipient Unprotected Header are used.  In this case, the
 * JOSE Header and the JWE Protected Header are the same.
 *
 * Therefore:
 *
 *  - Everything needed in the header part must go in the protected header
 *    (it's the only part emitted).  We expect the caller did this.
 *
 *  - You can't emit Compact representation if there are multiple recipients
 */

int
lws_jwe_render_compact(struct lws_jwe *jwe, char *out, size_t out_len)
{
	size_t orig = out_len;
	int n;

	if (jwe->jose.recipients > 1) {
		lwsl_notice("%s: can't issue compact representation for"
			    " multiple recipients (%d)\n", __func__,
			    jwe->jose.recipients);

		return -1;
	}

	n = lws_jws_base64_enc(jwe->jws.map.buf[LJWS_JOSE],
			       jwe->jws.map.len[LJWS_JOSE], out, out_len);
	if (n < 0 || (int)out_len == n) {
		lwsl_info("%s: unable to encode JOSE\n", __func__);
		return -1;
	}

	out += n;
	*out++ = '.';
	out_len -= (unsigned int)n + 1;

	n = lws_jws_base64_enc(jwe->jws.map.buf[LJWE_EKEY],
			       jwe->jws.map.len[LJWE_EKEY], out, out_len);
	if (n < 0 || (int)out_len == n) {
		lwsl_info("%s: unable to encode EKEY\n", __func__);
		return -1;
	}

	out += n;
	*out++ = '.';
	out_len -= (unsigned int)n + 1;
	n = lws_jws_base64_enc(jwe->jws.map.buf[LJWE_IV],
			       jwe->jws.map.len[LJWE_IV], out, out_len);
	if (n < 0 || (int)out_len == n) {
		lwsl_info("%s: unable to encode IV\n", __func__);
		return -1;
	}

	out += n;
	*out++ = '.';
	out_len -= (unsigned int)n + 1;

	n = lws_jws_base64_enc(jwe->jws.map.buf[LJWE_CTXT],
			       jwe->jws.map.len[LJWE_CTXT], out, out_len);
	if (n < 0 || (int)out_len == n) {
		lwsl_info("%s: unable to encode CTXT\n", __func__);
		return -1;
	}

	out += n;
	*out++ = '.';
	out_len -= (unsigned int)n + 1;
	n = lws_jws_base64_enc(jwe->jws.map.buf[LJWE_ATAG],
			       jwe->jws.map.len[LJWE_ATAG], out, out_len);
	if (n < 0 || (int)out_len == n) {
		lwsl_info("%s: unable to encode ATAG\n", __func__);
		return -1;
	}

	out += n;
	*out++ = '\0';
	out_len -= (unsigned int)n;

	return (int)(orig - out_len);
}

int
lws_jwe_create_packet(struct lws_jwe *jwe, const char *payload, size_t len,
		      const char *nonce, char *out, size_t out_len,
		      struct lws_context *context)
{
	char *buf, *start, *p, *end, *p1, *end1;
	struct lws_jws jws;
	int n, m;

	lws_jws_init(&jws, &jwe->jwk, context);

	/*
	 * This buffer is local to the function, the actual output is prepared
	 * into out.  Only the plaintext protected header
	 * (which contains the public key, 512 bytes for 4096b) goes in
	 * here temporarily.
	 */
	n = LWS_PRE + 2048;
	buf = malloc((unsigned int)n);
	if (!buf) {
		lwsl_notice("%s: malloc %d failed\n", __func__, n);
		return -1;
	}

	p = start = buf + LWS_PRE;
	end = buf + n - LWS_PRE - 1;

	/*
	 * temporary JWS protected header plaintext
	 */

	if (!jwe->jose.alg || !jwe->jose.alg->alg)
		goto bail;

	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "{\"alg\":\"%s\",\"jwk\":",
			  jwe->jose.alg->alg);
	m = lws_ptr_diff(end, p);
	n = lws_jwk_export(&jwe->jwk, 0, p, &m);
	if (n < 0) {
		lwsl_notice("failed to export jwk\n");

		goto bail;
	}
	p += n;
	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ",\"nonce\":\"%s\"}", nonce);

	/*
	 * prepare the signed outer JSON with all the parts in
	 */

	p1 = out;
	end1 = out + out_len - 1;

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "{\"protected\":\"");
	jws.map_b64.buf[LJWS_JOSE] = p1;
	n = lws_jws_base64_enc(start, lws_ptr_diff_size_t(p, start), p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("%s: failed to encode protected\n", __func__);
		goto bail;
	}
	jws.map_b64.len[LJWS_JOSE] = (unsigned int)n;
	p1 += n;

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\",\"payload\":\"");
	jws.map_b64.buf[LJWS_PYLD] = p1;
	n = lws_jws_base64_enc(payload, len, p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("%s: failed to encode payload\n", __func__);
		goto bail;
	}
	jws.map_b64.len[LJWS_PYLD] = (unsigned int)n;
	p1 += n;

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\",\"header\":\"");
	jws.map_b64.buf[LJWS_UHDR] = p1;
	n = lws_jws_base64_enc(payload, len, p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("%s: failed to encode payload\n", __func__);
		goto bail;
	}
	jws.map_b64.len[LJWS_UHDR] = (unsigned int)n;

	p1 += n;
	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\",\"signature\":\"");

	/*
	 * taking the b64 protected header and the b64 payload, sign them
	 * and place the signature into the packet
	 */
	n = lws_jws_sign_from_b64(&jwe->jose, &jws, p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("sig gen failed\n");

		goto bail;
	}
	jws.map_b64.buf[LJWS_SIG] = p1;
	jws.map_b64.len[LJWS_SIG] = (unsigned int)n;

	p1 += n;
	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\"}");

	free(buf);

	return lws_ptr_diff(p1, out);

bail:
	lws_jws_destroy(&jws);
	free(buf);

	return -1;
}

static const char *protected_en[] = {
	"encrypted_key", "aad", "iv", "ciphertext", "tag"
};

static int protected_idx[] = {
	LJWE_EKEY, LJWE_AAD, LJWE_IV, LJWE_CTXT, LJWE_ATAG
};

/*
 * The complete JWE may look something like this:
 *
 *  {
 *    "protected":
 *     "eyJlbmMiOiJBMTI4Q0JDLUhTMjU2In0",
 *    "unprotected":
 *     {"jku":"https://server.example.com/keys.jwks"},
 *    "recipients":[
 *     {"header":
 *       {"alg":"RSA1_5","kid":"2011-04-29"},
 *      "encrypted_key":
 *       "UGhIOguC7IuEvf_NPVaXsGMoLOmwvc1GyqlIKOK1nN94nHPoltGRhWhw7Zx0-
 *        kFm1NJn8LE9XShH59_i8J0PH5ZZyNfGy2xGdULU7sHNF6Gp2vPLgNZ__deLKx
 *        GHZ7PcHALUzoOegEI-8E66jX2E4zyJKx-YxzZIItRzC5hlRirb6Y5Cl_p-ko3
 *        YvkkysZIFNPccxRU7qve1WYPxqbb2Yw8kZqa2rMWI5ng8OtvzlV7elprCbuPh
 *        cCdZ6XDP0_F8rkXds2vE4X-ncOIM8hAYHHi29NX0mcKiRaD0-D-ljQTP-cFPg
 *        wCp6X-nZZd9OHBv-B3oWh2TbqmScqXMR4gp_A"},
 *     {"header":
 *       {"alg":"A128KW","kid":"7"},
 *      "encrypted_key":
 *       "6KB707dM9YTIgHtLvtgWQ8mKwboJW3of9locizkDTHzBC2IlrT1oOQ"}],
 *    "iv":
 *     "AxY8DCtDaGlsbGljb3RoZQ",
 *    "ciphertext":
 *     "KDlTtXchhZTGufMYmOYGS4HffxPSUrfmqCHXaI9wOGY",
 *    "tag":
 *     "Mz-VPPyU4RlcuYv1IwIvzw"
 *   }
 *
 *  The flattened JWE ends up like this
 *
 *   {
 *    "protected": "eyJlbmMiOiJBMTI4Q0JDLUhTMjU2In0",
 *    "unprotected": {"jku":"https://server.example.com/keys.jwks"},
 *    "header": {"alg":"A128KW","kid":"7"},
 *    "encrypted_key": "6KB707dM9YTIgHtLvtgWQ8mKwboJW3of9locizkDTHzBC2IlrT1oOQ",
 *    "iv": "AxY8DCtDaGlsbGljb3RoZQ",
 *    "ciphertext": "KDlTtXchhZTGufMYmOYGS4HffxPSUrfmqCHXaI9wOGY",
 *    "tag": "Mz-VPPyU4RlcuYv1IwIvzw"
 *   }
 *
 *    {
 *      "protected":"<integrity-protected header contents>",
 *      "unprotected":<non-integrity-protected header contents>,
 *      "header":<more non-integrity-protected header contents>,
 *      "encrypted_key":"<encrypted key contents>",
 *      "aad":"<additional authenticated data contents>",
 *      "iv":"<initialization vector contents>",
 *      "ciphertext":"<ciphertext contents>",
 *      "tag":"<authentication tag contents>"
 *     }
 */

int
lws_jwe_render_flattened(struct lws_jwe *jwe, char *out, size_t out_len)
{
	char buf[3072], *p1, *end1, protected[128];
	int m, n, jlen, plen;

	jlen = lws_jose_render(&jwe->jose, jwe->jws.jwk, buf, sizeof(buf));
	if (jlen < 0) {
		lwsl_err("%s: lws_jose_render failed\n", __func__);

		return -1;
	}

	/*
	 * prepare the JWE JSON with all the parts in
	 */

	p1 = out;
	end1 = out + out_len - 1;

	/*
	 * The protected header is b64url encoding of the JOSE header part
	 */

	plen = lws_snprintf(protected, sizeof(protected),
			    "{\"alg\":\"%s\",\"enc\":\"%s\"}",
			    jwe->jose.alg->alg, jwe->jose.enc_alg->alg);

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "{\"protected\":\"");
	jwe->jws.map_b64.buf[LJWS_JOSE] = p1;
	n = lws_jws_base64_enc(protected, (size_t)plen, p1, lws_ptr_diff_size_t(end1, p1));
	if (n < 0) {
		lwsl_notice("%s: failed to encode protected\n", __func__);
		goto bail;
	}
	jwe->jws.map_b64.len[LJWS_JOSE] = (unsigned int)n;
	p1 += n;

	/* unprotected not supported atm */

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\",\n\"header\":");
	lws_strnncpy(p1, buf, jlen, end1 - p1);
	p1 += strlen(p1);

	for (m = 0; m < (int)LWS_ARRAY_SIZE(protected_en); m++)
		if (jwe->jws.map.buf[protected_idx[m]]) {
			p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), ",\n\"%s\":\"",
					   protected_en[m]);
			//jwe->jws.map_b64.buf[protected_idx[m]] = p1;
			n = lws_jws_base64_enc(jwe->jws.map.buf[protected_idx[m]],
					       jwe->jws.map.len[protected_idx[m]],
					       p1, lws_ptr_diff_size_t(end1, p1));
			if (n < 0) {
				lwsl_notice("%s: failed to encode %s\n",
					    __func__, protected_en[m]);
				goto bail;
			}
			//jwe->jws.map_b64.len[protected_idx[m]] = n;
			p1 += n;
			p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\"");
		}

	p1 += lws_snprintf(p1, lws_ptr_diff_size_t(end1, p1), "\n}\n");

	return lws_ptr_diff(p1, out);

bail:
	lws_jws_destroy(&jwe->jws);

	return -1;
}
