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

static int
rops_handle_POLLIN_mqtt(struct lws_context_per_thread *pt, struct lws *wsi,
			   struct lws_pollfd *pollfd)
{
	unsigned int pending = 0;
	struct lws_tokens ebuf;
	int n = 0;
	char buffered = 0;

	lwsl_debug("%s: wsistate 0x%x, %s pollout %d\n", __func__,
		   (unsigned int)wsi->wsistate,  wsi->a.protocol->name,
		   pollfd->revents);

	/*
	 * After the CONNACK and nwsi establishment, the first logical
	 * stream is migrated out of the nwsi to be child sid 1, and the
	 * nwsi no longer has a wsi->mqtt of its own.
	 *
	 * RX events on the nwsi must be converted to events seen or not
	 * seen by one or more child streams.
	 *
	 * SUBACK - reflected to child stream that asked for it
	 * PUBACK - routed to child that did the related publish
	 */

	ebuf.token = NULL;
	ebuf.len = 0;

	if (lwsi_state(wsi) != LRS_ESTABLISHED) {
#if defined(LWS_WITH_CLIENT)

		if (lwsi_state(wsi) == LRS_WAITING_SSL &&
		    ((pollfd->revents & LWS_POLLOUT)) &&
		    lws_change_pollfd(wsi, LWS_POLLOUT, 0)) {
			lwsl_info("failed at set pollfd\n");
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		if ((pollfd->revents & LWS_POLLOUT) &&
		    lws_handle_POLLOUT_event(wsi, pollfd)) {
			lwsl_debug("POLLOUT event closed it\n");
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		n = lws_mqtt_client_socket_service(wsi, pollfd, NULL);
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

		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	/* 3: buflist needs to be drained
	 */
read:
	// lws_buflist_describe(&wsi->buflist, wsi, __func__);
	ebuf.len = (int)lws_buflist_next_segment_len(&wsi->buflist, &ebuf.token);
	if (ebuf.len) {
		lwsl_info("draining buflist (len %d)\n", ebuf.len);
		buffered = 1;
		goto drain;
	}

	if (!(pollfd->revents & pollfd->events & LWS_POLLIN))
		return LWS_HPI_RET_HANDLED;

	/* if (lws_is_flowcontrolled(wsi)) { */
	/*	lwsl_info("%s: %p should be rxflow (bm 0x%x)..\n", */
	/*		    __func__, wsi, wsi->rxflow_bitmap); */
	/*	return LWS_HPI_RET_HANDLED; */
	/* } */

	if (!(lwsi_role_client(wsi) && lwsi_state(wsi) != LRS_ESTABLISHED)) {
		/*
		 * In case we are going to react to this rx by scheduling
		 * writes, we need to restrict the amount of rx to the size
		 * the protocol reported for rx buffer.
		 *
		 * Otherwise we get a situation we have to absorb possibly a
		 * lot of reads before we get a chance to drain them by writing
		 * them, eg, with echo type tests in autobahn.
		 */

		buffered = 0;
		ebuf.token = pt->serv_buf;
		ebuf.len = (int)wsi->a.context->pt_serv_buf_size;

		if ((unsigned int)ebuf.len > wsi->a.context->pt_serv_buf_size)
			ebuf.len = (int)wsi->a.context->pt_serv_buf_size;

		if ((int)pending > ebuf.len)
			pending = (unsigned int)ebuf.len;

		ebuf.len = lws_ssl_capable_read(wsi, ebuf.token,
						pending ? pending :
						(unsigned int)ebuf.len);
		switch (ebuf.len) {
		case 0:
			lwsl_info("%s: zero length read\n",
				  __func__);
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		case LWS_SSL_CAPABLE_MORE_SERVICE:
			lwsl_info("SSL Capable more service\n");
			return LWS_HPI_RET_HANDLED;
		case LWS_SSL_CAPABLE_ERROR:
			lwsl_info("%s: LWS_SSL_CAPABLE_ERROR\n",
					__func__);
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		/*
		 * coverity thinks ssl_capable_read() may read over
		 * 2GB.  Dissuade it...
		 */
		ebuf.len &= 0x7fffffff;
	}

drain:
	/* service incoming data */
	//lws_buflist_describe(&wsi->buflist, wsi, __func__);
	if (ebuf.len) {
		n = lws_read_mqtt(wsi, ebuf.token, (unsigned int)ebuf.len);
		if (n < 0) {
			lwsl_notice("%s: lws_read_mqtt returned %d\n",
					__func__, n);
			/* we closed wsi */
			goto fail;
                }
		// lws_buflist_describe(&wsi->buflist, wsi, __func__);
		lwsl_debug("%s: consuming %d / %d\n", __func__, n, ebuf.len);
		if (lws_buflist_aware_finished_consuming(wsi, &ebuf, ebuf.len,
							 buffered, __func__))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	ebuf.token = NULL;
	ebuf.len = 0;

	pending = (unsigned int)lws_ssl_pending(wsi);
	if (pending) {
		pending = pending > wsi->a.context->pt_serv_buf_size ?
			wsi->a.context->pt_serv_buf_size : pending;
		goto read;
	}

	if (buffered && /* were draining, now nothing left */
	    !lws_buflist_next_segment_len(&wsi->buflist, NULL)) {
		lwsl_info("%s: %s flow buf: drained\n", __func__, lws_wsi_tag(wsi));
		/* having drained the rxflow buffer, can rearm POLLIN */
#if !defined(LWS_WITH_SERVER)
		n =
#endif
		__lws_rx_flow_control(wsi);
		/* n ignored, needed for NO_SERVER case */
	}

	/* n = 0 */
	return LWS_HPI_RET_HANDLED;

fail:
	lwsl_err("%s: Failed, bailing\n", __func__);
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "mqtt svc fail");

	return LWS_HPI_RET_WSI_ALREADY_DIED;
}

#if 0 /* defined(LWS_WITH_SERVER) */

static int
rops_adoption_bind_mqtt(struct lws *wsi, int type, const char *vh_prot_name)
{
	/* no http but socket... must be mqtt */
	if ((type & LWS_ADOPT_HTTP) || !(type & LWS_ADOPT_SOCKET) ||
	    (type & _LWS_ADOPT_FINISH))
		return 0; /* no match */

	lws_role_transition(wsi, 0, (type & LWS_ADOPT_ALLOW_SSL) ? LRS_SSL_INIT :
				LRS_ESTABLISHED, &role_ops_mqtt);

	if (vh_prot_name)
		lws_bind_protocol(wsi, wsi->a.protocol, __func__);
	else
		/* this is the only time he will transition */
		lws_bind_protocol(wsi,
			&wsi->a.vhost->protocols[wsi->a.vhost->mqtt_protocol_index],
			__func__);

	return 1; /* bound */
}
#endif

static int
rops_client_bind_mqtt(struct lws *wsi, const struct lws_client_connect_info *i)
{
	lwsl_debug("%s: i = %p\n", __func__, i);
	if (!i) {

		/* finalize */

		if (!wsi->user_space && wsi->stash->cis[CIS_METHOD])
			if (lws_ensure_user_space(wsi))
				return 1;

		if (!wsi->stash->cis[CIS_METHOD] && !wsi->stash->cis[CIS_ALPN])
			wsi->stash->cis[CIS_ALPN] = "x-amzn-mqtt-ca";

		/* if we went on the ah waiting list, it's ok, we can
		 * wait.
		 *
		 * When we do get the ah, now or later, he will end up
		 * at lws_http_client_connect_via_info2().
		 */
#if defined(LWS_WITH_CLIENT)
		if (lws_header_table_attach(wsi, 0) < 0)
			/*
			 * if we failed here, the connection is already closed
			 * and freed.
			 */
			return -1;
#else
		if (lws_header_table_attach(wsi, 0))
			return 0;
#endif
		return 0;
	}

	/* if a recognized mqtt method, bind to it */
	if (strcmp(i->method, "MQTT"))
		return 0; /* no match */

	if (lws_create_client_mqtt_object(i, wsi))
		return 1;

	lws_role_transition(wsi, LWSIFR_CLIENT, LRS_UNCONNECTED,
				&role_ops_mqtt);
	return 1; /* matched */
}

static int
rops_handle_POLLOUT_mqtt(struct lws *wsi)
{
	struct lws **wsi2;

	lwsl_debug("%s\n", __func__);

#if defined(LWS_WITH_CLIENT)
	if (wsi->mqtt && wsi->mqtt->send_pingreq && !wsi->mqtt->inside_payload) {
		uint8_t buf[LWS_PRE + 2];

		/*
		 * We are swallowing this POLLOUT in order to send a PINGREQ
		 * autonomously
		 */

		wsi->mqtt->send_pingreq = 0;

		lwsl_notice("%s: issuing PINGREQ\n", __func__);

		buf[LWS_PRE] = LMQCP_CTOS_PINGREQ << 4;
		buf[LWS_PRE + 1] = 0;

		if (lws_write(wsi, (uint8_t *)&buf[LWS_PRE], 2,
			      LWS_WRITE_BINARY) != 2)
			return LWS_HP_RET_BAIL_DIE;

		return LWS_HP_RET_BAIL_OK;
	}
#endif
	if (wsi->mqtt && !wsi->mqtt->inside_payload &&
	    (wsi->mqtt->send_pubrec || wsi->mqtt->send_pubrel ||
	     wsi->mqtt->send_pubcomp)) {
		uint8_t buf[LWS_PRE + 4];
		/* Remaining len = 2 */
		buf[LWS_PRE + 1] = 2;
		if (wsi->mqtt->send_pubrec) {
			lwsl_notice("%s: issuing PUBREC for pkt id: %d\n",
				    __func__, wsi->mqtt->peer_ack_pkt_id);
			buf[LWS_PRE] = LMQCP_PUBREC << 4 | 0x2;
			/* Packet ID */
			lws_ser_wu16be(&buf[LWS_PRE + 2],
				       wsi->mqtt->peer_ack_pkt_id);
			wsi->mqtt->send_pubrec = 0;
		} else if (wsi->mqtt->send_pubrel) {
			lwsl_notice("%s: issuing PUBREL for pkt id: %d\n",
				    __func__, wsi->mqtt->ack_pkt_id);
			buf[LWS_PRE] = LMQCP_PUBREL << 4 | 0x2;
			lws_ser_wu16be(&buf[LWS_PRE + 2],
				       wsi->mqtt->ack_pkt_id);
			wsi->mqtt->send_pubrel = 0;
		} else {
			lwsl_notice("%s: issuing PUBCOMP for pkt id: %d\n",
				    __func__, wsi->mqtt->peer_ack_pkt_id);
			buf[LWS_PRE] = LMQCP_PUBCOMP << 4 | 0x2;
			lws_ser_wu16be(&buf[LWS_PRE + 2],
				       wsi->mqtt->peer_ack_pkt_id);
			wsi->mqtt->send_pubcomp = 0;
		}
		if (lws_write(wsi, (uint8_t *)&buf[LWS_PRE], 4,
			      LWS_WRITE_BINARY) != 4)
			return LWS_HP_RET_BAIL_DIE;
		return LWS_HP_RET_BAIL_OK;
	}

	wsi = lws_get_network_wsi(wsi);

	wsi->mux.requested_POLLOUT = 0;

	wsi2 = &wsi->mux.child_list;
	if (!*wsi2) {
		lwsl_debug("%s: no children\n", __func__);
		return LWS_HP_RET_DROP_POLLOUT;
	}

	if (!wsi->mqtt)
		return LWS_HP_RET_BAIL_DIE;

	lws_wsi_mux_dump_waiting_children(wsi);

	do {
		struct lws *w, **wa;

		wa = &(*wsi2)->mux.sibling_list;
		if (!(*wsi2)->mux.requested_POLLOUT)
			goto next_child;

		if (!lwsi_state_can_handle_POLLOUT(wsi))
			goto next_child;

		/*
		 * If the nwsi is in the middle of a frame, we can only
		 * continue to send that
		 */

		if (wsi->mqtt->inside_payload && !(*wsi2)->mqtt->inside_payload)
			goto next_child;

		/*
		 * we're going to do writable callback for this child.
		 * move him to be the last child
		 */
		w = lws_wsi_mux_move_child_to_tail(wsi2);
		if (!w) {
			wa = &wsi->mux.child_list;
			goto next_child;
		}

		lwsl_debug("%s: child %s (wsistate 0x%x)\n", __func__,
			   lws_wsi_tag(w), (unsigned int)w->wsistate);

		if (lwsi_state(wsi) == LRS_ESTABLISHED &&
		    !wsi->mqtt->inside_payload &&
		    wsi->mqtt->send_puback) {
			uint8_t buf[LWS_PRE + 4];
			lwsl_notice("%s: issuing PUBACK for pkt id: %d\n",
				    __func__, wsi->mqtt->ack_pkt_id);

			/* Fixed header */
			buf[LWS_PRE] = LMQCP_PUBACK << 4;
			/* Remaining len = 2 */
			buf[LWS_PRE + 1] = 2;
			/* Packet ID */
			lws_ser_wu16be(&buf[LWS_PRE + 2], wsi->mqtt->peer_ack_pkt_id);

			if (lws_write(wsi, (uint8_t *)&buf[LWS_PRE], 4,
				      LWS_WRITE_BINARY) != 4)
				return LWS_HP_RET_BAIL_DIE;

			wsi->mqtt->send_puback = 0;
			w->mux.requested_POLLOUT = 1;

			wa = &wsi->mux.child_list;
			goto next_child;
		}

		if (lws_callback_as_writeable(w)) {
			lwsl_notice("%s: Closing child %s\n", __func__, lws_wsi_tag(w));
			lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
					   "mqtt pollout handle");
			wa = &wsi->mux.child_list;
		}

next_child:
		wsi2 = wa;
	} while (wsi2 && *wsi2 && !lws_send_pipe_choked(wsi));

	// lws_wsi_mux_dump_waiting_children(wsi);

	if (lws_wsi_mux_action_pending_writeable_reqs(wsi))
		return LWS_HP_RET_BAIL_DIE;

	return LWS_HP_RET_BAIL_OK;
}

#if defined(LWS_WITH_CLIENT)
static int
rops_issue_keepalive_mqtt(struct lws *wsi, int isvalid)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);

	if (isvalid) {
		_lws_validity_confirmed_role(nwsi);

		return 0;
	}

	nwsi->mqtt->send_pingreq = 1;
	lws_callback_on_writable(nwsi);

	return 0;
}
#endif

static int
rops_close_role_mqtt(struct lws_context_per_thread *pt, struct lws *wsi)
{
	struct lws *nwsi = lws_get_network_wsi(wsi);
	lws_mqtt_subs_t	*s, *s1, *mysub;
	lws_mqttc_t *c;

	if (!wsi->mqtt)
		return 0;

	c = &wsi->mqtt->client;

	lws_sul_cancel(&wsi->mqtt->sul_qos_puback_pubrec_wait);

	lws_mqtt_str_free(&c->username);
	lws_mqtt_str_free(&c->password);
	lws_mqtt_str_free(&c->will.message);
	lws_mqtt_str_free(&c->will.topic);
	lws_mqtt_str_free(&c->id);

	/* clean up any subscription allocations */

	s = wsi->mqtt->subs_head;
	wsi->mqtt->subs_head = NULL;
	while (s) {
		s1 = s->next;
		/*
		 * Account for children no longer using nwsi subscription
		 */
		mysub = lws_mqtt_find_sub(nwsi->mqtt, (const char *)&s[1]);
//		assert(mysub); /* if child subscribed, nwsi must feel the same */
		if (mysub) {
			assert(mysub->ref_count);
			mysub->ref_count--;
		}
		lws_free(s);
		s = s1;
	}

	lws_mqtt_publish_param_t *pub =
			(lws_mqtt_publish_param_t *)
				wsi->mqtt->rx_cpkt_param;

	if (pub)
		lws_free_set_NULL(pub->topic);

	lws_free_set_NULL(wsi->mqtt->rx_cpkt_param);

	lws_free_set_NULL(wsi->mqtt);

	return 0;
}

static int
rops_callback_on_writable_mqtt(struct lws *wsi)
{
#if defined(LWS_WITH_CLIENT)
	struct lws *network_wsi;
#endif
	int already;

	lwsl_debug("%s: %s (wsistate 0x%x)\n", __func__, lws_wsi_tag(wsi),
			(unsigned int)wsi->wsistate);

	if (wsi->mux.requested_POLLOUT
#if defined(LWS_WITH_CLIENT)
			&& !wsi->client_h2_alpn
#endif
	) {
		lwsl_debug("already pending writable\n");
		return 1;
	}
#if 0
	/* is this for DATA or for control messages? */
	if (wsi->upgraded_to_http2 && !wsi->h2.h2n->pps &&
	    !lws_h2_tx_cr_get(wsi)) {
		/*
		 * other side is not able to cope with us sending DATA
		 * anything so no matter if we have POLLOUT on our side if it's
		 * DATA we want to send.
		 *
		 * Delay waiting for our POLLOUT until peer indicates he has
		 * space for more using tx window command in http2 layer
		 */
		lwsl_notice("%s: %p: skint (%d)\n", __func__, wsi,
			    wsi->h2.tx_cr);
		wsi->h2.skint = 1;
		return 0;
	}

	wsi->h2.skint = 0;
#endif
#if defined(LWS_WITH_CLIENT)
	network_wsi = lws_get_network_wsi(wsi);
#endif
	already = lws_wsi_mux_mark_parents_needing_writeable(wsi);

	/* for network action, act only on the network wsi */

	if (already
#if defined(LWS_WITH_CLIENT)
			&& !network_wsi->client_mux_substream
#endif
			)
		return 1;

	return 0;
}

static int
rops_close_kill_connection_mqtt(struct lws *wsi, enum lws_close_status reason)
{
	lwsl_info(" %s, his parent %s: child list %p, siblings:\n",
			lws_wsi_tag(wsi),
			lws_wsi_tag(wsi->mux.parent_wsi), wsi->mux.child_list);
	//lws_wsi_mux_dump_children(wsi);

	if (wsi->mux_substream
#if defined(LWS_WITH_CLIENT)
			|| wsi->client_mux_substream
#endif
	) {
		lwsl_info("closing %s: parent %s: first child %p\n",
				lws_wsi_tag(wsi),
				lws_wsi_tag(wsi->mux.parent_wsi),
				wsi->mux.child_list);

		if (wsi->mux.child_list && lwsl_visible(LLL_INFO)) {
			lwsl_info(" parent %s: closing children: list:\n", lws_wsi_tag(wsi));
			lws_wsi_mux_dump_children(wsi);
		}

		lws_wsi_mux_close_children(wsi, (int)reason);
	}

	if ((
#if defined(LWS_WITH_CLIENT)
			wsi->client_mux_substream ||
#endif
			wsi->mux_substream) &&
	     wsi->mux.parent_wsi) {
		lws_wsi_mux_sibling_disconnect(wsi);
	}

	return 0;
}

static const lws_rops_t rops_table_mqtt[] = {
	/*  1 */ { .handle_POLLIN	  = rops_handle_POLLIN_mqtt },
	/*  2 */ { .handle_POLLOUT	  = rops_handle_POLLOUT_mqtt },
	/*  3 */ { .callback_on_writable  = rops_callback_on_writable_mqtt },
	/*  4 */ { .close_role		  = rops_close_role_mqtt },
	/*  5 */ { .close_kill_connection = rops_close_kill_connection_mqtt },
#if defined(LWS_WITH_CLIENT)
	/*  6 */ { .client_bind		  = rops_client_bind_mqtt },
	/*  7 */ { .issue_keepalive	  = rops_issue_keepalive_mqtt },
#endif
};

struct lws_role_ops role_ops_mqtt = {
	/* role name */			"mqtt",
	/* alpn id */			"x-amzn-mqtt-ca", /* "mqtt/3.1.1" */

	/* rops_table */		rops_table_mqtt,
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
	  /* LWS_ROPS_tx_credit */			0x30,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x00,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x00,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x45,
	  /* LWS_ROPS_destroy_role */
	  /* LWS_ROPS_adoption_bind */			0x00,

	  /* LWS_ROPS_client_bind */
#if defined(LWS_WITH_CLIENT)
	  /* LWS_ROPS_issue_keepalive */		0x67,
#else
	  /* LWS_ROPS_issue_keepalive */		0x00,
#endif
					},

	.adoption_cb =			{ LWS_CALLBACK_MQTT_NEW_CLIENT_INSTANTIATED,
					  LWS_CALLBACK_MQTT_NEW_CLIENT_INSTANTIATED },
	.rx_cb =			{ LWS_CALLBACK_MQTT_CLIENT_RX,
					  LWS_CALLBACK_MQTT_CLIENT_RX },
	.writeable_cb =			{ LWS_CALLBACK_MQTT_CLIENT_WRITEABLE,
					  LWS_CALLBACK_MQTT_CLIENT_WRITEABLE },
	.close_cb =			{ LWS_CALLBACK_MQTT_CLIENT_CLOSED,
					  LWS_CALLBACK_MQTT_CLIENT_CLOSED },
	.protocol_bind_cb =		{ LWS_CALLBACK_MQTT_IDLE,
					  LWS_CALLBACK_MQTT_IDLE },
	.protocol_unbind_cb =		{ LWS_CALLBACK_MQTT_DROP_PROTOCOL,
					  LWS_CALLBACK_MQTT_DROP_PROTOCOL },
	.file_handle =			0,
};
