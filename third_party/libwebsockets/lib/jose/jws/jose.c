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
 * JOSE is actually specified as part of JWS RFC7515.  JWE references RFC7515
 * to specify its JOSE JSON object.  So it lives in ./lib/jose/jws/jose.c.
 */

#include "private-lib-core.h"
#include "jose/private-lib-jose.h"

#include <stdint.h>

static const char * const jws_jose[] = {
	"alg", /* REQUIRED */
	"jku",
	"jwk",
	"kid",
	"x5u",
	"x5c",
	"x5t",
	"x5t#S256",
	"typ",
	"cty",
	"crit",

	/* valid for JWE only below here */

	"recipients[].header",
	"recipients[].header.alg",
	"recipients[].header.kid",
	"recipients[].encrypted_key",

	"enc",
	"zip", /* ("DEF" = deflate) */

	"epk", /* valid for JWE ECDH only */
	"apu", /* valid for JWE ECDH only */
	"apv", /* valid for JWE ECDH only */
	"iv",  /* valid for JWE AES only */
	"tag", /* valid for JWE AES only */
	"p2s", /* valid for JWE PBES2 only */
	"p2c"  /* valid for JWE PBES2 only */
};

struct jose_cb_args {
	struct lws_jose *jose;

	struct lejp_ctx jwk_jctx; /* fake lejp context used to parse epk */
	struct lws_jwk_parse_state jps; /* fake jwk parse state */

	char *temp;
	int *temp_len;

	unsigned int is_jwe;
	unsigned int recipients_array;

	int recip;
};

/*
 * JWE A.4.7 Complete JWE JSON Serialization example
 *
 * LEJPCB_CONSTRUCTED
 *  LEJPCB_START
 *   LEJPCB_OBJECT_START
 *
 *    protected LEJPCB_PAIR_NAME
 *    protected LEJPCB_VAL_STR_START
 *    protected LEJPCB_VAL_STR_END
 *
 *    unprotected LEJPCB_PAIR_NAME
 *    unprotected LEJPCB_OBJECT_START
 *     unprotected.jku LEJPCB_PAIR_NAME
 *     unprotected.jku LEJPCB_VAL_STR_START
 *     unprotected.jku LEJPCB_VAL_STR_END
 *    unprotected.jku LEJPCB_OBJECT_END
 *
 *    recipients LEJPCB_PAIR_NAME
 *    recipients[] LEJPCB_ARRAY_START
 *
 *     recipients[] LEJPCB_OBJECT_START
 *      recipients[].header LEJPCB_PAIR_NAME
 *      recipients[].header LEJPCB_OBJECT_START
 *       recipients[].header.alg LEJPCB_PAIR_NAME
 *       recipients[].header.alg LEJPCB_VAL_STR_START
 *       recipients[].header.alg LEJPCB_VAL_STR_END
 *       recipients[].header.kid LEJPCB_PAIR_NAME
 *       recipients[].header.kid LEJPCB_VAL_STR_START
 *       recipients[].header.kid LEJPCB_VAL_STR_END
 *      recipients[] LEJPCB_OBJECT_END
 *      recipients[].encrypted_key LEJPCB_PAIR_NAME
 *      recipients[].encrypted_key LEJPCB_VAL_STR_START
 *      recipients[].encrypted_key LEJPCB_VAL_STR_CHUNK
 *      recipients[].encrypted_key LEJPCB_VAL_STR_END
 *     recipients[] LEJPCB_OBJECT_END (ctx->sp = 1)
 *
 *     recipients[] LEJPCB_OBJECT_START
 *      recipients[].header LEJPCB_PAIR_NAME
 *      recipients[].header LEJPCB_OBJECT_START
 *       recipients[].header.alg LEJPCB_PAIR_NAME
 *       recipients[].header.alg LEJPCB_VAL_STR_START
 *       recipients[].header.alg LEJPCB_VAL_STR_END
 *       recipients[].header.kid LEJPCB_PAIR_NAME
 *       recipients[].header.kid LEJPCB_VAL_STR_START
 *       recipients[].header.kid LEJPCB_VAL_STR_END
 *      recipients[] LEJPCB_OBJECT_END
 *      recipients[].encrypted_key LEJPCB_PAIR_NAME
 *      recipients[].encrypted_key LEJPCB_VAL_STR_START
 *      recipients[].encrypted_key LEJPCB_VAL_STR_END
 *     recipients[] LEJPCB_OBJECT_END (ctx->sp = 1)
 *
 *    recipients[] LEJPCB_ARRAY_END
 *
 *    iv LEJPCB_PAIR_NAME
 *    iv LEJPCB_VAL_STR_START
 *    iv LEJPCB_VAL_STR_END
 *    ciphertext LEJPCB_PAIR_NAME
 *    ciphertext LEJPCB_VAL_STR_START
 *    ciphertext LEJPCB_VAL_STR_END
 *    tag LEJPCB_PAIR_NAME
 *    tag LEJPCB_VAL_STR_START
 *    tag LEJPCB_VAL_STR_END
 *
 *   tag LEJPCB_OBJECT_END
 *  tag LEJPCB_COMPLETE
 * tag LEJPCB_DESTRUCTED
 *
 */

/*
 * RFC7516 7.2.2
 *
 * Note that when using the flattened syntax, just as when using the
 * general syntax, any unprotected Header Parameter values can reside in
 * either the "unprotected" member or the "header" member, or in both.
 */

static signed char
lws_jws_jose_cb(struct lejp_ctx *ctx, char reason)
{
	struct jose_cb_args *args = (struct jose_cb_args *)ctx->user;
	int n; //, dest;

	/*
	 * In JOSE JSON, the element "epk" contains a fully-formed JWK.
	 *
	 * For JOSE paths beginning "epk.", we pass them through to a JWK
	 * LEJP subcontext to parse using the JWK parser directly.
	 */

	if (args->is_jwe && !strncmp(ctx->path, "epk.", 4)) {
		memcpy(args->jwk_jctx.path, ctx->path + 4,
		       sizeof(ctx->path) - 4);
		memcpy(args->jwk_jctx.buf, ctx->buf, ctx->npos);
		args->jwk_jctx.npos = ctx->npos;

		if (!ctx->path_match)
			args->jwk_jctx.path_match = 0;
		lejp_check_path_match(&args->jwk_jctx);

		if (args->jwk_jctx.path_match)
			args->jwk_jctx.pst[args->jwk_jctx.pst_sp].
				callback(&args->jwk_jctx, reason);
	}

	// lwsl_notice("%s: %s %d (%d)\n", __func__, ctx->path, reason, ctx->sp);

	/* at the end of each recipients[] entry, bump recipients count */

	if (args->is_jwe && reason == LEJPCB_OBJECT_END && ctx->sp == 1 &&
	    !strcmp(ctx->path, "recipients[]"))
		args->jose->recipients++;

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	//dest = ctx->path_match - 1;

	switch (ctx->path_match - 1) {

	/* strings */

	case LJJHI_ALG: /* REQUIRED */

		/*
		 * look up whether we support this alg and point the caller at
		 * its definition if so
		 */

		if (!args->is_jwe &&
		    lws_gencrypto_jws_alg_to_definition(ctx->buf,
						        &args->jose->alg)) {
			lwsl_notice("%s: unknown alg '%s'\n", __func__,
				    ctx->buf);

			return -1;
		}

		if (args->is_jwe &&
		    lws_gencrypto_jwe_alg_to_definition(ctx->buf,
						        &args->jose->alg)) {
			lwsl_notice("%s: unknown JWE alg '%s'\n", __func__,
				    ctx->buf);

			return -1;
		}

		return 0;

	case LJJHI_TYP: /* Optional: string: media type */
		lws_strnncpy(args->jose->typ, ctx->buf, ctx->npos,
			     sizeof(args->jose->typ));
		break;

	case LJJHI_JKU:	/* Optional: string */
	case LJJHI_KID:	/* Optional: string */
	case LJJHI_X5U:	/* Optional: string: url of public key cert / chain */
	case LJJHI_CTY:	/* Optional: string: content media type */

	/* base64 */

	case LJJHI_X5C:	/* Optional: base64 (NOT -url): actual cert */

	/* base64-url */

	case LJJHI_X5T:	/* Optional: base64url: SHA-1 of actual cert */
	case LJJHI_X5T_S256: /* Optional: base64url: SHA-256 of actual cert */

	/* array of strings */

	case LJJHI_CRIT: /* Optional for send, REQUIRED: array of strings:
			  * mustn't contain standardized strings or null set */
		break;

	/* jwk child */

	case LJJHI_JWK:	/* Optional: jwk JSON object: public key: */

	/* past here, JWE only */

	case LJJHI_RECIPS_HDR:
		if (!args->is_jwe) {
			lwsl_info("%s: recipients in jws\n", __func__);
			return -1;
		}
		args->recipients_array = 1;
		break;

	case LJJHI_RECIPS_HDR_ALG:
	case LJJHI_RECIPS_HDR_KID:
		break;

	case LJJHI_RECIPS_EKEY:
		if (!args->is_jwe) {
			lwsl_info("%s: recipients in jws\n", __func__);
			return -1;
		}
		args->recipients_array = 1;
		//dest = ;
		goto append_string;

	case LJJHI_ENC:	/* JWE only: Mandatory: string */
		if (!args->is_jwe) {
			lwsl_info("%s: enc in jws\n", __func__);
			return -1;
		}
		if (lws_gencrypto_jwe_enc_to_definition(ctx->buf,
							&args->jose->enc_alg)) {
			lwsl_notice("%s: unknown enc '%s'\n", __func__,
				    ctx->buf);

			return -1;
		}
		break;

	case LJJHI_ZIP:	/* JWE only: Optional: string ("DEF" = deflate) */
		if (!args->is_jwe)
			return -1;
		goto append_string;

	case LJJHI_EPK:	/* Additional arg for JWE ECDH */
		if (!args->is_jwe)
			return -1;
		/* Ephemeral key... this JSON subsection is actually a JWK */
		lwsl_err("LJJHI_EPK\n");
		break;

	case LJJHI_APU:	/* Additional arg for JWE ECDH */
		if (!args->is_jwe)
			return -1;
		/* Agreement Party U */
		goto append_string;

	case LJJHI_APV:	/* Additional arg for JWE ECDH */
		if (!args->is_jwe)
			return -1;
		/* Agreement Party V */
		goto append_string;

	case LJJHI_IV:  /* Additional arg for JWE AES */
		if (!args->is_jwe)
			return -1;
		goto append_string;

	case LJJHI_TAG:	/* Additional arg for JWE AES */
		if (!args->is_jwe)
			return -1;
		goto append_string;

	case LJJHI_P2S:	/* Additional arg for JWE PBES2 */
		if (!args->is_jwe)
			return -1;
		goto append_string;
	case LJJHI_P2C:	/* Additional arg for JWE PBES2 */
		if (!args->is_jwe)
			return -1;
		goto append_string;

	/* ignore what we don't understand */

	default:
		return 0;
	}

	return 0;

append_string:

	if (*args->temp_len < ctx->npos) {
		lwsl_err("%s: out of parsing space\n", __func__);
		return -1;
	}

	if (!args->jose->e[ctx->path_match - 1].buf) {
		args->jose->e[ctx->path_match - 1].buf = (uint8_t *)args->temp;
		args->jose->e[ctx->path_match - 1].len = 0;
	}

	memcpy(args->temp, ctx->buf, ctx->npos);
	args->temp += ctx->npos;
	*args->temp_len -= ctx->npos;
	args->jose->e[ctx->path_match - 1].len += ctx->npos;

	if (reason == LEJPCB_VAL_STR_END) {
		n = lws_b64_decode_string_len(
			(const char *)args->jose->e[ctx->path_match - 1].buf,
			(int)args->jose->e[ctx->path_match - 1].len,
			(char *)args->jose->e[ctx->path_match - 1].buf,
			(int)args->jose->e[ctx->path_match - 1].len + 1);
		if (n < 0) {
			lwsl_err("%s: b64 decode failed\n", __func__);
			return -1;
		}

		args->temp -= (int)args->jose->e[ctx->path_match - 1].len - n - 1;
		*args->temp_len +=
			(int)args->jose->e[ctx->path_match - 1].len - n - 1;

		args->jose->e[ctx->path_match - 1].len = (uint32_t)n;
	}

	return 0;
}

void
lws_jose_init(struct lws_jose *jose)
{
	memset(jose, 0, sizeof(*jose));
}

static void
lws_jose_recip_destroy(struct lws_jws_recpient *r)
{
	lws_jwk_destroy(&r->jwk_ephemeral);
	lws_jwk_destroy(&r->jwk);
}

void
lws_jose_destroy(struct lws_jose *jose)
{
	int n;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(jose->recipient); n++)
		lws_jose_recip_destroy(&jose->recipient[n]);
}

static int
lws_jose_parse(struct lws_jose *jose, const uint8_t *buf, int n,
	       char *temp, int *temp_len, int is_jwe)
{
	struct lejp_ctx jctx;
	struct jose_cb_args args;
	int m;

	if (is_jwe) {
		/* prepare a context for JOSE epk ephemeral jwk parsing */
		lws_jwk_init_jps(&args.jps,
				 &jose->recipient[jose->recipients].jwk_ephemeral,
				 NULL, NULL);
		lejp_construct(&args.jwk_jctx, cb_jwk, &args.jps,
				jwk_tok, LWS_ARRAY_SIZE(jwk_tok));
	}

	args.is_jwe		= (unsigned int)is_jwe;
	args.temp		= temp;
	args.temp_len		= temp_len;
	args.jose		= jose;
	args.recip		= 0;
	args.recipients_array	= 0;
	jose->recipients	= 0;

	lejp_construct(&jctx, lws_jws_jose_cb, &args, jws_jose,
		       LWS_ARRAY_SIZE(jws_jose));

	m = lejp_parse(&jctx, (uint8_t *)buf, n);
	lejp_destruct(&jctx);
	if (m < 0) {
		lwsl_notice("%s: parse returned %d\n", __func__, m);
		return -1;
	}

	if (!args.recipients_array && jose->recipient[0].unprot[LJJHI_ALG].buf)
		/* if no explicit recipients[], we got one */
		jose->recipients++;

	return 0;
}

int
lws_jws_parse_jose(struct lws_jose *jose,
		   const char *buf, int len, char *temp, int *temp_len)
{
	return lws_jose_parse(jose, (const uint8_t *)buf, len,
			temp, temp_len, 0);
}

int
lws_jwe_parse_jose(struct lws_jose *jose,
		   const char *buf, int len, char *temp, int *temp_len)
{
	return lws_jose_parse(jose,
			      (const uint8_t *)buf, len, temp, temp_len, 1);
}

int
lws_jose_render(struct lws_jose *jose, struct lws_jwk *aux_jwk,
		char *out, size_t out_len)
{
	struct lws_jwk *jwk;
	char *end = out + out_len - 1;
	int n, m, f, sub = 0, vl;

	/* JOSE requires an alg */
	if (!jose->alg || !jose->alg->alg)
		goto bail;

	*out++ = '{';

	for (n = 0; n < LWS_COUNT_JOSE_HDR_ELEMENTS; n++) {
		switch (n) {

		/* strings */

		case LJJHI_ALG:	/* REQUIRED */
		case LJJHI_JKU:	/* Optional: string */
		case LJJHI_KID:	/* Optional: string */
		case LJJHI_TYP:	/* Optional: string: media type */
		case LJJHI_CTY:	/* Optional: string: content media type */
		case LJJHI_X5U:	/* Optional: string: pubkey cert / chain URL */
		case LJJHI_ENC:	/* JWE only: Optional: string */
		case LJJHI_ZIP:	/* JWE only: Optional: string ("DEF"=deflate) */
			if (jose->e[n].buf) {
				out += lws_snprintf(out, lws_ptr_diff_size_t(end, out),
					"%s\"%s\":\"%s\"", sub ? ",\n" : "",
					jws_jose[n], jose->e[n].buf);
				sub = 1;
			}
			break;

		case LJJHI_X5T:	/* Optional: base64url: SHA-1 of actual cert */
		case LJJHI_X5T_S256: /* Optional: base64url: SHA-256 of cert */
		case LJJHI_APU:	/* Additional arg for JWE ECDH:  b64url */
		case LJJHI_APV:	/* Additional arg for JWE ECDH:  b64url */
		case LJJHI_IV:	/* Additional arg for JWE AES:   b64url */
		case LJJHI_TAG:	/* Additional arg for JWE AES:   b64url */
		case LJJHI_P2S:	/* Additional arg for JWE PBES2: b64url: salt */
			if (jose->e[n].buf) {
				out += lws_snprintf(out, lws_ptr_diff_size_t(end, out),
					"%s\"%s\":\"", sub ? ",\n" : "",
						jws_jose[n]);
				sub = 1;
				m = lws_b64_encode_string_url((const char *)
						jose->e[n].buf, (int)jose->e[n].len,
						out, lws_ptr_diff(end, out));
				if (m < 0)
					return -1;
				out += m;
				out += lws_snprintf(out, lws_ptr_diff_size_t(end, out), "\"");
			}
			break;

		case LJJHI_P2C: /* Additional arg for JWE PBES2: int: count */
			break; /* don't support atm */

		case LJJHI_X5C:	/* Optional: base64 (NOT -url): actual cert */
			if (jose->e[n].buf) {
				out += lws_snprintf(out, lws_ptr_diff_size_t(end, out),
					"%s\"%s\":\"", sub ? ",\n" : "",
							jws_jose[n]);
				sub = 1;
				m = lws_b64_encode_string((const char *)
						jose->e[n].buf, (int)jose->e[n].len,
						out, lws_ptr_diff(end, out));
				if (m < 0)
					return -1;
				out += m;
				out += lws_snprintf(out, lws_ptr_diff_size_t(end, out), "\"");
			}
			break;

		case LJJHI_EPK:	/* Additional arg for JWE ECDH:  eph pubkey */
		case LJJHI_JWK:	/* Optional: jwk JSON object: public key: */

			jwk = n == LJJHI_EPK ? &jose->recipient[0].jwk_ephemeral : aux_jwk;
			if (!jwk || !jwk->kty)
				break;

			out += lws_snprintf(out, lws_ptr_diff_size_t(end, out), "%s\"%s\":",
					    sub ? ",\n" : "", jws_jose[n]);
			sub = 1;
			vl = lws_ptr_diff(end, out);
			m = lws_jwk_export(jwk, 0, out, &vl);
			if (m < 0) {
				lwsl_notice("%s: failed to export key\n",
						__func__);

				return -1;
			}
			out += m;
			break;

		case LJJHI_CRIT:/* Optional for send, REQUIRED: array of strings:
				 * mustn't contain standardized strings or null set */
			if (!jose->e[n].buf)
				break;

			out += lws_snprintf(out, lws_ptr_diff_size_t(end, out),
				"%s\"%s\":[", sub ? ",\n" : "", jws_jose[n]);
			sub = 1;

			m = 0;
			f = 1;
			while ((unsigned int)m < jose->e[n].len && (end - out) > 1) {
				if (jose->e[n].buf[m] == ' ') {
					if (!f)
						*out++ = '\"';

					m++;
					f = 1;
					continue;
				}

				if (f) {
					if (m)
						*out++ = ',';
					*out++ = '\"';
					f = 0;
				}

				*out++ = (char)jose->e[n].buf[m];
				m++;
			}

			break;
		}
	}

	*out++ = '}';

	if (out > end - 2)
		return -1;

	return lws_ptr_diff(out_len, (end - out)) - 1;

bail:
	return -1;
}
