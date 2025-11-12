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
 *  This is included from private-lib-core.h if either H1 or H2 roles are
 *  enabled
 */

#if defined(LWS_WITH_HUBBUB)
  #include <hubbub/hubbub.h>
  #include <hubbub/parser.h>
 #endif

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
#include "private-lib-roles-http-compression.h"
#endif

#define lwsi_role_http(wsi) (lwsi_role_h1(wsi) || lwsi_role_h2(wsi))

enum http_version {
	HTTP_VERSION_1_0,
	HTTP_VERSION_1_1,
	HTTP_VERSION_2
};

enum http_conn_type {
	HTTP_CONNECTION_CLOSE,
	HTTP_CONNECTION_KEEP_ALIVE
};

/*
 * This is totally opaque to code using the library.  It's exported as a
 * forward-reference pointer-only declaration; the user can use the pointer with
 * other APIs to get information out of it.
 */

#if defined(LWS_PLAT_FREERTOS)
typedef uint16_t ah_data_idx_t;
#else
typedef uint32_t ah_data_idx_t;
#endif

struct lws_fragments {
	ah_data_idx_t	offset;
	uint16_t	len;
	uint8_t		nfrag; /* which ah->frag[] continues this content, or 0 */
	uint8_t		flags; /* only http2 cares */
};

#if defined(LWS_WITH_RANGES)
enum range_states {
	LWSRS_NO_ACTIVE_RANGE,
	LWSRS_BYTES_EQ,
	LWSRS_FIRST,
	LWSRS_STARTING,
	LWSRS_ENDING,
	LWSRS_COMPLETED,
	LWSRS_SYNTAX,
};

struct lws_range_parsing {
	unsigned long long start, end, extent, agg, budget;
	const char buf[128];
	int pos;
	enum range_states state;
	char start_valid, end_valid, ctr, count_ranges, did_try, inside, send_ctr;
};

int
lws_ranges_init(struct lws *wsi, struct lws_range_parsing *rp,
		unsigned long long extent);
int
lws_ranges_next(struct lws_range_parsing *rp);
void
lws_ranges_reset(struct lws_range_parsing *rp);
#endif

#define LWS_HTTP_NO_KNOWN_HEADER 0xff

/*
 * these are assigned from a pool held in the context.
 * Both client and server mode uses them for http header analysis
 */

struct allocated_headers {
	struct allocated_headers *next; /* linked list */
	struct lws *wsi; /* owner */
	char *data; /* prepared by context init to point to dedicated storage */
	ah_data_idx_t data_length;
	/*
	 * the randomly ordered fragments, indexed by frag_index and
	 * lws_fragments->nfrag for continuation.
	 */
	struct lws_fragments frags[WSI_TOKEN_COUNT];
	time_t assigned;
	/*
	 * for each recognized token, frag_index says which frag[] his data
	 * starts in (0 means the token did not appear)
	 * the actual header data gets dumped as it comes in, into data[]
	 */
	uint8_t frag_index[WSI_TOKEN_COUNT];

#if defined(LWS_WITH_CLIENT)
	char initial_handshake_hash_base64[30];
#endif
	int hdr_token_idx;

	ah_data_idx_t pos;
	ah_data_idx_t http_response;
	ah_data_idx_t current_token_limit;
	ah_data_idx_t unk_pos; /* to undo speculative unknown header */

#if defined(LWS_WITH_CUSTOM_HEADERS)
	ah_data_idx_t unk_value_pos;

	ah_data_idx_t unk_ll_head;
	ah_data_idx_t unk_ll_tail;
#endif

	int16_t lextable_pos;

	uint8_t in_use;
	uint8_t nfrag;
	char /*enum uri_path_states */ ups;
	char /*enum uri_esc_states */ ues;

	char esc_stash;
	char post_literal_equal;
	uint8_t /* enum lws_token_indexes */ parser_state;
};



#if defined(LWS_WITH_HUBBUB)
struct lws_rewrite {
	hubbub_parser *parser;
	hubbub_parser_optparams params;
	const char *from, *to;
	int from_len, to_len;
	unsigned char *p, *end;
	struct lws *wsi;
};
static LWS_INLINE int hstrcmp(hubbub_string *s, const char *p, int len)
{
	if ((int)s->len != len)
		return 1;

	return strncmp((const char *)s->ptr, p, len);
}
typedef hubbub_error (*hubbub_callback_t)(const hubbub_token *token, void *pw);
LWS_EXTERN struct lws_rewrite *
lws_rewrite_create(struct lws *wsi, hubbub_callback_t cb, const char *from, const char *to);
LWS_EXTERN void
lws_rewrite_destroy(struct lws_rewrite *r);
LWS_EXTERN int
lws_rewrite_parse(struct lws_rewrite *r, const unsigned char *in, int in_len);
#endif

struct lws_pt_role_http {
	struct allocated_headers *ah_list;
	struct lws *ah_wait_list;
#ifdef LWS_WITH_CGI
	struct lws_cgi *cgi_list;
#endif
	int ah_wait_list_length;
	uint32_t ah_pool_length;

	int ah_count_in_use;
};

struct lws_peer_role_http {
	uint32_t count_ah;
	uint32_t total_ah;
};

struct lws_vhost_role_http {
#if defined(LWS_CLIENT_HTTP_PROXYING)
	char http_proxy_address[128];
#endif
	const struct lws_http_mount *mount_list;
	const char *error_document_404;
#if defined(LWS_CLIENT_HTTP_PROXYING)
	unsigned int http_proxy_port;
#endif
};

#ifdef LWS_WITH_ACCESS_LOG
struct lws_access_log {
	char *header_log;
	char *user_agent;
	char *referrer;
	unsigned long sent;
	int response;
};
#endif

#define LWS_HTTP_CHUNK_HDR_MAX_SIZE (6 + 2) /* 6 hex digits and then CRLF */
#define LWS_HTTP_CHUNK_TRL_MAX_SIZE (2 + 5) /* CRLF, then maybe 0 CRLF CRLF */

struct _lws_http_mode_related {
	struct lws *new_wsi_list;

	unsigned char *pending_return_headers;
	size_t pending_return_headers_len;
	size_t prh_content_length;

#if defined(LWS_WITH_HTTP_PROXY)
	struct lws_rewrite *rw;
	struct lws_buflist *buflist_post_body;
#endif
	struct allocated_headers *ah;
	struct lws *ah_wait_list;

	unsigned long		writeable_len;

#if defined(LWS_WITH_FILE_OPS)
	lws_filepos_t filepos;
	lws_filepos_t filelen;
	lws_fop_fd_t fop_fd;
#endif
#if defined(LWS_WITH_CLIENT)
	char multipart_boundary[16];
#endif
#if defined(LWS_WITH_RANGES)
	struct lws_range_parsing range;
	char multipart_content_type[64];
#endif

#ifdef LWS_WITH_ACCESS_LOG
	struct lws_access_log access_log;
#endif
#if defined(LWS_WITH_SERVER)
	unsigned int response_code;
#endif
#ifdef LWS_WITH_CGI
	struct lws_cgi *cgi; /* wsi being cgi stream have one of these */
#endif
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	struct lws_compression_support *lcs;
	lws_comp_ctx_t comp_ctx;
	unsigned char comp_accept_mask;
#endif

	enum http_version request_version;
	enum http_conn_type conn_type;
	lws_filepos_t tx_content_length;
	lws_filepos_t tx_content_remain;
	lws_filepos_t rx_content_length;
	lws_filepos_t rx_content_remain;

#if defined(LWS_WITH_HTTP_PROXY)
	unsigned int perform_rewrite:1;
	unsigned int proxy_clientside:1;
	unsigned int proxy_parent_chunked:1;
#endif
	unsigned int deferred_transaction_completed:1;
	unsigned int content_length_explicitly_zero:1;
	unsigned int content_length_given:1;
	unsigned int did_stream_close:1;
	unsigned int multipart:1;
	unsigned int cgi_transaction_complete:1;
	unsigned int multipart_issue_boundary:1;
};


#if defined(LWS_WITH_CLIENT)
enum lws_chunk_parser {
	ELCP_HEX,
	ELCP_CR,
	ELCP_CONTENT,
	ELCP_POST_CR,
	ELCP_POST_LF,
	ELCP_TRAILER_CR,
	ELCP_TRAILER_LF
};
#endif

enum lws_parse_urldecode_results {
	LPUR_CONTINUE,
	LPUR_SWALLOW,
	LPUR_FORBID,
	LPUR_EXCESSIVE,
};

enum lws_check_basic_auth_results {
	LCBA_CONTINUE,
	LCBA_FAILED_AUTH,
	LCBA_END_TRANSACTION,
};

enum lws_check_basic_auth_results
lws_check_basic_auth(struct lws *wsi, const char *basic_auth_login_file, unsigned int auth_mode);

int
lws_unauthorised_basic_auth(struct lws *wsi);

int
lws_read_h1(struct lws *wsi, unsigned char *buf, lws_filepos_t len);

void
_lws_header_table_reset(struct allocated_headers *ah);

LWS_EXTERN int
_lws_destroy_ah(struct lws_context_per_thread *pt, struct allocated_headers *ah);

int
lws_http_proxy_start(struct lws *wsi, const struct lws_http_mount *hit,
		     char *uri_ptr, char ws);

void
lws_sul_http_ah_lifecheck(lws_sorted_usec_list_t *sul);

uint8_t *
lws_http_multipart_headers(struct lws *wsi, uint8_t *p);

int
lws_http_string_to_known_header(const char *s, size_t slen);

int
lws_http_date_render_from_unix(char *buf, size_t len, const time_t *t);

int
lws_http_date_parse_unix(const char *b, size_t len, time_t *t);

enum {
	CCTLS_RETURN_ERROR		= -1,
	CCTLS_RETURN_DONE		= 0,
	CCTLS_RETURN_RETRY		= 1,
};

int
lws_client_create_tls(struct lws *wsi, const char **pcce, int do_c1);
