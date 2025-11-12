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
#include "extension-permessage-deflate.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define LWS_ZLIB_MEMLEVEL 8

const struct lws_ext_options lws_ext_pm_deflate_options[] = {
	/* public RFC7692 settings */
	{ "server_no_context_takeover", EXTARG_NONE },
	{ "client_no_context_takeover", EXTARG_NONE },
	{ "server_max_window_bits",	EXTARG_OPT_DEC },
	{ "client_max_window_bits",	EXTARG_OPT_DEC },
	/* ones only user code can set */
	{ "rx_buf_size",		EXTARG_DEC },
	{ "tx_buf_size",		EXTARG_DEC },
	{ "compression_level",		EXTARG_DEC },
	{ "mem_level",			EXTARG_DEC },
	{ NULL, 0 }, /* sentinel */
};

static void
lws_extension_pmdeflate_restrict_args(struct lws *wsi,
				      struct lws_ext_pm_deflate_priv *priv)
{
	int n, extra;

	/* cap the RX buf at the nearest power of 2 to protocol rx buf */

	n = (int)wsi->a.context->pt_serv_buf_size;
	if (wsi->a.protocol->rx_buffer_size)
		n = (int)wsi->a.protocol->rx_buffer_size;

	extra = 7;
	while (n >= 1 << (extra + 1))
		extra++;

	if (extra < priv->args[PMD_RX_BUF_PWR2]) {
		priv->args[PMD_RX_BUF_PWR2] = (unsigned char)extra;
		lwsl_wsi_info(wsi, " Capping pmd rx to %d", 1 << extra);
	}
}

static unsigned char trail[] = { 0, 0, 0xff, 0xff };

LWS_VISIBLE int
lws_extension_callback_pm_deflate(struct lws_context *context,
				  const struct lws_extension *ext,
				  struct lws *wsi,
				  enum lws_extension_callback_reasons reason,
				  void *user, void *in, size_t len)
{
	struct lws_ext_pm_deflate_priv *priv =
				     (struct lws_ext_pm_deflate_priv *)user;
	struct lws_ext_pm_deflate_rx_ebufs *pmdrx =
				(struct lws_ext_pm_deflate_rx_ebufs *)in;
	struct lws_ext_option_arg *oa;
	int n, ret = 0, was_fin = 0, m;
	unsigned int pen = 0;
	int penbits = 0;

	switch (reason) {
	case LWS_EXT_CB_NAMED_OPTION_SET:
		oa = in;
		if (!oa->option_name)
			break;
		lwsl_wsi_ext(wsi, "named option set: %s", oa->option_name);
		for (n = 0; n < (int)LWS_ARRAY_SIZE(lws_ext_pm_deflate_options);
		     n++)
			if (!strcmp(lws_ext_pm_deflate_options[n].name,
				    oa->option_name))
				break;

		if (n == (int)LWS_ARRAY_SIZE(lws_ext_pm_deflate_options))
			break;
		oa->option_index = n;

		/* fallthru */

	case LWS_EXT_CB_OPTION_SET:
		oa = in;
		lwsl_wsi_ext(wsi, "option set: idx %d, %s, len %d",
			 oa->option_index, oa->start, oa->len);
		if (oa->start)
			priv->args[oa->option_index] = (unsigned char)atoi(oa->start);
		else
			priv->args[oa->option_index] = 1;

		if (priv->args[PMD_CLIENT_MAX_WINDOW_BITS] == 8)
			priv->args[PMD_CLIENT_MAX_WINDOW_BITS] = 9;

		lws_extension_pmdeflate_restrict_args(wsi, priv);
		break;

	case LWS_EXT_CB_OPTION_CONFIRM:
		if (priv->args[PMD_SERVER_MAX_WINDOW_BITS] < 8 ||
		    priv->args[PMD_SERVER_MAX_WINDOW_BITS] > 15 ||
		    priv->args[PMD_CLIENT_MAX_WINDOW_BITS] < 8 ||
		    priv->args[PMD_CLIENT_MAX_WINDOW_BITS] > 15)
			return -1;
		break;

	case LWS_EXT_CB_CLIENT_CONSTRUCT:
	case LWS_EXT_CB_CONSTRUCT:

		n = (int)context->pt_serv_buf_size;
		if (wsi->a.protocol->rx_buffer_size)
			n = (int)wsi->a.protocol->rx_buffer_size;

		if (n < 128) {
			lwsl_wsi_info(wsi, " permessage-deflate requires the protocol "
				  "(%s) to have an RX buffer >= 128",
				  wsi->a.protocol->name);
			return -1;
		}

		/* fill in **user */
		priv = lws_zalloc(sizeof(*priv), "pmd priv");
		*((void **)user) = priv;
		lwsl_wsi_ext(wsi, "LWS_EXT_CB_*CONSTRUCT");
		memset(priv, 0, sizeof(*priv));

		/* fill in pointer to options list */
		if (in)
			*((const struct lws_ext_options **)in) =
					lws_ext_pm_deflate_options;

		/* fallthru */

	case LWS_EXT_CB_OPTION_DEFAULT:

		/* set the public, RFC7692 defaults... */

		priv->args[PMD_SERVER_NO_CONTEXT_TAKEOVER] = 0,
		priv->args[PMD_CLIENT_NO_CONTEXT_TAKEOVER] = 0;
		priv->args[PMD_SERVER_MAX_WINDOW_BITS] = 15;
		priv->args[PMD_CLIENT_MAX_WINDOW_BITS] = 15;

		/* ...and the ones the user code can override */

		priv->args[PMD_RX_BUF_PWR2] = 10; /* ie, 1024 */
		priv->args[PMD_TX_BUF_PWR2] = 10; /* ie, 1024 */
		priv->args[PMD_COMP_LEVEL] = 1;
		priv->args[PMD_MEM_LEVEL] = 8;

		lws_extension_pmdeflate_restrict_args(wsi, priv);
		break;

	case LWS_EXT_CB_DESTROY:
		lwsl_wsi_ext(wsi, "LWS_EXT_CB_DESTROY");
		lws_free(priv->buf_rx_inflated);
		lws_free(priv->buf_tx_deflated);
		if (priv->rx_init)
			(void)inflateEnd(&priv->rx);
		if (priv->tx_init)
			(void)deflateEnd(&priv->tx);
		lws_free(priv);

		return ret;


	case LWS_EXT_CB_PAYLOAD_RX:
		/*
		 * ie, we are INFLATING
		 */
		lwsl_wsi_ext(wsi, " LWS_EXT_CB_PAYLOAD_RX: in %d, existing in %d",
			 pmdrx->eb_in.len, priv->rx.avail_in);

		/*
		 * If this frame is not marked as compressed,
		 * there is nothing we should do with it
		 */

		if (!(wsi->ws->rsv_first_msg & 0x40) || (wsi->ws->opcode & 8))
			/*
			 * This is a bit different than DID_NOTHING... we have
			 * identified using ext-private bits in the packet, or
			 * by it being a control fragment that we SHOULD not do
			 * anything to it, parent should continue as if we
			 * processed it
			 */
			return PMDR_NOTHING_WE_SHOULD_DO;

		/*
		 * we shouldn't come back in here if we already applied the
		 * trailer for this compressed packet
		 */
		if (!wsi->ws->pmd_trailer_application)
			return PMDR_DID_NOTHING;

		pmdrx->eb_out.len = 0;

		lwsl_wsi_ext(wsi, "LWS_EXT_CB_PAYLOAD_RX: in %d, "
			 "existing avail in %d, pkt fin: %d",
			 pmdrx->eb_in.len, priv->rx.avail_in, wsi->ws->final);

		/* if needed, initialize the inflator */

		if (!priv->rx_init) {
			if (inflateInit2(&priv->rx,
			     -priv->args[PMD_SERVER_MAX_WINDOW_BITS]) != Z_OK) {
				lwsl_wsi_err(wsi, "iniflateInit failed");
				return PMDR_FAILED;
			}
			priv->rx_init = 1;
			if (!priv->buf_rx_inflated)
				priv->buf_rx_inflated = lws_malloc(
					(unsigned int)(LWS_PRE + 7 + 5 +
					    (1 << priv->args[PMD_RX_BUF_PWR2])),
					    "pmd rx inflate buf");
			if (!priv->buf_rx_inflated) {
				lwsl_wsi_err(wsi, "OOM");
				return PMDR_FAILED;
			}
		}

#if 0
		/*
		 * don't give us new input while we still work through
		 * the last input
		 */

		if (priv->rx.avail_in && pmdrx->eb_in.token &&
					 pmdrx->eb_in.len) {
			lwsl_wsi_warn(wsi, "priv->rx.avail_in %d while getting new in",
					priv->rx.avail_in);
	//		assert(0);
		}
#endif
		if (!priv->rx.avail_in && pmdrx->eb_in.token && pmdrx->eb_in.len) {
			priv->rx.next_in = (unsigned char *)pmdrx->eb_in.token;
			priv->rx.avail_in = (uInt)pmdrx->eb_in.len;
		}

		priv->rx.next_out = priv->buf_rx_inflated + LWS_PRE;
		pmdrx->eb_out.token = priv->rx.next_out;
		priv->rx.avail_out = (uInt)(1 << priv->args[PMD_RX_BUF_PWR2]);

		/* so... if...
		 *
		 *  - he has no remaining input content for this message, and
		 *
		 *  - and this is the final fragment, and
		 *
		 *  - we used everything that could be drained on the input side
		 *
		 * ...then put back the 00 00 FF FF the sender stripped as our
		 * input to zlib
		 */
		if (!priv->rx.avail_in &&
		    wsi->ws->final &&
		    !wsi->ws->rx_packet_length &&
		    wsi->ws->pmd_trailer_application) {
			lwsl_wsi_ext(wsi, "trailer apply 1");
			was_fin = 1;
			wsi->ws->pmd_trailer_application = 0;
			priv->rx.next_in = trail;
			priv->rx.avail_in = sizeof(trail);
		}

		/*
		 * if after all that there's nothing pending and nothing to give
		 * him right now, bail without having done anything
		 */

		if (!priv->rx.avail_in)
			return PMDR_DID_NOTHING;

		n = inflate(&priv->rx, was_fin ? Z_SYNC_FLUSH : Z_NO_FLUSH);
		lwsl_wsi_ext(wsi, "inflate ret %d, avi %d, avo %d, wsifinal %d", n,
			 priv->rx.avail_in, priv->rx.avail_out, wsi->ws->final);
		switch (n) {
		case Z_NEED_DICT:
		case Z_STREAM_ERROR:
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			lwsl_wsi_err(wsi, "zlib error inflate %d: \"%s\"",
				  n, priv->rx.msg);
			return PMDR_FAILED;
		}

		/*
		 * track how much input was used, and advance it
		 */

		pmdrx->eb_in.token = pmdrx->eb_in.token +
				         ((unsigned int)pmdrx->eb_in.len - (unsigned int)priv->rx.avail_in);
		pmdrx->eb_in.len = (int)priv->rx.avail_in;

		lwsl_wsi_debug(wsi, "%d %d %d %d %d",
				priv->rx.avail_in,
				wsi->ws->final,
				(int)wsi->ws->rx_packet_length,
				was_fin,
				wsi->ws->pmd_trailer_application);

		if (!priv->rx.avail_in &&
		    wsi->ws->final &&
		    !wsi->ws->rx_packet_length &&
		    !was_fin &&
		    wsi->ws->pmd_trailer_application) {
			lwsl_wsi_ext(wsi, "RX trailer apply 2");

			/* we overallocated just for this situation where
			 * we might issue something */
			priv->rx.avail_out += 5;

			was_fin = 1;
			wsi->ws->pmd_trailer_application = 0;
			priv->rx.next_in = trail;
			priv->rx.avail_in = sizeof(trail);
			n = inflate(&priv->rx, Z_SYNC_FLUSH);
			lwsl_wsi_ext(wsi, "RX trailer infl ret %d, avi %d, avo %d",
				 n, priv->rx.avail_in, priv->rx.avail_out);
			switch (n) {
			case Z_NEED_DICT:
			case Z_STREAM_ERROR:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				lwsl_wsi_info(wsi, "zlib error inflate %d: %s",
					  n, priv->rx.msg);
				return -1;
			}

			assert(priv->rx.avail_out);
		}

		pmdrx->eb_out.len = lws_ptr_diff(priv->rx.next_out,
						 pmdrx->eb_out.token);
		priv->count_rx_between_fin = priv->count_rx_between_fin + (size_t)pmdrx->eb_out.len;

		lwsl_wsi_ext(wsi, "  RX leaving with new effbuff len %d, "
			 "rx.avail_in=%d, TOTAL RX since FIN %lu",
			 pmdrx->eb_out.len, priv->rx.avail_in,
			 (unsigned long)priv->count_rx_between_fin);

		if (was_fin) {
			lwsl_wsi_ext(wsi, "was_fin");
			priv->count_rx_between_fin = 0;
			if (priv->args[PMD_SERVER_NO_CONTEXT_TAKEOVER]) {
				lwsl_wsi_ext(wsi, "PMD_SERVER_NO_CONTEXT_TAKEOVER");
				(void)inflateEnd(&priv->rx);
				priv->rx_init = 0;
			}

			return PMDR_EMPTY_FINAL;
		}

		if (priv->rx.avail_in)
			return PMDR_HAS_PENDING;

		return PMDR_EMPTY_NONFINAL;

	case LWS_EXT_CB_PAYLOAD_TX:

		/*
		 * ie, we are DEFLATING
		 *
		 * initialize us if needed
		 */

		if (!priv->tx_init) {
			n = deflateInit2(&priv->tx, priv->args[PMD_COMP_LEVEL],
					 Z_DEFLATED,
					 -priv->args[PMD_SERVER_MAX_WINDOW_BITS +
						(wsi->a.vhost->listen_port <= 0)],
					 priv->args[PMD_MEM_LEVEL],
					 Z_DEFAULT_STRATEGY);
			if (n != Z_OK) {
				lwsl_wsi_ext(wsi, "inflateInit2 failed %d", n);
				return PMDR_FAILED;
			}
			priv->tx_init = 1;
		}

		if (!priv->buf_tx_deflated)
			priv->buf_tx_deflated = lws_malloc((unsigned int)(LWS_PRE + 7 + 5 +
					    (1 << priv->args[PMD_TX_BUF_PWR2])),
					    "pmd tx deflate buf");
		if (!priv->buf_tx_deflated) {
			lwsl_wsi_err(wsi, "OOM");
			return PMDR_FAILED;
		}

		/* hook us up with any deflated input that the caller has */

		if (pmdrx->eb_in.token) {

			assert(!priv->tx.avail_in);

			priv->count_tx_between_fin = priv->count_tx_between_fin + (size_t)pmdrx->eb_in.len;
			lwsl_wsi_ext(wsi, "TX: eb_in length %d, "
				    "TOTAL TX since FIN: %d",
				    pmdrx->eb_in.len,
				    (int)priv->count_tx_between_fin);
			priv->tx.next_in = (unsigned char *)pmdrx->eb_in.token;
			priv->tx.avail_in = (uInt)pmdrx->eb_in.len;
		}

		priv->tx.next_out = priv->buf_tx_deflated + LWS_PRE + 5;
		pmdrx->eb_out.token = priv->tx.next_out;
		priv->tx.avail_out = (uInt)(1 << priv->args[PMD_TX_BUF_PWR2]);

		pen = 0;
		penbits = 0;
		deflatePending(&priv->tx, &pen, &penbits);
		pen = pen | (unsigned int)penbits;

		if (!priv->tx.avail_in && (len & LWS_WRITE_NO_FIN)) {
			lwsl_wsi_ext(wsi, "no available in, pen: %u", pen);

			if (!pen)
				return PMDR_DID_NOTHING;
		}

		m = Z_NO_FLUSH;
		if (!(len & LWS_WRITE_NO_FIN)) {
			lwsl_wsi_ext(wsi, "deflate with SYNC_FLUSH, pkt len %d",
					(int)wsi->ws->rx_packet_length);
			m = Z_SYNC_FLUSH;
		}

		n = deflate(&priv->tx, m);
		if (n == Z_STREAM_ERROR) {
			lwsl_wsi_notice(wsi, "Z_STREAM_ERROR");
			return PMDR_FAILED;
		}

		pen = (!priv->tx.avail_out) && n != Z_STREAM_END;

		lwsl_wsi_ext(wsi, "deflate ret %d, len 0x%x", n,
				(unsigned int)len);

		if ((len & 0xf) == LWS_WRITE_TEXT)
			priv->tx_first_frame_type = LWSWSOPC_TEXT_FRAME;
		if ((len & 0xf) == LWS_WRITE_BINARY)
			priv->tx_first_frame_type = LWSWSOPC_BINARY_FRAME;

		pmdrx->eb_out.len = lws_ptr_diff(priv->tx.next_out,
						 pmdrx->eb_out.token);

		if (m == Z_SYNC_FLUSH && !(len & LWS_WRITE_NO_FIN) && !pen &&
		    pmdrx->eb_out.len < 4) {
			lwsl_wsi_err(wsi, "FAIL want to trim out length %d",
					(int)pmdrx->eb_out.len);
			assert(0);
		}

		if (!(len & LWS_WRITE_NO_FIN) &&
		    m == Z_SYNC_FLUSH &&
		    !pen &&
		    pmdrx->eb_out.len >= 4) {
			// lwsl_wsi_err(wsi, "Trimming 4 from end of write");
			priv->tx.next_out -= 4;
			priv->tx.avail_out += 4;
			priv->count_tx_between_fin = 0;

			assert(priv->tx.next_out[0] == 0x00 &&
			       priv->tx.next_out[1] == 0x00 &&
			       priv->tx.next_out[2] == 0xff &&
			       priv->tx.next_out[3] == 0xff);
		}


		/*
		 * track how much input was used and advance it
		 */

		pmdrx->eb_in.token = pmdrx->eb_in.token +
					((unsigned int)pmdrx->eb_in.len - (unsigned int)priv->tx.avail_in);
		pmdrx->eb_in.len = (int)priv->tx.avail_in;

		priv->compressed_out = 1;
		pmdrx->eb_out.len = lws_ptr_diff(priv->tx.next_out,
						 pmdrx->eb_out.token);

		lwsl_wsi_ext(wsi, "  TX rewritten with new eb_in len %d, "
				"eb_out len %d, deflatePending %d",
				pmdrx->eb_in.len, pmdrx->eb_out.len, pen);

		if (pmdrx->eb_in.len || pen)
			return PMDR_HAS_PENDING;

		if (!(len & LWS_WRITE_NO_FIN))
			return PMDR_EMPTY_FINAL;

		return PMDR_EMPTY_NONFINAL;

	case LWS_EXT_CB_PACKET_TX_PRESEND:
		if (!priv->compressed_out)
			break;
		priv->compressed_out = 0;

		/*
		 * we may have not produced any output for the actual "first"
		 * write... in that case, we need to fix up the inappropriate
		 * use of CONTINUATION when the first real write does come.
		 */
		if (priv->tx_first_frame_type & 0xf) {
			*pmdrx->eb_in.token = (unsigned char)((((unsigned char)*pmdrx->eb_in.token) & (unsigned char)~0xf) |
				((unsigned char)priv->tx_first_frame_type & (unsigned char)0xf));
			/*
			 * We have now written the "first" fragment, only
			 * do that once
			 */
			priv->tx_first_frame_type = 0;
		}

		n = *(pmdrx->eb_in.token) & 15;

		/* set RSV1, but not on CONTINUATION */
		if (n == LWSWSOPC_TEXT_FRAME || n == LWSWSOPC_BINARY_FRAME)
			*pmdrx->eb_in.token |= 0x40;

		lwsl_wsi_ext(wsi, "PRESEND compressed: ws frame 0x%02X, len %d",
			    ((*pmdrx->eb_in.token) & 0xff),
			    pmdrx->eb_in.len);

		if (((*pmdrx->eb_in.token) & 0x80) &&	/* fin */
		    priv->args[PMD_CLIENT_NO_CONTEXT_TAKEOVER]) {
			lwsl_wsi_debug(wsi, "PMD_CLIENT_NO_CONTEXT_TAKEOVER");
			(void)deflateEnd(&priv->tx);
			priv->tx_init = 0;
		}

		break;

	default:
		break;
	}

	return 0;
}

