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

#define VERBOSE

#define MAX_BLOBBED_PARAMS		96 /* largest bstr-encoded params */

enum {
	ST_UNKNOWN,

	ST_OUTER_PROTECTED,
	ST_OUTER_UNPROTECTED,
	ST_OUTER_PAYLOAD,
	ST_OUTER_SIGN1_SIGNATURE,

	ST_OUTER_SIGN_SIGARRAY,

	ST_OUTER_MACTAG,

	ST_INNER_PROTECTED,
	ST_INNER_UNPROTECTED,
	ST_INNER_SIGNATURE,

	ST_INNER_EXCESS,
};

typedef struct lws_cose_sig_alg {
	lws_dll2_t			list;
	uint8_t				rhash[512];
	const lws_cose_key_t		*cose_key;
	struct lws_genhash_ctx		hash_ctx;
	union {
		struct lws_genec_ctx	ecdsactx;
		struct lws_genrsa_ctx	rsactx;
		struct lws_genhmac_ctx	hmacctx;
	} u;
	cose_param_t			cose_alg;
	int				keybits;
	int				rhash_len;

	char				failed;
	char				completed;
} lws_cose_sig_alg_t;

typedef struct lws_cose_validate_param_stack {
	uint8_t				ph[4][MAX_BLOBBED_PARAMS];
	int				ph_pos[4];
	struct lws_gencrypto_keyelem	kid;
	cose_param_t			alg;
} lws_cose_validate_param_stack_t;

struct lws_cose_validate_context {
	lws_cose_validate_create_info_t	info;
	uint8_t				mac[LWS_GENHASH_LARGEST];
	uint8_t				sig_agg[512];
	lws_cose_validate_param_stack_t	st[3];
	lws_dll2_owner_t		algs;
	lws_dll2_owner_t		results;
	uint8_t				*payload_stash;
	struct lwsac			*ac;
	struct lecp_ctx			ctx;
	void				*user;

	size_t				payload_pos;
	size_t				payload_stash_size;

	int				seen;
	int				depth;

	int				outer;
	size_t				mac_pos;
	size_t				sig_agg_pos;

	cose_param_t			map_key; /* parsing temp before val */

	int				tli; /* toplevel item */
	int				sp;

	uint8_t				sub;
};

struct lws_cose_sign_context {
	lws_cose_sign_create_info_t	info;

	lws_dll2_owner_t		algs;
	lws_cose_sig_alg_t		*alg;

	size_t				rem_pay;
	enum lws_cose_sig_types 	type; /* computed */
	int				flags;

	size_t				along;

	int				tli;

	char				subsequent;
};

extern const uint8_t *sig_mctx[];
extern uint8_t sig_mctx_len[];
extern const char *cose_sections[];

lws_cose_sig_alg_t *
lws_cose_val_alg_create(struct lws_context *cx, lws_cose_key_t *ck,
		    cose_param_t cose_alg, int op);

int
lws_cose_val_alg_hash(lws_cose_sig_alg_t *alg, const uint8_t *in, size_t in_len);

void
lws_cose_val_alg_destroy(struct lws_cose_validate_context *cps,
		     lws_cose_sig_alg_t **_alg, const uint8_t *against,
		     size_t against_len);

lws_cose_sig_alg_t *
lws_cose_sign_alg_create(struct lws_context *cx, const lws_cose_key_t *ck,
		    cose_param_t cose_alg, int op);

int
lws_cose_sign_alg_hash(lws_cose_sig_alg_t *alg, const uint8_t *in, size_t in_len);

void
lws_cose_sign_alg_complete(lws_cose_sig_alg_t *alg);

void
lws_cose_sign_alg_destroy(lws_cose_sig_alg_t **_alg);

