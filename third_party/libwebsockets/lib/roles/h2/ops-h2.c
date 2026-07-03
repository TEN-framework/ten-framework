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

/*
 * These are the standardized defaults.
 * Override what actually goes in the vhost settings in platform or user code.
 * Leave these alone because they are used to determine "what is different
 * from the protocol defaults".
 */
const struct http2_settings lws_h2_defaults = { {
	1,
	/* H2SET_HEADER_TABLE_SIZE */			4096,
	/* *** This controls how many entries in the dynamic table ***
	 * Allows the sender to inform the remote endpoint of the maximum
	 * size of the header compression table used to decode header
	 * blocks, in octets.  The encoder can select any size equal to or
	 * less than this value by using signaling specific to the header
	 * compression format inside a header block (see [COMPRESSION]).
	 * The initial value is 4,096 octets.
	 */
	/* H2SET_ENABLE_PUSH */				   1,
	/* H2SET_MAX_CONCURRENT_STREAMS */	  0x7fffffff,
	/* H2SET_INITIAL_WINDOW_SIZE */		       65535,
	/* H2SET_MAX_FRAME_SIZE */		       16384,
	/* H2SET_MAX_HEADER_LIST_SIZE */	  0x7fffffff,
	/*< This advisory setting informs a peer of the maximum size of
	 * header list that the sender is prepared to accept, in octets.
	 * The value is based on the uncompressed size of header fields,
	 * including the length of the name and value in octets plus an
	 * overhead of 32 octets for each header field.
	 */
	/* H2SET_RESERVED7 */				   0,
	/* H2SET_ENABLE_CONNECT_PROTOCOL */		   0,
}};

/* these are the "lws defaults"... they can be overridden in plat */

const struct http2_settings lws_h2_stock_settings = { {
	1,
	/* H2SET_HEADER_TABLE_SIZE */			65536, /* ffox */
	/* *** This controls how many entries in the dynamic table ***
	 * Allows the sender to inform the remote endpoint of the maximum
	 * size of the header compression table used to decode header
	 * blocks, in octets.  The encoder can select any size equal to or
	 * less than this value by using signaling specific to the header
	 * compression format inside a header block (see [COMPRESSION]).
	 * The initial value is 4,096 octets.
	 *
	 * Can't pass h2spec with less than 4096 here...
	 */
	/* H2SET_ENABLE_PUSH */				   0,
	/* H2SET_MAX_CONCURRENT_STREAMS */		  24,
	/* H2SET_INITIAL_WINDOW_SIZE */		           0,
	/*< This is managed by explicit WINDOW_UPDATE.  Because otherwise no
	 * way to precisely control it when we do want to.
	 */
	/* H2SET_MAX_FRAME_SIZE */		       16384,
	/* H2SET_MAX_HEADER_LIST_SIZE */	        4096,
	/*< This advisory setting informs a peer of the maximum size of
	 * header list that the sender is prepared to accept, in octets.
	 * The value is based on the uncompressed size of header fields,
	 * including the length of the name and value in octets plus an
	 * overhead of 32 octets for each header field.
	 */
	/* H2SET_RESERVED7 */				   0,
	/* H2SET_ENABLE_CONNECT_PROTOCOL */		   1,
}};

/*
 * The wsi at this level is normally the network wsi... we can get called on
 * another path via lws_service_do_ripe_rxflow() on mux children too tho...
 */

static int
rops_handle_POLLIN_h2(struct lws_context_per_thread *pt, struct lws *wsi,
		       struct lws_pollfd *pollfd)
{
	struct lws_tokens ebuf;
	unsigned int pending = 0;
	char buffered = 0;
	struct lws *wsi1;
	int n, m;

#ifdef LWS_WITH_CGI
	if (wsi->http.cgi && (pollfd->revents & LWS_POLLOUT)) {
		if (lws_handle_POLLOUT_event(wsi, pollfd))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;

		return LWS_HPI_RET_HANDLED;
	}
#endif

	 lwsl_info("%s: %s wsistate 0x%x, events %d, revents %d, pollout %d\n", __func__,
		   wsi->lc.gutag, (unsigned int)wsi->wsistate,
		   pollfd->events, pollfd->revents,
		   pollfd->revents & LWS_POLLOUT);

	 /* !!! */
	 if (wsi->wsistate == 0x10000013) {
		 wsi->bugcatcher++;
		 if (wsi->bugcatcher == 250) {
			 lwsl_err("%s: BUGCATCHER\n", __func__);
			 return LWS_HPI_RET_PLEASE_CLOSE_ME;
		 }
	 } else
		 wsi->bugcatcher = 0;

	/*
	 * something went wrong with parsing the handshake, and
	 * we ended up back in the event loop without completing it
	 */
	if (lwsi_state(wsi) == LRS_PRE_WS_SERVING_ACCEPT) {
		wsi->socket_is_permanently_unusable = 1;
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	if (lwsi_state(wsi) == LRS_WAITING_CONNECT) {
#if defined(LWS_WITH_CLIENT)
		if ((pollfd->revents & LWS_POLLOUT) &&
		    lws_handle_POLLOUT_event(wsi, pollfd)) {
			lwsl_debug("POLLOUT event closed it\n");
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		n = lws_http_client_socket_service(wsi, pollfd);
		if (n)
			return LWS_HPI_RET_WSI_ALREADY_DIED;
#endif
		return LWS_HPI_RET_HANDLED;
	}

	/* 1: something requested a callback when it was OK to write */

	if ((pollfd->revents & LWS_POLLOUT) &&
	    lwsi_state_can_handle_POLLOUT(wsi) &&
	    lws_handle_POLLOUT_event(wsi, pollfd)) {
		if (lwsi_state(wsi) == LRS_RETURNED_CLOSE)
			lwsi_set_state(wsi, LRS_FLUSHING_BEFORE_CLOSE);
		/* the write failed... it's had it */
		wsi->socket_is_permanently_unusable = 1;

		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	if (lwsi_state(wsi) == LRS_RETURNED_CLOSE ||
	    lwsi_state(wsi) == LRS_WAITING_TO_SEND_CLOSE ||
	    lwsi_state(wsi) == LRS_AWAITING_CLOSE_ACK) {
		/*
		 * we stopped caring about anything except control
		 * packets.  Force flow control off, defeat tx
		 * draining.
		 */
		lws_rx_flow_control(wsi, 1);
#if defined(LWS_ROLE_WS) && !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws)
			wsi->ws->tx_draining_ext = 0;
#endif
	}

	if (wsi->mux_substream || wsi->upgraded_to_http2) {
		wsi1 = lws_get_network_wsi(wsi);
		if (wsi1 && lws_has_buffered_out(wsi1)) {

			lwsl_info("%s: has buffered out\n", __func__);
			/*
			 * We cannot deal with any kind of new RX
			 * because we are dealing with a partial send
			 * (new RX may trigger new http_action() that
			 * expect to be able to send)
			 */
			return LWS_HPI_RET_HANDLED;
		}
	}

read:
	/* 3: network wsi buflist needs to be drained */

	// lws_buflist_describe(&wsi->buflist, wsi, __func__);

	ebuf.len = (int)lws_buflist_next_segment_len(&wsi->buflist,
						&ebuf.token);
	if (ebuf.len) {
		lwsl_info("draining buflist (len %d)\n", ebuf.len);
		buffered = 1;
		goto drain;
	} else {

		if (wsi->mux_substream) {
			lwsl_warn("%s: uh... %s mux child with nothing to drain\n", __func__, lws_wsi_tag(wsi));
			// assert(0);
			lws_dll2_remove(&wsi->dll_buflist);
			return LWS_HPI_RET_HANDLED;
		}
	}

	if (!lws_ssl_pending(wsi) &&
	    !(pollfd->revents & pollfd->events & LWS_POLLIN))
		return LWS_HPI_RET_HANDLED;

	/* We have something to read... */

	if (!(lwsi_role_client(wsi) &&
	      (lwsi_state(wsi) != LRS_ESTABLISHED &&
	       // lwsi_state(wsi) != LRS_H1C_ISSUE_HANDSHAKE2 &&
	       lwsi_state(wsi) != LRS_H2_WAITING_TO_SEND_HEADERS))) {

		ebuf.token = pt->serv_buf;
		ebuf.len = lws_ssl_capable_read(wsi,
					ebuf.token,
					wsi->a.context->pt_serv_buf_size);
		switch (ebuf.len) {
		case 0:
			lwsl_info("%s: zero length read\n", __func__);
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		case LWS_SSL_CAPABLE_MORE_SERVICE:
			lwsl_info("SSL Capable more service\n");
			return LWS_HPI_RET_HANDLED;
		case LWS_SSL_CAPABLE_ERROR:
			lwsl_info("%s: LWS_SSL_CAPABLE_ERROR\n", __func__);
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		// lwsl_notice("%s: Actual RX %d\n", __func__, ebuf.len);
		// if (ebuf.len > 0)
		//	lwsl_hexdump_notice(ebuf.token, ebuf.len);
	} else
		lwsl_info("%s: skipped read\n", __func__);

	if (ebuf.len < 0)
		return LWS_HPI_RET_PLEASE_CLOSE_ME;

drain:
#if defined(LWS_WITH_CLIENT)
	if (lwsi_role_http(wsi) && lwsi_role_client(wsi) &&
	    wsi->hdr_parsing_completed && !wsi->told_user_closed) {

		/*
		 * In SSL mode we get POLLIN notification about
		 * encrypted data in.
		 *
		 * But that is not necessarily related to decrypted
		 * data out becoming available; in may need to perform
		 * other in or out before that happens.
		 *
		 * simply mark ourselves as having readable data
		 * and turn off our POLLIN
		 */
		wsi->client_rx_avail = 1;
		if (lws_change_pollfd(wsi, LWS_POLLIN, 0))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;

		/* let user code know, he'll usually ask for writeable
		 * callback and drain / re-enable it there
		 */
		if (user_callback_handle_rxflow(
				wsi->a.protocol->callback,
				wsi, LWS_CALLBACK_RECEIVE_CLIENT_HTTP,
				wsi->user_space, NULL, 0)) {
			lwsl_info("RECEIVE_CLIENT_HTTP closed it\n");
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		return LWS_HPI_RET_HANDLED;
	}
#endif

	/* service incoming data */

	if (ebuf.len) {
		n = 0;
		if (lwsi_role_h2(wsi) && lwsi_state(wsi) != LRS_BODY &&
		    lwsi_state(wsi) != LRS_DISCARD_BODY)
			n = lws_read_h2(wsi, ebuf.token, (unsigned int)ebuf.len);
		else
			n = lws_read_h1(wsi, ebuf.token, (unsigned int)ebuf.len);

		if (n < 0) {
			/* we closed wsi */
			return LWS_HPI_RET_WSI_ALREADY_DIED;
		}

		if (n && buffered) {
			// lwsl_notice("%s: h2 use %d\n", __func__, n);
			m = (int)lws_buflist_use_segment(&wsi->buflist, (size_t)n);
			lwsl_info("%s: draining rxflow: used %d, next %d\n",
				    __func__, n, m);
			if (!m) {
				lwsl_notice("%s: removed %s from dll_buflist\n",
					    __func__, lws_wsi_tag(wsi));
				lws_dll2_remove(&wsi->dll_buflist);
			}
		} else
			if (n && n < ebuf.len && ebuf.len > 0) {
				// lwsl_notice("%s: h2 append seg %d\n", __func__, ebuf.len - n);
				m = lws_buflist_append_segment(&wsi->buflist,
						ebuf.token + n,
						(unsigned int)(ebuf.len - n));
				if (m < 0)
					return LWS_HPI_RET_PLEASE_CLOSE_ME;
				if (m) {
					lwsl_debug("%s: added %s to rxflow list\n",
						   __func__, lws_wsi_tag(wsi));
					if (lws_dll2_is_detached(&wsi->dll_buflist))
						lws_dll2_add_head(&wsi->dll_buflist,
							 &pt->dll_buflist_owner);
				}
			}
	}

	// lws_buflist_describe(&wsi->buflist, wsi, __func__);

#if 0

	/*
	 * This seems to be too aggressive... we don't want the ah stuck
	 * there but eg, WINDOW_UPDATE may come and detach it if we leave
	 * it like that... it will get detached at stream close
	 */

	if (wsi->http.ah
#if defined(LWS_WITH_CLIENT)
			&& !wsi->client_h2_alpn
#endif
			) {
		lwsl_err("xxx\n");

		lws_header_table_detach(wsi, 0);
	}
#endif

	pending = (unsigned int)lws_ssl_pending(wsi);
	if (pending) {
		// lwsl_info("going around\n");
		goto read;
	}

	return LWS_HPI_RET_HANDLED;
}

int rops_handle_POLLOUT_h2(struct lws *wsi)
{
	// lwsl_notice("%s\n", __func__);

	if (lwsi_state(wsi) == LRS_ISSUE_HTTP_BODY)
		return LWS_HP_RET_USER_SERVICE;

	/*
	 * Priority 1: H2 protocol packets
	 */
	if ((wsi->upgraded_to_http2
#if defined(LWS_WITH_CLIENT)
			|| wsi->client_h2_alpn
#endif
			) && wsi->h2.h2n->pps) {
		lwsl_info("servicing pps\n");
		/*
		 * this is called on the network connection, but may close
		 * substreams... that may affect callers
		 */
		if (lws_h2_do_pps_send(wsi)) {
			wsi->socket_is_permanently_unusable = 1;
			return LWS_HP_RET_BAIL_DIE;
		}
		if (wsi->h2.h2n->pps)
			return LWS_HP_RET_BAIL_OK;

		/* we can resume whatever we were doing */
		lws_rx_flow_control(wsi, LWS_RXFLOW_REASON_APPLIES_ENABLE |
					 LWS_RXFLOW_REASON_H2_PPS_PENDING);

		return LWS_HP_RET_BAIL_OK; /* leave POLLOUT active */
	}

	/* Priority 2: if we are closing, not allowed to send more data frags
	 *	       which means user callback or tx ext flush banned now
	 */
	if (lwsi_state(wsi) == LRS_RETURNED_CLOSE)
		return LWS_HP_RET_USER_SERVICE;

	return LWS_HP_RET_USER_SERVICE;
}

static int
rops_write_role_protocol_h2(struct lws *wsi, unsigned char *buf, size_t len,
			    enum lws_write_protocol *wp)
{
	unsigned char flags = 0, base = (*wp) & 0x1f;
	size_t olen = len;
	int n;
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	unsigned char mtubuf[4096 + LWS_PRE];
#endif

	/* if not in a state to send stuff, then just send nothing */

	if (!lwsi_role_ws(wsi) && !wsi->mux_stream_immortal &&
	    base != LWS_WRITE_HTTP &&
	    base != LWS_WRITE_HTTP_FINAL &&
	    base != LWS_WRITE_HTTP_HEADERS_CONTINUATION &&
	    base != LWS_WRITE_HTTP_HEADERS && lwsi_state(wsi) != LRS_BODY &&
	    ((lwsi_state(wsi) != LRS_RETURNED_CLOSE &&
	      lwsi_state(wsi) != LRS_WAITING_TO_SEND_CLOSE &&
	      lwsi_state(wsi) != LRS_ESTABLISHED &&
	      lwsi_state(wsi) != LRS_AWAITING_CLOSE_ACK)
#if defined(LWS_ROLE_WS)
	   || base != LWS_WRITE_CLOSE
#endif
	)) {
		//assert(0);
		lwsl_notice("%s: binning wsistate 0x%x %d: %s\n", __func__,
				(unsigned int)wsi->wsistate, *wp, wsi->a.protocol ?
					wsi->a.protocol->name : "no protocol");

		return 0;
	}

	/* compression transform... */

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	if (wsi->http.lcs) {
		unsigned char *out = mtubuf + LWS_PRE;
		size_t o = sizeof(mtubuf) - LWS_PRE;

		n = lws_http_compression_transform(wsi, buf, len, wp, &out, &o);
		if (n)
			return n;

		lwsl_info("%s: %s: transformed %d bytes to %d "
			   "(wp 0x%x, more %d)\n", __func__,
			   lws_wsi_tag(wsi), (int)len, (int)o, (int)*wp,
			   wsi->http.comp_ctx.may_have_more);

		buf = out;
		len = o;
		base = (*wp) & 0x1f;

		if (!len)
			return (int)olen;
	}
#endif

	/*
	 * ws-over-h2 also ends up here after the ws framing applied
	 */

	n = LWS_H2_FRAME_TYPE_DATA;
	if (base == LWS_WRITE_HTTP_HEADERS) {
		n = LWS_H2_FRAME_TYPE_HEADERS;
		if (!((*wp) & LWS_WRITE_NO_FIN))
			flags = LWS_H2_FLAG_END_HEADERS;
		if (wsi->h2.send_END_STREAM ||
		    ((*wp) & LWS_WRITE_H2_STREAM_END)) {
			flags |= LWS_H2_FLAG_END_STREAM;
			wsi->h2.send_END_STREAM = 1;
		}
	}

	if (base == LWS_WRITE_HTTP_HEADERS_CONTINUATION) {
		n = LWS_H2_FRAME_TYPE_CONTINUATION;
		if (!((*wp) & LWS_WRITE_NO_FIN))
			flags = LWS_H2_FLAG_END_HEADERS;
		if (wsi->h2.send_END_STREAM ||
		    ((*wp) & LWS_WRITE_H2_STREAM_END)) {
			flags |= LWS_H2_FLAG_END_STREAM;
			wsi->h2.send_END_STREAM = 1;
		}
	}

	if ((base == LWS_WRITE_HTTP ||
	     base == LWS_WRITE_HTTP_FINAL) &&
	     wsi->http.tx_content_length) {
		wsi->http.tx_content_remain -= len;
		lwsl_info("%s: %s: tx_content_rem = %llu\n", __func__,
			  lws_wsi_tag(wsi),
			  (unsigned long long)wsi->http.tx_content_remain);
		if (!wsi->http.tx_content_remain) {
			lwsl_info("%s: selecting final write mode\n", __func__);
			base = *wp = LWS_WRITE_HTTP_FINAL;
		}
	}

	if (base == LWS_WRITE_HTTP_FINAL || ((*wp) & LWS_WRITE_H2_STREAM_END)) {
		flags |= LWS_H2_FLAG_END_STREAM;
		lwsl_info("%s: %s: setting END_STREAM, 0x%x\n", __func__,
				lws_wsi_tag(wsi), flags);
		wsi->h2.send_END_STREAM = 1;
	}

	n = lws_h2_frame_write(wsi, n, flags, wsi->mux.my_sid, (unsigned int)len, buf);
	if (n < 0)
		return n;

	/* hide it may have been compressed... */

	return (int)olen;
}

#if defined(LWS_WITH_SERVER)
static int
rops_check_upgrades_h2(struct lws *wsi)
{
#if defined(LWS_ROLE_WS)
	char *p;

	/*
	 * with H2 there's also a way to upgrade a stream to something
	 * else... :method is CONNECT and :protocol says the name of
	 * the new protocol we want to carry.  We have to have sent a
	 * SETTINGS saying that we support it though.
	 */
	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD);
	if (!wsi->a.vhost->h2.set.s[H2SET_ENABLE_CONNECT_PROTOCOL] ||
	    !wsi->mux_substream || !p || strcmp(p, "CONNECT"))
		return LWS_UPG_RET_CONTINUE;

	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_COLON_PROTOCOL);
	if (!p || strcmp(p, "websocket"))
		return LWS_UPG_RET_CONTINUE;

	lwsl_info("Upgrade h2 to ws\n");
	lws_mux_mark_immortal(wsi);
	wsi->h2_stream_carries_ws = 1;

	lws_metrics_tag_wsi_add(wsi, "upg", "ws_over_h2");

	if (lws_process_ws_upgrade(wsi))
		return LWS_UPG_RET_BAIL;

	lwsl_info("Upgraded h2 to ws OK\n");

	return LWS_UPG_RET_DONE;
#else
	return LWS_UPG_RET_CONTINUE;
#endif
}
#endif

static int
rops_init_vhost_h2(struct lws_vhost *vh,
		   const struct lws_context_creation_info *info)
{
	vh->h2.set = vh->context->set;
	if (info->http2_settings[0]) {
		int n;

		for (n = 1; n < LWS_H2_SETTINGS_LEN; n++)
			vh->h2.set.s[n] = info->http2_settings[n];
	}

	return 0;
}

int
rops_pt_init_destroy_h2(struct lws_context *context,
		    const struct lws_context_creation_info *info,
		    struct lws_context_per_thread *pt, int destroy)
{
	/* if not already set by plat, use lws default SETTINGS */
	if (!context->set.s[0])
		context->set = lws_h2_stock_settings;

	/*
	 * We only want to do this once... we will do it if we are built
	 * otherwise h1 ops will do it (or nobody if no http at all)
	 */
#if !defined(LWS_ROLE_H2) && defined(LWS_WITH_SERVER)
	if (!destroy) {

		pt->sul_ah_lifecheck.cb = lws_sul_http_ah_lifecheck;

		__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
				 &pt->sul_ah_lifecheck, 30 * LWS_US_PER_SEC);
	} else
		lws_dll2_remove(&pt->sul_ah_lifecheck.list);
#endif

	return 0;
}


static int
rops_tx_credit_h2(struct lws *wsi, char peer_to_us, int add)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	int n;

	if (add) {
		if (peer_to_us == LWSTXCR_PEER_TO_US) {
			/*
			 * We want to tell the peer they can write an additional
			 * "add" bytes to us
			 */
			return lws_h2_update_peer_txcredit(wsi, (unsigned int)-1, add);
		}

		/*
		 * We're being told we can write an additional "add" bytes
		 * to the peer
		 */

		wsi->txc.tx_cr += add;
		nwsi->txc.tx_cr += add;

		return 0;
	}

	if (peer_to_us == LWSTXCR_US_TO_PEER)
		return lws_h2_tx_cr_get(wsi);

	n = wsi->txc.peer_tx_cr_est;
	if (n > nwsi->txc.peer_tx_cr_est)
		n = nwsi->txc.peer_tx_cr_est;

	return n;
}

static int
rops_destroy_role_h2(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct allocated_headers *ah;

	/* we may not have an ah, but may be on the waiting list... */
	lwsl_info("%s: %s: ah det due to close\n", __func__, lws_wsi_tag(wsi));
	__lws_header_table_detach(wsi, 0);

	ah = pt->http.ah_list;

	while (ah) {
		if (ah->in_use && ah->wsi == wsi) {
			lwsl_err("%s: ah leak: %s\n", __func__, lws_wsi_tag(wsi));
			ah->in_use = 0;
			ah->wsi = NULL;
			pt->http.ah_count_in_use--;
			break;
		}
		ah = ah->next;
	}

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	lws_http_compression_destroy(wsi);
#endif

	if (wsi->upgraded_to_http2 || wsi->mux_substream) {
		lws_hpack_destroy_dynamic_header(wsi);

		if (wsi->h2.h2n)
			lws_free_set_NULL(wsi->h2.h2n);
	}

	return 0;
}

static int
rops_close_kill_connection_h2(struct lws *wsi, enum lws_close_status reason)
{

#if defined(LWS_WITH_HTTP_PROXY)
	if (wsi->http.proxy_clientside) {

		wsi->http.proxy_clientside = 0;

		if (user_callback_handle_rxflow(wsi->a.protocol->callback,
						wsi,
					    LWS_CALLBACK_COMPLETED_CLIENT_HTTP,
						wsi->user_space, NULL, 0))
			wsi->http.proxy_clientside = 0;
	}
#endif

	if (wsi->mux_substream && wsi->h2_stream_carries_ws)
		lws_h2_rst_stream(wsi, 0, "none");
/*	else
		if (wsi->mux_substream)
			lws_h2_rst_stream(wsi, H2_ERR_STREAM_CLOSED, "swsi got closed");
*/

	lwsl_info(" %s, his parent %s: siblings:\n", lws_wsi_tag(wsi), lws_wsi_tag(wsi->mux.parent_wsi));
	lws_wsi_mux_dump_children(wsi);

	if (wsi->upgraded_to_http2 || wsi->mux_substream
#if defined(LWS_WITH_CLIENT)
			|| wsi->client_mux_substream
#endif
	) {
		lwsl_info("closing %s: parent %s\n", lws_wsi_tag(wsi),
				lws_wsi_tag(wsi->mux.parent_wsi));

		if (wsi->mux.child_list && lwsl_visible(LLL_INFO)) {
			lwsl_info(" parent %s: closing children: list:\n", lws_wsi_tag(wsi));
			lws_wsi_mux_dump_children(wsi);
		}
		lws_wsi_mux_close_children(wsi, (int)reason);
	}

	if (wsi->upgraded_to_http2) {
		/* remove pps */
		struct lws_h2_protocol_send *w = wsi->h2.h2n->pps, *w1;

		while (w) {
			w1 = w->next;
			free(w);
			w = w1;
		}
		wsi->h2.h2n->pps = NULL;
	}

	if ((
#if defined(LWS_WITH_CLIENT)
			wsi->client_mux_substream ||
#endif
			wsi->mux_substream) &&
	     wsi->mux.parent_wsi) {
		lws_wsi_mux_sibling_disconnect(wsi);
		if (wsi->h2.pending_status_body)
			lws_free_set_NULL(wsi->h2.pending_status_body);
	}

	return 0;
}

static int
rops_callback_on_writable_h2(struct lws *wsi)
{
#if defined(LWS_WITH_CLIENT)
	struct lws *network_wsi;
#endif
	int already;

//	if (!lwsi_role_h2(wsi) && !lwsi_role_h2_ENCAPSULATION(wsi))
//		return 0;

	if (wsi->mux.requested_POLLOUT
#if defined(LWS_WITH_CLIENT)
			&& !wsi->client_h2_alpn
#endif
	) {
		lwsl_debug("already pending writable\n");
		// return 1;
	}

	/* is this for DATA or for control messages? */

	if (wsi->upgraded_to_http2 && !wsi->h2.h2n->pps &&
	    lws_wsi_txc_check_skint(&wsi->txc, lws_h2_tx_cr_get(wsi))) {
		/*
		 * refuse his efforts to get WRITABLE if we have no credit and
		 * no non-DATA pps to send
		 */
		lwsl_err("%s: skint\n", __func__);
		return 0;
	}

#if defined(LWS_WITH_CLIENT)
	network_wsi = lws_get_network_wsi(wsi);
#endif
	already = lws_wsi_mux_mark_parents_needing_writeable(wsi);

	/* for network action, act only on the network wsi */

	if (already
#if defined(LWS_WITH_CLIENT)
			&& !network_wsi->client_h2_alpn
			&& !network_wsi->client_mux_substream
#endif
			)
		return 1;

	return 0;
}

#if defined(LWS_WITH_SERVER)
static int
lws_h2_bind_for_post_before_action(struct lws *wsi)
{
	const struct lws_http_mount *hit;
	int uri_len = 0, methidx;
	char *uri_ptr = NULL;
	uint8_t *buffered;
	const char *p;
	size_t blen;

	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD);
	if (!p || strcmp(p, "POST"))
		return 0;


	if (!lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_PATH) ||
	    !lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_PATH))
		/*
		 * There must be a path.  Actually this is checked at
		 * http2.c along with the other required header
		 * presence before we can get here.
		 *
		 * But Coverity insists to see us check it.
		 */
		return 1;

	hit = lws_find_mount(wsi,
		  lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_PATH),
		  lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_PATH));

	lwsl_debug("%s: %s: hit %p: %s\n", __func__,
		    lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_PATH),
		    hit, hit ? hit->origin : "null");
	if (hit) {
		const struct lws_protocols *pp;
		const char *name = hit->origin;

		if (hit->origin_protocol == LWSMPRO_CGI ||
		    hit->origin_protocol == LWSMPRO_HTTP ||
		    hit->origin_protocol == LWSMPRO_HTTPS)
			return 0;

		if (hit->protocol)
			name = hit->protocol;
		else
			if (hit->origin_protocol == LWSMPRO_FILE)
				return 0;

		pp = lws_vhost_name_to_protocol(wsi->a.vhost, name);
		if (!pp) {
			lwsl_info("Unable to find protocol '%s'\n", name);
			return 1;
		}

		if (lws_bind_protocol(wsi, pp, __func__))
			return 1;
	}

	methidx = lws_http_get_uri_and_method(wsi, &uri_ptr, &uri_len);

	if (methidx >= 0)
		if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP,
					      wsi->user_space,
					      hit ? uri_ptr +
						  hit->mountpoint_len : uri_ptr,
					      (size_t)(hit ? uri_len -
							  hit->mountpoint_len :
							  uri_len)))
			return 1;

#if defined(LWS_WITH_ACCESS_LOG)
	lws_prepare_access_log_info(wsi, uri_ptr, uri_len, methidx);
#endif

	lwsl_info("%s: setting LRS_BODY from 0x%x (%s)\n", __func__,
		    (int)wsi->wsistate, wsi->a.protocol->name);

	lwsi_set_state(wsi, LRS_BODY);

	if (wsi->http.content_length_explicitly_zero)
		return 0;

	/*
	 * Dump any stashed body
	 */

	while (((!wsi->http.content_length_given) ||
		  wsi->http.rx_content_length) &&
	       (blen = lws_buflist_next_segment_len(&wsi->buflist, &buffered))) {

		if ((size_t)wsi->http.rx_content_length < blen)
			blen = (size_t)wsi->http.rx_content_length;

		if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP_BODY,
				wsi->user_space, buffered, blen))
			return 1;
		lws_buflist_use_segment(&wsi->buflist, blen);

		wsi->http.rx_content_length -= blen;
	}

	if (!wsi->buflist)
		/* Take us off the pt's "wsi holding input buflist" list */
		lws_dll2_remove(&wsi->dll_buflist);

	if (wsi->http.content_length_given && wsi->http.rx_content_length)
		/* still a-ways to go */
		return 0;

	if (!wsi->http.content_length_given && !wsi->h2.END_STREAM)
		return 0;

	if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP_BODY_COMPLETION,
				      wsi->user_space, NULL, 0))
		return 1;

	return 0;
}
#endif

/*
 * we are the 'network wsi' for potentially many muxed child wsi with
 * no network connection of their own, who have to use us for all their
 * network actions.  So we use a round-robin scheme to share out the
 * POLLOUT notifications to our children.
 *
 * But because any child could exhaust the socket's ability to take
 * writes, we can only let one child get notified each time.
 *
 * In addition children may be closed / deleted / added between POLLOUT
 * notifications, so we can't hold pointers
 */

static int
rops_perform_user_POLLOUT_h2(struct lws *wsi)
{
	struct lws **wsi2;
#if defined(LWS_ROLE_WS)
	int write_type = LWS_WRITE_PONG;
#endif
	int n;

	wsi = lws_get_network_wsi(wsi);

	wsi->mux.requested_POLLOUT = 0;
//	if (!wsi->h2.initialized) {
//		lwsl_info("pollout on uninitialized http2 conn\n");
//		return 0;
//	}

	lws_wsi_mux_dump_waiting_children(wsi);

	wsi2 = &wsi->mux.child_list;
	if (!*wsi2)
		return 0;

	do {
		struct lws *w, **wa;

		wa = &(*wsi2)->mux.sibling_list;
		if (!(*wsi2)->mux.requested_POLLOUT)
			goto next_child;

		/*
		 * we're going to do writable callback for this child.
		 * move him to be the last child
		 */

		lwsl_debug("servicing child %s\n", lws_wsi_tag(*wsi2));

		w = lws_wsi_mux_move_child_to_tail(wsi2);

		if (!w) {
			wa = &wsi->mux.child_list;
			goto next_child;
		}

		lwsl_info("%s: child %s, sid %d, (wsistate 0x%x)\n",
			  __func__, lws_wsi_tag(w), w->mux.my_sid,
			  (unsigned int)w->wsistate);

		/* priority 1: post compression-transform buffered output */

		if (lws_has_buffered_out(w)) {
			lwsl_debug("%s: completing partial\n", __func__);
			if (lws_issue_raw(w, NULL, 0) < 0) {
				lwsl_info("%s signalling to close\n", __func__);
				lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
						   "h2 end stream 1");
				wa = &wsi->mux.child_list;
				goto next_child;
			}
			lws_callback_on_writable(w);
			wa = &wsi->mux.child_list;
			goto next_child;
		}

		/* priority 2: pre compression-transform buffered output */

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
		if (w->http.comp_ctx.buflist_comp ||
		    w->http.comp_ctx.may_have_more) {
			enum lws_write_protocol wp = LWS_WRITE_HTTP;

			lwsl_info("%s: completing comp partial"
				   "(buflist_comp %p, may %d)\n",
				   __func__, w->http.comp_ctx.buflist_comp,
				    w->http.comp_ctx.may_have_more);

			if (rops_write_role_protocol_h2(w, NULL, 0, &wp) < 0) {
				lwsl_info("%s signalling to close\n", __func__);
				lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
						   "comp write fail");
			}
			lws_callback_on_writable(w);
			wa = &wsi->mux.child_list;
			goto next_child;
		}
#endif

		/* priority 3: if no buffered out and waiting for that... */

		if (lwsi_state(w) == LRS_FLUSHING_BEFORE_CLOSE) {
			w->socket_is_permanently_unusable = 1;
			lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
					   "h2 end stream 1");
			wa = &wsi->mux.child_list;
			goto next_child;
		}

		/* if we arrived here, even by looping, we checked choked */
		w->could_have_pending = 0;
		wsi->could_have_pending = 0;

		if (w->h2.pending_status_body) {
			w->h2.send_END_STREAM = 1;
			n = lws_write(w, (uint8_t *)w->h2.pending_status_body +
					 LWS_PRE,
				         strlen(w->h2.pending_status_body +
					        LWS_PRE), LWS_WRITE_HTTP_FINAL);
			lws_free_set_NULL(w->h2.pending_status_body);
			lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
					   "h2 end stream 1");
			wa = &wsi->mux.child_list;
			goto next_child;
		}

#if defined(LWS_WITH_CLIENT)
		if (lwsi_state(w) == LRS_H2_WAITING_TO_SEND_HEADERS) {
			if (lws_h2_client_handshake(w))
				return -1;

			goto next_child;
		}
#endif

#if defined(LWS_WITH_SERVER)
		if (lwsi_state(w) == LRS_DEFERRING_ACTION) {

			/*
			 * we had to defer the http_action to the POLLOUT
			 * handler, because we know it will send something and
			 * only in the POLLOUT handler do we know for sure
			 * that there is no partial pending on the network wsi.
			 */

			lwsi_set_state(w, LRS_ESTABLISHED);

			if (w->buflist) {
				struct lws_context_per_thread *pt;

				pt = &w->a.context->pt[(int)w->tsi];
				lwsl_debug("%s: added %s to rxflow list\n",
					   __func__, lws_wsi_tag(w));
				lws_dll2_add_head(
					&w->dll_buflist,
					&pt->dll_buflist_owner);
			}

			if (lws_h2_bind_for_post_before_action(w))
				return -1;

			/*
			 * Well, we could be getting a POST from the client, it
			 * may not have any content-length.  In that case, we
			 * will be in LRS_BODY state, we can't actually start
			 * the action until we had the body and the stream is
			 * half-closed, indicating that we can reply
			 */

			if (lwsi_state(w) == LRS_BODY &&
			    w->h2.h2_state != LWS_H2_STATE_HALF_CLOSED_REMOTE)
				goto next_child;

			lwsl_info("  h2 action start...\n");
			n = lws_http_action(w);
			if (n < 0)
				lwsl_info ("   h2 action result %d\n", n);
			else
			lwsl_info("  h2 action result %d "
				  "(wsi->http.rx_content_remain %lld)\n",
				  n, w->http.rx_content_remain);

			/*
			 * Commonly we only managed to start a larger transfer
			 * that will complete asynchronously under its own wsi
			 * states.  In those cases we will hear about
			 * END_STREAM going out in the POLLOUT handler.
			 */
			if (n >= 0 && !w->h2.pending_status_body &&
			    (n || w->h2.send_END_STREAM)) {
				lwsl_info("closing stream after h2 action\n");
				lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
						   "h2 end stream");
				wa = &wsi->mux.child_list;
			}

			if (n < 0)
				wa = &wsi->mux.child_list;

			goto next_child;
		}

#if defined(LWS_WITH_FILE_OPS)

		if (lwsi_state(w) == LRS_ISSUING_FILE) {

			if (lws_wsi_txc_check_skint(&w->txc,
						    lws_h2_tx_cr_get(w))) {
				wa = &wsi->mux.child_list;
				goto next_child;
			}

			((volatile struct lws *)w)->leave_pollout_active = 0;

			/* >0 == completion, <0 == error
			 *
			 * We'll get a LWS_CALLBACK_HTTP_FILE_COMPLETION
			 * callback when it's done.  That's the case even if we
			 * just completed the send, so wait for that.
			 */
			n = lws_serve_http_file_fragment(w);
			lwsl_debug("lws_serve_http_file_fragment says %d\n", n);

			/*
			 * We will often hear about out having sent the final
			 * DATA here... if so close the actual wsi
			 */
			if (n < 0 || w->h2.send_END_STREAM) {
				lwsl_debug("Closing POLLOUT child %s\n",
						lws_wsi_tag(w));
				lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
						   "h2 end stream file");
				wa = &wsi->mux.child_list;
				goto next_child;
			}
			if (n > 0)
				if (lws_http_transaction_completed(w))
					return -1;
			if (!n) {
				lws_callback_on_writable(w);
				(w)->mux.requested_POLLOUT = 1;
			}

			goto next_child;
		}
#endif
#endif

#if defined(LWS_ROLE_WS)

		/* Notify peer that we decided to close */

		if (lwsi_role_ws(w) &&
		    lwsi_state(w) == LRS_WAITING_TO_SEND_CLOSE) {
			lwsl_debug("sending close packet\n");
			w->waiting_to_send_close_frame = 0;
			n = lws_write(w, &w->ws->ping_payload_buf[LWS_PRE],
				      w->ws->close_in_ping_buffer_len,
				      LWS_WRITE_CLOSE);
			if (n >= 0) {
				lwsi_set_state(w, LRS_AWAITING_CLOSE_ACK);
				lws_set_timeout(w, PENDING_TIMEOUT_CLOSE_ACK, 5);
				lwsl_debug("sent close frame, awaiting ack\n");
			}

			goto next_child;
		}

		/*
		 * Acknowledge receipt of peer's notification he closed,
		 * then logically close ourself
		 */

		if ((lwsi_role_ws(w) && w->ws->pong_pending_flag) ||
		    (lwsi_state(w) == LRS_RETURNED_CLOSE &&
		     w->ws->payload_is_close)) {

			if (w->ws->payload_is_close)
				write_type = LWS_WRITE_CLOSE |
					     LWS_WRITE_H2_STREAM_END;

			n = lws_write(w, &w->ws->pong_payload_buf[LWS_PRE],
				      w->ws->pong_payload_len, (enum lws_write_protocol)write_type);
			if (n < 0)
				return -1;

			/* well he is sent, mark him done */
			w->ws->pong_pending_flag = 0;
			if (w->ws->payload_is_close) {
				/* oh... a close frame... then we are done */
				lwsl_debug("Ack'd peer's close packet\n");
				w->ws->payload_is_close = 0;
				lwsi_set_state(w, LRS_RETURNED_CLOSE);
				lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
						   "returned close packet");
				wa = &wsi->mux.child_list;
				goto next_child;
			}

			lws_callback_on_writable(w);
			(w)->mux.requested_POLLOUT = 1;

			/* otherwise for PING, leave POLLOUT active both ways */
			goto next_child;
		}
#endif

		/*
		 * set client wsi to immortal long-poll mode; send END_STREAM
		 * flag on headers to indicate to a server, that allows
		 * it, that you want them to leave the stream in a long poll
		 * ro immortal state.  We have to send headers so the client
		 * understands the http connection is ongoing.
		 */

		if (w->h2.send_END_STREAM && w->h2.long_poll) {
			uint8_t buf[LWS_PRE + 1];
			enum lws_write_protocol wp = 0;

			if (!rops_write_role_protocol_h2(w, buf + LWS_PRE, 0,
							 &wp)) {
				lwsl_info("%s: %s: entering ro long poll\n",
					  __func__, lws_wsi_tag(w));
				lws_mux_mark_immortal(w);
			} else
				lwsl_err("%s: %s: failed to set long poll\n",
						__func__, lws_wsi_tag(w));
			goto next_child;
		}

		if (lws_callback_as_writeable(w)) {
			lwsl_info("Closing POLLOUT child (end stream %d)\n",
				  w->h2.send_END_STREAM);
			lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
					   "h2 pollout handle");
			wa = &wsi->mux.child_list;
		} else
			 if (w->h2.send_END_STREAM)
				lws_h2_state(w, LWS_H2_STATE_HALF_CLOSED_LOCAL);

next_child:
		wsi2 = wa;
	} while (wsi2 && *wsi2 && !lws_send_pipe_choked(wsi));

	// lws_wsi_mux_dump_waiting_children(wsi);

	if (lws_wsi_mux_action_pending_writeable_reqs(wsi))
		return -1;

	return 0;
}

static struct lws *
rops_encapsulation_parent_h2(struct lws *wsi)
{
	if (wsi->mux.parent_wsi)
		return wsi->mux.parent_wsi;

	return NULL;
}

static int
rops_alpn_negotiated_h2(struct lws *wsi, const char *alpn)
{
	struct allocated_headers *ah;

	lwsl_debug("%s: client %d\n", __func__, lwsi_role_client(wsi));
#if defined(LWS_WITH_CLIENT)
	if (lwsi_role_client(wsi)) {
		lwsl_info("%s: upgraded to H2\n", __func__);
		wsi->client_h2_alpn = 1;
	}
#endif

	wsi->upgraded_to_http2 = 1;

	/* adopt the header info */

	ah = wsi->http.ah;

	lws_role_transition(wsi, lwsi_role_client(wsi) ? LWSIFR_CLIENT : LWSIFR_SERVER, LRS_H2_AWAIT_PREFACE,
			    &role_ops_h2);

	/* http2 union member has http union struct at start */
	wsi->http.ah = ah;

	if (!wsi->h2.h2n)
		wsi->h2.h2n = lws_zalloc(sizeof(*wsi->h2.h2n), "h2n");
	if (!wsi->h2.h2n)
		return 1;

	lws_h2_init(wsi);

	/* HTTP2 union */

	if (lws_hpack_dynamic_size(wsi,
			   (int)wsi->h2.h2n->our_set.s[H2SET_HEADER_TABLE_SIZE]))
		return 1;
	wsi->txc.tx_cr = 65535;

	lwsl_info("%s: %s: configured for h2\n", __func__, lws_wsi_tag(wsi));

	return 0;
}

static int
rops_issue_keepalive_h2(struct lws *wsi, int isvalid)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	struct lws_h2_protocol_send *pps;
	uint64_t us = (uint64_t)lws_now_usecs();

	if (isvalid) {
		_lws_validity_confirmed_role(nwsi);

		return 0;
	}

	/*
	 * We can only send these frames on the network connection itself...
	 * we shouldn't be tracking validity on anything else
	 */

	assert(wsi == nwsi);

	pps = lws_h2_new_pps(LWS_H2_PPS_PING);
	if (!pps)
		return 1;

	/*
	 * The peer is defined to copy us back the unchanged payload in another
	 * PING frame this time with ACK set.  So by sending that out with the
	 * current time, it's an interesting opportunity to learn the effective
	 * RTT on the link when the PONG comes in, plus or minus the time to
	 * schedule the PPS.
	 */

	memcpy(pps->u.ping.ping_payload, &us, 8);
	lws_pps_schedule(nwsi, pps);

	return 0;
}

static const lws_rops_t rops_table_h2[] = {
#if defined(LWS_WITH_SERVER)
	/*  1 */ { .check_upgrades	  = rops_check_upgrades_h2 },
#else
	/*  1 */ { .check_upgrades	  = NULL },
#endif
	/*  2 */ { .pt_init_destroy	  = rops_pt_init_destroy_h2 },
	/*  3 */ { .init_vhost		  = rops_init_vhost_h2 },
	/*  4 */ { .handle_POLLIN	  = rops_handle_POLLIN_h2 },
	/*  5 */ { .handle_POLLOUT	  = rops_handle_POLLOUT_h2 },
	/*  6 */ { .perform_user_POLLOUT  = rops_perform_user_POLLOUT_h2 },
	/*  7 */ { .callback_on_writable  = rops_callback_on_writable_h2 },
	/*  8 */ { .tx_credit		  = rops_tx_credit_h2 },
	/*  9 */ { .write_role_protocol	  = rops_write_role_protocol_h2 },
	/* 10 */ { .encapsulation_parent  = rops_encapsulation_parent_h2 },
	/* 11 */ { .alpn_negotiated	  = rops_alpn_negotiated_h2 },
	/* 12 */ { .close_kill_connection = rops_close_kill_connection_h2 },
	/* 13 */ { .destroy_role	  = rops_destroy_role_h2 },
	/* 14 */ { .issue_keepalive	  = rops_issue_keepalive_h2 },
};


const struct lws_role_ops role_ops_h2 = {
	/* role name */			"h2",
	/* alpn id */			"h2",

	/* rops_table */		rops_table_h2,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
#if defined(LWS_WITH_SERVER)
	  /* LWS_ROPS_pt_init_destroy */		0x12,
#else
	  /* LWS_ROPS_pt_init_destroy */		0x02,
#endif
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x30,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x04,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x56,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x78,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x9a,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0xb0,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x0c,
	  /* LWS_ROPS_destroy_role */
	  /* LWS_ROPS_adoption_bind */			0xd0,
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x0e,
					},
	/* adoption_cb clnt, srv */	{ LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED,
					  LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED },
	/* rx cb clnt, srv */		{ LWS_CALLBACK_RECEIVE_CLIENT_HTTP,
					  0 /* may be POST, etc */ },
	/* writeable cb clnt, srv */	{ LWS_CALLBACK_CLIENT_HTTP_WRITEABLE,
					  LWS_CALLBACK_HTTP_WRITEABLE },
	/* close cb clnt, srv */	{ LWS_CALLBACK_CLOSED_CLIENT_HTTP,
					  LWS_CALLBACK_CLOSED_HTTP },
	/* protocol_bind cb c, srv */	{ LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL,
					  LWS_CALLBACK_HTTP_BIND_PROTOCOL },
	/* protocol_unbind cb c, srv */	{ LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL,
					  LWS_CALLBACK_HTTP_DROP_PROTOCOL },
	/* file_handle */		0,
};
