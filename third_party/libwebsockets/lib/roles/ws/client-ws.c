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
 * In-place str to lower case
 */

static void
strtolower(char *s)
{
	while (*s) {
#ifdef LWS_PLAT_OPTEE
		int tolower_optee(int c);
		*s = tolower_optee((int)*s);
#else
		*s = (char)tolower((int)*s);
#endif
		s++;
	}
}

int
lws_create_client_ws_object(const struct lws_client_connect_info *i,
			    struct lws *wsi)
{
	int v = SPEC_LATEST_SUPPORTED;

	/* allocate the ws struct for the wsi */
	wsi->ws = lws_zalloc(sizeof(*wsi->ws), "client ws struct");
	if (!wsi->ws) {
		lwsl_wsi_notice(wsi, "OOM");
		return 1;
	}

	/* -1 means just use latest supported */
	if (i->ietf_version_or_minus_one != -1 &&
	    i->ietf_version_or_minus_one)
		v = i->ietf_version_or_minus_one;

	wsi->ws->ietf_spec_revision = (uint8_t)v;

	return 0;
}

#if defined(LWS_WITH_CLIENT)
int
lws_ws_handshake_client(struct lws *wsi, unsigned char **buf, size_t len)
{
	unsigned char *bufin = *buf;

	if ((lwsi_state(wsi) != LRS_WAITING_PROXY_REPLY) &&
	    (lwsi_state(wsi) != LRS_H1C_ISSUE_HANDSHAKE) &&
	    (lwsi_state(wsi) != LRS_WAITING_SERVER_REPLY) &&
	    !lwsi_role_client(wsi))
		return 0;

	lwsl_wsi_debug(wsi, "hs client feels it has %d in", (int)len);

	while (len) {
		/*
		 * we were accepting input but now we stopped doing so
		 */
		if (lws_is_flowcontrolled(wsi)) {
			lwsl_wsi_debug(wsi, "caching %ld", (long)len);
			/*
			 * Since we cached the remaining available input, we
			 * can say we "consumed" it.
			 *
			 * But what about the case where the available input
			 * came out of the rxflow cache already?  If we are
			 * effectively "putting it back in the cache", we have
			 * to place it at the cache head, not the tail as usual.
			 */
			if (lws_rxflow_cache(wsi, *buf, 0, len) ==
							LWSRXFC_TRIMMED) {
				/*
				 * we dealt with it by trimming the existing
				 * rxflow cache HEAD to account for what we used.
				 *
				 * indicate we didn't use anything to the caller
				 * so he doesn't do any consumed processing
				 */
				lwsl_wsi_info(wsi, "trimming inside rxflow cache");
				*buf = bufin;
			} else
				*buf += len;

			return 0;
		}
#if !defined(LWS_WITHOUT_EXTENSIONS)
		if (wsi->ws->rx_draining_ext) {
			int m;

			lwsl_wsi_info(wsi, "draining ext");
			if (lwsi_role_client(wsi))
				m = lws_ws_client_rx_sm(wsi, 0);
			else
				m = lws_ws_rx_sm(wsi, 0, 0);
			if (m < 0)
				return -1;
			continue;
		}
#endif
		/*
		 * caller will account for buflist usage by studying what
		 * happened to *buf
		 */

		if (lws_ws_client_rx_sm(wsi, *(*buf)++)) {
			lwsl_wsi_info(wsi, "client_rx_sm exited, DROPPING %d",
				      (int)len);
			return -1;
		}
		len--;
	}
	// lwsl_wsi_notice(wsi, "finished with %ld", (long)len);

	return 0;
}
#endif

char *
lws_generate_client_ws_handshake(struct lws *wsi, char *p, const char *conn1)
{
	char buf[128], hash[20], key_b64[40];
	int n;
#if !defined(LWS_WITHOUT_EXTENSIONS)
	const struct lws_extension *ext;
	int ext_count = 0;
#endif

	/*
	 * create the random key
	 */
	if (lws_get_random(wsi->a.context, hash, 16) != 16) {
		lwsl_wsi_err(wsi, "Unable to read from random dev %s",
			 SYSTEM_RANDOM_FILEPATH);
		return NULL;
	}

	/* coverity[tainted_scalar] */
	lws_b64_encode_string(hash, 16, key_b64, sizeof(key_b64));

	p += sprintf(p, "Upgrade: websocket\x0d\x0a"
			"Connection: %sUpgrade\x0d\x0a"
			"Sec-WebSocket-Key: ", conn1);
	strcpy(p, key_b64);
	p += strlen(key_b64);
	p += sprintf(p, "\x0d\x0a");
	if (lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_SENT_PROTOCOLS))
		p += sprintf(p, "Sec-WebSocket-Protocol: %s\x0d\x0a",
		     lws_hdr_simple_ptr(wsi,
				     _WSI_TOKEN_CLIENT_SENT_PROTOCOLS));

	/* tell the server what extensions we could support */

#if !defined(LWS_WITHOUT_EXTENSIONS)
	ext = wsi->a.vhost->ws.extensions;
	while (ext && ext->callback) {

		n = wsi->a.vhost->protocols[0].callback(wsi,
			LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED,
				wsi->user_space, (char *)ext->name, 0);

		/*
		 * zero return from callback means go ahead and allow
		 * the extension, it's what we get if the callback is
		 * unhandled
		 */

		if (n) {
			ext++;
			continue;
		}

		/* apply it */

		if (ext_count)
			*p++ = ',';
		else
			p += sprintf(p, "Sec-WebSocket-Extensions: ");
		p += sprintf(p, "%s", ext->client_offer);
		ext_count++;

		ext++;
	}
	if (ext_count)
		p += sprintf(p, "\x0d\x0a");
#endif

	if (wsi->ws->ietf_spec_revision)
		p += sprintf(p, "Sec-WebSocket-Version: %d\x0d\x0a",
			     wsi->ws->ietf_spec_revision);

	/* prepare the expected server accept response */

	key_b64[39] = '\0'; /* enforce composed length below buf sizeof */
	n = sprintf(buf, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
			  key_b64);

	lws_SHA1((unsigned char *)buf, (unsigned int)n, (unsigned char *)hash);

	lws_b64_encode_string(hash, 20,
		  wsi->http.ah->initial_handshake_hash_base64,
		  sizeof(wsi->http.ah->initial_handshake_hash_base64));

	return p;
}

int
lws_client_ws_upgrade(struct lws *wsi, const char **cce)
{
	struct lws_context *context = wsi->a.context;
	struct lws_tokenize ts;
	int n, len, okay = 0;
	lws_tokenize_elem e;
	char *p, buf[64];
	const char *pc;
#if !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	char *sb = (char *)&pt->serv_buf[0];
	const struct lws_ext_options *opts;
	const struct lws_extension *ext;
	char ext_name[128];
	const char *c, *a;
	int more = 1;
	char ignore;
#endif

	if (wsi->client_mux_substream) {/* !!! client ws-over-h2 not there yet */
		lwsl_wsi_warn(wsi, "client ws-over-h2 upgrade not supported yet");
		*cce = "HS: h2 / ws upgrade unsupported";
		goto bail3;
	}

	if (wsi->http.ah->http_response == 401) {
		lwsl_wsi_warn(wsi, "got bad HTTP response '%d'",
			      wsi->http.ah->http_response);
		*cce = "HS: ws upgrade unauthorized";
		goto bail3;
	}

	if (wsi->http.ah->http_response != 101) {
		lwsl_wsi_warn(wsi, "got bad HTTP response '%d'",
			      wsi->http.ah->http_response);
		*cce = "HS: ws upgrade response not 101";
		goto bail3;
	}

	if (lws_hdr_total_length(wsi, WSI_TOKEN_ACCEPT) == 0) {
		lwsl_wsi_info(wsi, "no ACCEPT");
		*cce = "HS: ACCEPT missing";
		goto bail3;
	}

	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_UPGRADE);
	if (!p) {
		lwsl_wsi_info(wsi, "no UPGRADE");
		*cce = "HS: UPGRADE missing";
		goto bail3;
	}
	strtolower(p);
	if (strcmp(p, "websocket")) {
		lwsl_wsi_warn(wsi, "got bad Upgrade header '%s'", p);
		*cce = "HS: Upgrade to something other than websocket";
		goto bail3;
	}

	/* connection: must have "upgrade" */

	lws_tokenize_init(&ts, buf, LWS_TOKENIZE_F_COMMA_SEP_LIST |
				    LWS_TOKENIZE_F_MINUS_NONTERM);
	n = lws_hdr_copy(wsi, buf, sizeof(buf) - 1, WSI_TOKEN_CONNECTION);
	if (n <= 0) /* won't fit, or absent */
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

		default: /* includes ENDED found by the tokenizer itself */
bad_conn_format:
			lwsl_wsi_info(wsi, "malformed connection '%s'", buf);
			*cce = "HS: UPGRADE malformed";
			goto bail3;
		}
	} while (e > 0);

	pc = lws_hdr_simple_ptr(wsi, _WSI_TOKEN_CLIENT_SENT_PROTOCOLS);
#if defined(_DEBUG)
	if (!pc)
		lwsl_wsi_parser(wsi, "lws_client_int_s_hs: no protocol list");
	else
		lwsl_wsi_parser(wsi, "lws_client_int_s_hs: protocol list '%s'", pc);
#endif

	/*
	 * confirm the protocol the server wants to talk was in the list
	 * of protocols we offered
	 */

	len = lws_hdr_total_length(wsi, WSI_TOKEN_PROTOCOL);
	if (!len) {
		lwsl_wsi_info(wsi, "WSI_TOKEN_PROTOCOL is null");
		/*
		 * no protocol name to work from, if we don't already have one
		 * default to first protocol
		 */

		if (wsi->a.protocol) {
			p = (char *)wsi->a.protocol->name;
			goto identify_protocol;
		}

		/* no choice but to use the default protocol */

		n = 0;
		wsi->a.protocol = &wsi->a.vhost->protocols[0];
		goto check_extensions;
	}

	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_PROTOCOL);
	len = (int)strlen(p);

	while (pc && *pc && !okay) {
		if (!strncmp(pc, p, (unsigned int)len) &&
		    (pc[len] == ',' || pc[len] == '\0')) {
			okay = 1;
			continue;
		}
		while (*pc && *pc++ != ',')
			;
		while (*pc == ' ')
			pc++;
	}

	if (!okay) {
		lwsl_wsi_info(wsi, "got bad protocol %s", p);
		*cce = "HS: PROTOCOL malformed";
		goto bail2;
	}

identify_protocol:

#if defined(LWS_WITH_HTTP_PROXY)
	lws_strncpy(wsi->ws->actual_protocol, p,
		    sizeof(wsi->ws->actual_protocol));
#endif

	/*
	 * identify the selected protocol struct and set it
	 */
	n = 0;
	/* keep client connection pre-bound protocol */
	if (!lwsi_role_client(wsi))
		wsi->a.protocol = NULL;

	while (n < wsi->a.vhost->count_protocols) {
		if (!wsi->a.protocol &&
		    strcmp(p, wsi->a.vhost->protocols[n].name) == 0) {
			wsi->a.protocol = &wsi->a.vhost->protocols[n];
			break;
		}
		n++;
	}

	if (n == wsi->a.vhost->count_protocols) { /* no match */
		/* if server, that's already fatal */
		if (!lwsi_role_client(wsi)) {
			lwsl_wsi_info(wsi, "fail protocol %s", p);
			*cce = "HS: Cannot match protocol";
			goto bail2;
		}

		/* for client, find the index of our pre-bound protocol */

		n = 0;
		while (wsi->a.vhost->protocols[n].callback) {
			if (wsi->a.protocol && strcmp(wsi->a.protocol->name,
				   wsi->a.vhost->protocols[n].name) == 0) {
				wsi->a.protocol = &wsi->a.vhost->protocols[n];
				break;
			}
			n++;
		}

		if (!wsi->a.vhost->protocols[n].callback) {
			if (wsi->a.protocol)
				lwsl_wsi_err(wsi, "Failed to match protocol %s",
						wsi->a.protocol->name);
			else
				lwsl_wsi_err(wsi, "No protocol on client");
			*cce = "ws protocol no match";
			goto bail2;
		}
	}

	lwsl_wsi_debug(wsi, "Selected protocol %s", wsi->a.protocol ?
					     wsi->a.protocol->name : "no pcol");

check_extensions:
	/*
	 * stitch protocol choice into the vh protocol linked list
	 * We always insert ourselves at the start of the list
	 *
	 * X <-> B
	 * X <-> pAn <-> pB
	 */

	lws_same_vh_protocol_insert(wsi, n);

#if !defined(LWS_WITHOUT_EXTENSIONS)
	/* instantiate the accepted extensions */

	if (!lws_hdr_total_length(wsi, WSI_TOKEN_EXTENSIONS)) {
		lwsl_wsi_ext(wsi, "no client extensions allowed by server");
		goto check_accept;
	}

	/*
	 * break down the list of server accepted extensions
	 * and go through matching them or identifying bogons
	 */

	if (lws_hdr_copy(wsi, sb, (int)context->pt_serv_buf_size,
			 WSI_TOKEN_EXTENSIONS) < 0) {
		lwsl_wsi_warn(wsi, "ext list from server failed to copy");
		*cce = "HS: EXT: list too big";
		goto bail2;
	}

	c = sb;
	n = 0;
	ignore = 0;
	a = NULL;
	while (more) {

		if (*c && (*c != ',' && *c != '\t')) {
			if (*c == ';') {
				ignore = 1;
				if (!a)
					a = c + 1;
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

		/* check we actually support it */

		lwsl_wsi_notice(wsi, "checking client ext %s", ext_name);

		n = 0;
		ext = wsi->a.vhost->ws.extensions;
		while (ext && ext->callback) {
			if (strcmp(ext_name, ext->name)) {
				ext++;
				continue;
			}

			n = 1;
			lwsl_wsi_notice(wsi, "instantiating client ext %s", ext_name);

			/* instantiate the extension on this conn */

			wsi->ws->active_extensions[wsi->ws->count_act_ext] = ext;

			/* allow him to construct his ext instance */

			if (ext->callback(lws_get_context(wsi), ext, wsi,
				   LWS_EXT_CB_CLIENT_CONSTRUCT,
				   (void *)&wsi->ws->act_ext_user[
				                        wsi->ws->count_act_ext],
				   (void *)&opts, 0)) {
				lwsl_wsi_info(wsi, " ext %s failed construction",
					  ext_name);
				ext++;
				continue;
			}

			/*
			 * allow the user code to override ext defaults if it
			 * wants to
			 */
			ext_name[0] = '\0';
			if (user_callback_handle_rxflow(wsi->a.protocol->callback,
					wsi, LWS_CALLBACK_WS_EXT_DEFAULTS,
					(char *)ext->name, ext_name,
					sizeof(ext_name))) {
				*cce = "HS: EXT: failed setting defaults";
				goto bail2;
			}

			if (ext_name[0] &&
			    lws_ext_parse_options(ext, wsi,
					          wsi->ws->act_ext_user[
						        wsi->ws->count_act_ext],
					          opts, ext_name,
						  (int)strlen(ext_name))) {
				lwsl_wsi_err(wsi, "unable to parse user defaults '%s'",
					     ext_name);
				*cce = "HS: EXT: failed parsing defaults";
				goto bail2;
			}

			/*
			 * give the extension the server options
			 */
			if (a && lws_ext_parse_options(ext, wsi,
					wsi->ws->act_ext_user[
					                wsi->ws->count_act_ext],
					opts, a, lws_ptr_diff(c, a))) {
				lwsl_wsi_err(wsi, "unable to parse remote def '%s'", a);
				*cce = "HS: EXT: failed parsing options";
				goto bail2;
			}

			if (ext->callback(lws_get_context(wsi), ext, wsi,
					LWS_EXT_CB_OPTION_CONFIRM,
				      wsi->ws->act_ext_user[wsi->ws->count_act_ext],
				      NULL, 0)) {
				lwsl_wsi_err(wsi, "ext %s rejects server options %s",
					     ext->name, a);
				*cce = "HS: EXT: Rejects server options";
				goto bail2;
			}

			wsi->ws->count_act_ext++;

			ext++;
		}

		if (n == 0) {
			lwsl_wsi_warn(wsi, "Unknown ext '%s'!", ext_name);
			*cce = "HS: EXT: unknown ext";
			goto bail2;
		}

		a = NULL;
		n = 0;
	}

check_accept:
#endif

	/*
	 * Confirm his accept token is the one we precomputed
	 */

	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_ACCEPT);
	if (strcmp(p, wsi->http.ah->initial_handshake_hash_base64)) {
		lwsl_wsi_warn(wsi, "lws_client_int_s_hs: accept '%s' wrong vs '%s'", p,
				  wsi->http.ah->initial_handshake_hash_base64);
		*cce = "HS: Accept hash wrong";
		goto bail2;
	}

	/* allocate the per-connection user memory (if any) */
	if (lws_ensure_user_space(wsi)) {
		lwsl_wsi_err(wsi, "Problem allocating wsi user mem");
		*cce = "HS: OOM";
		goto bail2;
	}

	/*
	 * we seem to be good to go, give client last chance to check
	 * headers and OK it
	 */
	if (wsi->a.protocol->callback(wsi,
				    LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH,
				    wsi->user_space, NULL, 0)) {
		*cce = "HS: Rejected by filter cb";
		goto bail2;
	}

	/* clear his proxy connection timeout */
	lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

	/* free up his parsing allocations */
	lws_header_table_detach(wsi, 0);

	lws_role_transition(wsi, LWSIFR_CLIENT, LRS_ESTABLISHED, &role_ops_ws);
	lws_validity_confirmed(wsi);

	wsi->rxflow_change_to = LWS_RXFLOW_ALLOW;

	/*
	 * create the frame buffer for this connection according to the
	 * size mentioned in the protocol definition.  If 0 there, then
	 * use a big default for compatibility
	 */
	n = (int)wsi->a.protocol->rx_buffer_size;
	if (!n)
		n = (int)context->pt_serv_buf_size;
	n += LWS_PRE;
	wsi->ws->rx_ubuf = lws_malloc((unsigned int)n + 4 /* 0x0000ffff zlib */,
				"client frame buffer");
	if (!wsi->ws->rx_ubuf) {
		lwsl_wsi_err(wsi, "OOM allocating rx buffer %d", n);
		*cce = "HS: OOM";
		goto bail2;
	}
	wsi->ws->rx_ubuf_alloc = (unsigned int)n;

	lwsl_wsi_debug(wsi, "handshake OK for protocol %s", wsi->a.protocol->name);

	/* call him back to inform him he is up */

	if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_CLIENT_ESTABLISHED,
				    wsi->user_space, NULL, 0)) {
		*cce = "HS: Rejected at CLIENT_ESTABLISHED";
		goto bail3;
	}

	return 0;

bail3:
	return 3;

bail2:
	return 2;
}
