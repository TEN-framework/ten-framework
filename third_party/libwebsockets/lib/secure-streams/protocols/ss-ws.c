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

static int
secstream_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
#if defined(LWS_WITH_SERVER)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
#endif
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	uint8_t buf[LWS_PRE + 1400];
	lws_ss_state_return_t r;
	int f = 0, f1, n;
	size_t buflen;

	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_info("%s: CLIENT_CONNECTION_ERROR: %s\n", __func__,
			 in ? (char *)in : "(null)");
		if (!h)
			break;

#if defined(LWS_WITH_CONMON)
		lws_conmon_ss_json(h);
#endif

		r = lws_ss_event_helper(h, LWSSSCS_UNREACHABLE);
		if (r == LWSSSSRET_DESTROY_ME)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		h->wsi = NULL;
		r = lws_ss_backoff(h);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		break;

	case LWS_CALLBACK_CLOSED: /* server */
	case LWS_CALLBACK_CLIENT_CLOSED:
		if (!h)
			break;
		lws_sul_cancel(&h->sul_timeout);

#if defined(LWS_WITH_CONMON)
		lws_conmon_ss_json(h);
#endif

		r = lws_ss_event_helper(h, LWSSSCS_DISCONNECTED);
		if (r == LWSSSSRET_DESTROY_ME)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		if (h->wsi)
			lws_set_opaque_user_data(h->wsi, NULL);
		h->wsi = NULL;

#if defined(LWS_WITH_SERVER)
		lws_pt_lock(pt, __func__);
		lws_dll2_remove(&h->cli_list);
		lws_pt_unlock(pt);
#endif

		if (reason == LWS_CALLBACK_CLIENT_CLOSED) {
			if (h->policy &&
			    !(h->policy->flags & LWSSSPOLF_OPPORTUNISTIC) &&
#if defined(LWS_WITH_SERVER)
			    !(h->info.flags & LWSSSINFLAGS_ACCEPTED) && /* not server */
#endif
			    !wsi->a.context->being_destroyed) {
				r = lws_ss_backoff(h);
				if (r != LWSSSSRET_OK)
					return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
				break;
			}

#if defined(LWS_WITH_SERVER)
			if (h->info.flags & LWSSSINFLAGS_ACCEPTED) {
				/*
				 * was an accepted client connection to
				 * our server, so the stream is over now
				 */
				lws_ss_destroy(&h);
				return 0;
			}
#endif

		}
		break;

	case LWS_CALLBACK_ESTABLISHED:
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		h->retry = 0;
		h->seqstate = SSSEQ_CONNECTED;
		lws_sul_cancel(&h->sul);
#if defined(LWS_WITH_SYS_METRICS)
		/*
		 * If any hanging caliper measurement, dump it, and free any tags
		 */
		lws_metrics_caliper_report_hist(h->cal_txn, (struct lws *)NULL);
#endif
		r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		break;

	case LWS_CALLBACK_RECEIVE:
	case LWS_CALLBACK_CLIENT_RECEIVE:
		// lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: read %d\n", (int)len);
		if (!h || !h->info.rx)
			return 0;
		if (lws_is_first_fragment(wsi))
			f |= LWSSS_FLAG_SOM;
		if (lws_is_final_fragment(wsi))
			f |= LWSSS_FLAG_EOM;
		// lws_frame_is_binary(wsi);

		h->subseq = 1;

		r = h->info.rx(ss_to_userobj(h), (const uint8_t *)in, len, f);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		return 0; /* don't passthru */

	case LWS_CALLBACK_SERVER_WRITEABLE:
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		// lwsl_notice("%s: %s: WRITEABLE\n", __func__, lws_ss_tag(h));
		if (!h || !h->info.tx)
			return 0;

		if (h->seqstate != SSSEQ_CONNECTED) {
			lwsl_warn("%s: seqstate %d\n", __func__, h->seqstate);
			break;
		}

		buflen = sizeof(buf) - LWS_PRE;
		r = h->info.tx(ss_to_userobj(h),  h->txord++, buf + LWS_PRE,
				  &buflen, &f);
		if (r == LWSSSSRET_TX_DONT_SEND)
			return 0;
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		f1 = lws_write_ws_flags(h->policy->u.http.u.ws.binary ?
					   LWS_WRITE_BINARY : LWS_WRITE_TEXT,
					!!(f & LWSSS_FLAG_SOM),
					!!(f & LWSSS_FLAG_EOM));

		n = lws_write(wsi, buf + LWS_PRE, buflen, (enum lws_write_protocol)f1);
		if (n < (int)buflen) {
			lwsl_info("%s: write failed %d %d\n", __func__,
					n, (int)buflen);

			return -1;
		}

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

const struct lws_protocols protocol_secstream_ws = {
	"lws-secstream-ws",
	secstream_ws,
	0, 0, 0, NULL, 0
};
/*
 * Munge connect info according to protocol-specific considerations... this
 * usually means interpreting aux in a protocol-specific way and using the
 * pieces at connection setup time, eg, http url pieces.
 *
 * len bytes of buf can be used for things with scope until after the actual
 * connect.
 *
 * For ws, protocol aux is <url path>;<ws subprotocol name>
 */

static int
secstream_connect_munge_ws(lws_ss_handle_t *h, char *buf, size_t len,
			   struct lws_client_connect_info *i,
			   union lws_ss_contemp *ct)
{
	const char *pbasis = h->policy->u.http.url;
	size_t used_in, used_out;
	lws_strexp_t exp;

	/* i.path on entry is used to override the policy urlpath if not "" */

	if (i->path[0])
		pbasis = i->path;

	if (!pbasis)
		return 0;

	if (h->policy->flags & LWSSSPOLF_HTTP_CACHE_COOKIES)
		i->ssl_connection |= LCCSCF_CACHE_COOKIES;

	if (h->policy->flags & LWSSSPOLF_PRIORITIZE_READS)
		i->ssl_connection |= LCCSCF_PRIORITIZE_READS;

	/* protocol aux is the path part ; ws subprotocol name */

	i->path = buf;
	buf[0] = '/';

	lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata, buf + 1, len - 1);

	if (lws_strexp_expand(&exp, pbasis, strlen(pbasis),
			      &used_in, &used_out) != LSTRX_DONE)
		return 1;

	i->protocol = h->policy->u.http.u.ws.subprotocol;

	lwsl_ss_info(h, "url %s, ws subprotocol %s", buf, i->protocol);

	return 0;
}

const struct ss_pcols ss_pcol_ws = {
	"ws",  "http/1.1",  &protocol_secstream_ws, secstream_connect_munge_ws, 0, 0
};
