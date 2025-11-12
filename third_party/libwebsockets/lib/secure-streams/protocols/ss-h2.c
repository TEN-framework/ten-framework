/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
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

#include <private-lib-core.h>

extern int
secstream_h1(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len);

static int
secstream_h2(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	lws_ss_state_return_t r;
	int n;

	switch (reason) {

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:

		if (!h)
			return -1;

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
		if (h->being_serialized) {
			/*
			 * We are the proxy-side SS for a remote client... we
			 * need to inform the client about the initial tx credit
			 * to write to it that the remote h2 server set up
			 */
			lwsl_info("%s: reporting initial tx cr from server %d\n",
				  __func__, wsi->txc.tx_cr);
			ss_proxy_onward_txcr((void *)(h + 1), wsi->txc.tx_cr);
		}
#endif

		n = secstream_h1(wsi, reason, user, in, len);

		if (!n && (h->policy->flags & LWSSSPOLF_LONG_POLL)) {
			lwsl_notice("%s: h2 client %s entering LONG_POLL\n",
					__func__, lws_wsi_tag(wsi));
			lws_h2_client_stream_long_poll_rxonly(wsi);
		}
		return n;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		/*
		 * Only allow the wsi that the handle believes is representing
		 * him to report closure up to h1
		 */
		if (!h || h->wsi != wsi)
			return 0;

		break;

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:

		if (!h)
			return -1;

		// lwsl_err("%s: h2 COMPLETED_CLIENT_HTTP\n", __func__);
		r = 0;
		if (h->hanging_som)
			r = h->info.rx(ss_to_userobj(h), NULL, 0, LWSSS_FLAG_EOM);

		h->txn_ok = 1;
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		if (h->hanging_som && r == LWSSSSRET_DESTROY_ME)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		h->hanging_som = 0;
		break;

	case LWS_CALLBACK_WSI_TX_CREDIT_GET:

		if (!h)
			return -1;

		/*
		 * The peer has sent us additional tx credit...
		 */
		lwsl_info("%s: LWS_CALLBACK_WSI_TX_CREDIT_GET: %d\n",
			    __func__, (int)len);

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
		if (h->being_serialized)
			/* we are the proxy-side SS for a remote client */
			ss_proxy_onward_txcr((void *)(h + 1), (int)len);
#endif
		break;

	default:
		break;
	}

	return secstream_h1(wsi, reason, user, in, len);
}

const struct lws_protocols protocol_secstream_h2 = {
	"lws-secstream-h2",
	secstream_h2,
	0, 0, 0, NULL, 0
};

/*
 * Munge connect info according to protocol-specific considerations... this
 * usually means interpreting aux in a protocol-specific way and using the
 * pieces at connection setup time, eg, http url pieces.
 *
 * len bytes of buf can be used for things with scope until after the actual
 * connect.
 */

int
secstream_connect_munge_h2(lws_ss_handle_t *h, char *buf, size_t len,
			   struct lws_client_connect_info *i,
			   union lws_ss_contemp *ct)
{
	const char *pbasis = h->policy->u.http.url;
	size_t used_in, used_out;
	lws_strexp_t exp;

	/* i.path on entry is used to override the policy urlpath if not "" */

	if (i->path[0])
		pbasis = i->path;

	if (h->policy->flags & LWSSSPOLF_QUIRK_NGHTTP2_END_STREAM)
		i->ssl_connection |= LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;

	if (h->policy->flags & LWSSSPOLF_H2_QUIRK_OVERFLOWS_TXCR)
		i->ssl_connection |= LCCSCF_H2_QUIRK_OVERFLOWS_TXCR;

	if (h->policy->flags & LWSSSPOLF_HTTP_MULTIPART)
		i->ssl_connection |= LCCSCF_HTTP_MULTIPART_MIME;

	if (h->policy->flags & LWSSSPOLF_HTTP_X_WWW_FORM_URLENCODED)
		i->ssl_connection |= LCCSCF_HTTP_X_WWW_FORM_URLENCODED;

	if (h->policy->flags & LWSSSPOLF_HTTP_CACHE_COOKIES)
		i->ssl_connection |= LCCSCF_CACHE_COOKIES;

	i->ssl_connection |= LCCSCF_PIPELINE;

	i->alpn = "h2";

	/* initial peer tx credit */

	if (h->info.manual_initial_tx_credit) {
		i->ssl_connection |= LCCSCF_H2_MANUAL_RXFLOW;
		i->manual_initial_tx_credit = h->info.manual_initial_tx_credit;
		lwsl_info("%s: initial txcr %d\n", __func__,
				i->manual_initial_tx_credit);
	}

	if (!pbasis)
		return 0;

	/* protocol aux is the path part */

	i->path = buf;
	buf[0] = '/';

	lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata, buf + 1, len - 1);

	if (lws_strexp_expand(&exp, pbasis, strlen(pbasis),
			      &used_in, &used_out) != LSTRX_DONE)
		return 1;

	return 0;
}

static int
secstream_tx_credit_add_h2(lws_ss_handle_t *h, int add)
{
	lwsl_info("%s: %s: add %d\n", __func__, lws_ss_tag(h), add);
	if (h->wsi)
		return lws_h2_update_peer_txcredit(h->wsi, (unsigned int)LWS_H2_STREAM_SID, add);

	return 0;
}

static int
secstream_tx_credit_est_h2(lws_ss_handle_t *h)
{
	if (h->wsi) {
		lwsl_info("%s: %s: est %d\n", __func__, lws_ss_tag(h),
				lws_h2_get_peer_txcredit_estimate(h->wsi));

		return lws_h2_get_peer_txcredit_estimate(h->wsi);
	}

	lwsl_info("%s: %s: Unknown (0)\n", __func__, lws_ss_tag(h));

	return 0;
}

const struct ss_pcols ss_pcol_h2 = {
	"h2",
	"h2",
	&protocol_secstream_h2,
	secstream_connect_munge_h2,
	secstream_tx_credit_add_h2,
	secstream_tx_credit_est_h2
};
