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

#define LWS_CPYAPP(ptr, str) { strcpy(ptr, str); ptr += strlen(str); }

/*
 * client-parser.c: lws_ws_client_rx_sm() needs to be roughly kept in
 *   sync with changes here, esp related to ext draining
 */

int
lws_ws_rx_sm(struct lws *wsi, char already_processed, unsigned char c)
{
	int callback_action = LWS_CALLBACK_RECEIVE;
	struct lws_ext_pm_deflate_rx_ebufs pmdrx;
	unsigned short close_code;
	unsigned char *pp;
	int ret = 0;
	int n = 0;
#if !defined(LWS_WITHOUT_EXTENSIONS)
	int rx_draining_ext = 0;
	int lin;
#endif

	pmdrx.eb_in.token = NULL;
	pmdrx.eb_in.len = 0;
	pmdrx.eb_out.token = NULL;
	pmdrx.eb_out.len = 0;

	switch (wsi->lws_rx_parse_state) {
	case LWS_RXPS_NEW:
#if !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws->rx_draining_ext) {
			pmdrx.eb_in.token = NULL;
			pmdrx.eb_in.len = 0;
			pmdrx.eb_out.token = NULL;
			pmdrx.eb_out.len = 0;
			lws_remove_wsi_from_draining_ext_list(wsi);
			rx_draining_ext = 1;
			lwsl_debug("%s: doing draining flow\n", __func__);

			goto drain_extension;
		}
#endif
		switch (wsi->ws->ietf_spec_revision) {
		case 13:
			/*
			 * no prepended frame key any more
			 */
			wsi->ws->all_zero_nonce = 1;
			goto handle_first;

		default:
			lwsl_warn("lws_ws_rx_sm: unknown spec version %d\n",
				  wsi->ws->ietf_spec_revision);
			break;
		}
		break;
	case LWS_RXPS_04_mask_1:
		wsi->ws->mask[1] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;
		wsi->lws_rx_parse_state = LWS_RXPS_04_mask_2;
		break;
	case LWS_RXPS_04_mask_2:
		wsi->ws->mask[2] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;
		wsi->lws_rx_parse_state = LWS_RXPS_04_mask_3;
		break;
	case LWS_RXPS_04_mask_3:
		wsi->ws->mask[3] = c;
		if (c)
			wsi->ws->all_zero_nonce = 0;

		/*
		 * start from the zero'th byte in the XOR key buffer since
		 * this is the start of a frame with a new key
		 */

		wsi->ws->mask_idx = 0;

		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_1;
		break;

	/*
	 *  04 logical framing from the spec (all this is masked when incoming
	 *  and has to be unmasked)
	 *
	 * We ignore the possibility of extension data because we don't
	 * negotiate any extensions at the moment.
	 *
	 *    0                   1                   2                   3
	 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 *   +-+-+-+-+-------+-+-------------+-------------------------------+
	 *   |F|R|R|R| opcode|R| Payload len |    Extended payload length    |
	 *   |I|S|S|S|  (4)  |S|     (7)     |             (16/63)           |
	 *   |N|V|V|V|       |V|             |   (if payload len==126/127)   |
	 *   | |1|2|3|       |4|             |                               |
	 *   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
	 *   |     Extended payload length continued, if payload len == 127  |
	 *   + - - - - - - - - - - - - - - - +-------------------------------+
	 *   |                               |         Extension data        |
	 *   +-------------------------------+ - - - - - - - - - - - - - - - +
	 *   :                                                               :
	 *   +---------------------------------------------------------------+
	 *   :                       Application data                        :
	 *   +---------------------------------------------------------------+
	 *
	 *  We pass payload through to userland as soon as we get it, ignoring
	 *  FIN.  It's up to userland to buffer it up if it wants to see a
	 *  whole unfragmented block of the original size (which may be up to
	 *  2^63 long!)
	 */

	case LWS_RXPS_04_FRAME_HDR_1:
handle_first:

		wsi->ws->opcode = c & 0xf;
		wsi->ws->rsv = c & 0x70;
		wsi->ws->final = !!((c >> 7) & 1);
		wsi->ws->defeat_check_utf8 = 0;

		if (((wsi->ws->opcode) & 8) && !wsi->ws->final) {
			lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR,
					(uint8_t *)"frag ctl", 8);
			return -1;
		}

		switch (wsi->ws->opcode) {
		case LWSWSOPC_TEXT_FRAME:
			wsi->ws->check_utf8 = lws_check_opt(
				wsi->a.context->options,
				LWS_SERVER_OPTION_VALIDATE_UTF8);
			/* fallthru */
		case LWSWSOPC_BINARY_FRAME:
			if (wsi->ws->opcode == LWSWSOPC_BINARY_FRAME)
				wsi->ws->check_utf8 = 0;
			if (wsi->ws->continuation_possible) {
				lws_close_reason(wsi,
					LWS_CLOSE_STATUS_PROTOCOL_ERR,
					(uint8_t *)"bad cont", 8);
				return -1;
			}
			wsi->ws->rsv_first_msg = (c & 0x70);
#if !defined(LWS_WITHOUT_EXTENSIONS)
			/*
			 *  set the expectation that we will have to
			 * fake up the zlib trailer to the inflator for this
			 * frame
			 */
			wsi->ws->pmd_trailer_application = !!(c & 0x40);
#endif
			wsi->ws->frame_is_binary =
			     wsi->ws->opcode == LWSWSOPC_BINARY_FRAME;
			wsi->ws->first_fragment = 1;
			wsi->ws->continuation_possible = !wsi->ws->final;
			break;
		case LWSWSOPC_CONTINUATION:
			if (!wsi->ws->continuation_possible) {
				lws_close_reason(wsi,
					LWS_CLOSE_STATUS_PROTOCOL_ERR,
					(uint8_t *)"bad cont", 8);
				return -1;
			}
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
			lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR,
					(uint8_t *)"bad opc", 7);
			lwsl_info("illegal opcode\n");
			return -1;
		}

		if (wsi->ws->owed_a_fin &&
		    (wsi->ws->opcode == LWSWSOPC_TEXT_FRAME ||
		     wsi->ws->opcode == LWSWSOPC_BINARY_FRAME)) {
			lwsl_info("hey you owed us a FIN\n");
			lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR,
					(uint8_t *)"bad fin", 7);
			return -1;
		}
		if ((!(wsi->ws->opcode & 8)) && wsi->ws->final) {
			wsi->ws->continuation_possible = 0;
			wsi->ws->owed_a_fin = 0;
		}

		if (!wsi->ws->final)
			wsi->ws->owed_a_fin = 1;

		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN;
		if (wsi->ws->rsv &&
		    (
#if !defined(LWS_WITHOUT_EXTENSIONS)
				    !wsi->ws->count_act_ext ||
#endif
				    (wsi->ws->rsv & ~0x40))) {
			lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR,
					 (uint8_t *)"rsv bits", 8);
			return -1;
		}
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN:

		wsi->ws->this_frame_masked = !!(c & 0x80);

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
			else
				if (wsi->ws->rx_packet_length) {
					wsi->lws_rx_parse_state =
					LWS_RXPS_WS_FRAME_PAYLOAD;
				} else {
					wsi->lws_rx_parse_state = LWS_RXPS_NEW;
					goto spill;
				}
			break;
		}
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN16_2:
		wsi->ws->rx_packet_length = (size_t)(c << 8);
		wsi->lws_rx_parse_state = LWS_RXPS_04_FRAME_HDR_LEN16_1;
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN16_1:
		wsi->ws->rx_packet_length |= c;
		if (wsi->ws->this_frame_masked)
			wsi->lws_rx_parse_state =
					LWS_RXPS_07_COLLECT_FRAME_KEY_1;
		else {
			wsi->lws_rx_parse_state =
				LWS_RXPS_WS_FRAME_PAYLOAD;
		}
		break;

	case LWS_RXPS_04_FRAME_HDR_LEN64_8:
		if (c & 0x80) {
			lwsl_warn("b63 of length must be zero\n");
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
		wsi->ws->rx_packet_length |= ((size_t)c);
		if (wsi->ws->this_frame_masked)
			wsi->lws_rx_parse_state =
					LWS_RXPS_07_COLLECT_FRAME_KEY_1;
		else
			wsi->lws_rx_parse_state = LWS_RXPS_WS_FRAME_PAYLOAD;
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
		wsi->lws_rx_parse_state = LWS_RXPS_WS_FRAME_PAYLOAD;
		wsi->ws->mask_idx = 0;
		if (wsi->ws->rx_packet_length == 0) {
			wsi->lws_rx_parse_state = LWS_RXPS_NEW;
			goto spill;
		}
		break;


	case LWS_RXPS_WS_FRAME_PAYLOAD:
		assert(wsi->ws->rx_ubuf);

		if (wsi->ws->rx_ubuf_head + LWS_PRE >= wsi->ws->rx_ubuf_alloc) {
			lwsl_err("Attempted overflow \n");
			return -1;
		}
		if (!(already_processed & ALREADY_PROCESSED_IGNORE_CHAR)) {
			if (wsi->ws->all_zero_nonce)
				wsi->ws->rx_ubuf[LWS_PRE +
				                 (wsi->ws->rx_ubuf_head++)] = c;
			else
				wsi->ws->rx_ubuf[LWS_PRE +
				                 (wsi->ws->rx_ubuf_head++)] =
				   c ^ wsi->ws->mask[(wsi->ws->mask_idx++) & 3];

			--wsi->ws->rx_packet_length;
		}

		if (!wsi->ws->rx_packet_length) {
			lwsl_debug("%s: ws fragment length exhausted\n",
				   __func__);
			/* spill because we have the whole frame */
			wsi->lws_rx_parse_state = LWS_RXPS_NEW;
			goto spill;
		}
#if !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws->rx_draining_ext) {
			lwsl_debug("%s: UNTIL_EXHAUSTED draining\n", __func__);
			goto drain_extension;
		}
#endif
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
spill:
		/*
		 * is this frame a control packet we should take care of at this
		 * layer?  If so service it and hide it from the user callback
		 */

		lwsl_parser("spill on %s\n", wsi->a.protocol->name);

		switch (wsi->ws->opcode) {
		case LWSWSOPC_CLOSE:

			if (wsi->ws->peer_has_sent_close)
				break;

			wsi->ws->peer_has_sent_close = 1;

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
				lwsl_parser("seen client close ack\n");
				return -1;
			}
			if (lwsi_state(wsi) == LRS_RETURNED_CLOSE)
				/* if he sends us 2 CLOSE, kill him */
				return -1;

			if (lws_partial_buffered(wsi)) {
				/*
				 * if we're in the middle of something,
				 * we can't do a normal close response and
				 * have to just close our end.
				 */
				wsi->socket_is_permanently_unusable = 1;
				lwsl_parser("Closing on peer close "
					    "due to pending tx\n");
				return -1;
			}

			if (wsi->ws->rx_ubuf_head >= 2) {
				close_code = (unsigned short)((pp[0] << 8) | pp[1]);
				if (close_code < 1000 ||
				    close_code == 1004 ||
				    close_code == 1005 ||
				    close_code == 1006 ||
				    close_code == 1012 ||
				    close_code == 1013 ||
				    close_code == 1014 ||
				    close_code == 1015 ||
				    (close_code >= 1016 && close_code < 3000)
				) {
					pp[0] = (LWS_CLOSE_STATUS_PROTOCOL_ERR >> 8) & 0xff;
					pp[1] = LWS_CLOSE_STATUS_PROTOCOL_ERR & 0xff;
				}
			}

			if (user_callback_handle_rxflow(
					wsi->a.protocol->callback, wsi,
					LWS_CALLBACK_WS_PEER_INITIATED_CLOSE,
					wsi->user_space,
					&wsi->ws->rx_ubuf[LWS_PRE],
					wsi->ws->rx_ubuf_head))
				return -1;

			lwsl_parser("server sees client close packet\n");
			lwsi_set_state(wsi, LRS_RETURNED_CLOSE);
			/* deal with the close packet contents as a PONG */
			wsi->ws->payload_is_close = 1;
			goto process_as_ping;

		case LWSWSOPC_PING:
			lwsl_info("received %d byte ping, sending pong\n",
						 (int)wsi->ws->rx_ubuf_head);

			if (wsi->ws->pong_pending_flag) {
				/*
				 * there is already a pending pong payload
				 * we should just log and drop
				 */
				lwsl_parser("DROP PING since one pending\n");
				goto ping_drop;
			}
process_as_ping:
			/* control packets can only be < 128 bytes long */
			if (wsi->ws->rx_ubuf_head > 128 - 3) {
				lwsl_parser("DROP PING payload too large\n");
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
			return 0;

		case LWSWSOPC_PONG:
			lwsl_info("received pong\n");
			lwsl_hexdump(&wsi->ws->rx_ubuf[LWS_PRE],
			             wsi->ws->rx_ubuf_head);

			lws_validity_confirmed(wsi);

			/* issue it */
			callback_action = LWS_CALLBACK_RECEIVE_PONG;
			break;

		case LWSWSOPC_TEXT_FRAME:
		case LWSWSOPC_BINARY_FRAME:
		case LWSWSOPC_CONTINUATION:
			break;

		default:
			lwsl_parser("unknown opc %x\n", wsi->ws->opcode);

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

		pmdrx.eb_in.token = &wsi->ws->rx_ubuf[LWS_PRE];
		pmdrx.eb_in.len = (int)wsi->ws->rx_ubuf_head;

		/* for the non-pm-deflate case */

		pmdrx.eb_out = pmdrx.eb_in;

		if (wsi->ws->opcode == LWSWSOPC_PONG && !pmdrx.eb_in.len)
			goto already_done;
#if !defined(LWS_WITHOUT_EXTENSIONS)
drain_extension:
#endif

		do {

//			lwsl_notice("%s: pmdrx.eb_in.len: %d\n", __func__,
//					(int)pmdrx.eb_in.len);

			if (lwsi_state(wsi) == LRS_RETURNED_CLOSE ||
			    lwsi_state(wsi) == LRS_AWAITING_CLOSE_ACK)
				goto already_done;

			n = PMDR_DID_NOTHING;

#if !defined(LWS_WITHOUT_EXTENSIONS)
			lin = pmdrx.eb_in.len;
			//if (lin)
			//	lwsl_hexdump_notice(ebuf.token, ebuf.len);
			lwsl_ext("%s: +++ passing %d %p to ext\n", __func__,
					pmdrx.eb_in.len, pmdrx.eb_in.token);

			n = lws_ext_cb_active(wsi, LWS_EXT_CB_PAYLOAD_RX, &pmdrx, 0);
			lwsl_debug("%s: ext says %d / ebuf.len %d\n", __func__,
				   n, pmdrx.eb_out.len);
			if (wsi->ws->rx_draining_ext)
				already_processed &= (char)~ALREADY_PROCESSED_NO_CB;
#endif

			/*
			 * ebuf may be pointing somewhere completely different
			 * now, it's the output
			 */
#if !defined(LWS_WITHOUT_EXTENSIONS)
			if (n < 0) {
				/*
				 * we may rely on this to get RX, just drop
				 * connection
				 */
				wsi->socket_is_permanently_unusable = 1;
				return -1;
			}
			if (n == PMDR_DID_NOTHING)
				/* ie, not PMDR_NOTHING_WE_SHOULD_DO */
				break;
#endif
			lwsl_debug("%s: post ext ret %d, ebuf in %d / out %d\n",
				    __func__, n, pmdrx.eb_in.len,
				    pmdrx.eb_out.len);

#if !defined(LWS_WITHOUT_EXTENSIONS)
			if (rx_draining_ext && !pmdrx.eb_out.len) {
				lwsl_debug("   --- ending drain on 0 read\n");
				goto already_done;
			}

			if (n == PMDR_HAS_PENDING)
				/*
				 * extension had more...
				 * main loop will come back
				 */
				lws_add_wsi_to_draining_ext_list(wsi);
			else
				lws_remove_wsi_from_draining_ext_list(wsi);

			rx_draining_ext = wsi->ws->rx_draining_ext;
#endif

			if (pmdrx.eb_out.len &&
			    wsi->ws->check_utf8 && !wsi->ws->defeat_check_utf8) {
				if (lws_check_utf8(&wsi->ws->utf8,
						   pmdrx.eb_out.token,
						   (size_t)pmdrx.eb_out.len)) {
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
					lwsl_info("FINAL utf8 error\n");
					lws_close_reason(wsi,
						LWS_CLOSE_STATUS_INVALID_PAYLOAD,
						(uint8_t *)"partial utf8", 12);
utf8_fail:
					lwsl_notice("utf8 error\n");
					lwsl_hexdump_notice(pmdrx.eb_out.token,
							    (size_t)pmdrx.eb_out.len);

					return -1;
				}
			}

			/* if pmd not enabled, in == out */

			if (n == PMDR_DID_NOTHING
#if !defined(LWS_WITHOUT_EXTENSIONS)
					||
			    n == PMDR_NOTHING_WE_SHOULD_DO ||
			    n == PMDR_UNKNOWN
#endif
			    )
				pmdrx.eb_in.len -= pmdrx.eb_out.len;

	if (!wsi->wsistate_pre_close &&
			    (pmdrx.eb_out.len >= 0 ||
			     callback_action == LWS_CALLBACK_RECEIVE_PONG ||
						       n == PMDR_EMPTY_FINAL)) {
				if (pmdrx.eb_out.len)
					pmdrx.eb_out.token[pmdrx.eb_out.len] = '\0';

				if (wsi->a.protocol->callback &&
				    !(already_processed & ALREADY_PROCESSED_NO_CB)) {
					if (callback_action ==
						      LWS_CALLBACK_RECEIVE_PONG)
						lwsl_info("Doing pong callback\n");

					ret = user_callback_handle_rxflow(
						wsi->a.protocol->callback, wsi,
						(enum lws_callback_reasons)
							     callback_action,
						wsi->user_space,
						pmdrx.eb_out.token,
						(size_t)pmdrx.eb_out.len);
				}
				wsi->ws->first_fragment = 0;
			}

#if !defined(LWS_WITHOUT_EXTENSIONS)
			if (!lin)
				break;
#endif

		} while (pmdrx.eb_in.len
#if !defined(LWS_WITHOUT_EXTENSIONS)
				|| rx_draining_ext
#endif
		);

already_done:
		wsi->ws->rx_ubuf_head = 0;
		break;
	}

	return ret;

illegal_ctl_length:

	lwsl_warn("Control frame with xtended length is illegal\n");
	/* kill the connection */
	return -1;
}


size_t
lws_remaining_packet_payload(struct lws *wsi)
{
	return wsi->ws->rx_packet_length;
}

int lws_frame_is_binary(struct lws *wsi)
{
	return wsi->ws->frame_is_binary;
}

void
lws_add_wsi_to_draining_ext_list(struct lws *wsi)
{
#if !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	if (wsi->ws->rx_draining_ext)
		return;

	lwsl_debug("%s: RX EXT DRAINING: Adding to list\n", __func__);

	wsi->ws->rx_draining_ext = 1;
	wsi->ws->rx_draining_ext_list = pt->ws.rx_draining_ext_list;
	pt->ws.rx_draining_ext_list = wsi;
#endif
}

void
lws_remove_wsi_from_draining_ext_list(struct lws *wsi)
{
#if !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws **w = &pt->ws.rx_draining_ext_list;

	if (!wsi->ws->rx_draining_ext)
		return;

	lwsl_debug("%s: RX EXT DRAINING: Removing from list\n", __func__);

	wsi->ws->rx_draining_ext = 0;

	/* remove us from context draining ext list */
	while (*w) {
		if (*w == wsi) {
			/* if us, point it instead to who we were pointing to */
			*w = wsi->ws->rx_draining_ext_list;
			break;
		}
		w = &((*w)->ws->rx_draining_ext_list);
	}
	wsi->ws->rx_draining_ext_list = NULL;
#endif
}

static int
lws_0405_frame_mask_generate(struct lws *wsi)
{
	size_t n;
	/* fetch the per-frame nonce */

	n = lws_get_random(lws_get_context(wsi), wsi->ws->mask, 4);
	if (n != 4) {
		lwsl_parser("Unable to read from random device %s %d\n",
			    SYSTEM_RANDOM_FILEPATH, (int)n);
		return 1;
	}

	/* start masking from first byte of masking key buffer */
	wsi->ws->mask_idx = 0;

	return 0;
}

int
lws_server_init_wsi_for_ws(struct lws *wsi)
{
	int n;

	lwsi_set_state(wsi, LRS_ESTABLISHED);

	/*
	 * create the frame buffer for this connection according to the
	 * size mentioned in the protocol definition.  If 0 there, use
	 * a big default for compatibility
	 */

	n = (int)wsi->a.protocol->rx_buffer_size;
	if (!n)
		n = (int)wsi->a.context->pt_serv_buf_size;
	n += LWS_PRE;
	wsi->ws->rx_ubuf = lws_malloc((unsigned int)n + 4 /* 0x0000ffff zlib */, "rx_ubuf");
	if (!wsi->ws->rx_ubuf) {
		lwsl_err("Out of Mem allocating rx buffer %d\n", n);
		return 1;
	}
	wsi->ws->rx_ubuf_alloc = (uint32_t)n;

	/* notify user code that we're ready to roll */

	if (wsi->a.protocol->callback)
		if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_ESTABLISHED,
					    wsi->user_space,
#ifdef LWS_WITH_TLS
					    wsi->tls.ssl,
#else
					    NULL,
#endif
					    wsi->h2_stream_carries_ws))
			return 1;

	lws_validity_confirmed(wsi);
	lwsl_debug("ws established\n");

	return 0;
}



int
lws_is_final_fragment(struct lws *wsi)
{
#if !defined(LWS_WITHOUT_EXTENSIONS)
	lwsl_debug("%s: final %d, rx pk length %ld, draining %ld\n", __func__,
		   wsi->ws->final, (long)wsi->ws->rx_packet_length,
		   (long)wsi->ws->rx_draining_ext);
	return wsi->ws->final && !wsi->ws->rx_packet_length &&
	       !wsi->ws->rx_draining_ext;
#else
	return wsi->ws->final && !wsi->ws->rx_packet_length;
#endif
}

int
lws_is_first_fragment(struct lws *wsi)
{
	return wsi->ws->first_fragment;
}

unsigned char
lws_get_reserved_bits(struct lws *wsi)
{
	return wsi->ws->rsv;
}

int
lws_get_close_length(struct lws *wsi)
{
	return wsi->ws->close_in_ping_buffer_len;
}

unsigned char *
lws_get_close_payload(struct lws *wsi)
{
	return &wsi->ws->ping_payload_buf[LWS_PRE];
}

void
lws_close_reason(struct lws *wsi, enum lws_close_status status,
		 unsigned char *buf, size_t len)
{
	unsigned char *p, *start;
	int budget = sizeof(wsi->ws->ping_payload_buf) - LWS_PRE;

	assert(lwsi_role_ws(wsi));

	start = p = &wsi->ws->ping_payload_buf[LWS_PRE];

	*p++ = (uint8_t)((((int)status) >> 8) & 0xff);
	*p++ = (uint8_t)(((int)status) & 0xff);

	if (buf)
		while (len-- && p < start + budget)
			*p++ = *buf++;

	wsi->ws->close_in_ping_buffer_len = (uint8_t)lws_ptr_diff(p, start);
}

static int
lws_is_ws_with_ext(struct lws *wsi)
{
#if defined(LWS_WITHOUT_EXTENSIONS)
	return 0;
#else
	return lwsi_role_ws(wsi) && !!wsi->ws->count_act_ext;
#endif
}

static int
rops_handle_POLLIN_ws(struct lws_context_per_thread *pt, struct lws *wsi,
		       struct lws_pollfd *pollfd)
{
	unsigned int pending = 0;
	struct lws_tokens ebuf;
	char buffered = 0;
	int n = 0, m, sanity = 10;
#if defined(LWS_WITH_HTTP2)
	struct lws *wsi1;
#endif

	if (!wsi->ws) {
		lwsl_err("ws role wsi with no ws\n");
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	// lwsl_notice("%s: %s\n", __func__, wsi->a.protocol->name);

	//lwsl_info("%s: wsistate 0x%x, pollout %d\n", __func__,
	//	   wsi->wsistate, pollfd->revents & LWS_POLLOUT);

	/*
	 * something went wrong with parsing the handshake, and
	 * we ended up back in the event loop without completing it
	 */
	if (lwsi_state(wsi) == LRS_PRE_WS_SERVING_ACCEPT) {
		wsi->socket_is_permanently_unusable = 1;
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	ebuf.token = NULL;
	ebuf.len = 0;

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

		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	if (lwsi_state(wsi) == LRS_RETURNED_CLOSE ||
	    lwsi_state(wsi) == LRS_WAITING_TO_SEND_CLOSE) {
		/*
		 * we stopped caring about anything except control
		 * packets.  Force flow control off, defeat tx
		 * draining.
		 */
		lws_rx_flow_control(wsi, 1);
#if !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws)
			wsi->ws->tx_draining_ext = 0;
#endif
	}
#if !defined(LWS_WITHOUT_EXTENSIONS)
	if (wsi->ws->tx_draining_ext) {
		lws_handle_POLLOUT_event(wsi, pollfd);
		//lwsl_notice("%s: tx drain\n", __func__);
		/*
		 * We cannot deal with new RX until the TX ext path has
		 * been drained.  It's because new rx will, eg, crap on
		 * the wsi rx buf that may be needed to retain state.
		 *
		 * TX ext drain path MUST go through event loop to avoid
		 * blocking.
		 */
		lws_callback_on_writable(wsi);
		return LWS_HPI_RET_HANDLED;
	}
#endif
	if ((pollfd->revents & LWS_POLLIN) && lws_is_flowcontrolled(wsi)) {
		/* We cannot deal with any kind of new RX because we are
		 * RX-flowcontrolled.
		 */
		lwsl_info("%s: flowcontrolled, ignoring rx\n", __func__);

		if (__lws_change_pollfd(wsi, LWS_POLLIN, 0))
			return -1;

		return LWS_HPI_RET_HANDLED;
	}

	if (lws_is_flowcontrolled(wsi))
		return LWS_HPI_RET_HANDLED;

#if defined(LWS_WITH_HTTP2)
	if (wsi->mux_substream || wsi->upgraded_to_http2) {
		wsi1 = lws_get_network_wsi(wsi);
		if (wsi1 && lws_has_buffered_out(wsi1))
			/* We cannot deal with any kind of new RX
			 * because we are dealing with a partial send
			 * (new RX may trigger new http_action() that
			 * expect to be able to send)
			 */
			return LWS_HPI_RET_HANDLED;
	}
#endif

#if !defined(LWS_WITHOUT_EXTENSIONS)
	/* 2: RX Extension needs to be drained
	 */

	if (wsi->ws->rx_draining_ext) {

		lwsl_debug("%s: RX EXT DRAINING: Service\n", __func__);
#if defined(LWS_WITH_CLIENT)
		if (lwsi_role_client(wsi)) {
			n = lws_ws_client_rx_sm(wsi, 0);
			if (n < 0)
				/* we closed wsi */
				return LWS_HPI_RET_PLEASE_CLOSE_ME;
		} else
#endif
			n = lws_ws_rx_sm(wsi, ALREADY_PROCESSED_IGNORE_CHAR, 0);

		return LWS_HPI_RET_HANDLED;
	}

	if (wsi->ws->rx_draining_ext)
		/*
		 * We have RX EXT content to drain, but can't do it
		 * right now.  That means we cannot do anything lower
		 * priority either.
		 */
		return LWS_HPI_RET_HANDLED;
#endif

	/* 3: buflist needs to be drained
	 */
read:
	//lws_buflist_describe(&wsi->buflist, wsi, __func__);
	ebuf.len = (int)lws_buflist_next_segment_len(&wsi->buflist,
						     &ebuf.token);
	if (ebuf.len) {
		lwsl_info("draining buflist (len %d)\n", ebuf.len);
		buffered = 1;
		goto drain;
	}

	if (!(pollfd->revents & pollfd->events & LWS_POLLIN) && !wsi->http.ah)
		return LWS_HPI_RET_HANDLED;

	if (lws_is_flowcontrolled(wsi)) {
		lwsl_info("%s: %p should be rxflow (bm 0x%x)..\n",
			    __func__, wsi, wsi->rxflow_bitmap);
		return LWS_HPI_RET_HANDLED;
	}

	if (!(lwsi_role_client(wsi) &&
	      (lwsi_state(wsi) != LRS_ESTABLISHED &&
	       lwsi_state(wsi) != LRS_AWAITING_CLOSE_ACK &&
	       lwsi_state(wsi) != LRS_H2_WAITING_TO_SEND_HEADERS))) {
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
		if (lwsi_role_ws(wsi))
			ebuf.len = (int)wsi->ws->rx_ubuf_alloc;
		else
			ebuf.len = (int)wsi->a.context->pt_serv_buf_size;

		if ((unsigned int)ebuf.len > wsi->a.context->pt_serv_buf_size)
			ebuf.len = (int)wsi->a.context->pt_serv_buf_size;

		if ((int)pending > ebuf.len)
			pending = (unsigned int)ebuf.len;

		ebuf.len = lws_ssl_capable_read(wsi, ebuf.token,
						(size_t)(pending ? pending :
						(unsigned int)ebuf.len));
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

	/*
	 * give any active extensions a chance to munge the buffer
	 * before parse.  We pass in a pointer to an lws_tokens struct
	 * prepared with the default buffer and content length that's in
	 * there.  Rather than rewrite the default buffer, extensions
	 * that expect to grow the buffer can adapt .token to
	 * point to their own per-connection buffer in the extension
	 * user allocation.  By default with no extensions or no
	 * extension callback handling, just the normal input buffer is
	 * used then so it is efficient.
	 */
	m = 0;
	do {

		/* service incoming data */
		//lws_buflist_describe(&wsi->buflist, wsi, __func__);
		if (ebuf.len > 0) {
#if defined(LWS_ROLE_H2)
			if (lwsi_role_h2(wsi) && lwsi_state(wsi) != LRS_BODY &&
			    lwsi_state(wsi) != LRS_DISCARD_BODY)
				n = lws_read_h2(wsi, ebuf.token,
					     (unsigned int)ebuf.len);
			else
#endif
				n = lws_read_h1(wsi, ebuf.token,
					     (unsigned int)ebuf.len);

			if (n < 0) {
				/* we closed wsi */
				return LWS_HPI_RET_WSI_ALREADY_DIED;
			}
			//lws_buflist_describe(&wsi->buflist, wsi, __func__);
			//lwsl_notice("%s: consuming %d / %d\n", __func__, n, ebuf.len);
			if (ebuf.len < 0 ||
			    lws_buflist_aware_finished_consuming(wsi, &ebuf, n,
							buffered, __func__))
				return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		ebuf.token = NULL;
		ebuf.len = 0;
	} while (m);

	if (wsi->http.ah
#if defined(LWS_WITH_CLIENT)
			&& !wsi->client_h2_alpn
#endif
			) {
		lwsl_info("%s: %p: detaching ah\n", __func__, wsi);
		lws_header_table_detach(wsi, 0);
	}

	pending = (unsigned int)lws_ssl_pending(wsi);

#if defined(LWS_WITH_CLIENT)
	if (!pending && (wsi->flags & LCCSCF_PRIORITIZE_READS) &&
	    lws_buflist_total_len(&wsi->buflist))
		pending = 9999999;
#endif

	if (pending) {
		if (lws_is_ws_with_ext(wsi))
			pending = pending > wsi->ws->rx_ubuf_alloc ?
				wsi->ws->rx_ubuf_alloc : pending;
		else
			pending = pending > wsi->a.context->pt_serv_buf_size ?
				wsi->a.context->pt_serv_buf_size : pending;
		if (--sanity)
			goto read;
		else
			/*
			 * Something has gone wrong, we are spinning...
			 * let's bail on this connection
			 */
			return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	if (buffered && /* were draining, now nothing left */
	    !lws_buflist_next_segment_len(&wsi->buflist, NULL)) {
		lwsl_info("%s: %p flow buf: drained\n", __func__, wsi);
		/* having drained the rxflow buffer, can rearm POLLIN */
#if !defined(LWS_WITH_SERVER)
		n =
#endif
		__lws_rx_flow_control(wsi);
		/* n ignored, needed for NO_SERVER case */
	}

	/* n = 0 */
	return LWS_HPI_RET_HANDLED;
}


int rops_handle_POLLOUT_ws(struct lws *wsi)
{
	int write_type = LWS_WRITE_PONG;
#if !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_ext_pm_deflate_rx_ebufs pmdrx;
	int ret, m;
#endif
	int n;

#if !defined(LWS_WITHOUT_EXTENSIONS)
	lwsl_debug("%s: %s: wsi->ws->tx_draining_ext %d\n", __func__,
			wsi->a.protocol->name, wsi->ws->tx_draining_ext);
#endif

	/* Priority 3: pending control packets (pong or close)
	 *
	 * 3a: close notification packet requested from close api
	 */

	if (lwsi_state(wsi) == LRS_WAITING_TO_SEND_CLOSE) {
		lwsl_debug("sending close packet\n");
		lwsl_hexdump_debug(&wsi->ws->ping_payload_buf[LWS_PRE],
				   wsi->ws->close_in_ping_buffer_len);
		wsi->waiting_to_send_close_frame = 0;
		n = lws_write(wsi, &wsi->ws->ping_payload_buf[LWS_PRE],
			      wsi->ws->close_in_ping_buffer_len,
			      LWS_WRITE_CLOSE);
		if (n >= 0) {
			if (wsi->close_needs_ack) {
				lwsi_set_state(wsi, LRS_AWAITING_CLOSE_ACK);
				lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_ACK,
						5);
				lwsl_debug("sent close, await ack\n");

				return LWS_HP_RET_BAIL_OK;
			}
			wsi->close_needs_ack = 0;
			lwsi_set_state(wsi, LRS_RETURNED_CLOSE);
		}

		return LWS_HP_RET_BAIL_DIE;
	}

	/* else, the send failed and we should just hang up */

	if ((lwsi_role_ws(wsi) && wsi->ws->pong_pending_flag) ||
	    (lwsi_state(wsi) == LRS_RETURNED_CLOSE &&
	     wsi->ws->payload_is_close)) {

		if (wsi->ws->payload_is_close)
			write_type = LWS_WRITE_CLOSE;
		else {
			if (wsi->wsistate_pre_close) {
				/* we started close flow, forget pong */
				wsi->ws->pong_pending_flag = 0;
				return LWS_HP_RET_BAIL_OK;
			}
			lwsl_info("issuing pong %d on %s\n",
				  wsi->ws->pong_payload_len, lws_wsi_tag(wsi));
		}

		n = lws_write(wsi, &wsi->ws->pong_payload_buf[LWS_PRE],
			      wsi->ws->pong_payload_len, (enum lws_write_protocol)write_type);
		if (n < 0)
			return LWS_HP_RET_BAIL_DIE;

		/* well he is sent, mark him done */
		wsi->ws->pong_pending_flag = 0;
		if (wsi->ws->payload_is_close) {
			// assert(0);
			/* oh... a close frame was it... then we are done */
			return LWS_HP_RET_BAIL_DIE;
		}

		/* otherwise for PING, leave POLLOUT active either way */
		return LWS_HP_RET_BAIL_OK;
	}

	if (!wsi->socket_is_permanently_unusable &&
	    wsi->ws->send_check_ping) {

		lwsl_info("%s: issuing ping on wsi %s: %s %s h2: %d\n", __func__,
				lws_wsi_tag(wsi),
				wsi->role_ops->name, wsi->a.protocol->name,
				wsi->mux_substream);
		wsi->ws->send_check_ping = 0;
		n = lws_write(wsi, &wsi->ws->ping_payload_buf[LWS_PRE],
			      0, LWS_WRITE_PING);
		if (n < 0)
			return LWS_HP_RET_BAIL_DIE;

		return LWS_HP_RET_BAIL_OK;
	}

	/* Priority 4: if we are closing, not allowed to send more data frags
	 *	       which means user callback or tx ext flush banned now
	 */
	if (lwsi_state(wsi) == LRS_RETURNED_CLOSE)
		return LWS_HP_RET_USER_SERVICE;

#if !defined(LWS_WITHOUT_EXTENSIONS)
	/* Priority 5: Tx path extension with more to send
	 *
	 *	       These are handled as new fragments each time around
	 *	       So while we must block new writeable callback to enforce
	 *	       payload ordering, but since they are always complete
	 *	       fragments control packets can interleave OK.
	 */
	if (wsi->ws->tx_draining_ext) {
		lwsl_ext("SERVICING TX EXT DRAINING\n");
		if (lws_write(wsi, NULL, 0, LWS_WRITE_CONTINUATION) < 0)
			return LWS_HP_RET_BAIL_DIE;
		/* leave POLLOUT active */
		return LWS_HP_RET_BAIL_OK;
	}

	/* Priority 6: extensions
	 */
	if (!wsi->ws->extension_data_pending && !wsi->ws->tx_draining_ext) {
		lwsl_ext("%s: !wsi->ws->extension_data_pending\n", __func__);
		return LWS_HP_RET_USER_SERVICE;
	}

	/*
	 * Check in on the active extensions, see if they had pending stuff to
	 * spill... they need to get the first look-in otherwise sequence will
	 * be disordered.
	 *
	 * coming here with a NULL, zero-length ebuf means just spill pending
	 */

	ret = 1;
	if (wsi->role_ops == &role_ops_raw_skt
#if defined(LWS_ROLE_RAW_FILE)
		|| wsi->role_ops == &role_ops_raw_file
#endif
	    )
		ret = 0;

	while (ret == 1) {

		/* default to nobody has more to spill */

		ret = 0;
		pmdrx.eb_in.token = NULL;
		pmdrx.eb_in.len = 0;

		/* give every extension a chance to spill */

		m = lws_ext_cb_active(wsi, LWS_EXT_CB_PACKET_TX_PRESEND,
				      &pmdrx, 0);
		if (m < 0) {
			lwsl_err("ext reports fatal error\n");
			return LWS_HP_RET_BAIL_DIE;
		}
		if (m)
			/*
			 * at least one extension told us he has more
			 * to spill, so we will go around again after
			 */
			ret = 1;

		/* assuming they gave us something to send, send it */

		if (pmdrx.eb_in.len) {
			n = lws_issue_raw(wsi, (unsigned char *)pmdrx.eb_in.token,
					(unsigned int)pmdrx.eb_in.len);
			if (n < 0) {
				lwsl_info("closing from POLLOUT spill\n");
				return LWS_HP_RET_BAIL_DIE;
			}
			/*
			 * Keep amount spilled small to minimize chance of this
			 */
			if (n != pmdrx.eb_in.len) {
				lwsl_err("Unable to spill ext %d vs %d\n",
						pmdrx.eb_in.len, n);
				return LWS_HP_RET_BAIL_DIE;
			}
		} else
			continue;

		/* no extension has more to spill */

		if (!ret)
			continue;

		/*
		 * There's more to spill from an extension, but we just sent
		 * something... did that leave the pipe choked?
		 */

		if (!lws_send_pipe_choked(wsi))
			/* no we could add more */
			continue;

		lwsl_info("choked in POLLOUT service\n");

		/*
		 * Yes, he's choked.  Leave the POLLOUT masked on so we will
		 * come back here when he is unchoked.  Don't call the user
		 * callback to enforce ordering of spilling, he'll get called
		 * when we come back here and there's nothing more to spill.
		 */

		return LWS_HP_RET_BAIL_OK;
	}

	wsi->ws->extension_data_pending = 0;
#endif

	return LWS_HP_RET_USER_SERVICE;
}

static int
rops_service_flag_pending_ws(struct lws_context *context, int tsi)
{
#if !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws *wsi;
	int forced = 0;

	/* POLLIN faking (the pt lock is taken by the parent) */

	/*
	 * 1) For all guys with already-available ext data to drain, if they are
	 * not flowcontrolled, fake their POLLIN status
	 */
	wsi = pt->ws.rx_draining_ext_list;
	while (wsi && wsi->position_in_fds_table != LWS_NO_FDS_POS) {
		pt->fds[wsi->position_in_fds_table].revents =
			(short)((short)pt->fds[wsi->position_in_fds_table].revents |
			(short)(pt->fds[wsi->position_in_fds_table].events & LWS_POLLIN));
		if (pt->fds[wsi->position_in_fds_table].revents & LWS_POLLIN)
			forced = 1;

		wsi = wsi->ws->rx_draining_ext_list;
	}

	return forced;
#else
	return 0;
#endif
}

static int
rops_close_via_role_protocol_ws(struct lws *wsi, enum lws_close_status reason)
{
	if (!wsi->ws)
		return 0;

	if (!wsi->ws->close_in_ping_buffer_len && /* already a reason */
	     (reason == LWS_CLOSE_STATUS_NOSTATUS ||
	      reason == LWS_CLOSE_STATUS_NOSTATUS_CONTEXT_DESTROY))
		return 0;

	lwsl_debug("%s: sending close indication...\n", __func__);

	/* if no prepared close reason, use 1000 and no aux data */

	if (!wsi->ws->close_in_ping_buffer_len) {
		wsi->ws->close_in_ping_buffer_len = 2;
		wsi->ws->ping_payload_buf[LWS_PRE] = (reason >> 8) & 0xff;
		wsi->ws->ping_payload_buf[LWS_PRE + 1] = reason & 0xff;
	}

	wsi->waiting_to_send_close_frame = 1;
	wsi->close_needs_ack = 1;
	lwsi_set_state(wsi, LRS_WAITING_TO_SEND_CLOSE);
	__lws_set_timeout(wsi, PENDING_TIMEOUT_CLOSE_SEND, 5);

	lws_callback_on_writable(wsi);

	return 1;
}

static int
rops_close_role_ws(struct lws_context_per_thread *pt, struct lws *wsi)
{
	if (!wsi->ws)
		return 0;

#if !defined(LWS_WITHOUT_EXTENSIONS)

	if (wsi->ws->rx_draining_ext) {
		struct lws **w = &pt->ws.rx_draining_ext_list;

		wsi->ws->rx_draining_ext = 0;
		/* remove us from context draining ext list */
		while (*w) {
			if (*w == wsi) {
				*w = wsi->ws->rx_draining_ext_list;
				break;
			}
			w = &((*w)->ws->rx_draining_ext_list);
		}
		wsi->ws->rx_draining_ext_list = NULL;
	}

	if (wsi->ws->tx_draining_ext) {
		struct lws **w = &pt->ws.tx_draining_ext_list;
		lwsl_ext("%s: CLEARING tx_draining_ext\n", __func__);
		wsi->ws->tx_draining_ext = 0;
		/* remove us from context draining ext list */
		while (*w) {
			if (*w == wsi) {
				*w = wsi->ws->tx_draining_ext_list;
				break;
			}
			w = &((*w)->ws->tx_draining_ext_list);
		}
		wsi->ws->tx_draining_ext_list = NULL;
	}
#endif
	lws_free_set_NULL(wsi->ws->rx_ubuf);

	wsi->ws->pong_payload_len = 0;
	wsi->ws->pong_pending_flag = 0;

	/* deallocate any active extension contexts */

	if (lws_ext_cb_active(wsi, LWS_EXT_CB_DESTROY, NULL, 0) < 0)
		lwsl_warn("extension destruction failed\n");

	return 0;
}

static int
rops_write_role_protocol_ws(struct lws *wsi, unsigned char *buf, size_t len,
			    enum lws_write_protocol *wp)
{
#if !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	enum lws_write_protocol wpt;
#endif
	struct lws_ext_pm_deflate_rx_ebufs pmdrx;
	int masked7 = lwsi_role_client(wsi);
	unsigned char is_masked_bit = 0;
	unsigned char *dropmask = NULL;
	size_t orig_len = len;
	int pre = 0, n = 0;

	// lwsl_err("%s: wp 0x%x len %d\n", __func__, *wp, (int)len);
#if !defined(LWS_WITHOUT_EXTENSIONS)
	if (wsi->ws->tx_draining_ext) {
		/* remove us from the list */
		struct lws **w = &pt->ws.tx_draining_ext_list;

		lwsl_ext("%s: CLEARING tx_draining_ext\n", __func__);
		wsi->ws->tx_draining_ext = 0;
		/* remove us from context draining ext list */
		while (*w) {
			if (*w == wsi) {
				*w = wsi->ws->tx_draining_ext_list;
				break;
			}
			w = &((*w)->ws->tx_draining_ext_list);
		}
		wsi->ws->tx_draining_ext_list = NULL;

		wpt = *wp;
		*wp = (wsi->ws->tx_draining_stashed_wp & 0xc0) |
				LWS_WRITE_CONTINUATION;

		/*
		 * When we are just flushing (len == 0), we can trust the
		 * stashed wp info completely.  Otherwise adjust it to the
		 * FIN status of the incoming packet.
		 */

		if (!(wpt & LWS_WRITE_NO_FIN) && len)
			*wp &= (enum lws_write_protocol)~LWS_WRITE_NO_FIN;

		lwsl_ext("FORCED draining wp to 0x%02X "
			 "(stashed 0x%02X, incoming 0x%02X)\n", *wp,
			 wsi->ws->tx_draining_stashed_wp, wpt);
		// assert(0);
	}
#endif

	if (((*wp) & 0x1f) == LWS_WRITE_HTTP ||
	    ((*wp) & 0x1f) == LWS_WRITE_HTTP_FINAL ||
	    ((*wp) & 0x1f) == LWS_WRITE_HTTP_HEADERS_CONTINUATION ||
	    ((*wp) & 0x1f) == LWS_WRITE_HTTP_HEADERS)
		goto send_raw;



	/* if we are continuing a frame that already had its header done */

	if (wsi->ws->inside_frame) {
		lwsl_debug("INSIDE FRAME\n");
		goto do_more_inside_frame;
	}

	wsi->ws->clean_buffer = 1;

	/*
	 * give a chance to the extensions to modify payload
	 * the extension may decide to produce unlimited payload erratically
	 * (eg, compression extension), so we require only that if he produces
	 * something, it will be a complete fragment of the length known at
	 * the time (just the fragment length known), and if he has
	 * more we will come back next time he is writeable and allow him to
	 * produce more fragments until he's drained.
	 *
	 * This allows what is sent each time it is writeable to be limited to
	 * a size that can be sent without partial sends or blocking, allows
	 * interleaving of control frames and other connection service.
	 */

	pmdrx.eb_in.token = buf;
	pmdrx.eb_in.len = (int)len;

	/* for the non-pm-deflate case */

	pmdrx.eb_out = pmdrx.eb_in;

	switch ((int)*wp) {
	case LWS_WRITE_PING:
	case LWS_WRITE_PONG:
	case LWS_WRITE_CLOSE:
		break;
	default:
#if !defined(LWS_WITHOUT_EXTENSIONS)
		n = lws_ext_cb_active(wsi, (int)LWS_EXT_CB_PAYLOAD_TX, &pmdrx, (int)*wp);
		if (n < 0)
			return -1;
		lwsl_ext("%s: defl ext ret %d, ext in remaining %d, "
			    "out %d compressed (wp 0x%x)\n", __func__, n,
			    (int)pmdrx.eb_in.len, (int)pmdrx.eb_out.len, *wp);

		if (n == PMDR_HAS_PENDING) {
			lwsl_ext("%s: HAS PENDING: write drain len %d "
				    "(wp 0x%x) SETTING tx_draining_ext "
				    "(remaining in %d)\n", __func__,
				    (int)pmdrx.eb_out.len, *wp,
				    (int)pmdrx.eb_in.len);
			/* extension requires further draining */
			wsi->ws->tx_draining_ext = 1;
			wsi->ws->tx_draining_ext_list =
					pt->ws.tx_draining_ext_list;
			pt->ws.tx_draining_ext_list = wsi;
			/* we must come back to do more */
			lws_callback_on_writable(wsi);
			/*
			 * keep a copy of the write type for the overall
			 * action that has provoked generation of these
			 * fragments, so the last guy can use its FIN state.
			 */
			wsi->ws->tx_draining_stashed_wp = (uint8_t)*wp;
			/*
			 * Despite what we may have thought, this is definitely
			 * NOT the last fragment, because the extension asserted
			 * he has more coming.  For example, the extension may
			 * be compressing, and has saved up everything until the
			 * end, where the output is larger than one chunk.
			 *
			 * Make sure this intermediate one doesn't actually
			 * go out with a FIN.
			 */
			*wp |= LWS_WRITE_NO_FIN;
		}
#endif
		if (pmdrx.eb_out.len && wsi->ws->stashed_write_pending) {
			wsi->ws->stashed_write_pending = 0;
			*wp = (unsigned int)(((*wp) & 0xc0) | (unsigned int)wsi->ws->stashed_write_type);
		}
	}

	/*
	 * an extension did something we need to keep... for example, if
	 * compression extension, it has already updated its state according
	 * to this being issued
	 */
	if (buf != pmdrx.eb_out.token) {
		/*
		 * ext might eat it, but not have anything to issue yet.
		 * In that case we have to follow his lead, but stash and
		 * replace the write type that was lost here the first time.
		 */
		if (len && !pmdrx.eb_out.len) {
			if (!wsi->ws->stashed_write_pending)
				wsi->ws->stashed_write_type =
						(char)(*wp) & 0x3f;
			wsi->ws->stashed_write_pending = 1;
			return (int)len;
		}
		/*
		 * extension recreated it:
		 * need to buffer this if not all sent
		 */
		wsi->ws->clean_buffer = 0;
	}

	buf = pmdrx.eb_out.token;
	len = (unsigned int)pmdrx.eb_out.len;

	if (!buf) {
		lwsl_err("null buf (%d)\n", (int)len);
		return -1;
	}

	switch (wsi->ws->ietf_spec_revision) {
	case 13:
		if (masked7) {
			pre += 4;
			dropmask = &buf[0 - pre];
			is_masked_bit = 0x80;
		}

		switch ((*wp) & 0xf) {
		case LWS_WRITE_TEXT:
			n = LWSWSOPC_TEXT_FRAME;
			break;
		case LWS_WRITE_BINARY:
			n = LWSWSOPC_BINARY_FRAME;
			break;
		case LWS_WRITE_CONTINUATION:
			n = LWSWSOPC_CONTINUATION;
			break;

		case LWS_WRITE_CLOSE:
			n = LWSWSOPC_CLOSE;
			break;
		case LWS_WRITE_PING:
			n = LWSWSOPC_PING;
			break;
		case LWS_WRITE_PONG:
			n = LWSWSOPC_PONG;
			break;
		default:
			lwsl_warn("lws_write: unknown write opc / wp\n");
			return -1;
		}

		if (!((*wp) & LWS_WRITE_NO_FIN))
			n |= 1 << 7;

		if (len < 126) {
			pre += 2;
			buf[-pre] = (uint8_t)n;
			buf[-pre + 1] = (unsigned char)(len | is_masked_bit);
		} else {
			if (len < 65536) {
				pre += 4;
				buf[-pre] = (uint8_t)n;
				buf[-pre + 1] = (uint8_t)(126 | is_masked_bit);
				buf[-pre + 2] = (unsigned char)(len >> 8);
				buf[-pre + 3] = (unsigned char)len;
			} else {
				pre += 10;
				buf[-pre] = (uint8_t)n;
				buf[-pre + 1] = (uint8_t)(127 | is_masked_bit);
#if defined __LP64__
					buf[-pre + 2] = (len >> 56) & 0x7f;
					buf[-pre + 3] = (uint8_t)(len >> 48);
					buf[-pre + 4] = (uint8_t)(len >> 40);
					buf[-pre + 5] = (uint8_t)(len >> 32);
#else
					buf[-pre + 2] = 0;
					buf[-pre + 3] = 0;
					buf[-pre + 4] = 0;
					buf[-pre + 5] = 0;
#endif
				buf[-pre + 6] = (unsigned char)(len >> 24);
				buf[-pre + 7] = (unsigned char)(len >> 16);
				buf[-pre + 8] = (unsigned char)(len >> 8);
				buf[-pre + 9] = (unsigned char)len;
			}
		}
		break;
	}

do_more_inside_frame:

	/*
	 * Deal with masking if we are in client -> server direction and
	 * the wp demands it
	 */

	if (masked7) {
		if (!wsi->ws->inside_frame)
			if (lws_0405_frame_mask_generate(wsi)) {
				lwsl_err("frame mask generation failed\n");
				return -1;
			}

		/*
		 * in v7, just mask the payload
		 */
		if (dropmask) { /* never set if already inside frame */
			for (n = 4; n < (int)len + 4; n++)
				dropmask[n] = dropmask[n] ^ wsi->ws->mask[
					(wsi->ws->mask_idx++) & 3];

			/* copy the frame nonce into place */
			memcpy(dropmask, wsi->ws->mask, 4);
		}
	}

	if (lwsi_role_h2_ENCAPSULATION(wsi)) {
		struct lws *encap = lws_get_network_wsi(wsi);

		assert(encap != wsi);

		return lws_rops_func_fidx(encap->role_ops,
				   LWS_ROPS_write_role_protocol).
					write_role_protocol(wsi, buf - pre,
							    len + (unsigned int)pre, wp);
	}

	switch ((*wp) & 0x1f) {
	case LWS_WRITE_TEXT:
	case LWS_WRITE_BINARY:
	case LWS_WRITE_CONTINUATION:
		if (!wsi->h2_stream_carries_ws) {

			/*
			 * give any active extensions a chance to munge the
			 * buffer before send.  We pass in a pointer to an
			 * lws_tokens struct prepared with the default buffer
			 * and content length that's in there.  Rather than
			 * rewrite the default buffer, extensions that expect
			 * to grow the buffer can adapt .token to point to their
			 * own per-connection buffer in the extension user
			 * allocation.  By default with no extensions or no
			 * extension callback handling, just the normal input
			 * buffer is used then so it is efficient.
			 *
			 * callback returns 1 in case it wants to spill more
			 * buffers
			 *
			 * This takes care of holding the buffer if send is
			 * incomplete, ie, if wsi->ws->clean_buffer is 0
			 * (meaning an extension meddled with the buffer).  If
			 * wsi->ws->clean_buffer is 1, it will instead return
			 * to the user code how much OF THE USER BUFFER was
			 * consumed.
			 */

			n = lws_issue_raw_ext_access(wsi, buf - pre, len + (unsigned int)pre);
			wsi->ws->inside_frame = 1;
			if (n <= 0)
				return n;

			if (n == (int)len + pre) {
				/* everything in the buffer was handled
				 * (or rebuffered...) */
				wsi->ws->inside_frame = 0;
				return (int)orig_len;
			}

			/*
			 * it is how many bytes of user buffer got sent... may
			 * be < orig_len in which case callback when writable
			 * has already been arranged and user code can call
			 * lws_write() again with the rest later.
			 */

			return n - pre;
		}
		break;
	default:
		break;
	}

send_raw:
	return lws_issue_raw(wsi, (unsigned char *)buf - pre, len + (unsigned int)pre);
}

static int
rops_close_kill_connection_ws(struct lws *wsi, enum lws_close_status reason)
{
	/* deal with ws encapsulation in h2 */
#if defined(LWS_WITH_HTTP2)
	if (wsi->mux_substream && wsi->h2_stream_carries_ws)
		return lws_rops_func_fidx(&role_ops_h2,
				   LWS_ROPS_close_kill_connection).
				close_kill_connection(wsi, reason);

	return 0;
#else
	return 0;
#endif
}

static int
rops_callback_on_writable_ws(struct lws *wsi)
{
#if defined(LWS_WITH_HTTP2)
	if (lwsi_role_h2_ENCAPSULATION(wsi)) {
		/* we know then that it has an h2 parent */
		struct lws *enc = lws_rops_func_fidx(&role_ops_h2,
						     LWS_ROPS_encapsulation_parent).
						     encapsulation_parent(wsi);

		assert(enc);
		if (lws_rops_func_fidx(enc->role_ops,
				       LWS_ROPS_callback_on_writable).
						callback_on_writable(wsi))
			return 1;
	}
#endif
	return 0;
}

static int
rops_init_vhost_ws(struct lws_vhost *vh,
		   const struct lws_context_creation_info *info)
{
#if !defined(LWS_WITHOUT_EXTENSIONS)
#ifdef LWS_WITH_PLUGINS
	struct lws_plugin *plugin;
	int m;

	if (vh->context->plugin_extension_count) {

		m = 0;
		while (info->extensions && info->extensions[m].callback)
			m++;

		/*
		 * give the vhost a unified list of extensions including the
		 * ones that came from plugins
		 */
		vh->ws.extensions = lws_zalloc(sizeof(struct lws_extension) *
				     (unsigned int)(m + vh->context->plugin_extension_count + 1),
				     "extensions");
		if (!vh->ws.extensions)
			return 1;

		memcpy((struct lws_extension *)vh->ws.extensions, info->extensions,
		       sizeof(struct lws_extension) * (unsigned int)m);
		plugin = vh->context->plugin_list;
		while (plugin) {
			const lws_plugin_protocol_t *plpr =
				(const lws_plugin_protocol_t *)plugin->hdr;

			memcpy((struct lws_extension *)&vh->ws.extensions[m],
				plpr->extensions,
			       sizeof(struct lws_extension) *
			       (unsigned int)plpr->count_extensions);
			m += plpr->count_extensions;
			plugin = plugin->list;
		}
	} else
#endif
		vh->ws.extensions = info->extensions;
#endif

	return 0;
}

static int
rops_destroy_vhost_ws(struct lws_vhost *vh)
{
#ifdef LWS_WITH_PLUGINS
#if !defined(LWS_WITHOUT_EXTENSIONS)
	if (vh->context->plugin_extension_count)
		lws_free((void *)vh->ws.extensions);
#endif
#endif

	return 0;
}

#if defined(LWS_WITH_HTTP_PROXY)
static int
ws_destroy_proxy_buf(struct lws_dll2 *d, void *user)
{
	lws_free(d);

	return 0;
}
#endif

static int
rops_destroy_role_ws(struct lws *wsi)
{
#if defined(LWS_WITH_HTTP_PROXY)
	lws_dll2_foreach_safe(&wsi->ws->proxy_owner, NULL, ws_destroy_proxy_buf);
#endif

	lws_free_set_NULL(wsi->ws);

	return 0;
}

static int
rops_issue_keepalive_ws(struct lws *wsi, int isvalid)
{
	uint64_t us;

#if defined(LWS_WITH_HTTP2)
	if (lwsi_role_h2_ENCAPSULATION(wsi)) {
		/* we know then that it has an h2 parent */
		struct lws *enc = lws_rops_func_fidx(&role_ops_h2,
						     LWS_ROPS_encapsulation_parent).
						     encapsulation_parent(wsi);

		assert(enc);
		if (lws_rops_func_fidx(enc->role_ops, LWS_ROPS_issue_keepalive).
						  issue_keepalive(enc, isvalid))
			return 1;
	}
#endif

	if (isvalid)
		_lws_validity_confirmed_role(wsi);
	else {
		us = (uint64_t)lws_now_usecs();
		memcpy(&wsi->ws->ping_payload_buf[LWS_PRE], &us, 8);
		wsi->ws->send_check_ping = 1;
		lws_callback_on_writable(wsi);
	}

	return 0;
}

static const lws_rops_t rops_table_ws[] = {
	/*  1 */ { .init_vhost		    = rops_init_vhost_ws },
	/*  2 */ { .destroy_vhost	    = rops_destroy_vhost_ws },
	/*  3 */ { .service_flag_pending    = rops_service_flag_pending_ws },
	/*  4 */ { .handle_POLLIN	    = rops_handle_POLLIN_ws },
	/*  5 */ { .handle_POLLOUT	    = rops_handle_POLLOUT_ws },
	/*  6 */ { .callback_on_writable    = rops_callback_on_writable_ws },
	/*  7 */ { .write_role_protocol	    = rops_write_role_protocol_ws },
	/*  8 */ { .close_via_role_protocol = rops_close_via_role_protocol_ws },
	/*  9 */ { .close_role		    = rops_close_role_ws },
	/* 10 */ { .close_kill_connection   = rops_close_kill_connection_ws },
	/* 11 */ { .destroy_role	    = rops_destroy_role_ws },
	/* 12 */ { .issue_keepalive	    = rops_issue_keepalive_ws },
};

const struct lws_role_ops role_ops_ws = {
	/* role name */			"ws",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_ws,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x00,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x12,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x34,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x50,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x60,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x70,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x08,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x9a,
	  /* LWS_ROPS_destroy_role */
	  /* LWS_ROPS_adoption_bind */			0xb0,
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x0c,
					},

	/* adoption_cb clnt, srv */	{ LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED,
					  LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED },
	/* rx_cb clnt, srv */		{ LWS_CALLBACK_CLIENT_RECEIVE,
					  LWS_CALLBACK_RECEIVE },
	/* writeable cb clnt, srv */	{ LWS_CALLBACK_CLIENT_WRITEABLE,
					  LWS_CALLBACK_SERVER_WRITEABLE },
	/* close cb clnt, srv */	{ LWS_CALLBACK_CLIENT_CLOSED,
					  LWS_CALLBACK_CLOSED },
	/* protocol_bind cb c, srv */	{ LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL,
					  LWS_CALLBACK_WS_SERVER_BIND_PROTOCOL },
	/* protocol_unbind cb c, srv */	{ LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL,
					  LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL },
	/* file handles */		0
};
