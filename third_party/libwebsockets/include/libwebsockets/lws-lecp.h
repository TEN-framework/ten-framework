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
 */

/** \defgroup lecp CBOR parser
 * ##CBOR parsing related functions
 * \ingroup lwsapi
 *
 * LECP is an extremely lightweight CBOR stream parser included in lws.  It
 * is aligned in approach with the LEJP JSON stream parser, with some additional
 * things needed for CBOR.
 */
//@{

#ifndef LECP_MAX_PARSING_STACK_DEPTH
#define LECP_MAX_PARSING_STACK_DEPTH	5
#endif
#ifndef LECP_MAX_DEPTH
#define LECP_MAX_DEPTH			12
#endif
#ifndef LECP_MAX_INDEX_DEPTH
#define LECP_MAX_INDEX_DEPTH		8
#endif
#ifndef LECP_MAX_PATH
#define LECP_MAX_PATH			128
#endif
#ifndef LECP_STRING_CHUNK
/* must be >= 30 to assemble floats */
#define LECP_STRING_CHUNK		254
#endif

#define LECP_FLAG_CB_IS_VALUE 64

/*
 * CBOR initial byte 3 x MSB bits are these
 */

enum {
	LWS_CBOR_MAJTYP_UINT		= 0 << 5,
	LWS_CBOR_MAJTYP_INT_NEG		= 1 << 5,
	LWS_CBOR_MAJTYP_BSTR		= 2 << 5,
	LWS_CBOR_MAJTYP_TSTR		= 3 << 5,
	LWS_CBOR_MAJTYP_ARRAY		= 4 << 5,
	LWS_CBOR_MAJTYP_MAP		= 5 << 5,
	LWS_CBOR_MAJTYP_TAG		= 6 << 5,
	LWS_CBOR_MAJTYP_FLOAT		= 7 << 5,  /* also BREAK */

	LWS_CBOR_MAJTYP_MASK		= 7 << 5,

	/*
	 * For the low 5 bits of the opcode, 0-23 are literals, unless it's
	 * FLOAT.
	 *
	 * 24 = 1 byte; 25 = 2..., 26 = 4... and 27 = 8 bytes following literal.
	 */
	LWS_CBOR_1			= 24,
	LWS_CBOR_2			= 25,
	LWS_CBOR_4			= 26,
	LWS_CBOR_8			= 27,

	LWS_CBOR_RESERVED		= 28,

	LWS_CBOR_SUBMASK		= 0x1f,

	/*
	 * Major type 7 discriminators in low 5 bits
	 * 0 - 23 is SIMPLE implicit value (like, eg, LWS_CBOR_SWK_TRUE)
	 */
	LWS_CBOR_SWK_FALSE		= 20,
	LWS_CBOR_SWK_TRUE		= 21,
	LWS_CBOR_SWK_NULL		= 22,
	LWS_CBOR_SWK_UNDEFINED		= 23,

	LWS_CBOR_M7_SUBTYP_SIMPLE_X8	= 24, /* simple with additional byte */
	LWS_CBOR_M7_SUBTYP_FLOAT16	= 25,
	LWS_CBOR_M7_SUBTYP_FLOAT32	= 26,
	LWS_CBOR_M7_SUBTYP_FLOAT64	= 27,
	LWS_CBOR_M7_BREAK		= 31,

/* 28, 29, 30 are illegal.
 *
 * 31 is illegal for UINT, INT_NEG, and TAG;
 *               for BSTR, TSTR, ARRAY and MAP it means "indefinite length", ie,
 *               it's made up of an endless amount of determinite-length
 *               fragments terminated with a BREAK (FLOAT | 31) instead of the
 *               next determinite-length fragment.  The second framing level
 *               means no need for escapes for BREAK in the data.
 */

	LWS_CBOR_INDETERMINITE		= 31,

/*
 * Well-known tags
 */

	LWS_CBOR_WKTAG_DATETIME_STD	= 0, /* text */
	LWS_CBOR_WKTAG_DATETIME_EPOCH	= 1, /* int or float */
	LWS_CBOR_WKTAG_BIGNUM_UNSIGNED	= 2, /* byte string */
	LWS_CBOR_WKTAG_BIGNUM_NEGATIVE	= 3, /* byte string */
	LWS_CBOR_WKTAG_DECIMAL_FRAC	= 4, /* array */
	LWS_CBOR_WKTAG_BIGFLOAT		= 5, /* array */

	LWS_CBOR_WKTAG_COSE_ENC0	= 16,
	LWS_CBOR_WKTAG_COSE_MAC0	= 17,
	LWS_CBOR_WKTAG_COSE_SIGN1	= 18,

	LWS_CBOR_WKTAG_TO_B64U		= 21, /* any */
	LWS_CBOR_WKTAG_TO_B64		= 22, /* any */
	LWS_CBOR_WKTAG_TO_B16		= 23, /* any */
	LWS_CBOR_WKTAG_CBOR		= 24, /* byte string */

	LWS_CBOR_WKTAG_URI		= 32, /* text string */
	LWS_CBOR_WKTAG_B64U		= 33, /* text string */
	LWS_CBOR_WKTAG_B64		= 34, /* text string */
	LWS_CBOR_WKTAG_MIME		= 36, /* text string */

	LWS_CBOR_WKTAG_COSE_ENC		= 96,
	LWS_CBOR_WKTAG_COSE_MAC		= 97,
	LWS_CBOR_WKTAG_COSE_SIGN	= 98,

	LWS_CBOR_WKTAG_SELFDESCCBOR	= 55799
};

enum lecp_callbacks {
	LECPCB_CONSTRUCTED		= 0,
	LECPCB_DESTRUCTED		= 1,

	LECPCB_COMPLETE			= 3,
	LECPCB_FAILED			= 4,

	LECPCB_PAIR_NAME		= 5,

	LECPCB_VAL_TRUE			= LECP_FLAG_CB_IS_VALUE | 6,
	LECPCB_VAL_FALSE		= LECP_FLAG_CB_IS_VALUE | 7,
	LECPCB_VAL_NULL			= LECP_FLAG_CB_IS_VALUE | 8,
	LECPCB_VAL_NUM_INT		= LECP_FLAG_CB_IS_VALUE | 9,
	LECPCB_VAL_RESERVED		= LECP_FLAG_CB_IS_VALUE | 10,
	LECPCB_VAL_STR_START		= 11, /* notice handle separately */
	LECPCB_VAL_STR_CHUNK		= LECP_FLAG_CB_IS_VALUE | 12,
	LECPCB_VAL_STR_END		= LECP_FLAG_CB_IS_VALUE | 13,

	LECPCB_ARRAY_START		= 14,
	LECPCB_ARRAY_END		= 15,

	LECPCB_OBJECT_START		= 16,
	LECPCB_OBJECT_END		= 17,

	LECPCB_TAG_START		= 18,
	LECPCB_TAG_END			= 19,

	LECPCB_VAL_NUM_UINT		= LECP_FLAG_CB_IS_VALUE | 20,
	LECPCB_VAL_UNDEFINED		= LECP_FLAG_CB_IS_VALUE | 21,
	LECPCB_VAL_FLOAT16		= LECP_FLAG_CB_IS_VALUE | 22,
	LECPCB_VAL_FLOAT32		= LECP_FLAG_CB_IS_VALUE | 23,
	LECPCB_VAL_FLOAT64		= LECP_FLAG_CB_IS_VALUE | 24,

	LECPCB_VAL_SIMPLE		= LECP_FLAG_CB_IS_VALUE | 25,

	LECPCB_VAL_BLOB_START		= 26, /* notice handle separately */
	LECPCB_VAL_BLOB_CHUNK		= LECP_FLAG_CB_IS_VALUE | 27,
	LECPCB_VAL_BLOB_END		= LECP_FLAG_CB_IS_VALUE | 28,

	LECPCB_ARRAY_ITEM_START		= 29,
	LECPCB_ARRAY_ITEM_END		= 30,

	LECPCB_LITERAL_CBOR		= 31,
};

enum lecp_reasons {
	LECP_CONTINUE			= -1,
	LECP_REJECT_BAD_CODING		= -2,
	LECP_REJECT_UNKNOWN		= -3,
	LECP_REJECT_CALLBACK		= -4,
	LECP_STACK_OVERFLOW		= -5,
};


struct lecp_item {
	union {
		uint64_t	u64;
		int64_t		i64;

		uint64_t	u32;

		uint16_t	hf;
#if defined(LWS_WITH_CBOR_FLOAT)
		float		f;
		double		d;
#else
		uint32_t	f;
		uint64_t	d;
#endif
	} u;
	uint8_t			opcode;
};

struct lecp_ctx;
typedef signed char (*lecp_callback)(struct lecp_ctx *ctx, char reason);

struct _lecp_stack {
	char			s; /* lejp_state stack*/
	uint8_t			p; /* path length */
	char			i; /* index array length */
	char			indet; /* indeterminite */
	char			intermediate; /* in middle of string */

	char			pop_iss;
	uint64_t		tag;
	uint64_t		collect_rem;
	uint32_t		ordinal;
	uint8_t			opcode;
	uint8_t			send_new_array_item;
	uint8_t			barrier;
};

struct _lecp_parsing_stack {
	void			*user;	/* private to the stack level */
	lecp_callback		cb;
	const char * const	*paths;
	uint8_t			count_paths;
	uint8_t			ppos;
	uint8_t			path_match;
};

struct lecp_ctx {

	/* sorted by type for most compact alignment
	 *
	 * pointers
	 */
	void *user;
	uint8_t			*collect_tgt;

	/* arrays */

	struct _lecp_parsing_stack pst[LECP_MAX_PARSING_STACK_DEPTH];
	struct _lecp_stack	st[LECP_MAX_DEPTH];
	uint16_t		i[LECP_MAX_INDEX_DEPTH]; /* index array */
	uint16_t		wild[LECP_MAX_INDEX_DEPTH]; /* index array */
	char			path[LECP_MAX_PATH];
	uint8_t			cbor[64]; /* literal cbor capture */

	struct lecp_item	item;


	/* size_t */

	size_t			path_stride; /* 0 means default ptr size, else
					      * stride...  allows paths to be
					      * provided composed inside a
					      * larger user struct instead of a
					      * duplicated array */
	size_t			used_in;     /* bytes of input consumed */

	/* short */

	uint16_t 		uni;

	/* char */

	uint8_t			npos;
	uint8_t			dcount;
	uint8_t			f;
	uint8_t			sp; /* stack head */
	uint8_t			ipos; /* index stack depth */
	uint8_t			count_paths;
	uint8_t			path_match;
	uint8_t			path_match_len;
	uint8_t			wildcount;
	uint8_t			pst_sp; /* parsing stack head */
	uint8_t			outer_array;
	uint8_t			cbor_pos;
	uint8_t			literal_cbor_report;
	char			present; /* temp for cb reason to use */

	uint8_t			be; /* big endian */

	/* at end so we can memset the rest of it */

	char buf[LECP_STRING_CHUNK + 1];
};

enum lws_lec_pctx_ret {
	LWS_LECPCTX_RET_FINISHED		= 0,
	LWS_LECPCTX_RET_AGAIN, /* call again to continue writing buffer */
	LWS_LECPCTX_RET_FAIL /* something broken, eg, format string */
};

enum cbp_state {
	CBPS_IDLE,
	CBPS_PC1,
	CBPS_PC2,
	CBPS_PC3,

	CBPS_STRING_BODY,

	CBPS_NUM_LIT,

	CBPS_STRING_LIT,

	CBPS_CONTYPE,
};

typedef struct lws_lec_pctx {
	uint8_t			stack[16];
	uint8_t			vaa[16];
	uint8_t			indet[16];
	uint8_t			scratch[24];
	uint8_t			*start;	   /* the beginning of the out buf */
	uint8_t			*buf;	   /* cur pos in output buf */
	uint8_t			*end;	   /* the end of the output buf */

	const uint8_t		*ongoing_src;
	uint64_t		ongoing_len;
	uint64_t		ongoing_done;

	struct lecp_item	item;

	size_t			used;	   /* number of bytes valid from start */

	int			opaque[4]; /* ignored by lws, caller may use */

	enum cbp_state		state;
	unsigned int		fmt_pos;
	uint8_t			sp;
	uint8_t			scratch_len;
	uint8_t			escflag;
	uint8_t			_long;
	uint8_t			vaa_pos;
	uint8_t			dotstar;
} lws_lec_pctx_t;

LWS_VISIBLE LWS_EXTERN void
lws_lec_int(lws_lec_pctx_t *ctx, uint8_t opcode, uint8_t indet, uint64_t num);

LWS_VISIBLE LWS_EXTERN int
lws_lec_scratch(lws_lec_pctx_t *ctx);

/*
 * lws_lec_init() - prepare a cbor writing context
 *
 * \param ctx: the cbor writing context to prepare
 * \param buf: the output buffer start
 * \param len: the amount of the output buffer we can use
 *
 * Prepares a cbor writing context so that les_lec_printf can be used to
 * write into it.
 */
LWS_VISIBLE LWS_EXTERN void
lws_lec_init(lws_lec_pctx_t *ctx, uint8_t *buf, size_t len);

/*
 * lws_lec_setbuf() - update the output buffer for an initialized cbor writing ctx
 *
 * \param ctx: the cbor writing context to prepare
 * \param buf: the output buffer start
 * \param len: the amount of the output buffer we can use
 *
 * Leaves the cbor writing context state as it is, but resets the output buffer
 * it writes into as given in \p buf and \p len
 */
LWS_VISIBLE LWS_EXTERN void
lws_lec_setbuf(lws_lec_pctx_t *ctx, uint8_t *buf, size_t len);

/*
 * lws_lec_vsprintf() - write into a cbor writing context
 *
 * \param ctx: the cbor writing context to prepare
 * \param format: a printf style argument map
 * \param args: the va args
 *
 * CBOR-aware vsprintf which pauses output when it fills the output buffer.  You
 * can call it again with the same args and same lws_lex_pctx to resume filling
 *
 * Returns either LWS_LECPCTX_RET_FINISHED if we have nothing left over that we
 * want to put in the buffer, or LWS_LECPCTX_RET_AGAIN if the function should
 * be called again with the same arguments (perhaps into a different output
 * buffer) to continue emitting output from where it left off.
 *
 * If LWS_LECPCTX_RET_AGAIN is returned, lws_lec_setbuf() must be used on the
 * context to reset or change the output buffer before calling again.
 *
 * The number of bytes placed in the output buffer is available in ctx->used.
 *
 * \p format is a printf-type format string that is specialized for CBOR
 * generation.  It understands the following specifiers
 *
 * |`123`||unsigned literal number|
 * |`-123`||signed literal number|
 * |`%u`|`unsigned int`|number|
 * |`%lu`|`unsigned long int`|number|
 * |`%llu`|`unsigned long long int`|number|
 * |`%d`|`signed int`|number|
 * |`%ld`|`signed long int`|number|
 * |`%lld`|`signed long long int`|number|
 * |`%f`|`double`|floating point number|
 * |`123(...)`||literal tag and scope|
 * |`%t(...)`|`unsigned int`|tag and scope|
 * |`%lt(...)`|`unsigned long int`|tag and scope|
 * |`%llt(...)`|`unsigned long long int`|tag and scope|
 * |`[...]`||Array (fixed len if `]` in same format string)|
 * |`{...}`||Map (fixed len if `}` in same format string)|
 * |`<t...>`||Container for indeterminite text string frags|
 * |`<b...>`||Container for indeterminite binary string frags|
 * |`'string'`||Literal text of known length|
 * |`%s`|`const char *`|NUL-terminated string|
 * |`%.*s`|`int`, `const char *`|length-specified string|
 * |`%.*b`|`int`, `const uint8_t *`|length-specified binary|
 * |`:`||separator between Map items (a:b)|
 * |`,`||separator between Map pairs or array items|
 *
 * See READMEs/README.cbor-lecp.md for more details.
 */
LWS_VISIBLE LWS_EXTERN enum lws_lec_pctx_ret
lws_lec_vsprintf(lws_lec_pctx_t *ctx, const char *format, va_list args);

/*
 * lws_lec_printf() - write into a cbor writing context
 *
 * \param ctx: the cbor writing context to prepare
 * \param format: a printf style argument map
 * \param ...: format args
 *
 * See lws_lec_vsprintf() for format details.  This is the most common way
 * to format the CBOR output.
 *
 * See READMEs/README.cbor-lecp.md for more details.
 */
LWS_VISIBLE LWS_EXTERN enum lws_lec_pctx_ret
lws_lec_printf(lws_lec_pctx_t *ctx, const char *format, ...);

/**
 * lecp_construct() - Construct an LECP parser context
 *
 * \param ctx: the parser context object to be initialized
 * \param cb: the user callback to receive the parsing events
 * \param user: an opaque user pointer available at \p cb
 * \param paths: an optional array of parsing paths
 * \param paths_count: how many paths in \p paths
 *
 * Prepares an LECP parser context for parsing.
 */
LWS_VISIBLE LWS_EXTERN void
lecp_construct(struct lecp_ctx *ctx, lecp_callback cb, void *user,
	       const char * const *paths, unsigned char paths_count);

/**
 * lecp_destruct() - Destroys an LECP parser context
 *
 * \param ctx: the parser context object to be destroyed
 */
LWS_VISIBLE LWS_EXTERN void
lecp_destruct(struct lecp_ctx *ctx);

/**
 * lecp_parse() - parses a chunk of input CBOR
 *
 * \p ctx: the parsing context
 * \p cbor: the start of the chunk of CBOR
 * \p len: the number of bytes of CBOR available at \p cbor
 *
 * Returns LECP_CONTINUE if more input needed, one of enum lecp_reasons for a
 * fatal error, else 0 for successful parsing completion.
 *
 * On success or _CONTINUE, ctx->used_in is set to the number of input bytes
 * consumed.
 */
LWS_VISIBLE LWS_EXTERN int
lecp_parse(struct lecp_ctx *ctx, const uint8_t *cbor, size_t len);

LWS_VISIBLE LWS_EXTERN void
lecp_change_callback(struct lecp_ctx *ctx, lecp_callback cb);

LWS_VISIBLE LWS_EXTERN const char *
lecp_error_to_string(int e);

/**
 * lecp_parse_report_raw() - turn cbor raw reporting on and off
 *
 * \param ctx: the lecp context
 * \param on: 0 to disable (defaults disabled), 1 to enable
 *
 * For cose_sign, it needs access to raw cbor subtrees for the hash input.
 * This api causes LECPCB_LITERAL_CBOR parse callbacks when there are
 * ctx->cbor_pos bytes of raw cbor available in ctx->cbor[]. the callbacks
 * occur when the ctx->cbor[] buffer fills or if it holds anything when this
 * spi is used to stop the reports.
 *
 * The same CBOR that is being captured continues to be passed for parsing.
 */
LWS_VISIBLE LWS_EXTERN void
lecp_parse_report_raw(struct lecp_ctx *ctx, int on);

/**
 * lecp_parse_map_is_key() - return nonzero if we're in a map and this is a key
 *
 * \param ctx: the lwcp context
 *
 * Checks if the current value is a key in a map, ie, that you are on a "key" in
 * a list of "{key: value}" pairs.  Zero means you're either not in a map or not
 * on the key part, and nonzero means you are in a map and on a key part.
 */
LWS_VISIBLE LWS_EXTERN int
lecp_parse_map_is_key(struct lecp_ctx *ctx);

LWS_VISIBLE LWS_EXTERN int
lecp_parse_subtree(struct lecp_ctx *ctx, const uint8_t *in, size_t len);

/*
 * Helpers for half-float
 */

LWS_VISIBLE LWS_EXTERN void
lws_singles2halfp(uint16_t *hp, uint32_t x);

LWS_VISIBLE LWS_EXTERN void
lws_halfp2singles(uint32_t *xp, uint16_t h);

//@}
