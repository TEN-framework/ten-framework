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
 * This is included from private-lib-core.h if LWS_ROLE_WS
 */

#if defined(LWS_WITH_ZLIB)
#if defined(LWS_WITH_MINIZ)
#include <miniz.h>
#else
#include <zlib.h>
#endif
#endif

extern const struct lws_role_ops role_ops_cgi;

#define lwsi_role_cgi(wsi) (wsi->role_ops == &role_ops_cgi)

#define LWS_HTTP_CHUNK_HDR_SIZE 16

enum {
	SIGNIFICANT_HDR_CONTENT_LENGTH,		/* numeric */
	SIGNIFICANT_HDR_LOCATION,
	SIGNIFICANT_HDR_STATUS,			/* numeric */
	SIGNIFICANT_HDR_TRANSFER_ENCODING,
	SIGNIFICANT_HDR_CONTENT_ENCODING_GZIP,

	SIGNIFICANT_HDR_COUNT
};

struct lws;

/* wsi who is owns the cgi points to an lws_cgi */

struct lws_cgi {
	struct lws_cgi *cgi_list;

	struct lws_spawn_piped		*lsp;
	lws_sorted_usec_list_t		sul_grace;

	struct lws *wsi; /* owner */
	unsigned char *headers_buf;
	unsigned char *headers_start;
	unsigned char *headers_pos;
	unsigned char *headers_dumped;
	unsigned char *headers_end;

	char summary[128];
#if defined(LWS_WITH_ZLIB)
	z_stream inflate;
	uint8_t inflate_buf[1024];
#endif

	lws_filepos_t post_in_expected;
	lws_filepos_t content_length;
	lws_filepos_t content_length_seen;

	int match[SIGNIFICANT_HDR_COUNT];
	char l[12];
	int response_code;
	int lp;

	unsigned char being_closed:1;
	unsigned char explicitly_chunked:1;
	unsigned char cgi_transaction_over:1;
	unsigned char implied_chunked:1;
	unsigned char gzip_inflate:1;
	unsigned char gzip_init:1;

	unsigned char chunked_grace;
};
