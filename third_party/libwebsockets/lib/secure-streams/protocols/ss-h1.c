/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
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
 * This is the glue that wires up h1 to Secure Streams.
 */

#include <private-lib-core.h>

#if !defined(LWS_PLAT_FREERTOS) || defined(LWS_ROLE_H2)
#define LWS_WITH_SS_RIDESHARE
#endif

#if defined(LWS_WITH_SS_RIDESHARE)
static int
ss_http_multipart_parser(lws_ss_handle_t *h, void *in, size_t len)
{
	uint8_t *q = (uint8_t *)in;
	int pending_issue = 0, n = 0;


	/* let's stick it in the boundary state machine first */
	while (n < (int)len) {
		if (h->u.http.boundary_seq != h->u.http.boundary_len) {
			if (q[n] == h->u.http.boundary[h->u.http.boundary_seq])
				h->u.http.boundary_seq++;
			else {
				h->u.http.boundary_seq = 0;
				h->u.http.boundary_dashes = 0;
				h->u.http.boundary_post = 0;
			}
			goto around;
		}

		/*
		 * We already matched the boundary string, now we're
		 * looking if there's a -- afterwards
		 */
		if (h->u.http.boundary_dashes < 2) {
			if (q[n] == '-') {
				h->u.http.boundary_dashes++;
				goto around;
			}
			/* there was no final -- ... */
		}

		if (h->u.http.boundary_dashes == 2) {
			/*
			 * It's an EOM boundary: issue pending + multipart EOP
			 */
			lwsl_debug("%s: seen EOP, n %d pi %d\n",
				    __func__, n, pending_issue);
			/*
			 * It's possible we already started the decode before
			 * the end of the last packet.  Then there is no
			 * remainder to send.
			 */
			if (n >= pending_issue + h->u.http.boundary_len +
			    (h->u.http.any ? 2 : 0) + 1) {
				h->info.rx(ss_to_userobj(h),
					   &q[pending_issue],
					   (unsigned int)(n - pending_issue -
					   h->u.http.boundary_len - 1 -
					   (h->u.http.any ? 2 : 0) /* crlf */),
				   (!h->u.http.som ? LWSSS_FLAG_SOM : 0) |
				   LWSSS_FLAG_EOM | LWSSS_FLAG_RELATED_END);
				h->u.http.eom = 1;
			}

			/*
			 * Peer may not END_STREAM us
			 */
			return 0;
			//return -1;
		}

		/* how about --boundaryCRLF */

		if (h->u.http.boundary_post < 2) {
			if ((!h->u.http.boundary_post && q[n] == '\x0d') ||
			    (h->u.http.boundary_post && q[n] == '\x0a')) {
				h->u.http.boundary_post++;
				goto around;
			}
			/* there was no final CRLF ... it's wrong */

			return -1;
		}
		if (h->u.http.boundary_post != 2)
			goto around;

		/*
		 * We have a starting "--boundaryCRLF" or intermediate
		 * "CRLF--boundaryCRLF" boundary
		 */
		lwsl_debug("%s: b_post = 2 (pi %d)\n", __func__, pending_issue);
		h->u.http.boundary_seq = 0;
		h->u.http.boundary_post = 0;

		if (n >= pending_issue && (h->u.http.any || !h->u.http.som)) {
			/* Intermediate... do the EOM */
			lwsl_debug("%s: seen interm EOP n %d pi %d\n", __func__,
				   n, pending_issue);
			/*
			 * It's possible we already started the decode before
			 * the end of the last packet.  Then there is no
			 * remainder to send.
			 */
			if (n >= pending_issue + h->u.http.boundary_len +
			    (h->u.http.any ? 2 : 0)) {
				h->info.rx(ss_to_userobj(h), &q[pending_issue],
					   (unsigned int)(n - pending_issue -
					       h->u.http.boundary_len -
					       (h->u.http.any ? 2 /* crlf */ : 0)),
					   (!h->u.http.som ? LWSSS_FLAG_SOM : 0) |
					   LWSSS_FLAG_EOM);
				h->u.http.eom = 1;
			}
		}

		/* Next message starts after this boundary */

		pending_issue = n;
		if (h->u.http.eom) {
			/* reset only if we have sent eom */
			h->u.http.som = 0;
			h->u.http.eom = 0;
		}

around:
		n++;
	}

	if (pending_issue != n) {
		uint8_t oh = 0;

		/*
		 * handle the first or last "--boundaryCRLF" case which is not captured in the
		 * previous loop, on the Bob downchannel (/directive)
		 *
		 * probably does not cover the case that one boundary term is separated in multipile
		 * one callbacks though never see such case
		 */

		if ((n >= h->u.http.boundary_len) &&
			h->u.http.boundary_seq == h->u.http.boundary_len &&
			h->u.http.boundary_post == 2) {

			oh = 1;
		}

		h->info.rx(ss_to_userobj(h), &q[pending_issue],
				(unsigned int)(oh ?
				(n - pending_issue - h->u.http.boundary_len -
					(h->u.http.any ? 2 : 0)) :
				(n - pending_issue)),
			   (!h->u.http.som ? LWSSS_FLAG_SOM : 0) |
			     (oh && h->u.http.any ? LWSSS_FLAG_EOM : 0));

		if (oh && h->u.http.any)
			h->u.http.eom = 1;

		h->u.http.any = 1;
		h->u.http.som = 1;
	}

	return 0;
}
#endif

/*
 * Returns 0, or the ss state resp maps on to
 */

static int
lws_ss_http_resp_to_state(lws_ss_handle_t *h, int resp)
{
	const lws_ss_http_respmap_t *r = h->policy->u.http.respmap;
	int n = h->policy->u.http.count_respmap;

	while (n--)
		if (resp == r->resp)
			return r->state;
		else
			r++;

	return 0; /* no hit */
}

/*
 * This converts any set metadata items into outgoing http headers
 */

static int
lws_apply_metadata(lws_ss_handle_t *h, struct lws *wsi, uint8_t *buf,
		   uint8_t **pp, uint8_t *end)
{
	lws_ss_metadata_t *polmd = h->policy->metadata;
	int m = 0;

	while (polmd) {

		/* has to have a non-empty header string */

		if (polmd->value__may_own_heap &&
		    ((uint8_t *)polmd->value__may_own_heap)[0] &&
		    h->metadata[m].value__may_own_heap) {
			if (lws_add_http_header_by_name(wsi,
					polmd->value__may_own_heap,
					h->metadata[m].value__may_own_heap,
					(int)h->metadata[m].length, pp, end))
			return -1;

			/*
			 * Check for the case he's setting a non-zero
			 * content-length "via the backdoor" metadata-
			 * driven headers, and set the body_pending()
			 * state if so...
			 */

			if (!strncmp(polmd->value__may_own_heap,
				     "content-length", 14) &&
			    atoi(h->metadata[m].value__may_own_heap))
				lws_client_http_body_pending(wsi, 1);
		}

		m++;
		polmd = polmd->next;
	}

	/*
	 * Content-length on POST / PUT if we have the length information
	 */

	if (h->policy->u.http.method && (
		(!strcmp(h->policy->u.http.method, "POST") ||
	         !strcmp(h->policy->u.http.method, "PUT"))) &&
	    wsi->http.writeable_len) {
		if (!(h->policy->flags &
			LWSSSPOLF_HTTP_NO_CONTENT_LENGTH)) {
			int n = lws_snprintf((char *)buf, 20, "%u",
				(unsigned int)wsi->http.writeable_len);
			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_LENGTH,
					buf, n, pp, end))
				return -1;
		}
		lws_client_http_body_pending(wsi, 1);
	}

	return 0;
}


#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
static int
lws_apply_instant_metadata(lws_ss_handle_t *h, struct lws *wsi, uint8_t *buf,
		   uint8_t **pp, uint8_t *end)
{
	lws_ss_metadata_t *imd = h->instant_metadata;

	while (imd) {
		if (imd->name && imd->value__may_own_heap) {
			lwsl_debug("%s add header %s %s %d\n", __func__,
					           imd->name,
			                           (char *)imd->value__may_own_heap,
						   (int)imd->length);
			if (lws_add_http_header_by_name(wsi,
					(const unsigned char *)imd->name,
					(const unsigned char *)imd->value__may_own_heap,
					(int)imd->length, pp, end))
			return -1;

			/* it's possible user set content-length directly */
			if (!strncmp(imd->name,
				     "content-length", 14) &&
			    atoi(imd->value__may_own_heap))
				lws_client_http_body_pending(wsi, 1);

		}

		imd = imd->next;
	}

	return 0;
}
#endif
/*
 * Check if any metadata headers present in the server headers, and record
 * them into the associated metadata item if so.
 */

static int
lws_extract_metadata(lws_ss_handle_t *h, struct lws *wsi)
{
	lws_ss_metadata_t *polmd = h->policy->metadata, *omd;
	int n, m = 0;

	while (polmd) {

		if (polmd->value_is_http_token != LWS_HTTP_NO_KNOWN_HEADER) {

			/* it's a well-known header token */

			n = lws_hdr_total_length(wsi, polmd->value_is_http_token);
			if (n) {
				const char *cp = lws_hdr_simple_ptr(wsi,
						polmd->value_is_http_token);
				omd = lws_ss_get_handle_metadata(h, polmd->name);
				if (!omd || !cp)
					return 1;

				assert(!strcmp(omd->name, polmd->name));

				/*
				 * it's present on the wsi, we want to
				 * set the related metadata name to it then
				 */

				_lws_ss_alloc_set_metadata(omd, polmd->name, cp,
							   (unsigned int)n);

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
				/*
				 * ...and because we are doing it from parsing
				 * onward rx, we want to mark the metadata as
				 * needing passing to the client
				 */
				omd->pending_onward = 1;
#endif
			}
		}

#if defined(LWS_WITH_CUSTOM_HEADERS)
		else

			/* has to have a non-empty header string */

			if (polmd->value__may_own_heap &&
			    ((uint8_t *)polmd->value__may_own_heap)[0]) {
				char *p;

				/*
				 * Can it be a custom header?
				 */

				n = lws_hdr_custom_length(wsi, (const char *)
						    polmd->value__may_own_heap,
						    polmd->value_length);
				if (n > 0) {

					p = lws_malloc((unsigned int)n + 1, __func__);
					if (!p)
						return 1;

					/* if needed, free any previous value */

					if (polmd->value_on_lws_heap) {
						lws_free(
						    polmd->value__may_own_heap);
						polmd->value_on_lws_heap = 0;
					}

					/*
					 * copy the named custom header value
					 * into the malloc'd buffer
					 */

					if (lws_hdr_custom_copy(wsi, p, n + 1,
						     (const char *)
						     polmd->value__may_own_heap,
						     polmd->value_length) < 0) {
						lws_free(p);

						return 1;
					}

					omd = lws_ss_get_handle_metadata(h,
								   polmd->name);
					if (omd) {

						_lws_ss_set_metadata(omd,
							polmd->name, p, (size_t)n);
						omd->value_on_lws_heap = 1;

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
						omd->pending_onward = 1;
#endif
					}
				}
			}
#endif

		m++;
		polmd = polmd->next;
	}

	return 0;
}

static const uint8_t blob_idx[] = {
	LWS_SYSBLOB_TYPE_AUTH,
	LWS_SYSBLOB_TYPE_DEVICE_SERIAL,
	LWS_SYSBLOB_TYPE_DEVICE_FW_VERSION,
	LWS_SYSBLOB_TYPE_DEVICE_TYPE,
};

int
secstream_h1(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
#if defined(LWS_WITH_SERVER)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
#endif
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	uint8_t buf[LWS_PRE + 1520], *p = &buf[LWS_PRE],
#if defined(LWS_WITH_SERVER)
			*start = p,
#endif
		*end = &buf[sizeof(buf) - 1];
	lws_ss_state_return_t r;
	int f = 0, m, status;
	char conceal_eom = 0;
	lws_usec_t inter;
	size_t buflen;

	switch (reason) {

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		if (!h) {
			lwsl_err("%s: CCE with no ss handle %s\n", __func__, lws_wsi_tag(wsi));
			break;
		}

		lws_ss_assert_extant(wsi->a.context, wsi->tsi, h);

		assert(h->policy);

#if defined(LWS_WITH_CONMON)
		lws_conmon_ss_json(h);
#endif

		lws_metrics_caliper_report_hist(h->cal_txn, wsi);
		lwsl_info("%s: %s CLIENT_CONNECTION_ERROR: %s\n", __func__,
			  h->lc.gutag, in ? (const char *)in : "none");
		if (h->ss_dangling_connected) {
			/* already disconnected, no action for DISCONNECT_ME */
			r = lws_ss_event_helper(h, LWSSSCS_DISCONNECTED);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		} else {
			/* already disconnected, no action for DISCONNECT_ME */
			r = lws_ss_event_helper(h, LWSSSCS_UNREACHABLE);
			if (r) {
				if (h->inside_connect) {
					h->pending_ret = r;
					break;
				}

				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
			}
		}

		h->wsi = NULL;
		r = lws_ss_backoff(h);
		if (r != LWSSSSRET_OK) {
			if (h->inside_connect) {
				h->pending_ret = r;
				break;
			}
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		}
		break;

	case LWS_CALLBACK_CLIENT_HTTP_REDIRECT:

		if (!h)
			return -1;

		if (h->policy->u.http.fail_redirect)
			lws_system_cpd_set(lws_get_context(wsi),
					   LWS_CPD_CAPTIVE_PORTAL);
		/* unless it's explicitly allowed, reject to follow it */
		return !(h->policy->flags & LWSSSPOLF_ALLOW_REDIRECTS);

	case LWS_CALLBACK_CLOSED_HTTP: /* server */
	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		if (!h)
			break;

		lws_sul_cancel(&h->sul_timeout);

		lws_ss_assert_extant(wsi->a.context, wsi->tsi, h);

#if defined(LWS_WITH_CONMON)
		if (wsi->conmon.pcol == LWSCONMON_PCOL_NONE) {
			wsi->conmon.pcol = LWSCONMON_PCOL_HTTP;
			wsi->conmon.protocol_specific.http.response =
					(int)lws_http_client_http_response(wsi);
		}

		lws_conmon_ss_json(h);
#endif

		lws_metrics_caliper_report_hist(h->cal_txn, wsi);
		//lwsl_notice("%s: %s LWS_CALLBACK_CLOSED_CLIENT_HTTP\n",
		//		__func__, wsi->lc.gutag);

		h->wsi = NULL;
		h->hanging_som = 0;
		h->subseq = 0;

#if defined(LWS_WITH_SERVER)
		lws_pt_lock(pt, __func__);
		lws_dll2_remove(&h->cli_list);
		lws_pt_unlock(pt);
#endif

		if (h->policy && !(h->policy->flags & LWSSSPOLF_OPPORTUNISTIC) &&
#if defined(LWS_WITH_SERVER)
		    !(h->info.flags & LWSSSINFLAGS_ACCEPTED) && /* not server */
#endif
		    !h->txn_ok && !wsi->a.context->being_destroyed) {
			r = lws_ss_backoff(h);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
			break;
		} else
			h->seqstate = SSSEQ_IDLE;

		if (h->ss_dangling_connected) {
			/* already disconnected, no action for DISCONNECT_ME */
			r = lws_ss_event_helper(h, LWSSSCS_DISCONNECTED);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		}
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:

		if (!h)
			return -1;

		lws_ss_assert_extant(wsi->a.context, wsi->tsi, h);
		h->wsi = wsi; /* since we accept the wsi is bound to the SS,
			       * ensure the SS feels the same way about the wsi */

#if defined(LWS_WITH_CONMON)
		if (wsi->conmon.pcol == LWSCONMON_PCOL_NONE) {
			wsi->conmon.pcol = LWSCONMON_PCOL_HTTP;
			wsi->conmon.protocol_specific.http.response =
					(int)lws_http_client_http_response(wsi);
		}

		lws_conmon_ss_json(h);
#endif

		status = (int)lws_http_client_http_response(wsi);
		lwsl_info("%s: LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: %d\n", __func__, status);
	//	if (!status)
			/* it's just telling use we connected / joined the nwsi */
	//		break;

#if defined(LWS_WITH_SYS_METRICS)
		if (status) {
			lws_snprintf((char *)buf, 10, "%d", status);
			lws_metrics_tag_ss_add(h, "http_resp", (char *)buf);
		}
#endif

		if (status == HTTP_STATUS_SERVICE_UNAVAILABLE /* 503 */ ||
		    status == 429 /* Too many requests */) {
			/*
			 * We understand this attempt failed, and that we should
			 * conceal this attempt.  If there's a specified
			 * retry-after, we should use that if larger than our
			 * computed backoff
			 */

			inter = 0;
			lws_http_check_retry_after(wsi, &inter);

			r = _lws_ss_backoff(h, inter);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

			return -1; /* end this stream */
		}

		if (h->policy->u.http.resp_expect)
			h->u.http.good_respcode =
					status == h->policy->u.http.resp_expect;
		else
			h->u.http.good_respcode = (status >= 200 && status < 300);
		// lwsl_err("%s: good resp %d %d\n", __func__, status, h->u.http.good_respcode);

		if (lws_extract_metadata(h, wsi)) {
			lwsl_info("%s: rx metadata extract failed\n", __func__);

			return -1;
		}

		if (status) {
			/*
			 * Check and see if it's something from the response
			 * map, if so, generate the requested status.  If we're
			 * the proxy onward connection, metadata has priority
			 * over state updates on the serialization, so the
			 * state callback will see the right metadata.
			 */
			int n = lws_ss_http_resp_to_state(h, status);
			if (n) {
				r = lws_ss_event_helper(h, (lws_ss_constate_t)n);
				if (r != LWSSSSRET_OK)
					return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi,
									&h);
			}
		}

		if (h->u.http.good_respcode)
			lwsl_info("%s: Connected streamtype %s, %d\n", __func__,
				  h->policy->streamtype, status);
		else
			if (h->u.http.good_respcode)
				lwsl_warn("%s: Connected streamtype %s, BAD %d\n",
					  __func__, h->policy->streamtype,
					  status);

		h->hanging_som = 0;

		h->retry = 0;
		h->seqstate = SSSEQ_CONNECTED;
		lws_sul_cancel(&h->sul);

		if (h->prev_ss_state != LWSSSCS_CONNECTED) {
			wsi->client_suppress_CONNECTION_ERROR = 1;
			if (h->prev_ss_state != LWSSSCS_CONNECTED) {
				r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
				if (r != LWSSSSRET_OK)
					return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
			}
		}

		/*
		 * Since it's an http transaction we initiated... this is
		 * proof of connection validity
		 */
		lws_validity_confirmed(wsi);

#if defined(LWS_WITH_SS_RIDESHARE)

		/*
		 * There are two ways we might want to deal with multipart,
		 * one is pass it through raw (although the user code needs
		 * a helping hand for learning the boundary), and the other
		 * is to deframe it and provide basically submessages in the
		 * different parts.
		 */

		if (lws_hdr_copy(wsi, (char *)buf, sizeof(buf),
				 WSI_TOKEN_HTTP_CONTENT_TYPE) > 0 &&
		/* multipart/form-data;
		 * boundary=----WebKitFormBoundarycc7YgAPEIHvgE9Bf */

		    (!strncmp((char *)buf, "multipart/form-data", 19) ||
		     !strncmp((char *)buf, "multipart/related", 17))) {
			struct lws_tokenize ts;
			lws_tokenize_elem e;

			// puts((const char *)buf);

			memset(&ts, 0, sizeof(ts));
			ts.start = (char *)buf;
			ts.len = strlen(ts.start);
			ts.flags = LWS_TOKENIZE_F_RFC7230_DELIMS |
					LWS_TOKENIZE_F_SLASH_NONTERM |
					LWS_TOKENIZE_F_MINUS_NONTERM;

			h->u.http.boundary[0] = '\0';
			do {
				e = lws_tokenize(&ts);
				if (e == LWS_TOKZE_TOKEN_NAME_EQUALS &&
				    !strncmp(ts.token, "boundary", 8) &&
				    ts.token_len == 8) {
					e = lws_tokenize(&ts);
					if (e != LWS_TOKZE_TOKEN)
						goto malformed;
					h->u.http.boundary[0] = '\x0d';
					h->u.http.boundary[1] = '\x0a';
					h->u.http.boundary[2] = '-';
					h->u.http.boundary[3] = '-';
					lws_strnncpy(h->u.http.boundary + 4,
						     ts.token, ts.token_len,
						     sizeof(h->u.http.boundary) - 4);
					h->u.http.boundary_len =
						(uint8_t)(ts.token_len + 4);
					h->u.http.boundary_seq = 2;
					h->u.http.boundary_dashes = 0;
				}
			} while (e > 0);
			lwsl_info("%s: multipart boundary '%s' len %d\n", __func__,
					h->u.http.boundary, h->u.http.boundary_len);

			/* inform the ss that a related message group begins */

			if ((h->policy->flags & LWSSSPOLF_HTTP_MULTIPART_IN) &&
			    h->u.http.boundary[0])
				h->info.rx(ss_to_userobj(h), NULL, 0,
					   LWSSS_FLAG_RELATED_START);

			// lws_header_table_detach(wsi, 0);
		}
		break;
malformed:
		lwsl_notice("%s: malformed multipart header\n", __func__);
		return -1;
#else
		break;
#endif

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		if (!h)
			return -1;
		if (h->writeable_len)
			wsi->http.writeable_len = h->writeable_len;

		{
			uint8_t **p = (uint8_t **)in, *end = (*p) + len,
				*oin = *(uint8_t **)in;

		/*
		 * blob-based headers
		 */

		for (m = 0; m < _LWSSS_HBI_COUNT; m++) {
			lws_system_blob_t *ab;
			int o = 0, n;

			if (!h->policy->u.http.blob_header[m])
				continue;

			/*
			 * To be backward compatible, default is system-wide LWA auth,
			 * and "http_auth_header" is for default LWA auth, current users do not
			 * need any change in their policy.
			 * If user wants different auth/token, need to specify the "use_auth"
			 * and will be handled after metadata headers are applied.
			 */

			if (m == LWSSS_HBI_AUTH &&
			    h->policy->u.http.auth_preamble)
				o = lws_snprintf((char *)buf, sizeof(buf), "%s",
					h->policy->u.http.auth_preamble);

			if (o > (int)sizeof(buf) - 2)
				return -1;

			ab = lws_system_get_blob(wsi->a.context, blob_idx[m], 0);
			if (!ab)
				return -1;

			buflen = sizeof(buf) - (unsigned int)o - 2u;
			n = lws_system_blob_get(ab, buf + o, &buflen, 0);
			if (n < 0)
				return -1;

			buf[(unsigned int)o + buflen] = '\0';
			lwsl_debug("%s: adding blob %d: %s\n", __func__, m, buf);

			if (lws_add_http_header_by_name(wsi,
				 (uint8_t *)h->policy->u.http.blob_header[m],
				 buf, (int)((int)buflen + o), p, end))
				return -1;
		}

		/*
		 * metadata-based headers
		 */

		if (lws_apply_metadata(h, wsi, buf, p, end))
			return -1;

#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
		if (h->policy->flags & LWSSSPOLF_DIRECT_PROTO_STR) {
			if (lws_apply_instant_metadata(h, wsi, buf, p, end))
				return -1;
		}
#endif

#if defined(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
		if (h->policy->auth && h->policy->auth->type &&
				!strcmp(h->policy->auth->type, "sigv4")) {

			if (lws_ss_apply_sigv4(wsi, h, p, end))
				return -1;
		}
#endif


		(void)oin;
		//if (*p != oin)
		//	lwsl_hexdump_notice(oin, lws_ptr_diff_size_t(*p, oin));

		}

		/*
		 * So when proxied, for POST we have to synthesize a CONNECTED
		 * state, so it can request a writeable and deliver the POST
		 * body
		 */
		if ((h->policy->protocol == LWSSSP_H1 ||
		     h->policy->protocol == LWSSSP_H2) &&
		     h->being_serialized && (
				!strcmp(h->policy->u.http.method, "PUT") ||
				!strcmp(h->policy->u.http.method, "POST"))) {

			wsi->client_suppress_CONNECTION_ERROR = 1;
			if (h->prev_ss_state != LWSSSCS_CONNECTED) {
				r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
				if (r)
					return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
			}
		}

		break;

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_HTTP_BODY:
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_debug("%s: RECEIVE_CLIENT_HTTP_READ: read %d\n",
				__func__, (int)len);
		if (!h || !h->info.rx)
			return 0;

#if defined(LWS_WITH_SS_RIDESHARE)
		if ((h->policy->flags & LWSSSPOLF_HTTP_MULTIPART_IN) &&
		    h->u.http.boundary[0])
			return ss_http_multipart_parser(h, in, len);
#endif

		if (!h->subseq) {
			f |= LWSSS_FLAG_SOM;
			h->hanging_som = 1;
			h->subseq = 1;
		}

	//	lwsl_notice("%s: HTTP_READ: client side sent len %d fl 0x%x\n",
	//		    __func__, (int)len, (int)f);

		h->wsi = wsi; /* since we accept the wsi is bound to the SS,
			       * ensure the SS feels the same way about the wsi */
		r = h->info.rx(ss_to_userobj(h), (const uint8_t *)in, len, f);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		return 0; /* don't passthru */

	/* uninterpreted http content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char *px = (char *)buf + LWS_PRE; /* guarantees LWS_PRE */
			int lenx = sizeof(buf) - LWS_PRE;

			m = lws_http_client_read(wsi, &px, &lenx);
			if (m < 0)
				return m;
		}
		lws_set_timeout(wsi, 99, 30);

		return 0; /* don't passthru */

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		// lwsl_debug("%s: LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n", __func__);

		if (!h)
			return -1;

		if (h->hanging_som) {
			h->info.rx(ss_to_userobj(h), NULL, 0, LWSSS_FLAG_EOM);
			h->hanging_som = 0;
			h->subseq = 0;
		}

		wsi->http.writeable_len = h->writeable_len = 0;
		lws_sul_cancel(&h->sul_timeout);

		h->txn_ok = 1;

#if defined(LWS_WITH_SYS_METRICS)
		lws_metrics_tag_ss_add(h, "result",
				       h->u.http.good_respcode ?
				       "SS_ACK_REMOTE" : "SS_NACK_REMOTE");
#endif

		r = lws_ss_event_helper(h, h->u.http.good_respcode ?
						LWSSSCS_QOS_ACK_REMOTE :
						LWSSSCS_QOS_NACK_REMOTE);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
	case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:

		if (!h || !h->info.tx) {
			lwsl_notice("%s: no handle / tx\n", __func__);
			return 0;
		}

#if defined(LWS_WITH_SERVER)
		if (h->txn_resp_pending) {
			/*
			 * If we're going to start sending something, we need to
			 * to take care of the http response header for it first
			 */
			h->txn_resp_pending = 0;

			if (lws_add_http_common_headers(wsi,
					(unsigned int)(h->txn_resp_set ?
						(h->txn_resp ? h->txn_resp : 200) :
						HTTP_STATUS_NOT_FOUND),
					NULL, h->wsi->http.writeable_len,
					&p, end))
				return 1;

			/*
			 * metadata-based headers
			 */

			if (lws_apply_metadata(h, wsi, buf, &p, end))
				return -1;

			if (lws_finalize_write_http_header(wsi, start, &p, end))
				return 1;

			/* write the body separately */
			lws_callback_on_writable(wsi);

			return 0;
		}
#endif

		if (
#if defined(LWS_WITH_SERVER)
		    !(h->info.flags & LWSSSINFLAGS_ACCEPTED) && /* not accepted */
#endif
		    !h->rideshare)

			h->rideshare = h->policy;

#if defined(LWS_WITH_SS_RIDESHARE)
		if (
#if defined(LWS_WITH_SERVER)
		    !(h->info.flags & LWSSSINFLAGS_ACCEPTED) && /* not accepted */
#endif
		    !h->inside_msg && h->rideshare->u.http.multipart_name)
			lws_client_http_multipart(wsi,
				h->rideshare->u.http.multipart_name,
				h->rideshare->u.http.multipart_filename,
				h->rideshare->u.http.multipart_content_type,
				(char **)&p, (char *)end);

		buflen = lws_ptr_diff_size_t(end, p);
		if (h->policy->u.http.multipart_name)
			buflen -= 24; /* allow space for end of multipart */
#else
		buflen = lws_ptr_diff_size_t(end, p);
#endif
		r = h->info.tx(ss_to_userobj(h), h->txord++, p, &buflen, &f);
		if (r == LWSSSSRET_TX_DONT_SEND)
			return 0;
		if (r < 0)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		// lwsl_notice("%s: WRITEABLE: user tx says len %d fl 0x%x\n",
		//	    __func__, (int)buflen, (int)f);

		p += buflen;

		if (f & LWSSS_FLAG_EOM) {
#if defined(LWS_WITH_SERVER)
		    if (!(h->info.flags & LWSSSINFLAGS_ACCEPTED)) {
#endif
			conceal_eom = 1;
			/* end of rideshares */
			if (!h->rideshare->rideshare_streamtype) {
				lws_client_http_body_pending(wsi, 0);
#if defined(LWS_WITH_SS_RIDESHARE)
				if (h->rideshare->u.http.multipart_name)
					lws_client_http_multipart(wsi, NULL, NULL, NULL,
						(char **)&p, (char *)end);
				conceal_eom = 0;
#endif
			} else {
				h->rideshare = lws_ss_policy_lookup(wsi->a.context,
						h->rideshare->rideshare_streamtype);
				lws_callback_on_writable(wsi);
			}
#if defined(LWS_WITH_SERVER)
		    }
#endif

			h->inside_msg = 0;
		} else {
			/* otherwise we can spin with zero length writes */
			if (!f && !lws_ptr_diff(p, buf + LWS_PRE))
				break;
			h->inside_msg = 1;
			lws_callback_on_writable(wsi);
		}

		lwsl_info("%s: lws_write %d %d\n", __func__,
			  lws_ptr_diff(p, buf + LWS_PRE), f);

		if (lws_write(wsi, buf + LWS_PRE, lws_ptr_diff_size_t(p, buf + LWS_PRE),
			 (!conceal_eom && (f & LWSSS_FLAG_EOM)) ?
				    LWS_WRITE_HTTP_FINAL : LWS_WRITE_HTTP) !=
				(int)lws_ptr_diff(p, buf + LWS_PRE)) {
			lwsl_err("%s: write failed\n", __func__);
			return -1;
		}

#if defined(LWS_WITH_SERVER)
		if ((h->info.flags & LWSSSINFLAGS_ACCEPTED) /* server */ &&
		    (f & LWSSS_FLAG_EOM) &&
		     lws_http_transaction_completed(wsi))
			return -1;
#else
		lws_set_timeout(wsi, 0, 0);
#endif
		break;

#if defined(LWS_WITH_SERVER)
	case LWS_CALLBACK_HTTP:

		if (!h)
			return -1;

		lwsl_info("%s: LWS_CALLBACK_HTTP\n", __func__);
		{

			h->txn_resp_set = 0;
			h->txn_resp_pending = 1;
			h->writeable_len = 0;

#if defined(LWS_ROLE_H2)
			m = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_METHOD);
			if (m) {
				if (lws_ss_alloc_set_metadata(h, "method",
						    lws_hdr_simple_ptr(wsi,
						     WSI_TOKEN_HTTP_COLON_METHOD), (unsigned int)m))
					return -1;
				m = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_PATH);
				if (m && lws_ss_alloc_set_metadata(h, "path",
						    lws_hdr_simple_ptr(wsi,
						     WSI_TOKEN_HTTP_COLON_PATH), (unsigned int)m))
					return -1;
			} else
#endif
			{
				m = lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI);
				if (m) {
					if (lws_ss_alloc_set_metadata(h, "path",
							lws_hdr_simple_ptr(wsi,
								WSI_TOKEN_GET_URI), (unsigned int)m))
						return -1;
					if (lws_ss_alloc_set_metadata(h, "method", "GET", 3))
						return -1;
				} else {
					m = lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI);
					if (m) {
						if (lws_ss_alloc_set_metadata(h, "path",
								lws_hdr_simple_ptr(wsi,
									WSI_TOKEN_POST_URI), (unsigned int)m))
							return -1;
						if (lws_ss_alloc_set_metadata(h, "method", "POST", 4))
							return -1;
					}
				}
			}
		}

		if (!h->ss_dangling_connected) {
#if defined(LWS_WITH_SYS_METRICS)
			/*
			 * If any hanging caliper measurement, dump it, and free any tags
			 */
			lws_metrics_caliper_report_hist(h->cal_txn, (struct lws *)NULL);
#endif
			wsi->client_suppress_CONNECTION_ERROR = 1;
			if (h->prev_ss_state != LWSSSCS_CONNECTED) {
				r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
				if (r)
					return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
			}
		}

		r = lws_ss_event_helper(h, LWSSSCS_SERVER_TXN);
		if (r)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r,
								wsi, &h);

		return 0;
#endif

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

const struct lws_protocols protocol_secstream_h1 = {
	"lws-secstream-h1",
	secstream_h1,
	0, 0, 0, NULL, 0
};

/*
 * Munge connect info according to protocol-specific considerations... this
 * usually means interpreting aux in a protocol-specific way and using the
 * pieces at connection setup time, eg, http url pieces.
 *
 * len bytes of buf can be used for things with scope until after the actual
 * connect.
 */

static int
secstream_connect_munge_h1(lws_ss_handle_t *h, char *buf, size_t len,
			   struct lws_client_connect_info *i,
			   union lws_ss_contemp *ct)
{
	const char *pbasis = h->policy->u.http.url;
	size_t used_in, used_out;
	lws_strexp_t exp;

	/* i.path on entry is used to override the policy urlpath if not "" */

	if (i->path[0])
		pbasis = i->path;

	if (!pbasis)
		return 0;

	/* uncomment to force h1 */
	// i->alpn = "http/1.1";

#if defined(LWS_WITH_SS_RIDESHARE)
	if (h->policy->flags & LWSSSPOLF_HTTP_MULTIPART)
		i->ssl_connection |= LCCSCF_HTTP_MULTIPART_MIME;

	if (h->policy->flags & LWSSSPOLF_HTTP_X_WWW_FORM_URLENCODED)
		i->ssl_connection |= LCCSCF_HTTP_X_WWW_FORM_URLENCODED;
#endif

	if (h->policy->flags & LWSSSPOLF_HTTP_CACHE_COOKIES)
		i->ssl_connection |= LCCSCF_CACHE_COOKIES;

	/* protocol aux is the path part */

	i->path = buf;

	/* skip the unnessary '/' */
	if (*pbasis == '/')
		pbasis = pbasis + 1;

	buf[0] = '/';

	lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata, buf + 1, len - 1);

	if (lws_strexp_expand(&exp, pbasis, strlen(pbasis),
			      &used_in, &used_out) != LSTRX_DONE)
		return 1;

	return 0;
}


const struct ss_pcols ss_pcol_h1 = {
	"h1",
	"http/1.1",
	&protocol_secstream_h1,
	secstream_connect_munge_h1,
	NULL, NULL
};
