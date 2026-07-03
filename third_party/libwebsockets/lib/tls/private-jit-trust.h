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
 *  This is included from private-lib-core.h if LWS_WITH_TLS
 *
 * First-party trusted certs are handled outside of JIT Trust, eg, in SS policy.
 * JIT Trust is used to validate arbitrary connections on demand, without
 * needing a complete set of CAs in memory.
 *
 * Instantiated CA X509s are bound to dedicated SSL_CTX in their own dynamic
 * vhosts for client connections to use, these are lazily culled when they have
 * no remaining active connections using them.
 *
 *   - check jit trust cache to see if hostname has vhost already
 *      - if so, use it
 *      - if not, check jit trust cache to see if we know the trusted kids list,
 *   - attempt connection
 *   - remote or local trust blob / store
 */

#if !defined(__LWS_TLS_PRIVATE_JIT_TRUST_H__)
#define __LWS_TLS_PRIVATE_JIT_TRUST_H__

/*
 * Refer to ./READMEs/README.jit-trust.md for blob layout specification
 */

#define LWS_JIT_TRUST_MAGIC_BE		0x54424c42

enum {
	LJT_OFS_32_COUNT_CERTS		= 6,
	LJT_OFS_32_DERLEN		= 0x0c,
	LJT_OFS_32_SKIDLEN		= 0x10,
	LJT_OFS_32_SKID			= 0x14,
	LJT_OFS_END			= 0x18,

	LJT_OFS_DER			= 0x1c,
};

typedef struct {
	uint8_t				kid[20];
	uint8_t				kid_len;
} lws_tls_kid_t;

typedef struct {
	lws_tls_kid_t			akid[4];
	lws_tls_kid_t			skid[4];
	uint8_t				count;
} lws_tls_kid_chain_t;

/*
 * This is used to manage ongoing jit trust lookups for a specific host.  It
 * collects results and any trusted DER certs until all of them have arrived,
 * then caches the hostname -> trusted SKIDs mapping, and creates a vhost +
 * SSL_CTX trusting the certs named after the trusted SKIDs.
 *
 * The cert copies and this inflight object are then freed.
 *
 * JIT Trust lookups may be async, there may be multiple lookups fired at one
 * time, and these mappings are not actually related to a wsi lifetime, so these
 * separate inflight tracking objects are needed.
 *
 * These objects only live until all the AKID lookups for the host that created
 * them complete.
 */

typedef struct {
	lws_dll2_t			list;

	lws_tls_kid_t			kid[2];	/* SKID of the der if any */
	uint8_t				*der[2]; /* temp allocated */

	int				ders;

	uint32_t			tag; /* xor'd from start of SKIDs that
					      * that contributed certs, so we
					      * can name the vhost in a way that
					      * can be regenerated no matter
					      * the order of SKID results
					      */

	short				der_len[2];

	char				refcount; /* expected results left */

	/* hostname overcommitted */
} lws_tls_jit_inflight_t;

/*
 * These are the items in the jit trust cache, the cache tag is the hostname
 * and it resolves to one of these if present.  It describes 1 - 3 SKIDs
 * of trusted CAs needed to validate that host, and a 32-bit tag that is
 * the first 4 bytes of each valid SKID xor'd together, so you can find any
 * existing vhost that already has the required trust (independent of the
 * order they are checked in due to commutative xor).
 */

typedef struct {
	lws_tls_kid_t			skids[3];
	int				count_skids;
	uint32_t			xor_tag;
} lws_tls_jit_cache_item_t;

union lws_tls_cert_info_results;

void
lws_tls_kid_copy(union lws_tls_cert_info_results *ci, lws_tls_kid_t *kid);

int
lws_tls_kid_cmp(const lws_tls_kid_t *a, const lws_tls_kid_t *b);

int
lws_tls_jit_trust_sort_kids(struct lws *wsi, lws_tls_kid_chain_t *ch);

void
lws_tls_jit_trust_inflight_destroy(lws_tls_jit_inflight_t *inf);

void
lws_tls_jit_trust_inflight_destroy_all(struct lws_context *cx);

int
lws_tls_jit_trust_vhost_bind(struct lws_context *cx, const char *address,
			     struct lws_vhost **pvh);

void
lws_tls_jit_trust_vh_start_grace(struct lws_vhost *vh);

#endif

