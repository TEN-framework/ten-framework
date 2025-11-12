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
 * cose_key code
 */

#include "private-lib-core.h"
//#include "private-lib-jose.h"

#define lwsl_cose lwsl_notice
#define lwsl_hexdump_cose lwsl_hexdump_notice

// #define VERBOSE 1

struct lws_cose_key_parse_state {
	struct lws_cose_key		*ck;
	/**< single key created here if pkey_set is NULL */
	char				buf[(8192 / 8) + 1];
	/**< enough for 8Kb key, only needed during parse */
	lws_cose_key_import_callback	per_key_cb;
	lws_dll2_owner_t		*pkey_set;
	/**< if non-NULL, expects a [ key set ], else single key */
	void				*user;
	size_t				pos;
	int				cose_state;
	cose_param_t			seen[16];
	int				seen_count;
	int				gencrypto_eidx;
	int				meta_idx;
	unsigned short			possible;
};

/*
 * A COSE key representation is a CBOR map with a specified structure.  The
 * keys are
 *
 * 	LWSCOSE_WKK_KTY			MUST	  int / tstr
 *	LWSCOSE_WKK_KID			OPT       bstr
 *	LWSCOSE_WKK_ALG			OPT	  int / tstr
 *	LWSCOSE_WKK_KEY_OPS		OPT	  [ + (int / tstr) ]
 *	LWSCOSE_WKK_BASE_IV		OPT	  bstr
 */

#if defined(_DEBUG)

static const char *meta_names[] = {
	"kty", "kid", "use", "key_ops", "base_iv", "alg"
};

static const char *oct_names[] = {
	"k"
};

static const char *rsa_names[] = {
	"e", "n", "d", "p", "q", "dp", "dq", "qi", "other", "ri", "di", "ti"
};

static const char *ec_names[] = {
	"crv", "x", "d", "y",
};

void
lws_cose_key_dump(const struct lws_cose_key *ck)
{
	const char **enames;
	char hex[2048];
	int elems;
	int n;

	(void)enames;
	(void)meta_names;

	switch (ck->gencrypto_kty) {

	case LWS_GENCRYPTO_KTY_OCT:
		elems = LWS_GENCRYPTO_OCT_KEYEL_COUNT;
		enames = oct_names;
		break;
	case LWS_GENCRYPTO_KTY_RSA:
		elems = LWS_GENCRYPTO_RSA_KEYEL_COUNT;
		enames = rsa_names;
		break;
	case LWS_GENCRYPTO_KTY_EC:
		elems = LWS_GENCRYPTO_EC_KEYEL_COUNT;
		enames = ec_names;
		break;

	default:
		lwsl_err("%s: jwk %p: unknown type\n", __func__, ck);

		return;
	}

	lwsl_cose("%s: cose_key %p, kty: %lld (gc %d)\n", __func__, ck,
			(long long)ck->kty, ck->gencrypto_kty);

	for (n = 0; n < LWS_COUNT_COSE_KEY_ELEMENTS; n++) {
		if (ck->meta[n].buf) {
			lws_hex_from_byte_array(ck->meta[n].buf, ck->meta[n].len,
						hex, sizeof(hex));
			lwsl_cose("  meta: %s: %s\n", meta_names[n], hex);
		}
	}

	for (n = 0; n < elems; n++) {
		if (ck->e[n].buf) {
			lws_hex_from_byte_array(ck->e[n].buf, ck->e[n].len,
						hex, sizeof(hex));
			lwsl_cose("  e: %s: %s\n", enames[n], hex);
		}
	}
}
#endif

static const char * const kty_strings[] = { NULL,
	"OKP", "EC2", "RSA", "SYMMETRIC", "HSS_LMS", "WALNUTDSA"
};

int
lws_cose_key_checks(const lws_cose_key_t *key, int64_t kty, cose_param_t alg,
		    int key_op, const char *crv)
{
	const struct lws_gencrypto_keyelem *ke;

	/*
	 * we ourselves have to have a very clear idea what we need, even if
	 * matches are optional in the key itself
	 */
	assert(key);
	assert(kty);
	assert(alg);
	assert(key_op);
	assert((kty != LWSCOSE_WKKTV_OKP && kty != LWSCOSE_WKKTV_EC2) || crv);

	/* RFC8152 8.1:
	 *
	 * The 'kty' field MUST be present, and it MUST be '...'.
	 *
	 * But kty can come as an int or a string, but we convert well-known
	 * kty ints to the corresponding string representation at key import
	 */
	if (!kty || kty >= (int)LWS_ARRAY_SIZE(kty_strings)) {
		/* we don't understand it */
		lwsl_notice("%s: unknown kty %d\n", __func__, (int)kty);
		goto bail;
	}

	ke = &key->meta[COSEKEY_META_KTY];
	if (ke->buf && (strlen(kty_strings[kty]) != ke->len ||
			memcmp(kty_strings[kty], ke->buf, ke->len))) {
		lwsl_notice("%s: key is of wrong kty\n", __func__);
		lwsl_hexdump_notice(ke->buf, ke->len);
		goto bail;
	}

	/* ...
	 * If the 'alg' field is present, it MUST match the ... signature
	 * algorithm being used.
	 *
	 * We attempt to convert key alg text representations to a well-known
	 * index, if we can't, then we don't know the alg anyway and should fail
	 * it
	 */

	if (!key->cose_alg && key->meta[COSEKEY_META_ALG].buf) {
		lwsl_notice("%s: alg fail 1\n", __func__);
		goto bail;
	}

	if (key->cose_alg && /* accept it being absent altogether */
	    key->cose_alg != alg) {
		lwsl_notice("%s: alg fail 2\n", __func__);

		goto bail;
	}

	/* ...
	 * If the 'key_ops' field is present, it MUST include 'sign' / 'verify'
	 * when creating /verifying an ... signature.
	 */

	ke = &key->meta[COSEKEY_META_KEY_OPS];
	if (ke->buf && ke->len) {
		uint32_t n;

		for (n = 0; n < ke->len; n++)
			if (ke->buf[n] == key_op)
				break;

		if (n == ke->len)
			goto bail;
	}

	/*
	 * If it's related to EC, check there is a curve associated with the
	 * key, and check it is what we expect
	 */

	if (kty == LWSCOSE_WKKTV_OKP || kty == LWSCOSE_WKKTV_EC2) {
		ke = &key->e[LWS_GENCRYPTO_EC_KEYEL_CRV];

		if (!ke->buf)
			goto bail;
		if (ke->len != strlen(crv))
			goto bail;
		if (memcmp(ke->buf, crv, ke->len))
			goto bail;
	}

	/* We're willing to use this key for this operation */

	return 0;

bail:
	lwsl_notice("%s: key rejected\n", __func__);

	return 1;
}


static int
lws_ck_set_el(struct lws_gencrypto_keyelem *e, char *in, size_t len)
{
	e->buf = lws_malloc(len + 1, "ck");
	if (!e->buf)
		return -1;

	memcpy(e->buf, in, len);
	e->buf[len] = '\0';
	e->len = (uint32_t)len;

	return 0;
}

static struct {
	const char *curve;
	cose_param_t cose_id;
} cose_curves[] = {
	{ "P-256",	LWSCOSE_WKEC_P256 },
	{ "P-384",	LWSCOSE_WKEC_P384 },
	{ "P-521",	LWSCOSE_WKEC_P521 },
	{ "X25519",	LWSCOSE_WKEC_X25519 },
	{ "X448",	LWSCOSE_WKEC_X448 },
	{ "ED25519",	LWSCOSE_WKEC_ED25519 },
	{ "ED448",	LWSCOSE_WKEC_ED448 },
	{ "SECP256K1",	LWSCOSE_WKEC_SECP256K1 },
};

/* 0 means failed */

static cose_param_t
lws_cose_curve_name_to_id(const char *curve)
{
	int n;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(cose_curves); n++)
		if (!strcmp(cose_curves[n].curve, curve))
			return cose_curves[n].cose_id;

	return 0;
}

static const char *
lws_cose_curve_id_to_name(cose_param_t id)
{
	int n;

	for (n = 0; n < (int)LWS_ARRAY_SIZE(cose_curves); n++)
		if (cose_curves[n].cose_id == id)
			return cose_curves[n].curve;

	return 0;
}

static const char * const wk_algs[] = {
	"ES256", "ES384", "ES512"
};
static signed char wk_alg_indexes[] = {
	LWSCOSE_WKAECDSA_ALG_ES256,
	LWSCOSE_WKAECDSA_ALG_ES384,
	LWSCOSE_WKAECDSA_ALG_ES512,
};

static signed char
cb_cose_key(struct lecp_ctx *ctx, char reason)
{
	struct lws_cose_key_parse_state *cps =
			(struct lws_cose_key_parse_state *)ctx->user;
	struct lws_gencrypto_keyelem *ke = NULL;
	const char *p;
	int n;

#if defined(VERBOSE)
	lwsl_notice("%s: reason %d, path %s, ord %u, ppos %d\n", __func__,
			reason & 0x3f,
			ctx->path, ctx->st[ctx->sp - 1].ordinal,
			ctx->pst[ctx->pst_sp].ppos);
#endif

	switch (reason) {
	case LECPCB_OBJECT_START:
		if (cps->ck)
			break;
		goto ak;
	case LECPCB_ARRAY_ITEM_START:
		if (cps->pkey_set && ctx->pst[ctx->pst_sp].ppos == 2) {
			ak:
			cps->ck = lws_zalloc(sizeof(*cps->ck), __func__);
			if (!cps->ck)
				goto bail;
			cps->cose_state = 0;
			cps->meta_idx = -1;
			cps->gencrypto_eidx = -1;
			cps->seen_count = 0;

			if (cps->pkey_set)
				lws_dll2_add_tail(&cps->ck->list, cps->pkey_set);
		}
		break;
	case LECPCB_ARRAY_ITEM_END:
		if (cps->pkey_set && ctx->pst[ctx->pst_sp].ppos == 2) {
			if (cps->per_key_cb)
				cps->per_key_cb(cps->ck, cps->user);
		}
		break;
	case LECPCB_TAG_START:
		if (ctx->item.u.u64 != LWSCOAP_CONTENTFORMAT_COSE_KEY) {
			lwsl_warn("%s: unexpected tag\n", __func__);
			goto bail;
		}
		break;

	case LECPCB_VAL_NUM_INT:
	case LECPCB_VAL_NUM_UINT:
		if (!ctx->sp) {
			lwsl_warn("%s: unexpected uint %d, ppos %d\n",
				  __func__, ctx->sp, ctx->pst[ctx->sp].ppos);
			goto bail;
		}

		if (!lecp_parse_map_is_key(ctx)) {
			const char *kty_str;

			/* value part of map */

			switch (cps->cose_state) {
			case LWSCOSE_WKK_KTY:
				assert(cps->ck);
				cps->ck->kty = (int)ctx->item.u.u64;

				/* convert the cose key type to gencrypto one */
				switch (ctx->item.u.u64) {
				case LWSCOSE_WKKTV_OKP:
					cps->ck->gencrypto_kty =
							LWS_GENCRYPTO_KTY_EC;
					kty_str = "OKP";
					break;
				case LWSCOSE_WKKTV_EC2:
					kty_str = "EC2";
					cps->ck->gencrypto_kty =
							LWS_GENCRYPTO_KTY_EC;
					break;
				case LWSCOSE_WKKTV_RSA:
					kty_str = "RSA";
					cps->ck->gencrypto_kty =
							LWS_GENCRYPTO_KTY_RSA;
					break;
				case LWSCOSE_WKKTV_SYMMETRIC:
					kty_str = "SYMMETRIC";
					cps->ck->gencrypto_kty =
							LWS_GENCRYPTO_KTY_OCT;
					break;
				// case LWSCOSE_WKKTV_HSS_LMS:
				// case LWSCOSE_WKKTV_WALNUTDSA:
				default:
					lwsl_warn("%s: unknown kty\n", __func__);
					goto bail;
				}

				/* store the string version of the key type */

				ke = &cps->ck->meta[COSEKEY_META_KTY];
				ke->len = (uint32_t)strlen(kty_str);
				ke->buf = lws_malloc(ke->len + 1, __func__);
				if (!ke->buf)
					goto bail;
				memcpy(ke->buf, kty_str, ke->len + 1);
				break;
			case LWSCOSE_WKK_ALG:
				/*
				 * He can tie the key to a cose alg code
				 */
				cps->ck->cose_alg = (int)ctx->item.u.u64;
				break;
			case LWSCOSE_WKK_KEY_OPS:
				if (!cps->pkey_set &&
				    (ctx->pst[ctx->sp].ppos != 3 ||
				     strcmp(ctx->path, ".[]"))) {
					lwsl_warn("%s: unexpected kops\n",
								__func__);
					goto bail;
				}
				if (cps->pkey_set &&
				    (ctx->pst[ctx->sp].ppos != 5 ||
				     strcmp(ctx->path, "[].[]"))) {
					lwsl_warn("%s: unexpected kops\n",
								__func__);
					goto bail;
				}
				break;
			case LWSCOSE_WKOKP_CRV:
				cps->ck->cose_curve = (int)ctx->item.u.u64;
				p = lws_cose_curve_id_to_name(cps->ck->cose_curve);
				if (p) {
					ke = &cps->ck->e[LWS_GENCRYPTO_EC_KEYEL_CRV];
					ke->len = (uint32_t)strlen(p);
					ke->buf = lws_malloc(ke->len + 1, __func__);
					if (!ke->buf)
						goto bail;
					memcpy(ke->buf, p, ke->len);
					ke->buf[ke->len] = '\0';
				}
				break;
			default:
				lwsl_warn("%s: uint not allowed in state %d\n",
						__func__, cps->cose_state);
				/* int not allowed in this state */
				goto bail;
			}

			cps->cose_state = 0;
			break;
		}

		/* key part of map pair */

		/*
		 * Disallow any of these coming more than once
		 */
		cps->cose_state = (int)ctx->item.u.u64;
		for (n = 0 ; n < cps->seen_count; n++)
			if (cps->seen[n] == cps->cose_state) {
				/* dupe */
				lwsl_warn("%s: duplicate map name %d\n",
						__func__, cps->cose_state);
				goto bail;
			}

		if (cps->seen_count >= (int)LWS_ARRAY_SIZE(cps->seen))
			goto bail;
		cps->seen[cps->seen_count++] = cps->cose_state;

		cps->meta_idx = -1;
		switch ((int)ctx->item.u.u64) {
		case LWSCOSE_WKK_KTY:
			cps->meta_idx = COSEKEY_META_KTY;
			break;
		case LWSCOSE_WKK_KID:
			cps->meta_idx = COSEKEY_META_KID;
			break;
		case LWSCOSE_WKK_ALG:
			cps->meta_idx = COSEKEY_META_ALG;
			break;
		case LWSCOSE_WKK_KEY_OPS:
			cps->meta_idx = COSEKEY_META_KEY_OPS;
			break;
		case LWSCOSE_WKK_BASE_IV:
			cps->meta_idx = COSEKEY_META_BASE_IV;
			break;

		default:
			cps->gencrypto_eidx = -1;

			switch (cps->ck->kty) {
			case LWSCOSE_WKKTV_OKP:
				switch ((int)ctx->item.u.u64) {
				case LWSCOSE_WKOKP_CRV:
					cps->cose_state = LWSCOSE_WKOKP_CRV;
					break;
				case LWSCOSE_WKOKP_X:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_EC_KEYEL_X;
					break;
				case LWSCOSE_WKOKP_D:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_EC_KEYEL_D;
					break;
				default:
					goto bail;
				}
				break;
			case LWSCOSE_WKKTV_EC2:
				switch ((int)ctx->item.u.u64) {
				case LWSCOSE_WKECKP_CRV:
					cps->cose_state = LWSCOSE_WKOKP_CRV;
					break;
				case LWSCOSE_WKECKP_X:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_EC_KEYEL_X;
					break;
				case LWSCOSE_WKECKP_Y:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_EC_KEYEL_Y;
					break;
				case LWSCOSE_WKECKP_D:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_EC_KEYEL_D;
					break;
				default:
					goto bail;
				}
				break;
			case LWSCOSE_WKKTV_RSA:
				switch ((int)ctx->item.u.u64) {
				case LWSCOSE_WKKPRSA_N:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_N;
					break;
				case LWSCOSE_WKKPRSA_E:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_E;
					break;
				case LWSCOSE_WKKPRSA_D:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_D;
					break;
				case LWSCOSE_WKKPRSA_P:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_P;
					break;
				case LWSCOSE_WKKPRSA_Q:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_Q;
					break;
				case LWSCOSE_WKKPRSA_DP:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_DP;
					break;
				case LWSCOSE_WKKPRSA_DQ:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_DQ;
					break;
				case LWSCOSE_WKKPRSA_QINV:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_QI;
					break;
				case LWSCOSE_WKKPRSA_OTHER:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_OTHER;
					break;
				case LWSCOSE_WKKPRSA_RI:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_RI;
					break;
				case LWSCOSE_WKKPRSA_DI:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_DI;
					break;
				case LWSCOSE_WKKPRSA_TI:
					cps->gencrypto_eidx =
						LWS_GENCRYPTO_RSA_KEYEL_TI;
					break;
				default:
					goto bail;
				}
				break;
			case LWSCOSE_WKKTV_SYMMETRIC:
				if (ctx->item.u.i64 != -1 &&
				    ctx->item.u.u64 != LWSCOSE_WKSYMKP_KEY_VALUE)
					goto bail;

				cps->gencrypto_eidx = LWS_GENCRYPTO_OCT_KEYEL_K;
				break;
			default:
				lwsl_warn("%s: unknown kty\n", __func__);
				goto bail;
			}
			break;
		}
		break;

	case LECPCB_VAL_BLOB_START:
		if (!ctx->sp || !(ctx->st[ctx->sp - 1].ordinal & 1)) {
			lwsl_warn("%s: unexpected blob\n", __func__);
			goto bail;
		}

		if (cps->cose_state == COSEKEY_META_KID)
			break;

		/*
		 * Validate the association of the blob now, collect it into
		 * the temp buf in cps and then alloc and copy it into the
		 * related key element when it's at the end and the size known
		 */

		cps->pos = 0;
		if (cps->gencrypto_eidx >= 0) {
			if (cps->ck->e[cps->gencrypto_eidx].buf) {
				lwsl_warn("%s: e[%d] set twice %d\n", __func__,
						cps->gencrypto_eidx,
						cps->ck->e[cps->gencrypto_eidx].len);
				/* key elements must only come at most once */
				goto bail;
			}
			break;
		}
		if (cps->meta_idx >= 0)
			break;

		goto bail;

	case LECPCB_VAL_BLOB_CHUNK:
	case LECPCB_VAL_BLOB_END:
		if (cps->pos + ctx->npos > sizeof(cps->buf)) {
			lwsl_warn("%s: oversize blob\n", __func__);
			goto bail;
		}
		memcpy(cps->buf + cps->pos, ctx->buf, ctx->npos);
		cps->pos += ctx->npos;

		if (reason == LECPCB_VAL_BLOB_CHUNK)
			break;

		/* we have the key element data, let's make the ck element */
		if (cps->gencrypto_eidx >= 0) {

			if (cps->ck->e[cps->gencrypto_eidx].buf)
				break;

			lws_ck_set_el(&cps->ck->e[cps->gencrypto_eidx],
					(char *)cps->buf, cps->pos);
			cps->gencrypto_eidx = -1;
			break;
		}


		if (cps->meta_idx >= 0) {
			lws_ck_set_el(&cps->ck->meta[cps->meta_idx],
					(char *)cps->buf, cps->pos);
			cps->meta_idx = -1;
		}
		cps->pos = 0;
		break;
	case LECPCB_VAL_STR_END:
		if (cps->cose_state == LWSCOSE_WKOKP_CRV) {
			cps->ck->cose_curve = lws_cose_curve_name_to_id(ctx->buf);
			ke = &cps->ck->e[LWS_GENCRYPTO_EC_KEYEL_CRV];
			ke->len = ctx->npos;
			ke->buf = lws_malloc(ctx->npos, __func__);
			if (!ke->buf)
				goto bail;
			memcpy(ke->buf, ctx->buf, ctx->npos);
		}

		if (!lecp_parse_map_is_key(ctx) &&
		    cps->cose_state == LWSCOSE_WKK_ALG) {
			size_t n;

			for (n = 0; n < LWS_ARRAY_SIZE(wk_algs); n++)
				if (ctx->npos == strlen(wk_algs[n]) &&
				    !memcmp(ctx->buf, wk_algs[n], ctx->npos)) {
					cps->ck->cose_alg = wk_alg_indexes[n];
					break;
				}

			if (n == LWS_ARRAY_SIZE(wk_algs))
				/* key is for an alg we don't understand */
				lwsl_warn("%s: key for unknown alg %.*s\n",
					  __func__, (int)ctx->npos, ctx->buf);

			ke = &cps->ck->meta[COSEKEY_META_ALG];
			ke->len = ctx->npos;
			ke->buf = lws_malloc(ctx->npos, __func__);
			if (!ke->buf)
				goto bail;
			memcpy(ke->buf, ctx->buf, ctx->npos);
		}

		break;
	}

	return 0;

bail:
	lwsl_warn("%s: bail\n", __func__);
	lws_cose_key_destroy(&cps->ck);

	if (cps->pkey_set) {
		lws_cose_key_set_destroy(cps->pkey_set);
		cps->pkey_set = NULL;
	}

	return -1;
}

void
lws_cose_key_destroy_elements(struct lws_gencrypto_keyelem *el, int m)
{
	int n;

	if (!el)
		return;

	for (n = 0; n < m; n++)
		if (el[n].buf) {
			/* wipe all key material when it goes out of scope */
			lws_explicit_bzero(el[n].buf, el[n].len);
			lws_free_set_NULL(el[n].buf);
			el[n].len = 0;
		}
}

void
lws_cose_key_destroy(struct lws_cose_key **pck)
{
	struct lws_cose_key *ck = *pck;

	if (!ck)
		return;

	lws_dll2_remove(&ck->list);

	lws_cose_key_destroy_elements(ck->e, LWS_ARRAY_SIZE(ck->e));
	lws_cose_key_destroy_elements(ck->meta, LWS_ARRAY_SIZE(ck->meta));

	lws_free_set_NULL(*pck);
}

static int
lws_cose_key_set_memb_remove(struct lws_dll2 *d, void *user)
{
	lws_cose_key_t *ck = lws_container_of(d, lws_cose_key_t, list);

	lws_dll2_remove(d);
	lws_cose_key_destroy(&ck);

	return 0;
}

void
lws_cose_key_set_destroy(lws_dll2_owner_t *o)
{
	lws_dll2_foreach_safe(o, NULL, lws_cose_key_set_memb_remove);
}

lws_cose_key_t *
lws_cose_key_from_set(lws_dll2_owner_t *set, const uint8_t *kid, size_t kl)
{
	lws_start_foreach_dll(struct lws_dll2 *, p, lws_dll2_get_head(set)) {
		lws_cose_key_t *ck = lws_container_of(p, lws_cose_key_t, list);
		struct lws_gencrypto_keyelem *ke = &ck->meta[COSEKEY_META_KID];

		if (!kid) /* always the first then */
			return ck;

		if (ke->buf && ke->len == (uint32_t)kl &&
		    !memcmp(ke->buf, kid, ke->len))
			return ck;

	} lws_end_foreach_dll(p);

	return NULL;
}

lws_cose_key_t *
lws_cose_key_generate(struct lws_context *context, cose_param_t cose_kty,
		      int use_mask, int bits, const char *curve,
		      const uint8_t *kid, size_t kl)
{
	struct lws_gencrypto_keyelem *ke;
	lws_cose_key_t *ck;
	size_t sn;
	int n;

	ck = lws_zalloc(sizeof(*ck), __func__);
	if (!ck)
		return NULL;

	ck->kty = cose_kty;
	ck->private_key = 1;

	if (use_mask & 0xfffe) {
		int count = 0;

		for (n = 1; n < 15; n++)
			if (use_mask & (1 << n))
				count++;
		ke = &ck->meta[COSEKEY_META_KEY_OPS];
		ke->buf = lws_malloc((size_t)count, __func__);
		if (!ke->buf)
			goto fail;
		ke->len = (uint32_t)count;
		count = 0;
		for (n = 1; n < 15; n++)
			if (use_mask & (1 << n))
				ke->buf[count++] = (uint8_t)n;
	}

	if (kid) {
		ke = &ck->meta[COSEKEY_META_KID];
		ke->buf = lws_malloc(kl, __func__);
		ke->len = (uint32_t)kl;
		memcpy(ke->buf, kid, ke->len);
	}

	switch (cose_kty) {
	case LWSCOSE_WKKTV_RSA:
		{
			struct lws_genrsa_ctx ctx;

			memset(&ctx, 0, sizeof(ctx));
			ck->gencrypto_kty = LWS_GENCRYPTO_KTY_RSA;

			lwsl_notice("%s: generating %d bit RSA key\n",
					__func__, bits);
			n = lws_genrsa_new_keypair(context, &ctx,
						   LGRSAM_PKCS1_1_5,
						   ck->e, bits);
			lws_genrsa_destroy(&ctx);
			if (n) {
				lwsl_err("%s: problem generating RSA key\n",
						__func__);
				goto fail;
			}
		}
		break;
	case LWSCOSE_WKKTV_SYMMETRIC:

		ck->gencrypto_kty = LWS_GENCRYPTO_KTY_OCT;
		sn = (unsigned int)lws_gencrypto_bits_to_bytes(bits);
		ke = &ck->e[LWS_GENCRYPTO_OCT_KEYEL_K];
		ke->buf = lws_malloc(sn, "oct");
		if (!ke->buf)
			goto fail;
		ke->len = (uint32_t)sn;
		if (lws_get_random(context, ke->buf, sn) != sn) {
			lwsl_err("%s: problem getting random\n", __func__);
			goto fail;
		}
		break;

	case LWSCOSE_WKKTV_OKP:
	case LWSCOSE_WKKTV_EC2:
	{
		struct lws_genec_ctx ctx;

		ck->gencrypto_kty = LWS_GENCRYPTO_KTY_EC;

		if (!curve) {
			lwsl_err("%s: must have a named curve\n", __func__);

			goto fail;
		}

		if (lws_genecdsa_create(&ctx, context, NULL))
			goto fail;

		ctx.genec_alg = LEGENEC_ECDSA;
		lwsl_notice("%s: generating ECDSA key on curve %s\n", __func__,
				curve);

		n = lws_genecdsa_new_keypair(&ctx, curve, ck->e);
		lws_genec_destroy(&ctx);
		if (n) {
			lwsl_err("%s: problem generating ECDSA key\n", __func__);
			goto fail;
		}
		/* trim the trailing NUL */
		ck->e[LWS_GENCRYPTO_EC_KEYEL_CRV].len = (uint32_t)strlen(curve);
	}
		break;

	default:
		lwsl_err("%s: unknown kty\n", __func__);
		goto fail;
	}

	return ck;

fail:
	lws_free_set_NULL(ck);

	return NULL;
}

struct lws_cose_key *
lws_cose_key_import(lws_dll2_owner_t *pkey_set, lws_cose_key_import_callback cb,
		    void *user, const uint8_t *in, size_t len)
{
	struct lws_cose_key_parse_state cps;
	struct lecp_ctx ctx;
	int m;

	memset(&cps, 0, sizeof(cps));

	cps.per_key_cb		= cb;
	cps.user		= user;
	cps.pkey_set		= pkey_set;
	cps.gencrypto_eidx	= -1;

	lecp_construct(&ctx, cb_cose_key, &cps, NULL, 0);
	m = lecp_parse(&ctx, in, len);
	lecp_destruct(&ctx);

	if (m < 0) {
		lwsl_notice("%s: parse got %d\n", __func__, m);
		if (cps.pkey_set)
			lws_cose_key_set_destroy(cps.pkey_set);

		return NULL;
	}

	switch (cps.ck->gencrypto_kty) {
	case LWS_GENCRYPTO_KTY_UNKNOWN:
		lwsl_notice("%s: missing or unknown ktys\n", __func__);
		goto bail;
	default:
		break;
	}

	return cps.ck;

bail:
	lws_cose_key_destroy(&cps.ck);
	return NULL;
}

/* gencrypto element orering -> cose key parameters */

static const signed char ckp[3][12] = {
	{ /* LWS_GENCRYPTO_KTY_OCT (1) */
		/* LWS_GENCRYPTO_OCT_KEYEL_K */ LWSCOSE_WKSYMKP_KEY_VALUE,
	},
	{ /* LWS_GENCRYPTO_KTY_RSA (2) */
		/* LWS_GENCRYPTO_RSA_KEYEL_E */       LWSCOSE_WKKPRSA_E,
		/* LWS_GENCRYPTO_RSA_KEYEL_N */       LWSCOSE_WKKPRSA_N,
		/* LWS_GENCRYPTO_RSA_KEYEL_D */       LWSCOSE_WKKPRSA_D,
		/* LWS_GENCRYPTO_RSA_KEYEL_P */       LWSCOSE_WKKPRSA_P,
		/* LWS_GENCRYPTO_RSA_KEYEL_Q */       LWSCOSE_WKKPRSA_Q,
		/* LWS_GENCRYPTO_RSA_KEYEL_DP */      LWSCOSE_WKKPRSA_DP,
		/* LWS_GENCRYPTO_RSA_KEYEL_DQ */      LWSCOSE_WKKPRSA_DQ,
		/* LWS_GENCRYPTO_RSA_KEYEL_QT */      LWSCOSE_WKKPRSA_QINV,
		/* LWS_GENCRYPTO_RSA_KEYEL_OTHER */   LWSCOSE_WKKPRSA_OTHER,
		/* LWS_GENCRYPTO_RSA_KEYEL_RI */      LWSCOSE_WKKPRSA_RI,
		/* LWS_GENCRYPTO_RSA_KEYEL_DI */      LWSCOSE_WKKPRSA_DI,
		/* LWS_GENCRYPTO_RSA_KEYEL_TI */      LWSCOSE_WKKPRSA_TI,
	},
	{ /* LWS_GENCRYPTO_KTY_EC (3) */
		/* LWS_GENCRYPTO_EC_KEYEL_CRV */ LWSCOSE_WKECKP_CRV,
		/* LWS_GENCRYPTO_EC_KEYEL_X */   LWSCOSE_WKECKP_X,
		/* LWS_GENCRYPTO_EC_KEYEL_D */   LWSCOSE_WKECKP_D,
		/* LWS_GENCRYPTO_EC_KEYEL_Y */   LWSCOSE_WKECKP_Y,
	}
};

enum lws_lec_pctx_ret
lws_cose_key_export(lws_cose_key_t *ck, lws_lec_pctx_t *ctx, int flags)
{
	cose_param_t pa = 0;
	int n;

	if (!ctx->opaque[0]) {

		ctx->opaque[0] = 1; /* map pair count */
		ctx->opaque[1] = 1; /* element index */
		ctx->opaque[2] = 0; /* public mask */
		ctx->opaque[3] = 0; /* doing AGAIN */

		switch (ck->gencrypto_kty) {
		case LWS_GENCRYPTO_KTY_OCT:
			/* nothing to differentiate */
			ctx->opaque[2] = 1 << LWS_GENCRYPTO_OCT_KEYEL_K;
			break;
		case LWS_GENCRYPTO_KTY_RSA:
			ctx->opaque[2] = 1 << LWS_GENCRYPTO_RSA_KEYEL_E;
			break;
		case LWS_GENCRYPTO_KTY_EC:
			ctx->opaque[2] = (1 << LWS_GENCRYPTO_EC_KEYEL_X) |
					 (1 << LWS_GENCRYPTO_EC_KEYEL_Y);
			break;
		default:
			goto fail;
		}

		if (flags & LWSJWKF_EXPORT_PRIVATE)
			ctx->opaque[2] = 0xffff;

		/*
		 * We first need to find out how many CBOR map pairs we are
		 * planning to create, so we can set a fixed length map of the
		 * right size.
		 */

		for (n = 0; n < (int)LWS_ARRAY_SIZE(ck->e); n++)
			if ((ctx->opaque[2] & (1 << n)) && ck->e[n].buf)
				ctx->opaque[0]++;

		/*
		 * We always issue kty, others may be
		 *
		 * KID / ALG / KEY_OPS / BASE_IV
		 */

		if (ck->meta[COSEKEY_META_KID].buf)
			ctx->opaque[0]++;
		if (ck->meta[COSEKEY_META_ALG].buf)
			ctx->opaque[0]++;
		if (ck->meta[COSEKEY_META_KEY_OPS].buf)
			ctx->opaque[0]++;
		if (ck->meta[COSEKEY_META_BASE_IV].buf)
			ctx->opaque[0]++;

		lws_lec_int(ctx, LWS_CBOR_MAJTYP_MAP, 0, (uint64_t)ctx->opaque[0]);
		lws_lec_signed(ctx, LWSCOSE_WKK_KTY);
		lws_lec_signed(ctx, (int64_t)ck->kty);

		if (ck->gencrypto_kty == LWS_GENCRYPTO_KTY_EC) {
			struct lws_gencrypto_keyelem *ke =
					&ck->e[LWS_GENCRYPTO_EC_KEYEL_CRV];

			if (!ke->buf ||
			    ck->e[LWS_GENCRYPTO_EC_KEYEL_CRV].len > 10) {
				lwsl_err("%s: no curve type\n", __func__);
				goto fail;
			}

			pa = lws_cose_curve_name_to_id((const char *)ke->buf);
			lws_lec_signed(ctx, LWSCOSE_WKECKP_CRV);
			if (pa)
				lws_lec_signed(ctx, pa);
			else
				lws_lec_printf(ctx, "%.*s",
						(int)ke->len, ke->buf);
		}


		ctx->opaque[1] = COSEKEY_META_KID;
	}

	/*
	 * Start from the second key meta, then do any elements that are set
	 */

	while (ctx->buf != ctx->end) {
		struct lws_gencrypto_keyelem *ke = NULL;
		int cose_key_param = 0;

		if (lws_lec_scratch(ctx))
			break;

		if (ctx->opaque[1] == LWS_ARRAY_SIZE(ck->e) +
				      LWS_COUNT_COSE_KEY_ELEMENTS)
			break;

		if (ctx->opaque[1] >= LWS_COUNT_COSE_KEY_ELEMENTS) {
			n = ctx->opaque[1] - LWS_COUNT_COSE_KEY_ELEMENTS;

			if (ck->gencrypto_kty != LWS_GENCRYPTO_KTY_EC ||
			    n != LWS_GENCRYPTO_EC_KEYEL_CRV) {
				/* we didn't already encode his curve */

				if ((ctx->opaque[2] & (1 << n)) &&
				     ck->e[n].buf && ck->e[n].len) {
					ke = &ck->e[n];
					cose_key_param = ckp[ck->gencrypto_kty - 1][n];
				}
			}
		} else

			switch (ctx->opaque[1]) {

			case COSEKEY_META_KID: /* bstr */
				if (ck->meta[COSEKEY_META_KID].buf) {
					ke = &ck->meta[COSEKEY_META_KID];
					cose_key_param = LWSCOSE_WKK_KID;
					// lwsl_hexdump_notice(ke->buf, ke->len);
				}
				break;

			case COSEKEY_META_ALG: /* int, tstr */
				if (ck->meta[COSEKEY_META_ALG].buf) {
					ke = &ck->meta[COSEKEY_META_ALG];
					cose_key_param = LWSCOSE_WKK_ALG;
				}
				break;

			case COSEKEY_META_KEY_OPS: /* [ int ] */
				if (!ck->meta[COSEKEY_META_KEY_OPS].buf)
					break;
				ke = &ck->meta[COSEKEY_META_KEY_OPS];

				n = (int)ke->len;
				if (n > 10)
					n = 10;

				/*
				 * We copy this array into scratch by hand now we
				 * made sure it will fit, we will never need AGAIN
				 */

				lws_lec_signed(ctx, LWSCOSE_WKK_KEY_OPS);
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_ARRAY, 0, (uint64_t)n);
				memcpy(&ctx->scratch[ctx->scratch_len], ke->buf,
						(size_t)n);
				ctx->scratch_len = (uint8_t)(ctx->scratch_len + (uint8_t)n);
				ke = NULL;
				break;

			case COSEKEY_META_BASE_IV: /* bstr */
				if (ck->meta[COSEKEY_META_BASE_IV].buf) {
					ke = &ck->meta[COSEKEY_META_BASE_IV];
					cose_key_param = LWSCOSE_WKK_BASE_IV;
				}
				break;

			default:
				break;
			}

		if (ke && ke->buf && ke->len) {

			if (!ctx->opaque[3])
				lws_lec_signed(ctx, cose_key_param);

			/* binary string or text string? */
			if (ctx->opaque[1] == COSEKEY_META_KID ||
			    ctx->opaque[1] == COSEKEY_META_BASE_IV ||
			    ctx->opaque[1] >= LWS_COUNT_COSE_KEY_ELEMENTS)
				n = (int)lws_lec_printf(ctx, "%.*b",
							(int)ke->len, ke->buf);
			else
				n = (int)lws_lec_printf(ctx, "%.*s",
							(int)ke->len, ke->buf);

			switch (n) {
			case LWS_LECPCTX_RET_AGAIN:
				ctx->opaque[3] = 1;
				/* dump what we have and come back */
				continue;
			case LWS_LECPCTX_RET_FAIL:
				goto fail;
			case LWS_LECPCTX_RET_FINISHED:
				break;
			}
		}

		/* move on if we finished that guy */
		ctx->opaque[1]++;
		ctx->opaque[3] = 0;
	}

	ctx->used = lws_ptr_diff_size_t(ctx->buf, ctx->start);

	if (ctx->buf == ctx->end || ctx->scratch_len)
		return LWS_LECPCTX_RET_AGAIN;

	ctx->opaque[0] = 0;

	return LWS_LECPCTX_RET_FINISHED;

fail:
	lwsl_notice("%s: failed\n", __func__);

	ctx->opaque[0] = 0;

	return LWS_LECPCTX_RET_FAIL;
}
