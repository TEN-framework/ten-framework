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

/* max individual proxied header payload size */
#define MAXHDRVAL 1024

#if defined(LWS_WITH_HTTP_PROXY)
static int
proxy_header(struct lws *wsi, struct lws *par, unsigned char *temp,
	     int temp_len, int index, unsigned char **p, unsigned char *end)
{
	int n = lws_hdr_total_length(par, (enum lws_token_indexes)index);

	if (n < 1) {
		lwsl_wsi_debug(wsi, "no index %d:", index);

		return 0;
	}

	if (lws_hdr_copy(par, (char *)temp, temp_len, (enum lws_token_indexes)index) < 0) {
		lwsl_wsi_notice(wsi, "unable to copy par hdr idx %d (len %d)",
				      index, n);
		return -1;
	}

	lwsl_wsi_debug(wsi, "index %d: %s", index, (char *)temp);

	if (lws_add_http_header_by_token(wsi, (enum lws_token_indexes)index, temp, n, p, end)) {
		lwsl_wsi_notice(wsi, "unable to append par hdr idx %d (len %d)",
				     index, n);
		return -1;
	}

	return 0;
}

static int
stream_close(struct lws *wsi)
{
	char buf[LWS_PRE + 6], *out = buf + LWS_PRE;

	if (wsi->http.did_stream_close)
		return 0;

	wsi->http.did_stream_close = 1;

	if (wsi->mux_substream) {
		if (lws_write(wsi, (unsigned char *)buf + LWS_PRE, 0,
			      LWS_WRITE_HTTP_FINAL) < 0)
			goto bail;

		return 0;
	}

	*out++ = '0';
	*out++ = '\x0d';
	*out++ = '\x0a';
	*out++ = '\x0d';
	*out++ = '\x0a';

	if (lws_write(wsi, (unsigned char *)buf + LWS_PRE, 5,
		      LWS_WRITE_HTTP_FINAL) < 0)
		goto bail;

	return 0;

bail:
	lwsl_wsi_info(wsi, "h2 fin wr failed");

	return -1;
}

#endif

struct lws_proxy_pkt {
	struct lws_dll2 pkt_list;
	size_t len;
	char binary;
	char first;
	char final;

	/* data follows */
};

#if defined(LWS_WITH_HTTP_PROXY) && defined(LWS_ROLE_WS)
int
lws_callback_ws_proxy(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct lws_proxy_pkt *pkt;
	struct lws_dll2 *dll;

	switch (reason) {

	/* h1 ws proxying... child / client / onward */

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		if (!wsi->h1_ws_proxied || !wsi->parent)
			break;

		if (lws_process_ws_upgrade2(wsi->parent))
			return -1;

#if defined(LWS_WITH_HTTP2)
		if (wsi->parent->mux_substream)
			lwsl_wsi_info(wsi, "proxied h2 -> h1 ws established");
#endif
		break;

	case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		return 1;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_wsi_info(wsi, "client closed: parent %s",
				   lws_wsi_tag(wsi->parent));
		if (wsi->parent)
                       lws_set_timeout(wsi->parent, 1, LWS_TO_KILL_ASYNC);
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	{
		unsigned char **p = (unsigned char **)in, *end = (*p) + len,
				    tmp[MAXHDRVAL];

		proxy_header(wsi, wsi->parent, tmp, sizeof(tmp),
			      WSI_TOKEN_HTTP_ACCEPT_LANGUAGE, p, end);

		proxy_header(wsi, wsi->parent, tmp, sizeof(tmp),
			      WSI_TOKEN_HTTP_COOKIE, p, end);

		proxy_header(wsi, wsi->parent, tmp, sizeof(tmp),
			      WSI_TOKEN_HTTP_SET_COOKIE, p, end);
		break;
	}

	case LWS_CALLBACK_CLIENT_RECEIVE:
		wsi->parent->ws->proxy_buffered += len;
		if (wsi->parent->ws->proxy_buffered > 10 * 1024 * 1024) {
			lwsl_wsi_err(wsi, "proxied ws connection "
					  "excessive buffering: dropping");
			return -1;
		}
		pkt = lws_zalloc(sizeof(*pkt) + LWS_PRE + len, __func__);
		if (!pkt)
			return -1;

		pkt->len = len;
		pkt->first = (char)lws_is_first_fragment(wsi);
		pkt->final = (char)lws_is_final_fragment(wsi);
		pkt->binary = (char)lws_frame_is_binary(wsi);

		memcpy(((uint8_t *)&pkt[1]) + LWS_PRE, in, len);

		lws_dll2_add_tail(&pkt->pkt_list, &wsi->parent->ws->proxy_owner);
		lws_callback_on_writable(wsi->parent);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		dll = lws_dll2_get_head(&wsi->ws->proxy_owner);
		if (!dll)
			break;

		pkt = (struct lws_proxy_pkt *)dll;
		if (lws_write(wsi, ((unsigned char *)&pkt[1]) +
			      LWS_PRE, pkt->len, (enum lws_write_protocol)lws_write_ws_flags(
				pkt->binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
					pkt->first, pkt->final)) < 0)
			return -1;

		lws_dll2_remove(dll);
		lws_free(pkt);

		if (lws_dll2_get_head(&wsi->ws->proxy_owner))
			lws_callback_on_writable(wsi);
		break;

	/* h1 ws proxying... parent / server / incoming */

	case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
		return 1;

	case LWS_CALLBACK_CLOSED:
		lwsl_wsi_info(wsi, "closed");
		return -1;

	case LWS_CALLBACK_RECEIVE:
		pkt = lws_zalloc(sizeof(*pkt) + LWS_PRE + len, __func__);
		if (!pkt)
			return -1;

		pkt->len = len;
		pkt->first = (char)lws_is_first_fragment(wsi);
		pkt->final = (char)lws_is_final_fragment(wsi);
		pkt->binary = (char)lws_frame_is_binary(wsi);

		memcpy(((uint8_t *)&pkt[1]) + LWS_PRE, in, len);

		lws_dll2_add_tail(&pkt->pkt_list, &wsi->child_list->ws->proxy_owner);
		lws_callback_on_writable(wsi->child_list);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		dll = lws_dll2_get_head(&wsi->ws->proxy_owner);
		if (!dll)
			break;

		pkt = (struct lws_proxy_pkt *)dll;
		if (lws_write(wsi, ((unsigned char *)&pkt[1]) +
			      LWS_PRE, pkt->len, (enum lws_write_protocol)lws_write_ws_flags(
				pkt->binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
					pkt->first, pkt->final)) < 0)
			return -1;

		wsi->ws->proxy_buffered -= pkt->len;

		lws_dll2_remove(dll);
		lws_free(pkt);

		if (lws_dll2_get_head(&wsi->ws->proxy_owner))
			lws_callback_on_writable(wsi);
		break;

	default:
		return 0;
	}

	return 0;
}

const struct lws_protocols lws_ws_proxy = {
		"lws-ws-proxy",
		lws_callback_ws_proxy,
		0,
		8192,
		8192, NULL, 0
};

#endif


int
lws_callback_http_dummy(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct lws_ssl_info *si;
#ifdef LWS_WITH_CGI
	struct lws_cgi_args *args;
#endif
#if defined(LWS_WITH_CGI) || defined(LWS_WITH_HTTP_PROXY)
	char buf[LWS_PRE + 32 + 8192];
	int n;
#endif
#if defined(LWS_WITH_HTTP_PROXY)
	unsigned char **p, *end;
	struct lws *parent;
#endif

	switch (reason) {
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	case LWS_CALLBACK_HTTP:
#if defined(LWS_WITH_SERVER)
		if (lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL))
			return -1;

		if (lws_http_transaction_completed(wsi))
#endif
			return -1;
		break;
#if defined(LWS_WITH_SERVER)
	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
#if defined(LWS_WITH_HTTP_PROXY)
		if (wsi->child_list) {
			lwsl_wsi_info(wsi, "HTTP_BODY_COMPLETION: %d",
					   (int)len);
			lws_callback_on_writable(wsi->child_list);
			break;
		}
#endif
		if (lws_return_http_status(wsi, 200, NULL))
			return -1;
		break;

		/* fallthru */
	case LWS_CALLBACK_HTTP_FILE_COMPLETION:
		if (lws_http_transaction_completed(wsi))
			return -1;
		break;
#endif

#if defined(LWS_WITH_HTTP_PROXY)
	case LWS_CALLBACK_HTTP_BODY:
		if (wsi->child_list) {
			lwsl_wsi_info(wsi, "HTTP_BODY: stashing %d", (int)len);
			if (lws_buflist_append_segment(
				     &wsi->http.buflist_post_body, in, len) < 0)
				return -1;
			lws_client_http_body_pending(wsi->child_list, 1);
			lws_callback_on_writable(wsi->child_list);
		}
		break;
#endif

	case LWS_CALLBACK_HTTP_WRITEABLE:
		// lwsl_err("%s: LWS_CALLBACK_HTTP_WRITEABLE\n", __func__);
#ifdef LWS_WITH_CGI
		if (wsi->reason_bf & (LWS_CB_REASON_AUX_BF__CGI_HEADERS |
				      LWS_CB_REASON_AUX_BF__CGI)) {
			n = lws_cgi_write_split_stdout_headers(wsi);
			if (n < 0) {
				lwsl_wsi_debug(wsi, "AUX_BF__CGI forcing close");
				return -1;
			}
			if (!n && wsi->http.cgi && wsi->http.cgi->lsp &&
			    wsi->http.cgi->lsp->stdwsi[LWS_STDOUT])
				lws_rx_flow_control(
					wsi->http.cgi->lsp->stdwsi[LWS_STDOUT], 1);

			if (wsi->reason_bf & LWS_CB_REASON_AUX_BF__CGI_HEADERS)
				wsi->reason_bf &=
					(char)~LWS_CB_REASON_AUX_BF__CGI_HEADERS;
			else
				wsi->reason_bf &= (char)~LWS_CB_REASON_AUX_BF__CGI;

			if (wsi->http.cgi && wsi->http.cgi->cgi_transaction_over) {
				lwsl_wsi_info(wsi, "txn over");
				return -1;
			}

			break;
		}

		if ((wsi->http.cgi && wsi->http.cgi->cgi_transaction_over) ||
		    (wsi->reason_bf & LWS_CB_REASON_AUX_BF__CGI_CHUNK_END)) {
			if (!wsi->mux_substream) {
				memcpy(buf + LWS_PRE, "0\x0d\x0a\x0d\x0a", 5);
				lwsl_wsi_debug(wsi, "wr chunk term and exiting");
				lws_write(wsi, (unsigned char *)buf +
						   LWS_PRE, 5, LWS_WRITE_HTTP);
			} else
				lws_write(wsi, (unsigned char *)buf +
						   LWS_PRE, 0,
						   LWS_WRITE_HTTP_FINAL);

			/* always close after sending it */
			if (lws_http_transaction_completed(wsi))
				return -1;
			return 0;
		}
#endif
#if defined(LWS_WITH_HTTP_PROXY)

		if (wsi->reason_bf & LWS_CB_REASON_AUX_BF__PROXY_HEADERS) {

			wsi->reason_bf &=
				     (char)~LWS_CB_REASON_AUX_BF__PROXY_HEADERS;

			n = LWS_WRITE_HTTP_HEADERS;
			if (!wsi->http.prh_content_length)
				n |= LWS_WRITE_H2_STREAM_END;

			lwsl_wsi_debug(wsi, "issuing proxy headers: clen %d",
				    (int)wsi->http.prh_content_length);
			n = lws_write(wsi, wsi->http.pending_return_headers +
					   LWS_PRE,
				      wsi->http.pending_return_headers_len,
				      (enum lws_write_protocol)n);

			lws_free_set_NULL(wsi->http.pending_return_headers);

			if (n < 0) {
				lwsl_wsi_err(wsi, "EST_CLIENT_HTTP: wr failed");

				return -1;
			}

			lws_callback_on_writable(wsi);
			break;
		}

		if (wsi->reason_bf & LWS_CB_REASON_AUX_BF__PROXY) {
			char *px = buf + LWS_PRE;
			int lenx = sizeof(buf) - LWS_PRE - 32;

			/*
			 * our sink is writeable and our source has something
			 * to read.  So read a lump of source material of
			 * suitable size to send or what's available, whichever
			 * is the smaller.
			 */
			wsi->reason_bf &= (char)~LWS_CB_REASON_AUX_BF__PROXY;
			if (!lws_get_child(wsi))
				break;

			/* this causes LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ */
			if (lws_http_client_read(lws_get_child(wsi), &px,
						 &lenx) < 0) {
				lwsl_wsi_info(wsi, "LWS_CB_REASON_AUX_BF__PROXY: "
					   "client closed");

				stream_close(wsi);

				return -1;
			}
			break;
		}

		if (wsi->reason_bf & LWS_CB_REASON_AUX_BF__PROXY_TRANS_END) {
			lwsl_wsi_info(wsi, "PROXY_TRANS_END");

			wsi->reason_bf &= (char)~LWS_CB_REASON_AUX_BF__PROXY_TRANS_END;

			if (stream_close(wsi))
				return -1;

			if (lws_http_transaction_completed(wsi))
				return -1;
		}
#endif
		break;

#if defined(LWS_WITH_HTTP_PROXY)
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		assert(lws_get_parent(wsi));
		if (!lws_get_parent(wsi))
			break;
		lws_get_parent(wsi)->reason_bf |= LWS_CB_REASON_AUX_BF__PROXY;
		lws_callback_on_writable(lws_get_parent(wsi));
		break;

	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
		char *out = buf + LWS_PRE;

		assert(lws_get_parent(wsi));

		if (wsi->http.proxy_parent_chunked) {

			if (len > sizeof(buf) - LWS_PRE - 16) {
				lwsl_wsi_err(wsi, "oversize buf %d %d", (int)len,
						(int)sizeof(buf) - LWS_PRE - 16);
				return -1;
			}

			/*
			 * this only needs dealing with on http/1.1 to allow
			 * pipelining
			 */
			n = lws_snprintf(out, 14, "%X\x0d\x0a", (int)len);
			out += n;
			memcpy(out, in, len);
			out += len;
			*out++ = '\x0d';
			*out++ = '\x0a';

			n = lws_write(lws_get_parent(wsi),
				      (unsigned char *)buf + LWS_PRE,
				      (size_t)(unsigned int)(len + (unsigned int)n + 2), LWS_WRITE_HTTP);
		} else
			n = lws_write(lws_get_parent(wsi), (unsigned char *)in,
				      len, LWS_WRITE_HTTP);
		if (n < 0)
			return -1;
		break; }

	/* h1 http proxying... */

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
		unsigned char *start, *p, *end;

		/*
		 * We want to proxy these headers, but we are being called
		 * at the point the onward client was established, which is
		 * unrelated to the state or writability of our proxy
		 * connection.
		 *
		 * Therefore produce the headers using the onward client ah
		 * while we have it, and stick them on the output buflist to be
		 * written on the proxy connection as soon as convenient.
		 */

		parent = lws_get_parent(wsi);

		if (!parent)
			return 0;

		start = p = (unsigned char *)buf + LWS_PRE;
		end = p + sizeof(buf) - LWS_PRE - MAXHDRVAL;

		if (lws_add_http_header_status(lws_get_parent(wsi),
				lws_http_client_http_response(wsi), &p, end))
			return 1;

		/*
		 * copy these headers from the client connection to the parent
		 */

		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_CONTENT_LENGTH, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_CONTENT_TYPE, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_ETAG, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_ACCEPT_LANGUAGE, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_CONTENT_ENCODING, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_CACHE_CONTROL, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_SET_COOKIE, &p, end);
		proxy_header(parent, wsi, end, MAXHDRVAL,
			     WSI_TOKEN_HTTP_LOCATION, &p, end);

		if (!parent->mux_substream)
			if (lws_add_http_header_by_token(parent,
				WSI_TOKEN_CONNECTION, (unsigned char *)"close",
				5, &p, end))
			return -1;

		/*
		 * We proxy using h1 only atm, and strip any chunking so it
		 * can go back out on h2 just fine.
		 *
		 * However if we are actually going out on h1, we need to add
		 * our own chunking since we still don't know the size.
		 */

		if (!parent->mux_substream &&
		    !lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH)) {
			lwsl_wsi_debug(wsi, "downstream parent chunked");
			if (lws_add_http_header_by_token(parent,
					WSI_TOKEN_HTTP_TRANSFER_ENCODING,
					(unsigned char *)"chunked", 7, &p, end))
				return -1;

			wsi->http.proxy_parent_chunked = 1;
		}

		if (lws_finalize_http_header(parent, &p, end))
			return 1;

		parent->http.prh_content_length = (size_t)-1;
		if (lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH))
			parent->http.prh_content_length = (size_t)atoll(
				lws_hdr_simple_ptr(wsi,
						WSI_TOKEN_HTTP_CONTENT_LENGTH));

		parent->http.pending_return_headers_len = lws_ptr_diff_size_t(p, start);
		parent->http.pending_return_headers =
			lws_malloc(parent->http.pending_return_headers_len +
				    LWS_PRE, "return proxy headers");
		if (!parent->http.pending_return_headers)
			return -1;

		memcpy(parent->http.pending_return_headers + LWS_PRE, start,
		       parent->http.pending_return_headers_len);

		parent->reason_bf |= LWS_CB_REASON_AUX_BF__PROXY_HEADERS;

		lwsl_wsi_debug(wsi, "ESTABLISHED_CLIENT_HTTP: "
			   "prepared %d headers (len %d)",
			   lws_http_client_http_response(wsi),
			   (int)parent->http.prh_content_length);

		/*
		 * so at this point, the onward client connection can bear
		 * traffic.  We might be doing a POST and have pending cached
		 * inbound stuff to send, it can go now.
		 */

		lws_callback_on_writable(parent);

		break; }

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		lwsl_wsi_info(wsi, "COMPLETED_CLIENT_HTTP: (parent %s)",
				   lws_wsi_tag(lws_get_parent(wsi)));
		if (!lws_get_parent(wsi))
			break;
		lws_get_parent(wsi)->reason_bf |=
				LWS_CB_REASON_AUX_BF__PROXY_TRANS_END;
		lws_callback_on_writable(lws_get_parent(wsi));
		break;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		if (!lws_get_parent(wsi))
			break;
	//	lwsl_err("%s: LWS_CALLBACK_CLOSED_CLIENT_HTTP\n", __func__);
               lws_set_timeout(lws_get_parent(wsi),
        		       (enum pending_timeout)LWS_TO_KILL_ASYNC,
                               (int)PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE);
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		parent = lws_get_parent(wsi);
		if (!parent)
			break;

		p = (unsigned char **)in;
		end = (*p) + len;

		/*
		 * copy these headers from the parent request to the client
		 * connection's request
		 */

		proxy_header(wsi, parent, (unsigned char *)buf, sizeof(buf),
				WSI_TOKEN_HTTP_ETAG, p, end);
		proxy_header(wsi, parent, (unsigned char *)buf, sizeof(buf),
				WSI_TOKEN_HTTP_IF_MODIFIED_SINCE, p, end);
		proxy_header(wsi, parent, (unsigned char *)buf, sizeof(buf),
				WSI_TOKEN_HTTP_ACCEPT_LANGUAGE, p, end);
		proxy_header(wsi, parent, (unsigned char *)buf, sizeof(buf),
				WSI_TOKEN_HTTP_ACCEPT_ENCODING, p, end);
		proxy_header(wsi, parent, (unsigned char *)buf, sizeof(buf),
				WSI_TOKEN_HTTP_CACHE_CONTROL, p, end);
		proxy_header(wsi, parent, (unsigned char *)buf, sizeof(buf),
				WSI_TOKEN_HTTP_COOKIE, p, end);

		buf[0] = '\0';
		lws_get_peer_simple(parent, buf, sizeof(buf));
		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_X_FORWARDED_FOR,
				(unsigned char *)buf, (int)strlen(buf), p, end))
			return -1;

		break;
#endif

#ifdef LWS_WITH_CGI
	/* CGI IO events (POLLIN/OUT) appear here, our default policy is:
	 *
	 *  - POST data goes on subprocess stdin
	 *  - subprocess stdout goes on http via writeable callback
	 *  - subprocess stderr goes to the logs
	 */
	case LWS_CALLBACK_CGI:
		args = (struct lws_cgi_args *)in;
		switch (args->ch) { /* which of stdin/out/err ? */
		case LWS_STDIN:
			/* TBD stdin rx flow control */
			break;
		case LWS_STDOUT:
			if (args->stdwsi[LWS_STDOUT])
				/* quench POLLIN on STDOUT until MASTER got writeable */
				lws_rx_flow_control(args->stdwsi[LWS_STDOUT], 0);
			wsi->reason_bf |= LWS_CB_REASON_AUX_BF__CGI;
			/* when writing to MASTER would not block */
			lws_callback_on_writable(wsi);
			break;
		case LWS_STDERR:
			n = lws_get_socket_fd(args->stdwsi[LWS_STDERR]);
			if (n < 0)
				break;
			n = (int)read(n, buf, sizeof(buf) - 2);
			if (n > 0) {
				if (buf[n - 1] != '\n')
					buf[n++] = '\n';
				buf[n] = '\0';
				lwsl_wsi_notice(wsi, "CGI-stderr: %s", buf);
			}
			break;
		}
		break;

	case LWS_CALLBACK_CGI_TERMINATED:
		if (wsi->http.cgi) {
			lwsl_wsi_debug(wsi, "CGI_TERMINATED: %d %" PRIu64,
				wsi->http.cgi->explicitly_chunked,
				(uint64_t)wsi->http.cgi->content_length);
			if (!(wsi->http.cgi->explicitly_chunked && wsi->mux_substream) &&
			    !wsi->http.cgi->content_length) {
				/* send terminating chunk */
				lwsl_wsi_debug(wsi, "LWS_CALLBACK_CGI_TERMINATED: ending");
				wsi->reason_bf |= LWS_CB_REASON_AUX_BF__CGI_CHUNK_END;
				lws_callback_on_writable(wsi);
				lws_set_timeout(wsi, PENDING_TIMEOUT_CGI, 3);
				break;
			}
			if (wsi->mux_substream && !wsi->cgi_stdout_zero_length)
				lws_write(wsi, (unsigned char *)buf + LWS_PRE, 0,
						      LWS_WRITE_HTTP_FINAL);
		}
#if defined(LWS_WITH_SERVER)
		if (lws_http_transaction_completed(wsi))
			return -1;
#endif
		return 0;

	case LWS_CALLBACK_CGI_STDIN_DATA:  /* POST body for stdin */
		args = (struct lws_cgi_args *)in;
		args->data[args->len] = '\0';
		if (!args->stdwsi[LWS_STDIN])
			return -1;
		n = lws_get_socket_fd(args->stdwsi[LWS_STDIN]);
		if (n < 0)
			return -1;

#if defined(LWS_WITH_ZLIB)
		if (wsi->http.cgi->gzip_inflate) {
			/* gzip handling */

			if (!wsi->http.cgi->gzip_init) {
				lwsl_wsi_info(wsi, "inflating gzip");

				memset(&wsi->http.cgi->inflate, 0,
				       sizeof(wsi->http.cgi->inflate));

				if (inflateInit2(&wsi->http.cgi->inflate,
						 16 + 15) != Z_OK) {
					lwsl_wsi_err(wsi, "iniflateInit fail");
					return -1;
				}

				wsi->http.cgi->gzip_init = 1;
			}

			wsi->http.cgi->inflate.next_in = args->data;
			wsi->http.cgi->inflate.avail_in = (unsigned int)args->len;

			do {

				wsi->http.cgi->inflate.next_out =
						wsi->http.cgi->inflate_buf;
				wsi->http.cgi->inflate.avail_out =
					sizeof(wsi->http.cgi->inflate_buf);

				n = inflate(&wsi->http.cgi->inflate,
					    Z_SYNC_FLUSH);

				switch (n) {
				case Z_NEED_DICT:
				case Z_STREAM_ERROR:
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					inflateEnd(&wsi->http.cgi->inflate);
					wsi->http.cgi->gzip_init = 0;
					lwsl_wsi_err(wsi, "zlib err inflate %d", n);
					return -1;
				}

				if (wsi->http.cgi->inflate.avail_out !=
					   sizeof(wsi->http.cgi->inflate_buf)) {
					int written;

					written = (int)write(args->stdwsi[LWS_STDIN]->desc.filefd,
						wsi->http.cgi->inflate_buf,
						sizeof(wsi->http.cgi->inflate_buf) -
						wsi->http.cgi->inflate.avail_out);

					if (written != (int)(
						sizeof(wsi->http.cgi->inflate_buf) -
						wsi->http.cgi->inflate.avail_out)) {
						lwsl_wsi_notice(wsi,
							"CGI_STDIN_DATA: "
							"sent %d only %d went",
							n, args->len);
					}

					if (n == Z_STREAM_END) {
						lwsl_wsi_err(wsi,
							    "gzip inflate end");
						inflateEnd(&wsi->http.cgi->inflate);
						wsi->http.cgi->gzip_init = 0;
						break;
					}

				} else
					break;

				if (wsi->http.cgi->inflate.avail_out)
					break;

			} while (1);

			return args->len;
		}
#endif /* WITH_ZLIB */

		n = (int)write(n, args->data, (unsigned int)args->len);
//		lwsl_hexdump_notice(args->data, args->len);
		if (n < args->len)
			lwsl_wsi_notice(wsi, "CGI_STDIN_DATA: "
				    "sent %d only %d went", n, args->len);

		lwsl_wsi_info(wsi, "proxied %d bytes", n);

		if (wsi->http.cgi->post_in_expected && args->stdwsi[LWS_STDIN] &&
		    args->stdwsi[LWS_STDIN]->desc.filefd > 0) {
			wsi->http.cgi->post_in_expected -= (unsigned int)n;

			if (!wsi->http.cgi->post_in_expected) {
				struct lws *siwsi = args->stdwsi[LWS_STDIN];

				/*
				 * The situation here is that we finished
				 * proxying the incoming body from the net to
				 * the STDIN stdwsi... and we want to close it
				 * so it can understand we are done (necessary
				 * if no content-length)...
				 */

				lwsl_wsi_info(siwsi, "expected POST in end: "
						     "closing stdin fd %d",
						     siwsi->desc.sockfd);

				/*
				 * We don't want the child / parent relationship
				 * to be handled in close, since we want the
				 * rest of the cgi and children to stay up
				 */

				lws_remove_child_from_any_parent(siwsi);
				lws_wsi_close(siwsi, LWS_TO_KILL_ASYNC);
				wsi->http.cgi->lsp->stdwsi[LWS_STDIN] = NULL;
				lws_spawn_stdwsi_closed(wsi->http.cgi->lsp, siwsi);
			}
		}

		return n;
#endif /* WITH_CGI */
#endif /* ROLE_ H1 / H2 */
	case LWS_CALLBACK_SSL_INFO:
		si = in;

		(void)si;
		lwsl_wsi_notice(wsi, "SSL_INFO: where: 0x%x, ret: 0x%x",
				si->where, si->ret);
		break;

#if LWS_MAX_SMP > 1
	case LWS_CALLBACK_GET_THREAD_ID:
#ifdef __PTW32_H
		/* If we use implementation of PThreads for Win that is
		 * distributed by VCPKG */
		return (int)(lws_intptr_t)(pthread_self()).p;
#else
		return (int)(lws_intptr_t)pthread_self();
#endif // __PTW32_H
#endif

	default:
		break;
	}

	return 0;
}
