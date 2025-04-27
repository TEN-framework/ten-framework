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

/* compression methods listed in order of preference */

struct lws_compression_support *lcs_available[] = {
#if defined(LWS_WITH_HTTP_BROTLI)
	&lcs_brotli,
#endif
	&lcs_deflate,
};

/* compute acceptable compression encodings while we still have an ah */

int
lws_http_compression_validate(struct lws *wsi)
{
	const char *a;
	size_t n;

	wsi->http.comp_accept_mask = 0;

	if (!wsi->http.ah || !lwsi_role_server(wsi))
		return 0;

	a = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_ACCEPT_ENCODING);
	if (!a)
		return 0;

	for (n = 0; n < LWS_ARRAY_SIZE(lcs_available); n++)
		if (strstr(a, lcs_available[n]->encoding_name))
			wsi->http.comp_accept_mask = (uint8_t)(wsi->http.comp_accept_mask | (1 << n));

	return 0;
}

int
lws_http_compression_apply(struct lws *wsi, const char *name,
			   unsigned char **p, unsigned char *end, char decomp)
{
	size_t n;

	for (n = 0; n < LWS_ARRAY_SIZE(lcs_available); n++) {
		/* if name is non-NULL, choose only that compression method */
		if (name && strcmp(lcs_available[n]->encoding_name, name))
			continue;
		/*
		 * If we're the server, confirm that the client told us he could
		 * handle this kind of compression transform...
		 */
		if (!decomp && !(wsi->http.comp_accept_mask & (1 << n)))
			continue;

		/* let's go with this one then... */
		break;
	}

	if (n == LWS_ARRAY_SIZE(lcs_available))
		return 1;

	lcs_available[n]->init_compression(&wsi->http.comp_ctx, decomp);
	if (!wsi->http.comp_ctx.u.generic_ctx_ptr) {
		lwsl_err("%s: init_compression %d failed\n", __func__, (int)n);
		return 1;
	}

	wsi->http.lcs = lcs_available[n];
	wsi->http.comp_ctx.may_have_more = 0;
	wsi->http.comp_ctx.final_on_input_side = 0;
	wsi->http.comp_ctx.chunking = 0;
	wsi->http.comp_ctx.is_decompression = !!decomp;

	if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_ENCODING,
			(unsigned char *)lcs_available[n]->encoding_name,
			(int)strlen(lcs_available[n]->encoding_name), p, end))
		return -1;

	lwsl_info("%s: %s: applied %s content-encoding\n", __func__,
		    lws_wsi_tag(wsi), lcs_available[n]->encoding_name);

	return 0;
}

void
lws_http_compression_destroy(struct lws *wsi)
{
	if (!wsi->http.lcs || !wsi->http.comp_ctx.u.generic_ctx_ptr)
		return;

	wsi->http.lcs->destroy(&wsi->http.comp_ctx);

	wsi->http.lcs = NULL;
}

/*
 * This manages the compression transform independent of h1 or h2.
 *
 * wsi->buflist_comp stashes pre-transform input that was not yet compressed
 */

int
lws_http_compression_transform(struct lws *wsi, unsigned char *buf,
			       size_t len, enum lws_write_protocol *wp,
			       unsigned char **outbuf, size_t *olen_oused)
{
	size_t ilen_iused = len;
	int n, use = 0, wp1f = (*wp) & 0x1f;
	lws_comp_ctx_t *ctx = &wsi->http.comp_ctx;

	ctx->may_have_more = 0;

	if (!wsi->http.lcs ||
	    (wp1f != LWS_WRITE_HTTP && wp1f != LWS_WRITE_HTTP_FINAL)) {
		*outbuf = buf;
		*olen_oused = len;

		return 0;
	}

	if (wp1f == LWS_WRITE_HTTP_FINAL) {
		/*
		 * ...we may get a large buffer that represents the final input
		 * buffer, but it may form multiple frames after being
		 * tranformed by compression; only the last of those is actually
		 * the final frame on the output stream.
		 *
		 * Note that we have received the FINAL input, and downgrade it
		 * to a non-final for now.
		 */
		ctx->final_on_input_side = 1;
		*wp = (unsigned int)(LWS_WRITE_HTTP | ((*wp) & ~0x1fu));
	}

	if (ctx->buflist_comp) {
		/*
		 * we can't send this new stuff when we have old stuff
		 * buffered and not compressed yet.  Add it to the tail
		 * and switch to trying to process the head.
		 */
		if (buf && len) {
			if (lws_buflist_append_segment(
					&ctx->buflist_comp, buf, len) < 0)
				return -1;
			lwsl_debug("%s: %s: adding %d to comp buflist\n",
				   __func__, lws_wsi_tag(wsi), (int)len);
		}

		len = lws_buflist_next_segment_len(&ctx->buflist_comp, &buf);
		ilen_iused = len;
		use = 1;
		lwsl_debug("%s: %s: trying comp buflist %d\n", __func__,
				lws_wsi_tag(wsi), (int)len);
	}

	if (!buf && ilen_iused)
		return 0;

	lwsl_debug("%s: %s: pre-process: ilen_iused %d, olen_oused %d\n",
		   __func__, lws_wsi_tag(wsi), (int)ilen_iused, (int)*olen_oused);

	n = wsi->http.lcs->process(ctx, buf, &ilen_iused, *outbuf, olen_oused);

	if (n && n != 1) {
		lwsl_err("%s: problem with compression\n", __func__);

		return -1;
	}

	if (!ctx->may_have_more && ctx->final_on_input_side)

		*wp = (unsigned int)(LWS_WRITE_HTTP_FINAL | ((*wp) & ~0x1fu));

	lwsl_debug("%s: %s: more %d, ilen_iused %d\n", __func__, lws_wsi_tag(wsi),
		   ctx->may_have_more, (int)ilen_iused);

	if (use && ilen_iused) {
		/*
		 * we were flushing stuff from the buflist head... account for
		 * however much actually got processed by the compression
		 * transform
		 */
		lws_buflist_use_segment(&ctx->buflist_comp, ilen_iused);
		lwsl_debug("%s: %s: marking %d of comp buflist as used "
			   "(ctx->buflist_comp %p)\n", __func__,
			   lws_wsi_tag(wsi), (int)len, ctx->buflist_comp);
	}

	if (!use && ilen_iused != len) {
		 /*
		  * ...we were sending stuff from the caller directly and not
		  * all of it got processed... stash on the buflist tail
		  */
		if (lws_buflist_append_segment(&ctx->buflist_comp,
					   buf + ilen_iused, len - ilen_iused) < 0)
			return -1;

		lwsl_debug("%s: buffering %d unused comp input\n", __func__,
			   (int)(len - ilen_iused));
	}
	if (ctx->buflist_comp || ctx->may_have_more)
		lws_callback_on_writable(wsi);

	return 0;
}
