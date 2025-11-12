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

/*
 * bitmap of control messages that are valid to receive for each http2 state
 */

static const uint16_t http2_rx_validity[] = {
	/* LWS_H2S_IDLE */
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY) |
//		(1 << LWS_H2_FRAME_TYPE_WINDOW_UPDATE)| /* ignore */
		(1 << LWS_H2_FRAME_TYPE_HEADERS) |
		(1 << LWS_H2_FRAME_TYPE_CONTINUATION),
	/* LWS_H2S_RESERVED_LOCAL */
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_RST_STREAM) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY) |
		(1 << LWS_H2_FRAME_TYPE_WINDOW_UPDATE),
	/* LWS_H2S_RESERVED_REMOTE */
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_HEADERS) |
		(1 << LWS_H2_FRAME_TYPE_CONTINUATION) |
		(1 << LWS_H2_FRAME_TYPE_RST_STREAM) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY),
	/* LWS_H2S_OPEN */
		(1 << LWS_H2_FRAME_TYPE_DATA) |
		(1 << LWS_H2_FRAME_TYPE_HEADERS) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY) |
		(1 << LWS_H2_FRAME_TYPE_RST_STREAM) |
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_PUSH_PROMISE) |
		(1 << LWS_H2_FRAME_TYPE_PING) |
		(1 << LWS_H2_FRAME_TYPE_GOAWAY) |
		(1 << LWS_H2_FRAME_TYPE_WINDOW_UPDATE) |
		(1 << LWS_H2_FRAME_TYPE_CONTINUATION),
	/* LWS_H2S_HALF_CLOSED_REMOTE */
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_WINDOW_UPDATE) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY) |
		(1 << LWS_H2_FRAME_TYPE_RST_STREAM),
	/* LWS_H2S_HALF_CLOSED_LOCAL */
		(1 << LWS_H2_FRAME_TYPE_DATA) |
		(1 << LWS_H2_FRAME_TYPE_HEADERS) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY) |
		(1 << LWS_H2_FRAME_TYPE_RST_STREAM) |
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_PUSH_PROMISE) |
		(1 << LWS_H2_FRAME_TYPE_PING) |
		(1 << LWS_H2_FRAME_TYPE_GOAWAY) |
		(1 << LWS_H2_FRAME_TYPE_WINDOW_UPDATE) |
		(1 << LWS_H2_FRAME_TYPE_CONTINUATION),
	/* LWS_H2S_CLOSED */
		(1 << LWS_H2_FRAME_TYPE_SETTINGS) |
		(1 << LWS_H2_FRAME_TYPE_PRIORITY) |
		(1 << LWS_H2_FRAME_TYPE_WINDOW_UPDATE) |
		(1 << LWS_H2_FRAME_TYPE_RST_STREAM),
};

static const char *preface = "PRI * HTTP/2.0\x0d\x0a\x0d\x0aSM\x0d\x0a\x0d\x0a";

static const char * const h2_state_names[] = {
	"LWS_H2S_IDLE",
	"LWS_H2S_RESERVED_LOCAL",
	"LWS_H2S_RESERVED_REMOTE",
	"LWS_H2S_OPEN",
	"LWS_H2S_HALF_CLOSED_REMOTE",
	"LWS_H2S_HALF_CLOSED_LOCAL",
	"LWS_H2S_CLOSED",
};

#if 0
static const char * const h2_setting_names[] = {
	"",
	"H2SET_HEADER_TABLE_SIZE",
	"H2SET_ENABLE_PUSH",
	"H2SET_MAX_CONCURRENT_STREAMS",
	"H2SET_INITIAL_WINDOW_SIZE",
	"H2SET_MAX_FRAME_SIZE",
	"H2SET_MAX_HEADER_LIST_SIZE",
	"reserved",
	"H2SET_ENABLE_CONNECT_PROTOCOL"
};

void
lws_h2_dump_settings(struct http2_settings *set)
{
	int n;

	for (n = 1; n < H2SET_COUNT; n++)
		lwsl_notice("   %30s: %10d\n", h2_setting_names[n], set->s[n]);
}
#else
void
lws_h2_dump_settings(struct http2_settings *set)
{
}
#endif

struct lws_h2_protocol_send *
lws_h2_new_pps(enum lws_h2_protocol_send_type type)
{
	struct lws_h2_protocol_send *pps = lws_malloc(sizeof(*pps), "pps");

	if (pps)
		pps->type = type;

	return pps;
}

void lws_h2_init(struct lws *wsi)
{
	wsi->h2.h2n->our_set = wsi->a.vhost->h2.set;
	wsi->h2.h2n->peer_set = lws_h2_defaults;
}

void
lws_h2_state(struct lws *wsi, enum lws_h2_states s)
{
	if (!wsi)
		return;
	lwsl_info("%s: %s: state %s -> %s\n", __func__, lws_wsi_tag(wsi),
			h2_state_names[wsi->h2.h2_state],
			h2_state_names[s]);
		
	(void)h2_state_names;
	wsi->h2.h2_state = (uint8_t)s;
}

int
lws_h2_update_peer_txcredit(struct lws *wsi, unsigned int sid, int bump)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	struct lws_h2_protocol_send *pps;

	assert(wsi);

	if (!bump)
		return 0;

	if (sid == (unsigned int)-1)
		sid = wsi->mux.my_sid;

	lwsl_info("%s: sid %d: bump %d -> %d\n", __func__, sid, bump,
			(int)wsi->txc.peer_tx_cr_est + bump);

	pps = lws_h2_new_pps(LWS_H2_PPS_UPDATE_WINDOW);
	if (!pps)
		return 1;

	pps->u.update_window.sid = (unsigned int)sid;
	pps->u.update_window.credit = (unsigned int)bump;
	wsi->txc.peer_tx_cr_est += bump;

	lws_wsi_txc_describe(&wsi->txc, __func__, wsi->mux.my_sid);

	lws_pps_schedule(wsi, pps);

	pps = lws_h2_new_pps(LWS_H2_PPS_UPDATE_WINDOW);
	if (!pps)
		return 1;

	pps->u.update_window.sid = 0;
	pps->u.update_window.credit = (unsigned int)bump;
	nwsi->txc.peer_tx_cr_est += bump;

	lws_wsi_txc_describe(&nwsi->txc, __func__, nwsi->mux.my_sid);

	lws_pps_schedule(nwsi, pps);

	return 0;
}

int
lws_h2_get_peer_txcredit_estimate(struct lws *wsi)
{
	lws_wsi_txc_describe(&wsi->txc, __func__, wsi->mux.my_sid);
	return (int)wsi->txc.peer_tx_cr_est;
}

static int
lws_h2_update_peer_txcredit_thresh(struct lws *wsi, unsigned int sid, int threshold, int bump)
{
	if (wsi->txc.peer_tx_cr_est > threshold)
		return 0;

	return lws_h2_update_peer_txcredit(wsi, sid, bump);
}

/* cx + vh lock */

static struct lws *
__lws_wsi_server_new(struct lws_vhost *vh, struct lws *parent_wsi,
		     unsigned int sid)
{
	struct lws *nwsi = lws_get_network_wsi(parent_wsi);
	struct lws_h2_netconn *h2n = nwsi->h2.h2n;
	char tmp[50], tmp1[50];
	unsigned int n, b = 0;
	struct lws *wsi;
	const char *p;

	lws_context_assert_lock_held(vh->context);
	lws_vhost_assert_lock_held(vh);

	/*
	 * The identifier of a newly established stream MUST be numerically
   	 * greater than all streams that the initiating endpoint has opened or
   	 * reserved.  This governs streams that are opened using a HEADERS frame
   	 * and streams that are reserved using PUSH_PROMISE.  An endpoint that
   	 * receives an unexpected stream identifier MUST respond with a
   	 * connection error (Section 5.4.1) of type PROTOCOL_ERROR.
	 */
	if (sid <= h2n->highest_sid_opened) {
		lwsl_info("%s: tried to open lower sid %d (%d)\n", __func__,
				sid, (int)h2n->highest_sid_opened);
		lws_h2_goaway(nwsi, H2_ERR_PROTOCOL_ERROR, "Bad sid");
		return NULL;
	}

	/* no more children allowed by parent */
	if (parent_wsi->mux.child_count + 1 >
	    parent_wsi->h2.h2n->our_set.s[H2SET_MAX_CONCURRENT_STREAMS]) {
		lwsl_notice("reached concurrent stream limit\n");
		return NULL;
	}

	n = 0;
	p = &parent_wsi->lc.gutag[1];
	do {
		if (*p == '|') {
			b++;
			if (b == 3)
				continue;
		}
		tmp1[n++] = *p++;
	} while (b < 3 && n < sizeof(tmp1) - 2);
	tmp1[n] = '\0';
	lws_snprintf(tmp, sizeof(tmp), "h2_sid%u_(%s)", sid, tmp1);
	wsi = lws_create_new_server_wsi(vh, parent_wsi->tsi, tmp);
	if (!wsi) {
		lwsl_notice("new server wsi failed (%s)\n", lws_vh_tag(vh));
		return NULL;
	}

#if defined(LWS_WITH_SERVER)
	if (lwsi_role_server(parent_wsi)) {
		lws_metrics_caliper_bind(wsi->cal_conn, wsi->a.context->mth_srv);
	}
#endif

	h2n->highest_sid_opened = sid;

	lws_wsi_mux_insert(wsi, parent_wsi, sid);
	if (sid >= h2n->highest_sid)
		h2n->highest_sid = sid + 2;

	wsi->mux_substream = 1;
	wsi->seen_nonpseudoheader = 0;

	wsi->txc.tx_cr = (int32_t)nwsi->h2.h2n->peer_set.s[H2SET_INITIAL_WINDOW_SIZE];
	wsi->txc.peer_tx_cr_est =
			(int32_t)nwsi->h2.h2n->our_set.s[H2SET_INITIAL_WINDOW_SIZE];

	lwsi_set_state(wsi, LRS_ESTABLISHED);
	lwsi_set_role(wsi, lwsi_role(parent_wsi));

	wsi->a.protocol = &vh->protocols[0];
	if (lws_ensure_user_space(wsi))
		goto bail1;

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SECURE_STREAMS)
	if (lws_adopt_ss_server_accept(wsi))
		goto bail1;
#endif

	/* get the ball rolling */
	lws_validity_confirmed(wsi);

	lwsl_info("%s: %s new ch %s, sid %d, usersp=%p\n", __func__,
		  lws_wsi_tag(parent_wsi), lws_wsi_tag(wsi), sid, wsi->user_space);

	lws_wsi_txc_describe(&wsi->txc, __func__, wsi->mux.my_sid);
	lws_wsi_txc_describe(&nwsi->txc, __func__, 0);

	return wsi;

bail1:
	/* undo the insert */
	parent_wsi->mux.child_list = wsi->mux.sibling_list;
	parent_wsi->mux.child_count--;

	if (wsi->user_space)
		lws_free_set_NULL(wsi->user_space);
	vh->protocols[0].callback(wsi, LWS_CALLBACK_WSI_DESTROY, NULL, NULL, 0);
	__lws_vhost_unbind_wsi(wsi);
	lws_free(wsi);

	return NULL;
}

struct lws *
lws_wsi_h2_adopt(struct lws *parent_wsi, struct lws *wsi)
{
	struct lws *nwsi = lws_get_network_wsi(parent_wsi);

	/* no more children allowed by parent */
	if (parent_wsi->mux.child_count + 1 >
	    parent_wsi->h2.h2n->our_set.s[H2SET_MAX_CONCURRENT_STREAMS]) {
		lwsl_notice("reached concurrent stream limit\n");
		return NULL;
	}

	/* sid is set just before issuing the headers, ensuring monoticity */

	wsi->seen_nonpseudoheader = 0;
#if defined(LWS_WITH_CLIENT)
	wsi->client_mux_substream = 1;
#endif
	wsi->h2.initialized = 1;

#if 0
	/* only assign sid at header send time when we know it */
	if (!wsi->mux.my_sid) {
		wsi->mux.my_sid = nwsi->h2.h2n->highest_sid;
		nwsi->h2.h2n->highest_sid += 2;
	}
#endif

	lwsl_info("%s: binding wsi %s to sid %d (next %d)\n", __func__,
		lws_wsi_tag(wsi), (int)wsi->mux.my_sid, (int)nwsi->h2.h2n->highest_sid);

	lws_wsi_mux_insert(wsi, parent_wsi, wsi->mux.my_sid);

	wsi->txc.tx_cr = (int32_t)nwsi->h2.h2n->peer_set.s[H2SET_INITIAL_WINDOW_SIZE];
	wsi->txc.peer_tx_cr_est = (int32_t)
			nwsi->h2.h2n->our_set.s[H2SET_INITIAL_WINDOW_SIZE];

	lws_wsi_txc_describe(&wsi->txc, __func__, wsi->mux.my_sid);

	if (lws_ensure_user_space(wsi))
		goto bail1;

	lws_role_transition(wsi, LWSIFR_CLIENT, LRS_H2_WAITING_TO_SEND_HEADERS,
			    &role_ops_h2);

	lws_callback_on_writable(wsi);

	return wsi;

bail1:
	/* undo the insert */
	parent_wsi->mux.child_list = wsi->mux.sibling_list;
	parent_wsi->mux.child_count--;

	if (wsi->user_space)
		lws_free_set_NULL(wsi->user_space);
	wsi->a.protocol->callback(wsi, LWS_CALLBACK_WSI_DESTROY, NULL, NULL, 0);
	lws_free(wsi);

	return NULL;
}


int
lws_h2_issue_preface(struct lws *wsi)
{
	struct lws_h2_netconn *h2n = wsi->h2.h2n;
	struct lws_h2_protocol_send *pps;

	if (!h2n) {
		lwsl_warn("%s: no valid h2n\n", __func__);
		return 1;
	}

	if (h2n->sent_preface)
		return 1;

	lwsl_debug("%s: %s: fd %d\n", __func__, lws_wsi_tag(wsi), (int)wsi->desc.sockfd);

	if (lws_issue_raw(wsi, (uint8_t *)preface, strlen(preface)) !=
		(int)strlen(preface))
		return 1;

	h2n->sent_preface = 1;

	lws_role_transition(wsi, LWSIFR_CLIENT, LRS_H2_WAITING_TO_SEND_HEADERS,
			    &role_ops_h2);

	h2n->count = 0;
	wsi->txc.tx_cr = 65535;

	/*
	 * we must send a settings frame
	 */
	pps = lws_h2_new_pps(LWS_H2_PPS_MY_SETTINGS);
	if (!pps)
		return 1;
	lws_pps_schedule(wsi, pps);
	lwsl_info("%s: h2 client sending settings\n", __func__);

	return 0;
}

void
lws_pps_schedule(struct lws *wsi, struct lws_h2_protocol_send *pps)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	struct lws_h2_netconn *h2n = nwsi->h2.h2n;

	if (!h2n) {
		lwsl_warn("%s: null h2n\n", __func__);
		lws_free(pps);
		return;
	}

	pps->next = h2n->pps;
	h2n->pps = pps;
	lws_rx_flow_control(wsi, LWS_RXFLOW_REASON_APPLIES_DISABLE |
				 LWS_RXFLOW_REASON_H2_PPS_PENDING);
	lws_callback_on_writable(wsi);
}

int
lws_h2_goaway(struct lws *wsi, uint32_t err, const char *reason)
{
	struct lws_h2_netconn *h2n = wsi->h2.h2n;
	struct lws_h2_protocol_send *pps;

	if (h2n->type == LWS_H2_FRAME_TYPE_COUNT)
		return 0;

	pps = lws_h2_new_pps(LWS_H2_PPS_GOAWAY);
	if (!pps)
		return 1;

	lwsl_info("%s: %s: ERR 0x%x, '%s'\n", __func__, lws_wsi_tag(wsi), (int)err, reason);

	pps->u.ga.err = err;
	pps->u.ga.highest_sid = h2n->highest_sid;
	lws_strncpy(pps->u.ga.str, reason, sizeof(pps->u.ga.str));
	lws_pps_schedule(wsi, pps);

	h2n->type = LWS_H2_FRAME_TYPE_COUNT; /* ie, IGNORE */

	return 0;
}

int
lws_h2_rst_stream(struct lws *wsi, uint32_t err, const char *reason)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	struct lws_h2_netconn *h2n = nwsi->h2.h2n;
	struct lws_h2_protocol_send *pps;

	if (!h2n)
		return 0;

	if (!wsi->h2_stream_carries_ws && h2n->type == LWS_H2_FRAME_TYPE_COUNT)
		return 0;

	pps = lws_h2_new_pps(LWS_H2_PPS_RST_STREAM);
	if (!pps)
		return 1;

	lwsl_info("%s: RST_STREAM 0x%x, sid %d, REASON '%s'\n", __func__,
		  (int)err, wsi->mux.my_sid, reason);

	pps->u.rs.sid = wsi->mux.my_sid;
	pps->u.rs.err = err;

	lws_pps_schedule(wsi, pps);

	h2n->type = LWS_H2_FRAME_TYPE_COUNT; /* ie, IGNORE */
	lws_h2_state(wsi, LWS_H2_STATE_CLOSED);

	return 0;
}

int
lws_h2_settings(struct lws *wsi, struct http2_settings *settings,
		unsigned char *buf, int len)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	unsigned int a, b;

	if (!len)
		return 0;

	if (len < LWS_H2_SETTINGS_LEN)
		return 1;

	while (len >= LWS_H2_SETTINGS_LEN) {
		a = (unsigned int)((buf[0] << 8) | buf[1]);
		if (!a || a >= H2SET_COUNT)
			goto skip;
		b = (unsigned int)(buf[2] << 24 | buf[3] << 16 | buf[4] << 8 | buf[5]);

		switch (a) {
		case H2SET_HEADER_TABLE_SIZE:
			break;
		case H2SET_ENABLE_PUSH:
			if (b > 1) {
				lws_h2_goaway(nwsi, H2_ERR_PROTOCOL_ERROR,
					      "ENABLE_PUSH invalid arg");
				return 1;
			}
			break;
		case H2SET_MAX_CONCURRENT_STREAMS:
			break;
		case H2SET_INITIAL_WINDOW_SIZE:
			if (b > 0x7fffffff) {
				lws_h2_goaway(nwsi, H2_ERR_FLOW_CONTROL_ERROR,
					      "Inital Window beyond max");
				return 1;
			}

#if defined(LWS_WITH_CLIENT)
#if defined(LWS_AMAZON_RTOS) || defined(LWS_AMAZON_LINUX)
			if (
#else
			if (wsi->flags & LCCSCF_H2_QUIRK_OVERFLOWS_TXCR &&
#endif
			    b == 0x7fffffff) {
				b >>= 4;

				break;
			}
#endif

			/*
			 * In addition to changing the flow-control window for
			 * streams that are not yet active, a SETTINGS frame
			 * can alter the initial flow-control window size for
			 * streams with active flow-control windows (that is,
			 * streams in the "open" or "half-closed (remote)"
			 * state).  When the value of
			 * SETTINGS_INITIAL_WINDOW_SIZE changes, a receiver
			 * MUST adjust the size of all stream flow-control
			 * windows that it maintains by the difference between
			 * the new value and the old value.
			 */

			lws_start_foreach_ll(struct lws *, w,
					     nwsi->mux.child_list) {
				lwsl_info("%s: adi child tc cr %d +%d -> %d",
					  __func__, (int)w->txc.tx_cr,
					  b - (unsigned int)settings->s[a],
					  (int)(w->txc.tx_cr + (int)b -
						  (int)settings->s[a]));
				w->txc.tx_cr += (int)b - (int)settings->s[a];
				if (w->txc.tx_cr > 0 &&
				    w->txc.tx_cr <=
						  (int32_t)(b - settings->s[a]))

					lws_callback_on_writable(w);
			} lws_end_foreach_ll(w, mux.sibling_list);

			break;
		case H2SET_MAX_FRAME_SIZE:
			if (b < wsi->a.vhost->h2.set.s[H2SET_MAX_FRAME_SIZE]) {
				lws_h2_goaway(nwsi, H2_ERR_PROTOCOL_ERROR,
					      "Frame size < initial");
				return 1;
			}
			if (b > 0x00ffffff) {
				lws_h2_goaway(nwsi, H2_ERR_PROTOCOL_ERROR,
					      "Settings Frame size above max");
				return 1;
			}
			break;
		case H2SET_MAX_HEADER_LIST_SIZE:
			break;
		}
		settings->s[a] = b;
		lwsl_info("http2 settings %d <- 0x%x\n", a, b);
skip:
		len -= LWS_H2_SETTINGS_LEN;
		buf += LWS_H2_SETTINGS_LEN;
	}

	if (len)
		return 1;

	lws_h2_dump_settings(settings);

	return 0;
}

/* RFC7640 Sect 6.9
 *
 * The WINDOW_UPDATE frame can be specific to a stream or to the entire
 * connection.  In the former case, the frame's stream identifier
 * indicates the affected stream; in the latter, the value "0" indicates
 * that the entire connection is the subject of the frame.
 *
 * ...
 *
 * Two flow-control windows are applicable: the stream flow-control
 * window and the connection flow-control window.  The sender MUST NOT
 * send a flow-controlled frame with a length that exceeds the space
 * available in either of the flow-control windows advertised by the
 * receiver.  Frames with zero length with the END_STREAM flag set (that
 * is, an empty DATA frame) MAY be sent if there is no available space
 * in either flow-control window.
 */

int
lws_h2_tx_cr_get(struct lws *wsi)
{
	int c = wsi->txc.tx_cr;
	struct lws *nwsi = lws_get_network_wsi(wsi);

	if (!wsi->mux_substream && !nwsi->upgraded_to_http2)
		return ~0x80000000;

	lwsl_info ("%s: %s: own tx credit %d: nwsi credit %d\n",
		     __func__, lws_wsi_tag(wsi), c, (int)nwsi->txc.tx_cr);

	if (nwsi->txc.tx_cr < c)
		c = nwsi->txc.tx_cr;

	if (c < 0)
		return 0;

	return c;
}

void
lws_h2_tx_cr_consume(struct lws *wsi, int consumed)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);

	wsi->txc.tx_cr -= consumed;

	if (nwsi != wsi)
		nwsi->txc.tx_cr -= consumed;
}

int lws_h2_frame_write(struct lws *wsi, int type, int flags,
		       unsigned int sid, unsigned int len, unsigned char *buf)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	unsigned char *p = &buf[-LWS_H2_FRAME_HEADER_LENGTH];
	int n;

	//if (wsi->h2_stream_carries_ws)
	// lwsl_hexdump_level(LLL_NOTICE, buf, len);

	*p++ = (uint8_t)(len >> 16);
	*p++ = (uint8_t)(len >> 8);
	*p++ = (uint8_t)len;
	*p++ = (uint8_t)type;
	*p++ = (uint8_t)flags;
	*p++ = (uint8_t)(sid >> 24);
	*p++ = (uint8_t)(sid >> 16);
	*p++ = (uint8_t)(sid >> 8);
	*p++ = (uint8_t)sid;

	lwsl_debug("%s: %s (eff %s). typ %d, fl 0x%x, sid=%d, len=%d, "
		   "txcr=%d, nwsi->txcr=%d\n", __func__, lws_wsi_tag(wsi),
		   lws_wsi_tag(nwsi), type, flags,
		   sid, len, (int)wsi->txc.tx_cr, (int)nwsi->txc.tx_cr);

	if (type == LWS_H2_FRAME_TYPE_DATA) {
		if (wsi->txc.tx_cr < (int)len)

			lwsl_info("%s: %s: sending payload len %d"
				 " but tx_cr only %d!\n", __func__,
				 lws_wsi_tag(wsi), len, (int)wsi->txc.tx_cr);
				lws_h2_tx_cr_consume(wsi, (int)len);
	}

	n = lws_issue_raw(nwsi, &buf[-LWS_H2_FRAME_HEADER_LENGTH],
			  len + LWS_H2_FRAME_HEADER_LENGTH);
	if (n < 0)
		return n;

	if (n >= LWS_H2_FRAME_HEADER_LENGTH)
		return n - LWS_H2_FRAME_HEADER_LENGTH;

	return n;
}

static void lws_h2_set_bin(struct lws *wsi, int n, unsigned char *buf)
{
	*buf++ = (uint8_t)(n >> 8);
	*buf++ = (uint8_t)n;
	*buf++ = (uint8_t)(wsi->h2.h2n->our_set.s[n] >> 24);
	*buf++ = (uint8_t)(wsi->h2.h2n->our_set.s[n] >> 16);
	*buf++ = (uint8_t)(wsi->h2.h2n->our_set.s[n] >> 8);
	*buf = (uint8_t)wsi->h2.h2n->our_set.s[n];
}

/* we get called on the network connection */

int lws_h2_do_pps_send(struct lws *wsi)
{
	struct lws_h2_netconn *h2n = wsi->h2.h2n;
	struct lws_h2_protocol_send *pps = NULL;
	struct lws *cwsi;
	uint8_t set[LWS_PRE + 64], *p = &set[LWS_PRE], *q;
	int n, m = 0, flags = 0;

	if (!h2n)
		return 1;

	/* get the oldest pps */

	lws_start_foreach_llp(struct lws_h2_protocol_send **, pps1, h2n->pps) {
		if ((*pps1)->next == NULL) { /* we are the oldest in the list */
			pps = *pps1; /* remove us from the list */
			*pps1 = NULL;
			continue;
		}
	} lws_end_foreach_llp(pps1, next);

	if (!pps)
		return 1;

	lwsl_info("%s: %s: %d\n", __func__, lws_wsi_tag(wsi), pps->type);

	switch (pps->type) {

	case LWS_H2_PPS_MY_SETTINGS:

		/*
		 * if any of our settings varies from h2 "default defaults"
		 * then we must inform the peer
		 */
		for (n = 1; n < H2SET_COUNT; n++)
			if (h2n->our_set.s[n] != lws_h2_defaults.s[n]) {
				lwsl_debug("sending SETTING %d 0x%x\n", n,
					   (unsigned int)
						   wsi->h2.h2n->our_set.s[n]);

				lws_h2_set_bin(wsi, n, &set[LWS_PRE + m]);
				m += (int)sizeof(h2n->one_setting);
			}
		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_SETTINGS,
				       flags, LWS_H2_STREAM_ID_MASTER, (unsigned int)m,
		     		       &set[LWS_PRE]);
		if (n != m) {
			lwsl_info("send %d %d\n", n, m);
			goto bail;
		}
		break;

	case LWS_H2_PPS_SETTINGS_INITIAL_UPDATE_WINDOW:
		q = &set[LWS_PRE];
		*q++ = (uint8_t)(H2SET_INITIAL_WINDOW_SIZE >> 8);
		*q++ = (uint8_t)(H2SET_INITIAL_WINDOW_SIZE);
		*q++ = (uint8_t)(pps->u.update_window.credit >> 24);
		*q++ = (uint8_t)(pps->u.update_window.credit >> 16);
		*q++ = (uint8_t)(pps->u.update_window.credit >> 8);
		*q = (uint8_t)(pps->u.update_window.credit);

		lwsl_debug("%s: resetting initial window to %d\n", __func__,
				(int)pps->u.update_window.credit);

		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_SETTINGS,
				       flags, LWS_H2_STREAM_ID_MASTER, 6,
		     		       &set[LWS_PRE]);
		if (n != 6) {
			lwsl_info("send %d %d\n", n, m);
			goto bail;
		}
		break;

	case LWS_H2_PPS_ACK_SETTINGS:
		/* send ack ... always empty */
		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_SETTINGS, 1,
				       LWS_H2_STREAM_ID_MASTER, 0,
				       &set[LWS_PRE]);
		if (n) {
			lwsl_err("%s: writing settings ack frame failed %d\n", __func__, n);
			goto bail;
		}
		wsi->h2_acked_settings = 0;
		/* this is the end of the preface dance then? */
		if (lwsi_state(wsi) == LRS_H2_AWAIT_SETTINGS) {
			lwsi_set_state(wsi, LRS_ESTABLISHED);
#if defined(LWS_WITH_FILE_OPS)
			wsi->http.fop_fd = NULL;
#endif
			if (lws_is_ssl(lws_get_network_wsi(wsi)))
				break;

			if (wsi->a.vhost->options &
				LWS_SERVER_OPTION_H2_PRIOR_KNOWLEDGE)
				break;

			/*
			 * we need to treat the headers from the upgrade as the
			 * first job.  So these need to get shifted to sid 1.
			 */

			lws_context_lock(wsi->a.context, "h2 mig");
			lws_vhost_lock(wsi->a.vhost);

			h2n->swsi = __lws_wsi_server_new(wsi->a.vhost, wsi, 1);

			lws_vhost_unlock(wsi->a.vhost);
			lws_context_unlock(wsi->a.context);

			if (!h2n->swsi)
				goto bail;

			/* pass on the initial headers to SID 1 */
			h2n->swsi->http.ah = wsi->http.ah;
			wsi->http.ah = NULL;

			lwsl_info("%s: inherited headers %p\n", __func__,
				  h2n->swsi->http.ah);
			h2n->swsi->txc.tx_cr = (int32_t)
				h2n->our_set.s[H2SET_INITIAL_WINDOW_SIZE];
			lwsl_info("initial tx credit on %s: %d\n",
				  lws_wsi_tag(h2n->swsi),
				  (int)h2n->swsi->txc.tx_cr);
			h2n->swsi->h2.initialized = 1;
			/* demanded by HTTP2 */
			h2n->swsi->h2.END_STREAM = 1;
			lwsl_info("servicing initial http request\n");

#if defined(LWS_WITH_SERVER)
			if (lws_http_action(h2n->swsi))
				goto bail;
#endif
			break;
		}
		break;

	/*
	 * h2 only has PING... ACK = 0 = ping, ACK = 1 = pong
	 */

	case LWS_H2_PPS_PING:
	case LWS_H2_PPS_PONG:
		if (pps->type == LWS_H2_PPS_PING)
			lwsl_info("sending PING\n");
		else {
			lwsl_info("sending PONG\n");
			flags = LWS_H2_FLAG_SETTINGS_ACK;
		}

		memcpy(&set[LWS_PRE], pps->u.ping.ping_payload, 8);
		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_PING, flags,
				       LWS_H2_STREAM_ID_MASTER, 8,
				       &set[LWS_PRE]);
		if (n != 8)
			goto bail;

		break;

	case LWS_H2_PPS_GOAWAY:
		lwsl_info("LWS_H2_PPS_GOAWAY\n");
		*p++ = (uint8_t)(pps->u.ga.highest_sid >> 24);
		*p++ = (uint8_t)(pps->u.ga.highest_sid >> 16);
		*p++ = (uint8_t)(pps->u.ga.highest_sid >> 8);
		*p++ = (uint8_t)(pps->u.ga.highest_sid);
		*p++ = (uint8_t)(pps->u.ga.err >> 24);
		*p++ = (uint8_t)(pps->u.ga.err >> 16);
		*p++ = (uint8_t)(pps->u.ga.err >> 8);
		*p++ = (uint8_t)(pps->u.ga.err);
		q = (unsigned char *)pps->u.ga.str;
		n = 0;
		while (*q && n++ < (int)sizeof(pps->u.ga.str))
			*p++ = *q++;
		h2n->we_told_goaway = 1;
		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_GOAWAY, 0,
				       LWS_H2_STREAM_ID_MASTER,
				       (unsigned int)lws_ptr_diff(p, &set[LWS_PRE]),
				       &set[LWS_PRE]);
		if (n != 4) {
			lwsl_info("send %d %d\n", n, m);
			goto bail;
		}
		goto bail;

	case LWS_H2_PPS_RST_STREAM:
		lwsl_info("LWS_H2_PPS_RST_STREAM\n");
		*p++ = (uint8_t)(pps->u.rs.err >> 24);
		*p++ = (uint8_t)(pps->u.rs.err >> 16);
		*p++ = (uint8_t)(pps->u.rs.err >> 8);
		*p++ = (uint8_t)(pps->u.rs.err);
		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_RST_STREAM,
				       0, pps->u.rs.sid, 4, &set[LWS_PRE]);
		if (n != 4) {
			lwsl_info("send %d %d\n", n, m);
			goto bail;
		}
		cwsi = lws_wsi_mux_from_id(wsi, pps->u.rs.sid);
		if (cwsi) {
			lwsl_debug("%s: closing cwsi %s %s %s (wsi %s)\n",
				   __func__, lws_wsi_tag(cwsi),
				   cwsi->role_ops->name,
				   cwsi->a.protocol->name, lws_wsi_tag(wsi));
			lws_close_free_wsi(cwsi, 0, "reset stream");
		}
		break;

	case LWS_H2_PPS_UPDATE_WINDOW:
		lwsl_info("Issuing LWS_H2_PPS_UPDATE_WINDOW: sid %d: add %d\n",
			    (int)pps->u.update_window.sid,
			    (int)pps->u.update_window.credit);
		*p++ = (uint8_t)((pps->u.update_window.credit >> 24) & 0x7f); /* 31b */
		*p++ = (uint8_t)(pps->u.update_window.credit >> 16);
		*p++ = (uint8_t)(pps->u.update_window.credit >> 8);
		*p++ = (uint8_t)(pps->u.update_window.credit);
		n = lws_h2_frame_write(wsi, LWS_H2_FRAME_TYPE_WINDOW_UPDATE,
				       0, pps->u.update_window.sid, 4,
				       &set[LWS_PRE]);
		if (n != 4) {
			lwsl_info("send %d %d\n", n, m);
			goto bail;
		}
		break;

	default:
		break;
	}

	lws_free(pps);

	return 0;

bail:
	lws_free(pps);

	return 1;
}

static int
lws_h2_parse_end_of_frame(struct lws *wsi);

/*
 * The frame header part has just completely arrived.
 * Perform actions for header completion.
 */
static int
lws_h2_parse_frame_header(struct lws *wsi)
{
	struct lws_h2_netconn *h2n = wsi->h2.h2n;
	struct lws_h2_protocol_send *pps;
	int n;

	/*
	 * We just got the frame header
	 */
	h2n->count = 0;
	h2n->swsi = wsi;
	/* b31 is a reserved bit */
	h2n->sid = h2n->sid & 0x7fffffff;

	if (h2n->sid && !(h2n->sid & 1)) {
		char pes[32];
		lws_snprintf(pes, sizeof(pes), "Even Stream ID 0x%x", (unsigned int)h2n->sid);
		lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR, pes);

		return 0;
	}

	/* let the network wsi live a bit longer if subs are active */

	if (!wsi->immortal_substream_count)
		lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE,
				wsi->a.vhost->keepalive_timeout ?
					wsi->a.vhost->keepalive_timeout : 31);

	if (h2n->sid)
		h2n->swsi = lws_wsi_mux_from_id(wsi, h2n->sid);

	lwsl_debug("%s (%s): fr hdr: typ 0x%x, fla 0x%x, sid 0x%x, len 0x%x\n",
		  lws_wsi_tag(wsi), lws_wsi_tag(h2n->swsi), h2n->type,
		  h2n->flags, (unsigned int)h2n->sid, (unsigned int)h2n->length);

	if (h2n->we_told_goaway && h2n->sid > h2n->highest_sid)
		h2n->type = LWS_H2_FRAME_TYPE_COUNT; /* ie, IGNORE */

	if (h2n->type >= LWS_H2_FRAME_TYPE_COUNT) {
		lwsl_info("%s: ignoring unknown frame type %d (len %d)\n", __func__, h2n->type, (unsigned int)h2n->length);
		/* we MUST ignore frames we don't understand */
		h2n->type = LWS_H2_FRAME_TYPE_COUNT;
	}

	/*
	 * Even if we have decided to logically ignore this frame, we must
	 * consume the correct "frame length" amount of data to retain sync
	 */

	if (h2n->length > h2n->our_set.s[H2SET_MAX_FRAME_SIZE]) {
		/*
		 * peer sent us something bigger than we told
		 * it we would allow
		 */
		lwsl_info("%s: received oversize frame %d\n", __func__,
			  (unsigned int)h2n->length);
		lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
			      "Peer ignored our frame size setting");
		return 1;
	}

	if (h2n->swsi)
		lwsl_info("%s: %s, State: %s, received cmd %d\n",
		  __func__, lws_wsi_tag(h2n->swsi),
		  h2_state_names[h2n->swsi->h2.h2_state], h2n->type);
	else {
		/* if it's data, either way no swsi means CLOSED state */
		if (h2n->type == LWS_H2_FRAME_TYPE_DATA) {
			if (h2n->sid <= h2n->highest_sid_opened
#if defined(LWS_WITH_CLIENT)
					&& wsi->client_h2_alpn
#endif
			) {
				lwsl_notice("ignoring straggling data fl 0x%x\n",
						h2n->flags);
				/* ie, IGNORE */
				h2n->type = LWS_H2_FRAME_TYPE_COUNT;
			} else {
				lwsl_info("%s: received %d bytes data for unknown sid %d, highest known %d\n",
						__func__, (int)h2n->length, (int)h2n->sid, (int)h2n->highest_sid_opened);

//				if (h2n->sid > h2n->highest_sid_opened) {
				lws_h2_goaway(wsi, H2_ERR_STREAM_CLOSED,
				      "Data for nonexistent sid");
				return 0;
//				}
			}
		}
		/* if the sid is credible, treat as wsi for it closed */
		if (h2n->sid > h2n->highest_sid_opened &&
		    h2n->type != LWS_H2_FRAME_TYPE_HEADERS &&
		    h2n->type != LWS_H2_FRAME_TYPE_PRIORITY) {
			/* if not credible, reject it */
			lwsl_info("%s: %s, No child for sid %d, rxcmd %d\n",
			  __func__, lws_wsi_tag(h2n->swsi), (unsigned int)h2n->sid, h2n->type);
			lws_h2_goaway(wsi, H2_ERR_STREAM_CLOSED,
				     "Data for nonexistent sid");
			return 0;
		}
	}

	if (h2n->swsi && h2n->sid && h2n->type != LWS_H2_FRAME_TYPE_COUNT &&
	    !(http2_rx_validity[h2n->swsi->h2.h2_state] & (1 << h2n->type))) {
		lwsl_info("%s: %s, State: %s, ILLEGAL cmdrx %d (OK 0x%x)\n",
			  __func__, lws_wsi_tag(h2n->swsi),
			  h2_state_names[h2n->swsi->h2.h2_state], h2n->type,
			  http2_rx_validity[h2n->swsi->h2.h2_state]);

		if (h2n->swsi->h2.h2_state == LWS_H2_STATE_CLOSED ||
		    h2n->swsi->h2.h2_state == LWS_H2_STATE_HALF_CLOSED_REMOTE)
			n = H2_ERR_STREAM_CLOSED;
		else
			n = H2_ERR_PROTOCOL_ERROR;
		lws_h2_goaway(wsi, (unsigned int)n, "invalid rx for state");

		return 0;
	}

	if (h2n->cont_exp && h2n->type != LWS_H2_FRAME_TYPE_COUNT &&
	    (h2n->cont_exp_sid != h2n->sid ||
			      h2n->type != LWS_H2_FRAME_TYPE_CONTINUATION)) {
		lwsl_info("%s: expected cont on sid %u (got %d on sid %u)\n",
			  __func__, (unsigned int)h2n->cont_exp_sid, h2n->type,
			  (unsigned int)h2n->sid);
		h2n->cont_exp = 0;
		if (h2n->cont_exp_headers)
			n = H2_ERR_COMPRESSION_ERROR;
		else
			n = H2_ERR_PROTOCOL_ERROR;
		lws_h2_goaway(wsi, (unsigned int)n, "Continuation hdrs State");

		return 0;
	}

	switch (h2n->type) {
	case LWS_H2_FRAME_TYPE_DATA:
		lwsl_info("seen incoming LWS_H2_FRAME_TYPE_DATA start\n");
		if (!h2n->sid) {
			lwsl_info("DATA: 0 sid\n");
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR, "DATA 0 sid");
			break;
		}
		lwsl_info("Frame header DATA: sid %u, flags 0x%x, len %u\n",
				(unsigned int)h2n->sid, h2n->flags,
				(unsigned int)h2n->length);

		if (!h2n->swsi) {
			lwsl_notice("DATA: NULL swsi\n");
			break;
		}

		lwsl_info("DATA rx on state %d\n", h2n->swsi->h2.h2_state);

		if (
		    h2n->swsi->h2.h2_state == LWS_H2_STATE_HALF_CLOSED_REMOTE ||
		    h2n->swsi->h2.h2_state == LWS_H2_STATE_CLOSED) {
			lws_h2_goaway(wsi, H2_ERR_STREAM_CLOSED, "conn closed");
			break;
		}

		if (h2n->length == 0)
			lws_h2_parse_end_of_frame(wsi);

		break;

	case LWS_H2_FRAME_TYPE_PRIORITY:
		lwsl_info("LWS_H2_FRAME_TYPE_PRIORITY complete frame\n");
		if (!h2n->sid) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "Priority has 0 sid");
			break;
		}
		if (h2n->length != 5) {
			lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
				      "Priority has length other than 5");
			break;
		}
		break;
	case LWS_H2_FRAME_TYPE_PUSH_PROMISE:
		lwsl_info("LWS_H2_FRAME_TYPE_PUSH_PROMISE complete frame\n");
		lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR, "Server only");
		break;

	case LWS_H2_FRAME_TYPE_GOAWAY:
		lwsl_debug("LWS_H2_FRAME_TYPE_GOAWAY received\n");
		break;

	case LWS_H2_FRAME_TYPE_RST_STREAM:
		if (!h2n->sid)
			return 1;
		if (!h2n->swsi) {
			if (h2n->sid <= h2n->highest_sid_opened)
				break;
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "crazy sid on RST_STREAM");
			return 1;
		}
		if (h2n->length != 4) {
			lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
				      "RST_STREAM can only be length 4");
			break;
		}
		lws_h2_state(h2n->swsi, LWS_H2_STATE_CLOSED);
		break;

	case LWS_H2_FRAME_TYPE_SETTINGS:
		lwsl_info("LWS_H2_FRAME_TYPE_SETTINGS complete frame\n");
		/* nonzero sid on settings is illegal */
		if (h2n->sid) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
					 "Settings has nonzero sid");
			break;
		}

		if (!(h2n->flags & LWS_H2_FLAG_SETTINGS_ACK)) {
			if (h2n->length % 6) {
				lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
						 "Settings length error");
				break;
			}

			if (h2n->type == LWS_H2_FRAME_TYPE_COUNT)
				return 0;

			if (wsi->upgraded_to_http2 &&
#if defined(LWS_WITH_CLIENT)
			    (!(wsi->flags & LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM) ||
#else
			    (
#endif
					    !wsi->h2_acked_settings)) {

				pps = lws_h2_new_pps(LWS_H2_PPS_ACK_SETTINGS);
				if (!pps)
					return 1;
				lws_pps_schedule(wsi, pps);
				wsi->h2_acked_settings = 1;
			}
			break;
		}
		/* came to us with ACK set... not allowed to have payload */

		if (h2n->length) {
			lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
				      "Settings with ACK not allowed payload");
			break;
		}
		break;
	case LWS_H2_FRAME_TYPE_PING:
		if (h2n->sid) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "Ping has nonzero sid");
			break;
		}
		if (h2n->length != 8) {
			lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
				      "Ping payload can only be 8");
			break;
		}
		break;
	case LWS_H2_FRAME_TYPE_CONTINUATION:
		lwsl_info("LWS_H2_FRAME_TYPE_CONTINUATION: sid = %u %d %d\n",
			  (unsigned int)h2n->sid, (int)h2n->cont_exp,
			  (int)h2n->cont_exp_sid);

		if (!h2n->cont_exp ||
		     h2n->cont_exp_sid != h2n->sid ||
		     !h2n->sid ||
		     !h2n->swsi) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "unexpected CONTINUATION");
			break;
		}

		if (h2n->swsi->h2.END_HEADERS) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "END_HEADERS already seen");
			break;
		}
		/* END_STREAM is in HEADERS, skip resetting it */
		goto update_end_headers;

	case LWS_H2_FRAME_TYPE_HEADERS:
		lwsl_info("HEADERS: frame header: sid = %u\n",
				(unsigned int)h2n->sid);
		if (!h2n->sid) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR, "sid 0");
			return 1;
		}

		if (h2n->swsi && !h2n->swsi->h2.END_STREAM &&
		    h2n->swsi->h2.END_HEADERS &&
		    !(h2n->flags & LWS_H2_FLAG_END_STREAM)) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "extra HEADERS together");
			return 1;
		}

#if defined(LWS_WITH_CLIENT)
		if (wsi->client_h2_alpn) {
			if (h2n->sid) {
				h2n->swsi = lws_wsi_mux_from_id(wsi, h2n->sid);
				lwsl_info("HEADERS: nwsi %s: sid %u mapped "
					  "to wsi %s\n", lws_wsi_tag(wsi),
					  (unsigned int)h2n->sid,
					  lws_wsi_tag(h2n->swsi));
				if (!h2n->swsi)
					break;
			}
			goto update_end_headers;
		}
#endif

		if (!h2n->swsi) {
			/* no more children allowed by parent */
			if (wsi->mux.child_count + 1 >
			    wsi->h2.h2n->our_set.s[H2SET_MAX_CONCURRENT_STREAMS]) {
				lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				"Another stream not allowed");

				return 1;
			}

			/*
			 * The peer has sent us a HEADERS implying the creation
			 * of a new stream
			 */

			lws_context_lock(wsi->a.context, "h2 new str");
			lws_vhost_lock(wsi->a.vhost);

			h2n->swsi = __lws_wsi_server_new(wsi->a.vhost, wsi,
						         h2n->sid);

			lws_vhost_unlock(wsi->a.vhost);
			lws_context_unlock(wsi->a.context);

			if (!h2n->swsi) {
				lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
					      "OOM");

				return 1;
			}

			if (h2n->sid >= h2n->highest_sid)
				h2n->highest_sid = h2n->sid + 2;

			h2n->swsi->h2.initialized = 1;

			if (lws_h2_update_peer_txcredit(h2n->swsi,
					h2n->swsi->mux.my_sid, 4 * 65536))
				goto cleanup_wsi;
		}

		/*
		 * ah needs attaching to child wsi, even though
		 * we only fill it from network wsi
		 */
		if (!h2n->swsi->http.ah)
			if (lws_header_table_attach(h2n->swsi, 0)) {
				lwsl_err("%s: Failed to get ah\n", __func__);
				return 1;
			}

		/*
		 * The first use of a new stream identifier implicitly closes
		 * all streams in the "idle" state that might have been
		 * initiated by that peer with a lower-valued stream identifier.
		 *
		 * For example, if a client sends a HEADERS frame on stream 7
		 * without ever sending a frame on stream 5, then stream 5
		 * transitions to the "closed" state when the first frame for
		 * stream 7 is sent or received.
		 */
		lws_start_foreach_ll(struct lws *, w, wsi->mux.child_list) {
			if (w->mux.my_sid < h2n->sid &&
			    w->h2.h2_state == LWS_H2_STATE_IDLE)
				lws_close_free_wsi(w, 0, "h2 sid close");
			assert(w->mux.sibling_list != w);
		} lws_end_foreach_ll(w, mux.sibling_list);

		h2n->cont_exp = !(h2n->flags & LWS_H2_FLAG_END_HEADERS);
		h2n->cont_exp_sid = h2n->sid;
		h2n->cont_exp_headers = 1;
	//	lws_header_table_reset(h2n->swsi, 0);

update_end_headers:
		if (lws_check_opt(h2n->swsi->a.vhost->options,
			       LWS_SERVER_OPTION_VH_H2_HALF_CLOSED_LONG_POLL)) {

			/*
			 * We don't directly timeout streams that enter the
			 * half-closed remote state, allowing immortal long
			 * poll
			 */
			lws_mux_mark_immortal(h2n->swsi);
			lwsl_info("%s: %s: h2 stream entering long poll\n",
					__func__, lws_wsi_tag(h2n->swsi));

		} else {
			h2n->swsi->h2.END_STREAM =
					!!(h2n->flags & LWS_H2_FLAG_END_STREAM);
			lwsl_debug("%s: hdr END_STREAM = %d\n",__func__,
			  h2n->swsi->h2.END_STREAM);
		}

		/* no END_HEADERS means CONTINUATION must come */
		h2n->swsi->h2.END_HEADERS =
				!!(h2n->flags & LWS_H2_FLAG_END_HEADERS);
		lwsl_info("%s: %s: END_HEADERS %d\n", __func__, lws_wsi_tag(h2n->swsi),
			  h2n->swsi->h2.END_HEADERS);
		if (h2n->swsi->h2.END_HEADERS)
			h2n->cont_exp = 0;
		lwsl_debug("END_HEADERS %d\n", h2n->swsi->h2.END_HEADERS);
		break;

cleanup_wsi:

		return 1;

	case LWS_H2_FRAME_TYPE_WINDOW_UPDATE:
		if (h2n->length != 4) {
			lws_h2_goaway(wsi, H2_ERR_FRAME_SIZE_ERROR,
				      "window update frame not 4");
			break;
		}
		lwsl_info("LWS_H2_FRAME_TYPE_WINDOW_UPDATE\n");
		break;
	case LWS_H2_FRAME_TYPE_COUNT:
		if (h2n->length == 0)
			lws_h2_parse_end_of_frame(wsi);
		else
			lwsl_debug("%s: going on to deal with unknown frame remaining len %d\n", __func__, (unsigned int)h2n->length);
		break;
	default:
		lwsl_info("%s: ILLEGAL FRAME TYPE %d\n", __func__, h2n->type);
		h2n->type = LWS_H2_FRAME_TYPE_COUNT; /* ie, IGNORE */
		break;
	}
	if (h2n->length == 0)
		h2n->frame_state = 0;

	return 0;
}

static const char * const method_names[] = {
	"GET", "POST",
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	"OPTIONS", "PUT", "PATCH", "DELETE",
#endif
	"CONNECT", "HEAD"
};
static unsigned char method_index[] = {
	WSI_TOKEN_GET_URI,
	WSI_TOKEN_POST_URI,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	WSI_TOKEN_OPTIONS_URI,
	WSI_TOKEN_PUT_URI,
	WSI_TOKEN_PATCH_URI,
	WSI_TOKEN_DELETE_URI,
#endif
	WSI_TOKEN_CONNECT,
	WSI_TOKEN_HEAD_URI,
};

/*
 * The last byte of the whole frame has been handled.
 * Perform actions for frame completion.
 *
 * This is the crunch time for parsing that may have occured on a network
 * wsi with a pending partial send... we may call lws_http_action() to send
 * a response, conflicting with the partial.
 *
 * So in that case we change the wsi state and do the lws_http_action() in the
 * WRITABLE handler as a priority.
 */
static int
lws_h2_parse_end_of_frame(struct lws *wsi)
{
	struct lws_h2_netconn *h2n = wsi->h2.h2n;
	struct lws *eff_wsi = wsi;
	const char *p;
	int n;

	h2n->frame_state = 0;
	h2n->count = 0;

	if (h2n->sid)
		h2n->swsi = lws_wsi_mux_from_id(wsi, h2n->sid);

	if (h2n->sid > h2n->highest_sid)
		h2n->highest_sid = h2n->sid;

	if (h2n->collected_priority && (h2n->dep & ~(1u << 31)) == h2n->sid) {
		lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR, "depends on own sid");
		return 0;
	}

	switch (h2n->type) {

	case LWS_H2_FRAME_TYPE_SETTINGS:

#if defined(LWS_WITH_CLIENT)
		if (wsi->client_h2_alpn && !wsi->client_mux_migrated &&
		    !(h2n->flags & LWS_H2_FLAG_SETTINGS_ACK)) {
			struct lws_h2_protocol_send *pps;

			/* migrate original client ask on to substream 1 */
#if defined(LWS_WITH_FILE_OPS)
			wsi->http.fop_fd = NULL;
#endif
			lwsl_info("%s: migrating\n", __func__);
			wsi->client_mux_migrated = 1;
			/*
			 * we need to treat the headers from the upgrade as the
			 * first job.  So these need to get shifted to sid 1.
			 */
			lws_context_lock(wsi->a.context, "h2 mig");
			lws_vhost_lock(wsi->a.vhost);

			h2n->swsi = __lws_wsi_server_new(wsi->a.vhost, wsi, 1);

			lws_vhost_unlock(wsi->a.vhost);
			lws_context_unlock(wsi->a.context);

			if (!h2n->swsi)
				return 1;
			h2n->sid = 1;

			assert(lws_wsi_mux_from_id(wsi, 1) == h2n->swsi);

		//	lws_role_transition(wsi, LWSIFR_CLIENT,
		//			    LRS_H2_WAITING_TO_SEND_HEADERS,
		//			    &role_ops_h2);

			lws_role_transition(h2n->swsi, LWSIFR_CLIENT,
					    LRS_H2_WAITING_TO_SEND_HEADERS,
					    &role_ops_h2);

			/* pass on the initial headers to SID 1 */
			h2n->swsi->http.ah = wsi->http.ah;
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
			lws_fi_import(&h2n->swsi->fic, &wsi->fic);
#endif
			h2n->swsi->client_mux_substream = 1;
			h2n->swsi->client_h2_alpn = 1;
#if defined(LWS_WITH_CLIENT)
			h2n->swsi->flags = wsi->flags;
#if defined(LWS_WITH_CONMON)
			/* sid1 needs to represent the connection experience
			 * ... we take over responsibility for the DNS list
			 * copy as well
			 */
			h2n->swsi->conmon = wsi->conmon;
			h2n->swsi->conmon_datum = wsi->conmon_datum;
			h2n->swsi->sa46_peer = wsi->sa46_peer;
			wsi->conmon.dns_results_copy = NULL;
#endif
#endif /* CLIENT */

#if defined(LWS_WITH_SECURE_STREAMS)
			if (wsi->for_ss) {
				lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);

				h2n->swsi->for_ss = 1;
				wsi->for_ss = 0;

				if (h->wsi == wsi)
					h->wsi = h2n->swsi;
			}
#endif

			h2n->swsi->a.protocol = wsi->a.protocol;
			if (h2n->swsi->user_space &&
			    !h2n->swsi->user_space_externally_allocated)
				lws_free(h2n->swsi->user_space);
			h2n->swsi->user_space = wsi->user_space;
			h2n->swsi->user_space_externally_allocated =
					wsi->user_space_externally_allocated;
			h2n->swsi->a.opaque_user_data = wsi->a.opaque_user_data;
			wsi->a.opaque_user_data = NULL;
			h2n->swsi->txc.manual_initial_tx_credit =
					wsi->txc.manual_initial_tx_credit;

#if defined(LWS_WITH_TLS)
			lws_strncpy(h2n->swsi->alpn, wsi->alpn,
					sizeof(wsi->alpn));
#endif

			wsi->user_space = NULL;

			if (h2n->swsi->http.ah)
				h2n->swsi->http.ah->wsi = h2n->swsi;
			wsi->http.ah = NULL;

			lwsl_info("%s: MIGRATING nwsi %s -> swsi %s\n", __func__,
				  lws_wsi_tag(wsi), lws_wsi_tag(h2n->swsi));
			h2n->swsi->txc.tx_cr = (int32_t)
				h2n->peer_set.s[H2SET_INITIAL_WINDOW_SIZE];
			lwsl_info("%s: initial tx credit on %s: %d\n",
				  __func__, lws_wsi_tag(h2n->swsi),
				  (int)h2n->swsi->txc.tx_cr);
			h2n->swsi->h2.initialized = 1;

			/* set our initial window size */
			if (!wsi->h2.initialized) {
				wsi->txc.tx_cr = (int32_t)
				     h2n->peer_set.s[H2SET_INITIAL_WINDOW_SIZE];

				lwsl_info("%s: initial tx credit for us to "
					  "write on nwsi %s: %d\n", __func__,
					  lws_wsi_tag(wsi), (int)wsi->txc.tx_cr);
				wsi->h2.initialized = 1;
			}

			lws_callback_on_writable(h2n->swsi);

			if (!wsi->h2_acked_settings ||
			    !(wsi->flags & LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM)
			) {
				pps = lws_h2_new_pps(LWS_H2_PPS_ACK_SETTINGS);
				if (!pps)
					return 1;
				lws_pps_schedule(wsi, pps);
				lwsl_info("%s: SETTINGS ack PPS\n", __func__);
				wsi->h2_acked_settings = 1;
			}

			/* also attach any queued guys */

			lws_wsi_mux_apply_queue(wsi);
		}
#endif
		break;

	case LWS_H2_FRAME_TYPE_CONTINUATION:
	case LWS_H2_FRAME_TYPE_HEADERS:

		if (!h2n->swsi)
			break;

		/* service the http request itself */

		if (h2n->last_action_dyntable_resize) {
			lws_h2_goaway(wsi, H2_ERR_COMPRESSION_ERROR,
				"dyntable resize last in headers");
			break;
		}

		if (!h2n->swsi->h2.END_HEADERS) {
			/* we are not finished yet */
			lwsl_info("witholding http action for continuation\n");
			h2n->cont_exp_sid = h2n->sid;
			h2n->cont_exp = 1;
			break;
		}

		/* confirm the hpack stream state is reasonable for finishing */

		if (h2n->hpack != HPKS_TYPE) {
			/* hpack incomplete */
			lwsl_info("hpack incomplete %d (type %d, len %u)\n",
				  h2n->hpack, h2n->type,
				  (unsigned int)h2n->hpack_len);
			lws_h2_goaway(wsi, H2_ERR_COMPRESSION_ERROR,
				      "hpack incomplete");
			break;
		}

		/* this is the last part of HEADERS */
		switch (h2n->swsi->h2.h2_state) {
		case LWS_H2_STATE_IDLE:
			lws_h2_state(h2n->swsi, LWS_H2_STATE_OPEN);
			break;
		case LWS_H2_STATE_RESERVED_REMOTE:
			lws_h2_state(h2n->swsi, LWS_H2_STATE_HALF_CLOSED_LOCAL);
			break;
		}

		lwsl_info("http req, %s, h2n->swsi=%s\n", lws_wsi_tag(wsi),
				lws_wsi_tag(h2n->swsi));
		h2n->swsi->hdr_parsing_completed = 1;

#if defined(LWS_WITH_CLIENT)
		if (h2n->swsi->client_mux_substream &&
		    lws_client_interpret_server_handshake(h2n->swsi)) {
			/*
			 * This is more complicated than it looks, one exit from
			 * interpret_server_handshake() is to do a close that
			 * turns into a redirect.
			 *
			 * In that case, the wsi survives having being reset
			 * and detached from any h2 identity.  We need to get
			 * our parents out from touching it any more
			 */
			lwsl_info("%s: cli int serv hs closed, or redir\n", __func__);
			return 2;
		}
#endif

		if (lws_hdr_extant(h2n->swsi, WSI_TOKEN_HTTP_CONTENT_LENGTH)) {
			const char *simp = lws_hdr_simple_ptr(h2n->swsi,
					      WSI_TOKEN_HTTP_CONTENT_LENGTH);

			if (!simp) /* coverity */
				return 1;
			h2n->swsi->http.rx_content_length = (unsigned long long)atoll(simp);
			h2n->swsi->http.rx_content_remain =
					h2n->swsi->http.rx_content_length;
			h2n->swsi->http.content_length_given = 1;
			lwsl_info("setting rx_content_length %lld\n",
				  (long long)h2n->swsi->http.rx_content_length);
		}

		{
			int n = 0, len;
			char buf[256];
			const unsigned char *c;

			do {
				c = lws_token_to_string((enum lws_token_indexes)n);
				if (!c) {
					n++;
					continue;
				}

				len = lws_hdr_total_length(h2n->swsi, (enum lws_token_indexes)n);
				if (!len || len > (int)sizeof(buf) - 1) {
					n++;
					continue;
				}

				if (lws_hdr_copy(h2n->swsi, buf, sizeof buf,
						(enum lws_token_indexes)n) < 0) {
					lwsl_info("    %s !oversize!\n",
						  (char *)c);
				} else {
					buf[sizeof(buf) - 1] = '\0';

					lwsl_info("    %s = %s\n",
						  (char *)c, buf);
				}
				n++;
			} while (c);
		}

		if (h2n->swsi->h2.h2_state == LWS_H2_STATE_HALF_CLOSED_REMOTE ||
		    h2n->swsi->h2.h2_state == LWS_H2_STATE_CLOSED) {
			lws_h2_goaway(wsi, H2_ERR_STREAM_CLOSED,
				      "Banning service on CLOSED_REMOTE");
			break;
		}

		switch (h2n->swsi->h2.h2_state) {
		case LWS_H2_STATE_IDLE:
			lws_h2_state(h2n->swsi, LWS_H2_STATE_OPEN);
			break;
		case LWS_H2_STATE_OPEN:
			if (h2n->swsi->h2.END_STREAM)
				lws_h2_state(h2n->swsi,
					     LWS_H2_STATE_HALF_CLOSED_REMOTE);
			break;
		case LWS_H2_STATE_HALF_CLOSED_LOCAL:
			if (h2n->swsi->h2.END_STREAM)
				/*
				 * action the END_STREAM
				 */
				lws_h2_state(h2n->swsi, LWS_H2_STATE_CLOSED);
			break;
		}

#if defined(LWS_WITH_CLIENT)

		/*
		 * If we already had the END_STREAM along with the END_HEADERS,
		 * we have already transitioned to STATE_CLOSED and we are not
		 * going to be doing anything further on this stream.
		 *
		 * In that case handle the transaction completion and
		 * finalize the stream for the peer
		 */

		if (h2n->swsi->h2.h2_state == LWS_H2_STATE_CLOSED &&
			h2n->swsi->client_mux_substream) {

			lws_h2_rst_stream(h2n->swsi, H2_ERR_NO_ERROR,
				"client done");

			if (lws_http_transaction_completed_client(h2n->swsi))
				lwsl_debug("tx completed returned close\n");
			break;
		}

		if (h2n->swsi->client_mux_substream) {
			lwsl_info("%s: %s: headers: client path (h2 state %s)\n",
				  __func__, lws_wsi_tag(wsi),
				  h2_state_names[h2n->swsi->h2.h2_state]);
			break;
		}
#endif

		if (!lws_hdr_total_length(h2n->swsi, WSI_TOKEN_HTTP_COLON_PATH) ||
		    !lws_hdr_total_length(h2n->swsi, WSI_TOKEN_HTTP_COLON_METHOD) ||
		    !lws_hdr_total_length(h2n->swsi, WSI_TOKEN_HTTP_COLON_SCHEME) ||
		     lws_hdr_total_length(h2n->swsi, WSI_TOKEN_HTTP_COLON_STATUS) ||
		     lws_hdr_extant(h2n->swsi, WSI_TOKEN_CONNECTION)) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "Pseudoheader checks");
			break;
		}

		if (lws_hdr_extant(h2n->swsi, WSI_TOKEN_TE)) {
			n = lws_hdr_total_length(h2n->swsi, WSI_TOKEN_TE);

			if (n != 8 ||
			    !lws_hdr_simple_ptr(h2n->swsi, WSI_TOKEN_TE) ||
			    strncmp(lws_hdr_simple_ptr(h2n->swsi, WSI_TOKEN_TE),
				  "trailers", (unsigned int)n)) {
				lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
					      "Illegal transfer-encoding");
				break;
			}
		}

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
		lws_http_compression_validate(h2n->swsi);
#endif

		p = lws_hdr_simple_ptr(h2n->swsi, WSI_TOKEN_HTTP_COLON_METHOD);
		/*
		 * duplicate :path into the individual method uri header
		 * index, so that it looks the same as h1 in the ah
		 */
		for (n = 0; n < (int)LWS_ARRAY_SIZE(method_names); n++)
			if (p && !strcasecmp(p, method_names[n])) {
				h2n->swsi->http.ah->frag_index[method_index[n]] =
						h2n->swsi->http.ah->frag_index[
				                     WSI_TOKEN_HTTP_COLON_PATH];
				break;
			}

		{
			lwsl_debug("%s: setting DEF_ACT from 0x%x\n", __func__,
				   (unsigned int)h2n->swsi->wsistate);
			lwsi_set_state(h2n->swsi, LRS_DEFERRING_ACTION);
			lws_callback_on_writable(h2n->swsi);
		}
		break;

	case LWS_H2_FRAME_TYPE_DATA:
		lwsl_info("%s: DATA flags 0x%x\n", __func__, h2n->flags);
		if (!h2n->swsi)
			break;

		if (lws_hdr_total_length(h2n->swsi,
					 WSI_TOKEN_HTTP_CONTENT_LENGTH) &&
		    h2n->swsi->h2.END_STREAM &&
		    h2n->swsi->http.rx_content_length &&
		    h2n->swsi->http.rx_content_remain) {
			lws_h2_rst_stream(h2n->swsi, H2_ERR_PROTOCOL_ERROR,
					  "Not enough rx content");
			break;
		}

		if (h2n->swsi->h2.END_STREAM &&
		    h2n->swsi->h2.h2_state == LWS_H2_STATE_OPEN)
			lws_h2_state(h2n->swsi,
				     LWS_H2_STATE_HALF_CLOSED_REMOTE);

		if (h2n->swsi->h2.END_STREAM &&
		    h2n->swsi->h2.h2_state == LWS_H2_STATE_HALF_CLOSED_LOCAL)
			lws_h2_state(h2n->swsi, LWS_H2_STATE_CLOSED);

#if defined(LWS_WITH_CLIENT)
		/*
		 * client... remote END_STREAM implies we weren't going to
		 * send anything else anyway.
		 */

		if (h2n->swsi->client_mux_substream &&
		    (h2n->flags & LWS_H2_FLAG_END_STREAM)) {
			lwsl_info("%s: %s: DATA: end stream\n",
				  __func__, lws_wsi_tag(h2n->swsi));

			if (h2n->swsi->h2.h2_state == LWS_H2_STATE_OPEN) {
				lws_h2_state(h2n->swsi,
					     LWS_H2_STATE_HALF_CLOSED_REMOTE);
		//		lws_h2_rst_stream(h2n->swsi, H2_ERR_NO_ERROR,
		//				  "client done");

		//		if (lws_http_transaction_completed_client(h2n->swsi))
		//			lwsl_debug("tx completed returned close\n");
			}

			//if (h2n->swsi->h2.h2_state == LWS_H2_STATE_HALF_CLOSED_LOCAL)
			{
				lws_h2_state(h2n->swsi, LWS_H2_STATE_CLOSED);

				lws_h2_rst_stream(h2n->swsi, H2_ERR_NO_ERROR,
						  "client done");

				if (lws_http_transaction_completed_client(h2n->swsi))
					lwsl_debug("tx completed returned close\n");
			}
		}
#endif
		break;

	case LWS_H2_FRAME_TYPE_PING:
		if (h2n->flags & LWS_H2_FLAG_SETTINGS_ACK)
			lws_validity_confirmed(wsi);
		else {
			/* they're sending us a ping request */
			struct lws_h2_protocol_send *pps =
					lws_h2_new_pps(LWS_H2_PPS_PONG);
			if (!pps)
				return 1;

			lwsl_info("rx ping, preparing pong\n");

			memcpy(pps->u.ping.ping_payload, h2n->ping_payload, 8);
			lws_pps_schedule(wsi, pps);
		}

		break;

	case LWS_H2_FRAME_TYPE_WINDOW_UPDATE:
		/*
		 * We only have an unsigned 31-bit (positive) increment possible
		 */
		h2n->hpack_e_dep &= ~(1u << 31);
		lwsl_info("WINDOW_UPDATE: sid %u %u (0x%x)\n",
			  (unsigned int)h2n->sid,
			  (unsigned int)h2n->hpack_e_dep,
			  (unsigned int)h2n->hpack_e_dep);

		if (h2n->sid)
			eff_wsi = h2n->swsi;

		if (!eff_wsi) {
			if (h2n->sid > h2n->highest_sid_opened)
				lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
					      "alien sid");
			break; /* ignore */
		}

		if (eff_wsi->a.vhost->options &
		        LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW &&
		    (uint64_t)eff_wsi->txc.tx_cr + (uint64_t)h2n->hpack_e_dep >
		    (uint64_t)0x7fffffff)
			h2n->hpack_e_dep = (uint32_t)(0x7fffffff - eff_wsi->txc.tx_cr);

		if ((uint64_t)eff_wsi->txc.tx_cr + (uint64_t)h2n->hpack_e_dep >
		    (uint64_t)0x7fffffff) {
			lwsl_warn("%s: WINDOW_UPDATE 0x%llx + 0x%llx = 0x%llx, too high\n",
					__func__, (unsigned long long)eff_wsi->txc.tx_cr,
					(unsigned long long)h2n->hpack_e_dep,
					(unsigned long long)eff_wsi->txc.tx_cr + (unsigned long long)h2n->hpack_e_dep);
			if (h2n->sid)
				lws_h2_rst_stream(h2n->swsi,
						  H2_ERR_FLOW_CONTROL_ERROR,
						  "Flow control exceeded max");
			else
				lws_h2_goaway(wsi, H2_ERR_FLOW_CONTROL_ERROR,
					      "Flow control exceeded max");
			break;
		}

		if (!h2n->hpack_e_dep) {
			lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
				      "Zero length window update");
			break;
		}
		n = eff_wsi->txc.tx_cr;
		eff_wsi->txc.tx_cr += (int32_t)h2n->hpack_e_dep;

		lws_wsi_txc_report_manual_txcr_in(eff_wsi,
						  (int32_t)h2n->hpack_e_dep);

		lws_wsi_txc_describe(&eff_wsi->txc, "WINDOW_UPDATE in",
				     eff_wsi->mux.my_sid);

		if (n <= 0 && eff_wsi->txc.tx_cr <= 0)
			/* it helps, but won't change sendability for anyone */
			break;

		/*
		 * It may have changed sendability (depends on SID 0 tx credit
		 * too)... for us and any children waiting on us... reassess
		 * blockage for all children first
		 */
		lws_start_foreach_ll(struct lws *, w, wsi->mux.child_list) {
			lws_callback_on_writable(w);
		} lws_end_foreach_ll(w, mux.sibling_list);

		if (eff_wsi->txc.skint &&
		    !lws_wsi_txc_check_skint(&eff_wsi->txc,
					     lws_h2_tx_cr_get(eff_wsi)))
			/*
			 * This one became un-skint, schedule a writeable
			 * callback
			 */
			lws_callback_on_writable(eff_wsi);

		break;

	case LWS_H2_FRAME_TYPE_GOAWAY:
		lwsl_notice("GOAWAY: last sid %u, error 0x%08X, string '%s'\n",
			  (unsigned int)h2n->goaway_last_sid,
			  (unsigned int)h2n->goaway_err, h2n->goaway_str);

		return 1;

	case LWS_H2_FRAME_TYPE_RST_STREAM:
		lwsl_info("LWS_H2_FRAME_TYPE_RST_STREAM: sid %u: reason 0x%x\n",
			  (unsigned int)h2n->sid,
			  (unsigned int)h2n->hpack_e_dep);
		break;

	case LWS_H2_FRAME_TYPE_COUNT: /* IGNORING FRAME */
		break;
	}

	return 0;
}

/*
 * This may want to send something on the network wsi, which may be in the
 * middle of a partial send.  PPS sends are OK because they are queued to
 * go through the WRITABLE handler already.
 *
 * The read parser for the network wsi has no choice but to parse its stream
 * anyway, because otherwise it will not be able to get tx credit window
 * messages.
 *
 * Therefore if we will send non-PPS, ie, lws_http_action() for a stream
 * wsi, we must change its state and handle it as a priority in the
 * POLLOUT handler instead of writing it here.
 *
 * About closing... for the main network wsi, it should return nonzero to
 * close it all.  If it needs to close an swsi, it can do it here.
 */
int
lws_h2_parser(struct lws *wsi, unsigned char *in, lws_filepos_t _inlen,
	      lws_filepos_t *inused)
{
	struct lws_h2_netconn *h2n = wsi->h2.h2n;
	struct lws_h2_protocol_send *pps;
	unsigned char c, *oldin = in, *iend = in + (size_t)_inlen;
	int n, m;

	if (!h2n)
		goto fail;

	while (in < iend) {

		c = *in++;

		switch (lwsi_state(wsi)) {
		case LRS_H2_AWAIT_PREFACE:
			if (preface[h2n->count++] != c)
				goto fail;

			if (preface[h2n->count])
				break;

			lwsl_info("http2: %s: established\n", lws_wsi_tag(wsi));
			lwsi_set_state(wsi, LRS_H2_AWAIT_SETTINGS);
			lws_validity_confirmed(wsi);
			h2n->count = 0;
			wsi->txc.tx_cr = 65535;

			/*
			 * we must send a settings frame -- empty one is OK...
			 * that must be the first thing sent by server
			 * and the peer must send a SETTINGS with ACK flag...
			 */
			pps = lws_h2_new_pps(LWS_H2_PPS_MY_SETTINGS);
			if (!pps)
				goto fail;
			lws_pps_schedule(wsi, pps);
			break;

		case LRS_H2_WAITING_TO_SEND_HEADERS:
		case LRS_ESTABLISHED:
		case LRS_H2_AWAIT_SETTINGS:

			if (h2n->frame_state != LWS_H2_FRAME_HEADER_LENGTH)
				goto try_frame_start;

			/*
			 * post-header, preamble / payload / padding part
			 */
			h2n->count++;

			if (h2n->type == LWS_H2_FRAME_TYPE_COUNT) { /* IGNORING FRAME */
				//lwsl_debug("%s: consuming for ignored %u %u\n", __func__, (unsigned int)h2n->count, (unsigned int)h2n->length);
				goto frame_end;
			}


			if (h2n->flags & LWS_H2_FLAG_PADDED &&
			    !h2n->pad_length) {
				/*
				 * Get the padding count... actual padding is
				 * at the end of the frame.
				 */
				h2n->padding = c;
				h2n->pad_length = 1;
				h2n->preamble++;

				if (h2n->padding > h2n->length - 1)
					lws_h2_goaway(wsi,
						      H2_ERR_PROTOCOL_ERROR,
						      "execssive padding");
				break; /* we consumed this */
			}

			if (h2n->flags & LWS_H2_FLAG_PRIORITY &&
			    !h2n->collected_priority) {
				/* going to be 5 preamble bytes */

				lwsl_debug("PRIORITY FLAG:  0x%x\n", c);

				if (h2n->preamble++ - h2n->pad_length < 4) {
					h2n->dep = ((h2n->dep) << 8) | c;
					break; /* we consumed this */
				}
				h2n->weight_temp = c;
				h2n->collected_priority = 1;
				lwsl_debug("PRI FL: dep 0x%x, weight 0x%02X\n",
					   (unsigned int)h2n->dep,
					   h2n->weight_temp);
				break; /* we consumed this */
			}
			if (h2n->padding && h2n->count >
			    (h2n->length - h2n->padding)) {
				if (c) {
					lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
						      "nonzero padding");
					break;
				}
				goto frame_end;
			}

			/* applies to wsi->h2.swsi which may be wsi */
			switch(h2n->type) {

			case LWS_H2_FRAME_TYPE_SETTINGS:
				n = (int)(h2n->count - 1u - h2n->preamble) %
				     LWS_H2_SETTINGS_LEN;
				h2n->one_setting[n] = c;
				if (n != LWS_H2_SETTINGS_LEN - 1)
					break;
				lws_h2_settings(wsi, &h2n->peer_set,
						h2n->one_setting,
						LWS_H2_SETTINGS_LEN);
				break;

			case LWS_H2_FRAME_TYPE_CONTINUATION:
			case LWS_H2_FRAME_TYPE_HEADERS:
				if (!h2n->swsi)
					break;
				if (lws_hpack_interpret(h2n->swsi, c)) {
					lwsl_info("%s: hpack failed\n",
						  __func__);
					goto fail;
				}
				break;

			case LWS_H2_FRAME_TYPE_GOAWAY:
				switch (h2n->inside++) {
				case 0:
				case 1:
				case 2:
				case 3:
					h2n->goaway_last_sid <<= 8;
					h2n->goaway_last_sid |= c;
					h2n->goaway_str[0] = '\0';
					break;

				case 4:
				case 5:
				case 6:
				case 7:
					h2n->goaway_err <<= 8;
					h2n->goaway_err |= c;
					break;

				default:
					if (h2n->inside - 9 <
					    sizeof(h2n->goaway_str) - 1)
						h2n->goaway_str[
						           h2n->inside - 9] = (char)c;
					h2n->goaway_str[
					    sizeof(h2n->goaway_str) - 1] = '\0';
					break;
				}
				break;

			case LWS_H2_FRAME_TYPE_DATA:

			//	lwsl_info("%s: LWS_H2_FRAME_TYPE_DATA: fl 0x%x\n",
			//		  __func__, h2n->flags);

				/*
				 * let the network wsi live a bit longer if
				 * subs are active... our frame may take a long
				 * time to chew through
				 */
				if (!wsi->immortal_substream_count)
					lws_set_timeout(wsi,
					PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE,
						wsi->a.vhost->keepalive_timeout ?
					    wsi->a.vhost->keepalive_timeout : 31);

				if (!h2n->swsi)
					break;

				if (lws_buflist_next_segment_len(
						&h2n->swsi->buflist, NULL))
					lwsl_info("%s: substream has pending\n",
						  __func__);

				if (lwsi_role_http(h2n->swsi) &&
				    lwsi_state(h2n->swsi) == LRS_ESTABLISHED) {
					lwsi_set_state(h2n->swsi, LRS_BODY);
					lwsl_info("%s: %s to LRS_BODY\n",
							__func__, lws_wsi_tag(h2n->swsi));
				}

				/*
				 * in + length may cover multiple frames, we
				 * can only consider the length of the DATA
				 * in front of us
				 */

				if (lws_hdr_total_length(h2n->swsi,
					     WSI_TOKEN_HTTP_CONTENT_LENGTH) &&
				    h2n->swsi->http.rx_content_length &&
				    h2n->swsi->http.rx_content_remain <
						     h2n->length && /* last */
				    h2n->inside < h2n->length) {

					lwsl_warn("%s: %lu %lu %lu %lu\n", __func__,
						  (unsigned long)h2n->swsi->http.rx_content_remain,
						(unsigned long)(lws_ptr_diff_size_t(iend, in) + 1),
						(unsigned long)h2n->inside, (unsigned long)h2n->length);

					/* unread data in frame */
					lws_h2_goaway(wsi,
						      H2_ERR_PROTOCOL_ERROR,
					    "More rx than content_length told");
					break;
				}

				/*
				 * We operate on a frame.  The RX we have at
				 * hand may exceed the current frame.
				 */

				n = (int)lws_ptr_diff_size_t(iend, in)  + 1;
				if (n > (int)(h2n->length - h2n->count + 1)) {
					if (h2n->count > h2n->length)
						goto close_swsi_and_return;
					n = (int)(h2n->length - h2n->count) + 1;
					lwsl_debug("---- restricting len to %d "
						   "\n", n);
				}
#if defined(LWS_WITH_CLIENT)
				if (h2n->swsi->client_mux_substream) {
					if (!h2n->swsi->a.protocol) {
						lwsl_err("%s: %p doesn't have protocol\n",
							 __func__, lws_wsi_tag(h2n->swsi));
						m = 1;
					} else {
						h2n->swsi->txc.peer_tx_cr_est -= n;
						wsi->txc.peer_tx_cr_est -= n;
						lws_wsi_txc_describe(&h2n->swsi->txc,
							__func__,
							h2n->swsi->mux.my_sid);
					m = user_callback_handle_rxflow(
						h2n->swsi->a.protocol->callback,
						h2n->swsi,
					  LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ,
						h2n->swsi->user_space,
						in - 1, (unsigned int)n);
					}

					in += n - 1;
					h2n->inside += (unsigned int)n;
					h2n->count += (unsigned int)n - 1;

					if (m) {
						lwsl_info("RECEIVE_CLIENT_HTTP "
							  "closed it\n");
						goto close_swsi_and_return;
					}

					goto do_windows;
				}
#endif
				if (lwsi_state(h2n->swsi) == LRS_DEFERRING_ACTION) {
					m = lws_buflist_append_segment(
						&h2n->swsi->buflist, in - 1, (unsigned int)n);
					if (m < 0)
						return -1;

					/*
					 * Since we're in an open-ended
					 * DEFERRING_ACTION, don't add this swsi
					 * to the pt list of wsi holding buflist
					 * content yet, we are not in a position
					 * to consume it until we get out of
					 * DEFERRING_ACTION.
					 */

					in += n - 1;
					h2n->inside += (unsigned int)n;
					h2n->count += (unsigned int)n - 1;

					lwsl_debug("%s: deferred %d\n", __func__, n);
					goto do_windows;
				}

				h2n->swsi->outer_will_close = 1;
				/*
				 * choose the length for this go so that we end at
				 * the frame boundary, in the case there is already
				 * more waiting leave it for next time around
				 */

				n = lws_read_h1(h2n->swsi, in - 1, (unsigned int)n);
				// lwsl_notice("%s: lws_read_h1 %d\n", __func__, n);
				h2n->swsi->outer_will_close = 0;
				/*
				 * can return 0 in POST body with
				 * content len exhausted somehow.
				 */
				if (n < 0 ||
				    (!n && h2n->swsi->http.content_length_given && !lws_buflist_next_segment_len(
						    &wsi->buflist, NULL))) {
					lwsl_info("%s: lws_read_h1 told %d %u / %u\n",
						__func__, n,
						(unsigned int)h2n->count,
						(unsigned int)h2n->length);
					in += h2n->length - h2n->count;
					h2n->inside = h2n->length;
					h2n->count = h2n->length - 1;

					//if (n < 0)
					//	goto already_closed_swsi;
					goto close_swsi_and_return;
				}

				lwsl_info("%s: lws_read_h1 telling %d %u / %u\n",
						__func__, n,
						(unsigned int)h2n->count,
						(unsigned int)h2n->length);

				in += (unsigned int)n - 1;
				h2n->inside += (unsigned int)n;
				h2n->count += (unsigned int)n - 1;

				h2n->swsi->txc.peer_tx_cr_est -= n;
				wsi->txc.peer_tx_cr_est -= n;

do_windows:

#if defined(LWS_WITH_CLIENT)
				if (!(h2n->swsi->flags & LCCSCF_H2_MANUAL_RXFLOW))
#endif
				{
					/*
					 * The default behaviour is we just keep
					 * cranking the other side's tx credit
					 * back up, for simple bulk transfer as
					 * fast as we can take it
					 */

					m = n  + 65536;

					/* update both the stream and nwsi */

					lws_h2_update_peer_txcredit_thresh(h2n->swsi,
								    h2n->sid, m, m);
				}
#if defined(LWS_WITH_CLIENT)
				else {
					/*
					 * If he's handling it himself, only
					 * repair the nwsi credit but allow the
					 * stream credit to run down until the
					 * user code deals with it
					 */
					lws_h2_update_peer_txcredit(wsi, 0, n);
					h2n->swsi->txc.manual = 1;
				}
#endif
				break;

			case LWS_H2_FRAME_TYPE_PRIORITY:
				if (h2n->count <= 4) {
					h2n->dep <<= 8;
					h2n->dep |= c;
					break;
				}
				h2n->weight_temp = c;
				lwsl_info("PRIORITY: dep 0x%x, weight 0x%02X\n",
					  (unsigned int)h2n->dep, h2n->weight_temp);

				if ((h2n->dep & ~(1u << 31)) == h2n->sid) {
					lws_h2_goaway(wsi, H2_ERR_PROTOCOL_ERROR,
						      "cant depend on own sid");
					break;
				}
				break;

			case LWS_H2_FRAME_TYPE_RST_STREAM:
				h2n->hpack_e_dep <<= 8;
				h2n->hpack_e_dep |= c;
				break;

			case LWS_H2_FRAME_TYPE_PUSH_PROMISE:
				break;

			case LWS_H2_FRAME_TYPE_PING:
				if (h2n->flags & LWS_H2_FLAG_SETTINGS_ACK) { // ack
				} else { /* they're sending us a ping request */
					if (h2n->count > 8)
						return 1;
					h2n->ping_payload[h2n->count - 1] = c;
				}
				break;

			case LWS_H2_FRAME_TYPE_WINDOW_UPDATE:
				h2n->hpack_e_dep <<= 8;
				h2n->hpack_e_dep |= c;
				break;

			case LWS_H2_FRAME_TYPE_COUNT: /* IGNORING FRAME */
				//lwsl_debug("%s: consuming for ignored %u %u\n", __func__, (unsigned int)h2n->count, (unsigned int)h2n->length);
				h2n->count++;
				break;

			default:
				lwsl_notice("%s: unhandled frame type %d\n",
					    __func__, h2n->type);

				goto fail;
			}

frame_end:
			if (h2n->count > h2n->length) {
				lwsl_notice("%s: count > length %u %u (type %d)\n",
					    __func__, (unsigned int)h2n->count,
					    (unsigned int)h2n->length, h2n->type);

			} else
				if (h2n->count != h2n->length)
					break;

			/*
			 * end of frame just happened
			 */
			n = lws_h2_parse_end_of_frame(wsi);
			if (n == 2) {
				*inused = (lws_filepos_t)lws_ptr_diff_size_t(in, oldin);

				return 2;
			}
			if (n)
				goto fail;

			break;

try_frame_start:
			if (h2n->frame_state <= 8) {

				switch (h2n->frame_state++) {
				case 0:
					h2n->pad_length = 0;
					h2n->collected_priority = 0;
					h2n->padding = 0;
					h2n->preamble = 0;
					h2n->length = c;
					h2n->inside = 0;
					break;
				case 1:
				case 2:
					h2n->length <<= 8;
					h2n->length |= c;
					break;
				case 3:
					h2n->type = c;
					break;
				case 4:
					h2n->flags = c;
					break;

				case 5:
				case 6:
				case 7:
				case 8:
					h2n->sid <<= 8;
					h2n->sid |= c;
					break;
				}
			}

			if (h2n->frame_state == LWS_H2_FRAME_HEADER_LENGTH &&
			    lws_h2_parse_frame_header(wsi))
				goto fail;
			break;

		default:
			if (h2n->type == LWS_H2_FRAME_TYPE_COUNT) { /* IGNORING FRAME */
				//lwsl_debug("%s: consuming for ignored %u %u\n", __func__, (unsigned int)h2n->count, (unsigned int)h2n->length);
				h2n->count++;
			}
			break;
		}
	}

	*inused = (lws_filepos_t)lws_ptr_diff_size_t(in, oldin);

	return 0;

close_swsi_and_return:

	lws_close_free_wsi(h2n->swsi, 0, "close_swsi_and_return");
	h2n->swsi = NULL;
	h2n->frame_state = 0;
	h2n->count = 0;

// already_closed_swsi:
	*inused = (lws_filepos_t)lws_ptr_diff_size_t(in, oldin);

	return 2;

fail:
	*inused = (lws_filepos_t)lws_ptr_diff_size_t(in, oldin);

	return 1;
}

#if defined(LWS_WITH_CLIENT)
int
lws_h2_client_handshake(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	uint8_t *buf, *start, *p, *p1, *end;
	char *meth = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_METHOD),
	     *uri = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_URI), *simp;
	struct lws *nwsi = lws_get_network_wsi(wsi);
	const char *path = "/";
	int n, m;
	/*
	 * The identifier of a newly established stream MUST be numerically
	 * greater than all streams that the initiating endpoint has opened or
	 * reserved.  This governs streams that are opened using a HEADERS frame
	 * and streams that are reserved using PUSH_PROMISE.  An endpoint that
	 * receives an unexpected stream identifier MUST respond with a
	 * connection error (Section 5.4.1) of type PROTOCOL_ERROR.
	 */
	unsigned int sid = nwsi->h2.h2n->highest_sid_opened + 2;

	lwsl_debug("%s\n", __func__);

	/*
	 * We MUST allocate our sid here at the point we're about to send the
	 * stream open.  It's because we don't know the order in which multiple
	 * open streams will send their headers... in h2, sending the headers
	 * is the point the stream is opened.  The peer requires that we only
	 * open streams in ascending sid order
	 */

	wsi->mux.my_sid = nwsi->h2.h2n->highest_sid_opened = sid;
	lwsl_info("%s: %s: assigning SID %d at header send\n", __func__,
			lws_wsi_tag(wsi), sid);


	lwsl_info("%s: CLIENT_WAITING_TO_SEND_HEADERS: pollout (sid %d)\n",
			__func__, wsi->mux.my_sid);

	p = start = buf = pt->serv_buf + LWS_PRE;
	end = start + (wsi->a.context->pt_serv_buf_size / 2) - LWS_PRE - 1;

	/* it's time for us to send our client stream headers */

	if (!meth)
		meth = "GET";

	/* h2 pseudoheaders must be in a bunch at the start */

	if (lws_add_http_header_by_token(wsi,
				WSI_TOKEN_HTTP_COLON_METHOD,
				(unsigned char *)meth,
				(int)strlen(meth), &p, end))
		goto fail_length;

	if (lws_add_http_header_by_token(wsi,
				WSI_TOKEN_HTTP_COLON_SCHEME,
				(unsigned char *)"https", 5,
				&p, end))
		goto fail_length;


	n = lws_hdr_total_length(wsi, _WSI_TOKEN_CLIENT_URI);
	if (n)
		path = uri;
	else
		if (wsi->stash && wsi->stash->cis[CIS_PATH]) {
			path = wsi->stash->cis[CIS_PATH];
			n = (int)strlen(path);
		} else
			n = 1;

	if (n > 1 && path[0] == '/' && path[1] == '/') {
		path++;
		n--;
	}

	if (n && lws_add_http_header_by_token(wsi,
				WSI_TOKEN_HTTP_COLON_PATH,
				(unsigned char *)path, n, &p, end))
		goto fail_length;

	n = lws_hdr_total_length(wsi, _WSI_TOKEN_CLIENT_HOST);
	simp = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_HOST);
	if (!n && wsi->stash && wsi->stash->cis[CIS_ADDRESS]) {
		n = (int)strlen(wsi->stash->cis[CIS_ADDRESS]);
		simp = wsi->stash->cis[CIS_ADDRESS];
	}

//	n = lws_hdr_total_length(wsi, _WSI_TOKEN_CLIENT_ORIGIN);
//	simp = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_ORIGIN);
#if 0
	if (n && simp && lws_add_http_header_by_token(wsi,
				WSI_TOKEN_HTTP_COLON_AUTHORITY,
				(unsigned char *)simp, n, &p, end))
		goto fail_length;
#endif


	if (/*!wsi->client_h2_alpn && */n && simp &&
	    lws_add_http_header_by_token(wsi, WSI_TOKEN_HOST,
				(unsigned char *)simp, n, &p, end))
		goto fail_length;


	if (wsi->flags & LCCSCF_HTTP_MULTIPART_MIME) {
		p1 = lws_http_multipart_headers(wsi, p);
		if (!p1)
			goto fail_length;
		p = p1;
	}

	if (wsi->flags & LCCSCF_HTTP_X_WWW_FORM_URLENCODED) {
		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
			   (unsigned char *)"application/x-www-form-urlencoded",
			   33, &p, end))
			goto fail_length;
		lws_client_http_body_pending(wsi, 1);
	}

	/* give userland a chance to append, eg, cookies */

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
	if (wsi->flags & LCCSCF_CACHE_COOKIES)
		lws_cookie_send_cookies(wsi, (char **)&p, (char *)end);
#endif

	if (wsi->a.protocol->callback(wsi,
				LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER,
				wsi->user_space, &p, lws_ptr_diff_size_t(end, p) - 12))
		goto fail_length;

	if (lws_finalize_http_header(wsi, &p, end))
		goto fail_length;

	m = LWS_WRITE_HTTP_HEADERS;
#if defined(LWS_WITH_CLIENT)
	/* below is not needed in spec, indeed it destroys the long poll
	 * feature, but required by nghttp2 */
	if ((wsi->flags & LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM) &&
	    !(wsi->client_http_body_pending  || lws_has_buffered_out(wsi)))
		m |= LWS_WRITE_H2_STREAM_END;
#endif

	// lwsl_hexdump_notice(start, p - start);

	n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start), (enum lws_write_protocol)m);

	if (n != lws_ptr_diff(p, start)) {
		lwsl_err("_write returned %d from %ld\n", n,
			 (long)(p - start));
		return -1;
	}

	/*
	 * Normally let's charge up the peer tx credit a bit.  But if
	 * MANUAL_REFLOW is set, just set it to the initial credit given in
	 * the client create info
	 */

	n = 4 * 65536;
	if (wsi->flags & LCCSCF_H2_MANUAL_RXFLOW) {
		n = wsi->txc.manual_initial_tx_credit;
		wsi->txc.manual = 1;
	}

	if (lws_h2_update_peer_txcredit(wsi, wsi->mux.my_sid, n))
		return 1;

	lws_h2_state(wsi, LWS_H2_STATE_OPEN);
	lwsi_set_state(wsi, LRS_ESTABLISHED);

	if (wsi->flags & LCCSCF_HTTP_MULTIPART_MIME)
		lws_callback_on_writable(wsi);

	return 0;

fail_length:
	lwsl_err("Client hdrs too long: incr context info.pt_serv_buf_size\n");

	return -1;
}
#endif

#if defined(LWS_ROLE_WS) && defined(LWS_WITH_SERVER)
int
lws_h2_ws_handshake(struct lws *wsi)
{
	uint8_t buf[LWS_PRE + 2048], *p = buf + LWS_PRE, *start = p,
		*end = &buf[sizeof(buf) - 1];
	const struct lws_http_mount *hit;
	const char * uri_ptr;
	size_t m;
	int n;

	if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
		return -1;

	if (lws_hdr_total_length(wsi, WSI_TOKEN_PROTOCOL) > 64)
		return -1;

	if (wsi->proxied_ws_parent && wsi->child_list) {
		if (lws_hdr_simple_ptr(wsi, WSI_TOKEN_PROTOCOL)) {
			if (lws_add_http_header_by_token(wsi, WSI_TOKEN_PROTOCOL,
				(uint8_t *)lws_hdr_simple_ptr(wsi,
							   WSI_TOKEN_PROTOCOL),
				(int)strlen(lws_hdr_simple_ptr(wsi,
							   WSI_TOKEN_PROTOCOL)),
						 &p, end))
			return -1;
		}
	} else {

		/* we can only return the protocol header if:
		 *  - one came in, and ... */
		if (lws_hdr_total_length(wsi, WSI_TOKEN_PROTOCOL) &&
		    /*  - it is not an empty string */
		    wsi->a.protocol->name && wsi->a.protocol->name[0]) {

#if defined(LWS_WITH_SECURE_STREAMS) && defined(LWS_WITH_SERVER)

		/*
		 * This is the h2 version of server-ws.c understanding that it
		 * did the ws upgrade on a ss server object, therefore it needs
		 * to pass back to the peer the policy ws-protocol name, not
		 * the generic ss-ws.c protocol name
		 */

		if (wsi->a.vhost && wsi->a.vhost->ss_handle &&
		    wsi->a.vhost->ss_handle->policy->u.http.u.ws.subprotocol) {
			lws_ss_handle_t *h =
				(lws_ss_handle_t *)wsi->a.opaque_user_data;

			lwsl_notice("%s: Server SS %s .wsi %s switching to ws protocol\n",
					__func__, lws_ss_tag(h), lws_wsi_tag(h->wsi));

			wsi->a.protocol = &protocol_secstream_ws;

			/*
			 * inform the SS user code that this has done a one-way
			 * upgrade to some other protocol... it will likely
			 * want to treat subsequent payloads differently
			 */

			lws_ss_event_helper(h, LWSSSCS_SERVER_UPGRADE);

			lws_mux_mark_immortal(wsi);

			if (lws_add_http_header_by_token(wsi, WSI_TOKEN_PROTOCOL,
				(unsigned char *)wsi->a.vhost->ss_handle->policy->
						u.http.u.ws.subprotocol,
				(int)strlen(wsi->a.vhost->ss_handle->policy->
						u.http.u.ws.subprotocol), &p, end))
					return -1;
		} else
#endif

			if (lws_add_http_header_by_token(wsi, WSI_TOKEN_PROTOCOL,
				(unsigned char *)wsi->a.protocol->name,
				(int)strlen(wsi->a.protocol->name), &p, end))
					return -1;
		}
	}

	if (lws_finalize_http_header(wsi, &p, end))
		return -1;

	m = lws_ptr_diff_size_t(p, start);
	// lwsl_hexdump_notice(start, m);
	n = lws_write(wsi, start, m, LWS_WRITE_HTTP_HEADERS);
	if (n != (int)m) {
		lwsl_err("_write returned %d from %d\n", n, (int)m);

		return -1;
	}

	/*
	 * alright clean up, set our state to generic ws established, the
	 * mode / state of the nwsi will get the h2 processing done.
	 */

	lwsi_set_state(wsi, LRS_ESTABLISHED);
	wsi->lws_rx_parse_state = 0; // ==LWS_RXPS_NEW;

	uri_ptr = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_PATH);
	n = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_PATH);
	hit = lws_find_mount(wsi, uri_ptr, n);

	if (hit && hit->cgienv &&
	    wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP_PMO, wsi->user_space,
				    (void *)hit->cgienv, 0))
		return 1;

	lws_validity_confirmed(wsi);

	return 0;
}
#endif

int
lws_read_h2(struct lws *wsi, unsigned char *buf, lws_filepos_t len)
{
	unsigned char *oldbuf = buf;
	lws_filepos_t body_chunk_len;

	// lwsl_notice("%s: h2 path: wsistate 0x%x len %d\n", __func__,
	//		wsi->wsistate, (int)len);

	/*
	 * wsi here is always the network connection wsi, not a stream
	 * wsi.  Once we unpicked the framing we will find the right
	 * swsi and make it the target of the frame.
	 *
	 * If it's ws over h2, the nwsi will get us here to do the h2
	 * processing, and that will call us back with the swsi +
	 * ESTABLISHED state for the inner payload, handled in a later
	 * case.
	 */
	while (len) {
		int m;

		/*
		 * we were accepting input but now we stopped doing so
		 */
		if (lws_is_flowcontrolled(wsi)) {
			lws_rxflow_cache(wsi, buf, 0, (size_t)len);
			buf += len;
			break;
		}

		/*
		 * lws_h2_parser() may send something; when it gets the
		 * whole frame, it will want to perform some action
		 * involving a reply.  But we may be in a partial send
		 * situation on the network wsi...
		 *
		 * Even though we may be in a partial send and unable to
		 * send anything new, we still have to parse the network
		 * wsi in order to gain tx credit to send, which is
		 * potentially necessary to clear the old partial send.
		 *
		 * ALL network wsi-specific frames are sent by PPS
		 * already, these are sent as a priority on the writable
		 * handler, and so respect partial sends.  The only
		 * problem is when a stream wsi wants to send an, eg,
		 * reply headers frame in response to the parsing
		 * we will do now... the *stream wsi* must stall in a
		 * different state until it is able to do so from a
		 * priority on the WRITABLE callback, same way that
		 * file transfers operate.
		 */

		m = lws_h2_parser(wsi, buf, len, &body_chunk_len);
		if (m && m != 2) {
			lwsl_debug("%s: http2_parser bail: %d\n", __func__, m);
			lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
					   "lws_read_h2 bail");

			return -1;
		}
		if (m == 2) {
			/* swsi has been closed */
			buf += body_chunk_len;
			break;
		}

		buf += body_chunk_len;
		len -= body_chunk_len;
	}

	return lws_ptr_diff(buf, oldbuf);
}

int
lws_h2_client_stream_long_poll_rxonly(struct lws *wsi)
{

	if (!wsi->mux_substream)
		return 1;

	/*
	 * Elect to send an empty DATA with END_STREAM, to force the stream
	 * into HALF_CLOSED LOCAL
	 */
	wsi->h2.long_poll = 1;
	wsi->h2.send_END_STREAM = 1;

	// lws_header_table_detach(wsi, 0);

	lws_callback_on_writable(wsi);

	return 0;
}
