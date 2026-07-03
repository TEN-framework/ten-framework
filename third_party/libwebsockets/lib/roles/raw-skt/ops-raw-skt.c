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

#include <private-lib-core.h>

#if defined(LWS_WITH_CLIENT)
static int
lws_raw_skt_connect(struct lws *wsi)
{
	int n;
#if defined(LWS_WITH_TLS)
	const char *cce = NULL;
	char ccebuf[128];

#if !defined(LWS_WITH_SYS_ASYNC_DNS)
	switch (lws_client_create_tls(wsi, &cce, 1)) {
#else
	switch (lws_client_create_tls(wsi, &cce, 0)) {
#endif
	case CCTLS_RETURN_ERROR:
		lws_inform_client_conn_fail(wsi, (void *)cce, strlen(cce));
		return -1;
	case CCTLS_RETURN_RETRY:
		return 0;
	case CCTLS_RETURN_DONE:
		break;
	}

	if (wsi->tls.use_ssl & LCCSCF_USE_SSL) {
		n = lws_ssl_client_connect2(wsi, ccebuf, sizeof(ccebuf));
		if (n < 0) {
			lws_inform_client_conn_fail(wsi, (void *)ccebuf,
						    strlen(ccebuf));

			return -1;
		}
		if (n != 1)
			return 0; /* wait */
	}
#endif

	n = user_callback_handle_rxflow(wsi->a.protocol->callback,
					wsi, wsi->role_ops->adoption_cb[lwsi_role_server(wsi)],
					wsi->user_space, NULL, 0);
	if (n) {
		lws_inform_client_conn_fail(wsi, (void *)"user", 4);
		return 1;
	}

	lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);
	lwsi_set_state(wsi, LRS_ESTABLISHED);

	return 1; /* success */
}
#endif

static int
rops_handle_POLLIN_raw_skt(struct lws_context_per_thread *pt, struct lws *wsi,
			   struct lws_pollfd *pollfd)
{
#if defined(LWS_WITH_SOCKS5)
	const char *cce = NULL;
#endif
	struct lws_tokens ebuf;
	int n = 0, buffered = 0;

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


#if defined(LWS_WITH_SERVER)
	if (!lwsi_role_client(wsi) &&  lwsi_state(wsi) != LRS_ESTABLISHED) {

		lwsl_wsi_debug(wsi, "wsistate 0x%x\n", (int)wsi->wsistate);

		if (lwsi_state(wsi) != LRS_SSL_INIT)
			if (lws_server_socket_service_ssl(wsi,
							  LWS_SOCK_INVALID,
				!!(pollfd->revents & pollfd->events & LWS_POLLIN)))
				return LWS_HPI_RET_PLEASE_CLOSE_ME;

		return LWS_HPI_RET_HANDLED;
	}
#endif

	if ((pollfd->revents & pollfd->events & LWS_POLLIN) &&
	    !(wsi->favoured_pollin &&
	      (pollfd->revents & pollfd->events & LWS_POLLOUT))) {

		lwsl_wsi_debug(wsi, "POLLIN: state 0x%x", lwsi_state(wsi));

		switch (lwsi_state(wsi)) {

		    /* any tunnel has to have been established... */
		case LRS_SSL_ACK_PENDING:
			goto nope;
		    /* we are actually connected */
		case LRS_WAITING_CONNECT:
			goto nope;

		case LRS_WAITING_SSL:
#if defined(LWS_WITH_CLIENT)
			n = lws_raw_skt_connect(wsi);
			if (n < 0)
				goto fail;
#endif
			break;

#if defined(LWS_WITH_SOCKS5)

		/* SOCKS Greeting Reply */
		case LRS_WAITING_SOCKS_GREETING_REPLY:
		case LRS_WAITING_SOCKS_AUTH_REPLY:
		case LRS_WAITING_SOCKS_CONNECT_REPLY:

			switch (lws_socks5c_handle_state(wsi, pollfd, &cce)) {
			case LW5CHS_RET_RET0:
				goto nope;
			case LW5CHS_RET_BAIL3:
				lws_inform_client_conn_fail(wsi, (void *)cce, strlen(cce));
				goto fail;
			case LW5CHS_RET_STARTHS:
				lwsi_set_state(wsi, LRS_ESTABLISHED);
				lws_client_connect_4_established(wsi, NULL, 0);

				/*
				 * Now we got the socks5 connection, we need to
				 * go down the tls path on it now if that's what
				 * we want
				 */
				goto post_rx;

			default:
				break;
			}
			goto post_rx;
#endif
		default:
			ebuf.token = NULL;
			ebuf.len = 0;

			buffered = lws_buflist_aware_read(pt, wsi, &ebuf, 1, __func__);
			switch (ebuf.len) {
			case 0:
				if (wsi->unix_skt)
					break;
				lwsl_wsi_info(wsi, "read 0 len");
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

#if defined(LWS_WITH_UDP)
			if (lws_fi(&wsi->fic, "udp_rx_loss")) {
				n = ebuf.len;
				goto post_rx;
			}
#endif

			n = user_callback_handle_rxflow(wsi->a.protocol->callback,
							wsi, LWS_CALLBACK_RAW_RX,
							wsi->user_space, ebuf.token,
							(unsigned int)ebuf.len);
#if defined(LWS_WITH_UDP) || defined(LWS_WITH_SOCKS5)
post_rx:
#endif
			if (n < 0) {
				lwsl_wsi_info(wsi, "LWS_CALLBACK_RAW_RX_fail");
				goto fail;
			}

			if (lws_buflist_aware_finished_consuming(wsi, &ebuf, ebuf.len,
								 buffered, __func__))
				return LWS_HPI_RET_PLEASE_CLOSE_ME;

			goto try_pollout;
		}
	}
nope:
	if (wsi->favoured_pollin &&
	    (pollfd->revents & pollfd->events & LWS_POLLOUT))
		/* we balanced the last favouring of pollin */
		wsi->favoured_pollin = 0;

try_pollout:

	if (!(pollfd->revents & LWS_POLLOUT))
		return LWS_HPI_RET_HANDLED;

#if defined(LWS_WITH_CLIENT)
	if (lwsi_state(wsi) == LRS_WAITING_CONNECT) {
	    if (!lws_client_connect_3_connect(wsi, NULL, NULL, 0, NULL))
		return LWS_HPI_RET_WSI_ALREADY_DIED;

	    if (lws_raw_skt_connect(wsi) < 0)
		    goto fail;
	}
#endif

	if (lwsi_state(wsi) == LRS_WAITING_SSL)
		return LWS_HPI_RET_HANDLED;

	/* one shot */
	if (lws_change_pollfd(wsi, LWS_POLLOUT, 0))
		goto fail;

	/* clear back-to-back write detection */
	wsi->could_have_pending = 0;

	n = user_callback_handle_rxflow(wsi->a.protocol->callback,
			wsi, LWS_CALLBACK_RAW_WRITEABLE,
			wsi->user_space, NULL, 0);
	if (n < 0) {
		lwsl_info("writeable_fail\n");
		goto fail;
	}

	return LWS_HPI_RET_HANDLED;

fail:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "raw svc fail");

	return LWS_HPI_RET_WSI_ALREADY_DIED;
}

#if defined(LWS_WITH_SERVER)
static int
rops_adoption_bind_raw_skt(struct lws *wsi, int type, const char *vh_prot_name)
{

	// lwsl_notice("%s: bind type %d\n", __func__, type);

	/* no http but socket... must be raw skt */
	if ((type & LWS_ADOPT_HTTP) || !(type & LWS_ADOPT_SOCKET) ||
	    ((type & _LWS_ADOPT_FINISH) && (!(type & LWS_ADOPT_FLAG_UDP))))
		return 0; /* no match */

#if defined(LWS_WITH_UDP)
	if ((type & LWS_ADOPT_FLAG_UDP) && !wsi->udp) {
		/*
		 * these can be >128 bytes, so just alloc for UDP
		 */
		wsi->udp = lws_malloc(sizeof(*wsi->udp), "udp struct");
		if (!wsi->udp)
			return 0;
		memset(wsi->udp, 0, sizeof(*wsi->udp));
	}
#endif

	lws_role_transition(wsi, 0, (type & LWS_ADOPT_ALLOW_SSL) ? LRS_SSL_INIT :
				LRS_ESTABLISHED, &role_ops_raw_skt);

	if (vh_prot_name)
		lws_bind_protocol(wsi, wsi->a.protocol, __func__);
	else
		/* this is the only time he will transition */
		lws_bind_protocol(wsi,
			&wsi->a.vhost->protocols[wsi->a.vhost->raw_protocol_index],
			__func__);

	return 1; /* bound */
}
#endif

#if defined(LWS_WITH_CLIENT)
static int
rops_client_bind_raw_skt(struct lws *wsi,
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

	if (!i->local_protocol_name ||
	    strcmp(i->local_protocol_name, "raw-proxy"))
		lws_role_transition(wsi, LWSIFR_CLIENT, LRS_UNCONNECTED,
			    &role_ops_raw_skt);

	return 1; /* matched */
}
#endif

static const lws_rops_t rops_table_raw_skt[] = {
	/*  1 */ { .handle_POLLIN	  = rops_handle_POLLIN_raw_skt },
#if defined(LWS_WITH_SERVER)
	/*  2 */ { .adoption_bind	  = rops_adoption_bind_raw_skt },
#else
	/*  2 */ { .adoption_bind	  = NULL },
#endif
#if defined(LWS_WITH_CLIENT)
	/*  3 */ { .client_bind		  = rops_client_bind_raw_skt },
#endif
};

const struct lws_role_ops role_ops_raw_skt = {
	/* role name */			"raw-skt",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_raw_skt,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x00,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x01,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x00,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x00,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x00,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x00,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x00,
	  /* LWS_ROPS_destroy_role */
#if defined(LWS_WITH_SERVER)
	  /* LWS_ROPS_adoption_bind */			0x02,
#else
	  /* LWS_ROPS_adoption_bind */			0x00,
#endif
#if defined(LWS_WITH_CLIENT)
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x30,
#else
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x00,
#endif
					},

	/* adoption_cb clnt, srv */	{ LWS_CALLBACK_RAW_CONNECTED,
					  LWS_CALLBACK_RAW_ADOPT },
	/* rx_cb clnt, srv */		{ LWS_CALLBACK_RAW_RX,
					  LWS_CALLBACK_RAW_RX },
	/* writeable cb clnt, srv */	{ LWS_CALLBACK_RAW_WRITEABLE,
					  LWS_CALLBACK_RAW_WRITEABLE},
	/* close cb clnt, srv */	{ LWS_CALLBACK_RAW_CLOSE,
					  LWS_CALLBACK_RAW_CLOSE },
	/* protocol_bind cb c, srv */	{ LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL,
					  LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL },
	/* protocol_unbind cb c, srv */	{ LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL,
					  LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL },
	/* file_handle */		0,
};
