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

/* Do not treat - as a terminal character, so "my-token" is one token */
#define LWS_TOKENIZE_F_MINUS_NONTERM	(1 << 0)
/* Separately report aggregate colon-delimited tokens */
#define LWS_TOKENIZE_F_AGG_COLON	(1 << 1)
/* Enforce sequencing for a simple token , token , token ... list */
#define LWS_TOKENIZE_F_COMMA_SEP_LIST	(1 << 2)
/* Allow more characters in the tokens and less delimiters... default is
 * only alphanumeric + underscore in tokens */
#define LWS_TOKENIZE_F_RFC7230_DELIMS	(1 << 3)
/* Do not treat . as a terminal character, so "warmcat.com" is one token */
#define LWS_TOKENIZE_F_DOT_NONTERM	(1 << 4)
/* If something starts looking like a float, like 1.2, force to be string token.
 * This lets you receive dotted-quads like 192.168.0.1 as string tokens, and
 * avoids illegal float format detection like 1.myserver.com */
#define LWS_TOKENIZE_F_NO_FLOATS	(1 << 5)
/* Instead of LWS_TOKZE_INTEGER, report integers as any other string token */
#define LWS_TOKENIZE_F_NO_INTEGERS	(1 << 6)
/* # makes the rest of the line a comment */
#define LWS_TOKENIZE_F_HASH_COMMENT	(1 << 7)
/* Do not treat / as a terminal character, so "multipart/related" is one token */
#define LWS_TOKENIZE_F_SLASH_NONTERM	(1 << 8)
/* Do not treat * as a terminal character, so "myfile*" is one token */
#define LWS_TOKENIZE_F_ASTERISK_NONTERM	(1 << 9)
/* Do not treat = as a terminal character, so "x=y" is one token */
#define LWS_TOKENIZE_F_EQUALS_NONTERM	(1 << 10)

typedef enum {

	LWS_TOKZE_ERRS			=  5, /* the number of errors defined */

	LWS_TOKZE_ERR_BROKEN_UTF8	= -5,	/* malformed or partial utf8 */
	LWS_TOKZE_ERR_UNTERM_STRING	= -4,	/* ended while we were in "" */
	LWS_TOKZE_ERR_MALFORMED_FLOAT	= -3,	/* like 0..1 or 0.1.1 */
	LWS_TOKZE_ERR_NUM_ON_LHS	= -2,	/* like 123= or 0.1= */
	LWS_TOKZE_ERR_COMMA_LIST	= -1,	/* like ",tok", or, "tok,," */

	LWS_TOKZE_ENDED = 0,		/* no more content */

	/* Note: results have ordinal 1+, EOT is 0 and errors are < 0 */

	LWS_TOKZE_DELIMITER,		/* a delimiter appeared */
	LWS_TOKZE_TOKEN,		/* a token appeared */
	LWS_TOKZE_INTEGER,		/* an integer appeared */
	LWS_TOKZE_FLOAT,		/* a float appeared */
	LWS_TOKZE_TOKEN_NAME_EQUALS,	/* token [whitespace] = */
	LWS_TOKZE_TOKEN_NAME_COLON,	/* token [whitespace] : (only with
					   LWS_TOKENIZE_F_AGG_COLON flag) */
	LWS_TOKZE_QUOTED_STRING,	/* "*", where * may have any char */

} lws_tokenize_elem;

/*
 * helper enums to allow caller to enforce legal delimiter sequencing, eg
 * disallow "token,,token", "token,", and ",token"
 */

enum lws_tokenize_delimiter_tracking {
	LWSTZ_DT_NEED_FIRST_CONTENT,
	LWSTZ_DT_NEED_DELIM,
	LWSTZ_DT_NEED_NEXT_CONTENT,
};

typedef struct lws_tokenize {
	const char *start; /**< set to the start of the string to tokenize */
	const char *token; /**< the start of an identified token or delimiter */
	size_t len;	/**< set to the length of the string to tokenize */
	size_t token_len;	/**< the length of the identied token or delimiter */

	uint16_t flags;	/**< optional LWS_TOKENIZE_F_ flags, or 0 */
	uint8_t delim;

	int8_t e; /**< convenient for storing lws_tokenize return */
} lws_tokenize_t;

/**
 * lws_tokenize() - breaks down a string into tokens and delimiters in-place
 *
 * \param ts: the lws_tokenize struct to init
 * \param start: the string to tokenize
 * \param flags: LWS_TOKENIZE_F_ option flags
 *
 * This initializes the tokenize struct to point to the given string, and
 * sets the length to 2GiB - 1 (so there must be a terminating NUL)... you can
 * override this requirement by setting ts.len yourself before using it.
 *
 * .delim is also initialized to LWSTZ_DT_NEED_FIRST_CONTENT.
 */

LWS_VISIBLE LWS_EXTERN void
lws_tokenize_init(struct lws_tokenize *ts, const char *start, int flags);

/**
 * lws_tokenize() - breaks down a string into tokens and delimiters in-place
 *
 * \param ts: the lws_tokenize struct with information and state on what to do
 *
 * The \p ts struct should have its start, len and flags members initialized to
 * reflect the string to be tokenized and any options.
 *
 * Then `lws_tokenize()` may be called repeatedly on the struct, returning one
 * of `lws_tokenize_elem` each time, and with the struct's `token` and
 * `token_len` members set to describe the content of the delimiter or token
 * payload each time.
 *
 * There are no allocations during the process.
 *
 * returns lws_tokenize_elem that was identified (LWS_TOKZE_ENDED means reached
 * the end of the string).
 */

LWS_VISIBLE LWS_EXTERN lws_tokenize_elem
lws_tokenize(struct lws_tokenize *ts);

/**
 * lws_tokenize_cstr() - copy token string to NUL-terminated buffer
 *
 * \param ts: pointer to lws_tokenize struct to operate on
 * \param str: destination buffer
 * \pparam max: bytes in destination buffer
 *
 * returns 0 if OK or nonzero if the string + NUL won't fit.
 */

LWS_VISIBLE LWS_EXTERN int
lws_tokenize_cstr(struct lws_tokenize *ts, char *str, size_t max);


/*
 * lws_strexp: flexible string expansion helper api
 *
 * This stateful helper can handle multiple separate input chunks and multiple
 * output buffer loads with arbitrary boundaries between literals and expanded
 * symbols.  This allows it to handle fragmented input as well as arbitrarily
 * long symbol expansions that are bigger than the output buffer itself.
 *
 * A user callback is used to convert symbol names to the symbol value.
 *
 * A single byte buffer for input and another for output can process any
 * length substitution then.  The state object is around 64 bytes on a 64-bit
 * system and it only uses 8 bytes stack.
 */


typedef int (*lws_strexp_expand_cb)(void *priv, const char *name, char *out,
				    size_t *pos, size_t olen, size_t *exp_ofs);

typedef struct lws_strexp {
	char			name[32];
	lws_strexp_expand_cb	cb;
	void			*priv;
	char			*out;
	size_t			olen;
	size_t			pos;

	size_t			exp_ofs;

	uint8_t			name_pos;
	char			state;
} lws_strexp_t;

enum {
	LSTRX_DONE,			/* it completed OK */
	LSTRX_FILLED_OUT,		/* out buf filled and needs resetting */
	LSTRX_FATAL_NAME_TOO_LONG = -1,	/* fatal */
	LSTRX_FATAL_NAME_UNKNOWN  = -2,
};


/**
 * lws_strexp_init() - initialize an lws_strexp_t for use
 *
 * \p exp: the exp object to init
 * \p priv: the user's object pointer to pass to callback
 * \p cb: the callback to expand named objects
 * \p out: the start of the output buffer, or NULL just to get the length
 * \p olen: the length of the output buffer in bytes
 *
 * Prepares an lws_strexp_t for use and sets the initial output buffer
 *
 * If \p out is NULL, substitution proceeds normally, but no output is produced,
 * only the length is returned.  olen should be set to the largest feasible
 * overall length.  To use this mode, the substitution callback must also check
 * for NULL \p out and avoid producing the output.
 */
LWS_VISIBLE LWS_EXTERN void
lws_strexp_init(lws_strexp_t *exp, void *priv, lws_strexp_expand_cb cb,
		char *out, size_t olen);

/**
 * lws_strexp_reset_out() - reset the output buffer on an existing strexp
 *
 * \p exp: the exp object to init
 * \p out: the start of the output buffer, or NULL to just get length
 * \p olen: the length of the output buffer in bytes
 *
 * Provides a new output buffer for lws_strexp_expand() to continue to write
 * into.  It can be the same as the old one if it has been copied out or used.
 * The position of the next write will be reset to the start of the given buf.
 *
 * If \p out is NULL, substitution proceeds normally, but no output is produced,
 * only the length is returned.  \p olen should be set to the largest feasible
 * overall length.  To use this mode, the substitution callback must also check
 * for NULL \p out and avoid producing the output.
 */
LWS_VISIBLE LWS_EXTERN void
lws_strexp_reset_out(lws_strexp_t *exp, char *out, size_t olen);

/**
 * lws_strexp_expand() - copy / expand a string into the output buffer
 *
 * \p exp: the exp object for the copy / expansion
 * \p in: the start of the next input data
 * \p len: the length of the input data
 * \p pused_in: pointer to write the amount of input used
 * \p pused_out: pointer to write the amount of output used
 *
 * Copies in to the output buffer set in exp, expanding any ${name} tokens using
 * the callback.  \p *pused_in is set to the number of input chars used and
 * \p *pused_out the number of output characters used
 *
 * May return LSTRX_FILLED_OUT early with *pused < len if the output buffer is
 * filled.  Handle the output buffer and reset it with lws_strexp_reset_out()
 * before calling again with adjusted in / len to continue.
 *
 * In the case of large expansions, the expansion itself may fill the output
 * buffer, in which case the expansion callback returns the LSTRX_FILLED_OUT
 * and will be called again to continue with its *exp_ofs parameter set
 * appropriately.
 */
LWS_VISIBLE LWS_EXTERN int
lws_strexp_expand(lws_strexp_t *exp, const char *in, size_t len,
		  size_t *pused_in, size_t *pused_out);

/**
 * lws_strcmp_wildcard() - strcmp but the first arg can have wildcards
 *
 * \p wildcard: a string that may contain zero to three *, and may lack a NUL
 * \p wlen: length of the wildcard string
 * \p check: string to test to see if it matches wildcard
 * \p clen: length of check string
 *
 * Like strcmp, but supports patterns like "a*", "a*b", "a*b*" etc
 * where a and b are arbitrary substrings.  Both the wc and check strings need
 * not be NUL terminated, but are specified by lengths.
 */
LWS_VISIBLE LWS_EXTERN int
lws_strcmp_wildcard(const char *wildcard, size_t wlen, const char *check,
		    size_t clen);
