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
 *  This is included from private-lib-core.h if LWS_WITH_HTTP_STREAM_COMPRESSION
 */

#if defined(LWS_WITH_MINIZ)
#include <miniz.h>
#else
#include <zlib.h>
#endif
#if defined(LWS_WITH_HTTP_BROTLI)
#include <brotli/encode.h>
#include <brotli/decode.h>
#endif

/*
 * struct holding union of all the available compression methods' context data,
 * and state if it's compressing or decompressing
 */

typedef struct lws_compression_ctx {
	union {

#if defined(LWS_WITH_HTTP_BROTLI)
		BrotliEncoderState *br_en;
		BrotliDecoderState *br_de;
#endif
		z_stream *deflate;
		void *generic_ctx_ptr;
	} u;

	struct lws_buflist *buflist_comp;

	unsigned int is_decompression:1;
	unsigned int final_on_input_side:1;
	unsigned int may_have_more:1;
	unsigned int chunking:1;
} lws_comp_ctx_t;

/* generic structure defining the interface to a compression method */

struct lws_compression_support {
	/** compression name as used by, eg, content-ecoding */
	const char *encoding_name;
	/** create a compression context for the compression method, or NULL */
	int (*init_compression)(lws_comp_ctx_t *ctx, int decomp);
	/** pass data into the context to be processed */
	int (*process)(lws_comp_ctx_t *ctx, const void *in, size_t *ilen_iused,
		       void *out, size_t *olen_oused);
	/** destroy the de/compression context */
	void (*destroy)(lws_comp_ctx_t *ctx);
};

extern struct lws_compression_support lcs_deflate;
extern struct lws_compression_support lcs_brotli;

int
lws_http_compression_validate(struct lws *wsi);

int
lws_http_compression_transform(struct lws *wsi, unsigned char *buf,
			       size_t len, enum lws_write_protocol *wp,
			       unsigned char **outbuf, size_t *olen_oused);

void
lws_http_compression_destroy(struct lws *wsi);
