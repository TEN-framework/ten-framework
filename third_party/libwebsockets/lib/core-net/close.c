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

#if defined(LWS_WITH_CLIENT)
static int
lws_close_trans_q_leader(struct lws_dll2 *d, void *user)
{
	struct lws *w = lws_container_of(d, struct lws, dll2_cli_txn_queue);

	__lws_close_free_wsi(w, (enum lws_close_status)-1, "trans q leader closing");

	return 0;
}
#endif

void
__lws_reset_wsi(struct lws *wsi)
{
	if (!wsi)
		return;

#if defined(LWS_WITH_CLIENT)

	lws_free_set_NULL(wsi->cli_hostname_copy);

#if defined(LWS_WITH_CONMON)

	if (wsi->conmon.dns_results_copy) {
		lws_conmon_addrinfo_destroy(wsi->conmon.dns_results_copy);
		wsi->conmon.dns_results_copy = NULL;
	}

	wsi->conmon.ciu_dns =
		wsi->conmon.ciu_sockconn =
		wsi->conmon.ciu_tls =
		wsi->conmon.ciu_txn_resp = 0;
#endif

	/*
	 * if we have wsi in our transaction queue, if we are closing we
	 * must go through and close all those first
	 */
	if (wsi->a.vhost) {

		/* we are no longer an active client connection that can piggyback */
		lws_dll2_remove(&wsi->dll_cli_active_conns);

		lws_dll2_foreach_safe(&wsi->dll2_cli_txn_queue_owner, NULL,
				      lws_close_trans_q_leader);

		/*
		 * !!! If we are closing, but we have pending pipelined
		 * transaction results we already sent headers for, that's going
		 * to destroy sync for HTTP/1 and leave H2 stream with no live
		 * swsi.`
		 *
		 * However this is normal if we are being closed because the
		 * transaction queue leader is closing.
		 */
		lws_dll2_remove(&wsi->dll2_cli_txn_queue);
	}
#endif

	if (wsi->a.vhost) {
		lws_vhost_lock(wsi->a.vhost);
		lws_dll2_remove(&wsi->vh_awaiting_socket);
		lws_vhost_unlock(wsi->a.vhost);
	}

	/*
	 * Protocol user data may be allocated either internally by lws
	 * or by specified the user. We should only free what we allocated.
	 */
	if (wsi->a.protocol && wsi->a.protocol->per_session_data_size &&
	    wsi->user_space && !wsi->user_space_externally_allocated) {
		/* confirm no sul left scheduled in user data itself */
		lws_sul_debug_zombies(wsi->a.context, wsi->user_space,
				wsi->a.protocol->per_session_data_size, __func__);
		lws_free_set_NULL(wsi->user_space);
	}

	/*
	 * Don't let buflist content or state from the wsi's previous life
	 * carry over to the new life
	 */

	lws_buflist_destroy_all_segments(&wsi->buflist);
	lws_dll2_remove(&wsi->dll_buflist);
	lws_buflist_destroy_all_segments(&wsi->buflist_out);
#if defined(LWS_WITH_UDP)
	if (wsi->udp) {
		/* confirm no sul left scheduled in wsi->udp itself */
		lws_sul_debug_zombies(wsi->a.context, wsi->udp,
				      sizeof(*wsi->udp), "close udp wsi");
		lws_free_set_NULL(wsi->udp);
	}
#endif
	wsi->retry = 0;

#if defined(LWS_WITH_CLIENT)
	lws_dll2_remove(&wsi->dll2_cli_txn_queue);
	lws_dll2_remove(&wsi->dll_cli_active_conns);
	if (wsi->cli_hostname_copy)
		lws_free_set_NULL(wsi->cli_hostname_copy);
#endif

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	lws_async_dns_cancel(wsi);
#endif

#if defined(LWS_WITH_HTTP_PROXY)
	if (wsi->http.buflist_post_body)
		lws_buflist_destroy_all_segments(&wsi->http.buflist_post_body);
#endif

#if defined(LWS_WITH_SERVER)
	lws_dll2_remove(&wsi->listen_list);
#endif

#if defined(LWS_WITH_CLIENT)
	if (wsi->a.vhost)
		lws_dll2_remove(&wsi->dll_cli_active_conns);
#endif

	__lws_same_vh_protocol_remove(wsi);
#if defined(LWS_WITH_CLIENT)
	//lws_free_set_NULL(wsi->stash);
	lws_free_set_NULL(wsi->cli_hostname_copy);
#endif

#if defined(LWS_WITH_PEER_LIMITS)
	lws_peer_track_wsi_close(wsi->a.context, wsi->peer);
	wsi->peer = NULL;
#endif

	/* since we will destroy the wsi, make absolutely sure now */

#if defined(LWS_WITH_OPENSSL)
	__lws_ssl_remove_wsi_from_buffered_list(wsi);
#endif
	__lws_wsi_remove_from_sul(wsi);

	if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_destroy_role))
		lws_rops_func_fidx(wsi->role_ops,
				   LWS_ROPS_destroy_role).destroy_role(wsi);

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	__lws_header_table_detach(wsi, 0);
#endif

#if defined(LWS_ROLE_H2)
	/*
	 * Let's try to clean out the h2-ness of the wsi
	 */

	memset(&wsi->h2, 0, sizeof(wsi->h2));

	wsi->hdr_parsing_completed = wsi->mux_substream =
	wsi->upgraded_to_http2 = wsi->mux_stream_immortal =
	wsi->h2_acked_settings = wsi->seen_nonpseudoheader =
	wsi->socket_is_permanently_unusable = wsi->favoured_pollin =
	wsi->already_did_cce = wsi->told_user_closed =
	wsi->waiting_to_send_close_frame = wsi->close_needs_ack =
	wsi->parent_pending_cb_on_writable = wsi->seen_zero_length_recv =
	wsi->close_when_buffered_out_drained = wsi->could_have_pending = 0;
#endif

#if defined(LWS_WITH_CLIENT)
	wsi->do_ws = wsi->chunked = wsi->client_rx_avail =
	wsi->client_http_body_pending = wsi->transaction_from_pipeline_queue =
	wsi->keepalive_active = wsi->keepalive_rejected =
	wsi->redirected_to_get = wsi->client_pipeline = wsi->client_h2_alpn =
	wsi->client_mux_substream = wsi->client_mux_migrated =
	wsi->tls_session_reused = wsi->perf_done = 0;

	wsi->immortal_substream_count = 0;
#endif
}

/* req cx lock */

void
__lws_free_wsi(struct lws *wsi)
{
	struct lws_vhost *vh;

	if (!wsi)
		return;

	lws_context_assert_lock_held(wsi->a.context);

#if defined(LWS_WITH_SECURE_STREAMS)
	if (wsi->for_ss) {

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
		if (wsi->client_bound_sspc) {
			lws_sspc_handle_t *h = (lws_sspc_handle_t *)
							wsi->a.opaque_user_data;
			if (h) {
				h->cwsi = NULL;
				wsi->a.opaque_user_data = NULL;
			}
		} else
#endif
		{
			/*
			 * Make certain it is disconnected from the ss by now
			 */
			lws_ss_handle_t *h = (lws_ss_handle_t *)
							wsi->a.opaque_user_data;

			if (h) {
				h->wsi = NULL;
				wsi->a.opaque_user_data = NULL;
			}
		}
	}
#endif

	vh = wsi->a.vhost;

	__lws_reset_wsi(wsi);
	__lws_wsi_remove_from_sul(wsi);

	if (vh)
		/* this may destroy vh */
		__lws_vhost_unbind_wsi(wsi); /* req cx + vh lock */

#if defined(LWS_WITH_CLIENT)
	if (wsi->stash)
		lws_free_set_NULL(wsi->stash);
#endif

	if (wsi->a.context->event_loop_ops->destroy_wsi)
		wsi->a.context->event_loop_ops->destroy_wsi(wsi);

	lwsl_wsi_debug(wsi, "tsi fds count %d\n",
			wsi->a.context->pt[(int)wsi->tsi].fds_count);

	/* confirm no sul left scheduled in wsi itself */
	lws_sul_debug_zombies(wsi->a.context, wsi, sizeof(*wsi), __func__);

	__lws_lc_untag(wsi->a.context, &wsi->lc);
	lws_free(wsi);
}


void
lws_remove_child_from_any_parent(struct lws *wsi)
{
	struct lws **pwsi;
	int seen = 0;

	if (!wsi->parent)
		return;

	/* detach ourselves from parent's child list */
	pwsi = &wsi->parent->child_list;
	while (*pwsi) {
		if (*pwsi == wsi) {
			lwsl_wsi_info(wsi, "detach from parent %s",
					    lws_wsi_tag(wsi->parent));

			if (wsi->parent->a.protocol)
				wsi->parent->a.protocol->callback(wsi,
						LWS_CALLBACK_CHILD_CLOSING,
					       wsi->parent->user_space, wsi, 0);

			*pwsi = wsi->sibling_list;
			seen = 1;
			break;
		}
		pwsi = &(*pwsi)->sibling_list;
	}
	if (!seen)
		lwsl_wsi_err(wsi, "failed to detach from parent");

	wsi->parent = NULL;
}

#if defined(LWS_WITH_CLIENT)
void
lws_inform_client_conn_fail(struct lws *wsi, void *arg, size_t len)
{
	lws_addrinfo_clean(wsi);

	if (wsi->already_did_cce)
		return;

	wsi->already_did_cce = 1;

	if (!wsi->a.protocol)
		return;

	if (!wsi->client_suppress_CONNECTION_ERROR)
		wsi->a.protocol->callback(wsi,
					LWS_CALLBACK_CLIENT_CONNECTION_ERROR,
					wsi->user_space, arg, len);
}
#endif

void
lws_addrinfo_clean(struct lws *wsi)
{
#if defined(LWS_WITH_CLIENT)
	struct lws_dll2 *d = lws_dll2_get_head(&wsi->dns_sorted_list), *d1;

	while (d) {
		lws_dns_sort_t *r = lws_container_of(d, lws_dns_sort_t, list);

		d1 = d->next;
		lws_dll2_remove(d);
		lws_free(r);

		d = d1;
	}
#endif
}

/* requires cx and pt lock */

void
__lws_close_free_wsi(struct lws *wsi, enum lws_close_status reason,
		     const char *caller)
{
	struct lws_context_per_thread *pt;
	const struct lws_protocols *pro;
	struct lws_context *context;
	struct lws *wsi1, *wsi2;
	int n, ccb;

	if (!wsi)
		return;

	lwsl_wsi_info(wsi, "caller: %s", caller);

	lws_access_log(wsi);

	if (!lws_dll2_is_detached(&wsi->dll_buflist))
		lwsl_wsi_info(wsi, "going down with stuff in buflist");

	context = wsi->a.context;
	pt = &context->pt[(int)wsi->tsi];

	if (pt->pipe_wsi == wsi)
		pt->pipe_wsi = NULL;

#if defined(LWS_WITH_SYS_METRICS) && \
    (defined(LWS_WITH_CLIENT) || defined(LWS_WITH_SERVER))
	/* wsi level: only reports if dangling caliper */
	if (wsi->cal_conn.mt && wsi->cal_conn.us_start) {
		if ((lws_metrics_priv_to_pub(wsi->cal_conn.mt)->flags) & LWSMTFL_REPORT_HIST) {
			lws_metrics_caliper_report_hist(wsi->cal_conn, (struct lws *)NULL);
		} else {
			lws_metrics_caliper_report(wsi->cal_conn, METRES_NOGO);
			lws_metrics_caliper_done(wsi->cal_conn);
		}
	} else
		lws_metrics_caliper_done(wsi->cal_conn);
#endif

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	if (wsi == context->async_dns.wsi)
		context->async_dns.wsi = NULL;
#endif

	lws_pt_assert_lock_held(pt);

#if defined(LWS_WITH_CLIENT)

	lws_free_set_NULL(wsi->cli_hostname_copy);
	wsi->client_mux_substream_was = wsi->client_mux_substream;

	lws_addrinfo_clean(wsi);
#endif

#if defined(LWS_WITH_HTTP2)
	if (wsi->mux_stream_immortal)
		lws_http_close_immortal(wsi);
#endif

	/* if we have children, close them first */
	if (wsi->child_list) {
		wsi2 = wsi->child_list;
		while (wsi2) {
			wsi1 = wsi2->sibling_list;
//			wsi2->parent = NULL;
			/* stop it doing shutdown processing */
			wsi2->socket_is_permanently_unusable = 1;
			__lws_close_free_wsi(wsi2, reason,
					     "general child recurse");
			wsi2 = wsi1;
		}
		wsi->child_list = NULL;
	}

#if defined(LWS_ROLE_RAW_FILE)
	if (wsi->role_ops == &role_ops_raw_file) {
		lws_remove_child_from_any_parent(wsi);
		__remove_wsi_socket_from_fds(wsi);
		if (wsi->a.protocol)
			wsi->a.protocol->callback(wsi, wsi->role_ops->close_cb[0],
					wsi->user_space, NULL, 0);
		goto async_close;
	}
#endif

	wsi->wsistate_pre_close = wsi->wsistate;

#ifdef LWS_WITH_CGI
	if (wsi->role_ops == &role_ops_cgi) {

		// lwsl_debug("%s: closing stdwsi index %d\n", __func__, (int)wsi->lsp_channel);

		/* we are not a network connection, but a handler for CGI io */
		if (wsi->parent && wsi->parent->http.cgi) {

			/*
			 * We need to keep the logical cgi around so we can
			 * drain it
			 */

//			if (wsi->parent->child_list == wsi && !wsi->sibling_list)
//				lws_cgi_remove_and_kill(wsi->parent);

			/* end the binding between us and network connection */
			if (wsi->parent->http.cgi && wsi->parent->http.cgi->lsp)
				wsi->parent->http.cgi->lsp->stdwsi[(int)wsi->lsp_channel] =
									NULL;
		}
		wsi->socket_is_permanently_unusable = 1;

		goto just_kill_connection;
	}

	if (wsi->http.cgi)
		lws_cgi_remove_and_kill(wsi);
#endif

#if defined(LWS_WITH_CLIENT)
	if (!wsi->close_is_redirect)
		lws_free_set_NULL(wsi->stash);
#endif

	if (wsi->role_ops == &role_ops_raw_skt) {
		wsi->socket_is_permanently_unusable = 1;
		goto just_kill_connection;
	}
#if defined(LWS_WITH_FILE_OPS) && (defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2))
	if (lwsi_role_http(wsi) && lwsi_role_server(wsi) &&
	    wsi->http.fop_fd != NULL)
		lws_vfs_file_close(&wsi->http.fop_fd);
#endif

	if (lwsi_state(wsi) == LRS_DEAD_SOCKET)
		return;

	if (wsi->socket_is_permanently_unusable ||
	    reason == LWS_CLOSE_STATUS_NOSTATUS_CONTEXT_DESTROY ||
	    lwsi_state(wsi) == LRS_SHUTDOWN)
		goto just_kill_connection;

	switch (lwsi_state_PRE_CLOSE(wsi)) {
	case LRS_DEAD_SOCKET:
		return;

	/* we tried the polite way... */
	case LRS_WAITING_TO_SEND_CLOSE:
	case LRS_AWAITING_CLOSE_ACK:
	case LRS_RETURNED_CLOSE:
		goto just_kill_connection;

	case LRS_FLUSHING_BEFORE_CLOSE:
		if (lws_has_buffered_out(wsi)
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
		    || wsi->http.comp_ctx.buflist_comp ||
		    wsi->http.comp_ctx.may_have_more
#endif
		 ) {
			lws_callback_on_writable(wsi);
			return;
		}
		lwsl_wsi_info(wsi, " end LRS_FLUSHING_BEFORE_CLOSE");
		goto just_kill_connection;
	default:
		if (lws_has_buffered_out(wsi)
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
				|| wsi->http.comp_ctx.buflist_comp ||
		    wsi->http.comp_ctx.may_have_more
#endif
		) {
			lwsl_wsi_info(wsi, "LRS_FLUSHING_BEFORE_CLOSE");
			lwsi_set_state(wsi, LRS_FLUSHING_BEFORE_CLOSE);
			__lws_set_timeout(wsi,
				PENDING_FLUSH_STORED_SEND_BEFORE_CLOSE, 5);
			return;
		}
		break;
	}

	if (lwsi_state(wsi) == LRS_WAITING_CONNECT ||
	    lwsi_state(wsi) == LRS_WAITING_DNS ||
	    lwsi_state(wsi) == LRS_H1C_ISSUE_HANDSHAKE)
		goto just_kill_connection;

	if (!wsi->told_user_closed && wsi->user_space && wsi->a.protocol &&
	    wsi->protocol_bind_balance) {
		wsi->a.protocol->callback(wsi,
				wsi->role_ops->protocol_unbind_cb[
				       !!lwsi_role_server(wsi)],
				       wsi->user_space, (void *)__func__, 0);
		wsi->protocol_bind_balance = 0;
	}

	/*
	 * signal we are closing, lws_write will
	 * add any necessary version-specific stuff.  If the write fails,
	 * no worries we are closing anyway.  If we didn't initiate this
	 * close, then our state has been changed to
	 * LRS_RETURNED_CLOSE and we will skip this.
	 *
	 * Likewise if it's a second call to close this connection after we
	 * sent the close indication to the peer already, we are in state
	 * LRS_AWAITING_CLOSE_ACK and will skip doing this a second time.
	 */

	if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_close_via_role_protocol) &&
	    lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_close_via_role_protocol).
					 close_via_role_protocol(wsi, reason)) {
		lwsl_wsi_info(wsi, "close_via_role took over (sockfd %d)",
			      wsi->desc.sockfd);
		return;
	}

just_kill_connection:

	lwsl_wsi_debug(wsi, "real just_kill_connection A: (sockfd %d)",
			wsi->desc.sockfd);

#if defined(LWS_WITH_THREADPOOL) && defined(LWS_HAVE_PTHREAD_H)
	lws_threadpool_wsi_closing(wsi);
#endif

#if defined(LWS_WITH_FILE_OPS) && (defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2))
	if (lwsi_role_http(wsi) && lwsi_role_server(wsi) &&
	    wsi->http.fop_fd != NULL)
		lws_vfs_file_close(&wsi->http.fop_fd);
#endif

	lws_sul_cancel(&wsi->sul_connect_timeout);
#if defined(LWS_WITH_SYS_ASYNC_DNS)
	lws_async_dns_cancel(wsi);
#endif

#if defined(LWS_WITH_HTTP_PROXY)
	if (wsi->http.buflist_post_body)
		lws_buflist_destroy_all_segments(&wsi->http.buflist_post_body);
#endif
#if defined(LWS_WITH_UDP)
	if (wsi->udp) {
		/* confirm no sul left scheduled in wsi->udp itself */
		lws_sul_debug_zombies(wsi->a.context, wsi->udp,
					sizeof(*wsi->udp), "close udp wsi");

		lws_free_set_NULL(wsi->udp);
	}
#endif

	if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_close_kill_connection))
		lws_rops_func_fidx(wsi->role_ops,
				   LWS_ROPS_close_kill_connection).
					    close_kill_connection(wsi, reason);

	n = 0;

	if (!wsi->told_user_closed && wsi->user_space &&
	    wsi->protocol_bind_balance && wsi->a.protocol) {
		lwsl_debug("%s: %s: DROP_PROTOCOL %s\n", __func__, lws_wsi_tag(wsi),
			   wsi->a.protocol ? wsi->a.protocol->name: "NULL");
		if (wsi->a.protocol)
			wsi->a.protocol->callback(wsi,
				wsi->role_ops->protocol_unbind_cb[
				       !!lwsi_role_server(wsi)],
				       wsi->user_space, (void *)__func__, 0);
		wsi->protocol_bind_balance = 0;
	}

#if defined(LWS_WITH_CLIENT)
	if ((
#if defined(LWS_ROLE_WS)
		/*
		 * If our goal is a ws upgrade, effectively we did not reach
		 * ESTABLISHED if we did not get the upgrade server reply
		 */
		(lwsi_state(wsi) == LRS_WAITING_SERVER_REPLY &&
		 wsi->role_ops == &role_ops_ws) ||
#endif
	     lwsi_state(wsi) == LRS_WAITING_DNS ||
	     lwsi_state(wsi) == LRS_WAITING_CONNECT) &&
	     !wsi->already_did_cce && wsi->a.protocol &&
	     !wsi->close_is_redirect) {
		static const char _reason[] = "closed before established";

		lwsl_wsi_debug(wsi, "closing in unestablished state 0x%x",
				lwsi_state(wsi));
		wsi->socket_is_permanently_unusable = 1;

		lws_inform_client_conn_fail(wsi,
			(void *)_reason, sizeof(_reason));
	}
#endif

	/*
	 * Testing with ab shows that we have to stage the socket close when
	 * the system is under stress... shutdown any further TX, change the
	 * state to one that won't emit anything more, and wait with a timeout
	 * for the POLLIN to show a zero-size rx before coming back and doing
	 * the actual close.
	 */
	if (wsi->role_ops != &role_ops_raw_skt && !lwsi_role_client(wsi) &&
	    lwsi_state(wsi) != LRS_SHUTDOWN &&
	    lwsi_state(wsi) != LRS_UNCONNECTED &&
	    reason != LWS_CLOSE_STATUS_NOSTATUS_CONTEXT_DESTROY &&
	    !wsi->socket_is_permanently_unusable) {

#if defined(LWS_WITH_TLS)
		if (lws_is_ssl(wsi) && wsi->tls.ssl) {
			n = 0;
			switch (__lws_tls_shutdown(wsi)) {
			case LWS_SSL_CAPABLE_DONE:
			case LWS_SSL_CAPABLE_ERROR:
			case LWS_SSL_CAPABLE_MORE_SERVICE_READ:
			case LWS_SSL_CAPABLE_MORE_SERVICE_WRITE:
			case LWS_SSL_CAPABLE_MORE_SERVICE:
				break;
			}
		} else
#endif
		{
			lwsl_info("%s: shutdown conn: %s (sk %d, state 0x%x)\n",
				  __func__, lws_wsi_tag(wsi), (int)(lws_intptr_t)wsi->desc.sockfd,
				  lwsi_state(wsi));
			if (!wsi->socket_is_permanently_unusable &&
			    lws_socket_is_valid(wsi->desc.sockfd)) {
				wsi->socket_is_permanently_unusable = 1;
				n = shutdown(wsi->desc.sockfd, SHUT_WR);
			}
		}
		if (n)
			lwsl_wsi_debug(wsi, "closing: shutdown (state 0x%x) ret %d",
				   lwsi_state(wsi), LWS_ERRNO);

		/*
		 * This causes problems on WINCE / ESP32 with disconnection
		 * when the events are half closing connection
		 */
#if !defined(_WIN32_WCE) && !defined(LWS_PLAT_FREERTOS)
		/* libuv: no event available to guarantee completion */
		if (!wsi->socket_is_permanently_unusable &&
#if defined(LWS_WITH_CLIENT)
		    !wsi->close_is_redirect &&
#endif
		    lws_socket_is_valid(wsi->desc.sockfd) &&
		    lwsi_state(wsi) != LRS_SHUTDOWN &&
		    (context->event_loop_ops->flags & LELOF_ISPOLL)) {
			__lws_change_pollfd(wsi, LWS_POLLOUT, LWS_POLLIN);
			lwsi_set_state(wsi, LRS_SHUTDOWN);
			__lws_set_timeout(wsi, PENDING_TIMEOUT_SHUTDOWN_FLUSH,
					  (int)context->timeout_secs);

			return;
		}
#endif
	}

	lwsl_wsi_info(wsi, "real just_kill_connection: sockfd %d\n",
			wsi->desc.sockfd);

#ifdef LWS_WITH_HUBBUB
	if (wsi->http.rw) {
		lws_rewrite_destroy(wsi->http.rw);
		wsi->http.rw = NULL;
	}
#endif

	if (wsi->http.pending_return_headers)
		lws_free_set_NULL(wsi->http.pending_return_headers);

	/*
	 * we won't be servicing or receiving anything further from this guy
	 * delete socket from the internal poll list if still present
	 */
	__lws_ssl_remove_wsi_from_buffered_list(wsi);
	__lws_wsi_remove_from_sul(wsi);

	//if (wsi->told_event_loop_closed) // cgi std close case (dummy-callback)
	//	return;

	/* checking return redundant since we anyway close */
	__remove_wsi_socket_from_fds(wsi);

	lwsi_set_state(wsi, LRS_DEAD_SOCKET);
	lws_buflist_destroy_all_segments(&wsi->buflist);
	lws_dll2_remove(&wsi->dll_buflist);

	if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_close_role))
		lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_close_role).
							close_role(pt, wsi);

	/* tell the user it's all over for this guy */

	ccb = 0;
	if ((lwsi_state_est_PRE_CLOSE(wsi) ||
	    /* raw skt adopted but didn't complete tls hs should CLOSE */
	    (wsi->role_ops == &role_ops_raw_skt && !lwsi_role_client(wsi)) ||
	     lwsi_state_PRE_CLOSE(wsi) == LRS_WAITING_SERVER_REPLY) &&
	    !wsi->told_user_closed &&
	    wsi->role_ops->close_cb[lwsi_role_server(wsi)]) {
		if (!wsi->upgraded_to_http2 || !lwsi_role_client(wsi))
			ccb = 1;
			/*
			 * The network wsi for a client h2 connection shouldn't
			 * call back for its role: the child stream connections
			 * own the role.  Otherwise h2 will call back closed
			 * one too many times as the children do it and then
			 * the closing network stream.
			 */
	}

	if (!wsi->told_user_closed &&
	    !lws_dll2_is_detached(&wsi->vh_awaiting_socket))
		/*
		 * He's a guy who go started with dns, but failed or is
		 * caught with a shutdown before he got the result.  We have
		 * to issclient_mux_substream_wasue him a close cb
		 */
		ccb = 1;

	lwsl_wsi_info(wsi, "cce=%d", ccb);

	pro = wsi->a.protocol;

	if (wsi->already_did_cce)
		/*
		 * If we handled this by CLIENT_CONNECTION_ERROR, it's
		 * mutually exclusive with CLOSE
		 */
		ccb = 0;

#if defined(LWS_WITH_CLIENT)
	if (!wsi->close_is_redirect && !ccb &&
	    (lwsi_state_PRE_CLOSE(wsi) & LWSIFS_NOT_EST) &&
			lwsi_role_client(wsi)) {
		lws_inform_client_conn_fail(wsi, "Closed before conn", 18);
	}
#endif
	if (ccb
#if defined(LWS_WITH_CLIENT)
			&& !wsi->close_is_redirect
#endif
	) {

		if (!wsi->a.protocol && wsi->a.vhost && wsi->a.vhost->protocols)
			pro = &wsi->a.vhost->protocols[0];

		if (pro)
			pro->callback(wsi,
				wsi->role_ops->close_cb[lwsi_role_server(wsi)],
				wsi->user_space, NULL, 0);
		wsi->told_user_closed = 1;
	}

#if defined(LWS_ROLE_RAW_FILE)
async_close:
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
	if (wsi->for_ss) {
		lwsl_wsi_debug(wsi, "for_ss");
		/*
		 * We were adopted for a particular ss, but, eg, we may not
		 * have succeeded with the connection... we are closing which is
		 * good, but we have to invalidate any pointer the related ss
		 * handle may be holding on us
		 */
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)

		if (wsi->client_proxy_onward) {
			/*
			 * We are an onward proxied wsi at the proxy,
			 * opaque is proxing "conn", we must remove its pointer
			 * to us since we are destroying
			 */
			lws_proxy_clean_conn_ss(wsi);
		} else

			if (wsi->client_bound_sspc) {
				lws_sspc_handle_t *h = (lws_sspc_handle_t *)wsi->a.opaque_user_data;

				if (h) { // && (h->info.flags & LWSSSINFLAGS_ACCEPTED)) {

#if defined(LWS_WITH_SYS_METRICS)
					/*
					 * If any hanging caliper measurement, dump it, and free any tags
					 */
					lws_metrics_caliper_report_hist(h->cal_txn, (struct lws *)NULL);
#endif

					h->cwsi = NULL;
					//wsi->a.opaque_user_data = NULL;
				}
			} else
#endif
		{
			lws_ss_handle_t *h = (lws_ss_handle_t *)wsi->a.opaque_user_data;

			if (h) { // && (h->info.flags & LWSSSINFLAGS_ACCEPTED)) {

				/*
				 * ss level: only reports if dangling caliper
				 * not already reported
				 */
				lws_metrics_caliper_report_hist(h->cal_txn, wsi);

				h->wsi = NULL;
				wsi->a.opaque_user_data = NULL;

				if (h->ss_dangling_connected &&
				    lws_ss_event_helper(h, LWSSSCS_DISCONNECTED) ==
						    LWSSSSRET_DESTROY_ME) {

					lws_ss_destroy(&h);
				}
			}
		}
	}
#endif


	lws_remove_child_from_any_parent(wsi);
	wsi->socket_is_permanently_unusable = 1;

	if (wsi->a.context->event_loop_ops->wsi_logical_close)
		if (wsi->a.context->event_loop_ops->wsi_logical_close(wsi))
			return;

	__lws_close_free_wsi_final(wsi);
}


/* cx + vh lock */

void
__lws_close_free_wsi_final(struct lws *wsi)
{
	int n;

	if (!wsi->shadow &&
	    lws_socket_is_valid(wsi->desc.sockfd) && !lws_ssl_close(wsi)) {
		lwsl_wsi_debug(wsi, "fd %d", wsi->desc.sockfd);
		n = compatible_close(wsi->desc.sockfd);
		if (n)
			lwsl_wsi_debug(wsi, "closing: close ret %d", LWS_ERRNO);

		__remove_wsi_socket_from_fds(wsi);
		if (lws_socket_is_valid(wsi->desc.sockfd))
			delete_from_fd(wsi->a.context, wsi->desc.sockfd);

#if !defined(LWS_PLAT_FREERTOS) && !defined(WIN32) && !defined(LWS_PLAT_OPTEE)
		delete_from_fdwsi(wsi->a.context, wsi);
#endif

		sanity_assert_no_sockfd_traces(wsi->a.context, wsi->desc.sockfd);
	}

	/* ... if we're closing the cancel pipe, account for it */

	{
		struct lws_context_per_thread *pt =
				&wsi->a.context->pt[(int)wsi->tsi];

		if (pt->pipe_wsi == wsi)
			pt->pipe_wsi = NULL;
		if (pt->dummy_pipe_fds[0] == wsi->desc.sockfd)
			pt->dummy_pipe_fds[0] = LWS_SOCK_INVALID;
	}

	wsi->desc.sockfd = LWS_SOCK_INVALID;

#if defined(LWS_WITH_CLIENT)
	lws_free_set_NULL(wsi->cli_hostname_copy);
	if (wsi->close_is_redirect) {

		wsi->close_is_redirect = 0;

		lwsl_wsi_info(wsi, "picking up redirection");

		lws_role_transition(wsi, LWSIFR_CLIENT, LRS_UNCONNECTED,
				    &role_ops_h1);

#if defined(LWS_WITH_HTTP2)
		if (wsi->client_mux_substream_was)
			wsi->h2.END_STREAM = wsi->h2.END_HEADERS = 0;
#endif
#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
		if (wsi->mux.parent_wsi) {
			lws_wsi_mux_sibling_disconnect(wsi);
			wsi->mux.parent_wsi = NULL;
		}
#endif

#if defined(LWS_WITH_TLS)
		memset(&wsi->tls, 0, sizeof(wsi->tls));
#endif

	//	wsi->a.protocol = NULL;
		if (wsi->a.protocol)
			lws_bind_protocol(wsi, wsi->a.protocol, "client_reset");
		wsi->pending_timeout = NO_PENDING_TIMEOUT;
		wsi->hdr_parsing_completed = 0;

#if defined(LWS_WITH_TLS)
		if (wsi->stash->cis[CIS_ALPN])
			lws_strncpy(wsi->alpn, wsi->stash->cis[CIS_ALPN],
				    sizeof(wsi->alpn));
#endif

		if (lws_header_table_attach(wsi, 0)) {
			lwsl_wsi_err(wsi, "failed to get ah");
			return;
		}
//		}
		//_lws_header_table_reset(wsi->http.ah);

#if defined(LWS_WITH_TLS)
		wsi->tls.use_ssl = (unsigned int)wsi->flags;
#endif

#if defined(LWS_WITH_TLS_JIT_TRUST)
		if (wsi->stash && wsi->stash->cis[CIS_ADDRESS]) {
			struct lws_vhost *vh = NULL;
			lws_tls_jit_trust_vhost_bind(wsi->a.context,
						     wsi->stash->cis[CIS_ADDRESS],
						     &vh);
			if (vh) {
				if (!vh->count_bound_wsi && vh->grace_after_unref) {
					lwsl_wsi_info(wsi, "%s in use\n",
								vh->lc.gutag);
					lws_sul_cancel(&vh->sul_unref);
				}
				vh->count_bound_wsi++;
				wsi->a.vhost = vh;
			}
		}
#endif

		return;
	}
#endif

	/* outermost destroy notification for wsi (user_space still intact) */
	if (wsi->a.vhost)
		wsi->a.vhost->protocols[0].callback(wsi, LWS_CALLBACK_WSI_DESTROY,
						  wsi->user_space, NULL, 0);

#ifdef LWS_WITH_CGI
	if (wsi->http.cgi) {
		lws_spawn_piped_destroy(&wsi->http.cgi->lsp);
		lws_sul_cancel(&wsi->http.cgi->sul_grace);
		lws_free_set_NULL(wsi->http.cgi);
	}
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_destroy(&wsi->fic);
#endif

	__lws_wsi_remove_from_sul(wsi);
	sanity_assert_no_wsi_traces(wsi->a.context, wsi);
	__lws_free_wsi(wsi);
}


void
lws_close_free_wsi(struct lws *wsi, enum lws_close_status reason, const char *caller)
{
	struct lws_context *cx = wsi->a.context;
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	lws_context_lock(cx, __func__);

	lws_pt_lock(pt, __func__);
	/* may destroy vhost, cannot hold vhost lock outside it */
	__lws_close_free_wsi(wsi, reason, caller);
	lws_pt_unlock(pt);

	lws_context_unlock(cx);
}


