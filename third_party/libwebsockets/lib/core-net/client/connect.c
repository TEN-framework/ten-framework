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

static const uint8_t hnames[] = {
	_WSI_TOKEN_CLIENT_PEER_ADDRESS,
	_WSI_TOKEN_CLIENT_URI,
	_WSI_TOKEN_CLIENT_HOST,
	_WSI_TOKEN_CLIENT_ORIGIN,
	_WSI_TOKEN_CLIENT_SENT_PROTOCOLS,
	_WSI_TOKEN_CLIENT_METHOD,
	_WSI_TOKEN_CLIENT_IFACE,
	_WSI_TOKEN_CLIENT_ALPN
};

struct lws *
lws_http_client_connect_via_info2(struct lws *wsi)
{
	struct client_info_stash *stash = wsi->stash;
	int n;

	lwsl_wsi_debug(wsi, "stash %p", stash);

	if (!stash)
		return wsi;

	wsi->a.opaque_user_data = wsi->stash->opaque_user_data;

	if (stash->cis[CIS_METHOD] && (!strcmp(stash->cis[CIS_METHOD], "RAW") ||
				      !strcmp(stash->cis[CIS_METHOD], "MQTT")))
		goto no_ah;

	/*
	 * we're not necessarily in a position to action these right away,
	 * stash them... we only need during connect phase so into a temp
	 * allocated stash
	 */
	for (n = 0; n < (int)LWS_ARRAY_SIZE(hnames); n++)
		if (hnames[n] && stash->cis[n] &&
		    lws_hdr_simple_create(wsi, hnames[n], stash->cis[n]))
			goto bail;

#if defined(LWS_WITH_SOCKS5)
	if (!wsi->a.vhost->socks_proxy_port)
		lws_free_set_NULL(wsi->stash);
#endif

no_ah:
	return lws_client_connect_2_dnsreq(wsi);

bail:
#if defined(LWS_WITH_SOCKS5)
	if (!wsi->a.vhost->socks_proxy_port)
		lws_free_set_NULL(wsi->stash);
#endif

	lws_free_set_NULL(wsi->stash);

	return NULL;
}

int
lws_client_stash_create(struct lws *wsi, const char **cisin)
{
	size_t size;
	char *pc;
	int n;

	size = sizeof(*wsi->stash) + 1;

	/*
	 * Let's overallocate the stash object with space for all the args
	 * in one hit.
	 */
	for (n = 0; n < CIS_COUNT; n++)
		if (cisin[n])
			size += strlen(cisin[n]) + 1;

	if (wsi->stash)
		lws_free_set_NULL(wsi->stash);

	wsi->stash = lws_malloc(size, "client stash");
	if (!wsi->stash)
		return 1;

	/* all the pointers default to NULL, but no need to zero the args */
	memset(wsi->stash, 0, sizeof(*wsi->stash));

	pc = (char *)&wsi->stash[1];

	for (n = 0; n < CIS_COUNT; n++)
		if (cisin[n]) {
			size_t mm;
			wsi->stash->cis[n] = pc;
			if (n == CIS_PATH && cisin[n][0] != '/')
				*pc++ = '/';
			mm = strlen(cisin[n]) + 1;
			memcpy(pc, cisin[n], mm);
			pc += mm;
		}

	return 0;
}

struct lws *
lws_client_connect_via_info(const struct lws_client_connect_info *i)
{
	const char *local = i->protocol;
	struct lws *wsi, *safe = NULL;
	const struct lws_protocols *p;
	const char *cisin[CIS_COUNT];
	struct lws_vhost *vh;
	int tsi;

	if (i->context->requested_stop_internal_loops)
		return NULL;

	if (!i->context->protocol_init_done)
		if (lws_protocol_init(i->context))
			return NULL;

	/*
	 * If we have .local_protocol_name, use it to select the local protocol
	 * handler to bind to.  Otherwise use .protocol if http[s].
	 */
	if (i->local_protocol_name)
		local = i->local_protocol_name;

	lws_context_lock(i->context, __func__);
	/*
	 * PHASE 1: if SMP, find out the tsi related to current service thread
	 */

	tsi = lws_pthread_self_to_tsi(i->context);
	assert(tsi >= 0);

	/* PHASE 2: create a bare wsi */

	wsi = __lws_wsi_create_with_role(i->context, tsi, NULL, i->log_cx);
	lws_context_unlock(i->context);
	if (wsi == NULL)
		return NULL;

	vh = i->vhost;
	if (!vh) {
#if defined(LWS_WITH_TLS_JIT_TRUST)
		if (lws_tls_jit_trust_vhost_bind(i->context, i->address, &vh))
#endif
		{
			vh = lws_get_vhost_by_name(i->context, "default");
			if (!vh) {

				vh = i->context->vhost_list;

				if (!vh) { /* coverity */
					lwsl_cx_err(i->context, "no vhost");
					goto bail;
				}
				if (!strcmp(vh->name, "system"))
					vh = vh->vhost_next;
			}
		}
	}

#if defined(LWS_WITH_SECURE_STREAMS)
	/* any of these imply we are a client wsi bound to an SS, which
	 * implies our opaque user ptr is the ss (or sspc if PROXY_LINK) handle
	 */
	wsi->for_ss = !!(i->ssl_connection & (LCCSCF_SECSTREAM_CLIENT | LCCSCF_SECSTREAM_PROXY_LINK | LCCSCF_SECSTREAM_PROXY_ONWARD));
	wsi->client_bound_sspc = !!(i->ssl_connection & LCCSCF_SECSTREAM_PROXY_LINK); /* so wsi close understands need to remove sspc ptr to wsi */
	wsi->client_proxy_onward = !!(i->ssl_connection & LCCSCF_SECSTREAM_PROXY_ONWARD);
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	wsi->fic.name = "wsi";
	if (i->fic.fi_owner.count)
		/*
		 * This moves all the lws_fi_t from i->fi to the vhost fi,
		 * leaving it empty
		 */
		lws_fi_import(&wsi->fic, &i->fic);

	lws_fi_inherit_copy(&wsi->fic, &i->context->fic, "wsi", i->fi_wsi_name);

	if (lws_fi(&wsi->fic, "createfail"))
		goto bail;

#if defined(LWS_WITH_SECURE_STREAMS)
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	if (wsi->client_bound_sspc) {
		lws_sspc_handle_t *fih = (lws_sspc_handle_t *)i->opaque_user_data;
		lws_fi_inherit_copy(&wsi->fic, &fih->fic, "wsi", NULL);
	}
#endif
	if (wsi->for_ss) {
		lws_ss_handle_t *fih = (lws_ss_handle_t *)i->opaque_user_data;
		lws_fi_inherit_copy(&wsi->fic, &fih->fic, "wsi", NULL);
	}
#endif
#endif

	lws_wsi_fault_timedclose(wsi);

	/*
	 * Until we exit, we can report connection failure directly to the
	 * caller without needing to call through to protocol CONNECTION_ERROR.
	 */
	wsi->client_suppress_CONNECTION_ERROR = 1;

	if (i->keep_warm_secs)
		wsi->keep_warm_secs = i->keep_warm_secs;
	else
		wsi->keep_warm_secs = 5;

	wsi->seq = i->seq;
	wsi->flags = i->ssl_connection;

	wsi->c_pri = i->priority;

	if (i->retry_and_idle_policy)
		wsi->retry_policy = i->retry_and_idle_policy;
	else
		wsi->retry_policy = &i->context->default_retry;

	if (i->ssl_connection & LCCSCF_WAKE_SUSPEND__VALIDITY)
		wsi->conn_validity_wakesuspend = 1;

	lws_vhost_bind_wsi(vh, wsi);

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	/* additionally inerit from vhost we bound to */
	lws_fi_inherit_copy(&wsi->fic, &vh->fic, "wsi", i->fi_wsi_name);
#endif

	if (!wsi->a.vhost) {
		lwsl_wsi_err(wsi, "No vhost in the context");

		goto bail;
	}

	/*
	 * PHASE 3: Choose an initial role for the wsi and do role-specific init
	 *
	 * Note the initial role may not reflect the final role, eg,
	 * we may want ws, but first we have to go through h1 to get that
	 */

	if (lws_role_call_client_bind(wsi, i) < 0) {
		lwsl_wsi_err(wsi, "unable to bind to role");

		goto bail;
	}
	lwsl_wsi_info(wsi, "role binding to %s", wsi->role_ops->name);

	/*
	 * PHASE 4: fill up the wsi with stuff from the connect_info as far as
	 * it can go.  It's uncertain because not only is our connection
	 * going to complete asynchronously, we might have bound to h1 and not
	 * even be able to get ahold of an ah immediately.
	 */

	wsi->user_space = NULL;
	wsi->pending_timeout = NO_PENDING_TIMEOUT;
	wsi->position_in_fds_table = LWS_NO_FDS_POS;
	wsi->ocport = wsi->c_port = (uint16_t)(unsigned int)i->port;
	wsi->sys_tls_client_cert = i->sys_tls_client_cert;

#if defined(LWS_ROLE_H2)
	wsi->txc.manual_initial_tx_credit =
			(int32_t)i->manual_initial_tx_credit;
#endif

	wsi->a.protocol = &wsi->a.vhost->protocols[0];
	wsi->client_pipeline = !!(i->ssl_connection & LCCSCF_PIPELINE);
	wsi->client_no_follow_redirect = !!(i->ssl_connection &
					    LCCSCF_HTTP_NO_FOLLOW_REDIRECT);

	/*
	 * PHASE 5: handle external user_space now, generic alloc is done in
	 * role finalization
	 */

	if (i->userdata) {
		wsi->user_space_externally_allocated = 1;
		wsi->user_space = i->userdata;
	}

	if (local) {
		lwsl_wsi_info(wsi, "vh %s protocol binding to %s\n",
				wsi->a.vhost->name, local);
		p = lws_vhost_name_to_protocol(wsi->a.vhost, local);
		if (p)
			lws_bind_protocol(wsi, p, __func__);
		else
			lwsl_wsi_info(wsi, "unknown protocol %s", local);

		lwsl_wsi_info(wsi, "%s: %s %s entry",
			    lws_wsi_tag(wsi), wsi->role_ops->name,
			    wsi->a.protocol ? wsi->a.protocol->name : "none");
	}

	/*
	 * PHASE 5: handle external user_space now, generic alloc is done in
	 * role finalization
	 */

	if (!wsi->user_space && i->userdata) {
		wsi->user_space_externally_allocated = 1;
		wsi->user_space = i->userdata;
	}

#if defined(LWS_WITH_TLS)
	wsi->tls.use_ssl = (unsigned int)i->ssl_connection;
#else
	if (i->ssl_connection & LCCSCF_USE_SSL) {
		lwsl_wsi_err(wsi, "lws not configured for tls");
		goto bail;
	}
#endif

	/*
	 * PHASE 6: stash the things from connect_info that we can't process
	 * right now, eg, if http binding, without an ah.  If h1 and no ah, we
	 * will go on the ah waiting list and process those things later (after
	 * the connect_info and maybe the things pointed to have gone out of
	 * scope)
	 *
	 * However these things are stashed in a generic way at this point,
	 * with no relationship to http or ah
	 */

	cisin[CIS_ADDRESS]	= i->address;
	cisin[CIS_PATH]		= i->path;
	cisin[CIS_HOST]		= i->host;
	cisin[CIS_ORIGIN]	= i->origin;
	cisin[CIS_PROTOCOL]	= i->protocol;
	cisin[CIS_METHOD]	= i->method;
	cisin[CIS_IFACE]	= i->iface;
	cisin[CIS_ALPN]		= i->alpn;

	if (lws_client_stash_create(wsi, cisin))
		goto bail;

#if defined(LWS_WITH_TLS)
	if (i->alpn)
		lws_strncpy(wsi->alpn, i->alpn, sizeof(wsi->alpn));
#endif

	wsi->a.opaque_user_data = wsi->stash->opaque_user_data =
		i->opaque_user_data;

#if defined(LWS_WITH_SECURE_STREAMS)

	if (wsi->for_ss) {
		/* it's related to ss... the options are
		 *
		 * LCCSCF_SECSTREAM_PROXY_LINK  : client SSPC link to proxy
		 * LCCSCF_SECSTREAM_PROXY_ONWARD: proxy's onward connection
		 */
		__lws_lc_tag(i->context, &i->context->lcg[
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
		         i->ssl_connection & LCCSCF_SECSTREAM_PROXY_LINK ? LWSLCG_WSI_SSP_CLIENT :
#if defined(LWS_WITH_SERVER)
		         (i->ssl_connection & LCCSCF_SECSTREAM_PROXY_ONWARD ? LWSLCG_WSI_SSP_ONWARD :
#endif
			  LWSLCG_WSI_CLIENT
#if defined(LWS_WITH_SERVER)
			  )
#endif
		],
#else
				LWSLCG_WSI_CLIENT],
#endif
			&wsi->lc, "%s/%s/%s/(%s)", i->method ? i->method : "WS",
			wsi->role_ops->name, i->address,
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
			wsi->client_bound_sspc ?
				lws_sspc_tag((lws_sspc_handle_t *)i->opaque_user_data) :
#endif
			lws_ss_tag(((lws_ss_handle_t *)i->opaque_user_data)));
	} else
#endif
		__lws_lc_tag(i->context, &i->context->lcg[LWSLCG_WSI_CLIENT], &wsi->lc,
			     "%s/%s/%s/%s", i->method ? i->method : "WS",
			     wsi->role_ops->name ? wsi->role_ops->name : "novh", vh->name, i->address);

	lws_metrics_tag_wsi_add(wsi, "vh", wsi->a.vhost->name);

	/*
	 * at this point user callbacks like
	 * LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER will be interested to
	 * know the parent... eg for proxying we can grab extra headers from
	 * the parent's incoming ah and add them to the child client handshake
	 */

	if (i->parent_wsi) {
		lwsl_wsi_info(wsi, "created as child %s",
			      lws_wsi_tag(i->parent_wsi));
		wsi->parent = i->parent_wsi;
		safe = wsi->sibling_list = i->parent_wsi->child_list;
		i->parent_wsi->child_list = wsi;
	}

	/*
	 * PHASE 7: Do any role-specific finalization processing.  We can still
	 * see important info things via wsi->stash
	 */

	if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_client_bind)) {

		int n = lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_client_bind).
							client_bind(wsi, NULL);

		if (n && i->parent_wsi)
			/* unpick from parent */
			i->parent_wsi->child_list = safe;

		if (n < 0)
			/* we didn't survive, wsi is freed */
			goto bail2;

		if (n)
			/* something else failed, wsi needs freeing */
			goto bail;
	}

	/* let the caller's optional wsi storage have the wsi we created */

	if (i->pwsi)
		*i->pwsi = wsi;

	if (!wsi->a.protocol)
		/* we must have one protocol or another bound by this point */
		goto bail;

	/* PHASE 8: notify protocol with role-specific connected callback */

	/* raw socket per se doesn't want this... raw socket proxy wants it... */

	if (wsi->role_ops != &role_ops_raw_skt ||
	    (i->local_protocol_name &&
	     !strcmp(i->local_protocol_name, "raw-proxy"))) {
		lwsl_wsi_debug(wsi, "adoption cb %d to %s %s",
			   wsi->role_ops->adoption_cb[0],
			   wsi->role_ops->name, wsi->a.protocol->name);

		wsi->a.protocol->callback(wsi, wsi->role_ops->adoption_cb[0],
				wsi->user_space, NULL, 0);
	}

#if defined(LWS_WITH_HUBBUB)
	if (i->uri_replace_to)
		wsi->http.rw = lws_rewrite_create(wsi, html_parser_cb,
					     i->uri_replace_from,
					     i->uri_replace_to);
#endif

	if (i->method && (!strcmp(i->method, "RAW") // ||
//			  !strcmp(i->method, "MQTT")
	)) {

		/*
		 * Not for MQTT here, since we don't know if we will
		 * pipeline it or not...
		 */

#if defined(LWS_WITH_TLS)

		wsi->tls.ssl = NULL;

		if (wsi->tls.use_ssl & LCCSCF_USE_SSL) {
			const char *cce = NULL;

			switch (
#if !defined(LWS_WITH_SYS_ASYNC_DNS)
			lws_client_create_tls(wsi, &cce, 1)
#else
			lws_client_create_tls(wsi, &cce, 0)
#endif
			) {
			case 1:
				return wsi;
			case 0:
				break;
			default:
				goto bail3;
			}
		}
#endif


		/* fallthru */

		wsi = lws_http_client_connect_via_info2(wsi);
	}

	if (wsi)
		/*
		 * If it subsequently fails, report CONNECTION_ERROR,
		 * because we're going to return a non-error return now.
		 */
		wsi->client_suppress_CONNECTION_ERROR = 0;

	return wsi;

#if defined(LWS_WITH_TLS)
bail3:
	lwsl_wsi_info(wsi, "tls start fail");
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "tls start fail");

	if (i->pwsi)
		*i->pwsi = NULL;

	return NULL;
#endif

bail:
#if defined(LWS_WITH_TLS)
	if (wsi->tls.ssl)
		lws_tls_restrict_return(wsi);
#endif

	lws_free_set_NULL(wsi->stash);
	lws_fi_destroy(&wsi->fic);
	lws_free(wsi);
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
bail2:
#endif

	if (i->pwsi)
		*i->pwsi = NULL;

	return NULL;
}
