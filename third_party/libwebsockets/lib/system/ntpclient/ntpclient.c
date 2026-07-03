 /*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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

#define LWSNTPC_LI_NONE			0
#define LWSNTPC_VN_3			3
#define LWSNTPC_MODE_CLIENT		3

struct vhd_ntpc {
	struct lws_context		*context;
	struct lws_vhost		*vhost;
	const struct lws_protocols	*protocol;
	lws_sorted_usec_list_t		sul_conn;
	lws_sorted_usec_list_t		sul_write; /* track write retries */
	const char			*ntp_server_ads;
	struct lws			*wsi_udp;
	uint16_t			retry_count_conn;
	uint16_t			retry_count_write;

	char				set_time;
};

/*
 * Without a valid ntp we won't be able to do anything requiring client tls.
 *
 * We have our own outer backoff scheme that just keeps retrying dns lookup
 * and the transaction forever.
 */

static const uint32_t botable[] =
		{ 300, 500, 650, 800, 800, 900, 1000, 1100, 1500 };
static const lws_retry_bo_t bo = {
	botable, LWS_ARRAY_SIZE(botable), LWS_RETRY_CONCEAL_ALWAYS, 0, 0, 20 };

/*
 * Once we resolved the remote server (implying we should have network),
 * we use a different policy on the wsi itself that gives it a few tries before
 * failing the wsi and using to outer retry policy to get dns to a different
 * server in the pool and try fresh
 */

static const uint32_t botable2[] = { 1000, 1250, 5000 /* in case dog slow */ };
static const lws_retry_bo_t bo2 = {
	botable2, LWS_ARRAY_SIZE(botable2), LWS_ARRAY_SIZE(botable2),
	/* don't conceal after the last table entry */ 0, 0, 20 };

static void
lws_ntpc_retry_conn(struct lws_sorted_usec_list *sul)
{
	struct vhd_ntpc *v = lws_container_of(sul, struct vhd_ntpc, sul_conn);

	lwsl_debug("%s: wsi_udp: %s\n", __func__, lws_wsi_tag(v->wsi_udp));

	if (v->wsi_udp || !lws_dll2_is_detached(&v->sul_conn.list))
		return;

	/* create the UDP socket aimed at the server */

	lwsl_notice("%s: server %s\n", __func__, v->ntp_server_ads);

	v->retry_count_write = 0;
	v->wsi_udp = lws_create_adopt_udp(v->vhost, v->ntp_server_ads, 123, 0,
					  v->protocol->name, NULL, NULL, NULL,
					  &bo2, "ntpclient");
	lwsl_debug("%s: created wsi_udp: %s\n", __func__, lws_wsi_tag(v->wsi_udp));
	if (!v->wsi_udp) {
		lwsl_err("%s: unable to create udp skt\n", __func__);

		lws_retry_sul_schedule(v->context, 0, &v->sul_conn, &bo,
				       lws_ntpc_retry_conn, &v->retry_count_conn);
	}
}

static void
lws_ntpc_retry_write(struct lws_sorted_usec_list *sul)
{
	struct vhd_ntpc *v = lws_container_of(sul, struct vhd_ntpc, sul_write);

	lwsl_debug("%s\n", __func__);

	if (v && v->wsi_udp)
		lws_callback_on_writable(v->wsi_udp);
}

static int
callback_ntpc(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct vhd_ntpc *v = (struct vhd_ntpc *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
	uint8_t pkt[LWS_PRE + 48];
	struct timeval t1;
	int64_t delta_us;
	uint64_t ns;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT: /* per vhost */
		if (v)
			break;

		lwsl_debug("%s: LWS_CALLBACK_PROTOCOL_INIT:\n", __func__);
		lws_protocol_vh_priv_zalloc(wsi->a.vhost, wsi->a.protocol,
					    sizeof(*v));
		v = (struct vhd_ntpc *)lws_protocol_vh_priv_get(wsi->a.vhost,
								wsi->a.protocol);
		v->context = lws_get_context(wsi);
		v->vhost = lws_get_vhost(wsi);
		v->protocol = lws_get_protocol(wsi);

		v->context->ntpclient_priv = v;

		if (!lws_system_get_ops(wsi->a.context) ||
		    !lws_system_get_ops(wsi->a.context)->set_clock) {
#if !defined(LWS_ESP_PLATFORM)
			lwsl_err("%s: set up system ops for set_clock\n",
					__func__);
#endif

		//	return -1;
		}

		/* register our lws_system notifier */

		v->ntp_server_ads = "pool.ntp.org";
		lws_plat_ntpclient_config(v->context);
		lws_system_blob_get_single_ptr(lws_system_get_blob(
				v->context, LWS_SYSBLOB_TYPE_NTP_SERVER, 0),
				(const uint8_t **)&v->ntp_server_ads);
		if (!v->ntp_server_ads || v->ntp_server_ads[0] == '\0')
			v->ntp_server_ads = "pool.ntp.org";

		lwsl_notice("%s: using ntp server %s\n", __func__,
			  v->ntp_server_ads);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY: /* per vhost */
		if (!v)
			break;
		if (v->wsi_udp)
			lws_set_timeout(v->wsi_udp, 1, LWS_TO_KILL_ASYNC);
		v->wsi_udp = NULL;
		goto cancel_conn_timer;

	/* callbacks related to raw socket descriptor */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_debug("%s: LWS_CALLBACK_RAW_ADOPT\n", __func__);
		lws_callback_on_writable(wsi);
		break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_info("%s: CONNECTION_ERROR\n", __func__);
		goto do_close;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_debug("%s: LWS_CALLBACK_RAW_CLOSE\n", __func__);
do_close:
		v->wsi_udp = NULL;

		/* cancel any pending write retry */
		lws_sul_cancel(&v->sul_write);

		if (v->set_time)
			goto cancel_conn_timer;

		lws_retry_sul_schedule(v->context, 0, &v->sul_conn, &bo,
				       lws_ntpc_retry_conn,
				       &v->retry_count_conn);
		break;

	case LWS_CALLBACK_RAW_RX:

		if (len != 48)
			return 0; /* ignore it */

		/*
		 * First get the seconds, corrected for the ntp epoch of 1900
		 * vs the unix epoch of 1970.  Then shift the seconds up by 1bn
		 * and add in the ns
		 */

		ns = (uint64_t)lws_ser_ru32be(((uint8_t *)in) + 40) - (uint64_t)2208988800;
		ns = (ns * 1000000000) + lws_ser_ru32be(((uint8_t *)in) + 44);

		/*
		 * Compute the step
		 */

		gettimeofday(&t1, NULL);

		delta_us = ((int64_t)ns / 1000) -
				((t1.tv_sec * LWS_US_PER_SEC) + t1.tv_usec);

		lwsl_notice("%s: Unix time: %llu, step: %lldus\n", __func__,
				(unsigned long long)ns / 1000000000,
				(long long)delta_us);

#if defined(LWS_PLAT_FREERTOS)
		{
			struct timeval t;

			t.tv_sec = (unsigned long long)ns / 1000000000;
			t.tv_usec = (ns % 1000000000) / 1000;

			lws_sul_nonmonotonic_adjust(wsi->a.context, delta_us);

			settimeofday(&t, NULL);
		}
#endif
		if (lws_system_get_ops(wsi->a.context) &&
		    lws_system_get_ops(wsi->a.context)->set_clock)
			lws_system_get_ops(wsi->a.context)->set_clock((int64_t)ns / 1000);

		v->set_time = 1;
		lws_state_transition_steps(&wsi->a.context->mgr_system,
					   LWS_SYSTATE_OPERATIONAL);

		/* close the wsi */
		return -1;

	case LWS_CALLBACK_RAW_WRITEABLE:

		/*
		 * UDP is not reliable, it can be locally dropped, or dropped
		 * by any intermediary or the remote peer.  So even though we
		 * will do the write in a moment, we schedule another request
		 * for rewrite according to the wsi retry policy.
		 *
		 * If the result came before, we'll cancel it in the close flow.
		 *
		 * If we have already reached the end of our concealed retries
		 * in the policy, just close without another write.
		 */
		if (lws_dll2_is_detached(&v->sul_write.list) &&
		    lws_retry_sul_schedule_retry_wsi(wsi, &v->sul_write,
						     lws_ntpc_retry_write,
						     &v->retry_count_write)) {
			/* we have reached the end of our concealed retries */
			lwsl_warn("%s: concealed retries done, failing\n", __func__);
			goto retry_conn;
		}

		memset(pkt + LWS_PRE, 0, sizeof(pkt) - LWS_PRE);
		pkt[LWS_PRE] = (LWSNTPC_LI_NONE << 6) |
			       (LWSNTPC_VN_3 << 3) |
			       (LWSNTPC_MODE_CLIENT << 0);

		if (lws_write(wsi, pkt + LWS_PRE, sizeof(pkt) - LWS_PRE, 0) ==
						  sizeof(pkt) - LWS_PRE)
			break;

		lwsl_err("%s: Failed to write ntp client req\n", __func__);

retry_conn:
		lws_retry_sul_schedule(wsi->a.context, 0, &v->sul_conn, &bo,
				       lws_ntpc_retry_conn,
				       &v->retry_count_conn);

		return -1;

	default:
		break;
	}

	return 0;


cancel_conn_timer:
	lws_sul_cancel(&v->sul_conn);

	return 0;
}

void
lws_ntpc_trigger(struct lws_context *ctx)
{
	struct vhd_ntpc *v = (struct vhd_ntpc *)ctx->ntpclient_priv;

	lwsl_notice("%s\n", __func__);
	v->retry_count_conn = 0;
	lws_ntpc_retry_conn(&v->sul_conn);
}

struct lws_protocols lws_system_protocol_ntpc =
	{ "lws-ntpclient", callback_ntpc, 0, 128, 0, NULL, 0 };

