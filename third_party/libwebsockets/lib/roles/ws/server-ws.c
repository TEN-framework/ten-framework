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

#if !defined(LWS_WITHOUT_EXTENSIONS)
static int
lws_extension_server_handshake(struct lws *wsi, char **p, int budget)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	char ext_name[64], *args, *end = (*p) + budget - 1;
	const struct lws_ext_options *opts, *po;
	const struct lws_extension *ext;
	struct lws_ext_option_arg oa;
	int n, m, more = 1;
	int ext_count = 0;
	char ignore;
	char *c;

	/*
	 * Figure out which extensions the client has that we want to
	 * enable on this connection, and give him back the list
	 */
	if (!lws_hdr_total_length(wsi, WSI_TOKEN_EXTENSIONS))
		return 0;

	/*
	 * break down the list of client extensions
	 * and go through them
	 */

	if (lws_hdr_copy(wsi, (char *)pt->serv_buf, (int)context->pt_serv_buf_size,
			 WSI_TOKEN_EXTENSIONS) < 0)
		return 1;

	c = (char *)pt->serv_buf;
	lwsl_parser("WSI_TOKEN_EXTENSIONS = '%s'\n", c);
	wsi->ws->count_act_ext = 0;
	ignore = 0;
	n = 0;
	args = NULL;

	/*
	 * We may get a simple request
	 *
	 * Sec-WebSocket-Extensions: permessage-deflate
	 *
	 * or an elaborated one with requested options
	 *
	 * Sec-WebSocket-Extensions: permessage-deflate; \
	 *			     server_no_context_takeover; \
	 *			     client_no_context_takeover
	 */

	while (more) {

		if (c >= (char *)pt->serv_buf + 255)
			return -1;

		if (*c && (*c != ',' && *c != '\t')) {
			if (*c == ';') {
				ignore = 1;
				if (!args)
					args = c + 1;
			}
			if (ignore || *c == ' ') {
				c++;
				continue;
			}
			ext_name[n] = *c++;
			if (n < (int)sizeof(ext_name) - 1)
				n++;
			continue;
		}
		ext_name[n] = '\0';

		ignore = 0;
		if (!*c)
			more = 0;
		else {
			c++;
			if (!n)
				continue;
		}

		while (args && *args == ' ')
			args++;

		/* check a client's extension against our support */

		ext = wsi->a.vhost->ws.extensions;

		while (ext && ext->callback) {

			if (strcmp(ext_name, ext->name)) {
				ext++;
				continue;
			}

			/*
			 * oh, we do support this one he asked for... but let's
			 * confirm he only gave it once
			 */
			for (m = 0; m < wsi->ws->count_act_ext; m++)
				if (wsi->ws->active_extensions[m] == ext) {
					lwsl_info("ext mentioned twice\n");
					return 1; /* shenanigans */
				}

			/*
			 * ask user code if it's OK to apply it on this
			 * particular connection + protocol
			 */
			m = (wsi->a.protocol->callback)(wsi,
				LWS_CALLBACK_CONFIRM_EXTENSION_OKAY,
				wsi->user_space, ext_name, 0);

			/*
			 * zero return from callback means go ahead and allow
			 * the extension, it's what we get if the callback is
			 * unhandled
			 */
			if (m) {
				ext++;
				continue;
			}

			/* apply it */

			ext_count++;

			/* instantiate the extension on this conn */

			wsi->ws->active_extensions[wsi->ws->count_act_ext] = ext;

			/* allow him to construct his context */

			if (ext->callback(lws_get_context(wsi), ext, wsi,
					  LWS_EXT_CB_CONSTRUCT,
					  (void *)&wsi->ws->act_ext_user[
					                wsi->ws->count_act_ext],
					  (void *)&opts, 0)) {
				lwsl_info("ext %s failed construction\n",
					    ext_name);
				ext_count--;
				ext++;

				continue;
			}

			if (ext_count > 1)
				*(*p)++ = ',';
			else
				LWS_CPYAPP(*p,
					  "\x0d\x0aSec-WebSocket-Extensions: ");
			*p += lws_snprintf(*p, lws_ptr_diff_size_t(end, *p), "%s", ext_name);

			/*
			 * The client may send a bunch of different option
			 * sets for the same extension, we are supposed to
			 * pick one we like the look of.  The option sets are
			 * separated by comma.
			 *
			 * Actually we just either accept the first one or
			 * nothing.
			 *
			 * Go through the options trying to apply the
			 * recognized ones
			 */

			lwsl_info("ext args %s\n", args);

			while (args && *args && *args != ',') {
				while (*args == ' ')
					args++;
				po = opts;
				while (po->name) {
					/* only support arg-less options... */
					if (po->type != EXTARG_NONE ||
					    strncmp(args, po->name,
						    strlen(po->name))) {
						po++;
						continue;
					}
					oa.option_name = NULL;
					oa.option_index = (int)(po - opts);
					oa.start = NULL;
					oa.len = 0;
					lwsl_info("setting '%s'\n", po->name);
					if (!ext->callback(lws_get_context(wsi),
							   ext, wsi,
						LWS_EXT_CB_OPTION_SET,
						wsi->ws->act_ext_user[
							wsi->ws->count_act_ext],
							  &oa, lws_ptr_diff_size_t(end, *p))) {

						*p += lws_snprintf(*p,
								   lws_ptr_diff_size_t(end, *p),
							      "; %s", po->name);
						lwsl_debug("adding option %s\n",
							   po->name);
					}
					po++;
				}
				while (*args && *args != ',' && *args != ';')
					args++;

				if (*args == ';')
					args++;
			}

			wsi->ws->count_act_ext++;
			lwsl_parser("cnt_act_ext <- %d\n",
				    wsi->ws->count_act_ext);

			if (args && *args == ',')
				more = 0;

			ext++;
		}

		n = 0;
		args = NULL;
	}

	return 0;
}
#endif

int
lws_process_ws_upgrade2(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
#if defined(LWS_WITH_HTTP_BASIC_AUTH)
	const struct lws_protocol_vhost_options *pvos = NULL;
	const char *ws_prot_basic_auth = NULL;


	/*
	 * Allow basic auth a look-in now we bound the wsi to the protocol.
	 *
	 * For vhost ws basic auth, it is "basic-auth": "path" as usual but
	 * applied to the protocol's entry in the vhost's "ws-protocols":
	 * section, as a pvo.
	 */

	pvos = lws_vhost_protocol_options(wsi->a.vhost, wsi->a.protocol->name);
	if (pvos && pvos->options &&
	    !lws_pvo_get_str((void *)pvos->options, "basic-auth",
			     &ws_prot_basic_auth)) {
		lwsl_info("%s: ws upgrade requires basic auth\n", __func__);
		switch (lws_check_basic_auth(wsi, ws_prot_basic_auth, LWSAUTHM_DEFAULT
						/* no callback based auth here */)) {
		case LCBA_CONTINUE:
			break;
		case LCBA_FAILED_AUTH:
			return lws_unauthorised_basic_auth(wsi);
		case LCBA_END_TRANSACTION:
			lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
			return lws_http_transaction_completed(wsi);
		}
	}
#endif

	/*
	 * We are upgrading to ws, so http/1.1 + h2 and keepalive + pipelined
	 * header considerations about keeping the ah around no longer apply.
	 *
	 * However it's common for the first ws protocol data to have been
	 * coalesced with the browser upgrade request and to already be in the
	 * ah rx buffer.
	 */

	lws_pt_lock(pt, __func__);

	/*
	 * Switch roles if we're upgrading away from http
	 */

	if (!wsi->h2_stream_carries_ws) {
		lws_role_transition(wsi, LWSIFR_SERVER, LRS_ESTABLISHED,
				    &role_ops_ws);

#if defined(LWS_WITH_SECURE_STREAMS) && defined(LWS_WITH_SERVER)

		/*
		 * If we're a SS server object, we have to switch to ss-ws
		 * protocol handler too
		 */
		if (wsi->a.vhost->ss_handle) {
			lwsl_info("%s: %s switching to ws protocol\n",
				  __func__, lws_ss_tag(wsi->a.vhost->ss_handle));
			wsi->a.protocol = &protocol_secstream_ws;

			/*
			 * inform the SS user code that this has done a one-way
			 * upgrade to some other protocol... it will likely
			 * want to treat subsequent payloads differently
			 */

			(void)lws_ss_event_helper(wsi->a.vhost->ss_handle,
						LWSSSCS_SERVER_UPGRADE);
		}
#endif
	}

	lws_pt_unlock(pt);

	/* allocate the ws struct for the wsi */

	wsi->ws = lws_zalloc(sizeof(*wsi->ws), "ws struct");
	if (!wsi->ws) {
		lwsl_notice("OOM\n");
		return 1;
	}

	if (lws_hdr_total_length(wsi, WSI_TOKEN_VERSION))
		wsi->ws->ietf_spec_revision = (uint8_t)
			       atoi(lws_hdr_simple_ptr(wsi, WSI_TOKEN_VERSION));

	/* allocate wsi->user storage */
	if (lws_ensure_user_space(wsi)) {
		lwsl_notice("problem with user space\n");
		return 1;
	}

	/*
	 * Give the user code a chance to study the request and
	 * have the opportunity to deny it
	 */
	if ((wsi->a.protocol->callback)(wsi,
			LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION,
			wsi->user_space,
		      lws_hdr_simple_ptr(wsi, WSI_TOKEN_PROTOCOL), 0)) {
		lwsl_warn("User code denied connection\n");
		return 1;
	}

	/*
	 * Perform the handshake according to the protocol version the
	 * client announced
	 */

	switch (wsi->ws->ietf_spec_revision) {
	default:
		lwsl_notice("Unknown client spec version %d\n",
			  wsi->ws->ietf_spec_revision);
		wsi->ws->ietf_spec_revision = 13;
		//return 1;
		/* fallthru */
	case 13:
#if defined(LWS_WITH_HTTP2)
		if (wsi->h2_stream_carries_ws) {
			if (lws_h2_ws_handshake(wsi)) {
				lwsl_notice("h2 ws handshake failed\n");
				return 1;
			}
			lws_role_transition(wsi,
					    LWSIFR_SERVER | LWSIFR_P_ENCAP_H2,
					    LRS_ESTABLISHED, &role_ops_ws);

			/*
			 * There should be no validity checking since we
			 * are encapsulated in something else with its own
			 * validity checking
			 */

			lws_sul_cancel(&wsi->sul_validity);
		} else
#endif
		{
			lwsl_parser("lws_parse calling handshake_04\n");
			if (handshake_0405(wsi->a.context, wsi)) {
				lwsl_notice("hs0405 has failed the connection\n");
				return 1;
			}
		}
		break;
	}

	if (lws_server_init_wsi_for_ws(wsi)) {
		lwsl_notice("%s: user ESTABLISHED failed connection\n", __func__);
		return 1;
	}
	lwsl_parser("accepted v%02d connection\n", wsi->ws->ietf_spec_revision);

#if defined(LWS_WITH_ACCESS_LOG)
	{
		char *uptr = "unknown method", combo[128], dotstar[64];
		int l = 14, meth = lws_http_get_uri_and_method(wsi, &uptr, &l);

		if (wsi->h2_stream_carries_ws)
			wsi->http.request_version = HTTP_VERSION_2;

		wsi->http.access_log.response = 101;

		lws_strnncpy(dotstar, uptr, l, sizeof(dotstar));
		l = lws_snprintf(combo, sizeof(combo), "%s (%s)", dotstar,
				 wsi->a.protocol->name);

		if (meth < 0)
			meth = 0;
		lws_prepare_access_log_info(wsi, combo, l, meth);
		lws_access_log(wsi);
	}
#endif

	lwsl_info("%s: %s: dropping ah on ws upgrade\n", __func__, lws_wsi_tag(wsi));
	lws_header_table_detach(wsi, 1);

	return 0;
}

int
lws_process_ws_upgrade(struct lws *wsi)
{
	const struct lws_protocols *pcol = NULL;
	char buf[128], name[64];
	struct lws_tokenize ts;
	lws_tokenize_elem e;
	int n;

	if (!wsi->a.protocol)
		lwsl_err("NULL protocol at lws_read\n");

	/*
	 * It's either websocket or h2->websocket
	 *
	 * If we are on h1, confirm we got the required "connection: upgrade"
	 * header.  h2 / ws-over-h2 does not have this.
	 */

#if defined(LWS_WITH_HTTP2)
	if (!wsi->mux_substream) {
#endif

		lws_tokenize_init(&ts, buf, LWS_TOKENIZE_F_COMMA_SEP_LIST |
					    LWS_TOKENIZE_F_DOT_NONTERM |
					    LWS_TOKENIZE_F_RFC7230_DELIMS |
					    LWS_TOKENIZE_F_MINUS_NONTERM);
		n = lws_hdr_copy(wsi, buf, sizeof(buf) - 1, WSI_TOKEN_CONNECTION);
		if (n <= 0)
			goto bad_conn_format;
		ts.len = (unsigned int)n;

		do {
			e = lws_tokenize(&ts);
			switch (e) {
			case LWS_TOKZE_TOKEN:
				if (!strncasecmp(ts.token, "upgrade", ts.token_len))
					e = LWS_TOKZE_ENDED;
				break;

			case LWS_TOKZE_DELIMITER:
				break;

			default: /* includes ENDED */
	bad_conn_format:
				lwsl_err("%s: malformed or absent conn hdr\n",
					 __func__);

				return 1;
			}
		} while (e > 0);

#if defined(LWS_WITH_HTTP2)
	}
#endif

#if defined(LWS_WITH_HTTP_PROXY)
	{
		const struct lws_http_mount *hit;
		int uri_len = 0, meth;
		char *uri_ptr;

		meth = lws_http_get_uri_and_method(wsi, &uri_ptr, &uri_len);
		hit = lws_find_mount(wsi, uri_ptr, uri_len);

		if (hit && (meth == LWSHUMETH_GET ||
			    meth == LWSHUMETH_CONNECT ||
			    meth == LWSHUMETH_COLON_PATH) &&
		    (hit->origin_protocol == LWSMPRO_HTTPS ||
		     hit->origin_protocol == LWSMPRO_HTTP))
			/*
			 * We are an h1 ws upgrade on a urlpath that corresponds
			 * to a proxying mount.  Don't try to deal with it
			 * locally, eg, we won't even have the right protocol
			 * handler since we're not the guy handling it, just a
			 * conduit.
			 *
			 * Instead open the related ongoing h1 connection
			 * according to the mount configuration and proxy
			 * whatever that has to say from now on.
			 */
			return lws_http_proxy_start(wsi, hit, uri_ptr, 1);
	}
#endif

	/*
	 * Select the first protocol we support from the list
	 * the client sent us.
	 */

	lws_tokenize_init(&ts, buf, LWS_TOKENIZE_F_COMMA_SEP_LIST |
				    LWS_TOKENIZE_F_MINUS_NONTERM |
				    LWS_TOKENIZE_F_DOT_NONTERM |
				    LWS_TOKENIZE_F_RFC7230_DELIMS);
	n = lws_hdr_copy(wsi, buf, sizeof(buf) - 1, WSI_TOKEN_PROTOCOL);
	if (n < 0) {
		lwsl_err("%s: protocol list too long\n", __func__);
		return 1;
	}
	ts.len = (unsigned int)n;
	if (!ts.len) {
		int n = wsi->a.vhost->default_protocol_index;
		/*
		 * Some clients only have one protocol and do not send the
		 * protocol list header... allow it and match to the vhost's
		 * default protocol (which itself defaults to zero).
		 *
		 * Setting the vhost default protocol index to -1 or anything
		 * more than the actual number of protocols on the vhost causes
		 * these "no protocol" ws connections to be rejected.
		 */

		if (n >= wsi->a.vhost->count_protocols) {
			lwsl_notice("%s: rejecting ws upg with no protocol\n",
				    __func__);

			return 1;
		}

		lwsl_info("%s: defaulting to prot handler %d\n", __func__, n);

		lws_bind_protocol(wsi, &wsi->a.vhost->protocols[n],
				  "ws upgrade default pcol");

		goto alloc_ws;
	}

#if defined(LWS_WITH_SECURE_STREAMS) && defined(LWS_WITH_SERVER)
	if (wsi->a.vhost->ss_handle) {
		lws_ss_handle_t *sssh = wsi->a.vhost->ss_handle;

		/*
		 * At the moment, once we see it's a ss ws server, whatever
		 * he asked for we bind him to the ss-ws protocol handler.
		 *
		 * In the response subprotocol header, we need to name
		 *
		 * sssh->policy->u.http.u.ws.subprotocol
		 *
		 * though...
		 */

		if (sssh->policy->u.http.u.ws.subprotocol) {
			pcol = lws_vhost_name_to_protocol(wsi->a.vhost,
							  "lws-secstream-ws");
			if (pcol) {
				lws_bind_protocol(wsi, pcol, "ss ws upg pcol");

				goto alloc_ws;
			}
		}
	}
#endif

	/* otherwise go through the user-provided protocol list */

	do {
		e = lws_tokenize(&ts);
		switch (e) {
		case LWS_TOKZE_TOKEN:

			if (lws_tokenize_cstr(&ts, name, sizeof(name))) {
				lwsl_err("%s: pcol name too long\n", __func__);

				return 1;
			}
			lwsl_debug("checking %s\n", name);
			pcol = lws_vhost_name_to_protocol(wsi->a.vhost, name);
			if (pcol) {
				/* if we know it, bind to it and stop looking */
				lws_bind_protocol(wsi, pcol, "ws upg pcol");
				e = LWS_TOKZE_ENDED;
			}
			break;

		case LWS_TOKZE_DELIMITER:
		case LWS_TOKZE_ENDED:
			break;

		default:
			lwsl_err("%s: malformatted protocol list", __func__);

			return 1;
		}
	} while (e > 0);

	/* we didn't find a protocol he wanted? */

	if (!pcol) {
		lwsl_notice("No supported protocol \"%s\"\n", buf);

		return 1;
	}

alloc_ws:

	return lws_process_ws_upgrade2(wsi);
}

int
handshake_0405(struct lws_context *context, struct lws *wsi)
{
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_process_html_args args;
	unsigned char hash[20];
	int n, accept_len;
	char *response;
	char *p;

	if (!lws_hdr_total_length(wsi, WSI_TOKEN_HOST) ||
	    !lws_hdr_total_length(wsi, WSI_TOKEN_KEY)) {
		lwsl_info("handshake_04 missing pieces\n");
		/* completed header processing, but missing some bits */
		goto bail;
	}

	if (lws_hdr_total_length(wsi, WSI_TOKEN_KEY) >=
	    MAX_WEBSOCKET_04_KEY_LEN) {
		lwsl_warn("Client key too long %d\n", MAX_WEBSOCKET_04_KEY_LEN);
		goto bail;
	}

	/*
	 * since key length is restricted above (currently 128), cannot
	 * overflow
	 */
	n = sprintf((char *)pt->serv_buf,
		    "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
		    lws_hdr_simple_ptr(wsi, WSI_TOKEN_KEY));

	lws_SHA1(pt->serv_buf, (unsigned int)n, hash);

	accept_len = lws_b64_encode_string((char *)hash, 20,
			(char *)pt->serv_buf, (int)context->pt_serv_buf_size);
	if (accept_len < 0) {
		lwsl_warn("Base64 encoded hash too long\n");
		goto bail;
	}

	/* allocate the per-connection user memory (if any) */
	if (lws_ensure_user_space(wsi))
		goto bail;

	/* create the response packet */

	/* make a buffer big enough for everything */

	response = (char *)pt->serv_buf + MAX_WEBSOCKET_04_KEY_LEN +
		   256 + LWS_PRE;
	p = response;
	LWS_CPYAPP(p, "HTTP/1.1 101 Switching Protocols\x0d\x0a"
		      "Upgrade: WebSocket\x0d\x0a"
		      "Connection: Upgrade\x0d\x0a"
		      "Sec-WebSocket-Accept: ");
	strcpy(p, (char *)pt->serv_buf);
	p += accept_len;

	/* we can only return the protocol header if:
	 *  - one came in, and ... */
	if (lws_hdr_total_length(wsi, WSI_TOKEN_PROTOCOL) &&
	    /*  - it is not an empty string */
	    wsi->a.protocol->name &&
	    wsi->a.protocol->name[0]) {
		const char *prot = wsi->a.protocol->name;

#if defined(LWS_WITH_HTTP_PROXY)
		if (wsi->proxied_ws_parent && wsi->child_list)
			prot = wsi->child_list->ws->actual_protocol;
#endif

#if defined(LWS_WITH_SECURE_STREAMS) && defined(LWS_WITH_SERVER)
		{
			lws_ss_handle_t *sssh = wsi->a.vhost->ss_handle;

			/*
			 * At the moment, once we see it's a ss ws server, whatever
			 * he asked for we bind him to the ss-ws protocol handler.
			 *
			 * In the response subprotocol header, we need to name
			 *
			 * sssh->policy->u.http.u.ws.subprotocol
			 *
			 * though...
			 */

			if (sssh && sssh->policy &&
			    sssh->policy->u.http.u.ws.subprotocol)
				prot = sssh->policy->u.http.u.ws.subprotocol;
		}
#endif

		LWS_CPYAPP(p, "\x0d\x0aSec-WebSocket-Protocol: ");
		p += lws_snprintf(p, 128, "%s", prot);
	}

#if !defined(LWS_WITHOUT_EXTENSIONS)
	/*
	 * Figure out which extensions the client has that we want to
	 * enable on this connection, and give him back the list.
	 *
	 * Give him a limited write bugdet
	 */
	if (lws_extension_server_handshake(wsi, &p, 192))
		goto bail;
#endif
	LWS_CPYAPP(p, "\x0d\x0a");

	args.p = p;
	args.max_len = lws_ptr_diff((char *)pt->serv_buf +
				    context->pt_serv_buf_size, p);
	if (user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
					LWS_CALLBACK_ADD_HEADERS,
					wsi->user_space, &args, 0))
		goto bail;

	p = args.p;

	/* end of response packet */

	LWS_CPYAPP(p, "\x0d\x0a");

	/* okay send the handshake response accepting the connection */

	lwsl_parser("issuing resp pkt %d len\n",
		    lws_ptr_diff(p, response));
#if defined(DEBUG)
	fwrite(response, 1,  p - response, stderr);
#endif
	n = lws_write(wsi, (unsigned char *)response, lws_ptr_diff_size_t(p, response),
		      LWS_WRITE_HTTP_HEADERS);
	if (n != lws_ptr_diff(p, response)) {
		lwsl_info("%s: ERROR writing to socket %d\n", __func__, n);
		goto bail;
	}

	/* alright clean up and set ourselves into established state */

	lwsi_set_state(wsi, LRS_ESTABLISHED);
	wsi->lws_rx_parse_state = LWS_RXPS_NEW;

	{
		const char * uri_ptr =
			lws_hdr_simple_ptr(wsi, WSI_TOKEN_GET_URI);
		int uri_len = lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI);
		const struct lws_http_mount *hit =
			lws_find_mount(wsi, uri_ptr, uri_len);
		if (hit && hit->cgienv &&
		    wsi->a.protocol->callback(wsi, LWS_CALLBACK_HTTP_PMO,
			wsi->user_space, (void *)hit->cgienv, 0))
			return 1;
	}

	return 0;

bail:
	/* caller will free up his parsing allocations */
	return -1;
}



/*
 * Once we reach LWS_RXPS_WS_FRAME_PAYLOAD, we know how much
 * to expect in that state and can deal with it in bulk more efficiently.
 */

static int
lws_ws_frame_rest_is_payload(struct lws *wsi, uint8_t **buf, size_t len)
{
	struct lws_ext_pm_deflate_rx_ebufs pmdrx;
	unsigned int avail = (unsigned int)len;
	uint8_t *buffer = *buf, mask[4];
#if !defined(LWS_WITHOUT_EXTENSIONS)
	unsigned int old_packet_length = (unsigned int)wsi->ws->rx_packet_length;
#endif
	int n = 0;

	/*
	 * With zlib, we can give it as much input as we like.  The pmd
	 * extension will draw it down in chunks (default 1024).
	 *
	 * If we try to restrict how much we give it, because we must go
	 * back to the event loop each time, we will drop the remainder...
	 */

#if !defined(LWS_WITHOUT_EXTENSIONS)
	if (!wsi->ws->count_act_ext)
#endif
	{
		if (wsi->a.protocol->rx_buffer_size)
			avail = (unsigned int)wsi->a.protocol->rx_buffer_size;
		else
			avail = wsi->a.context->pt_serv_buf_size;
	}

	/* do not consume more than we should */
	if (avail > wsi->ws->rx_packet_length)
		avail = (unsigned int)wsi->ws->rx_packet_length;

	/* do not consume more than what is in the buffer */
	if (avail > len)
		avail = (unsigned int)len;

	if (!avail)
		return 0;

	pmdrx.eb_in.token = buffer;
	pmdrx.eb_in.len = (int)avail;
	pmdrx.eb_out.token = buffer;
	pmdrx.eb_out.len = (int)avail;

	if (!wsi->ws->all_zero_nonce) {

		for (n = 0; n < 4; n++)
			mask[n] = wsi->ws->mask[(wsi->ws->mask_idx + n) & 3];

		/* deal with 4-byte chunks using unwrapped loop */
		n = (int)(avail >> 2);
		while (n--) {
			*(buffer) = *(buffer) ^ mask[0];
			buffer++;
			*(buffer) = *(buffer) ^ mask[1];
			buffer++;
			*(buffer) = *(buffer) ^ mask[2];
			buffer++;
			*(buffer) = *(buffer) ^ mask[3];
			buffer++;
		}
		/* and the remaining bytes bytewise */
		for (n = 0; n < (int)(avail & 3); n++) {
			*(buffer) = *(buffer) ^ mask[n];
			buffer++;
		}

		wsi->ws->mask_idx = (wsi->ws->mask_idx + avail) & 3;
	}

	lwsl_info("%s: using %d of raw input (total %d on offer)\n", __func__,
		    avail, (int)len);

	(*buf) += avail;
	len -= avail;
	wsi->ws->rx_packet_length -= avail;

#if !defined(LWS_WITHOUT_EXTENSIONS)
	n = lws_ext_cb_active(wsi, LWS_EXT_CB_PAYLOAD_RX, &pmdrx, 0);
	lwsl_info("%s: ext says %d / ebuf_out.len %d\n", __func__,  n,
			pmdrx.eb_out.len);

	/*
	 * ebuf may be pointing somewhere completely different now,
	 * it's the output
	 */

	if (n < 0) {
		/*
		 * we may rely on this to get RX, just drop connection
		 */
		lwsl_notice("%s: LWS_EXT_CB_PAYLOAD_RX blew out\n", __func__);
		wsi->socket_is_permanently_unusable = 1;

		return -1;
	}

	/*
	 * if we had an rx fragment right at the last compressed byte of the
	 * message, we can get a zero length inflated output, where no prior
	 * rx inflated output marked themselves with FIN, since there was
	 * raw ws payload still to drain at that time.
	 *
	 * Then we need to generate a zero length ws rx that can be understood
	 * as the message completion.
	 */

	if (!pmdrx.eb_out.len &&	      /* zero-length inflation output */
	    n == PMDR_EMPTY_FINAL &&    /* nothing to drain from the inflator */
	    old_packet_length &&	    /* we gave the inflator new input */
	    !wsi->ws->rx_packet_length &&   /* raw ws packet payload all gone */
	    wsi->ws->final &&		    /* the raw ws packet is a FIN guy */
	    wsi->a.protocol->callback &&
	    !wsi->wsistate_pre_close) {

		lwsl_ext("%s: issuing zero length FIN pkt\n", __func__);

		if (user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
						LWS_CALLBACK_RECEIVE,
						wsi->user_space, NULL, 0))
			return -1;

		return (int)avail;
	}

	/*
	 * If doing permessage-deflate, above was the only way to get a zero
	 * length receive.  Otherwise we're more willing.
	 */
	if (wsi->ws->count_act_ext && !pmdrx.eb_out.len)
		return (int)avail;

	if (n == PMDR_HAS_PENDING)
		/* extension had more... main loop will come back */
		lws_add_wsi_to_draining_ext_list(wsi);
	else
		lws_remove_wsi_from_draining_ext_list(wsi);
#endif

	if (pmdrx.eb_out.len &&
	    wsi->ws->check_utf8 && !wsi->ws->defeat_check_utf8) {
		if (lws_check_utf8(&wsi->ws->utf8,
				   pmdrx.eb_out.token,
				   (unsigned int)pmdrx.eb_out.len)) {
			lws_close_reason(wsi, LWS_CLOSE_STATUS_INVALID_PAYLOAD,
					 (uint8_t *)"bad utf8", 8);
			goto utf8_fail;
		}

		/* we are ending partway through utf-8 character? */
		if (!wsi->ws->rx_packet_length && wsi->ws->final &&
		    wsi->ws->utf8 && !n) {
			lwsl_info("FINAL utf8 error\n");
			lws_close_reason(wsi, LWS_CLOSE_STATUS_INVALID_PAYLOAD,
					 (uint8_t *)"partial utf8", 12);

utf8_fail:
			lwsl_info("utf8 error\n");
			lwsl_hexdump_info(pmdrx.eb_out.token, (size_t)pmdrx.eb_out.len);

			return -1;
		}
	}

	if (wsi->a.protocol->callback && !wsi->wsistate_pre_close)
		if (user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
						LWS_CALLBACK_RECEIVE,
						wsi->user_space,
						pmdrx.eb_out.token,
						(unsigned int)pmdrx.eb_out.len))
			return -1;

	wsi->ws->first_fragment = 0;

#if !defined(LWS_WITHOUT_EXTENSIONS)
	lwsl_info("%s: input used %d, output %d, rem len %d, rx_draining_ext %d\n",
		  __func__, avail, pmdrx.eb_out.len, (int)len,
		  wsi->ws->rx_draining_ext);
#endif

	return (int)avail; /* how much we used from the input */
}


int
lws_parse_ws(struct lws *wsi, unsigned char **buf, size_t len)
{
	unsigned char *bufin = *buf;
	int m, bulk = 0;

	lwsl_debug("%s: received %d byte packet\n", __func__, (int)len);

	//lwsl_hexdump_notice(*buf, len);

	/* let the rx protocol state machine have as much as it needs */

	while (len) {
		/*
		 * we were accepting input but now we stopped doing so
		 */
		if (wsi->rxflow_bitmap) {
			lwsl_info("%s: doing rxflow, caching %d\n", __func__,
				(int)len);
			/*
			 * Since we cached the remaining available input, we
			 * can say we "consumed" it.
			 *
			 * But what about the case where the available input
			 * came out of the rxflow cache already?  If we are
			 * effectively "putting it back in the cache", we have
			 * leave it where it is, already pointed to by the head.
			 */
			if (lws_rxflow_cache(wsi, *buf, 0, len) ==
							LWSRXFC_TRIMMED) {
				/*
				 * We dealt with it by trimming the existing
				 * rxflow cache HEAD to account for what we used.
				 *
				 * so he doesn't do any consumed processing
				 */
				lwsl_info("%s: trimming inside rxflow cache\n",
					  __func__);
				*buf = bufin;
			} else
				*buf += len;

			return 1;
		}
#if !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws->rx_draining_ext) {
			lwsl_debug("%s: draining rx ext\n", __func__);
			m = lws_ws_rx_sm(wsi, ALREADY_PROCESSED_IGNORE_CHAR, 0);
			if (m < 0)
				return -1;
			continue;
		}
#endif

		/* consume payload bytes efficiently */
		while (wsi->lws_rx_parse_state == LWS_RXPS_WS_FRAME_PAYLOAD &&
				(wsi->ws->opcode == LWSWSOPC_TEXT_FRAME ||
				 wsi->ws->opcode == LWSWSOPC_BINARY_FRAME ||
				 wsi->ws->opcode == LWSWSOPC_CONTINUATION) &&
		       len) {
			uint8_t *bin = *buf;

			bulk = 1;
			m = lws_ws_frame_rest_is_payload(wsi, buf, len);
			assert((int)lws_ptr_diff(*buf, bin) <= (int)len);
			len -= lws_ptr_diff_size_t(*buf, bin);

			if (!m) {

				break;
			}
			if (m < 0) {
				lwsl_info("%s: rest_is_payload bailed\n",
					  __func__);
				return -1;
			}
		}

		if (!bulk) {
			/* process the byte */
			m = lws_ws_rx_sm(wsi, 0, *(*buf)++);
			len--;
		} else {
			/*
			 * We already handled this byte in bulk, just deal
			 * with the ramifications
			 */
#if !defined(LWS_WITHOUT_EXTENSIONS)
			lwsl_debug("%s: coming out of bulk with len %d, "
				   "wsi->ws->rx_draining_ext %d\n",
				   __func__, (int)len,
				   wsi->ws->rx_draining_ext);
#endif
			m = lws_ws_rx_sm(wsi, ALREADY_PROCESSED_IGNORE_CHAR |
					      ALREADY_PROCESSED_NO_CB, 0);
		}

		if (m < 0) {
			lwsl_info("%s: lws_ws_rx_sm bailed %d\n", __func__,
				  bulk);

			return -1;
		}

		bulk = 0;
	}

	lwsl_debug("%s: exit with %d unused\n", __func__, (int)len);

	return 0;
}
