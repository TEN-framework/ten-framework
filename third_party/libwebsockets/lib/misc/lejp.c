/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
#include <string.h>
#include <stdio.h>

static const char * const parser_errs[] = {
	"",
	"",
	"No opening '{'",
	"Expected closing '}'",
	"Expected '\"'",
	"String underrun",
	"Illegal unescaped control char",
	"Illegal escape format",
	"Illegal hex number",
	"Expected ':'",
	"Illegal value start",
	"Digit required after decimal point",
	"Bad number format",
	"Bad exponent format",
	"Unknown token",
	"Too many ']'",
	"Mismatched ']'",
	"Expected ']'",
	"JSON nesting limit exceeded",
	"Nesting tracking used up",
	"Number too long",
	"Comma or block end expected",
	"Unknown",
	"Parser callback errored (see earlier error)",
};

/**
 * lejp_construct - prepare a struct lejp_ctx for use
 *
 * \param ctx:	pointer to your struct lejp_ctx
 * \param callback:	your user callback which will received parsed tokens
 * \param user:	optional user data pointer untouched by lejp
 * \param paths:	your array of name elements you are interested in
 * \param count_paths:	LWS_ARRAY_SIZE() of @paths
 *
 * Prepares your context struct for use with lejp
 */

void
lejp_construct(struct lejp_ctx *ctx,
	signed char (*callback)(struct lejp_ctx *ctx, char reason), void *user,
			const char * const *paths, unsigned char count_paths)
{
	ctx->st[0].s = 0;
	ctx->st[0].p = 0;
	ctx->st[0].i = 0;
	ctx->st[0].b = 0;
	ctx->sp = 0;
	ctx->ipos = 0;
	ctx->outer_array = 0;
	ctx->path_match = 0;
	ctx->path_stride = 0;
	ctx->path[0] = '\0';
	ctx->user = user;
	ctx->line = 1;

	ctx->pst_sp = 0;
	ctx->pst[0].callback = callback;
	ctx->pst[0].paths = paths;
	ctx->pst[0].count_paths = count_paths;
	ctx->pst[0].user = NULL;
	ctx->pst[0].ppos = 0;

	ctx->pst[0].callback(ctx, LEJPCB_CONSTRUCTED);
}

/**
 * lejp_destruct - retire a previously constructed struct lejp_ctx
 *
 * \param ctx:	pointer to your struct lejp_ctx
 *
 * lejp does not perform any allocations, but since your user code might, this
 * provides a one-time LEJPCB_DESTRUCTED callback at destruction time where
 * you can clean up in your callback.
 */

void
lejp_destruct(struct lejp_ctx *ctx)
{
	/* no allocations... just let callback know what it happening */
	if (ctx && ctx->pst[0].callback)
		ctx->pst[0].callback(ctx, LEJPCB_DESTRUCTED);
}

/**
 * lejp_change_callback - switch to a different callback from now on
 *
 * \param ctx:	pointer to your struct lejp_ctx
 * \param callback:	your user callback which will received parsed tokens
 *
 * This tells the old callback it was destroyed, in case you want to take any
 * action because that callback "lost focus", then changes to the new
 * callback and tells it first that it was constructed, and then started.
 *
 * Changing callback is a cheap and powerful trick to split out handlers
 * according to information earlier in the parse.  For example you may have
 * a JSON pair "schema" whose value defines what can be expected for the rest
 * of the JSON.  Rather than having one huge callback for all cases, you can
 * have an initial one looking for "schema" which then calls
 * lejp_change_callback() to a handler specific for the schema.
 *
 * Notice that afterwards, you need to construct the context again anyway to
 * parse another JSON object, and the callback is reset then to the main,
 * schema-interpreting one.  The construction action is very lightweight.
 */

void
lejp_change_callback(struct lejp_ctx *ctx,
		     signed char (*callback)(struct lejp_ctx *ctx, char reason))
{
	ctx->pst[0].callback(ctx, LEJPCB_DESTRUCTED);
	ctx->pst[0].callback = callback;
	ctx->pst[0].callback(ctx, LEJPCB_CONSTRUCTED);
	ctx->pst[0].callback(ctx, LEJPCB_START);
}

void
lejp_check_path_match(struct lejp_ctx *ctx)
{
	const char *p, *q;
	int n;
	size_t s = sizeof(char *);

	if (ctx->path_stride)
		s = ctx->path_stride;

	/* we only need to check if a match is not active */
	for (n = 0; !ctx->path_match &&
	     n < ctx->pst[ctx->pst_sp].count_paths; n++) {
		ctx->wildcount = 0;
		p = ctx->path;

		q = *((char **)(((char *)ctx->pst[ctx->pst_sp].paths) + ((unsigned int)n * s)));
//lwsl_notice("%s: %s %s\n", __func__, p, q);
		while (*p && *q) {
			if (*q != '*') {
				if (*p != *q)
					break;
				p++;
				q++;
				continue;
			}
			ctx->wild[ctx->wildcount++] = (uint16_t)lws_ptr_diff_size_t(p, ctx->path);
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
lejp_get_wildcard(struct lejp_ctx *ctx, int wildcard, char *dest, int len)
{
	int n;

	if (wildcard >= ctx->wildcount || !len)
		return 0;

	n = ctx->wild[wildcard];

	while (--len && n < ctx->pst[ctx->pst_sp].ppos &&
	       (n == ctx->wild[wildcard] || ctx->path[n] != '.'))
		*dest++ = ctx->path[n++];

	*dest = '\0';
	n++;

	return n - ctx->wild[wildcard];
}

/**
 * lejp_parse - interpret some more incoming data incrementally
 *
 * \param ctx:	previously constructed parsing context
 * \param json:	char buffer with the new data to interpret
 * \param len:	amount of data in the buffer
 *
 * Because lejp is a stream parser, it incrementally parses as new data
 * becomes available, maintaining all state in the context struct.  So an
 * incomplete JSON is a normal situation, getting you a LEJP_CONTINUE
 * return, signalling there's no error but to call again with more data when
 * it comes to complete the parsing.  Successful parsing completes with a
 * 0 or positive integer indicating how much of the last input buffer was
 * unused.
 */

static const char esc_char[] = "\"\\/bfnrt";
static const char esc_tran[] = "\"\\/\b\f\n\r\t";
static const char tokens[] = "rue alse ull ";

int
lejp_parse(struct lejp_ctx *ctx, const unsigned char *json, int len)
{
	unsigned char c, n, s;
	int ret = LEJP_REJECT_UNKNOWN;

	if (!ctx->sp && !ctx->pst[ctx->pst_sp].ppos)
		ctx->pst[ctx->pst_sp].callback(ctx, LEJPCB_START);

	while (len--) {
		c = *json++;
		s = (unsigned char)ctx->st[ctx->sp].s;

		/* skip whitespace unless we should care */
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#') {
			if (c == '\n') {
				ctx->line++;
				ctx->st[ctx->sp].s &= (char)~LEJP_FLAG_WS_COMMENTLINE;
			}
			if (!(s & LEJP_FLAG_WS_KEEP)) {
				if (c == '#')
					ctx->st[ctx->sp].s |=
						LEJP_FLAG_WS_COMMENTLINE;
				continue;
			}
		}

		if (ctx->st[ctx->sp].s & LEJP_FLAG_WS_COMMENTLINE)
			continue;

		switch (s) {
		case LEJP_IDLE:
			if (!ctx->sp && c == '[') {
				/* push */
				ctx->outer_array = 1;
				ctx->st[ctx->sp].s = LEJP_MP_ARRAY_END;
				c = LEJP_MP_VALUE;
				ctx->path[ctx->pst[ctx->pst_sp].ppos++] = '[';
				ctx->path[ctx->pst[ctx->pst_sp].ppos++] = ']';
				ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
				if (ctx->pst[ctx->pst_sp].callback(ctx, LEJPCB_ARRAY_START))
					goto reject_callback;
				ctx->i[ctx->ipos++] = 0;
				if (ctx->ipos > LWS_ARRAY_SIZE(ctx->i)) {
					ret = LEJP_REJECT_MP_DELIM_ISTACK;
					goto reject;
				}
				goto add_stack_level;
			}
			if (c != '{') {
				ret = LEJP_REJECT_IDLE_NO_BRACE;
				goto reject;
			}
			if (ctx->pst[ctx->pst_sp].callback(ctx,
							   LEJPCB_OBJECT_START))
				goto reject_callback;
			ctx->st[ctx->sp].s = LEJP_MEMBERS;
			break;
		case LEJP_MEMBERS:
			if (c == '}') {
				if (ctx->sp >= 1)
					goto pop_level;

				ctx->st[ctx->sp].s = LEJP_IDLE;
				ret = LEJP_REJECT_MEMBERS_NO_CLOSE;
				goto reject;
			}
			ctx->st[ctx->sp].s = LEJP_M_P;
			goto redo_character;
		case LEJP_M_P:
			if (c != '\"') {
				ret = LEJP_REJECT_MP_NO_OPEN_QUOTE;
				goto reject;
			}
			/* push */
			ctx->st[ctx->sp].s = LEJP_MP_DELIM;
			c = LEJP_MP_STRING;
			goto add_stack_level;

		case LEJP_MP_STRING:
			if (c == '\"') {
				if (!ctx->sp) { /* JSON can't end on quote */
					ret = LEJP_REJECT_MP_STRING_UNDERRUN;
					goto reject;
				}
				if (ctx->st[ctx->sp - 1].s != LEJP_MP_DELIM) {
					ctx->buf[ctx->npos] = '\0';
					if (ctx->pst[ctx->pst_sp].callback(ctx,
						      LEJPCB_VAL_STR_END) < 0)
						goto reject_callback;
				}
				/* pop */
				ctx->sp--;
				break;
			}
			if (c == '\\') {
				ctx->st[ctx->sp].s = LEJP_MP_STRING_ESC;
				break;
			}
			if (c < ' ') {/* "control characters" not allowed */
				ret = LEJP_REJECT_MP_ILLEGAL_CTRL;
				goto reject;
			}
			goto emit_string_char;

		case LEJP_MP_STRING_ESC:
			if (c == 'u') {
				ctx->st[ctx->sp].s = LEJP_MP_STRING_ESC_U1;
				ctx->uni = 0;
				break;
			}
			for (n = 0; n < sizeof(esc_char); n++) {
				if (c != esc_char[n])
					continue;
				/* found it */
				c = (unsigned char)esc_tran[n];
				ctx->st[ctx->sp].s = LEJP_MP_STRING;
				goto emit_string_char;
			}
			ret = LEJP_REJECT_MP_STRING_ESC_ILLEGAL_ESC;
			/* illegal escape char */
			goto reject;

		case LEJP_MP_STRING_ESC_U1:
		case LEJP_MP_STRING_ESC_U2:
		case LEJP_MP_STRING_ESC_U3:
		case LEJP_MP_STRING_ESC_U4:
			ctx->uni = (uint16_t)(ctx->uni << 4);
			if (c >= '0' && c <= '9')
				ctx->uni |= (uint16_t)(c - '0');
			else
				if (c >= 'a' && c <= 'f')
					ctx->uni |= (uint16_t)(c - 'a' + 10);
				else
					if (c >= 'A' && c <= 'F')
						ctx->uni |= (uint16_t)(c - 'A' + 10);
					else {
						ret = LEJP_REJECT_ILLEGAL_HEX;
						goto reject;
					}
			ctx->st[ctx->sp].s++;
			switch (s) {
			case LEJP_MP_STRING_ESC_U2:
				if (ctx->uni < 0x08)
					break;
				/*
				 * 0x08-0xff (0x0800 - 0xffff)
				 * emit 3-byte UTF-8
				 */
				c = (unsigned char)(0xe0 | ((ctx->uni >> 4) & 0xf));
				goto emit_string_char;

			case LEJP_MP_STRING_ESC_U3:
				if (ctx->uni >= 0x080) {
					/*
					 * 0x080 - 0xfff (0x0800 - 0xffff)
					 * middle 3-byte seq
					 * send ....XXXXXX..
					 */
					c = (unsigned char)(0x80 | ((ctx->uni >> 2) & 0x3f));
					goto emit_string_char;
				}
				if (ctx->uni < 0x008)
					break;
				/*
				 * 0x008 - 0x7f (0x0080 - 0x07ff)
				 * start 2-byte seq
				 */
				c = (unsigned char)(0xc0 | (ctx->uni >> 2));
				goto emit_string_char;

			case LEJP_MP_STRING_ESC_U4:
				if (ctx->uni >= 0x0080)
					/* end of 2 or 3-byte seq */
					c = (unsigned char)(0x80 | (ctx->uni & 0x3f));
				else
					/* literal */
					c = (unsigned char)ctx->uni;

				ctx->st[ctx->sp].s = LEJP_MP_STRING;
				goto emit_string_char;
			default:
				break;
			}
			break;

		case LEJP_MP_DELIM:
			if (c != ':') {
				ret = LEJP_REJECT_MP_DELIM_MISSING_COLON;
				goto reject;
			}
			ctx->st[ctx->sp].s = LEJP_MP_VALUE;
			ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';

			lejp_check_path_match(ctx);
			if (ctx->pst[ctx->pst_sp].callback(ctx, LEJPCB_PAIR_NAME))
				goto reject_callback;
			break;

		case LEJP_MP_VALUE:
			if (c == '-' || (c >= '0' && c <= '9')) {
				ctx->npos = 0;
				ctx->dcount = 0;
				ctx->f = 0;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE_NUM_INT;
				goto redo_character;
			}
			switch (c) {
			case'\"':
				/* push */
				ctx->st[ctx->sp].s = LEJP_MP_COMMA_OR_END;
				c = LEJP_MP_STRING;
				ctx->npos = 0;
				ctx->buf[0] = '\0';
				if (ctx->pst[ctx->pst_sp].callback(ctx,
							LEJPCB_VAL_STR_START))
					goto reject_callback;
				goto add_stack_level;

			case '{':
				/* push */
				ctx->st[ctx->sp].s = LEJP_MP_COMMA_OR_END;
				c = LEJP_MEMBERS;
				lejp_check_path_match(ctx);
				if (ctx->pst[ctx->pst_sp].callback(ctx,
							LEJPCB_OBJECT_START))
					goto reject_callback;
				ctx->path_match = 0;
				goto add_stack_level;

			case '[':
				/* push */
				ctx->st[ctx->sp].s = LEJP_MP_ARRAY_END;
				c = LEJP_MP_VALUE;
				if (ctx->pst[ctx->pst_sp].ppos + 3u >=
							sizeof(ctx->path))
					goto reject;
				ctx->path[ctx->pst[ctx->pst_sp].ppos++] = '[';
				ctx->path[ctx->pst[ctx->pst_sp].ppos++] = ']';
				ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
				if (ctx->pst[ctx->pst_sp].callback(ctx, LEJPCB_ARRAY_START))
					goto reject_callback;
				ctx->i[ctx->ipos++] = 0;
				if (ctx->ipos > LWS_ARRAY_SIZE(ctx->i)) {
					ret = LEJP_REJECT_MP_DELIM_ISTACK;
					goto reject;
				}
				goto add_stack_level;

			case ']':
				/* pop */
				if (!ctx->sp) { /* JSON can't end on ] */
					ret = LEJP_REJECT_MP_C_OR_E_UNDERF;
					goto reject;
				}
				ctx->sp--;
				if (ctx->st[ctx->sp].s != LEJP_MP_ARRAY_END) {
					ret = LEJP_REJECT_MP_C_OR_E_NOTARRAY;
					goto reject;
				}
				/* drop the path [n] bit */
				if (ctx->sp) {
					ctx->pst[ctx->pst_sp].ppos = (unsigned char)
							ctx->st[ctx->sp - 1].p;
					ctx->ipos = (unsigned char)ctx->st[ctx->sp - 1].i;
				}
				ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
				if (ctx->path_match &&
				    ctx->pst[ctx->pst_sp].ppos <= ctx->path_match_len)
					/*
					 * we shrank the path to be
					 * smaller than the matching point
					 */
					ctx->path_match = 0;
				if (ctx->outer_array && !ctx->sp) { /* ended on ] */
					n = LEJPCB_ARRAY_END;
					goto completed;
				}
				goto array_end;

			case 't': /* true */
				ctx->uni = 0;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE_TOK;
				break;

			case 'f':
				ctx->uni = 4;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE_TOK;
				break;

			case 'n':
				ctx->uni = 4 + 5;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE_TOK;
				break;
			default:
				ret = LEJP_REJECT_MP_DELIM_BAD_VALUE_START;
				goto reject;
			}
			break;

		case LEJP_MP_VALUE_NUM_INT:
			if (!ctx->npos && c == '-') {
				ctx->f |= LEJP_SEEN_MINUS;
				goto append_npos;
			}

			if (ctx->dcount < 20 && c >= '0' && c <= '9') {
				if (ctx->f & LEJP_SEEN_POINT)
					ctx->f |= LEJP_SEEN_POST_POINT;
				ctx->dcount++;
				goto append_npos;
			}
			if (c == '.') {
				if (!ctx->dcount || (ctx->f & LEJP_SEEN_POINT)) {
					ret = LEJP_REJECT_MP_VAL_NUM_FORMAT;
					goto reject;
				}
				ctx->f |= LEJP_SEEN_POINT;
				goto append_npos;
			}
			/*
			 * before exponent, if we had . we must have had at
			 * least one more digit
			 */
			if ((ctx->f &
				(LEJP_SEEN_POINT | LEJP_SEEN_POST_POINT)) ==
							      LEJP_SEEN_POINT) {
				ret = LEJP_REJECT_MP_VAL_NUM_INT_NO_FRAC;
				goto reject;
			}
			if (c == 'e' || c == 'E') {
				if (ctx->f & LEJP_SEEN_EXP) {
					ret = LEJP_REJECT_MP_VAL_NUM_FORMAT;
					goto reject;
				}
				ctx->f |= LEJP_SEEN_EXP;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE_NUM_EXP;
				goto append_npos;
			}
			/* if none of the above, did we even have a number? */
			if (!ctx->dcount) {
				ret = LEJP_REJECT_MP_VAL_NUM_FORMAT;
				goto reject;
			}

			ctx->buf[ctx->npos] = '\0';
			if (ctx->f & LEJP_SEEN_POINT) {
				if (ctx->pst[ctx->pst_sp].callback(ctx,
							LEJPCB_VAL_NUM_FLOAT))
					goto reject_callback;
			} else {
				if (ctx->pst[ctx->pst_sp].callback(ctx,
							LEJPCB_VAL_NUM_INT))
					goto reject_callback;
			}

			/* then this is the post-number character, loop */
			ctx->st[ctx->sp].s = LEJP_MP_COMMA_OR_END;
			goto redo_character;

		case LEJP_MP_VALUE_NUM_EXP:
			ctx->st[ctx->sp].s = LEJP_MP_VALUE_NUM_INT;
			if (c >= '0' && c <= '9')
				goto redo_character;
			if (c == '+' || c == '-')
				goto append_npos;
			ret = LEJP_REJECT_MP_VAL_NUM_EXP_BAD_EXP;
			goto reject;

		case LEJP_MP_VALUE_TOK: /* true, false, null */
			if (c != tokens[ctx->uni]) {
				ret = LEJP_REJECT_MP_VAL_TOK_UNKNOWN;
				goto reject;
			}
			ctx->uni++;
			if (tokens[ctx->uni] != ' ')
				break;
			switch (ctx->uni) {
			case 3:
				ctx->buf[0] = '1';
				ctx->buf[1] = '\0';
				if (ctx->pst[ctx->pst_sp].callback(ctx,
								LEJPCB_VAL_TRUE))
					goto reject_callback;
				break;
			case 8:
				ctx->buf[0] = '0';
				ctx->buf[1] = '\0';
				if (ctx->pst[ctx->pst_sp].callback(ctx,
								LEJPCB_VAL_FALSE))
					goto reject_callback;
				break;
			case 12:
				ctx->buf[0] = '\0';
				if (ctx->pst[ctx->pst_sp].callback(ctx,
								LEJPCB_VAL_NULL))
					goto reject_callback;
				break;
			}
			ctx->st[ctx->sp].s = LEJP_MP_COMMA_OR_END;
			break;

		case LEJP_MP_COMMA_OR_END:
			ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
			if (c == ',') {
				/* increment this stack level's index */
				ctx->st[ctx->sp].s = LEJP_M_P;
				if (!ctx->sp) {
					ctx->pst[ctx->pst_sp].ppos = 0;
					/*
					 * since we came back to root level,
					 * no path can still match
					 */
					ctx->path_match = 0;
					break;
				}
				ctx->pst[ctx->pst_sp].ppos = (unsigned char)ctx->st[ctx->sp - 1].p;
				ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
				if (ctx->path_match &&
				    ctx->pst[ctx->pst_sp].ppos <= ctx->path_match_len)
					/*
					 * we shrank the path to be
					 * smaller than the matching point
					 */
					ctx->path_match = 0;

				if (ctx->st[ctx->sp - 1].s != LEJP_MP_ARRAY_END)
					break;
				/* top level is definitely an array... */
				if (ctx->ipos)
					ctx->i[ctx->ipos - 1]++;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE;
				break;
			}
			if (c == ']') {
				if (!ctx->sp) {
					ret = LEJP_REJECT_MP_C_OR_E_UNDERF;
					goto reject;
				}
				/* pop */
				ctx->sp--;
				if (ctx->st[ctx->sp].s != LEJP_MP_ARRAY_END) {
					ret = LEJP_REJECT_MP_C_OR_E_NOTARRAY;
					goto reject;
				}

				/* drop the path [n] bit */
				if (ctx->sp) {
					ctx->pst[ctx->pst_sp].ppos = (unsigned char)
							ctx->st[ctx->sp - 1].p;
					ctx->ipos = (unsigned char)ctx->st[ctx->sp - 1].i;
				}
				ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
				if (ctx->path_match &&
				    ctx->pst[ctx->pst_sp].ppos <= ctx->path_match_len)
					/*
					 * we shrank the path to be
					 * smaller than the matching point
					 */
					ctx->path_match = 0;

				if (ctx->outer_array && !ctx->sp) { /* ended on ] */
					n = LEJPCB_ARRAY_END;
					goto completed;
				}

				/* do LEJP_MP_ARRAY_END processing */
				goto redo_character;
			}
			if (c != '}') {
				ret = LEJP_REJECT_MP_C_OR_E_NEITHER;
				goto reject;
			}
			if (!ctx->sp) {
				n = LEJPCB_OBJECT_END;
completed:
				lejp_check_path_match(ctx);
				if (ctx->pst[ctx->pst_sp].callback(ctx, (char)n) ||
				    ctx->pst[ctx->pst_sp].callback(ctx,
							    LEJPCB_COMPLETE))
					goto reject_callback;

				/* done, return unused amount */
				return len;
			}

			/* pop */
pop_level:
			ctx->sp--;
			if (ctx->sp) {
				ctx->pst[ctx->pst_sp].ppos = (unsigned char)ctx->st[ctx->sp].p;
				ctx->ipos = (unsigned char)ctx->st[ctx->sp].i;
			}
			ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
			if (ctx->path_match &&
			    ctx->pst[ctx->pst_sp].ppos <= ctx->path_match_len)
				/*
				 * we shrank the path to be
				 * smaller than the matching point
				 */
				ctx->path_match = 0;

			lejp_check_path_match(ctx);
			if (ctx->pst[ctx->pst_sp].callback(ctx,
							   LEJPCB_OBJECT_END))
				goto reject_callback;
			break;

		case LEJP_MP_ARRAY_END:
array_end:
			ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
			if (c == ',') {
				/* increment this stack level's index */
				if (ctx->ipos)
					ctx->i[ctx->ipos - 1]++;
				ctx->st[ctx->sp].s = LEJP_MP_VALUE;
				if (ctx->sp)
					ctx->pst[ctx->pst_sp].ppos = (unsigned char)
							ctx->st[ctx->sp - 1].p;
				ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
				break;
			}
			if (c != ']') {
				ret = LEJP_REJECT_MP_ARRAY_END_MISSING;
				goto reject;
			}

			ctx->st[ctx->sp].s = LEJP_MP_COMMA_OR_END;
			ctx->pst[ctx->pst_sp].callback(ctx, LEJPCB_ARRAY_END);
			break;
		}

		continue;

emit_string_char:
		if (!ctx->sp || ctx->st[ctx->sp - 1].s != LEJP_MP_DELIM) {
			/* assemble the string value into chunks */
			ctx->buf[ctx->npos++] = (char)c;
			if (ctx->npos == sizeof(ctx->buf) - 1) {
				if (ctx->pst[ctx->pst_sp].callback(ctx,
							  LEJPCB_VAL_STR_CHUNK))
					goto reject_callback;
				ctx->npos = 0;
			}
			continue;
		}
		/* name part of name:value pair */
		ctx->path[ctx->pst[ctx->pst_sp].ppos++] = (char)c;
		continue;

add_stack_level:
		/* push on to the object stack */
		if (ctx->pst[ctx->pst_sp].ppos &&
		    ctx->st[ctx->sp].s != LEJP_MP_COMMA_OR_END &&
		    ctx->st[ctx->sp].s != LEJP_MP_ARRAY_END)
			ctx->path[ctx->pst[ctx->pst_sp].ppos++] = '.';

		ctx->st[ctx->sp].p = (char)ctx->pst[ctx->pst_sp].ppos;
		ctx->st[ctx->sp].i = (char)ctx->ipos;
		if (++ctx->sp == LWS_ARRAY_SIZE(ctx->st)) {
			ret = LEJP_REJECT_STACK_OVERFLOW;
			goto reject;
		}
		ctx->path[ctx->pst[ctx->pst_sp].ppos] = '\0';
		ctx->st[ctx->sp].s = (char)c;
		ctx->st[ctx->sp].b = 0;
		continue;

append_npos:
		if (ctx->npos >= sizeof(ctx->buf)) {
			ret = LEJP_REJECT_NUM_TOO_LONG;
			goto reject;
		}
		ctx->buf[ctx->npos++] = (char)c;
		continue;

redo_character:
		json--;
		len++;
	}

	return LEJP_CONTINUE;


reject_callback:
	ret = LEJP_REJECT_CALLBACK;

reject:
	ctx->pst[ctx->pst_sp].callback(ctx, LEJPCB_FAILED);
	return ret;
}

int
lejp_parser_push(struct lejp_ctx *ctx, void *user, const char * const *paths,
		 unsigned char paths_count, lejp_callback lejp_cb)
{
	struct _lejp_parsing_stack *p;

	if (ctx->pst_sp + 1 == LEJP_MAX_PARSING_STACK_DEPTH)
		return -1;

	lejp_check_path_match(ctx);

	ctx->pst[ctx->pst_sp].path_match = ctx->path_match;
	ctx->pst_sp++;

	p = &ctx->pst[ctx->pst_sp];
	p->user = user;
	p->callback = lejp_cb;
	p->paths = paths;
	p->count_paths = paths_count;
	p->ppos = 0;

	ctx->path_match = 0;
	lejp_check_path_match(ctx);

	lwsl_debug("%s: pushed parser stack to %d (path %s)\n", __func__,
		   ctx->pst_sp, ctx->path);

	return 0;
}

int
lejp_parser_pop(struct lejp_ctx *ctx)
{
	if (!ctx->pst_sp)
		return -1;

	ctx->pst_sp--;
	lwsl_debug("%s: popped parser stack to %d\n", __func__, ctx->pst_sp);

	ctx->path_match = 0; /* force it to check */
	lejp_check_path_match(ctx);

	return 0;
}

const char *
lejp_error_to_string(int e)
{
	if (e > 0)
		e = 0;
	else
		e = -e;

	if (e >= (int)LWS_ARRAY_SIZE(parser_errs))
		return "Unknown error";

	return parser_errs[e];
}

