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
 * Stream parser for RFC8949 CBOR
 */

#include "private-lib-core.h"
#include <string.h>
#include <stdio.h>

#if defined(LWS_WITH_CBOR_FLOAT)
#include <math.h>
#endif

#define lwsl_lecp lwsl_debug

static const char * const parser_errs[] = {
	"",
	"",
	"Bad CBOR coding",
	"Unknown",
	"Parser callback errored (see earlier error)",
	"Overflow"
};

enum lecp_states {
	LECP_OPC,
	LECP_COLLECT,
	LECP_SIMPLEX8,
	LECP_COLLATE,
	LECP_ONLY_SAME
};

void
lecp_construct(struct lecp_ctx *ctx, lecp_callback cb, void *user,
	       const char * const *paths, unsigned char count_paths)
{
	uint16_t x = 0x1234;

	memset(ctx, 0, sizeof(*ctx) - sizeof(ctx->buf));

	ctx->user		= user;
	ctx->pst[0].cb		= cb;
	ctx->pst[0].paths	= paths;
	ctx->pst[0].count_paths = count_paths;
	ctx->be			= *((uint8_t *)&x) == 0x12;

	ctx->st[0].s		= LECP_OPC;

	ctx->pst[0].cb(ctx, LECPCB_CONSTRUCTED);
}

void
lecp_destruct(struct lecp_ctx *ctx)
{
	/* no allocations... just let callback know what it happening */
	if (ctx->pst[0].cb)
		ctx->pst[0].cb(ctx, LECPCB_DESTRUCTED);
}

void
lecp_change_callback(struct lecp_ctx *ctx, lecp_callback cb)
{
	ctx->pst[0].cb(ctx, LECPCB_DESTRUCTED);
	ctx->pst[0].cb = cb;
	ctx->pst[0].cb(ctx, LECPCB_CONSTRUCTED);
}


const char *
lecp_error_to_string(int e)
{
	if (e > 0)
		e = 0;
	else
		e = -e;

	if (e >= (int)LWS_ARRAY_SIZE(parser_errs))
		return "Unknown error";

	return parser_errs[e];
}

static void
ex(struct lecp_ctx *ctx, void *_start, size_t len)
{
	struct _lecp_stack *st = &ctx->st[ctx->sp];
	uint8_t *start = (uint8_t *)_start;

	st->s = LECP_COLLECT;
	st->collect_rem = (uint8_t)len;

	if (ctx->be)
		ctx->collect_tgt = start;
	else
		ctx->collect_tgt = start + len - 1;
}

static void
lecp_check_path_match(struct lecp_ctx *ctx)
{
	const char *p, *q;
	size_t s = sizeof(char *);
	int n;

	if (ctx->path_stride)
		s = ctx->path_stride;

	/* we only need to check if a match is not active */
	for (n = 0; !ctx->path_match &&
	     n < ctx->pst[ctx->pst_sp].count_paths; n++) {
		ctx->wildcount = 0;
		p = ctx->path;

		q = *((char **)(((char *)ctx->pst[ctx->pst_sp].paths) +
							((unsigned int)n * s)));

		while (*p && *q) {
			if (*q != '*') {
				if (*p != *q)
					break;
				p++;
				q++;
				continue;
			}
			ctx->wild[ctx->wildcount++] =
				    (uint16_t)lws_ptr_diff_size_t(p, ctx->path);
			q++;
			/*
			 * if * has something after it, match to .
			 * if ends with *, eat everything.
			 * This implies match sequences must be ordered like
			 *  x.*.*
			 *  x.*
			 * if both options are possible
			 */
			while (*p && (*p != '.' || !*q))
				p++;
		}
		if (*p || *q)
			continue;

		ctx->path_match = (uint8_t)(n + 1);
		ctx->path_match_len = ctx->pst[ctx->pst_sp].ppos;
		return;
	}

	if (!ctx->path_match)
		ctx->wildcount = 0;
}

int
lecp_push(struct lecp_ctx *ctx, char s_start, char s_end, char state)
{
	struct _lecp_stack *st = &ctx->st[ctx->sp];

	if (ctx->sp + 1 == LWS_ARRAY_SIZE(ctx->st))
		return LECP_STACK_OVERFLOW;

	if (s_start && ctx->pst[ctx->pst_sp].cb(ctx, s_start))
		return LECP_REJECT_CALLBACK;

	lwsl_lecp("%s: pushing from sp %d, parent "
		  "(opc %d, indet %d, collect_rem %d)\n",
		  __func__, ctx->sp, st->opcode >> 5, st->indet,
		  (int)st->collect_rem);


	st->pop_iss = s_end; /* issue this when we pop back here */
	ctx->st[ctx->sp + 1] = *st;
	ctx->sp++;
	st++;

	st->s			= state;
	st->collect_rem		= 0;
	st->intermediate	= 0;
	st->indet		= 0;
	st->ordinal		= 0;
	st->send_new_array_item = 0;
	st->barrier		= 0;

	return 0;
}

int
lecp_pop(struct lecp_ctx *ctx)
{
	struct _lecp_stack *st;

	assert(ctx->sp);
	ctx->sp--;

	st = &ctx->st[ctx->sp];

	if (st->pop_iss == LECPCB_ARRAY_END) {
		assert(ctx->ipos);
		ctx->ipos--;
	}

	ctx->pst[ctx->pst_sp].ppos = st->p;
	ctx->path[st->p] = '\0';
	lecp_check_path_match(ctx);

	lwsl_lecp("%s: popping to sp %d, parent "
		  "(opc %d, indet %d, collect_rem %d)\n",
		   __func__, ctx->sp, st->opcode >> 5, st->indet,
		   (int)st->collect_rem);

	if (st->pop_iss && ctx->pst[ctx->pst_sp].cb(ctx, st->pop_iss))
		return LECP_REJECT_CALLBACK;

	return 0;
}

static struct _lecp_stack *
lwcp_st_parent(struct lecp_ctx *ctx)
{
	assert(ctx->sp);

	return &ctx->st[ctx->sp - 1];
}

int
lwcp_completed(struct lecp_ctx *ctx, char indet)
{
	int r, il = ctx->ipos;

	ctx->st[ctx->sp].s = LECP_OPC;

	while (ctx->sp && !ctx->st[ctx->sp].barrier) {
		struct _lecp_stack *parent = lwcp_st_parent(ctx);

		lwsl_lecp("%s: sp %d, parent "
			  "(opc %d, indet %d, collect_rem %d)\n",
			  __func__, ctx->sp, parent->opcode >> 5, parent->indet,
			  (int)parent->collect_rem);

		parent->ordinal++;
		if (parent->opcode == LWS_CBOR_MAJTYP_ARRAY) {
			assert(il);
			il--;
			ctx->i[il]++;
			if (!parent->send_new_array_item) {
				if (ctx->pst[ctx->pst_sp].cb(ctx,
						LECPCB_ARRAY_ITEM_END))
					return LECP_REJECT_CALLBACK;
				parent->send_new_array_item = 1;
			}
		}

		if (!indet && parent->indet) {
			lwsl_lecp("%s: abandoning walk as parent needs indet\n", __func__);
			break;
		}

		if (!parent->indet && parent->collect_rem) {
			parent->collect_rem--;
			lwsl_lecp("%s: sp %d, parent (opc %d, indet %d, collect_rem -> %d)\n",
					__func__, ctx->sp, parent->opcode >> 5, parent->indet, (int)parent->collect_rem);

			if (parent->collect_rem) {
				/* more items to come */
				if (parent->opcode == LWS_CBOR_MAJTYP_ARRAY)
					parent->send_new_array_item = 1;
				break;
			}
		}

		lwsl_lecp("%s: parent (opc %d) collect_rem became zero\n", __func__, parent->opcode >> 5);

		ctx->st[ctx->sp - 1].s = LECP_OPC;
		r = lecp_pop(ctx);
		if (r)
			return r;
		indet = 0;
	}

	return 0;
}

static int
lwcp_is_indet_string(struct lecp_ctx *ctx)
{
	if (ctx->st[ctx->sp].indet)
		return 1;

	if (!ctx->sp)
		return 0;

	if (lwcp_st_parent(ctx)->opcode != LWS_CBOR_MAJTYP_BSTR &&
	    lwcp_st_parent(ctx)->opcode != LWS_CBOR_MAJTYP_TSTR)
		return 0;

	if (ctx->st[ctx->sp - 1].indet)
		return 1;

	return 0;
}

static int
report_raw_cbor(struct lecp_ctx *ctx)
{
	struct _lecp_parsing_stack *pst = &ctx->pst[ctx->pst_sp];

	if (!ctx->cbor_pos)
		return 0;

	if (pst->cb(ctx, LECPCB_LITERAL_CBOR))
		return 1;

	ctx->cbor_pos = 0;

	return 0;
}

void
lecp_parse_report_raw(struct lecp_ctx *ctx, int on)
{
	ctx->literal_cbor_report = (uint8_t)on;
	report_raw_cbor(ctx);
}

int
lecp_parse_map_is_key(struct lecp_ctx *ctx)
{
	return lwcp_st_parent(ctx)->opcode == LWS_CBOR_MAJTYP_MAP &&
	       !(lwcp_st_parent(ctx)->ordinal & 1);
}

int
lecp_parse_subtree(struct lecp_ctx *ctx, const uint8_t *in, size_t len)
{
	struct _lecp_stack *st = &ctx->st[++ctx->sp];
	int n;

	st->s			= 0;
	st->collect_rem		= 0;
	st->intermediate	= 0;
	st->indet		= 0;
	st->ordinal		= 0;
	st->send_new_array_item = 0;
	st->barrier		= 1;

	n = lecp_parse(ctx, in, len);
	ctx->sp--;

	return n;
}

int
lecp_parse(struct lecp_ctx *ctx, const uint8_t *cbor, size_t len)
{
	size_t olen = len;
	int ret;

	while (len--) {
		struct _lecp_parsing_stack *pst = &ctx->pst[ctx->pst_sp];
		struct _lecp_stack *st = &ctx->st[ctx->sp];
		uint8_t c, sm, o;
		char to;

		c = *cbor++;

		/*
		 * for, eg, cose_sign, we sometimes need to collect subtrees of
		 * raw CBOR.  Report buffers of it via the callback if we filled
		 * the buffer, or we stopped collecting.
		 */

		if (ctx->literal_cbor_report) {
			ctx->cbor[ctx->cbor_pos++] = c;
			if (ctx->cbor_pos == sizeof(ctx->cbor) &&
			    report_raw_cbor(ctx))
				goto reject_callback;
		}

		switch (st->s) {
		/*
		 * We're getting the nex opcode
		 */
		case LECP_OPC:
			st->opcode = ctx->item.opcode = c & LWS_CBOR_MAJTYP_MASK;
			sm = c & LWS_CBOR_SUBMASK;
			to = 0;

			lwsl_lecp("%s: %d: OPC %d|%d\n", __func__, ctx->sp,
					c >> 5, sm);

			if (c != 0xff && ctx->sp &&
			    ctx->st[ctx->sp - 1].send_new_array_item) {
				ctx->st[ctx->sp - 1].send_new_array_item = 0;
				if (ctx->pst[ctx->pst_sp].cb(ctx,
						LECPCB_ARRAY_ITEM_START))
					goto reject_callback;
			}

			switch (st->opcode) {
			case LWS_CBOR_MAJTYP_UINT:
				ctx->present = LECPCB_VAL_NUM_UINT;
				if (sm < LWS_CBOR_1) {
					ctx->item.u.i64 = (int64_t)sm;
					goto issue;
				}
				goto i2;

			case LWS_CBOR_MAJTYP_INT_NEG:
				ctx->present = LECPCB_VAL_NUM_INT;
				if (sm < 24) {
					ctx->item.u.i64 = (-1ll) - (int64_t)sm;
					goto issue;
				}
i2:
				if (sm >= LWS_CBOR_RESERVED)
					goto bad_coding;
				ctx->item.u.u64 = 0;
				o = (uint8_t)(1 << (sm - LWS_CBOR_1));
				ex(ctx, (uint8_t *)&ctx->item.u.u64, o);
				break;

			case LWS_CBOR_MAJTYP_BSTR:
				to = LECPCB_VAL_BLOB_END - LECPCB_VAL_STR_END;

				/* fallthru */

			case LWS_CBOR_MAJTYP_TSTR:
				/*
				 * The first thing is the string length, it's
				 * going to either be a byte count for the
				 * string or the indefinite length marker
				 * followed by determinite-length chunks of the
				 * same MAJTYP
				 */

				ctx->npos = 0;
				ctx->buf[0] = '\0';

				if (!sm) {
					if ((!ctx->sp || (ctx->sp &&
					    !ctx->st[ctx->sp - 1].intermediate)) &&
					    pst->cb(ctx, (char)(LECPCB_VAL_STR_START + to)))
						goto reject_callback;

					if (pst->cb(ctx, (char)(LECPCB_VAL_STR_END + to)))
						goto reject_callback;
					lwcp_completed(ctx, 0);
					break;
				}

				if (sm < LWS_CBOR_1) {
					ctx->item.u.u64 = (uint64_t)sm;
					if ((!ctx->sp || (ctx->sp &&
					    !ctx->st[ctx->sp - 1].intermediate)) &&
					    pst->cb(ctx, (char)(LECPCB_VAL_STR_START + to)))
						goto reject_callback;

					st->indet = 0;
					st->collect_rem = sm;
					st->s = LECP_COLLATE;
					break;
				}

				if (sm < LWS_CBOR_RESERVED)
					goto i2;

				if (sm != LWS_CBOR_INDETERMINITE)
					goto bad_coding;

				if ((!ctx->sp || (ctx->sp &&
				    !ctx->st[ctx->sp - 1].intermediate)) &&
				    pst->cb(ctx, (char)(LECPCB_VAL_STR_START + to)))
					goto reject_callback;

				st->indet = 1;

				st->p = pst->ppos;
				lecp_push(ctx, 0, (char)(LECPCB_VAL_STR_END + to),
						  LECP_ONLY_SAME);
				break;

			case LWS_CBOR_MAJTYP_ARRAY:
				ctx->npos = 0;
				ctx->buf[0] = '\0';

				if (pst->ppos + 3u >= sizeof(ctx->path))
					goto reject_overflow;

				st->p = pst->ppos;
				ctx->path[pst->ppos++] = '[';
				ctx->path[pst->ppos++] = ']';
				ctx->path[pst->ppos] = '\0';

				lecp_check_path_match(ctx);

				if (ctx->ipos + 1u >= LWS_ARRAY_SIZE(ctx->i))
					goto reject_overflow;

				ctx->i[ctx->ipos++] = 0;

				if (pst->cb(ctx, LECPCB_ARRAY_START))
					goto reject_callback;

				if (!sm) {
					if (pst->cb(ctx, LECPCB_ARRAY_END))
						goto reject_callback;
					pst->ppos = st->p;
					ctx->path[pst->ppos] = '\0';
					ctx->ipos--;
					lecp_check_path_match(ctx);
					lwcp_completed(ctx, 0);
					break;
				}

				ctx->st[ctx->sp].send_new_array_item = 1;

				if (sm < LWS_CBOR_1) {
					st->indet = 0;
					st->collect_rem = sm;
					goto push_a;
				}

				if (sm < LWS_CBOR_RESERVED)
					goto i2;

				if (sm != LWS_CBOR_INDETERMINITE)
					goto bad_coding;

				st->indet = 1;
push_a:
				lecp_push(ctx, 0, LECPCB_ARRAY_END, LECP_OPC);
				break;

			case LWS_CBOR_MAJTYP_MAP:
				ctx->npos = 0;
				ctx->buf[0] = '\0';

				if (pst->ppos + 1u >= sizeof(ctx->path))
					goto reject_overflow;

				st->p = pst->ppos;
				ctx->path[pst->ppos++] = '.';
				ctx->path[pst->ppos] = '\0';

				lecp_check_path_match(ctx);

				if (pst->cb(ctx, LECPCB_OBJECT_START))
					goto reject_callback;

				if (!sm) {
					if (pst->cb(ctx, LECPCB_OBJECT_END))
						goto reject_callback;
					pst->ppos = st->p;
					ctx->path[pst->ppos] = '\0';
					lecp_check_path_match(ctx);
					lwcp_completed(ctx, 0);
					break;
				}
				if (sm < LWS_CBOR_1) {
					st->indet = 0;
					st->collect_rem = (uint64_t)(sm * 2);
					goto push_m;
				}

				if (sm < LWS_CBOR_RESERVED)
					goto i2;

				if (sm != LWS_CBOR_INDETERMINITE)
					goto bad_coding;

				st->indet = 1;
push_m:
				lecp_push(ctx, 0, LECPCB_OBJECT_END, LECP_OPC);
				break;

			case LWS_CBOR_MAJTYP_TAG:
				/* tag has one or another kind of int first */
				if (sm < LWS_CBOR_1) {
					/*
					 * We have a literal tag number, push
					 * to decode the tag body
					 */
					ctx->item.u.u64 = st->tag = (uint64_t)sm;
					goto start_tag_enclosure;
				}
				/*
				 * We have to do more stuff to get the tag
				 * number...
				 */
				goto i2;

			case LWS_CBOR_MAJTYP_FLOAT:
				/*
				 * This can also be a bunch of specials as well
				 * as sizes of float...
				 */
				sm = c & LWS_CBOR_SUBMASK;

				switch (sm) {
				case LWS_CBOR_SWK_FALSE:
					ctx->present = LECPCB_VAL_FALSE;
					goto issue;

				case LWS_CBOR_SWK_TRUE:
					ctx->present = LECPCB_VAL_TRUE;
					goto issue;

				case LWS_CBOR_SWK_NULL:
					ctx->present = LECPCB_VAL_NULL;
					goto issue;

				case LWS_CBOR_SWK_UNDEFINED:
					ctx->present = LECPCB_VAL_UNDEFINED;
					goto issue;

				case LWS_CBOR_M7_SUBTYP_SIMPLE_X8:
					st->s = LECP_SIMPLEX8;
					break;

				case LWS_CBOR_M7_SUBTYP_FLOAT16:
					ctx->present = LECPCB_VAL_FLOAT16;
					ex(ctx, &ctx->item.u.hf, 2);
					break;

				case LWS_CBOR_M7_SUBTYP_FLOAT32:
					ctx->present = LECPCB_VAL_FLOAT32;
					ex(ctx, &ctx->item.u.f, 4);
					break;

				case LWS_CBOR_M7_SUBTYP_FLOAT64:
					ctx->present = LECPCB_VAL_FLOAT64;
					ex(ctx, &ctx->item.u.d, 8);
					break;

				case LWS_CBOR_M7_BREAK:
					if (!ctx->sp ||
					    !ctx->st[ctx->sp - 1].indet)
						goto bad_coding;

					lwcp_completed(ctx, 1);
					break;

				default:
					/* handle as simple */
					ctx->item.u.u64 = (uint64_t)sm;
					if (pst->cb(ctx, LECPCB_VAL_SIMPLE))
						goto reject_callback;
					break;
				}
				break;
			}
			break;

		/*
		 * We're collecting int / float pieces
		 */
		case LECP_COLLECT:
			if (ctx->be)
				*ctx->collect_tgt++ = c;
			else
				*ctx->collect_tgt-- = c;

			if (--st->collect_rem)
				break;

			/*
			 * We collected whatever it was...
			 */

			ctx->npos = 0;
			ctx->buf[0] = '\0';

			switch (st->opcode) {
			case LWS_CBOR_MAJTYP_BSTR:
			case LWS_CBOR_MAJTYP_TSTR:
				st->collect_rem = ctx->item.u.u64;
				if ((!ctx->sp || (ctx->sp &&
				    !ctx->st[ctx->sp - 1].intermediate)) &&
				    pst->cb(ctx, (char)((st->opcode ==
						    LWS_CBOR_MAJTYP_TSTR) ?
							LECPCB_VAL_STR_START :
							LECPCB_VAL_BLOB_START)))
					goto reject_callback;
				st->s = LECP_COLLATE;
				break;

			case LWS_CBOR_MAJTYP_ARRAY:
				st->collect_rem = ctx->item.u.u64;
				lecp_push(ctx, 0, LECPCB_ARRAY_END, LECP_OPC);
				break;

			case LWS_CBOR_MAJTYP_MAP:
				st->collect_rem = ctx->item.u.u64 * 2;
				lecp_push(ctx, 0, LECPCB_OBJECT_END, LECP_OPC);
				break;

			case LWS_CBOR_MAJTYP_TAG:
				st->tag = ctx->item.u.u64;
				goto start_tag_enclosure;

			default:
				/*
				 * ... then issue what we collected as a
				 * literal
				 */

				if (st->opcode == LWS_CBOR_MAJTYP_INT_NEG)
					ctx->item.u.i64 = (-1ll) - ctx->item.u.i64;

				goto issue;
			}
			break;

		case LECP_SIMPLEX8:
			/*
			 * Extended SIMPLE byte for 7|24 opcode, no uses
			 * for it in RFC8949
			 */
			if (c <= LWS_CBOR_INDETERMINITE)
				/*
				 * Duplication of implicit simple values is
				 * denied by RFC8949 3.3
				 */
				goto bad_coding;

			ctx->item.u.u64 = (uint64_t)c;
			if (pst->cb(ctx, LECPCB_VAL_SIMPLE))
				goto reject_callback;

			lwcp_completed(ctx, 0);
			break;

		case LECP_COLLATE:
			/*
			 * let's grab b/t string content into the context
			 * buffer, and issue chunks from there
			 */

			ctx->buf[ctx->npos++] = (char)c;
			if (st->collect_rem)
				st->collect_rem--;

			/* spill at chunk boundaries, or if we filled the buf */
			if (ctx->npos != sizeof(ctx->buf) - 1 &&
			    st->collect_rem)
				break;

			/* spill */
			ctx->buf[ctx->npos] = '\0';

			/* if it's a map name, deal with the path */
			if (ctx->sp && lecp_parse_map_is_key(ctx)) {
				if (lwcp_st_parent(ctx)->ordinal)
					pst->ppos = st->p;
				st->p = pst->ppos;
				if (pst->ppos + ctx->npos > sizeof(ctx->path))
					goto reject_overflow;
				memcpy(&ctx->path[pst->ppos], ctx->buf,
				       (size_t)(ctx->npos + 1));
				pst->ppos = (uint8_t)(pst->ppos + ctx->npos);
				lecp_check_path_match(ctx);
			}

			to = 0;
			if (ctx->item.opcode == LWS_CBOR_MAJTYP_BSTR)
				to = LECPCB_VAL_BLOB_END - LECPCB_VAL_STR_END;

			o = (uint8_t)(LECPCB_VAL_STR_END + to);
			c = (st->collect_rem /* more to come at this layer */ ||
			    /* we or direct parent is indeterminite */
			    lwcp_is_indet_string(ctx));

			if (ctx->sp)
				ctx->st[ctx->sp - 1].intermediate = !!c;
			if (c)
				o--;

			if (pst->cb(ctx, (char)o))
				goto reject_callback;
			ctx->npos = 0;
			ctx->buf[0] = '\0';

			if (ctx->sp && lwcp_st_parent(ctx)->indet)
				st->s = LECP_OPC;
			if (o == LECPCB_VAL_STR_END + to)
				lwcp_completed(ctx, 0);

			break;

		case LECP_ONLY_SAME:
			/*
			 * deterministic sized chunks same MAJTYP as parent
			 * level only (BSTR and TSTR frags inside interderminite
			 * BSTR or TSTR)
			 *
			 * Clean end when we see M7|31
			 */
			if (!ctx->sp) {
				/*
				 * We should only come here by pushing on stack
				 */
				assert(0);
				return LECP_STACK_OVERFLOW;
			}

			if (c == (LWS_CBOR_MAJTYP_FLOAT | LWS_CBOR_M7_BREAK)) {
				/* if's the end of an interdetminite list */
				if (!ctx->sp || !ctx->st[ctx->sp - 1].indet)
					/*
					 * Can't have a break without an
					 * indeterminite parent
					 */
					goto bad_coding;

				if (lwcp_completed(ctx, 1))
					goto reject_callback;
				break;
			}

			if (st->opcode != lwcp_st_parent(ctx)->opcode)
				/*
				 * Fragments have to be of the same type as the
				 * outer opcode
				 */
				goto bad_coding;

			sm = c & LWS_CBOR_SUBMASK;

			if (sm == LWS_CBOR_INDETERMINITE)
				/* indeterminite length frags not allowed */
				goto bad_coding;

			if (sm < LWS_CBOR_1) {
				st->indet = 0;
				st->collect_rem = (uint64_t)sm;
				st->s = LECP_COLLATE;
				break;
			}

			if (sm >= LWS_CBOR_RESERVED)
				goto bad_coding;

			goto i2;

		default:
			assert(0);
			return -1;
		}

		continue;

start_tag_enclosure:
		st->p = pst->ppos;
		ret = lecp_push(ctx, LECPCB_TAG_START, LECPCB_TAG_END, LECP_OPC);
		if (ret)
			return ret;

		continue;

issue:
		if (ctx->item.opcode == LWS_CBOR_MAJTYP_TAG) {
			st->tag = ctx->item.u.u64;
			goto start_tag_enclosure;
		}

		/* we are just a number */

		if (pst->cb(ctx, ctx->present))
			goto reject_callback;

		lwcp_completed(ctx, 0);

	}

	ctx->used_in = olen - len;

	if (!ctx->sp && ctx->st[0].s == LECP_OPC)
		return 0;

	return LECP_CONTINUE;

reject_overflow:
	ret = LECP_STACK_OVERFLOW;
	goto reject;

bad_coding:
	ret = LECP_REJECT_BAD_CODING;
	goto reject;

reject_callback:
	ret = LECP_REJECT_CALLBACK;

reject:
	ctx->pst[ctx->pst_sp].cb(ctx, LECPCB_FAILED);

	return ret;
}



void
lws_lec_init(lws_lec_pctx_t *ctx, uint8_t *buf, size_t len)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->start = ctx->buf = buf;
	ctx->end = ctx->start + len;
	ctx->fmt_pos = 0;
}

void
lws_lec_setbuf(lws_lec_pctx_t *ctx, uint8_t *buf, size_t len)
{
	ctx->start = ctx->buf = buf;
	ctx->end = ctx->start + len;
	ctx->used = 0;
	ctx->vaa_pos = 0;
}

enum lws_lec_pctx_ret
lws_lec_printf(lws_lec_pctx_t *ctx, const char *format, ...)
{
	enum lws_lec_pctx_ret r;
	va_list ap;

	va_start(ap, format);
	r = lws_lec_vsprintf(ctx, format, ap);
	va_end(ap);

	return r;
}

/*
 * Report how many next-level elements inbetween fmt[0] and the matching
 * closure, eg, [] returns 0,  [123] would return 1, [123,456] returns 2, and
 * [123,{'a':[123,456]}] returns 2.  Counts for { } maps are in pairs, ie,
 * {'a':1, 'b': 2} returns 2
 *
 * If there is no closure in the string it returns -1
 *
 * We use this to figure out if we should use indeterminite lengths or specific
 * lengths for items in the format string
 */

#define bump(_r) count[sp]++
//; lwsl_notice("%s: count[%d] -> %d\n", _r, sp, count[sp])

static int
format_scan(const char *fmt)
{
	char stack[12], literal = 0, numeric = 0;
	int count[12], sp = 0, pc = 0, swallow = 0;

	literal = *fmt == '\'';
	stack[sp] = *fmt++;
	count[sp] = 0;

//	lwsl_notice("%s: start %s\n", __func__, fmt - 1);

	while (*fmt) {

//		lwsl_notice("%s: %c %d %d\n", __func__, *fmt, sp, literal);

		if (swallow) {
			swallow--;
			fmt++;
			continue;
		}

		if (numeric) {
			if (*fmt >= '0' && *fmt <= '9')
				fmt++;
			numeric = 0;
			if (*fmt != '(')
				bump("a");
		}

		if (literal) {
			if (*fmt == '\\' && fmt[1]) {
				fmt += 2;
				continue;
			}
			if (*fmt == '\'') {
				literal = 0;
				if (!sp && stack[sp] == '\'')
					return count[sp];

				if (sp)
					sp--;
				fmt++;
				continue;
			}

			bump("b");
			fmt++;
			continue;
		}

		if (*fmt == '\'') {
			bump("c");
			sp++;
			literal = 1;
			fmt++;
			continue;
		}

		switch (pc) {
		case 1:
			if (*fmt == '.') {
				pc++;
				fmt++;
				continue;
			}
			if (*fmt == 'l') {
				pc++;
				fmt++;
				continue;
			}
			/* fallthru */
		case 2:
			if (*fmt == '*') {
				pc++;
				fmt++;
				continue;
			}
			if (*fmt == 'l') {
				pc++;
				fmt++;
				continue;
			}
			/* fallthru */
		case 3:
			bump("pc");
			pc = 0;
			fmt++;
			continue;
		}

		switch (*fmt) {

		case '<':
			swallow = 1;
			/* fallthru */
		case '[':
		case '(':
		case '{':
			if (sp == sizeof(stack))
				return -2;

			bump("d");
			sp++;
			stack[sp] = *fmt;
			count[sp] = 0;
			break;
		case ' ':
			break;
		case ',':
			//count[sp]++;
			break;
		case ':':
			if (stack[sp] != '{')
				goto mismatch;
			//count[sp]++;
			break;
		case '%':
			pc = 1;
			break;
		case ']':
			if (stack[sp] != '[')
				goto mismatch;
			goto pop;
		case ')':
			if (stack[sp] != '(')
				goto mismatch;
			goto pop;
		case '}':
			if (stack[sp] != '{')
				goto mismatch;
			goto pop;
		case '>':
			if (stack[sp] != '<')
				goto mismatch;
pop:
			if (sp) {
				sp--;
				break;
			}

			if (stack[0] == '{') {
				/* args have to come in pairs */
				if (count[0] & 1) {
					lwsl_err("%s: odd map args %d %s\n",
							__func__, count[0], fmt);
					return -2;
				}
				// lwsl_notice("%s: return %d pairs\n", __func__, count[0] >> 1);
				/* report how many pairs */
				return count[0] >> 1;
			}

			// lwsl_notice("%s: return %d items\n", __func__, count[0]);

			return count[0];

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			numeric = 1;

			break;

		default:
			bump("e");
			break;
		}
		fmt++;
	}

	return -1;

mismatch:
	lwsl_err("%s: format mismatch %c %c\n", __func__, stack[sp], *fmt);

	return -2;
}

void
lws_lec_signed(lws_lec_pctx_t *ctx, int64_t num)
{
	if (num < 0)
		lws_lec_int(ctx, LWS_CBOR_MAJTYP_INT_NEG, 0,
					(uint64_t)(-1ll - num));
	else
		lws_lec_int(ctx, LWS_CBOR_MAJTYP_UINT, 0, (uint64_t)num);
}

void
lws_lec_int(lws_lec_pctx_t *ctx, uint8_t opcode, uint8_t indet, uint64_t num)
{
	uint8_t hint = 0;
	unsigned int n;

	if (indet) {
		ctx->scratch[ctx->scratch_len++] = (uint8_t)(opcode |
							LWS_CBOR_INDETERMINITE);
		return;
	}

	if ((opcode & LWS_CBOR_MAJTYP_MASK) == LWS_CBOR_MAJTYP_FLOAT) {
		hint = opcode & LWS_CBOR_SUBMASK;
		switch (hint) {
		case LWS_CBOR_M7_SUBTYP_FLOAT16:
			num <<= 48;
			break;
		case LWS_CBOR_M7_SUBTYP_FLOAT32:
			num <<= 32;
			break;
		}
	} else {

		if (num < LWS_CBOR_1) {
			ctx->scratch[ctx->scratch_len++] = (uint8_t)(opcode | num);
			return;
		}

		if (!(num & (uint64_t)(~0xffull))) {
			hint = LWS_CBOR_1;
			num <<= 56;
		} else
			if (!(num & (uint64_t)(~0xffffull))) {
				hint = LWS_CBOR_2;
				num <<= 48;
			} else
				if (!(num & (uint64_t)(~0xffffffffull))) {
					hint = LWS_CBOR_4;
					num <<= 32;
				}
				else
					hint = LWS_CBOR_8;
	}

	ctx->scratch[ctx->scratch_len++] = (uint8_t)(opcode | hint);
	n = 1u << (hint - LWS_CBOR_1);
	while (n--) {
		ctx->scratch[ctx->scratch_len++] = (uint8_t)(num >> 56);
		num <<= 8;
	}
}

enum {
	NATTYPE_INT,
	NATTYPE_LONG,
	NATTYPE_LONG_LONG,
	NATTYPE_PTR,
	NATTYPE_DOUBLE,
};

int
lws_lec_scratch(lws_lec_pctx_t *ctx)
{
	size_t s;

	if (!ctx->scratch_len)
		return 0;

	s = lws_ptr_diff_size_t(ctx->end, ctx->buf);
	if (s > (size_t)ctx->scratch_len)
		s = (size_t)ctx->scratch_len;

	memcpy(ctx->buf, ctx->scratch, s);
	ctx->buf += s;
	ctx->scratch_len = (uint8_t)(ctx->scratch_len - (uint8_t)s);

	return ctx->buf == ctx->end;
}

enum lws_lec_pctx_ret
lws_lec_vsprintf(lws_lec_pctx_t *ctx, const char *fmt, va_list args)
{
	size_t fl = strlen(fmt);
	uint64_t u64;
	int64_t i64;
#if defined(LWS_WITH_CBOR_FLOAT)
	double dbl;
#endif
	size_t s;
	char c;
	int n;

	/*
	 * We might be being called after the first time, since we had to emit
	 * output buffer(s) before we could move on in the format string.  For
	 * this case, reposition ourselves at the vaarg we got to from the last
	 * call.
	 */

	for (n = 0; n < ctx->vaa_pos; n++) {

		switch (ctx->vaa[n]) {
		case NATTYPE_INT:
			(void)va_arg(args, int);
			break;
		case NATTYPE_LONG:
			(void)va_arg(args, long);
			break;
		case NATTYPE_LONG_LONG:
			(void)va_arg(args, long long);
			break;
		case NATTYPE_PTR:
			(void)va_arg(args, const char *);
			break;
		case NATTYPE_DOUBLE:
			(void)va_arg(args, double);
			break;
		}
		if (ctx->state == CBPS_STRING_BODY)
			/*
			 * when copying out text or binary strings, we reload
			 * the %s or %.*s pointer on subsequent calls, in case
			 * it was on the stack.  The length and contents should
			 * not change between calls, but it's OK if the source
			 * address does.
			 */
			ctx->ongoing_src = va_arg(args, uint8_t *);
	}

	while (ctx->buf != ctx->end) {

		/*
		 * We write small things into the context scratch array, then
		 * copy that into the output buffer fragmenting as needed.  Next
		 * time we will finish emptying the scratch into the output
		 * buffer preferentially.
		 *
		 * Then we don't otherwise have to handle fragmentations in
		 * order to exactly fill the output buffer, simplifying
		 * everything else.
		 */

		if (lws_lec_scratch(ctx))
			break;

		if (ctx->fmt_pos >= fl) {
			if (ctx->state == CBPS_IDLE)
				break;
			c = 0;
		} else
			c = fmt[ctx->fmt_pos];

		// lwsl_notice("%s: %d %d %c\n", __func__, ctx->state, ctx->sp, c);

		switch (ctx->state) {
		case CBPS_IDLE:
			ctx->scratch_len = 0;
			switch (c) {
			case '[':
				n = format_scan(&fmt[ctx->fmt_pos]);
				if (n == -2)
					return LWS_LECPCTX_RET_FAIL;
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_ARRAY, n == -1,
							(uint64_t)n);
				goto stack_push;
			case '{':
				n = format_scan(&fmt[ctx->fmt_pos]);
				if (n == -2)
					return LWS_LECPCTX_RET_FAIL;
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_MAP, n == -1,
							(uint64_t)n);
				goto stack_push;
			case '(':
				/* must be preceded by a number */
				goto fail;

			case '<': /* <t or <b */
				ctx->state = CBPS_CONTYPE;
				break;

			case ']':
				if (!ctx->sp || ctx->stack[ctx->sp - 1] != '[')
					return LWS_LECPCTX_RET_FAIL;
				ctx->sp--;
				break;
			case '}':
				if (!ctx->sp || ctx->stack[ctx->sp - 1] != '{')
					return LWS_LECPCTX_RET_FAIL;
				ctx->sp--;
				break;
			case ')':
				if (!ctx->sp || ctx->stack[ctx->sp - 1] != '(') {
					lwsl_notice("bad tag end %d %c\n",
						ctx->sp, ctx->stack[ctx->sp - 1]);
					goto fail;
				}
				ctx->sp--;
				break;
			case '>':
				if (!ctx->sp || ctx->stack[ctx->sp - 1] != '<')
					return LWS_LECPCTX_RET_FAIL;
				ctx->scratch[ctx->scratch_len++] =
						(uint8_t)(LWS_CBOR_MAJTYP_FLOAT |
							LWS_CBOR_M7_BREAK);
				ctx->sp--;
				break;
			case '\'':
				n = format_scan(&fmt[ctx->fmt_pos]);
				// lwsl_notice("%s: quote fs %d\n", __func__, n);
				if (n < 0)
					return LWS_LECPCTX_RET_FAIL;
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_TSTR, 0,
								(uint64_t)n);
				ctx->state = CBPS_STRING_LIT;
				break;
			case '%':
				if (ctx->vaa_pos >= sizeof(ctx->vaa) - 1) {
					lwsl_err("%s: too many %%\n", __func__);
					goto fail;
				}
				ctx->_long = 0;
				ctx->dotstar = 0;
				ctx->state = CBPS_PC1;
				break;
			case ':':
				break;
			case ',':
				break;
			case '-':
				ctx->item.opcode = LWS_CBOR_MAJTYP_INT_NEG;
				ctx->item.u.i64 = 0;
				ctx->state = CBPS_NUM_LIT;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				ctx->item.opcode = LWS_CBOR_MAJTYP_UINT;
				ctx->item.u.u64 = (uint64_t)(c - '0');
				ctx->state = CBPS_NUM_LIT;
				break;
			}
			break;
		case CBPS_PC1:
			if (c == 'l') {
				ctx->_long++;
				ctx->state = CBPS_PC2;
				break;
			}
			if (c == '.') {
				ctx->dotstar++;
				ctx->state = CBPS_PC2;
				break;
			}
			/* fallthru */

		case CBPS_PC2:
			if (c == 'l') {
				ctx->_long++;
				ctx->state = CBPS_PC3;
				break;
			}
			if (c == '*') {
				ctx->dotstar++;
				ctx->state = CBPS_PC3;
				break;
			}
			/* fallthru */

		case CBPS_PC3:
			switch (c) {
			case 'd':
				switch (ctx->_long) {
				case 0:
					i64 = (int64_t)va_arg(args, int);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_INT;
					break;
				case 1:
					i64 = (int64_t)va_arg(args, long);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_LONG;
					break;
				case 2:
					i64 = (int64_t)va_arg(args, long long);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_LONG_LONG;
					break;
				}
				if (i64 < 0)
					lws_lec_int(ctx,
						    LWS_CBOR_MAJTYP_INT_NEG, 0,
						    (uint64_t)(-1ll - i64));
				else
					lws_lec_int(ctx,
						    LWS_CBOR_MAJTYP_UINT, 0,
						    (uint64_t)i64);
				break;
			case 'u':
				switch (ctx->_long) {
				case 0:
					u64 = (uint64_t)va_arg(args, unsigned int);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_INT;
					break;
				case 1:
					u64 = (uint64_t)va_arg(args, unsigned long);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_LONG;
					break;
				case 2:
					u64 = (uint64_t)va_arg(args, unsigned long long);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_LONG_LONG;
					break;
				}
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_UINT, 0, u64);
				break;
			case 's': /* text string */
				ctx->ongoing_done = 0;
				if (ctx->dotstar == 2) {
					ctx->ongoing_len = (uint64_t)va_arg(args, int);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_INT;
				}
				/* vaa for ptr done at end of body copy */
				ctx->ongoing_src = va_arg(args, uint8_t *);
				if (ctx->dotstar != 2)
					ctx->ongoing_len = (uint64_t)strlen(
						(const char *)ctx->ongoing_src);
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_TSTR, 0, ctx->ongoing_len);
				ctx->state = CBPS_STRING_BODY;
				ctx->fmt_pos++;
				continue;
			case 'b': /* binary string (%.*b only) */
				if (ctx->dotstar != 2)
					goto fail;
				ctx->vaa[ctx->vaa_pos++] = NATTYPE_INT;
				ctx->ongoing_done = 0;
				ctx->ongoing_len = (uint64_t)va_arg(args, int);
				/* vaa for ptr done at end of body copy */
				ctx->ongoing_src = va_arg(args, uint8_t *);
				lws_lec_int(ctx, LWS_CBOR_MAJTYP_BSTR, 0, ctx->ongoing_len);
				ctx->state = CBPS_STRING_BODY;
				ctx->fmt_pos++;
				continue;
			case 't': /* dynamic tag */
				switch (ctx->_long) {
				case 0:
					ctx->item.u.u64 = (uint64_t)va_arg(args, int);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_INT;
					break;
				case 1:
					ctx->item.u.u64 = (uint64_t)va_arg(args, long);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_LONG;
					break;
				case 2:
					ctx->item.u.u64 = (uint64_t)va_arg(args, long long);
					ctx->vaa[ctx->vaa_pos++] = NATTYPE_LONG_LONG;
					break;
				}
				ctx->item.opcode = LWS_CBOR_MAJTYP_UINT;
				ctx->fmt_pos++;
				if (ctx->fmt_pos >= fl)
					continue;
				c = fmt[ctx->fmt_pos];
				if (c != '(')
					goto fail;
				goto tag_body;
#if defined(LWS_WITH_CBOR_FLOAT)
			case 'f': /* floating point double */
				dbl = va_arg(args, double);

				if (dbl == (float)dbl) {
					uint16_t hf;
					union {
						uint32_t ui;
						float f;
					} u1, u2;

					u1.f = (float)dbl;
					lws_singles2halfp(&hf, u1.ui);
					lws_halfp2singles(&u2.ui, hf);

					if ((isinf(u1.f) && isinf(u2.f)) ||
					    (isnan(u1.f) && isnan(u2.f)) ||
					    u1.f == u2.f) {
						lws_lec_int(ctx,
							    LWS_CBOR_MAJTYP_FLOAT |
							    LWS_CBOR_M7_SUBTYP_FLOAT16,
							    0, hf);
						break;
					}
					/* do it as 32-bit float */
					lws_lec_int(ctx,
						    LWS_CBOR_MAJTYP_FLOAT |
						    LWS_CBOR_M7_SUBTYP_FLOAT32,
						    0, u1.ui);
					break;
				}

				/* do it as 64-bit double */

				{
					union {
						uint64_t ui;
						double f;
					} u3;

					u3.f = dbl;
					lws_lec_int(ctx,
						    LWS_CBOR_MAJTYP_FLOAT |
						    LWS_CBOR_M7_SUBTYP_FLOAT64,
						    0, u3.ui);
				}
				break;
#else
			case 'f':
				lwsl_err("%s: no FP support\n", __func__);
				goto fail;
#endif
			}
			ctx->state = CBPS_IDLE;
			break;

		case CBPS_STRING_BODY:
			s = lws_ptr_diff_size_t(ctx->end, ctx->buf);
			if (s > (size_t)(ctx->ongoing_len - ctx->ongoing_done))
				s = (size_t)(ctx->ongoing_len - ctx->ongoing_done);
			memcpy(ctx->buf, ctx->ongoing_src + ctx->ongoing_done, s);
			ctx->buf += s;
			ctx->ongoing_done += s;
			if (ctx->ongoing_len == ctx->ongoing_done) {
				/* vaa for ptr */
				ctx->vaa[ctx->vaa_pos++] = NATTYPE_PTR;
				ctx->state = CBPS_IDLE;
			}
			continue;

		case CBPS_NUM_LIT:
			if (c >= '0' && c <= '9') {
				ctx->item.u.u64 = (ctx->item.u.u64 * 10) +
								(uint64_t)(c - '0');
				break;
			}

			if (ctx->item.opcode == LWS_CBOR_MAJTYP_INT_NEG)
				ctx->item.u.i64--;

			if (c == '(') { /* tag qualifier */
tag_body:
				n = format_scan(&fmt[ctx->fmt_pos]);
				if (n == -2)
					goto fail;
				/*
				 * inteterminite length not possible for tag,
				 * take it to mean that the closure is in a
				 * later format string
				 */

				lws_lec_int(ctx, LWS_CBOR_MAJTYP_TAG, 0,
							ctx->item.u.u64);

stack_push:
				if (ctx->sp >= sizeof(ctx->stack))
					return LWS_LECPCTX_RET_FAIL;
				ctx->stack[ctx->sp] = (uint8_t)c;
				ctx->indet[ctx->sp++] = (uint8_t)(n == -1);
				// lwsl_notice("%s: pushed %c\n", __func__, c);
				ctx->state = CBPS_IDLE;
				break;
			}

			lws_lec_int(ctx, ctx->item.opcode, 0, ctx->item.u.u64);

			ctx->state = CBPS_IDLE;
			/* deal with the terminating char fresh */
			continue;

		case CBPS_STRING_LIT:
			if (!ctx->escflag && c == '\\') {
				ctx->escflag = 1;
				break;
			}
			if (!ctx->escflag && c == '\'') {
				ctx->state = CBPS_IDLE;
				break;
			}

			*ctx->buf++ = (uint8_t)c;
			ctx->escflag = 0;

			break;

		case CBPS_CONTYPE:
			if (c != 't' && c != 'b')
				return LWS_LECPCTX_RET_FAIL;

			lws_lec_int(ctx, c == 't' ? LWS_CBOR_MAJTYP_TSTR :
						    LWS_CBOR_MAJTYP_BSTR, 1, 0);
			c = '<';
			n = 0;
			goto stack_push;
		}

		ctx->fmt_pos++;
	}

	ctx->used = lws_ptr_diff_size_t(ctx->buf, ctx->start);
	// lwsl_notice("%s: ctx->used %d\n", __func__, (int)ctx->used);

	if (ctx->buf == ctx->end || ctx->scratch_len)
		return LWS_LECPCTX_RET_AGAIN;

	ctx->fmt_pos = 0;
	ctx->vaa_pos = 0;

	return LWS_LECPCTX_RET_FINISHED;

fail:
	lwsl_notice("%s: failed\n", __func__);

	ctx->fmt_pos = 0;

	return LWS_LECPCTX_RET_FAIL;
}
