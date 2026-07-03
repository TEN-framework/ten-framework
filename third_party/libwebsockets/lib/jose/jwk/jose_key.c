/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
 * JOSE-specific JWK code
 */

#include "private-lib-core.h"
#include "private-lib-jose.h"

#if !defined(LWS_PLAT_OPTEE) && !defined(OPTEE_DEV_KIT)
#include <fcntl.h>
#endif

static const char * const kty_names[] = {
	"unknown",	/* LWS_GENCRYPTO_KTY_UNKNOWN */
	"oct",		/* LWS_GENCRYPTO_KTY_OCT */
	"RSA",		/* LWS_GENCRYPTO_KTY_RSA */
	"EC"		/* LWS_GENCRYPTO_KTY_EC */
};

/*
 * These are the entire legal token set for names in jwk.
 *
 * The first version is used to parse a detached single jwk that don't have any
 * parent JSON context.  The second version is used to parse full jwk objects
 * that has a "keys": [ ] array containing the keys.
 */

const char * const jwk_tok[] = {
	"keys[]",			/* dummy */
	"e", "n", "d", "p", "q", "dp", "dq", "qi", /* RSA */
	"kty",				/* generic */
	"k",				/* symmetric key data */
	"crv", "x", "y",		/* EC (also "D") */
	"kid",				/* generic */
	"use"				/* mutually exclusive with "key_ops" */,
	"key_ops"			/* mutually exclusive with "use" */,
	"x5c",				/* generic */
	"alg"				/* generic */
}, * const jwk_outer_tok[] = {
	"keys[]",
	"keys[].e", "keys[].n", "keys[].d", "keys[].p", "keys[].q", "keys[].dp",
	"keys[].dq", "keys[].qi",

	"keys[].kty", "keys[].k",		/* generic */
	"keys[].crv", "keys[].x", "keys[].y",	/* EC (also "D") */
	"keys[].kid", "keys[].use"	/* mutually exclusive with "key_ops" */,
	"keys[].key_ops",		/* mutually exclusive with "use" */
	"keys[].x5c", "keys[].alg"
};

static unsigned short tok_map[] = {
	F_RSA | F_EC | F_OCT | F_META |		 0xff,
	F_RSA |				F_B64U | F_M | LWS_GENCRYPTO_RSA_KEYEL_E,
	F_RSA |				F_B64U | F_M | LWS_GENCRYPTO_RSA_KEYEL_N,
	F_RSA | F_EC |			F_B64U |       LWS_GENCRYPTO_RSA_KEYEL_D,
	F_RSA |				F_B64U |       LWS_GENCRYPTO_RSA_KEYEL_P,
	F_RSA |				F_B64U |       LWS_GENCRYPTO_RSA_KEYEL_Q,
	F_RSA |				F_B64U |       LWS_GENCRYPTO_RSA_KEYEL_DP,
	F_RSA |				F_B64U |       LWS_GENCRYPTO_RSA_KEYEL_DQ,
	F_RSA |				F_B64U |       LWS_GENCRYPTO_RSA_KEYEL_QI,

	F_RSA | F_EC | F_OCT | F_META |		 F_M | JWK_META_KTY,
		       F_OCT |		F_B64U | F_M | LWS_GENCRYPTO_OCT_KEYEL_K,

		F_EC |				 F_M | LWS_GENCRYPTO_EC_KEYEL_CRV,
		F_EC |			F_B64U | F_M | LWS_GENCRYPTO_EC_KEYEL_X,
		F_EC |			F_B64U | F_M | LWS_GENCRYPTO_EC_KEYEL_Y,

	F_RSA | F_EC | F_OCT | F_META |		       JWK_META_KID,
	F_RSA | F_EC | F_OCT | F_META |		       JWK_META_USE,

	F_RSA | F_EC | F_OCT | F_META |		       JWK_META_KEY_OPS,
	F_RSA | F_EC | F_OCT | F_META | F_B64 |	       JWK_META_X5C,
	F_RSA | F_EC | F_OCT | F_META |		       JWK_META_ALG,
};

struct lexico {
	const char *name;
	int idx;
	char meta;
} lexico_ec[] =  {
	{ "alg",	JWK_META_ALG,			1 },
	{ "crv",	LWS_GENCRYPTO_EC_KEYEL_CRV,	0 },
	{ "d",		LWS_GENCRYPTO_EC_KEYEL_D,	2 | 0 },
	{ "key_ops",	JWK_META_KEY_OPS,		1 },
	{ "kid",	JWK_META_KID,			1 },
	{ "kty",	JWK_META_KTY,			1 },
	{ "use",	JWK_META_USE,			1 },
	{ "x",		LWS_GENCRYPTO_EC_KEYEL_X,	0 },
	{ "x5c",	JWK_META_X5C,			1 },
	{ "y",		LWS_GENCRYPTO_EC_KEYEL_Y,	0 }
}, lexico_oct[] =  {
	{ "alg",	JWK_META_ALG,			1 },
	{ "k",		LWS_GENCRYPTO_OCT_KEYEL_K,	0 },
	{ "key_ops",	JWK_META_KEY_OPS,		1 },
	{ "kid",	JWK_META_KID,			1 },
	{ "kty",	JWK_META_KTY,			1 },
	{ "use",	JWK_META_USE,			1 },
	{ "x5c",	JWK_META_X5C,			1 }
}, lexico_rsa[] =  {
	{ "alg",	JWK_META_ALG,			1 },
	{ "d",		LWS_GENCRYPTO_RSA_KEYEL_D,	2 | 0 },
	{ "dp",		LWS_GENCRYPTO_RSA_KEYEL_DP,	2 | 0 },
	{ "dq",		LWS_GENCRYPTO_RSA_KEYEL_DQ,	2 | 0 },
	{ "e",		LWS_GENCRYPTO_RSA_KEYEL_E,	0 },
	{ "key_ops",	JWK_META_KEY_OPS,		1 },
	{ "kid",	JWK_META_KID,			1 },
	{ "kty",	JWK_META_KTY,			1 },
	{ "n",		LWS_GENCRYPTO_RSA_KEYEL_N,	0 },
	{ "p",		LWS_GENCRYPTO_RSA_KEYEL_P,	2 | 0 },
	{ "q",		LWS_GENCRYPTO_RSA_KEYEL_Q,	2 | 0 },
	{ "qi",		LWS_GENCRYPTO_RSA_KEYEL_QI,	2 | 0 },
	{ "use",	JWK_META_USE,			1 },
	{ "x5c",	JWK_META_X5C,			1 }
};

static int
_lws_jwk_set_el_jwk_b64(struct lws_gencrypto_keyelem *e, char *in, int len)
{
	size_t dec_size = (unsigned int)lws_base64_size(len);
	int n;

	e->buf = lws_malloc(dec_size, "jwk");
	if (!e->buf)
		return -1;

	/* same decoder accepts both url or original styles */

	n = lws_b64_decode_string_len(in, len, (char *)e->buf, (int)dec_size - 1);
	if (n < 0)
		return -1;
	e->len = (uint32_t)n;

	return 0;
}

static int
_lws_jwk_set_el_jwk_b64u(struct lws_gencrypto_keyelem *e, char *in, int len)
{
	size_t dec_size = (size_t)lws_base64_size(len);
	int n;

	e->buf = lws_malloc(dec_size, "jwk");
	if (!e->buf)
		return -1;

	/* same decoder accepts both url or original styles */

	n = lws_b64_decode_string_len(in, len, (char *)e->buf, (int)dec_size - 1);
	if (n < 0)
		return -1;
	e->len = (uint32_t)n;

	return 0;
}


signed char
cb_jwk(struct lejp_ctx *ctx, char reason)
{
	struct lws_jwk_parse_state *jps = (struct lws_jwk_parse_state *)ctx->user;
	struct lws_jwk *jwk = jps->jwk;
	unsigned int idx, n;
	unsigned short poss;
	char dotstar[64];

	if (reason == LEJPCB_VAL_STR_START)
		jps->pos = 0;

	if (reason == LEJPCB_OBJECT_START && ctx->path_match == 0 + 1)
		/*
		 * new keys[] member is starting
		 *
		 * Until we see some JSON names, it could be anything...
		 * there is no requirement for kty to be given first and eg,
		 * ACME specifies the keys must be ordered in lexographic
		 * order - where kty is not first.
		 */
		jps->possible = F_RSA | F_EC | F_OCT;

	if (reason == LEJPCB_OBJECT_END && ctx->path_match == 0 + 1) {
		/* we completed parsing a key */
		if (jps->per_key_cb && jps->possible) {
			if (jps->per_key_cb(jps->jwk, jps->user)) {

				lwsl_notice("%s: user cb halts import\n",
					    __func__);

				return -2;
			}

			/* clear it down */
			lws_jwk_destroy(jps->jwk);
			jps->possible = 0;
		}
	}

	if (reason == LEJPCB_COMPLETE) {

		/*
		 * Now we saw the whole jwk and know the key type, let'jwk insist
		 * that as a whole, it must be consistent and complete.
		 *
		 * The tracking of ->possible bits from even before we know the
		 * kty already makes certain we cannot have key element members
		 * defined that are inconsistent with the key type.
		 */

		for (n = 0; n < LWS_ARRAY_SIZE(tok_map); n++)
			/*
			 * All mandataory elements for the key type
			 * must be present
			 */
			if ((tok_map[n] & jps->possible) && (
			    ((tok_map[n] & (F_M | F_META)) == (F_M | F_META) &&
			     !jwk->meta[tok_map[n] & 0xff].buf) ||
			    ((tok_map[n] & (F_M | F_META)) == F_M &&
			     !jwk->e[tok_map[n] & 0xff].buf))) {
				lwsl_notice("%s: missing %s\n", __func__,
					    jwk_tok[n]);
					return -3;
				}

		/*
		 * When the key may be public or public + private, ensure the
		 * intra-key members related to that are consistent.
		 *
		 * Only RSA keys need extra care, since EC keys are already
		 * confirmed by making CRV, X and Y mandatory and only D
		 * (the singular private part) optional.  For RSA, N and E are
		 * also already known to be present using mandatory checking.
		 */

		/*
		 * If a private key, it must have all D, P and Q.  Public key
		 * must have none of them.
		 */
		if (jwk->kty == LWS_GENCRYPTO_KTY_RSA &&
		    !(((!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf) &&
		      (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].buf) &&
		      (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_Q].buf)) ||
		      (jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf &&
		       jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].buf &&
		       jwk->e[LWS_GENCRYPTO_RSA_KEYEL_Q].buf))
		      ) {
			lwsl_notice("%s: RSA requires D, P and Q for private\n",
				    __func__);
			return -3;
		}

		/*
		 * If the precomputed private key terms appear, they must all
		 * appear together.
		 */
		if (jwk->kty == LWS_GENCRYPTO_KTY_RSA &&
		    !(((!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_DP].buf) &&
		      (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_DQ].buf) &&
		      (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_QI].buf)) ||
		      (jwk->e[LWS_GENCRYPTO_RSA_KEYEL_DP].buf &&
		       jwk->e[LWS_GENCRYPTO_RSA_KEYEL_DQ].buf &&
		       jwk->e[LWS_GENCRYPTO_RSA_KEYEL_QI].buf))
		      ) {
			lwsl_notice("%s: RSA DP, DQ, QI must all appear "
				    "or none\n", __func__);
			return -3;
		}

		/*
		 * The precomputed private key terms must not appear without
		 * the private key itself also appearing.
		 */
		if (jwk->kty == LWS_GENCRYPTO_KTY_RSA &&
		    !jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf &&
		     jwk->e[LWS_GENCRYPTO_RSA_KEYEL_DQ].buf) {
			lwsl_notice("%s: RSA DP, DQ, QI can appear only with "
				    "private key\n", __func__);
			return -3;
		}

		if ((jwk->kty == LWS_GENCRYPTO_KTY_RSA ||
		     jwk->kty == LWS_GENCRYPTO_KTY_EC) &&
		    jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf)
		jwk->private_key = 1;
	}

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	if (ctx->path_match == 0 + 1)
		return 0;

	idx = tok_map[ctx->path_match - 1];
	if ((idx & 0xff) == 0xff)
		return 0;

	switch (idx) {
	/* note: kty is not necessarily first... we have to keep track of
	 * what could match given which element names have already been
	 * seen.  Once kty comes, we confirm it'jwk still possible (ie, it'jwk
	 * not trying to tell us that it'jwk RSA now when we saw a "crv"
	 * earlier) and then reduce the possibilities to just the one that
	 * kty told. */
	case F_RSA | F_EC | F_OCT | F_META | F_M | JWK_META_KTY:

		if (ctx->npos == 3 && !strncmp(ctx->buf, "oct", 3)) {
			if (!(jps->possible & F_OCT))
				goto elements_mismatch;
			jwk->kty = LWS_GENCRYPTO_KTY_OCT;
			jps->possible = F_OCT;
			goto cont;
		}
		if (ctx->npos == 3 && !strncmp(ctx->buf, "RSA", 3)) {
			if (!(jps->possible & F_RSA))
				goto elements_mismatch;
			jwk->kty = LWS_GENCRYPTO_KTY_RSA;
			jps->possible = F_RSA;
			goto cont;
		}
		if (ctx->npos == 2 && !strncmp(ctx->buf, "EC", 2)) {
			if (!(jps->possible & F_EC))
				goto elements_mismatch;
			jwk->kty = LWS_GENCRYPTO_KTY_EC;
			jps->possible = F_EC;
			goto cont;
		}
		lws_strnncpy(dotstar, ctx->buf, ctx->npos, sizeof(dotstar));
		lwsl_err("%s: Unknown KTY '%s'\n", __func__, dotstar);
		return -1;

	default:
cont:
		if (jps->pos + ctx->npos >= (int)sizeof(jps->b64))
			goto bail;

		memcpy(jps->b64 + jps->pos, ctx->buf, ctx->npos);
		jps->pos += ctx->npos;

		if (reason == LEJPCB_VAL_STR_CHUNK)
			return 0;

		/* chunking has been collated */

		poss = idx & (F_RSA | F_EC | F_OCT);
		jps->possible &= poss;
		if (!jps->possible)
			goto elements_mismatch;

		if (idx & F_META) {
			if (_lws_jwk_set_el_jwk(&jwk->meta[idx & 0x7f],
						jps->b64, (unsigned int)jps->pos) < 0)
				goto bail;

			break;
		}

		if (idx & F_B64U) {
			/* key data... do the base64 decode as needed */
			if (_lws_jwk_set_el_jwk_b64u(&jwk->e[idx & 0x7f],
						     jps->b64, jps->pos) < 0)
				goto bail;

			if (jwk->e[idx & 0x7f].len >
					LWS_JWE_LIMIT_KEY_ELEMENT_BYTES) {
				lwsl_notice("%s: oversize keydata\n", __func__);
				goto bail;
			}

			return 0;
		}

		if (idx & F_B64) {

			/* cert data... do non-urlcoded base64 decode */
			if (_lws_jwk_set_el_jwk_b64(&jwk->e[idx & 0x7f],
						    jps->b64, jps->pos) < 0)
				goto bail;
			return 0;
		}

			if (_lws_jwk_set_el_jwk(&jwk->e[idx & 0x7f],
						jps->b64, (unsigned int)jps->pos) < 0)
				goto bail;
		break;
	}

	return 0;

elements_mismatch:
	lwsl_err("%s: jwk elements mismatch\n", __func__);

bail:
	lwsl_err("%s: element failed\n", __func__);

	return -1;
}

int
lws_jwk_import(struct lws_jwk *jwk, lws_jwk_key_import_callback cb, void *user,
	       const char *in, size_t len)
{
	struct lejp_ctx jctx;
	struct lws_jwk_parse_state jps;
	int m;

	lws_jwk_init_jps(&jps, jwk, cb, user);

	lejp_construct(&jctx, cb_jwk, &jps, cb ? jwk_outer_tok: jwk_tok,
		       LWS_ARRAY_SIZE(jwk_tok));

	m = lejp_parse(&jctx, (uint8_t *)in, (int)len);
	lejp_destruct(&jctx);

	if (m < 0) {
		lwsl_notice("%s: parse got %d\n", __func__, m);
		lws_jwk_destroy(jwk);
		return -1;
	}

	switch (jwk->kty) {
	case LWS_GENCRYPTO_KTY_UNKNOWN:
		lwsl_notice("%s: missing or unknown kty\n", __func__);
		lws_jwk_destroy(jwk);
		return -1;
	default:
		break;
	}

	return 0;
}


int
lws_jwk_export(struct lws_jwk *jwk, int flags, char *p, int *len)
{
	char *start = p, *end = &p[*len - 1];
	int n, m, limit, first = 1, asym = 0;
	struct lexico *l;

	/* RFC7638 lexicographic order requires
	 *  RSA: e -> kty -> n
	 *  oct: k -> kty
	 *
	 * ie, meta and key data elements appear interleaved in name alpha order
	 */

	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "{");

	switch (jwk->kty) {
	case LWS_GENCRYPTO_KTY_OCT:
		l = lexico_oct;
		limit = LWS_ARRAY_SIZE(lexico_oct);
		break;
	case LWS_GENCRYPTO_KTY_RSA:
		l = lexico_rsa;
		limit = LWS_ARRAY_SIZE(lexico_rsa);
		asym = 1;
		break;
	case LWS_GENCRYPTO_KTY_EC:
		l = lexico_ec;
		limit = LWS_ARRAY_SIZE(lexico_ec);
		asym = 1;
		break;
	default:
		return -1;
	}

	for (n = 0; n < limit; n++) {
		const char *q, *q_end;
		char tok[12];
		int pos = 0, f = 1;

		if ((l->meta & 1) && (jwk->meta[l->idx].buf ||
				      l->idx == (int)JWK_META_KTY)) {

			switch (l->idx) {
			case JWK_META_KTY:
				if (!first)
					*p++ = ',';
				first = 0;
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "\"%s\":\"%s\"",
						  l->name, kty_names[jwk->kty]);
				break;
			case JWK_META_KEY_OPS:
				if (!first)
					*p++ = ',';
				first = 0;
				q = (const char *)jwk->meta[l->idx].buf;
				q_end = q + jwk->meta[l->idx].len;

				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
						  "\"%s\":[", l->name);
				/*
				 * For the public version, usages that
				 * require the private part must be
				 * snipped
				 */

				while (q < q_end) {
					if (*q != ' ' && pos < (int)sizeof(tok) - 1) {
						tok[pos++] = *q++;
						if (q != q_end)
							continue;
					}
					tok[pos] = '\0';
					pos = 0;
					if ((flags & LWSJWKF_EXPORT_PRIVATE) ||
					    !asym || (strcmp(tok, "sign") &&
						      strcmp(tok, "encrypt"))) {
						if (!f)
							*p++ = ',';
						f = 0;
						p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
							"\"%s\"", tok);
					}
					q++;
				}

				*p++ = ']';

				break;

			default:
				/* both sig and enc require asym private key */
				if (!(flags & LWSJWKF_EXPORT_PRIVATE) &&
				    asym && l->idx == (int)JWK_META_USE)
					break;
				if (!first)
					*p++ = ',';
				first = 0;
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "\"%s\":\"",
						  l->name);
				lws_strnncpy(p, (const char *)jwk->meta[l->idx].buf,
					     jwk->meta[l->idx].len, end - p);
				p += strlen(p);
				p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "\"");
				break;
			}
		}

		if ((!(l->meta & 1)) && jwk->e[l->idx].buf &&
		    ((flags & LWSJWKF_EXPORT_PRIVATE) || !(l->meta & 2))) {
			if (!first)
				*p++ = ',';
			first = 0;

			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "\"%s\":\"", l->name);

			if (jwk->kty == LWS_GENCRYPTO_KTY_EC &&
			    l->idx == (int)LWS_GENCRYPTO_EC_KEYEL_CRV) {
				lws_strnncpy(p,
					     (const char *)jwk->e[l->idx].buf,
					     jwk->e[l->idx].len, end - p);
				m = (int)strlen(p);
			} else
				m = lws_jws_base64_enc(
					(const char *)jwk->e[l->idx].buf,
					jwk->e[l->idx].len, p, lws_ptr_diff_size_t(end, p) - 4);
			if (m < 0) {
				lwsl_notice("%s: enc failed\n", __func__);
				return -1;
			}
			p += m;
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "\"");
		}

		l++;
	}

	p += lws_snprintf(p, lws_ptr_diff_size_t(end, p),
			  (flags & LWSJWKF_EXPORT_NOCRLF) ? "}" : "}\n");

	*len -= lws_ptr_diff(p, start);

	return lws_ptr_diff(p, start);
}

int
lws_jwk_load(struct lws_jwk *jwk, const char *filename,
	     lws_jwk_key_import_callback cb, void *user)
{
	unsigned int buflen = 4096;
	char *buf = lws_malloc(buflen, "jwk-load");
	int n;

	if (!buf)
		return -1;

	n = lws_plat_read_file(filename, buf, buflen);
	if (n < 0)
		goto bail;

	n = lws_jwk_import(jwk, cb, user, buf, (unsigned int)n);
	lws_free(buf);

	return n;
bail:
	lws_free(buf);

	return -1;
}

int
lws_jwk_save(struct lws_jwk *jwk, const char *filename)
{
	int buflen = 4096;
	char *buf = lws_malloc((unsigned int)buflen, "jwk-save");
	int n, m;

	if (!buf)
		return -1;

	n = lws_jwk_export(jwk, LWSJWKF_EXPORT_PRIVATE, buf, &buflen);
	if (n < 0)
		goto bail;

	m = lws_plat_write_file(filename, buf, (size_t)n);

	lws_free(buf);
	if (m)
		return -1;

	return 0;

bail:
	lws_free(buf);

	return -1;
}
