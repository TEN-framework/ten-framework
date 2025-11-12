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
 * cose_sign handling
 *
 * Validation:
 *
 *  - we put all our pieces and results in an lwsac in the parse state object
 *
 *  - we collect pieces needed for sig validation into lwsac elements
 *
 *  - we go through each signature making discrete results in the lwsac for
 *    the user code to assess
 */

#include "private-lib-core.h"
#include "private-lib-cose.h"

const uint8_t *sig_mctx[] = { (uint8_t *)"",
				    (uint8_t *)"\x85\x69""Signature",
				    (uint8_t *)"\x84\x6a""Signature1",
				    (uint8_t *)"\x85\x6f""CounterSignature",
				    (uint8_t *)"\x84\x63""MAC",
				    (uint8_t *)"\x84\x64""MAC0",
};
uint8_t sig_mctx_len[] = { 0, 11, 12, 17, 5, 6 };

struct alg_names {
	const char	*name;
	cose_param_t	alg;
} alg_names[] = {
	{ "ES256",	LWSCOSE_WKAECDSA_ALG_ES256 },
	{ "ES384", 	LWSCOSE_WKAECDSA_ALG_ES384 },
	{ "ES512",	LWSCOSE_WKAECDSA_ALG_ES512 },
	{ "HS256_64", 	LWSCOSE_WKAHMAC_256_64 },
	{ "HS256", 	LWSCOSE_WKAHMAC_256_256 },
	{ "HS384", 	LWSCOSE_WKAHMAC_384_384 },
	{ "HS512", 	LWSCOSE_WKAHMAC_512_512 },
	{ "RS256", 	LWSCOSE_WKARSA_ALG_RS256 },
	{ "RS384", 	LWSCOSE_WKARSA_ALG_RS384 },
	{ "RS512", 	LWSCOSE_WKARSA_ALG_RS512 },
};

/*
 * The Sig_structure plaintext is new temp CBOR made up from pieces from the
 * cose_sign, cose_signature, and payload in a specific order
 *
 *  tstr     context string
 *  bstr     0-len or protected body headers
 *  bstr     (Missing for sign1) 0-len or protected signer headers
 *  bstr     0-len or protected application part
 *  bstr     the payload
 *
 * We are getting CBOR with an optional outer tag and then an array of exactly
 * 4 items in a fixed order
 *
 * [
 *   protected headers: bstr containing a map (captured as CBOR in cps->ph[])
 *   unprotected: map: for sign1, eg, the alg (!?), the kid
 *   payload: bstr
 *   if sign: signatures: [ cose_signature struct array,
 *   			    each is a 3-element array
 *     [
 *       protected: bstr containing a map: (eg, the alg) (captured as CBOR)
 *       unprotected: map: (eg, the kid)
 *       signature:  bstr
 *     ]
 *   if sign1: bstr containing signature
 * ]
 *
 * The last signatures field may be an array of signatures, or a single
 * cose_signature object for cose_sign1.
 *
 * For cose_sign1, we know the signature alg before the payload and can do it
 * in a single pass.  But for sign, we do not know the signature algs until
 * after the payload, which is an unfortunate oversight in cose_sign, meaning we
 * cannot hash the payload one or more ways in a single pass.
 */

#if defined(VERBOSE)
const char *cose_sections[] = {
	"ST_UNKNOWN",

	"ST_OUTER_PROTECTED",
	"ST_OUTER_UNPROTECTED",
	"ST_OUTER_PAYLOAD",
	"ST_OUTER_SIGN1_SIGNATURE",

	"ST_OUTER_SIGN_SIGARRAY",

	"ST_OUTER_MACTAG",

	"ST_INNER_PROTECTED",
	"ST_INNER_UNPROTECTED",
	"ST_INNER_SIGNATURE",

	"ST_INNER_EXCESS",
};
#endif

const char *
lws_cose_alg_to_name(cose_param_t alg)
{
	size_t n;

	for (n = 0; n < LWS_ARRAY_SIZE(alg_names); n++)
		if (alg_names[n].alg == alg)
			return alg_names[n].name;

	return "unknown_alg";
}

cose_param_t
lws_cose_name_to_alg(const char *name)
{
	size_t n;

	for (n = 0; n < LWS_ARRAY_SIZE(alg_names); n++)
		if (!strcmp(alg_names[n].name, name))
			return alg_names[n].alg;

	return 0;
}

static size_t
bstr_len(uint8_t *t, size_t buflen, uint8_t opcode, uint64_t len)
{
	uint8_t *ot = t;

	if (buflen < 9)
		return 0;

	if (len < 24) {
		*t = (uint8_t)(opcode | len);

		return 1;
	}
	if (len < 256) {
		*t++ = opcode | LWS_CBOR_1;
		goto b;
	}
	if (len < 65536) {
		*t++ = opcode | LWS_CBOR_2;
		goto b1;
	}
	if (len < 0xffffffffu) {
		*t++ = opcode | LWS_CBOR_4;
		goto b2;
	}

	*t++ = opcode | LWS_CBOR_8;

	*t++ = (uint8_t)(len >> 56);
	*t++ = (uint8_t)(len >> 48);
	*t++ = (uint8_t)(len >> 40);
	*t++ = (uint8_t)(len >> 32);

b2:
	*t++ = (uint8_t)(len >> 24);
	*t++ = (uint8_t)(len >> 16);
b1:
	*t++ = (uint8_t)(len >> 8);
b:
	*t++ = (uint8_t)len;

	return lws_ptr_diff_size_t(t, ot);
}

static int
apply_external(struct lws_cose_validate_context *cps)
{
	lws_cose_sig_alg_t *alg;
	uint8_t t[9];

	alg = lws_container_of(cps->algs.head, lws_cose_sig_alg_t, list);
	if (!alg)
		/* expected if no key */
		return 0;

	/* get the external payload first, if any indicated */

	if (cps->info.ext_len) {
		lws_cose_sig_ext_pay_t ex;
		size_t s;

		s = bstr_len(t, sizeof(t), LWS_CBOR_MAJTYP_BSTR,
			     cps->info.ext_len);
		if (lws_cose_val_alg_hash(alg, t, s))
			return 1;

		memset(&ex, 0, sizeof(ex));
		ex.cps = cps;

		do {
			int n;

			ex.xl = 0;
			n = cps->info.ext_cb(&ex);

			if (ex.xl &&
			    lws_cose_val_alg_hash(alg, ex.ext, ex.xl))
				return 1;

			if (n == LCOSESIGEXTCB_RET_ERROR)
				return 1;

			if (n == LCOSESIGEXTCB_RET_FINISHED)
				break;
		} while (1);
	}

	return 0;
}

static int
create_alg(struct lecp_ctx *ctx, struct lws_cose_validate_context *cps)
{
	lws_cose_validate_param_stack_t *sl = &cps->st[cps->sp], *sl0 = &cps->st[0];
	lws_cose_validate_res_t *res;
	lws_cose_sig_alg_t *alg;
	lws_cose_key_t *ck;
	uint8_t *p;
	size_t s;

	/* with sign1, we can hash the payload in a
	 * single pass */

	ck = lws_cose_key_from_set(cps->info.keyset, sl->kid.buf, sl->kid.len);
	if (!ck) {
		lwsl_notice("%s: no key\n", __func__);
		lwsl_hexdump_notice(sl->kid.buf, sl->kid.len);
		goto no_key_or_alg;
	}

	// lwsl_notice("%s: cps->alg %d\n", __func__, (int)cps->alg);

	alg = lws_cose_val_alg_create(cps->info.cx, ck, cps->st[0].alg,
				      LWSCOSE_WKKO_VERIFY);
	if (!alg) {
		lwsl_info("%s: no alg\n", __func__);

no_key_or_alg:
		/*
		 * We can't create the alg then, so we can't normally
		 * create a result object.  Create one especially for this
		 * case and continue on
		 */

		res = lws_zalloc(sizeof(*res), __func__);
		if (res) {
			res->result = -1001;

			lws_dll2_add_tail(&res->list, &cps->results);
		}

		return 0;
	}

	lws_dll2_add_tail(&alg->list, &cps->algs);

	/*
	 * Hash step 1: The first hash content depends on
	 *              sign/sign1/csign/mac/mac0 constant bstr
	 */

	if (lws_cose_val_alg_hash(alg, sig_mctx[cps->info.sigtype],
			       sig_mctx_len[cps->info.sigtype]))
		goto bail;

	/*
	 * Hash step 2: A zero-length bstr, or a copy of the
	 *              OUTER protected headers
	 *
	 *              A zero-entry map alone becomes a zero-
	 *              length bstr
	 */

	if (sl0->ph_pos[0] < 2) {
		/* nothing to speak of */
		sl0->ph[0][0] = LWS_CBOR_MAJTYP_BSTR;
		p = &sl0->ph[0][0];
		s = 1;
	} else {
		if (sl0->ph_pos[0] < 24) {
			sl0->ph[0][2] = (uint8_t)
			   (LWS_CBOR_MAJTYP_BSTR | sl0->ph_pos[0]);
			p = &sl0->ph[0][2];
			s = (size_t)sl0->ph_pos[0] + 1;
		} else {
			sl0->ph[0][1] = LWS_CBOR_MAJTYP_BSTR |
					LWS_CBOR_1;
			sl0->ph[0][2] = (uint8_t)sl0->ph_pos[0];
			p = &sl0->ph[0][1];
			s = (size_t)sl0->ph_pos[0] + 2;
		}
	}

	if (lws_cose_val_alg_hash(alg, p, s))
		goto bail;

	/*
	 * Hash step 3: Protected signer headers (Elided for sign1)
	 */

	if (cps->info.sigtype == SIGTYPE_MULTI) {
		if (sl->ph_pos[2] < 2) {
			/* nothing to speak of */
			sl->ph[2][0] = LWS_CBOR_MAJTYP_BSTR;
			p = &sl->ph[2][0];
			s = 1;
		} else {
			if (sl->ph_pos[2] < 24) {
				sl->ph[2][2] = (uint8_t)
				   (LWS_CBOR_MAJTYP_BSTR | sl->ph_pos[2]);
				p = &sl->ph[2][2];
				s = (size_t)sl->ph_pos[2] + 1;
			} else {
				sl->ph[2][1] = LWS_CBOR_MAJTYP_BSTR |
						LWS_CBOR_1;
				sl->ph[2][2] = (uint8_t)sl->ph_pos[2];
				p = &sl->ph[2][1];
				s = (size_t)sl->ph_pos[2] + 2;
			}
		}

		if (lws_cose_val_alg_hash(alg, p, s))
			goto bail;
	}

	/* Hash step 4: bstr for applictation protected pieces
	 *              empty for now
	 */

	if (!cps->info.ext_len) { /* ie, if no app data */
		uint8_t u = LWS_CBOR_MAJTYP_BSTR;
		if (lws_cose_val_alg_hash(alg, &u, 1))
			goto bail;
	}

	/*
	 * The final part is the payload in its own bstr, as
	 * we get it if sign1, else replayed from a cache in heap
	 */

	if (cps->info.sigtype == SIGTYPE_SINGLE)
		return 0;

	if (!cps->payload_stash) {
		lwsl_notice("%s: no payload stash\n", __func__);
		goto bail;
	}

	apply_external(cps);

	if (lws_cose_val_alg_hash(alg, cps->payload_stash, cps->payload_pos))
		goto bail;
lwsl_notice("a %d\n", (int)cps->sig_agg_pos);

	lws_cose_val_alg_destroy(cps, &alg, (const uint8_t *)cps->sig_agg,
				 cps->sig_agg_pos);

	return 0;

bail:
	return 1;
}

#if defined(VERBOSE)
static const char * const reason_names[] = {
	"LECPCB_CONSTRUCTED",
	"LECPCB_DESTRUCTED",
	"LECPCB_START",
	"LECPCB_COMPLETE",
	"LECPCB_FAILED",
	"LECPCB_PAIR_NAME",
	"LECPCB_VAL_TRUE",
	"LECPCB_VAL_FALSE",
	"LECPCB_VAL_NULL",
	"LECPCB_VAL_NUM_INT",
	"LECPCB_VAL_RESERVED", /* float in lejp */
	"LECPCB_VAL_STR_START",
	"LECPCB_VAL_STR_CHUNK",
	"LECPCB_VAL_STR_END",
	"LECPCB_ARRAY_START",
	"LECPCB_ARRAY_END",
	"LECPCB_OBJECT_START",
	"LECPCB_OBJECT_END",
	"LECPCB_TAG_START",
	"LECPCB_TAG_END",
	"LECPCB_VAL_NUM_UINT",
	"LECPCB_VAL_UNDEFINED",
	"LECPCB_VAL_FLOAT16",
	"LECPCB_VAL_FLOAT32",
	"LECPCB_VAL_FLOAT64",
	"LECPCB_VAL_SIMPLE",
	"LECPCB_VAL_BLOB_START",
	"LECPCB_VAL_BLOB_CHUNK",
	"LECPCB_VAL_BLOB_END",
	"LECPCB_ARRAY_ITEM_START",
	"LECPCB_ARRAY_ITEM_END",
	"LECPCB_LITERAL_CBOR"
};
#endif

static int
ph_index(struct lws_cose_validate_context *cps)
{
	switch (cps->tli) {
	case ST_OUTER_PROTECTED:
		return 0;
	case ST_OUTER_UNPROTECTED:
		return 1;
	case ST_INNER_PROTECTED:
		return 2;
	case ST_INNER_UNPROTECTED:
		return 3;
	}

	assert(0);
	return 0;
}

static signed char
cb_cose_sig(struct lecp_ctx *ctx, char reason)
{
	struct lws_cose_validate_context *cps =
			(struct lws_cose_validate_context *)ctx->user;
	lws_cose_validate_param_stack_t *sl;
	struct lws_gencrypto_keyelem *ke;
	lws_cose_sig_alg_t *alg;
	uint8_t t[9];
	size_t s;
	int hi;

#if defined(VERBOSE)
	lwsl_notice("%s: %s, tli %s, sub %d, ppos %d, sp %d\n", __func__,
			reason_names[reason & 0x1f], cose_sections[cps->tli],
			cps->sub, ctx->pst[ctx->pst_sp].ppos, cps->sp);
#endif

	switch (reason) {
	case LECPCB_CONSTRUCTED:
		break;

	case LECPCB_TAG_START:

		lwsl_notice("%s: tag sigtype %d\n", __func__, cps->info.sigtype);

		switch (cps->info.sigtype) {
		default:
			assert(0);
			break;
		case SIGTYPE_UNKNOWN:
			/* it means use the tag value to set the type */
			switch (ctx->item.u.u64) {
			case LWSCOAP_CONTENTFORMAT_COSE_SIGN:
				cps->info.sigtype = SIGTYPE_MULTI;
				break;
			case LWSCOAP_CONTENTFORMAT_COSE_SIGN1:
				cps->info.sigtype = SIGTYPE_SINGLE;
				break;
//			case LWSCOAP_CONTENTFORMAT_COSE_SIGN__:
//				cps->info.sigtype = SIGTYPE_COUNTERSIGNED;
//				break;
			case LWSCOAP_CONTENTFORMAT_COSE_MAC0:
				cps->info.sigtype = SIGTYPE_MAC0;
				break;
			case LWSCOAP_CONTENTFORMAT_COSE_MAC:
				cps->info.sigtype = SIGTYPE_MAC;
				break;
			default:
				goto unexpected_tag;
			}
			break;
		case SIGTYPE_MULTI:
			if (ctx->item.u.u64 != LWSCOAP_CONTENTFORMAT_COSE_SIGN)
				goto unexpected_tag;
			break;
		case SIGTYPE_SINGLE:
			if (ctx->item.u.u64 != LWSCOAP_CONTENTFORMAT_COSE_SIGN1)
				goto unexpected_tag;
			break;
		case SIGTYPE_COUNTERSIGNED:
			if (ctx->item.u.u64 != LWSCOAP_CONTENTFORMAT_COSE_SIGN)
				goto unexpected_tag;
			break;
		case SIGTYPE_MAC0:
			if (ctx->item.u.u64 != LWSCOAP_CONTENTFORMAT_COSE_MAC0)
				goto unexpected_tag;
			break;
		case SIGTYPE_MAC:
			if (ctx->item.u.u64 != LWSCOAP_CONTENTFORMAT_COSE_MAC) {
unexpected_tag:
				lwsl_warn("%s: unexpected tag %d\n", __func__,
						(int)ctx->item.u.u64);
				goto bail;
			}
			break;
		}

		cps->depth++;
		break;

	case LECPCB_ARRAY_ITEM_START:

		if (cps->sub)
			break;

		if (ctx->pst[ctx->pst_sp].ppos == 4 ||
		    ctx->pst[ctx->pst_sp].ppos == 6) {
			switch (cps->tli) {
			case ST_INNER_UNPROTECTED:
			case ST_INNER_PROTECTED:
				hi = ph_index(cps);
				sl = &cps->st[cps->sp];
				sl->ph_pos[hi] = 0;
				lecp_parse_report_raw(ctx, 1);
				break;
			default:
				break;
			}
			break;
		}

		if (ctx->pst[ctx->pst_sp].ppos != 2)
			break;

		switch (cps->tli) {
		case ST_OUTER_UNPROTECTED:
		case ST_OUTER_PROTECTED:
			/*
			 * Holy type confusion, Batman... this is a CBOR bstr
			 * containing valid CBOR that must also be parsed as
			 * part of the containing array... we need to collect
			 * it anyway since it is part of the signing plaintext
			 * in bstr form, let's get it and then parse it at the
			 * END of the bstr.
			 */
			lecp_parse_report_raw(ctx, 1);
			break;

		case ST_OUTER_PAYLOAD:
			if (cps->info.sigtype != SIGTYPE_SINGLE)
				break;

			if (create_alg(ctx, cps))
				goto bail;

			break;

		case ST_OUTER_SIGN_SIGARRAY:
			cps->tli = ST_INNER_PROTECTED;
			break;
		}
		break;

	case LECPCB_ARRAY_ITEM_END:

		if (cps->sub)
			break;

		if (ctx->pst[ctx->pst_sp].ppos == 2) {
			sl = &cps->st[cps->sp];
			switch (cps->tli) {
			case ST_OUTER_UNPROTECTED:
				break;
				/* fallthru */
			case ST_OUTER_PROTECTED:
				lecp_parse_report_raw(ctx, 0);

				hi = ph_index(cps);

				if (!sl->ph_pos[hi] || cps->sub)
					break;

				cps->sub = 1;
				s = (size_t)sl->ph_pos[hi];

				if (lecp_parse_subtree(&cps->ctx,
						       sl->ph[hi] + 3, s) !=
							      LECP_CONTINUE)
					goto bail;
				cps->sub = 0;
				break;

			case ST_OUTER_PAYLOAD:
				switch (cps->info.sigtype) {
				case SIGTYPE_MULTI:
					cps->tli = ST_OUTER_SIGN_SIGARRAY - 1;
					break;
				case SIGTYPE_MAC:
				case SIGTYPE_MAC0:
					cps->tli = ST_OUTER_MACTAG - 1;
					break;
				case SIGTYPE_COUNTERSIGNED:
					break;
				default:
					break;
				}
				break;

			case ST_OUTER_SIGN1_SIGNATURE:
			case ST_OUTER_MACTAG:
				cps->sp++;
				cps->tli = ST_INNER_PROTECTED - 1;
				break;

			case ST_INNER_UNPROTECTED:
				lwsl_notice("ST_INNER_UNPROTECTED end\n");
				break;
			case ST_INNER_PROTECTED:
				lwsl_notice("ST_INNER_PROTECTED end\n");
				break;

			case ST_INNER_EXCESS:
			case ST_OUTER_SIGN_SIGARRAY:
				cps->tli--; /* so no change */
				break;
			}
			if (!cps->sub)
				cps->tli++;
		}

		if (ctx->pst[ctx->pst_sp].ppos >= 4) {
			uint8_t *p;
			uint8_t u;
			size_t s1;

			switch (cps->tli) {
			case ST_INNER_UNPROTECTED:
			case ST_INNER_PROTECTED:

				hi = ph_index(cps);
				sl = &cps->st[cps->sp];
				p = sl->ph[hi] + 3;
				lecp_parse_report_raw(ctx, 0);

				if (!sl->ph_pos[hi] || cps->sub) {
					if (!cps->sub)
						cps->tli++;
					break;
				}

				cps->sub = 1;
				s = (size_t)sl->ph_pos[hi];

				/*
				 * somehow the raw captures the
				 * initial BSTR container length,
				 * let's strip it
				 */

				u = (*p) & LWS_CBOR_SUBMASK;
				if (((*p) & LWS_CBOR_MAJTYP_MASK) ==
							LWS_CBOR_MAJTYP_BSTR) {
					s1 = 1;
					if (u == LWS_CBOR_1)
						s1 = 2;
					else if (u == LWS_CBOR_2)
						s1 = 3;
					else if (u == LWS_CBOR_4)
						s1 = 5;
					else if (u == LWS_CBOR_8)
						s1 = 9;

					if (s1 > s)
						goto bail;

					sl->ph_pos[hi] = (int)
						(sl->ph_pos[hi] - (ssize_t)s1);
					s = s - s1;
					memmove(p, p + s1, s);
				}

				if (lecp_parse_subtree(&cps->ctx, p, s) !=
								LECP_CONTINUE)
					goto bail;

				cps->sub = 0;

				if (!cps->sub)
					cps->tli++;
				break;

			case ST_INNER_SIGNATURE:
				if (cps->info.sigtype == SIGTYPE_MAC) {
					// lwsl_err("Y: alg %d\n", (int)cps->alg);
					if (create_alg(ctx, cps))
						goto bail;
				}
				cps->tli++;
				break;
			default:
				break;
			}
		}

		break;

	case LECPCB_VAL_NUM_INT:
	case LECPCB_VAL_NUM_UINT:
		switch (cps->tli) {
		case ST_INNER_PROTECTED:
		case ST_INNER_UNPROTECTED:
		case ST_INNER_SIGNATURE:
		case ST_OUTER_PROTECTED:
		case ST_OUTER_UNPROTECTED:
			if (lecp_parse_map_is_key(ctx)) {
				cps->map_key = ctx->item.u.i64;
				// lwsl_notice("%s: key %d\n", __func__, (int)cps->map_key);
				break;
			}

			// lwsl_notice("%s: key %d val %d\n", __func__, (int)cps->map_key, (int)ctx->item.u.i64);

			if (cps->map_key == LWSCOSE_WKL_ALG) {
				sl = &cps->st[cps->sp];
				cps->map_key = 0;
				if (cps->tli == ST_INNER_PROTECTED ||
				     cps->tli == ST_INNER_UNPROTECTED ||
				     cps->tli == ST_INNER_SIGNATURE) {
					sl->alg = ctx->item.u.i64;
					if (!cps->st[0].alg)
						cps->st[0].alg = sl->alg;
				} else
					sl->alg = ctx->item.u.i64;
				break;
			}
			break;
		}
		break;

	case LECPCB_VAL_STR_END:
		switch (cps->tli) {
		case ST_OUTER_UNPROTECTED:
			break;
		}
		break;

	case LECPCB_VAL_BLOB_START:

		lwsl_notice("%s: blob size %d\n", __func__, (int)ctx->item.u.u64);

		if (cps->tli == ST_OUTER_SIGN1_SIGNATURE ||
		    cps->tli == ST_INNER_SIGNATURE) {
			if (ctx->item.u.u64 > sizeof(cps->sig_agg))
				goto bail;
			cps->sig_agg_pos = 0;
			break;
		}

		if (cps->tli != ST_OUTER_PAYLOAD)
			break;

		if (apply_external(cps)) {
			lwsl_notice("%s: ext\n", __func__);
			goto bail;
		}

		s = bstr_len(t, sizeof(t), LWS_CBOR_MAJTYP_BSTR,
			     ctx->item.u.u64);

		if (cps->info.sigtype == SIGTYPE_SINGLE) {
			alg = lws_container_of(cps->algs.head,
					       lws_cose_sig_alg_t, list);
			if (!alg)
				/* expected if no key */
				break;
			if (lws_cose_val_alg_hash(alg, t, s)) {
				lwsl_notice("%s: hash failed\n", __func__);
				goto bail;
			}

			break;
		}

		cps->payload_stash_size = (size_t)(ctx->item.u.u64 + s);
		cps->payload_stash = lws_malloc(cps->payload_stash_size,
							__func__);
		if (!cps->payload_stash) {
			lwsl_notice("%s: oom\n", __func__);
			goto bail;
		}

		memcpy(cps->payload_stash, t, s);
		cps->payload_pos = s;

		break;

	case LECPCB_VAL_BLOB_CHUNK:
		switch (cps->tli) {
		case ST_OUTER_PAYLOAD:

			if (cps->info.pay_cb && ctx->npos)
				cps->info.pay_cb(cps, cps->info.pay_opaque,
						 (uint8_t *)ctx->buf, ctx->npos);

			if (cps->payload_stash) {
				if (cps->payload_pos + ctx->npos >
					cps->payload_stash_size)
					goto bail;
				memcpy(cps->payload_stash + cps->payload_pos,
						ctx->buf, ctx->npos);
				cps->payload_pos += ctx->npos;
				break;
			}
			alg = lws_container_of(cps->algs.head,
					       lws_cose_sig_alg_t, list);
			if (!alg)
				/* expected if no key */
				break;
			if (ctx->npos &&
			    lws_cose_val_alg_hash(alg, (uint8_t *)ctx->buf,
					      ctx->npos)) {
				lwsl_notice("%s: chunk fail\n", __func__);
				goto bail;
			}
			break;
		case ST_INNER_SIGNATURE:
		case ST_OUTER_SIGN1_SIGNATURE:
			/* the sig is big compared to ctx->buf... we need to
			 * stash it then */
			memcpy(cps->sig_agg + cps->sig_agg_pos, ctx->buf,
				ctx->npos);
			cps->sig_agg_pos = cps->sig_agg_pos + ctx->npos;
			break;
		}
		break;

	case LECPCB_VAL_BLOB_END:
		switch (cps->tli) {

		case ST_INNER_SIGNATURE:
			if (cps->info.sigtype == SIGTYPE_MULTI) {
				memcpy(cps->sig_agg + cps->sig_agg_pos, ctx->buf,
					ctx->npos);
				cps->sig_agg_pos = cps->sig_agg_pos + ctx->npos;
				// lwsl_err("Y: alg %d\n", (int)cps->alg);
				if (create_alg(ctx, cps))
					goto bail;
				break;
			}
			if (cps->info.sigtype != SIGTYPE_MAC)
				break;
			/* fallthru */
		case ST_OUTER_PROTECTED:
		case ST_OUTER_UNPROTECTED:
		case ST_INNER_PROTECTED:
		case ST_INNER_UNPROTECTED:
			if (cps->map_key == LWSCOSE_WKL_KID) {
				sl = &cps->st[cps->sp];
				ke = &sl->kid;
				if (ke->buf)
					lws_free(ke->buf);
				ke->buf = lws_malloc(ctx->npos, __func__);
				if (!ke->buf)
					goto bail;
				ke->len = ctx->npos;
				memcpy(ke->buf, ctx->buf, ctx->npos);
				cps->map_key = 0;
			}
			break;

		case ST_OUTER_PAYLOAD:
			if (cps->info.pay_cb && ctx->npos)
				cps->info.pay_cb(cps, cps->info.pay_opaque,
						 (uint8_t *)ctx->buf, ctx->npos);
			if (cps->payload_stash) {
				if (cps->payload_pos + ctx->npos >
					cps->payload_stash_size)
					goto bail;
				memcpy(cps->payload_stash + cps->payload_pos,
						ctx->buf, ctx->npos);
				cps->payload_pos += ctx->npos;
				break;
			}
			alg = lws_container_of(cps->algs.head,
					       lws_cose_sig_alg_t, list);
			if (!alg)
				/* expected if no key */
				break;

			if (ctx->npos &&
			    lws_cose_val_alg_hash(alg, (uint8_t *)ctx->buf,
					      ctx->npos))
				goto bail;
			break;

		case ST_OUTER_SIGN1_SIGNATURE:
			if (cps->info.sigtype == SIGTYPE_MULTI)
				break;

			memcpy(cps->sig_agg + cps->sig_agg_pos, ctx->buf,
				ctx->npos);
			cps->sig_agg_pos += ctx->npos;

			alg = lws_container_of(cps->algs.head,
					lws_cose_sig_alg_t, list);
			lwsl_notice("b\n");
			if (alg)
				lws_cose_val_alg_destroy(cps, &alg,
							 cps->sig_agg,
							 cps->sig_agg_pos);
			break;

		case ST_OUTER_MACTAG:
			if (cps->mac_pos + ctx->npos > sizeof(cps->mac))
				goto bail;
			memcpy(cps->mac + cps->mac_pos, ctx->buf, ctx->npos);
			cps->mac_pos += ctx->npos;

			if (cps->info.sigtype == SIGTYPE_MAC0) {
				if (create_alg(ctx, cps))
					goto bail;
			}

			break;
		}
		break;

	case LECPCB_LITERAL_CBOR:
		/* only used for protected headers */
		switch (cps->tli) {
		case ST_INNER_PROTECTED:
		case ST_OUTER_PROTECTED:
		case ST_INNER_UNPROTECTED:
		case ST_OUTER_UNPROTECTED:
			sl = &cps->st[cps->sp];
			hi = ph_index(cps);
			if (sl->ph_pos[hi] + 3 + ctx->cbor_pos >
					(int)sizeof(sl->ph[hi]) - 3)
				/* more protected cbor than we can handle */
				goto bail;
			memcpy(sl->ph[hi] + 3 + sl->ph_pos[hi], ctx->cbor,
			       ctx->cbor_pos);
			sl->ph_pos[hi] += ctx->cbor_pos;
			break;
		}
	}

	return 0;

bail:

	return -1;
}

struct lws_cose_validate_context *
lws_cose_validate_create(const lws_cose_validate_create_info_t *info)
{
	struct lws_cose_validate_context *cps;

	/* you have to provide at least one key in a cose_keyset */
	assert(info->keyset);
	/* you have to provide an lws_context (for crypto random) */
	assert(info->cx);

	cps = lws_zalloc(sizeof(*cps), __func__);
	if (!cps)
		return NULL;

	cps->info			= *info;
	cps->tli			= ST_OUTER_PROTECTED;

	lecp_construct(&cps->ctx, cb_cose_sig, cps, NULL, 0);

	return cps;
}

int
lws_cose_validate_chunk(struct lws_cose_validate_context *cps,
			const uint8_t *in, size_t in_len, size_t *used_in)
{
	int n;

	n = lecp_parse(&cps->ctx, in, in_len);
	if (used_in)
		*used_in = cps->ctx.used_in;

	if (n == LECP_CONTINUE)
		return LECP_CONTINUE;

	lecp_destruct(&cps->ctx);

	return n;
}

lws_dll2_owner_t *
lws_cose_validate_results(struct lws_cose_validate_context *cps)
{
	return &cps->results;
}

void
lws_cose_validate_destroy(struct lws_cose_validate_context **_cps)
{
	struct lws_cose_validate_context *cps = *_cps;

	if (!cps)
		return;

	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   lws_dll2_get_head(&cps->algs)) {
		lws_cose_sig_alg_t *alg = lws_container_of(p,
						lws_cose_sig_alg_t, list);

		lws_dll2_remove(p);
		lws_cose_val_alg_destroy(cps, &alg, NULL, 0);
	} lws_end_foreach_dll_safe(p, tp);

	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   lws_dll2_get_head(&cps->results)) {
		lws_cose_validate_res_t *res = lws_container_of(p,
					lws_cose_validate_res_t, list);

		lws_dll2_remove(p);
		lws_free(res);
	} lws_end_foreach_dll_safe(p, tp);

	lws_free_set_NULL(cps->payload_stash);

	lwsac_free(&cps->ac);

	while (cps->sp >= 0) {
		if (cps->st[cps->sp].kid.buf)
			lws_free(cps->st[cps->sp].kid.buf);
		cps->sp--;
	}

	lws_free_set_NULL(*_cps);
}
