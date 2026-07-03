/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2021 Andy Green <andy@warmcat.com>
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
 *
 *
 * When the user code is in a different process, a non-tls unix domain socket
 * proxy is used to asynchronusly transfer buffers in each direction via the
 * network stack, without explicit IPC
 *
 *     user_process{ [user code] | shim | socket-}------ lws_process{ lws }
 *
 * Lws exposes a listening unix domain socket in this case, the user processes
 * connect to it and pass just info.streamtype in an initial tx packet.  All
 * packets are prepended by a 1-byte type field when used in this mode.  See
 * lws-secure-streams.h for documentation and definitions.
 *
 * Proxying in either direction can face the situation it cannot send the onward
 * packet immediately and is subject to separating the write request from the
 * write action.  To make the best use of memory, a single preallocated buffer
 * stashes pending packets in all four directions (c->p, p->c, p->ss, ss->p).
 * This allows it to adapt to different traffic patterns without wasted areas
 * dedicated to traffic that isn't coming in a particular application.
 *
 * A shim is provided to monitor the process' unix domain socket and regenerate
 * the secure sockets api there with callbacks happening in the process thread
 * context.
 *
 * This file implements the listening unix domain socket proxy... this code is
 * only going to run on a Linux-class device with its implications about memory
 * availability.
 */

#include <private-lib-core.h>

struct raw_pss {
	struct conn		*conn;
};

/*
 * Proxy - onward secure-stream handler
 */

typedef struct ss_proxy_onward {
	lws_ss_handle_t 	*ss;
	struct conn		*conn;
} ss_proxy_t;

void
lws_proxy_clean_conn_ss(struct lws *wsi)
{
#if 0
	lws_ss_handle_t *h = (lws_ss_handle_t *)wsi->a.opaque_user_data;
	struct conn *conn = h->conn_if_sspc_onw;

	if (!wsi)
		return;

	if (conn && conn->ss)
		conn->ss->wsi = NULL;
#endif
}


void
ss_proxy_onward_link_req_writeable(lws_ss_handle_t *h_onward)
{
	ss_proxy_t *m = (ss_proxy_t *)&h_onward[1];

	if (m->conn->wsi) /* if possible, request client conn write */
		lws_callback_on_writable(m->conn->wsi);
}

int
__lws_ss_proxy_bind_ss_to_conn_wsi(void *parconn, size_t dsh_size)
{
	struct conn *conn = (struct conn *)parconn;
	struct lws_context_per_thread *pt;

	if (!conn || !conn->wsi || !conn->ss)
		return -1;

	pt = &conn->wsi->a.context->pt[(int)conn->wsi->tsi];

	if (lws_fi(&conn->ss->fic, "ssproxy_dsh_create_oom"))
		return -1;
	conn->dsh = lws_dsh_create(&pt->ss_dsh_owner, dsh_size, 2);
	if (!conn->dsh)
		return -1;

	__lws_lc_tag_append(&conn->wsi->lc, lws_ss_tag(conn->ss));

	return 0;
}

/* Onward secure streams payload interface */

static lws_ss_state_return_t
ss_proxy_onward_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	ss_proxy_t *m = (ss_proxy_t *)userobj;
	const char *rsp = NULL;
	int n;

	// lwsl_notice("%s: len %d\n", __func__, (int)len);

	/*
	 * The onward secure stream connection has received something.
	 */

	if (m->ss->rideshare != m->ss->policy && m->ss->rideshare) {
		rsp = m->ss->rideshare->streamtype;
		flags |= LWSSS_FLAG_RIDESHARE;
	}

	/*
	 * Apply SSS framing around this chunk of RX and stash it in the dsh
	 * in ss -> proxy [ -> client] direction.  This can fail...
	 */

	if (lws_fi(&m->ss->fic, "ssproxy_dsh_rx_queue_oom"))
		n = 1;
	else
		n = lws_ss_serialize_rx_payload(m->conn->dsh, buf, len,
						flags, rsp);
	if (n)
		/*
		 * We couldn't buffer this rx, eg due to OOM, let's escalate it
		 * to be a "loss of connection", which it basically is...
		 */
		return LWSSSSRET_DISCONNECT_ME;

	/*
	 * Manage rx flow on the SS (onward) side according to our situation
	 * in the dsh holding proxy->client serialized forwarding rx
	 */

	if (!m->conn->onward_in_flow_control && m->ss->wsi &&
	    m->ss->policy->proxy_buflen_rxflow_on_above &&
	    lws_dsh_get_size(m->conn->dsh, KIND_SS_TO_P) >=
				m->ss->policy->proxy_buflen_rxflow_on_above) {
		lwsl_info("%s: %s: rxflow disabling rx (%lu / %lu, hwm %lu)\n", __func__,
				lws_wsi_tag(m->ss->wsi),
				(unsigned long)lws_dsh_get_size(m->conn->dsh, KIND_SS_TO_P),
				(unsigned long)m->ss->policy->proxy_buflen,
				(unsigned long)m->ss->policy->proxy_buflen_rxflow_on_above);
		/*
		 * stop taking in rx once the onward wsi rx is above the
		 * high water mark
		 */
		lws_rx_flow_control(m->ss->wsi, 0);
		m->conn->onward_in_flow_control = 1;
	}

	if (m->conn->wsi) /* if possible, request client conn write */
		lws_callback_on_writable(m->conn->wsi);

	return LWSSSSRET_OK;
}

/*
 * we are transmitting buffered payload originally from the client on to the ss
 */

static lws_ss_state_return_t
ss_proxy_onward_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf,
		   size_t *len, int *flags)
{
	ss_proxy_t *m = (ss_proxy_t *)userobj;
	void *p;
	size_t si;

	if (!m->conn->ss || m->conn->state != LPCSPROX_OPERATIONAL) {
		lwsl_notice("%s: ss not ready\n", __func__);
		*len = 0;

		return LWSSSSRET_TX_DONT_SEND;
	}

	/*
	 * The onward secure stream says that we could send something to it
	 * (by putting it in buf, and setting *len and *flags)... dredge the
	 * next thing out of the dsh
	 */

	if (lws_ss_deserialize_tx_payload(m->conn->dsh, m->ss->wsi,
					  ord, buf, len, flags))
		return LWSSSSRET_TX_DONT_SEND;

	/* ... there's more we want to send? */
	if (!lws_dsh_get_head(m->conn->dsh, KIND_C_TO_P, (void **)&p, &si))
		_lws_ss_request_tx(m->conn->ss);

	if (!*len && !*flags)
		/* we don't actually want to send anything */
		return LWSSSSRET_TX_DONT_SEND;

	lwsl_info("%s: onward tx %d fl 0x%x\n", __func__, (int)*len, *flags);

#if 0
	{
		int ff = open("/tmp/z", O_RDWR | O_CREAT | O_APPEND, 0666);
		if (ff == -1)
			lwsl_err("%s: errno %d\n", __func__, errno);
		write(ff, buf, *len);
		close(ff);
	}
#endif

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
ss_proxy_onward_state(void *userobj, void *sh,
		      lws_ss_constate_t state, lws_ss_tx_ordinal_t ack)
{
	ss_proxy_t *m = (ss_proxy_t *)userobj;
	size_t dsh_size;

	switch (state) {
	case LWSSSCS_CREATING:

		/*
		 * conn is private to -process.c, call thru to a) adjust
		 * the accepted incoming proxy link wsi tag name to be
		 * appended with the onward ss tag information now we
		 * have it, and b) allocate the dsh buffer now we
		 * can find out the policy about it for the streamtype.
		 */

		dsh_size = m->ss->policy->proxy_buflen ?
				m->ss->policy->proxy_buflen : 32768;

		lwsl_notice("%s: %s: initializing dsh max len %lu\n",
				__func__, lws_ss_tag(m->ss),
				(unsigned long)dsh_size);

		/* this includes ssproxy_dsh_create_oom fault generation */

		if (__lws_ss_proxy_bind_ss_to_conn_wsi(m->conn, dsh_size)) {

			/* failed to allocate the dsh */

			lwsl_notice("%s: dsh init failed\n", __func__);

			return LWSSSSRET_DESTROY_ME;
		}
		break;

	case LWSSSCS_DESTROYING:
		if (!m->conn)
			break;
		if (!m->conn->wsi) {
			/*
			 * Our onward secure stream is closing and our client
			 * connection has already gone away... destroy the conn.
			 */
			lwsl_info("%s: Destroying conn\n", __func__);
			lws_dsh_destroy(&m->conn->dsh);
			free(m->conn);
			m->conn = NULL;
			return 0;
		} else
			lwsl_info("%s: ss DESTROYING, wsi up\n", __func__);
		break;

	default:
		break;
	}
	if (!m->conn) {
		lwsl_warn("%s: dropping state due to conn not up\n", __func__);

		return LWSSSSRET_OK;
	}

	if (lws_ss_serialize_state(m->conn->wsi, m->conn->dsh, state, ack))
		/*
		 * Failed to alloc state packet that we want to send in dsh,
		 * we will lose coherence and have to disconnect the link
		 */
		return LWSSSSRET_DISCONNECT_ME;

	if (m->conn->wsi) /* if possible, request client conn write */
		lws_callback_on_writable(m->conn->wsi);

	return LWSSSSRET_OK;
}

void
ss_proxy_onward_txcr(void *userobj, int bump)
{
	ss_proxy_t *m = (ss_proxy_t *)userobj;

	if (!m->conn)
		return;

	lws_ss_serialize_txcr(m->conn->dsh, bump);

	if (m->conn->wsi) /* if possible, request client conn write */
		lws_callback_on_writable(m->conn->wsi);
}

/*
 * Client <-> Proxy connection, usually on Unix Domain Socket
 */

static int
callback_ss_proxy(struct lws *wsi, enum lws_callback_reasons reason,
		  void *user, void *in, size_t len)
{
	struct raw_pss *pss = (struct raw_pss *)user;
	const lws_ss_policy_t *rsp;
	struct conn *conn = NULL;
	lws_ss_metadata_t *md;
	lws_ss_info_t ssi;
	const uint8_t *cp;
	char s[512];
	uint8_t *p;
	size_t si;
	char pay;
	int n;

	if (pss)
		conn = pss->conn;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		break;

	/* callbacks related to raw socket descriptor "accepted side" */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_info("LWS_CALLBACK_RAW_ADOPT\n");
		if (!pss)
			return -1;

		if (lws_fi(&wsi->fic, "ssproxy_client_adopt_oom"))
			pss->conn = NULL;
		else
			pss->conn = malloc(sizeof(struct conn));
		if (!pss->conn)
			return -1;

		memset(pss->conn, 0, sizeof(*pss->conn));

		/* dsh is allocated when the onward ss is done */

		pss->conn->wsi = wsi;
		wsi->bound_ss_proxy_conn = 1; /* opaque is conn */

		pss->conn->state = LPCSPROX_WAIT_INITIAL_TX;

		/*
		 * Client is expected to follow the unix domain socket
		 * acceptance up rapidly with an initial tx containing the
		 * streamtype name.  We can't create the stream until then.
		 */
		lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_CLIENT_HS_SEND, 3);
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_info("LWS_CALLBACK_RAW_CLOSE:\n");

		if (!conn)
			break;

		/*
		 * the client unix domain socket connection (wsi / conn->wsi)
		 * has closed... eg, client has exited or otherwise has
		 * definitively finished with the proxying and onward connection
		 *
		 * But right now, the SS and possibly the SS onward wsi are
		 * still live...
		 */

		assert(conn->wsi == wsi);
		conn->wsi = NULL;

		lwsl_notice("%s: cli->prox link %s closing\n", __func__,
				lws_wsi_tag(wsi));

		/* sever relationship with conn */
		lws_set_opaque_user_data(wsi, NULL);

		/*
		 * The current wsi is decoupled from the pss / conn and
		 * the conn no longer has a pointer on it.
		 *
		 * If there's an outgoing, proxied SS conn on our behalf, we
		 * have to destroy those
		 */

		if (conn->ss) {
			struct lws *cw = conn->ss->wsi;
			/*
			 * conn->ss is the onward connection SS
			 */

			lwsl_info("%s: destroying %s, wsi %s\n",
					__func__, lws_ss_tag(conn->ss),
					lws_wsi_tag(conn->ss->wsi));

			/* sever conn relationship with ss about to be deleted */

			conn->ss->wsi = NULL;

			if (cw && wsi != cw) {

				/* disconnect onward SS from its wsi */

				lws_set_opaque_user_data(cw, NULL);

				/*
				 * The wsi doing the onward connection can no
				 * longer relate to the conn... otherwise when
				 * he gets callbacks he wants to bind to
				 * the ss we are about to delete
				 */
				lws_wsi_close(cw, LWS_TO_KILL_ASYNC);
			}

			lws_ss_destroy(&conn->ss);
			/*
			 * Conn may have gone, at ss destroy handler in
			 * ssi.state for proxied ss
			 */
			break;
		}

		if (conn->state == LPCSPROX_DESTROYED || !conn->ss) {
			/*
			 * There's no onward secure stream and our client
			 * connection is closing.  Destroy the conn.
			 */
			lws_dsh_destroy(&conn->dsh);
			free(conn);
			pss->conn = NULL;
		} else
			lwsl_debug("%s: CLOSE; %s\n", __func__, lws_ss_tag(conn->ss));

		break;

	case LWS_CALLBACK_RAW_RX:
		/*
		 * ie, the proxy is receiving something from a client
		 */
		lwsl_info("%s: RX: rx %d\n", __func__, (int)len);

		if (!conn || !conn->wsi) {
			lwsl_err("%s: rx with bad conn state\n", __func__);

			return -1;
		}

		// lwsl_hexdump_info(in, len);

		if (conn->state == LPCSPROX_WAIT_INITIAL_TX) {
			memset(&ssi, 0, sizeof(ssi));
			ssi.user_alloc = sizeof(ss_proxy_t);
			ssi.handle_offset = offsetof(ss_proxy_t, ss);
			ssi.opaque_user_data_offset =
					offsetof(ss_proxy_t, conn);
			ssi.rx = ss_proxy_onward_rx;
			ssi.tx = ss_proxy_onward_tx;
		}
		ssi.state = ss_proxy_onward_state;
		ssi.flags = 0;

		// coverity[uninit_use_in_call]
		n = lws_ss_deserialize_parse(&conn->parser,
				lws_get_context(wsi), conn->dsh, in, len,
				&conn->state, conn, &conn->ss, &ssi, 0);
		switch (n) {
		case LWSSSSRET_OK:
			break;
		case LWSSSSRET_DISCONNECT_ME:
			return -1;
		case LWSSSSRET_DESTROY_ME:
			if (conn->ss)
				lws_ss_destroy(&conn->ss);
			return -1;
		}

		if (conn->state == LPCSPROX_REPORTING_FAIL ||
		    conn->state == LPCSPROX_REPORTING_OK)
			lws_callback_on_writable(conn->wsi);

		break;

	case LWS_CALLBACK_RAW_WRITEABLE:

		lwsl_debug("%s: %s: LWS_CALLBACK_RAW_WRITEABLE, state 0x%x\n",
				__func__, lws_wsi_tag(wsi), lwsi_state(wsi));

		/*
		 * We can transmit something back to the client from the dsh
		 * of stuff we received on its behalf from the ss
		 */

		if (!conn || !conn->wsi)
			break;

		n = 0;
		pay = 0;

		s[3] = 0;
		cp = (const uint8_t *)s;
		switch (conn->state) {
		case LPCSPROX_REPORTING_FAIL:
			s[3] = 1;
			/* fallthru */
		case LPCSPROX_REPORTING_OK:
			s[0] = LWSSS_SER_RXPRE_CREATE_RESULT;
			s[1] = 0;
			s[2] = 1;

			n = 8;

			lws_ser_wu32be((uint8_t *)&s[4], conn->ss &&
							 conn->ss->policy ?
					conn->ss->policy->client_buflen : 0);

			/*
			 * If there's rideshare sequencing, it's added after the
			 * first 4 bytes or the create result, comma-separated
			 */

			if (conn->ss) {
				rsp = conn->ss->policy;

				while (rsp) {
					if (n != 4 && n < (int)sizeof(s) - 2)
						s[n++] = ',';
					n += lws_snprintf(&s[n], sizeof(s) - (unsigned int)n,
							"%s", rsp->streamtype);
					rsp = lws_ss_policy_lookup(wsi->a.context,
						rsp->rideshare_streamtype);
				}
			}
			s[2] = (char)(n - 3);
			conn->state = LPCSPROX_OPERATIONAL;
			lws_set_timeout(wsi, 0, 0);
			break;

		case LPCSPROX_OPERATIONAL:

			/*
			 * returning [onward -> ] proxy]-> client
			 * rx metadata has priority 1
			 */

			md = conn->ss->metadata;
			while (md) {
				// lwsl_notice("%s: check %s: %d\n", __func__,
				// md->name, md->pending_onward);
				if (md->pending_onward) {
					size_t naml = strlen(md->name);

					// lwsl_notice("%s: proxy issuing rxmd\n", __func__);

					if (4 + naml + md->length > sizeof(s)) {
						lwsl_err("%s: rxmdata too big\n",
								__func__);
						goto hangup;
					}
					md->pending_onward = 0;
					p = (uint8_t *)s;
					p[0] = LWSSS_SER_RXPRE_METADATA;
					lws_ser_wu16be(&p[1], (uint16_t)(1 + naml +
							      md->length));
					p[3] = (uint8_t)naml;
					memcpy(&p[4], md->name, naml);
					p += 4 + naml;
					memcpy(p, md->value__may_own_heap,
					       md->length);
					p += md->length;

					n = lws_ptr_diff(p, cp);
					goto again;
				}

				md = md->next;
			}

			/*
			 * If we have performance data, render it in JSON
			 * and send that in LWSSS_SER_RXPRE_PERF has
			 * priority 2
			 */

#if defined(LWS_WITH_CONMON)
			if (conn->ss->conmon_json) {
				unsigned int xlen = conn->ss->conmon_len;

				if (xlen > sizeof(s) - 3)
					xlen = sizeof(s) - 3;
				cp = (uint8_t *)s;
				p = (uint8_t *)s;
				p[0] = LWSSS_SER_RXPRE_PERF;
				lws_ser_wu16be(&p[1], (uint16_t)xlen);
				memcpy(&p[3], conn->ss->conmon_json, xlen);

				lws_free_set_NULL(conn->ss->conmon_json);
				n = (int)(xlen + 3);

				pay = 0;
				goto again;
			}
#endif
			/*
			 * if no fresh rx metadata, just pass through incoming
			 * dsh
			 */

			if (lws_dsh_get_head(conn->dsh, KIND_SS_TO_P,
					     (void **)&p, &si))
				break;

			cp = p;

#if 0
			if (cp[0] == LWSSS_SER_RXPRE_RX_PAYLOAD &&
			    wsi->a.context->detailed_latency_cb) {

				/*
				 * we're fulfilling rx that came in on ss
				 * by sending it back out to the client on
				 * the Unix Domain Socket
				 *
				 * +  7  u32  write will compute latency here...
				 * + 11  u32  ust we received from ss
				 *
				 * lws_write will report it and fill in
				 * LAT_DUR_PROXY_CLIENT_REQ_TO_WRITE
				 */

				us = lws_now_usecs();
				lws_ser_wu32be(&p[7], us -
						      lws_ser_ru64be(&p[11]));
				lws_ser_wu64be(&p[11], us);

				wsi->detlat.acc_size =
					wsi->detlat.req_size = si - 19;
				/* time proxy held it */
				wsi->detlat.latencies[
				            LAT_DUR_PROXY_RX_TO_ONWARD_TX] =
							lws_ser_ru32be(&p[7]);
			}
#endif
			pay = 1;
			n = (int)si;
			break;
		default:
			break;
		}
again:
		if (!n)
			break;

		if (lws_fi(&wsi->fic, "ssproxy_client_write_fail"))
			n = -1;
		else
			n = lws_write(wsi, (uint8_t *)cp, (unsigned int)n, LWS_WRITE_RAW);
		if (n < 0) {
			lwsl_info("%s: WRITEABLE: %d\n", __func__, n);

			goto hangup;
		}

		switch (conn->state) {
		case LPCSPROX_REPORTING_FAIL:
			goto hangup;
		case LPCSPROX_OPERATIONAL:
			if (!conn)
				break;
			if (pay) {
				lws_dsh_free((void **)&p);

				/*
				 * Did we go below the rx flow threshold for
				 * this dsh?
				 */

				if (conn->onward_in_flow_control &&
				    conn->ss->policy->proxy_buflen_rxflow_on_above &&
				    conn->ss->wsi &&
				    lws_dsh_get_size(conn->dsh, KIND_SS_TO_P) <
				      conn->ss->policy->proxy_buflen_rxflow_off_below) {
					lwsl_info("%s: %s: rxflow enabling rx (%lu / %lu, lwm %lu)\n", __func__,
							lws_wsi_tag(conn->ss->wsi),
							(unsigned long)lws_dsh_get_size(conn->dsh, KIND_SS_TO_P),
							(unsigned long)conn->ss->policy->proxy_buflen,
							(unsigned long)conn->ss->policy->proxy_buflen_rxflow_off_below);
					/*
					 * Resume receiving taking in rx once
					 * below the low threshold
					 */
					lws_rx_flow_control(conn->ss->wsi,
							    LWS_RXFLOW_ALLOW);
					conn->onward_in_flow_control = 0;
				}
			}
			if (!lws_dsh_get_head(conn->dsh, KIND_SS_TO_P,
					     (void **)&p, &si)) {
				if (!lws_send_pipe_choked(wsi)) {
					cp = p;
					pay = 1;
					n = (int)si;
					goto again;
				}
				lws_callback_on_writable(wsi);
			}
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);

hangup:
	/* hang up on him */

	return -1;
}

static const struct lws_protocols protocols[] = {
	{
		"ssproxy-protocol",
		callback_ss_proxy,
		sizeof(struct raw_pss),
		2048, 2048, NULL, 0
	},
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

/*
 * called from create_context()
 */

int
lws_ss_proxy_create(struct lws_context *context, const char *bind, int port)
{
	struct lws_context_creation_info info;

	memset(&info, 0, sizeof(info));

	info.vhost_name			= "ssproxy";
	info.options = LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG |
			LWS_SERVER_OPTION_SS_PROXY;
	info.port = port;
	if (!port) {
		if (!bind)
#if defined(__linux__)
			bind = "@proxy.ss.lws";
#else
			bind = "/tmp/proxy.ss.lws";
#endif
		info.options |= LWS_SERVER_OPTION_UNIX_SOCK;
	}
	info.iface			= bind;
#if defined(__linux__)
	info.unix_socket_perms		= "root:root";
#else
#endif
	info.listen_accept_role		= "raw-skt";
	info.listen_accept_protocol	= "ssproxy-protocol";
	info.protocols			= protocols;

	if (!lws_create_vhost(context, &info)) {
		lwsl_err("%s: Failed to create ss proxy vhost\n", __func__);

		return 1;
	}

	return 0;
}
