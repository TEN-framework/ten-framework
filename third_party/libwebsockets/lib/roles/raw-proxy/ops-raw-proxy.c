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

#include <private-lib-core.h>

static int
rops_handle_POLLIN_raw_proxy(struct lws_context_per_thread *pt, struct lws *wsi,
			     struct lws_pollfd *pollfd)
{
	struct lws_tokens ebuf;
	int n, buffered;

	/* pending truncated sends have uber priority */

	if (lws_has_buffered_out(wsi)) {
		if (!(pollfd->revents & LWS_POLLOUT))
			return LWS_HPI_RET_HANDLED;

		/* drain the output buflist */
		if (lws_issue_raw(wsi, NULL, 0) < 0)
			goto fail;
		/*
		 * we can't afford to allow input processing to send
		 * something new, so spin around he event loop until
		 * he doesn't have any partials
		 */
		return LWS_HPI_RET_HANDLED;
	}

	if (lwsi_state(wsi) == LRS_WAITING_CONNECT)
		goto try_pollout;

	if ((pollfd->revents & pollfd->events & LWS_POLLIN) &&
	    /* any tunnel has to have been established... */
	    lwsi_state(wsi) != LRS_SSL_ACK_PENDING &&
	    !(wsi->favoured_pollin &&
	      (pollfd->revents & pollfd->events & LWS_POLLOUT))) {

		ebuf.token = NULL;
		ebuf.len = 0;
		buffered = lws_buflist_aware_read(pt, wsi, &ebuf, 1, __func__);
		if (buffered < 0)
			goto fail;

		switch (ebuf.len) {
		case 0:
			lwsl_info("%s: read 0 len\n", __func__);
			wsi->seen_zero_length_recv = 1;
			if (lws_change_pollfd(wsi, LWS_POLLIN, 0))
				goto fail;

			/*
			 * we need to go to fail here, since it's the only
			 * chance we get to understand that the socket has
			 * closed
			 */
			// goto try_pollout;
			goto fail;

		case LWS_SSL_CAPABLE_ERROR:
			goto fail;
		case LWS_SSL_CAPABLE_MORE_SERVICE:
			goto try_pollout;
		}
		n = user_callback_handle_rxflow(wsi->a.protocol->callback,
						wsi, lwsi_role_client(wsi) ?
						 LWS_CALLBACK_RAW_PROXY_CLI_RX :
						 LWS_CALLBACK_RAW_PROXY_SRV_RX,
						wsi->user_space, ebuf.token,
						(size_t)ebuf.len);
		if (n < 0) {
			lwsl_info("LWS_CALLBACK_RAW_PROXY_*_RX fail\n");
			goto fail;
		}

		if (lws_buflist_aware_finished_consuming(wsi, &ebuf, ebuf.len,
							 buffered, __func__))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
	} else
		if (wsi->favoured_pollin &&
		    (pollfd->revents & pollfd->events & LWS_POLLOUT))
			/* we balanced the last favouring of pollin */
			wsi->favoured_pollin = 0;

try_pollout:

	if (!(pollfd->revents & LWS_POLLOUT))
		return LWS_HPI_RET_HANDLED;

	if (lws_handle_POLLOUT_event(wsi, pollfd)) {
		lwsl_debug("POLLOUT event closed it\n");
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

#if defined(LWS_WITH_CLIENT)
	if (lws_http_client_socket_service(wsi, pollfd))
		return LWS_HPI_RET_WSI_ALREADY_DIED;
#endif

	return LWS_HPI_RET_HANDLED;

fail:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "raw svc fail");

	return LWS_HPI_RET_WSI_ALREADY_DIED;
}

static int
rops_adoption_bind_raw_proxy(struct lws *wsi, int type,
			     const char *vh_prot_name)
{
	/* no http but socket... must be raw skt */
	if ((type & LWS_ADOPT_HTTP) || !(type & LWS_ADOPT_SOCKET) ||
	    (!(type & LWS_ADOPT_FLAG_RAW_PROXY)) || (type & _LWS_ADOPT_FINISH))
		return 0; /* no match */

#if defined(LWS_WITH_UDP)
	if (type & LWS_ADOPT_FLAG_UDP)
		/*
		 * these can be >128 bytes, so just alloc for UDP
		 */
		wsi->udp = lws_malloc(sizeof(*wsi->udp), "udp struct");
#endif

	lws_role_transition(wsi, LWSIFR_SERVER, (type & LWS_ADOPT_ALLOW_SSL) ?
				    LRS_SSL_INIT : LRS_ESTABLISHED,
			    &role_ops_raw_proxy);

	if (vh_prot_name)
		lws_bind_protocol(wsi, wsi->a.protocol, __func__);
	else
		/* this is the only time he will transition */
		lws_bind_protocol(wsi,
			&wsi->a.vhost->protocols[wsi->a.vhost->raw_protocol_index],
			__func__);

	return 1; /* bound */
}

static int
rops_client_bind_raw_proxy(struct lws *wsi,
			   const struct lws_client_connect_info *i)
{
	if (!i) {

		/* finalize */

		if (!wsi->user_space && wsi->stash->cis[CIS_METHOD])
			if (lws_ensure_user_space(wsi))
				return 1;

		return 0;
	}

	/* we are a fallback if nothing else matched */

	if (i->local_protocol_name && !strcmp(i->local_protocol_name, "raw-proxy"))
		lws_role_transition(wsi, LWSIFR_CLIENT, LRS_UNCONNECTED,
				    &role_ops_raw_proxy);

	return 0;
}

static int
rops_handle_POLLOUT_raw_proxy(struct lws *wsi)
{
	if (lwsi_state(wsi) == LRS_ESTABLISHED)
		return LWS_HP_RET_USER_SERVICE;

	if (lwsi_role_client(wsi))
		return LWS_HP_RET_USER_SERVICE;

	return LWS_HP_RET_BAIL_OK;
}

static const lws_rops_t rops_table_raw_proxy[] = {
	/*  1 */ { .handle_POLLIN	= rops_handle_POLLIN_raw_proxy },
	/*  2 */ { .handle_POLLOUT	= rops_handle_POLLOUT_raw_proxy },
	/*  3 */ { .adoption_bind	= rops_adoption_bind_raw_proxy },
	/*  4 */ { .client_bind		= rops_client_bind_raw_proxy },
};


const struct lws_role_ops role_ops_raw_proxy = {
	/* role name */			"raw-proxy",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_raw_proxy,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x00,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x01,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x20,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x00,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x00,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x00,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x00,
	  /* LWS_ROPS_destroy_role */
	  /* LWS_ROPS_adoption_bind */			0x03,
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x40,
					},

	/* adoption_cb clnt, srv */	{ LWS_CALLBACK_RAW_PROXY_CLI_ADOPT,
					  LWS_CALLBACK_RAW_PROXY_SRV_ADOPT },
	/* rx_cb clnt, srv */		{ LWS_CALLBACK_RAW_PROXY_CLI_RX,
					  LWS_CALLBACK_RAW_PROXY_SRV_RX },
	/* writeable cb clnt, srv */	{ LWS_CALLBACK_RAW_PROXY_CLI_WRITEABLE,
					  LWS_CALLBACK_RAW_PROXY_SRV_WRITEABLE, },
	/* close cb clnt, srv */	{ LWS_CALLBACK_RAW_PROXY_CLI_CLOSE,
					  LWS_CALLBACK_RAW_PROXY_SRV_CLOSE },
	/* protocol_bind cb c, srv */	{ LWS_CALLBACK_RAW_PROXY_CLI_BIND_PROTOCOL,
					  LWS_CALLBACK_RAW_PROXY_SRV_BIND_PROTOCOL },
	/* protocol_unbind cb c, srv */	{ LWS_CALLBACK_RAW_PROXY_CLI_DROP_PROTOCOL,
					  LWS_CALLBACK_RAW_PROXY_SRV_DROP_PROTOCOL },
	/* file_handle */		0,
};
