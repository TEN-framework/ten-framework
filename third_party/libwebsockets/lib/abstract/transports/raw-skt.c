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
#include "private-lib-abstract.h"

typedef struct lws_abstxp_raw_skt_priv {
	struct lws_abs *abs;
	struct lws *wsi;

	lws_dll2_t same_abs_transport_list;

	uint8_t established:1;
	uint8_t connecting:1;
} abs_raw_skt_priv_t;

struct vhd {
	lws_dll2_owner_t owner;
};

static int
heartbeat_cb(struct lws_dll2 *d, void *user)
{
	abs_raw_skt_priv_t *priv = lws_container_of(d, abs_raw_skt_priv_t,
						    same_abs_transport_list);

	if (priv->abs->ap->heartbeat)
		priv->abs->ap->heartbeat(priv->abs->api);

	return 0;
}

static int
callback_abs_client_raw_skt(struct lws *wsi, enum lws_callback_reasons reason,
			    void *user, void *in, size_t len)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)user;
	struct vhd *vhd = (struct vhd *)
		lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					 lws_get_protocol(wsi));

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct vhd));
		if (!vhd)
			return 1;
		lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
					       lws_get_protocol(wsi),
					       LWS_CALLBACK_USER, 1);
		break;

	case LWS_CALLBACK_USER:
		/*
		 * This comes at 1Hz without a wsi context, so there is no
		 * valid priv.  We need to track the live abstract objects that
		 * are using our abstract protocol, and pass the heartbeat
		 * through to the ones that care.
		 */
		if (!vhd)
			break;

		lws_dll2_foreach_safe(&vhd->owner, NULL, heartbeat_cb);

		lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
					       lws_get_protocol(wsi),
					       LWS_CALLBACK_USER, 1);
		break;

        case LWS_CALLBACK_RAW_CONNECTED:
		lwsl_debug("LWS_CALLBACK_RAW_CONNECTED\n");
		priv->connecting = 0;
		priv->established = 1;
		if (priv->abs->ap->accept)
			priv->abs->ap->accept(priv->abs->api);
		if (wsi->seq)
			/*
			 * we are bound to a sequencer who wants to know about
			 * our lifecycle events
			 */

			lws_seq_queue_event(wsi->seq, LWSSEQ_WSI_CONNECTED,
						  wsi, NULL);
                break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_user("CONNECTION_ERROR\n");
		if (in)
			lwsl_user("   %s\n", (const char *)in);

		if (wsi->seq)
			/*
			 * we are bound to a sequencer who wants to know about
			 * our lifecycle events
			 */

			lws_seq_queue_event(wsi->seq, LWSSEQ_WSI_CONN_FAIL,
					    wsi, NULL);

		goto close_path;

		/* fallthru */
	case LWS_CALLBACK_RAW_CLOSE:
		if (!user)
			break;

		if (wsi->seq)
			/*
			 * we are bound to a sequencer who wants to know about
			 * our lifecycle events
			 */

			lws_seq_queue_event(wsi->seq, LWSSEQ_WSI_CONN_CLOSE,
					    wsi, NULL);

close_path:
		lwsl_debug("LWS_CALLBACK_RAW_CLOSE\n");
		priv->established = 0;
		priv->connecting = 0;
		if (priv->abs && priv->abs->ap->closed)
			priv->abs->ap->closed(priv->abs->api);
		lws_set_wsi_user(wsi, NULL);
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_debug("LWS_CALLBACK_RAW_RX (%d)\n", (int)len);
		return !!priv->abs->ap->rx(priv->abs->api, in, len);

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_debug("LWS_CALLBACK_RAW_WRITEABLE\n");
		priv->abs->ap->writeable(priv->abs->api,
				lws_get_peer_write_allowance(priv->wsi));
		break;

	case LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL:
		lws_dll2_add_tail(&priv->same_abs_transport_list, &vhd->owner);
		break;

	case LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL:
		lws_dll2_remove(&priv->same_abs_transport_list);
		break;

	default:
		break;
	}

	return 0;
}

static int
lws_atcrs_close(lws_abs_transport_inst_t *ati)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)ati;
	struct lws *wsi = priv->wsi;

	if (!priv->wsi)
		return 0;

	if (!lws_raw_transaction_completed(priv->wsi))
		return 0;

	priv->wsi = NULL;
	lws_set_timeout(wsi, 1, LWS_TO_KILL_SYNC);

	/* priv is destroyed in the CLOSE callback */

	return 0;
}


const struct lws_protocols protocol_abs_client_raw_skt = {
	"lws-abs-cli-raw-skt", callback_abs_client_raw_skt,
	0, 1024, 1024, NULL, 0
};

static int
lws_atcrs_tx(lws_abs_transport_inst_t *ati, uint8_t *buf, size_t len)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)ati;

	if (!priv->wsi) {
		lwsl_err("%s: NULL priv->wsi\n", __func__);
		return 1;
	}

	lwsl_debug("%s: priv %p, wsi %p, ro %p\n", __func__,
			priv, priv->wsi, priv->wsi->role_ops);

	if (lws_write(priv->wsi, buf, len, LWS_WRITE_RAW) < 0)
		lws_atcrs_close(ati);

	return 0;
}

#if defined(LWS_WITH_CLIENT)
static int
lws_atcrs_client_conn(const lws_abs_t *abs)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)abs->ati;
	struct lws_client_connect_info i;
	const lws_token_map_t *tm;

	if (priv->connecting)
		return 0;

	if (priv->established) {
		lws_set_timeout(priv->wsi, PENDING_TIMEOUT_CLIENT_CONN_IDLE, 5);

		return 0;
	}

	memset(&i, 0, sizeof(i));

	/* address and port are passed-in using the abstract transport tokens */

	tm = lws_abs_get_token(abs->at_tokens, LTMI_PEER_V_DNS_ADDRESS);
	if (!tm) {
		lwsl_notice("%s: raw_skt needs LTMI_PEER_V_DNS_ADDRESS\n",
			    __func__);

		return 1;
	}
	i.address = tm->u.value;

	tm = lws_abs_get_token(abs->at_tokens, LTMI_PEER_LV_PORT);
	if (!tm) {
		lwsl_notice("%s: raw_skt needs LTMI_PEER_LV_PORT\n", __func__);

		return 1;
	}
	i.port = tm->u.lvalue;

	/* optional */
	i.ssl_connection = 0;
	tm = lws_abs_get_token(abs->at_tokens, LTMI_PEER_LV_TLS_FLAGS);
	if (tm)
		i.ssl_connection = tm->u.lvalue;


	lwsl_debug("%s: raw_skt priv %p connecting to %s:%u %p\n",
		   __func__, priv, i.address, i.port, abs->vh->context);

	i.path = "";
	i.method = "RAW";
	i.vhost = abs->vh;
	i.userdata = priv;
	i.host = i.address;
	i.pwsi = &priv->wsi;
	i.origin = i.address;
	i.context = abs->vh->context;
	i.local_protocol_name = "lws-abs-cli-raw-skt";
	i.seq = abs->seq;
	i.opaque_user_data = abs->opaque_user_data;

	/*
	 * the protocol itself has some natural attributes we should pass on
	 */

	if (abs->ap->flags & LWS_AP_FLAG_PIPELINE_TRANSACTIONS)
		i.ssl_connection |= LCCSCF_PIPELINE;

	if (abs->ap->flags & LWS_AP_FLAG_MUXABLE_STREAM)
		i.ssl_connection |= LCCSCF_MUXABLE_STREAM;

	priv->wsi = lws_client_connect_via_info(&i);
	if (!priv->wsi)
		return 1;

	priv->connecting = 1;

	return 0;
}
#endif

static int
lws_atcrs_ask_for_writeable(lws_abs_transport_inst_t *ati)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)ati;

	if (!priv->wsi || !priv->established)
		return 1;

	lws_callback_on_writable(priv->wsi);

	return 0;
}

static int
lws_atcrs_create(struct lws_abs *ai)
{
	abs_raw_skt_priv_t *at = (abs_raw_skt_priv_t *)ai->ati;

	memset(at, 0, sizeof(*at));
	at->abs = ai;

	return 0;
}

static void
lws_atcrs_destroy(lws_abs_transport_inst_t **pati)
{
	/*
	 * For ourselves, we don't free anything because the abstract layer
	 * combined our allocation with that of the abs instance, and it will
	 * free the whole thing after this.
	 */
	*pati = NULL;
}

static int
lws_atcrs_set_timeout(lws_abs_transport_inst_t *ati, int reason, int secs)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)ati;

	lws_set_timeout(priv->wsi, reason, secs);

	return 0;
}

static int
lws_atcrs_state(lws_abs_transport_inst_t *ati)
{
	abs_raw_skt_priv_t *priv = (abs_raw_skt_priv_t *)ati;

	if (!priv || !priv->wsi || (!priv->established && !priv->connecting))
		return 0;

	return 1;
}

static int
lws_atcrs_compare(lws_abs_t *abs1, lws_abs_t *abs2)
{
	const lws_token_map_t *tm1, *tm2;

	tm1 = lws_abs_get_token(abs1->at_tokens, LTMI_PEER_V_DNS_ADDRESS);
	tm2 = lws_abs_get_token(abs2->at_tokens, LTMI_PEER_V_DNS_ADDRESS);

	/* Address token is mandatory and must match */
	if (!tm1 || !tm2 || strcmp(tm1->u.value, tm2->u.value))
		return 1;

	/* Port token is mandatory and must match */
	tm1 = lws_abs_get_token(abs1->at_tokens, LTMI_PEER_LV_PORT);
	tm2 = lws_abs_get_token(abs2->at_tokens, LTMI_PEER_LV_PORT);
	if (!tm1 || !tm2 || tm1->u.lvalue != tm2->u.lvalue)
		return 1;

	/* TLS is optional... */
	tm1 = lws_abs_get_token(abs1->at_tokens, LTMI_PEER_LV_TLS_FLAGS);
	tm2 = lws_abs_get_token(abs2->at_tokens, LTMI_PEER_LV_TLS_FLAGS);

	/* ... but both must have the same situation with it given or not... */
	if (!!tm1 != !!tm2)
		return 1;

	/* if not using TLS, then that's enough to call it */
	if (!tm1)
		return 0;

	/* ...and if there are tls flags, both must have the same tls flags */
	if (tm1->u.lvalue != tm2->u.lvalue)
		return 1;

	/* ... and both must use the same client tls ctx / vhost */
	return abs1->vh != abs2->vh;
}

const lws_abs_transport_t lws_abs_transport_cli_raw_skt = {
	.name			= "raw_skt",
	.alloc			= sizeof(abs_raw_skt_priv_t),

	.create			= lws_atcrs_create,
	.destroy		= lws_atcrs_destroy,
	.compare		= lws_atcrs_compare,

	.tx			= lws_atcrs_tx,
#if !defined(LWS_WITH_CLIENT)
	.client_conn		= NULL,
#else
	.client_conn		= lws_atcrs_client_conn,
#endif
	.close			= lws_atcrs_close,
	.ask_for_writeable	= lws_atcrs_ask_for_writeable,
	.set_timeout		= lws_atcrs_set_timeout,
	.state			= lws_atcrs_state,
};
