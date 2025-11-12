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

/*
 * parsers.c: lws_ws_rx_sm() needs to be roughly kept in
 *   sync with changes here, esp related to ext draining
 */

int lws_ws_client_rx_sm(struct lws *wsi, unsigned char c)
{
	int callback_action = LWS_CALLBACK_CLIENT_RECEIVE;
	struct lws_ext_pm_deflate_rx_ebufs pmdrx;
	unsigned short close_code;
	unsigned char *pp;
	int handled, m, n;
#if !defined(LWS_WITHOUT_EXTENSIONS)
	int rx_draining_ext = 0;
#endif

	pmdrx.eb_in.token = NULL;
	pmdrx.eb_in.len = 0;
	pmdrx.eb_out.token = NULL;
	pmdrx.eb_out.len = 0;

#if !defined(LWS_WITHOUT_EXTENSIONS)
	if (wsi->ws->rx_draining_ext) {
		assert(!c);

		lws_remove_wsi_from_draining_ext_list(wsi);
		rx_draining_ext = 1;
		lwsl_wsi_debug(wsi, "doing draining flow");

		goto drain_extension;
	}
#endif

	switch (wsi->lws_rx_parse_state) {
	case LWS_RXPS_NEW:
		/* control frames (PING) may interrupt checkable sequences */
		wsi->ws->defeat_check_utf8 = 0;

		switch (wsi->ws->ietf_spec_revision) {
		case 13:
			wsi->ws->opcode = c & 0xf;
			/* revisit if an extension wants them... */
			switch (wsi->ws->opcode) {
			case LWSWSOPC_TEXT_FRAME:
				wsi->ws->rsv_first_msg = (c & 0x70);
#if !defined(LWS_WITHOUT_EXTENSIONS)
				/*
				 * set the expectation that we will have to
				 * fake up the zlib trailer to the inflator for
				 * this frame
				 */
				wsi->ws->pmd_trailer_application = !!(c & 0x40);
#endif
				wsi->ws->continuation_possible = 1;
				wsi->ws->check_utf8 = lws_check_opt(
					wsi->a.context->options,
					LWS_SERVER_OPTION_VALIDATE_UTF8);
				wsi->ws->utf8 = 0;
				wsi->ws->first_fragment = 1;
				break;
			case LWSWSOPC_BINARY_FRAME:
				wsi->ws->rsv_first_msg = (c & 0x70);
#if !defined(LWS_WITHOUT_EXTENSIONS)
				/*
				 * set the expectation that we will have to
				 * fake up the zlib trailer to the inflator for
				 * this frame
				 */
				wsi->ws->pmd_trailer_application = !!(c & 0x40);
#endif
				wsi->ws->check_utf8 = 0;
				wsi->ws->continuation_possible = 1;
				wsi->ws->first_fragment = 1;
				break;
			case LWSWSOPC_CONTINUATION:
				if (!wsi->ws->continuation_possible) {
					lwsl_wsi_info(wsi, "disordered continuation");
					return -1;
				}
				wsi->ws->first_fragment = 0;
				break;
			case LWSWSOPC_CLOSE:
				wsi->ws->check_utf8 = 0;
				wsi->ws->utf8 = 0;
				break;
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 0xb:
			case 0xc:
			case 0xd:
			case 0xe:
			case 0xf:
				lwsl_wsi_info(wsi, "illegal opcode");
				return -1;
			default:
				wsi->ws->defeat_check_utf8 = 1;
				break;
			}
			wsi->ws->rsv = (c & 0x70);
			/* revisit if an extension wants them... */
			if (
#if !defined(LWS_WITHOUT_EXTENSIONS)
				!wsi->ws->count_act_ext &&
#endif
				wsi->ws->rsv) {
				lwsl_wsi_info(wsi, "illegal rsv bits set");
				return -1;
			}
			wsi->ws->final = !!((c >> 7) & 1);
			lwsl_wsi_ext(wsi, "    This RX frame Final %d",
				 wsi->ws->final);

			if (wsi->ws->owed_a_fin &&
			    (wsi->ws->opcode == LWSWSOPC_TEXT_FRAME ||
			     wsi->ws->opcode == LWSWSOPC_BINARY_FRAME)) {
				lwsl_wsi_info(wsi, "hey you owed us a FIN");
				return -1;
			}
			if ((!(wsi->ws->opcode & 8)) && wsi->ws->final) {
				wsi->ws->continuation_possible = 0;
				wsi->ws->owed_a_fin = 0;
			}

			if ((wsi->ws->opcode & 8) && !wsi->ws->final) {
				lwsl_wsi_info(wsi, "control msg can't be fragmented");
				return -1;
			}
			if (!wsi->ws->final)
				wsi->ws->owed_a_fin = 1;

			switch (wsi->ws->opcode) {
			case LWSWSOPC_TEXT_FRAME:
			case LWSWSOPC_BINARY_FRAME:
				wsi->ws->frame_is_binary = wsi->ws->opcode ==
						 LWSWSOPC_BINARY_FRAME;
				break;
			}
			wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN;
			break;

		default:
			lwsl_wsi_err(wsi, "unknown spec version %02d",
				 wsi->ws->ietf_spec_revision);
			break;
		}
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN:

		wsi->ws->this_frame_masked = !!(c & 0x80);
		if (wsi->ws->this_frame_masked)
			goto server_cannot_mask;

		switch (c & 0x7f) {
		case 126:
			/* control frames are not allowed to have big lengths */
			if (wsi->ws->opcode & 8)
				goto illegal_ctl_length;
			wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN16_2;
			break;
		case 127:
			/* control frames are not allowed to have big lengths */
			if (wsi->ws->opcode & 8)
				goto illegal_ctl_length;
			wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_8;
			break;
		default:
			wsi->ws->rx_packet_length = c & 0x7f;
			if (wsi->ws->this_frame_masked)
				wsi->lws_rx_parse_state =
						LWS_RXPS_07_COLLECT_FRAME_KEY_1;
			else {
				if (wsi->ws->rx_packet_length) {
					wsi->lws_rx_parse_state =
					LWS_RXPS_WS_FRAME_PAYLOAD;
				} else {
					wsi->lws_rx_parse_state = LWS_RXPS_NEW;
					goto spill;
				}
			}
			break;
		}
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN16_2:
		wsi->ws->rx_packet_length = (size_t)((unsigned int)c << 8);
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN16_1;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN16_1:
		wsi->ws->rx_packet_length |= c;
		if (wsi->ws->this_frame_masked)
			wsi->lws_rx_parse_state = LWS_RXPS_07_COLLECT_FRAME_KEY_1;
		else {
			if (wsi->ws->rx_packet_length)
				wsi->lws_rx_parse_state =
					LWS_RXPS_WS_FRAME_PAYLOAD;
			else {
				wsi->lws_rx_parse_state = LWS_RXPS_NEW;
				goto spill;
			}
		}
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_8:
		if (c & 0x80) {
			lwsl_wsi_warn(wsi, "b63 of length must be zero");
			/* kill the connection */
			return -1;
		}
#if defined __LP64__
		wsi->ws->rx_packet_length = ((size_t)c) << 56;
#else
		wsi->ws->rx_packet_length = 0;
#endif
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_7;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_7:
#if defined __LP64__
		wsi->ws->rx_packet_length |= ((size_t)c) << 48;
#endif
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_6;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_6:
#if defined __LP64__
		wsi->ws->rx_packet_length |= ((size_t)c) << 40;
#endif
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_5;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_5:
#if defined __LP64__
		wsi->ws->rx_packet_length |= ((size_t)c) << 32;
#endif
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_4;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_4:
		wsi->ws->rx_packet_length |= ((size_t)c) << 24;
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_3;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_3:
		wsi->ws->rx_packet_length |= ((size_t)c) << 16;
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_2;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_2:
		wsi->ws->rx_packet_length |= ((size_t)c) << 8;
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN64_1;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_1:
		wsi->ws->rx_packet_length |= (size_t)c;
		if (wsi->ws->this_frame_masked)
			wsi->lws_rx_parse_state =
					LWS_RXPS_07_COLLECT_FRAME_KEY_1;
		else {
			if (wsi->ws->rx_packet_length)
				wsi->lws_rx_parse_state =
					LWS_RXPS_WS_FRAME_PAYLOAD;
			else {
				wsi->lws_rx_parse_state = LWS_RXPS_NEW;
				goto spill;
			}
		}
		break;

	case LWS_RXPS_07_COLLECT_FRAME_KEY_1:
		wsi->ws->mask[0] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;
		wsi->lws_rx_parse_state = LWS_RXPS_07_COLLECT_FRAME_KEY_2;
		break;

	case LWS_RXPS_07_COLLECT_FRAME_KEY_2:
		wsi->ws->mask[1] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;
		wsi->lws_rx_parse_state = LWS_RXPS_07_COLLECT_FRAME_KEY_3;
		break;

	case LWS_RXPS_07_COLLECT_FRAME_KEY_3:
		wsi->ws->mask[2] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;
		wsi->lws_rx_parse_state = LWS_RXPS_07_COLLECT_FRAME_KEY_4;
		break;

	case LWS_RXPS_07_COLLECT_FRAME_KEY_4:
		wsi->ws->mask[3] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;

		if (wsi->ws->rx_packet_length)
			wsi->lws_rx_parse_state =
					LWS_RXPS_WS_FRAME_PAYLOAD;
		else {
			wsi->lws_rx_parse_state = LWS_RXPS_NEW;
			goto spill;
		}
		break;

	case LWS_RXPS_WS_FRAME_PAYLOAD:

		assert(wsi->ws->rx_ubuf);
#if !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws->rx_draining_ext)
			goto drain_extension;
#endif
		if (wsi->ws->this_frame_masked && !wsi->ws->all_zero_nonce)
			c ^= wsi->ws->mask[(wsi->ws->mask_idx++) & 3];

		/*
		 * unmask and collect the payload body in
		 * rx_ubuf_head + LWS_PRE
		 */

		wsi->ws->rx_ubuf[LWS_PRE + (wsi->ws->rx_ubuf_head++)] = c;

		if (--wsi->ws->rx_packet_length == 0) {
			/* spill because we have the whole frame */
			wsi->lws_rx_parse_state = LWS_RXPS_NEW;
			lwsl_wsi_debug(wsi, "spilling as we have the whole frame");
			goto spill;
		}

		/*
		 * if there's no protocol max frame size given, we are
		 * supposed to default to context->pt_serv_buf_size
		 */
		if (!wsi->a.protocol->rx_buffer_size &&
		    wsi->ws->rx_ubuf_head != wsi->a.context->pt_serv_buf_size)
			break;

		if (wsi->a.protocol->rx_buffer_size &&
		    wsi->ws->rx_ubuf_head != wsi->a.protocol->rx_buffer_size)
			break;

		/* spill because we filled our rx buffer */

		lwsl_wsi_debug(wsi, "spilling as we filled our rx buffer");
spill:

		handled = 0;

		/*
		 * is this frame a control packet we should take care of at this
		 * layer?  If so service it and hide it from the user callback
		 */

		switch (wsi->ws->opcode) {
		case LWSWSOPC_CLOSE:
			pp = &wsi->ws->rx_ubuf[LWS_PRE];
			if (lws_check_opt(wsi->a.context->options,
					  LWS_SERVER_OPTION_VALIDATE_UTF8) &&
			    wsi->ws->rx_ubuf_head > 2 &&
			    lws_check_utf8(&wsi->ws->utf8, pp + 2,
					   wsi->ws->rx_ubuf_head - 2))
				goto utf8_fail;

			/* is this an acknowledgment of our close? */
			if (lwsi_state(wsi) == LRS_AWAITING_CLOSE_ACK) {
				/*
				 * fine he has told us he is closing too, let's
				 * finish our close
				 */
				lwsl_wsi_parser(wsi, "seen server's close ack");
				return -1;
			}

			lwsl_wsi_parser(wsi, "client sees server close len = %d",
						 (int)wsi->ws->rx_ubuf_head);
			if (wsi->ws->rx_ubuf_head >= 2) {
				close_code = (unsigned short)((pp[0] << 8) | pp[1]);
				if (close_code < 1000 ||
				    close_code == 1004 ||
				    close_code == 1005 ||
				    close_code == 1006 ||
				    (close_code >= 1016 && close_code < 3000)
				) {
					pp[0] = (LWS_CLOSE_STATUS_PROTOCOL_ERR >> 8) & 0xff;
					pp[1] = LWS_CLOSE_STATUS_PROTOCOL_ERR & 0xff;
				}
			}
			if (user_callback_handle_rxflow(
					wsi->a.protocol->callback, wsi,
					LWS_CALLBACK_WS_PEER_INITIATED_CLOSE,
					wsi->user_space, pp,
					wsi->ws->rx_ubuf_head))
				return -1;

			memcpy(wsi->ws->ping_payload_buf + LWS_PRE, pp,
			       wsi->ws->rx_ubuf_head);
			wsi->ws->close_in_ping_buffer_len =
					(uint8_t)wsi->ws->rx_ubuf_head;

			lwsl_wsi_info(wsi, "scheduling return close as ack");
			__lws_change_pollfd(wsi, LWS_POLLIN, 0);
			lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_SEND, 3);
			wsi->waiting_to_send_close_frame = 1;
			wsi->close_needs_ack = 0;
			lwsi_set_state(wsi, LRS_WAITING_TO_SEND_CLOSE);
			lws_callback_on_writable(wsi);
			handled = 1;
			break;

		case LWSWSOPC_PING:
			lwsl_wsi_info(wsi, "received %d byte ping, sending pong",
				  (int)wsi->ws->rx_ubuf_head);

			/* he set a close reason on this guy, ignore PING */
			if (wsi->ws->close_in_ping_buffer_len)
				goto ping_drop;

			if (wsi->ws->pong_pending_flag) {
				/*
				 * there is already a pending pong payload
				 * we should just log and drop
				 */
				lwsl_wsi_parser(wsi, "DROP PING since one pending");
				goto ping_drop;
			}

			/* control packets can only be < 128 bytes long */
			if (wsi->ws->rx_ubuf_head > 128 - 3) {
				lwsl_wsi_parser(wsi, "DROP PING payload too large");
				goto ping_drop;
			}

			/* stash the pong payload */
			memcpy(wsi->ws->pong_payload_buf + LWS_PRE,
			       &wsi->ws->rx_ubuf[LWS_PRE],
			       wsi->ws->rx_ubuf_head);

			wsi->ws->pong_payload_len = (uint8_t)wsi->ws->rx_ubuf_head;
			wsi->ws->pong_pending_flag = 1;

			/* get it sent as soon as possible */
			lws_callback_on_writable(wsi);
ping_drop:
			wsi->ws->rx_ubuf_head = 0;
			handled = 1;
			break;

		case LWSWSOPC_PONG:
			lwsl_wsi_info(wsi, "Received pong");
			lwsl_hexdump_wsi_debug(wsi, &wsi->ws->rx_ubuf[LWS_PRE],
				     wsi->ws->rx_ubuf_head);

			lws_validity_confirmed(wsi);
			/* issue it */
			callback_action = LWS_CALLBACK_CLIENT_RECEIVE_PONG;
			break;

		case LWSWSOPC_CONTINUATION:
		case LWSWSOPC_TEXT_FRAME:
		case LWSWSOPC_BINARY_FRAME:
			break;

		default:
			/* not handled or failed */
			lwsl_wsi_ext(wsi, "Unhandled ext opc 0x%x", wsi->ws->opcode);
			wsi->ws->rx_ubuf_head = 0;

			return -1;
		}

		/*
		 * No it's real payload, pass it up to the user callback.
		 *
		 * We have been statefully collecting it in the
		 * LWS_RXPS_WS_FRAME_PAYLOAD clause above.
		 *
		 * It's nicely buffered with the pre-padding taken care of
		 * so it can be sent straight out again using lws_write.
		 *
		 * However, now we have a chunk of it, we want to deal with it
		 * all here.  Since this may be input to permessage-deflate and
		 * there are block limits on that for input and output, we may
		 * need to iterate.
		 */
		if (handled)
			goto already_done;

		pmdrx.eb_in.token = &wsi->ws->rx_ubuf[LWS_PRE];
		pmdrx.eb_in.len = (int)wsi->ws->rx_ubuf_head;

		/* for the non-pm-deflate case */

		pmdrx.eb_out = pmdrx.eb_in;

		lwsl_wsi_debug(wsi, "starting disbursal of %d deframed rx",
				(int)wsi->ws->rx_ubuf_head);

#if !defined(LWS_WITHOUT_EXTENSIONS)
drain_extension:
#endif
		do {

		//	lwsl_wsi_notice("pmdrx.eb_in.len: %d",
		//		    (int)pmdrx.eb_in.len);

			n = PMDR_DID_NOTHING;

#if !defined(LWS_WITHOUT_EXTENSIONS)
			lwsl_wsi_ext(wsi, "+++ passing %d %p to ext",
				 pmdrx.eb_in.len, pmdrx.eb_in.token);

			n = lws_ext_cb_active(wsi, LWS_EXT_CB_PAYLOAD_RX,
					      &pmdrx, 0);
			lwsl_wsi_ext(wsi, "Ext RX returned %d", n);
			if (n < 0) {
				wsi->socket_is_permanently_unusable = 1;
				return -1;
			}
			if (n == PMDR_DID_NOTHING)
				/* ie, not PMDR_NOTHING_WE_SHOULD_DO */
				break;
#endif
			lwsl_wsi_ext(wsi, "post inflate ebuf in len %d / out len %d",
				    pmdrx.eb_in.len, pmdrx.eb_out.len);

#if !defined(LWS_WITHOUT_EXTENSIONS)
			if (rx_draining_ext && !pmdrx.eb_out.len) {
				lwsl_wsi_debug(wsi, "   --- ending drain on 0 read result");
				goto already_done;
			}

			if (n == PMDR_HAS_PENDING) {	/* 1 means stuff to drain */
				/* extension had more... main loop will come back */
				lwsl_wsi_ext(wsi, "adding to draining ext list");
				lws_add_wsi_to_draining_ext_list(wsi);
			} else {
				lwsl_wsi_ext(wsi, "removing from draining ext list");
				lws_remove_wsi_from_draining_ext_list(wsi);
			}
			rx_draining_ext = wsi->ws->rx_draining_ext;
#endif

			if (wsi->ws->check_utf8 && !wsi->ws->defeat_check_utf8) {

				if (lws_check_utf8(&wsi->ws->utf8,
						   pmdrx.eb_out.token,
						   (unsigned int)pmdrx.eb_out.len)) {
					lws_close_reason(wsi,
						LWS_CLOSE_STATUS_INVALID_PAYLOAD,
						(uint8_t *)"bad utf8", 8);
					goto utf8_fail;
				}

				/* we are ending partway through utf-8 character? */
				if (!wsi->ws->rx_packet_length &&
				    wsi->ws->final && wsi->ws->utf8
#if !defined(LWS_WITHOUT_EXTENSIONS)
				    /* if ext not negotiated, going to be UNKNOWN */
				    && (n == PMDR_EMPTY_FINAL || n == PMDR_UNKNOWN)
#endif
				    ) {
					lwsl_wsi_info(wsi, "FINAL utf8 error");
					lws_close_reason(wsi,
						LWS_CLOSE_STATUS_INVALID_PAYLOAD,
						(uint8_t *)"partial utf8", 12);
utf8_fail:
					lwsl_wsi_info(wsi, "utf8 error");
					lwsl_hexdump_wsi_info(wsi, pmdrx.eb_out.token,
							  (unsigned int)pmdrx.eb_out.len);

					return -1;
				}
			}

			if (pmdrx.eb_out.len < 0 &&
			    callback_action != LWS_CALLBACK_CLIENT_RECEIVE_PONG)
				goto already_done;

			if (!pmdrx.eb_out.token)
				goto already_done;

			pmdrx.eb_out.token[pmdrx.eb_out.len] = '\0';

			if (!wsi->a.protocol->callback)
				goto already_done;

			if (callback_action == LWS_CALLBACK_CLIENT_RECEIVE_PONG)
				lwsl_wsi_info(wsi, "Client doing pong callback");

#if !defined(LWS_WITHOUT_EXTENSIONS)
			if (n == PMDR_HAS_PENDING)
				/* extension had more... main loop will come back
				 * we want callback to be done with this set, if so,
				 * because lws_is_final() hides it was final until the
				 * last chunk
				 */
				lws_add_wsi_to_draining_ext_list(wsi);
			else
				lws_remove_wsi_from_draining_ext_list(wsi);
#endif

			if (lwsi_state(wsi) == LRS_RETURNED_CLOSE ||
			    lwsi_state(wsi) == LRS_WAITING_TO_SEND_CLOSE ||
			    lwsi_state(wsi) == LRS_AWAITING_CLOSE_ACK)
				goto already_done;

			/* if pmd not enabled, in == out */

			if (n == PMDR_DID_NOTHING
#if !defined(LWS_WITHOUT_EXTENSIONS)
			    || n == PMDR_UNKNOWN
#endif
			)
				pmdrx.eb_in.len -= pmdrx.eb_out.len;

			m = wsi->a.protocol->callback(wsi,
					(enum lws_callback_reasons)callback_action,
					wsi->user_space, pmdrx.eb_out.token,
					(unsigned int)pmdrx.eb_out.len);

			wsi->ws->first_fragment = 0;

			lwsl_wsi_debug(wsi, "bulk ws rx: inp used %d, output %d",
				    (int)wsi->ws->rx_ubuf_head,
				    (int)pmdrx.eb_out.len);

			/* if user code wants to close, let caller know */
			if (m)
				return 1;

		} while (pmdrx.eb_in.len
#if !defined(LWS_WITHOUT_EXTENSIONS)
	|| rx_draining_ext
#endif
		);

already_done:
		wsi->ws->rx_ubuf_head = 0;
		break;
	default:
		lwsl_wsi_err(wsi, "client rx illegal state");
		return 1;
	}

	return 0;

illegal_ctl_length:
	lwsl_wsi_warn(wsi, "Control frame asking for extended length is illegal");

	/* kill the connection */
	return -1;

server_cannot_mask:
	lws_close_reason(wsi,
			LWS_CLOSE_STATUS_PROTOCOL_ERR,
			(uint8_t *)"srv mask", 8);

	lwsl_wsi_warn(wsi, "Server must not mask");

	/* kill the connection */
	return -1;
}


