/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

struct lws *
lws_client_connect_4_established(struct lws *wsi, struct lws *wsi_piggyback,
				 ssize_t plen)
{
#if defined(LWS_CLIENT_HTTP_PROXYING)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
#endif
	const char *meth;
	struct lws_pollfd pfd;
	const char *cce = "";
	int n, m, rawish = 0;

	meth = lws_wsi_client_stash_item(wsi, CIS_METHOD,
					 _WSI_TOKEN_CLIENT_METHOD);

	if (meth && (!strcmp(meth, "RAW")
#if defined(LWS_ROLE_MQTT)
		     || !strcmp(meth, "MQTT")
#endif
	))
		rawish = 1;

	if (wsi_piggyback)
		goto send_hs;

#if defined(LWS_CLIENT_HTTP_PROXYING)
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	/* we are connected to server, or proxy */

	/* http proxy */
	if (wsi->a.vhost->http.http_proxy_port) {
		const char *cpa;

		cpa = lws_wsi_client_stash_item(wsi, CIS_ADDRESS,
						_WSI_TOKEN_CLIENT_PEER_ADDRESS);
		if (!cpa)
			goto failed;

		lwsl_wsi_info(wsi, "going via proxy");

		plen = lws_snprintf((char *)pt->serv_buf, 256,
			"CONNECT %s:%u HTTP/1.1\x0d\x0a"
			"Host: %s:%u\x0d\x0a"
			"User-agent: lws\x0d\x0a", cpa, wsi->ocport,
						   cpa, wsi->ocport);

#if defined(LWS_WITH_HTTP_BASIC_AUTH)
		if (wsi->a.vhost->proxy_basic_auth_token[0])
			plen += lws_snprintf((char *)pt->serv_buf + plen, 256,
					"Proxy-authorization: basic %s\x0d\x0a",
					wsi->a.vhost->proxy_basic_auth_token);
#endif

		plen += lws_snprintf((char *)pt->serv_buf + plen, 5,
					"\x0d\x0a");

		/* lwsl_hexdump_notice(pt->serv_buf, plen); */

		/*
		 * OK from now on we talk via the proxy, so connect to that
		 */
		if (wsi->stash)
			wsi->stash->cis[CIS_ADDRESS] =
				wsi->a.vhost->http.http_proxy_address;
		else
			if (lws_hdr_simple_create(wsi,
					_WSI_TOKEN_CLIENT_PEER_ADDRESS,
					wsi->a.vhost->http.http_proxy_address))
			goto failed;
		wsi->c_port = (uint16_t)wsi->a.vhost->http.http_proxy_port;

		n = (int)send(wsi->desc.sockfd, (char *)pt->serv_buf,
			      (unsigned int)plen,
			 MSG_NOSIGNAL);
		if (n < 0) {
			lwsl_wsi_debug(wsi, "ERROR writing to proxy socket");
			cce = "proxy write failed";
			goto failed;
		}

		lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE,
				(int)wsi->a.context->timeout_secs);

		wsi->conn_port = wsi->c_port;
		lwsi_set_state(wsi, LRS_WAITING_PROXY_REPLY);

		return wsi;
	}
#endif
#endif

	/* coverity */
	if (!wsi->a.protocol)
		return NULL;

#if defined(LWS_WITH_SOCKS5)
	if (lwsi_state(wsi) != 	LRS_ESTABLISHED)
		switch (lws_socks5c_greet(wsi, &cce)) {
		case -1:
			goto failed;
		case 1:
			return wsi;
		default:
			break;
		}
#endif

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
send_hs:

	if (wsi_piggyback &&
	    !lws_dll2_is_detached(&wsi->dll2_cli_txn_queue)) {
		/*
		 * We are pipelining on an already-established connection...
		 * we can skip tls establishment.
		 *
		 * Set these queued guys to a state where they won't actually
		 * send their headers until we decide later.
		 */

		lwsi_set_state(wsi, LRS_H2_WAITING_TO_SEND_HEADERS);

		/*
		 * we can't send our headers directly, because they have to
		 * be sent when the parent is writeable.  The parent will check
		 * for anybody on his client transaction queue that is in
		 * LRS_H1C_ISSUE_HANDSHAKE2, and let them write.
		 *
		 * If we are trying to do this too early, before the network
		 * connection has written his own headers, then it will just
		 * wait in the queue until it's possible to send them.
		 */
		lws_callback_on_writable(wsi_piggyback);

		lwsl_wsi_info(wsi, "waiting to send hdrs (par state 0x%x)",
			      lwsi_state(wsi_piggyback));
	} else {
		lwsl_wsi_info(wsi, "%s %s client created own conn "
			  "(raw %d) vh %s st 0x%x",
			  wsi->role_ops->name, wsi->a.protocol->name, rawish,
			  wsi->a.vhost->name, lwsi_state(wsi));

		/* we are making our own connection */

		if (!rawish
#if defined(LWS_WITH_TLS)
		    // && (!(wsi->tls.use_ssl & LCCSCF_USE_SSL) || wsi->tls.ssl)
#endif
		    ) {

			if (lwsi_state(wsi) != LRS_H1C_ISSUE_HANDSHAKE2)
				lwsi_set_state(wsi, LRS_H1C_ISSUE_HANDSHAKE);
		} else {
			/* for a method = "RAW" connection, this makes us
			 * established */

#if defined(LWS_WITH_TLS)// && !defined(LWS_WITH_MBEDTLS)

			/* we have connected if we got here */

			if (lwsi_state(wsi) == LRS_WAITING_CONNECT &&
			    (wsi->tls.use_ssl & LCCSCF_USE_SSL)) {
				int result;

				//lwsi_set_state(wsi, LRS_WAITING_SSL);

				/*
				 * We can retry this... just cook the SSL BIO
				 * the first time
				 */

				result = lws_client_create_tls(wsi, &cce, 1);
				switch (result) {
				case CCTLS_RETURN_DONE:
					break;
				case CCTLS_RETURN_RETRY:
					lwsl_wsi_debug(wsi, "create_tls RETRY");
					return wsi;
				default:
					lwsl_wsi_debug(wsi, "create_tls FAIL");
					goto failed;
				}

				/*
				 * We succeeded to negotiate a new client tls
				 * tunnel.  If it's h2 alpn, we have arranged
				 * to send the h2 prefix and set our state to
				 * LRS_H2_WAITING_TO_SEND_HEADERS already.
				 */

				lwsl_wsi_notice(wsi, "tls established st 0x%x, "
					    "client_h2_alpn %d", lwsi_state(wsi),
					    wsi->client_h2_alpn);

				if (lwsi_state(wsi) !=
						LRS_H2_WAITING_TO_SEND_HEADERS)
					lwsi_set_state(wsi,
						LRS_H1C_ISSUE_HANDSHAKE2);
				lws_set_timeout(wsi,
					PENDING_TIMEOUT_AWAITING_CLIENT_HS_SEND,
					(int)wsi->a.context->timeout_secs);
#if 0
				/* ensure pollin enabled */
				if (lws_change_pollfd(wsi, 0, LWS_POLLIN))
					lwsl_wsi_notice(wsi,
							"unable to set POLLIN");
#endif

				goto provoke_service;
			}
#endif

			/* clear his established timeout */
			lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

			m = wsi->role_ops->adoption_cb[0];
			if (m) {
				n = user_callback_handle_rxflow(
						wsi->a.protocol->callback, wsi,
						(enum lws_callback_reasons)m,
						wsi->user_space, NULL, 0);
				if (n < 0) {
					lwsl_wsi_info(wsi, "RAW_PROXY_CLI_ADOPT err");
					goto failed;
				}
			}

			/* service.c pollout processing wants this */
			wsi->hdr_parsing_completed = 1;
#if defined(LWS_ROLE_MQTT)
			if (meth && !strcmp(meth, "MQTT")) {
#if defined(LWS_WITH_TLS)
				if (wsi->tls.use_ssl & LCCSCF_USE_SSL) {
					lwsi_set_state(wsi, LRS_WAITING_SSL);
					return wsi;
				}
#endif
				lwsl_wsi_info(wsi, "settings LRS_MQTTC_IDLE");
				lwsi_set_state(wsi, LRS_MQTTC_IDLE);

				/*
				 * provoke service to issue the CONNECT
				 * directly.
				 */
				lws_set_timeout(wsi,
					PENDING_TIMEOUT_SENT_CLIENT_HANDSHAKE,
						(int)wsi->a.context->timeout_secs);

				assert(lws_socket_is_valid(wsi->desc.sockfd));

				pfd.fd = wsi->desc.sockfd;
				pfd.events = LWS_POLLIN;
				pfd.revents = LWS_POLLOUT;

				lwsl_wsi_info(wsi, "going to service fd");
				n = lws_service_fd(wsi->a.context, &pfd);
				if (n < 0) {
					cce = "first service failed";
					goto failed;
				}
				if (n)
					/* returns 1 on fail after close wsi */
					return NULL;
				return wsi;
			}
#endif
			lwsl_wsi_info(wsi, "setting ESTABLISHED");
			lwsi_set_state(wsi, LRS_ESTABLISHED);

			return wsi;
		}

		/*
		 * provoke service to issue the handshake directly.
		 *
		 * we need to do it this way because in the proxy case, this is
		 * the next state and executed only if and when we get a good
		 * proxy response inside the state machine... but notice in
		 * SSL case this may not have sent anything yet with 0 return,
		 * and won't until many retries from main loop.  To stop that
		 * becoming endless, cover with a timeout.
		 */
#if defined(LWS_WITH_TLS) //&& !defined(LWS_WITH_MBEDTLS)
provoke_service:
#endif
		lws_set_timeout(wsi, PENDING_TIMEOUT_SENT_CLIENT_HANDSHAKE,
				(int)wsi->a.context->timeout_secs);

		assert(lws_socket_is_valid(wsi->desc.sockfd));

		pfd.fd = wsi->desc.sockfd;
		pfd.events = LWS_POLLIN;
		pfd.revents = LWS_POLLIN;

		n = lws_service_fd(wsi->a.context, &pfd);
		if (n < 0) {
			cce = "first service failed";
			goto failed;
		}
		if (n) /* returns 1 on failure after closing wsi */
			return NULL;
	}
#endif
	return wsi;

failed:
	lws_inform_client_conn_fail(wsi, (void *)cce, strlen(cce));

	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "client_connect4");

	return NULL;
}
