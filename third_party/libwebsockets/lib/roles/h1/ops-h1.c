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

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif


/*
 * We have to take care about parsing because the headers may be split
 * into multiple fragments.  They may contain unknown headers with arbitrary
 * argument lengths.  So, we parse using a single-character at a time state
 * machine that is completely independent of packet size.
 *
 * Returns <0 for error or length of chars consumed from buf (up to len)
 */

int
lws_read_h1(struct lws *wsi, unsigned char *buf, lws_filepos_t len)
{
	unsigned char *last_char, *oldbuf = buf;
	lws_filepos_t body_chunk_len;
	size_t n;

	lwsl_debug("%s: h1 path: wsi state 0x%x\n", __func__, lwsi_state(wsi));

	switch (lwsi_state(wsi)) {

	case LRS_ISSUING_FILE:
		return 0;

	case LRS_ESTABLISHED:

		if (lwsi_role_ws(wsi))
			goto ws_mode;

		if (lwsi_role_client(wsi))
			break;

		wsi->hdr_parsing_completed = 0;

		/* fallthru */

	case LRS_HEADERS:
		if (!wsi->http.ah) {
			lwsl_err("%s: LRS_HEADERS: NULL ah\n", __func__);
			assert(0);
		}
		lwsl_parser("issuing %d bytes to parser\n", (int)len);
#if defined(LWS_ROLE_WS) && defined(LWS_WITH_CLIENT)
		if (lws_ws_handshake_client(wsi, &buf, (size_t)len))
			goto bail;
#endif
		last_char = buf;
		if (lws_handshake_server(wsi, &buf, (size_t)len))
			/* Handshake indicates this session is done. */
			goto bail;

		/* we might have transitioned to RAW */
		if (wsi->role_ops == &role_ops_raw_skt
#if defined(LWS_ROLE_RAW_FILE)
				||
		    wsi->role_ops == &role_ops_raw_file
#endif
		    )
			 /* we gave the read buffer to RAW handler already */
			goto read_ok;

		/*
		 * It's possible that we've exhausted our data already, or
		 * rx flow control has stopped us dealing with this early,
		 * but lws_handshake_server doesn't update len for us.
		 * Figure out how much was read, so that we can proceed
		 * appropriately:
		 */
		len -= (unsigned int)lws_ptr_diff(buf, last_char);

		if (!wsi->hdr_parsing_completed)
			/* More header content on the way */
			goto read_ok;

		switch (lwsi_state(wsi)) {
			case LRS_ESTABLISHED:
			case LRS_HEADERS:
				goto read_ok;
			case LRS_ISSUING_FILE:
				goto read_ok;
			case LRS_DISCARD_BODY:
			case LRS_BODY:
				wsi->http.rx_content_remain =
						wsi->http.rx_content_length;
				if (wsi->http.rx_content_remain)
					goto http_postbody;

				/* there is no POST content */
				goto postbody_completion;
			default:
				break;
		}
		break;

	case LRS_DISCARD_BODY:
	case LRS_BODY:
http_postbody:
		lwsl_info("%s: http post body: cl set %d, remain %d, len %d\n", __func__,
			    (int)wsi->http.content_length_given,
			    (int)wsi->http.rx_content_remain, (int)len);

		if (wsi->http.content_length_given && !wsi->http.rx_content_remain)
			goto postbody_completion;

		while (len && (!wsi->http.content_length_given || wsi->http.rx_content_remain)) {
			/* Copy as much as possible, up to the limit of:
			 * what we have in the read buffer (len)
			 * remaining portion of the POST body (content_remain)
			 */
			if (wsi->http.content_length_given)
				body_chunk_len = min(wsi->http.rx_content_remain, len);
			else
				body_chunk_len = len;
			wsi->http.rx_content_remain -= body_chunk_len;
			// len -= body_chunk_len;
#ifdef LWS_WITH_CGI
			if (wsi->http.cgi) {
				struct lws_cgi_args args;

				args.ch = LWS_STDIN;
				args.stdwsi = &wsi->http.cgi->lsp->stdwsi[0];
				args.data = buf;
				args.len = (int)(unsigned int)body_chunk_len;

				/* returns how much used */
				n = (unsigned int)user_callback_handle_rxflow(
					wsi->a.protocol->callback,
					wsi, LWS_CALLBACK_CGI_STDIN_DATA,
					wsi->user_space,
					(void *)&args, 0);
				if ((int)n < 0)
					goto bail;
			} else {
#endif
				if (lwsi_state(wsi) != LRS_DISCARD_BODY) {
					lwsl_info("%s: HTTP_BODY %d\n", __func__, (int)body_chunk_len);
					n = (unsigned int)wsi->a.protocol->callback(wsi,
						LWS_CALLBACK_HTTP_BODY, wsi->user_space,
						buf, (size_t)body_chunk_len);
					if (n)
						goto bail;
				}
				n = (size_t)body_chunk_len;
#ifdef LWS_WITH_CGI
			}
#endif
			lwsl_info("%s: advancing buf by %d\n", __func__, (int)n);
			buf += n;

#if defined(LWS_ROLE_H2)
			if (lwsi_role_h2(wsi) && !wsi->http.content_length_given) {
				struct lws *w = lws_get_network_wsi(wsi);

				if (w)
					lwsl_info("%s: h2: nwsi h2 flags %d\n", __func__,
						w->h2.h2n ? w->h2.h2n->flags: -1);

				if (w && w->h2.h2n && !(w->h2.h2n->flags & 1)) {
					lwsl_info("%s: h2, no cl, not END_STREAM, continuing\n", __func__);
					lws_set_timeout(wsi,
						PENDING_TIMEOUT_HTTP_CONTENT,
						(int)wsi->a.context->timeout_secs);
					break;
				}
				goto postbody_completion;
			}
#endif

			if (wsi->http.rx_content_remain)  {
				lws_set_timeout(wsi,
						PENDING_TIMEOUT_HTTP_CONTENT,
						(int)wsi->a.context->timeout_secs);
				break;
			}
			/* he sent all the content in time */
postbody_completion:
#ifdef LWS_WITH_CGI
			/*
			 * If we're running a cgi, we can't let him off the
			 * hook just because he sent his POST data
			 */
			if (wsi->http.cgi)
				lws_set_timeout(wsi, PENDING_TIMEOUT_CGI,
						(int)wsi->a.context->timeout_secs);
			else
#endif
			lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);
#ifdef LWS_WITH_CGI
			if (!wsi->http.cgi)
#endif
			{
#if defined(LWS_WITH_SERVER)
				if (lwsi_state(wsi) == LRS_DISCARD_BODY) {
					/*
					 * repeat the transaction completed
					 * that got us into this state, having
					 * consumed the pending body now
					 */

					if (lws_http_transaction_completed(wsi))
						goto bail;
					break;
				}
#endif
				lwsl_info("HTTP_BODY_COMPLETION: %s (%s)\n",
					  lws_wsi_tag(wsi), wsi->a.protocol->name);

				n = (unsigned int)wsi->a.protocol->callback(wsi,
					LWS_CALLBACK_HTTP_BODY_COMPLETION,
					wsi->user_space, NULL, 0);
				if (n) {
					lwsl_info("%s: bailing after BODY_COMPLETION\n", __func__);
					goto bail;
				}

				if (wsi->mux_substream)
					lwsi_set_state(wsi, LRS_ESTABLISHED);
			}

			break;
		}
		break;

	case LRS_RETURNED_CLOSE:
	case LRS_AWAITING_CLOSE_ACK:
	case LRS_WAITING_TO_SEND_CLOSE:
	case LRS_SHUTDOWN:

ws_mode:
#if defined(LWS_WITH_CLIENT) && defined(LWS_ROLE_WS)
		// lwsl_notice("%s: ws_mode\n", __func__);
		if (lws_ws_handshake_client(wsi, &buf, (size_t)len))
			goto bail;
#endif
#if defined(LWS_ROLE_WS)
		if (lwsi_role_ws(wsi) && lwsi_role_server(wsi) &&
			/*
			 * for h2 we are on the swsi
			 */
		    lws_parse_ws(wsi, &buf, (size_t)len) < 0) {
			lwsl_info("%s: lws_parse_ws bailed\n", __func__);
			goto bail;
		}
#endif
		// lwsl_notice("%s: ws_mode: buf moved on by %d\n", __func__,
		//	       lws_ptr_diff(buf, oldbuf));
		break;

	case LRS_DEFERRING_ACTION:
		lwsl_notice("%s: LRS_DEFERRING_ACTION\n", __func__);
		break;

	case LRS_SSL_ACK_PENDING:
		break;

	case LRS_FLUSHING_BEFORE_CLOSE:
		break;

	case LRS_DEAD_SOCKET:
		lwsl_err("%s: Unhandled state LRS_DEAD_SOCKET\n", __func__);
		goto bail;
		// assert(0);
		/* fallthru */

	case LRS_WAITING_CONNECT: /* observed on warmcat.com */
		break;

	default:
		lwsl_err("%s: Unhandled state %d\n", __func__, lwsi_state(wsi));
		goto bail;
	}

read_ok:
	/* Nothing more to do for now */
//	lwsl_info("%s: %p: read_ok, used %ld (len %d, state %d)\n", __func__,
//		  wsi, (long)(buf - oldbuf), (int)len, wsi->state);

	return lws_ptr_diff(buf, oldbuf);

bail:
	/*
	 * h2 / h2-ws calls us recursively in
	 *
	 * lws_read_h1()->
	 *   lws_h2_parser()->
	 *     lws_read_h1()
	 *
	 * pattern, having stripped the h2 framing in the middle.
	 *
	 * When taking down the whole connection, make sure that only the
	 * outer lws_read() does the wsi close.
	 */
	if (!wsi->outer_will_close)
		lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
				   "lws_read_h1 bail");

	return -1;
}
#if defined(LWS_WITH_SERVER)
static int
lws_h1_server_socket_service(struct lws *wsi, struct lws_pollfd *pollfd)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_tokens ebuf;
	int n, buffered;

	if (lwsi_state(wsi) == LRS_DEFERRING_ACTION)
		goto try_pollout;

	/* any incoming data ready? */

	if (!(pollfd->revents & pollfd->events & LWS_POLLIN))
		goto try_pollout;

	/*
	 * If we previously just did POLLIN when IN and OUT were signaled
	 * (because POLLIN processing may have used up the POLLOUT), don't let
	 * that happen twice in a row... next time we see the situation favour
	 * POLLOUT
	 */

	if (wsi->favoured_pollin &&
	    (pollfd->revents & pollfd->events & LWS_POLLOUT)) {
		// lwsl_notice("favouring pollout\n");
		wsi->favoured_pollin = 0;
		goto try_pollout;
	}

	/*
	 * We haven't processed that the tunnel is set up yet, so
	 * defer reading
	 */

	if (lwsi_state(wsi) == LRS_SSL_ACK_PENDING)
		return LWS_HPI_RET_HANDLED;

	/* these states imply we MUST have an ah attached */

	if ((lwsi_state(wsi) == LRS_ESTABLISHED ||
	     lwsi_state(wsi) == LRS_ISSUING_FILE ||
	     lwsi_state(wsi) == LRS_HEADERS ||
	     lwsi_state(wsi) == LRS_DOING_TRANSACTION || /* at least, SSE */
	     lwsi_state(wsi) == LRS_DISCARD_BODY ||
	     lwsi_state(wsi) == LRS_BODY)) {

		if (!wsi->http.ah && lws_header_table_attach(wsi, 0)) {
			lwsl_info("%s: %s: ah not available\n", __func__,
				  lws_wsi_tag(wsi));
			goto try_pollout;
		}

		/*
		 * We got here because there was specifically POLLIN...
		 * regardless of our buflist state, we need to get it,
		 * and either use it, or append to the buflist and use
		 * buflist head material.
		 *
		 * We will not notice a connection close until the buflist is
		 * exhausted and we tried to do a read of some kind.
		 */

		ebuf.token = NULL;
		ebuf.len = 0;
		buffered = lws_buflist_aware_read(pt, wsi, &ebuf, 0, __func__);
		switch (ebuf.len) {
		case 0:
			lwsl_info("%s: read 0 len a\n", __func__);
			wsi->seen_zero_length_recv = 1;
			if (lws_change_pollfd(wsi, LWS_POLLIN, 0))
				goto fail;
#if !defined(LWS_WITHOUT_EXTENSIONS)
			/*
			 * autobahn requires us to win the race between close
			 * and draining the extensions
			 */
			if (wsi->ws &&
			    (wsi->ws->rx_draining_ext ||
			     wsi->ws->tx_draining_ext))
				goto try_pollout;
#endif
			/*
			 * normally, we respond to close with logically closing
			 * our side immediately
			 */
			goto fail;

		case LWS_SSL_CAPABLE_ERROR:
			goto fail;
		case LWS_SSL_CAPABLE_MORE_SERVICE:
			goto try_pollout;
		}

		/* just ignore incoming if waiting for close */
		if (lwsi_state(wsi) == LRS_FLUSHING_BEFORE_CLOSE) {
			lwsl_notice("%s: just ignoring\n", __func__);
			goto try_pollout;
		}

		if (lwsi_state(wsi) == LRS_ISSUING_FILE) {
			// lwsl_notice("stashing: wsi %p: bd %d\n", wsi, buffered);
			if (lws_buflist_aware_finished_consuming(wsi, &ebuf, 0,
							buffered, __func__))
				return LWS_HPI_RET_PLEASE_CLOSE_ME;

			goto try_pollout;
		}

		/*
		 * Otherwise give it to whoever wants it according to the
		 * connection state
		 */
#if defined(LWS_ROLE_H2)
		if (lwsi_role_h2(wsi) && lwsi_state(wsi) != LRS_BODY)
			n = lws_read_h2(wsi, ebuf.token, (unsigned int)ebuf.len);
		else
#endif
			n = lws_read_h1(wsi, ebuf.token, (unsigned int)ebuf.len);
		if (n < 0) /* we closed wsi */
			return LWS_HPI_RET_WSI_ALREADY_DIED;

		// lwsl_notice("%s: consumed %d\n", __func__, n);

		if (lws_buflist_aware_finished_consuming(wsi, &ebuf, n,
							 buffered, __func__))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;

		/*
		 * during the parsing our role changed to something non-http,
		 * so the ah has no further meaning
		 */

		if (wsi->http.ah &&
		    !lwsi_role_h1(wsi) &&
		    !lwsi_role_h2(wsi) &&
		    !lwsi_role_cgi(wsi))
			lws_header_table_detach(wsi, 0);

		/*
		 * He may have used up the writability above, if we will defer
		 * POLLOUT processing in favour of POLLIN, note it
		 */

		if (pollfd->revents & LWS_POLLOUT)
			wsi->favoured_pollin = 1;

		return LWS_HPI_RET_HANDLED;
	}

	/*
	 * He may have used up the writability above, if we will defer POLLOUT
	 * processing in favour of POLLIN, note it
	 */

	if (pollfd->revents & LWS_POLLOUT)
		wsi->favoured_pollin = 1;

try_pollout:

	/* this handles POLLOUT for http serving fragments */

	if (!(pollfd->revents & LWS_POLLOUT))
		return LWS_HPI_RET_HANDLED;

	/* one shot */
	if (lws_change_pollfd(wsi, LWS_POLLOUT, 0)) {
		lwsl_notice("%s a\n", __func__);
		goto fail;
	}

	/* clear back-to-back write detection */
	wsi->could_have_pending = 0;

	if (lwsi_state(wsi) == LRS_DEFERRING_ACTION) {
		lwsl_debug("%s: LRS_DEFERRING_ACTION now writable\n", __func__);

		lwsi_set_state(wsi, LRS_ESTABLISHED);
		if (lws_change_pollfd(wsi, LWS_POLLOUT, 0)) {
			lwsl_info("failed at set pollfd\n");
			goto fail;
		}
	}

	if (!wsi->hdr_parsing_completed)
		return LWS_HPI_RET_HANDLED;

	if (lwsi_state(wsi) != LRS_ISSUING_FILE) {

		if (lws_has_buffered_out(wsi)) {
			//lwsl_notice("%s: completing partial\n", __func__);
			if (lws_issue_raw(wsi, NULL, 0) < 0) {
				lwsl_info("%s signalling to close\n", __func__);
				goto fail;
			}
			return LWS_HPI_RET_HANDLED;
		}

		n = user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
						LWS_CALLBACK_HTTP_WRITEABLE,
						wsi->user_space, NULL, 0);
		if (n < 0) {
			lwsl_info("writeable_fail\n");
			goto fail;
		}

		return LWS_HPI_RET_HANDLED;
	}

#if defined(LWS_WITH_FILE_OPS)

	/* >0 == completion, <0 == error
	 *
	 * We'll get a LWS_CALLBACK_HTTP_FILE_COMPLETION callback when
	 * it's done.  That's the case even if we just completed the
	 * send, so wait for that.
	 */
	n = lws_serve_http_file_fragment(wsi);
	if (n < 0)
		goto fail;
#endif

	return LWS_HPI_RET_HANDLED;


fail:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
			   "server socket svc fail");

	return LWS_HPI_RET_WSI_ALREADY_DIED;
}
#endif

static int
rops_handle_POLLIN_h1(struct lws_context_per_thread *pt, struct lws *wsi,
		       struct lws_pollfd *pollfd)
{
	if (lwsi_state(wsi) == LRS_IDLING) {
		uint8_t buf[1];
		int rlen;

		/*
		 * h1 staggered spins here in IDLING if we don't close it.
		 * It shows POLLIN but the tls connection returns ERROR if
		 * you try to read it.
		 */

		// lwsl_notice("%s: %p: wsistate 0x%x %s, revents 0x%x\n",
		//	    __func__, wsi, wsi->wsistate, wsi->role_ops->name,
		//	    pollfd->revents);

		rlen = lws_ssl_capable_read(wsi, buf, sizeof(buf));
		if (rlen == LWS_SSL_CAPABLE_ERROR)
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

#ifdef LWS_WITH_CGI
	if (wsi->http.cgi && (pollfd->revents & LWS_POLLOUT)) {
		if (lws_handle_POLLOUT_event(wsi, pollfd))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;

		return LWS_HPI_RET_HANDLED;
	}
#endif

	/* Priority 2: pre- compression transform */

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	if (wsi->http.comp_ctx.buflist_comp ||
	    wsi->http.comp_ctx.may_have_more) {
		enum lws_write_protocol wp = LWS_WRITE_HTTP;

		lwsl_info("%s: completing comp partial (buflist_comp %p, may %d)\n",
				__func__, wsi->http.comp_ctx.buflist_comp,
				wsi->http.comp_ctx.may_have_more
				);

		if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_write_role_protocol) &&
		    lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_write_role_protocol).
					write_role_protocol(wsi, NULL, 0, &wp) < 0) {
			lwsl_info("%s signalling to close\n", __func__);
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}
		lws_callback_on_writable(wsi);

		if (!wsi->http.comp_ctx.buflist_comp &&
		    !wsi->http.comp_ctx.may_have_more &&
		    wsi->http.deferred_transaction_completed) {
			wsi->http.deferred_transaction_completed = 0;
			if (lws_http_transaction_completed(wsi))
				return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		return LWS_HPI_RET_HANDLED;
	}
#endif

        if (lws_is_flowcontrolled(wsi))
                /* We cannot deal with any kind of new RX because we are
                 * RX-flowcontrolled.
                 */
		return LWS_HPI_RET_HANDLED;

#if defined(LWS_WITH_SERVER)
	if (!lwsi_role_client(wsi)) {
		int n;

		lwsl_debug("%s: %s: wsistate 0x%x\n", __func__, lws_wsi_tag(wsi),
			   (unsigned int)wsi->wsistate);

		if (pollfd->revents & LWS_POLLHUP &&
		    !lws_buflist_total_len(&wsi->buflist))
			return LWS_HPI_RET_PLEASE_CLOSE_ME;

		n = lws_h1_server_socket_service(wsi, pollfd);
		if (n != LWS_HPI_RET_HANDLED)
			return n;
		if (lwsi_state(wsi) != LRS_SSL_INIT)
			if (lws_server_socket_service_ssl(wsi,
							  LWS_SOCK_INVALID,
					!!(pollfd->revents & LWS_POLLIN)))
				return LWS_HPI_RET_PLEASE_CLOSE_ME;

		return LWS_HPI_RET_HANDLED;
	}
#endif

#if defined(LWS_WITH_CLIENT)
	if ((pollfd->revents & LWS_POLLIN) &&
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

		//lwsl_notice("calling back %s\n", wsi->a.protocol->name);

		/* let user code know, he'll usually ask for writeable
		 * callback and drain / re-enable it there
		 */
		if (user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
					       LWS_CALLBACK_RECEIVE_CLIENT_HTTP,
						wsi->user_space, NULL, 0)) {
			lwsl_info("RECEIVE_CLIENT_HTTP closed it\n");
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		return LWS_HPI_RET_HANDLED;
	}
#endif

//	if (lwsi_state(wsi) == LRS_ESTABLISHED)
//		return LWS_HPI_RET_HANDLED;

#if defined(LWS_WITH_CLIENT)
	if ((pollfd->revents & LWS_POLLOUT) &&
	    lws_handle_POLLOUT_event(wsi, pollfd)) {
		lwsl_debug("POLLOUT event closed it\n");
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	if (lws_http_client_socket_service(wsi, pollfd))
		return LWS_HPI_RET_WSI_ALREADY_DIED;
#endif

	if (lwsi_state(wsi) == LRS_WAITING_CONNECT &&
	    (pollfd->revents & LWS_POLLHUP))
		return LWS_HPI_RET_PLEASE_CLOSE_ME;

	return LWS_HPI_RET_HANDLED;
}

static int
rops_handle_POLLOUT_h1(struct lws *wsi)
{


	if (lwsi_state(wsi) == LRS_ISSUE_HTTP_BODY ||
	    lwsi_state(wsi) == LRS_WAITING_SERVER_REPLY) {
#if defined(LWS_WITH_HTTP_PROXY)
		if (wsi->http.proxy_clientside) {
			unsigned char *buf, prebuf[LWS_PRE + 1024];
			size_t len = lws_buflist_next_segment_len(
					&wsi->parent->http.buflist_post_body, &buf);
			int n;

			if (len > sizeof(prebuf) - LWS_PRE)
				len = sizeof(prebuf) - LWS_PRE;

			if (len) {
				memcpy(prebuf + LWS_PRE, buf, len);

				lwsl_debug("%s: %s: proxying body %d %d %d %d %d\n",
						__func__, lws_wsi_tag(wsi), (int)len,
						(int)wsi->http.tx_content_length,
						(int)wsi->http.tx_content_remain,
						(int)wsi->http.rx_content_length,
						(int)wsi->http.rx_content_remain
						);

				n = lws_write(wsi, prebuf + LWS_PRE, len, LWS_WRITE_HTTP);
				if (n < 0) {
					lwsl_err("%s: PROXY_BODY: write %d failed\n",
						 __func__, (int)len);
					return LWS_HP_RET_BAIL_DIE;
				}

				lws_buflist_use_segment(&wsi->parent->http.buflist_post_body, len);

			}

			if (wsi->parent->http.buflist_post_body) {
				lws_callback_on_writable(wsi);
				return LWS_HP_RET_DROP_POLLOUT;
			}

			lwsl_wsi_err(wsi, "nothing to send");
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
			/* prepare ourselves to do the parsing */
			wsi->http.ah->parser_state = WSI_TOKEN_NAME_PART;
			wsi->http.ah->lextable_pos = 0;
#if defined(LWS_WITH_CUSTOM_HEADERS)
			wsi->http.ah->unk_pos = 0;
#endif
#endif
			lwsi_set_state(wsi, LRS_WAITING_SERVER_REPLY);
			lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_SERVER_RESPONSE,
					(int)wsi->a.context->timeout_secs);

			return LWS_HP_RET_DROP_POLLOUT;
		}
#endif
		return LWS_HP_RET_USER_SERVICE;
	}

	if (lwsi_role_client(wsi))
		return LWS_HP_RET_USER_SERVICE;

	return LWS_HP_RET_BAIL_OK;
}

static int
rops_write_role_protocol_h1(struct lws *wsi, unsigned char *buf, size_t len,
			    enum lws_write_protocol *wp)
{
	size_t olen = len;
	int n;

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	if (wsi->http.lcs && (((*wp) & 0x1f) == LWS_WRITE_HTTP_FINAL ||
			      ((*wp) & 0x1f) == LWS_WRITE_HTTP)) {
		unsigned char mtubuf[1500 + LWS_PRE +
				     LWS_HTTP_CHUNK_HDR_MAX_SIZE +
				     LWS_HTTP_CHUNK_TRL_MAX_SIZE],
			      *out = mtubuf + LWS_PRE +
				     LWS_HTTP_CHUNK_HDR_MAX_SIZE;
		size_t o = sizeof(mtubuf) - LWS_PRE -
			   LWS_HTTP_CHUNK_HDR_MAX_SIZE -
			   LWS_HTTP_CHUNK_TRL_MAX_SIZE;

		n = lws_http_compression_transform(wsi, buf, len, wp, &out, &o);
		if (n)
			return n;

		lwsl_info("%s: %s: transformed %d bytes to %d "
			   "(wp 0x%x, more %d)\n", __func__,
			   lws_wsi_tag(wsi), (int)len,
			   (int)o, (int)*wp, wsi->http.comp_ctx.may_have_more);

		if (!o)
			return (int)olen;

		if (wsi->http.comp_ctx.chunking) {
			char c[LWS_HTTP_CHUNK_HDR_MAX_SIZE + 2];
			/*
			 * this only needs dealing with on http/1.1 to allow
			 * pipelining
			 */
			n = lws_snprintf(c, sizeof(c), "%X\x0d\x0a", (int)o);
			lwsl_info("%s: chunk (%d) %s", __func__, (int)o, c);
			out -= n;
			o += (unsigned int)n;
			memcpy(out, c, (unsigned int)n);
			out[o++] = '\x0d';
			out[o++] = '\x0a';

			if (((*wp) & 0x1f) == LWS_WRITE_HTTP_FINAL) {
				lwsl_info("%s: final chunk\n", __func__);
				out[o++] = '0';
				out[o++] = '\x0d';
				out[o++] = '\x0a';
				out[o++] = '\x0d';
				out[o++] = '\x0a';
			}
		}

		buf = out;
		len = o;
	}
#endif

	n = lws_issue_raw(wsi, (unsigned char *)buf, len);
	if (n < 0)
		return n;

	/* hide there may have been compression */

	return (int)olen;
}

static int
rops_alpn_negotiated_h1(struct lws *wsi, const char *alpn)
{
	lwsl_debug("%s: client %d\n", __func__, lwsi_role_client(wsi));
#if defined(LWS_WITH_CLIENT)
	if (lwsi_role_client(wsi)) {
		/*
		 * If alpn asserts it is http/1.1, server support for KA is
		 * mandatory.
		 *
		 * Knowing this lets us proceed with sending pipelined headers
		 * before we received the first response headers.
		 */
		wsi->keepalive_active = 1;
	}
#endif

	return 0;
}

static int
rops_destroy_role_h1(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct allocated_headers *ah;

	/* we may not have an ah, but may be on the waiting list... */
	lwsl_info("%s: ah det due to close\n", __func__);
	__lws_header_table_detach(wsi, 0);

	 ah = pt->http.ah_list;

	while (ah) {
		if (ah->in_use && ah->wsi == wsi) {
			lwsl_err("%s: ah leak: wsi %s\n", __func__,
					lws_wsi_tag(wsi));
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

#ifdef LWS_ROLE_WS
	lws_free_set_NULL(wsi->ws);
#endif
	return 0;
}

#if defined(LWS_WITH_SERVER)

static int
rops_adoption_bind_h1(struct lws *wsi, int type, const char *vh_prot_name)
{
	if (!(type & LWS_ADOPT_HTTP))
		return 0; /* no match */

	if (type & _LWS_ADOPT_FINISH && !lwsi_role_http(wsi))
		return 0;

	if (type & _LWS_ADOPT_FINISH) {
		if (!lws_header_table_attach(wsi, 0))
			lwsl_debug("Attached ah immediately\n");
		else
			lwsl_info("%s: waiting for ah\n", __func__);

		return 1;
	}

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SECURE_STREAMS)
	if (wsi->a.vhost->ss_handle &&
	    wsi->a.vhost->ss_handle->policy->protocol == LWSSSP_RAW) {
		lws_role_transition(wsi, LWSIFR_SERVER, (type & LWS_ADOPT_ALLOW_SSL) ?
				LRS_SSL_INIT : LRS_ESTABLISHED, &role_ops_raw_skt);
		return 1;
	}
#endif

	/* If Non-TLS and HTTP2 prior knowledge is enabled, skip to clear text HTTP2 */

#if defined(LWS_WITH_HTTP2)
	if ((!(type & LWS_ADOPT_ALLOW_SSL)) && (wsi->a.vhost->options & LWS_SERVER_OPTION_H2_PRIOR_KNOWLEDGE)) {
		lwsl_info("http/2 prior knowledge\n");
		lws_metrics_tag_wsi_add(wsi, "upg", "h2_prior");
		lws_role_call_alpn_negotiated(wsi, "h2");
	}
	else
#endif
		lws_role_transition(wsi, LWSIFR_SERVER, (type & LWS_ADOPT_ALLOW_SSL) ?
				LRS_SSL_INIT : LRS_HEADERS, &role_ops_h1);

	/*
	 * Otherwise, we have to bind to h1 as a default even when we're actually going to
	 * replace it as an h2 bind later.  So don't take this seriously if the
	 * default is disabled (ws upgrade caees properly about it)
	 */

	if (!vh_prot_name && wsi->a.vhost->default_protocol_index <
			     wsi->a.vhost->count_protocols)
		wsi->a.protocol = &wsi->a.vhost->protocols[
				wsi->a.vhost->default_protocol_index];
	else
		wsi->a.protocol = &wsi->a.vhost->protocols[0];

	/* the transport is accepted... give him time to negotiate */
	lws_set_timeout(wsi, PENDING_TIMEOUT_ESTABLISH_WITH_SERVER,
			(int)wsi->a.context->timeout_secs);

	return 1; /* bound */
}

#endif

#if defined(LWS_WITH_CLIENT)

static const char * const http_methods[] = {
	"GET", "POST", "OPTIONS", "HEAD", "PUT", "PATCH", "DELETE", "CONNECT"
};

static int
rops_client_bind_h1(struct lws *wsi, const struct lws_client_connect_info *i)
{
	int n;

	if (!i) {
		/* we are finalizing an already-selected role */

		/*
		 * If we stay in http, assuming there wasn't already-set
		 * external user_space, since we know our initial protocol
		 * we can assign the user space now, otherwise do it after the
		 * ws subprotocol negotiated
		 */
		if (!wsi->user_space && wsi->stash->cis[CIS_METHOD])
			if (lws_ensure_user_space(wsi))
				return 1;

		 /*
		  * For ws, default to http/1.1 only.  If i->alpn had been set
		  * though, defer to whatever he has set in there (eg, "h2").
		  *
		  * The problem is he has to commit to h2 before he can find
		  * out if the server has the SETTINGS for ws-over-h2 enabled;
		  * if not then ws is not possible on that connection.  So we
		  * only try h2 if he assertively said to use h2 alpn, otherwise
		  * ws implies alpn restriction to h1.
		  */
		if (!wsi->stash->cis[CIS_METHOD] && !wsi->stash->cis[CIS_ALPN])
			wsi->stash->cis[CIS_ALPN] = "http/1.1";

		/* if we went on the ah waiting list, it's ok, we can wait.
		 *
		 * When we do get the ah, now or later, he will end up at
		 * lws_http_client_connect_via_info2().
		 */
		if (lws_header_table_attach(wsi, 0)
#if defined(LWS_WITH_CLIENT)
				< 0)
			/*
			 * if we failed here, the connection is already closed
			 * and freed.
			 */
			return -1;
#else
			)
				return 0;
#endif

		return 0;
	}

	/*
	 * Clients that want to be h1, h2, or ws all start out as h1
	 * (we don't yet know if the server supports h2 or ws), unless their
	 * alpn is only "h2"
	 */

//	if (i->alpn && !strcmp(i->alpn, "h2"))
//		return 0; /* we are h1, he only wants h2 */

	if (!i->method) { /* websockets */
#if defined(LWS_ROLE_WS)
		if (lws_create_client_ws_object(i, wsi))
			goto fail_wsi;

		goto bind_h1;
#else
		lwsl_err("%s: ws role not configured\n", __func__);

		goto fail_wsi;
#endif
	}

	/* if a recognized http method, bind to it */

	for (n = 0; n < (int)LWS_ARRAY_SIZE(http_methods); n++)
		if (!strcmp(i->method, http_methods[n]))
			goto bind_h1;

	/* other roles may bind to it */

	return 0; /* no match */

bind_h1:
	/* assert the mode and union status (hdr) clearly */
	lws_role_transition(wsi, LWSIFR_CLIENT, LRS_UNCONNECTED, &role_ops_h1);

	return 1; /* matched */

fail_wsi:
	return -1;
}
#endif

static int
rops_close_kill_connection_h1(struct lws *wsi, enum lws_close_status reason)
{
#if defined(LWS_WITH_HTTP_PROXY)
	if (!wsi->http.proxy_clientside)
		return 0;

	wsi->http.proxy_clientside = 0;

	if (user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
					LWS_CALLBACK_COMPLETED_CLIENT_HTTP,
					wsi->user_space, NULL, 0))
		return 0;
#endif
	return 0;
}

int
rops_pt_init_destroy_h1(struct lws_context *context,
		    const struct lws_context_creation_info *info,
		    struct lws_context_per_thread *pt, int destroy)
{
	/*
	 * We only want to do this once... we will do it if no h2 support
	 * otherwise let h2 ops do it.
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

static const lws_rops_t rops_table_h1[] = {
	/*  1 */ { .pt_init_destroy	  = rops_pt_init_destroy_h1 },
	/*  2 */ { .handle_POLLIN	  = rops_handle_POLLIN_h1 },
	/*  3 */ { .handle_POLLOUT	  = rops_handle_POLLOUT_h1 },
	/*  4 */ { .write_role_protocol	  = rops_write_role_protocol_h1 },
	/*  5 */ { .alpn_negotiated	  = rops_alpn_negotiated_h1 },
	/*  6 */ { .close_kill_connection = rops_close_kill_connection_h1 },
	/*  7 */ { .destroy_role	  = rops_destroy_role_h1 },
#if defined(LWS_WITH_SERVER)
	/*  8 */ { .adoption_bind	  = rops_adoption_bind_h1 },
#endif
#if defined(LWS_WITH_CLIENT)
	/*  8 if client and no server */
	/*  9 */ { .client_bind		  = rops_client_bind_h1 },
#endif
};

const struct lws_role_ops role_ops_h1 = {
	/* role name */			"h1",
	/* alpn id */			"http/1.1",
	/* rops_table */		rops_table_h1,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x01,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x02,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x30,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x00,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x40,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x50,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x06,
	  /* LWS_ROPS_destroy_role */
#if defined(LWS_WITH_SERVER)
	  /* LWS_ROPS_adoption_bind */			0x78,
#else
	  /* LWS_ROPS_adoption_bind */			0x70,
#endif
	  /* LWS_ROPS_client_bind */
#if defined(LWS_WITH_CLIENT)
#if defined(LWS_WITH_SERVER)
	  /* LWS_ROPS_issue_keepalive */		0x90,
#else
	  /* LWS_ROPS_issue_keepalive */		0x80,
#endif
#else
	  /* LWS_ROPS_issue_keepalive */		0x00,
#endif
					},
	/* adoption_cb clnt, srv */	{ LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED,
					  LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED },
	/* rx_cb clnt, srv */		{ LWS_CALLBACK_RECEIVE_CLIENT_HTTP,
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
