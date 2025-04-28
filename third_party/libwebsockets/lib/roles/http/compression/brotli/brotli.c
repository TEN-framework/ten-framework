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
lcs_init_compression_brotli(lws_comp_ctx_t *ctx, int decomp)
{
	ctx->is_decompression = (unsigned char)!!decomp;

	if (!decomp) {
		ctx->u.br_en = BrotliEncoderCreateInstance(NULL, NULL, NULL);
		if (ctx->u.br_en) {
			BrotliEncoderSetParameter(ctx->u.br_en,
					BROTLI_PARAM_MODE, BROTLI_MODE_TEXT);
			BrotliEncoderSetParameter(ctx->u.br_en,
				BROTLI_PARAM_QUALITY, BROTLI_MIN_QUALITY);
		}
	}
	else
		ctx->u.br_de = BrotliDecoderCreateInstance(NULL, NULL, NULL);

	return !ctx->u.br_de;
}

static int
lcs_process_brotli(lws_comp_ctx_t *ctx, const void *in, size_t *ilen_iused,
		   void *out, size_t *olen_oused)
{
	size_t a_in, a_out, t_out;
	const uint8_t *n_in;
	uint8_t *n_out;
	int n;

	n_in = (void *)in;
	a_in = *ilen_iused;
	a_out = *olen_oused;
	n_out = out;
	t_out = 0;

	if (!ctx->is_decompression) {

		if (!a_in && !BrotliEncoderHasMoreOutput(ctx->u.br_en)) {
			*olen_oused = 0;

			goto bail;
		}

		n = BROTLI_OPERATION_PROCESS;
		if (!ctx->buflist_comp && ctx->final_on_input_side)
			n = BROTLI_OPERATION_FINISH;

		if (BrotliEncoderCompressStream(ctx->u.br_en, n, &a_in, &n_in,
						&a_out, &n_out, &t_out) ==
		    BROTLI_FALSE) {
			lwsl_err("brotli encode failed\n");

			return -1;
		}

		ctx->may_have_more = !a_out;

	} else {
		n = BrotliDecoderDecompressStream(ctx->u.br_de, &a_in, &n_in,
						  &a_out, &n_out, &t_out);

		switch (n) {
		case BROTLI_DECODER_RESULT_ERROR:
			lwsl_err("brotli decoder error\n");
			return -1;
		}
	}

	*ilen_iused -= a_in;
	*olen_oused -= a_out;

bail:
	if (!ctx->is_decompression)
		return BrotliEncoderIsFinished(ctx->u.br_en);
	else
		return BrotliDecoderIsFinished(ctx->u.br_de);
}

static void
lcs_destroy_brotli(lws_comp_ctx_t *ctx)
{
	if (!ctx)
		return;

	if (!(*ctx).is_decompression)
		BrotliEncoderDestroyInstance((*ctx).u.br_en);
	else
		BrotliDecoderDestroyInstance((*ctx).u.br_de);

	(*ctx).u.generic_ctx_ptr = NULL;
}

struct lws_compression_support lcs_brotli = {
	/* .encoding_name */		"br",
	/* .init_compression */		lcs_init_compression_brotli,
	/* .process */			lcs_process_brotli,
	/* .destroy */			lcs_destroy_brotli,
};
