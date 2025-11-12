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

#include "private-lib-core.h"

static int
lcs_init_compression_deflate(lws_comp_ctx_t *ctx, int decomp)
{
	int n;

	ctx->is_decompression = !!decomp;
	ctx->u.deflate = lws_malloc(sizeof(*ctx->u.deflate), __func__);

	if (!ctx->u.deflate)
		return 2;

	memset(ctx->u.deflate, 0, sizeof(*ctx->u.deflate));

	if (!decomp &&
	    (n = deflateInit2(ctx->u.deflate, 1, Z_DEFLATED, -15, 8,
			 Z_DEFAULT_STRATEGY)) != Z_OK) {
		lwsl_err("deflate init failed: %d\n", n);
		lws_free_set_NULL(ctx->u.deflate);

		return 1;
	}

	if (decomp &&
	    inflateInit2(ctx->u.deflate, 16 + 15) != Z_OK) {
		lws_free_set_NULL(ctx->u.deflate);
		return 1;
	}

	return 0;
}

static int
lcs_process_deflate(lws_comp_ctx_t *ctx, const void *in, size_t *ilen_iused,
		    void *out, size_t *olen_oused)
{
	size_t olen_oused_in = *olen_oused;
	int n;

	ctx->u.deflate->next_in = (void *)in;
	ctx->u.deflate->avail_in = (unsigned int)*ilen_iused;

	ctx->u.deflate->next_out = out;
	ctx->u.deflate->avail_out = (unsigned int)*olen_oused;

	if (!ctx->is_decompression)
		n = deflate(ctx->u.deflate, Z_SYNC_FLUSH);
	else
		n = inflate(ctx->u.deflate, Z_SYNC_FLUSH);

	switch (n) {
	case Z_NEED_DICT:
	case Z_STREAM_ERROR:
	case Z_DATA_ERROR:
	case Z_MEM_ERROR:
		lwsl_err("zlib error inflate %d\n", n);
		return -1;
	}

	*ilen_iused -= ctx->u.deflate->avail_in;
	*olen_oused -= ctx->u.deflate->avail_out;

	/* it's ambiguous with zlib... */
	ctx->may_have_more = (*olen_oused == olen_oused_in);

	return n == Z_STREAM_END;
}

static void
lcs_destroy_deflate(lws_comp_ctx_t *ctx)
{
	if (!ctx)
		return;

	if (!(*ctx).is_decompression)
		deflateEnd((*ctx).u.deflate);
	else
		inflateEnd((*ctx).u.deflate);

	lws_free_set_NULL(ctx->u.deflate);
}

struct lws_compression_support lcs_deflate = {
	/* .encoding_name */		"deflate",
	/* .init_compression */		lcs_init_compression_deflate,
	/* .process */			lcs_process_deflate,
	/* .destroy */			lcs_destroy_deflate,
};
