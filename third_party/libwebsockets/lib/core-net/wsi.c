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

const char *
lws_wsi_tag(struct lws *wsi)
{
	if (!wsi)
		return "[null wsi]";
	return lws_lc_tag(&wsi->lc);
}

#if defined (_DEBUG)
void lwsi_set_role(struct lws *wsi, lws_wsi_state_t role)
{
	wsi->wsistate = (wsi->wsistate & (~LWSI_ROLE_MASK)) | role;

	lwsl_wsi_debug(wsi, "state 0x%lx", (unsigned long)wsi->wsistate);
}

void lwsi_set_state(struct lws *wsi, lws_wsi_state_t lrs)
{
	lws_wsi_state_t old = wsi->wsistate;

	wsi->wsistate = (old & (unsigned int)(~LRS_MASK)) | lrs;

	lwsl_wsi_debug(wsi, "lwsi_set_state 0x%lx -> 0x%lx",
			(unsigned long)old, (unsigned long)wsi->wsistate);
}
#endif


void
lws_log_prepend_wsi(struct lws_log_cx *cx, void *obj, char **p, char *e)
{
	struct lws *wsi = (struct lws *)obj;

	*p += lws_snprintf(*p, lws_ptr_diff_size_t(e, (*p)), "%s: ",
							lws_wsi_tag(wsi));
}

void
lws_vhost_bind_wsi(struct lws_vhost *vh, struct lws *wsi)
{
	if (wsi->a.vhost == vh)
		return;

	lws_context_lock(vh->context, __func__); /* ---------- context { */
	wsi->a.vhost = vh;

#if defined(LWS_WITH_TLS_JIT_TRUST)
	if (!vh->count_bound_wsi && vh->grace_after_unref) {
		lwsl_wsi_info(wsi, "in use");
		lws_sul_cancel(&vh->sul_unref);
	}
#endif

	vh->count_bound_wsi++;
	lws_context_unlock(vh->context); /* } context ---------- */

	lwsl_wsi_debug(wsi, "vh %s: wsi %s/%s, count_bound_wsi %d\n",
		   vh->name, wsi->role_ops ? wsi->role_ops->name : "none",
		   wsi->a.protocol ? wsi->a.protocol->name : "none",
		   vh->count_bound_wsi);
	assert(wsi->a.vhost->count_bound_wsi > 0);
}


/* req cx lock... acquires vh lock */
void
__lws_vhost_unbind_wsi(struct lws *wsi)
{
        struct lws_vhost *vh = wsi->a.vhost;

        if (!vh)
                return;

	lws_context_assert_lock_held(wsi->a.context);

	lws_vhost_lock(vh);

	assert(vh->count_bound_wsi > 0);
	vh->count_bound_wsi--;

#if defined(LWS_WITH_TLS_JIT_TRUST)
	if (!vh->count_bound_wsi && vh->grace_after_unref)
		lws_tls_jit_trust_vh_start_grace(vh);
#endif

	lwsl_wsi_debug(wsi, "vh %s: count_bound_wsi %d",
		   vh->name, vh->count_bound_wsi);

	lws_vhost_unlock(vh);

	if (!vh->count_bound_wsi && vh->being_destroyed)
		/*
		 * We have closed all wsi that were bound to this vhost
		 * by any pt: nothing can be servicing any wsi belonging
		 * to it any more.
		 *
		 * Finalize the vh destruction... must drop vh lock
		 */
		__lws_vhost_destroy2(vh);

	wsi->a.vhost = NULL;
}

struct lws *
lws_get_network_wsi(struct lws *wsi)
{
	if (!wsi)
		return NULL;

#if defined(LWS_WITH_HTTP2) || defined(LWS_ROLE_MQTT)
	if (!wsi->mux_substream
#if defined(LWS_WITH_CLIENT)
			&& !wsi->client_mux_substream
#endif
	)
		return wsi;

	while (wsi->mux.parent_wsi)
		wsi = wsi->mux.parent_wsi;
#endif

	return wsi;
}


const struct lws_protocols *
lws_vhost_name_to_protocol(struct lws_vhost *vh, const char *name)
{
	int n;

	for (n = 0; n < vh->count_protocols; n++)
		if (vh->protocols[n].name && !strcmp(name, vh->protocols[n].name))
			return &vh->protocols[n];

	return NULL;
}

int
lws_callback_all_protocol(struct lws_context *context,
			  const struct lws_protocols *protocol, int reason)
{
	struct lws_context_per_thread *pt = &context->pt[0];
	unsigned int n, m = context->count_threads;
	struct lws *wsi;

	while (m--) {
		for (n = 0; n < pt->fds_count; n++) {
			wsi = wsi_from_fd(context, pt->fds[n].fd);
			if (!wsi)
				continue;
			if (wsi->a.protocol == protocol)
				protocol->callback(wsi,
					(enum lws_callback_reasons)reason,
					wsi->user_space, NULL, 0);
		}
		pt++;
	}

	return 0;
}

void *
lws_evlib_wsi_to_evlib_pt(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	return pt->evlib_pt;
}

void *
lws_evlib_tsi_to_evlib_pt(struct lws_context *cx, int tsi)
{
	struct lws_context_per_thread *pt = &cx->pt[tsi];

	return pt->evlib_pt;
}

int
lws_callback_all_protocol_vhost_args(struct lws_vhost *vh,
			  const struct lws_protocols *protocol, int reason,
			  void *argp, size_t len)
{
	struct lws_context *context = vh->context;
	struct lws_context_per_thread *pt = &context->pt[0];
	unsigned int n, m = context->count_threads;
	struct lws *wsi;

	while (m--) {
		for (n = 0; n < pt->fds_count; n++) {
			wsi = wsi_from_fd(context, pt->fds[n].fd);
			if (!wsi)
				continue;
			if (wsi->a.vhost == vh && (wsi->a.protocol == protocol ||
						 !protocol))
				wsi->a.protocol->callback(wsi, (enum lws_callback_reasons)reason,
						wsi->user_space, argp, len);
		}
		pt++;
	}

	return 0;
}

int
lws_callback_all_protocol_vhost(struct lws_vhost *vh,
			  const struct lws_protocols *protocol, int reason)
{
	return lws_callback_all_protocol_vhost_args(vh, protocol, reason, NULL, 0);
}

int
lws_callback_vhost_protocols(struct lws *wsi, int reason, void *in, size_t len)
{
	int n;

	for (n = 0; n < wsi->a.vhost->count_protocols; n++)
		if (wsi->a.vhost->protocols[n].callback(wsi, (enum lws_callback_reasons)reason, NULL, in, len))
			return 1;

	return 0;
}

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
/*
 * We want to inject a fault that makes it feel like the peer hung up on us,
 * or we were otherwise cut off.
 */
void
lws_wsi_fault_timedclose_cb(lws_sorted_usec_list_t *s)
{
	struct lws *wsi = lws_container_of(s, struct lws, sul_fault_timedclose);

	lwsl_wsi_warn(wsi, "force-closing");
	lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);
}
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
void
lws_wsi_fault_timedclose(struct lws *wsi)
{
	uint64_t u;

	if (!lws_fi(&wsi->fic, "timedclose"))
		return;

	if (lws_fi_range(&wsi->fic, "timedclose_ms", &u))
		return;

	lwsl_wsi_warn(wsi, "injecting close in %ums", (unsigned int)u);
	lws_sul_schedule(wsi->a.context, wsi->tsi, &wsi->sul_fault_timedclose,
			 lws_wsi_fault_timedclose_cb,
			 (lws_usec_t)(u * 1000ull));
}
#endif


/*
 * We need the context lock
 */

struct lws *
__lws_wsi_create_with_role(struct lws_context *context, int tsi,
			   const struct lws_role_ops *ops,
			   lws_log_cx_t *log_cx_template)
{
	size_t s = sizeof(struct lws);
	struct lws *wsi;

	assert(tsi >= 0 && tsi < LWS_MAX_SMP);

	lws_context_assert_lock_held(context);

#if defined(LWS_WITH_EVENT_LIBS)
	s += context->event_loop_ops->evlib_size_wsi;
#endif

	wsi = lws_zalloc(s, __func__);

	if (!wsi) {
		lwsl_cx_err(context, "OOM");
		return NULL;
	}

	if (log_cx_template)
		wsi->lc.log_cx = log_cx_template;
	else
		wsi->lc.log_cx = context->log_cx;

#if defined(LWS_WITH_EVENT_LIBS)
	wsi->evlib_wsi = (uint8_t *)wsi + sizeof(*wsi);
#endif
	wsi->a.context = context;
	lws_role_transition(wsi, 0, LRS_UNCONNECTED, ops);
	wsi->pending_timeout = NO_PENDING_TIMEOUT;
	wsi->a.protocol = NULL;
	wsi->tsi = (char)tsi;
	wsi->a.vhost = NULL;
	wsi->desc.sockfd = LWS_SOCK_INVALID;
	wsi->position_in_fds_table = LWS_NO_FDS_POS;

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_xos_init(&wsi->fic.xos, lws_xos(&context->fic.xos));
#endif

	lws_fi_inherit_copy(&wsi->fic, &context->fic, "wsi", NULL);

	if (lws_fi(&wsi->fic, "createfail")) {
		lws_fi_destroy(&wsi->fic);
		lws_free(wsi);
		return NULL;
	}

	return wsi;
}

int
lws_wsi_inject_to_loop(struct lws_context_per_thread *pt, struct lws *wsi)
{
	int ret = 1;

	lws_pt_lock(pt, __func__); /* -------------- pt { */

	if (pt->context->event_loop_ops->sock_accept)
		if (pt->context->event_loop_ops->sock_accept(wsi))
			goto bail;

	if (__insert_wsi_socket_into_fds(pt->context, wsi))
		goto bail;

	ret = 0;

bail:
	lws_pt_unlock(pt);

	return ret;
}

/*
 * Take a copy of wsi->desc.sockfd before calling this, then close it
 * afterwards
 */

int
lws_wsi_extract_from_loop(struct lws *wsi)
{
	if (lws_socket_is_valid(wsi->desc.sockfd))
		__remove_wsi_socket_from_fds(wsi);

	if (!wsi->a.context->event_loop_ops->destroy_wsi &&
	    wsi->a.context->event_loop_ops->wsi_logical_close) {
		wsi->a.context->event_loop_ops->wsi_logical_close(wsi);
		return 1; /* close / destroy continues async */
	}

	if (wsi->a.context->event_loop_ops->destroy_wsi)
		wsi->a.context->event_loop_ops->destroy_wsi(wsi);

	return 0; /* he is destroyed */
}

int
lws_callback_vhost_protocols_vhost(struct lws_vhost *vh, int reason, void *in,
				   size_t len)
{
	int n;
	struct lws *wsi = lws_zalloc(sizeof(*wsi), "fake wsi");

	if (!wsi)
		return 1;

	wsi->a.context = vh->context;
	lws_vhost_bind_wsi(vh, wsi);

	for (n = 0; n < wsi->a.vhost->count_protocols; n++) {
		wsi->a.protocol = &vh->protocols[n];
		if (wsi->a.protocol->callback(wsi, (enum lws_callback_reasons)reason, NULL, in, len)) {
			lws_free(wsi);
			return 1;
		}
	}

	lws_free(wsi);

	return 0;
}


int
lws_rx_flow_control(struct lws *wsi, int _enable)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	int en = _enable;

	// h2 ignores rx flow control atm
	if (lwsi_role_h2(wsi) || wsi->mux_substream ||
	    lwsi_role_h2_ENCAPSULATION(wsi))
		return 0; // !!!

	lwsl_wsi_info(wsi, "0x%x", _enable);

	if (!(_enable & LWS_RXFLOW_REASON_APPLIES)) {
		/*
		 * convert user bool style to bitmap style... in user simple
		 * bool style _enable = 0 = flow control it, = 1 = allow rx
		 */
		en = LWS_RXFLOW_REASON_APPLIES | LWS_RXFLOW_REASON_USER_BOOL;
		if (_enable & 1)
			en |= LWS_RXFLOW_REASON_APPLIES_ENABLE_BIT;
	}

	lws_pt_lock(pt, __func__);

	/* any bit set in rxflow_bitmap DISABLEs rxflow control */
	if (en & LWS_RXFLOW_REASON_APPLIES_ENABLE_BIT)
		wsi->rxflow_bitmap = (uint8_t)(wsi->rxflow_bitmap & ~(en & 0xff));
	else
		wsi->rxflow_bitmap = (uint8_t)(wsi->rxflow_bitmap | (en & 0xff));

	if ((LWS_RXFLOW_PENDING_CHANGE | (!wsi->rxflow_bitmap)) ==
	    wsi->rxflow_change_to)
		goto skip;

	wsi->rxflow_change_to = LWS_RXFLOW_PENDING_CHANGE |
				(!wsi->rxflow_bitmap);

	lwsl_wsi_info(wsi, "bitmap 0x%x: en 0x%x, ch 0x%x",
			   wsi->rxflow_bitmap, en, wsi->rxflow_change_to);

	if (_enable & LWS_RXFLOW_REASON_FLAG_PROCESS_NOW ||
	    !wsi->rxflow_will_be_applied) {
		en = __lws_rx_flow_control(wsi);
		lws_pt_unlock(pt);

		return en;
	}

skip:
	lws_pt_unlock(pt);

	return 0;
}

void
lws_rx_flow_allow_all_protocol(const struct lws_context *context,
			       const struct lws_protocols *protocol)
{
	const struct lws_context_per_thread *pt = &context->pt[0];
	struct lws *wsi;
	unsigned int n, m = context->count_threads;

	while (m--) {
		for (n = 0; n < pt->fds_count; n++) {
			wsi = wsi_from_fd(context, pt->fds[n].fd);
			if (!wsi)
				continue;
			if (wsi->a.protocol == protocol)
				lws_rx_flow_control(wsi, LWS_RXFLOW_ALLOW);
		}
		pt++;
	}
}

int user_callback_handle_rxflow(lws_callback_function callback_function,
				struct lws *wsi,
				enum lws_callback_reasons reason, void *user,
				void *in, size_t len)
{
	int n;

	wsi->rxflow_will_be_applied = 1;
	n = callback_function(wsi, reason, user, in, len);
	wsi->rxflow_will_be_applied = 0;
	if (!n)
		n = __lws_rx_flow_control(wsi);

	return n;
}

int
__lws_rx_flow_control(struct lws *wsi)
{
	struct lws *wsic = wsi->child_list;

	// h2 ignores rx flow control atm
	if (lwsi_role_h2(wsi) || wsi->mux_substream ||
	    lwsi_role_h2_ENCAPSULATION(wsi))
		return 0; // !!!

	/* if he has children, do those if they were changed */
	while (wsic) {
		if (wsic->rxflow_change_to & LWS_RXFLOW_PENDING_CHANGE)
			__lws_rx_flow_control(wsic);

		wsic = wsic->sibling_list;
	}

	/* there is no pending change */
	if (!(wsi->rxflow_change_to & LWS_RXFLOW_PENDING_CHANGE))
		return 0;

	/* stuff is still buffered, not ready to really accept new input */
	if (lws_buflist_next_segment_len(&wsi->buflist, NULL)) {
		/* get ourselves called back to deal with stashed buffer */
		lws_callback_on_writable(wsi);
		// return 0;
	}

	/* now the pending is cleared, we can change rxflow state */

	wsi->rxflow_change_to &= (~LWS_RXFLOW_PENDING_CHANGE) & 3;

	lwsl_wsi_info(wsi, "rxflow: change_to %d",
		      wsi->rxflow_change_to & LWS_RXFLOW_ALLOW);

	/* adjust the pollfd for this wsi */

	if (wsi->rxflow_change_to & LWS_RXFLOW_ALLOW) {
		lwsl_wsi_info(wsi, "reenable POLLIN");
		// lws_buflist_describe(&wsi->buflist, NULL, __func__);
		if (__lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
			lwsl_wsi_info(wsi, "fail");
			return -1;
		}
	} else
		if (__lws_change_pollfd(wsi, LWS_POLLIN, 0))
			return -1;

	return 0;
}


const struct lws_protocols *
lws_get_protocol(struct lws *wsi)
{
	return wsi->a.protocol;
}


int
lws_ensure_user_space(struct lws *wsi)
{
	if (!wsi->a.protocol)
		return 0;

	/* allocate the per-connection user memory (if any) */

	if (wsi->a.protocol->per_session_data_size && !wsi->user_space) {
		wsi->user_space = lws_zalloc(
			    wsi->a.protocol->per_session_data_size, "user space");
		if (wsi->user_space == NULL) {
			lwsl_wsi_err(wsi, "OOM");
			return 1;
		}
	} else
		lwsl_wsi_debug(wsi, "protocol pss %lu, user_space=%p",
				    (long)wsi->a.protocol->per_session_data_size,
				    wsi->user_space);
	return 0;
}

void *
lws_adjust_protocol_psds(struct lws *wsi, size_t new_size)
{
	((struct lws_protocols *)lws_get_protocol(wsi))->per_session_data_size =
		new_size;

	if (lws_ensure_user_space(wsi))
			return NULL;

	return wsi->user_space;
}

int
lws_get_tsi(struct lws *wsi)
{
        return (int)wsi->tsi;
}

int
lws_is_ssl(struct lws *wsi)
{
#if defined(LWS_WITH_TLS)
	return wsi->tls.use_ssl & LCCSCF_USE_SSL;
#else
	(void)wsi;
	return 0;
#endif
}

#if defined(LWS_WITH_TLS) && !defined(LWS_WITH_MBEDTLS)
lws_tls_conn*
lws_get_ssl(struct lws *wsi)
{
	return wsi->tls.ssl;
}
#endif

int
lws_has_buffered_out(struct lws *wsi)
{
	if (wsi->buflist_out)
		return 1;

#if defined(LWS_ROLE_H2)
	{
		struct lws *nwsi = lws_get_network_wsi(wsi);

		if (nwsi->buflist_out)
			return 1;
	}
#endif

	return 0;
}

int
lws_partial_buffered(struct lws *wsi)
{
	return lws_has_buffered_out(wsi);
}

lws_fileofs_t
lws_get_peer_write_allowance(struct lws *wsi)
{
	if (!lws_rops_fidx(wsi->role_ops, LWS_ROPS_tx_credit))
		return -1;

	return lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_tx_credit).
				   tx_credit(wsi, LWSTXCR_US_TO_PEER, 0);
}

void
lws_role_transition(struct lws *wsi, enum lwsi_role role, enum lwsi_state state,
		    const struct lws_role_ops *ops)
{
#if (_LWS_ENABLED_LOGS & LLL_DEBUG) 
	const char *name = "(unset)";
#endif
	wsi->wsistate = (unsigned int)role | (unsigned int)state;
	if (ops)
		wsi->role_ops = ops;
#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
	if (wsi->role_ops)
		name = wsi->role_ops->name;
	lwsl_wsi_debug(wsi, "wsistate 0x%lx, ops %s",
			    (unsigned long)wsi->wsistate, name);
#endif
}

int
lws_parse_uri(char *p, const char **prot, const char **ads, int *port,
	      const char **path)
{
	const char *end;
	char unix_skt = 0;

	/* cut up the location into address, port and path */
	*prot = p;
	while (*p && (*p != ':' || p[1] != '/' || p[2] != '/'))
		p++;
	if (!*p) {
		end = p;
		p = (char *)*prot;
		*prot = end;
	} else {
		*p = '\0';
		p += 3;
	}
	if (*p == '+') /* unix skt */
		unix_skt = 1;

	*ads = p;
	if (!strcmp(*prot, "http") || !strcmp(*prot, "ws"))
		*port = 80;
	else if (!strcmp(*prot, "https") || !strcmp(*prot, "wss"))
		*port = 443;

	if (*p == '[') {
		++(*ads);
		while (*p && *p != ']')
			p++;
		if (*p)
			*p++ = '\0';
	} else
		while (*p && *p != ':' && (unix_skt || *p != '/'))
			p++;

	if (*p == ':') {
		*p++ = '\0';
		*port = atoi(p);
		while (*p && *p != '/')
			p++;
	}
	*path = "/";
	if (*p) {
		*p++ = '\0';
		if (*p)
			*path = p;
	}

	return 0;
}

/* ... */

int
lws_get_urlarg_by_name_safe(struct lws *wsi, const char *name, char *buf, int len)
{
	int n = 0, fraglen, sl = (int)strlen(name);

	do {
		fraglen = lws_hdr_copy_fragment(wsi, buf, len,
						WSI_TOKEN_HTTP_URI_ARGS, n);

		if (fraglen < 0)
			break;

		if (fraglen + 1 < len &&
		    fraglen >= sl &&
		    !strncmp(buf, name, (size_t)sl)) {
			/*
			 * If he left off the trailing =, trim it from the
			 * result
			 */

			if (name[sl - 1] != '=' &&
			    sl < fraglen &&
			    buf[sl] == '=')
				sl++;

			memmove(buf, buf + sl, (size_t)(fraglen - sl));
			buf[fraglen - sl] = '\0';

			return fraglen - sl;
		}

		n++;
	} while (1);

	return -1;
}

const char *
lws_get_urlarg_by_name(struct lws *wsi, const char *name, char *buf, int len)
{
	int n = lws_get_urlarg_by_name_safe(wsi, name, buf, len);

	return n < 0 ? NULL : buf;
}


#if defined(LWS_WITHOUT_EXTENSIONS)

/* we need to provide dummy callbacks for internal exts
 * so user code runs when faced with a lib compiled with
 * extensions disabled.
 */

int
lws_extension_callback_pm_deflate(struct lws_context *context,
                                  const struct lws_extension *ext,
                                  struct lws *wsi,
                                  enum lws_extension_callback_reasons reason,
                                  void *user, void *in, size_t len)
{
	(void)context;
	(void)ext;
	(void)wsi;
	(void)reason;
	(void)user;
	(void)in;
	(void)len;

	return 0;
}

int
lws_set_extension_option(struct lws *wsi, const char *ext_name,
			 const char *opt_name, const char *opt_val)
{
	return -1;
}
#endif

int
lws_is_cgi(struct lws *wsi) {
#ifdef LWS_WITH_CGI
	return !!wsi->http.cgi;
#else
	return 0;
#endif
}

const struct lws_protocol_vhost_options *
lws_pvo_search(const struct lws_protocol_vhost_options *pvo, const char *name)
{
	while (pvo) {
		if (!strcmp(pvo->name, name))
			break;

		pvo = pvo->next;
	}

	return pvo;
}

int
lws_pvo_get_str(void *in, const char *name, const char **result)
{
	const struct lws_protocol_vhost_options *pv =
		lws_pvo_search((const struct lws_protocol_vhost_options *)in,
				name);

	if (!pv)
		return 1;

	*result = (const char *)pv->value;

	return 0;
}

int
lws_broadcast(struct lws_context_per_thread *pt, int reason, void *in, size_t len)
{
	struct lws_vhost *v = pt->context->vhost_list;
	lws_fakewsi_def_plwsa(pt);
	int n, ret = 0;

	lws_fakewsi_prep_plwsa_ctx(pt->context);
#if !defined(LWS_PLAT_FREERTOS) && LWS_MAX_SMP > 1
	((struct lws *)plwsa)->tsi = (char)(int)(pt - &pt->context->pt[0]);
#endif

	while (v) {
		const struct lws_protocols *p = v->protocols;

		plwsa->vhost = v; /* not a real bound wsi */

		for (n = 0; n < v->count_protocols; n++) {
			plwsa->protocol = p;
			if (p->callback &&
			    p->callback((struct lws *)plwsa, (enum lws_callback_reasons)reason, NULL, in, len))
				ret |= 1;
			p++;
		}

		v = v->vhost_next;
	}

	return ret;
}

void *
lws_wsi_user(struct lws *wsi)
{
	return wsi->user_space;
}

int
lws_wsi_tsi(struct lws *wsi)
{
	return wsi->tsi;
}


void
lws_set_wsi_user(struct lws *wsi, void *data)
{
	if (!wsi->user_space_externally_allocated && wsi->user_space)
		lws_free(wsi->user_space);

	wsi->user_space_externally_allocated = 1;
	wsi->user_space = data;
}

struct lws *
lws_get_parent(const struct lws *wsi)
{
	return wsi->parent;
}

struct lws *
lws_get_child(const struct lws *wsi)
{
	return wsi->child_list;
}

void *
lws_get_opaque_parent_data(const struct lws *wsi)
{
	return wsi->opaque_parent_data;
}

void
lws_set_opaque_parent_data(struct lws *wsi, void *data)
{
	wsi->opaque_parent_data = data;
}

void *
lws_get_opaque_user_data(const struct lws *wsi)
{
	return wsi->a.opaque_user_data;
}

void
lws_set_opaque_user_data(struct lws *wsi, void *data)
{
	wsi->a.opaque_user_data = data;
}

int
lws_get_child_pending_on_writable(const struct lws *wsi)
{
	return wsi->parent_pending_cb_on_writable;
}

void
lws_clear_child_pending_on_writable(struct lws *wsi)
{
	wsi->parent_pending_cb_on_writable = 0;
}



const char *
lws_get_vhost_name(struct lws_vhost *vhost)
{
	return vhost->name;
}

int
lws_get_vhost_port(struct lws_vhost *vhost)
{
	return vhost->listen_port;
}

void *
lws_get_vhost_user(struct lws_vhost *vhost)
{
	return vhost->user;
}

const char *
lws_get_vhost_iface(struct lws_vhost *vhost)
{
	return vhost->iface;
}

lws_sockfd_type
lws_get_socket_fd(struct lws *wsi)
{
	if (!wsi)
		return -1;
	return wsi->desc.sockfd;
}


struct lws_vhost *
lws_vhost_get(struct lws *wsi)
{
	return wsi->a.vhost;
}

struct lws_vhost *
lws_get_vhost(struct lws *wsi)
{
	return wsi->a.vhost;
}

const struct lws_protocols *
lws_protocol_get(struct lws *wsi)
{
	return wsi->a.protocol;
}

#if defined(LWS_WITH_UDP)
const struct lws_udp *
lws_get_udp(const struct lws *wsi)
{
	return wsi->udp;
}
#endif

struct lws_context *
lws_get_context(const struct lws *wsi)
{
	return wsi->a.context;
}

struct lws_log_cx *
lwsl_wsi_get_cx(struct lws *wsi)
{
	if (!wsi)
		return NULL;

	return wsi->lc.log_cx;
}

#if defined(LWS_WITH_CLIENT)
int
_lws_generic_transaction_completed_active_conn(struct lws **_wsi, char take_vh_lock)
{
	struct lws *wnew, *wsi = *_wsi;

	/*
	 * Are we constitutionally capable of having a queue, ie, we are on
	 * the "active client connections" list?
	 *
	 * If not, that's it for us.
	 */

	if (lws_dll2_is_detached(&wsi->dll_cli_active_conns))
		return 0; /* no new transaction */

	/*
	 * With h1 queuing, the original "active client" moves his attributes
	 * like fd, ssl, queue and active client list entry to the next guy in
	 * the queue before closing... it's because the user code knows the
	 * individual wsi and the action must take place in the correct wsi
	 * context.  Note this means we don't truly pipeline headers.
	 *
	 * Trying to keep the original "active client" in place to do the work
	 * of the wsi breaks down when dealing with queued POSTs otherwise; it's
	 * also competing with the real mux child arrangements and complicating
	 * the code.
	 *
	 * For that reason, see if we have any queued child now...
	 */

	if (!wsi->dll2_cli_txn_queue_owner.head) {
		/*
		 * Nothing pipelined... we should hang around a bit
		 * in case something turns up... otherwise we'll close
		 */
		lwsl_wsi_info(wsi, "nothing pipelined waiting");
		lwsi_set_state(wsi, LRS_IDLING);

		lws_set_timeout(wsi, PENDING_TIMEOUT_CLIENT_CONN_IDLE,
				wsi->keep_warm_secs);

		return 0; /* no new transaction right now */
	}

	/*
	 * We have a queued child wsi we should bequeath our assets to, before
	 * closing ourself
	 */

	if (take_vh_lock)
		lws_vhost_lock(wsi->a.vhost);

	wnew = lws_container_of(wsi->dll2_cli_txn_queue_owner.head, struct lws,
				dll2_cli_txn_queue);

	assert(wsi != wnew);

	lws_dll2_remove(&wnew->dll2_cli_txn_queue);

	assert(lws_socket_is_valid(wsi->desc.sockfd));

	__lws_change_pollfd(wsi, LWS_POLLOUT | LWS_POLLIN, 0);

	/* copy the fd */
	wnew->desc = wsi->desc;

	assert(lws_socket_is_valid(wnew->desc.sockfd));

	/* disconnect the fd from association with old wsi */

	if (__remove_wsi_socket_from_fds(wsi))
		return -1;

	sanity_assert_no_wsi_traces(wsi->a.context, wsi);
	sanity_assert_no_sockfd_traces(wsi->a.context, wsi->desc.sockfd);
	wsi->desc.sockfd = LWS_SOCK_INVALID;

	__lws_wsi_remove_from_sul(wsi);

	/*
	 * ... we're doing some magic here in terms of handing off the socket
	 * that has been active to a wsi that has not yet itself been active...
	 * depending on the event lib we may need to give a magic spark to the
	 * new guy and snuff out the old guy's magic spark at that level as well
	 */

#if defined(LWS_WITH_EVENT_LIBS)
	if (wsi->a.context->event_loop_ops->destroy_wsi)
		wsi->a.context->event_loop_ops->destroy_wsi(wsi);
	if (wsi->a.context->event_loop_ops->sock_accept)
		wsi->a.context->event_loop_ops->sock_accept(wnew);
#endif

	/* point the fd table entry to new guy */

	assert(lws_socket_is_valid(wnew->desc.sockfd));

	if (__insert_wsi_socket_into_fds(wsi->a.context, wnew))
		return -1;

#if defined(LWS_WITH_TLS)
	/* pass on the tls */

	wnew->tls = wsi->tls;
	wsi->tls.client_bio = NULL;
	wsi->tls.ssl = NULL;
	wsi->tls.use_ssl = 0;
#endif

	/* take over his copy of his endpoint as an active connection */

	if (!wnew->cli_hostname_copy && wsi->cli_hostname_copy) {
		wnew->cli_hostname_copy = wsi->cli_hostname_copy;
		wsi->cli_hostname_copy = NULL;
	}
	wnew->keep_warm_secs = wsi->keep_warm_secs;

	/*
	 * selected queued guy now replaces the original leader on the
	 * active client conn list
	 */

	lws_dll2_remove(&wsi->dll_cli_active_conns);
	lws_dll2_add_tail(&wnew->dll_cli_active_conns,
			  &wsi->a.vhost->dll_cli_active_conns_owner);

	/* move any queued guys to queue on new active conn */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   wsi->dll2_cli_txn_queue_owner.head) {
		struct lws *ww = lws_container_of(d, struct lws,
					  dll2_cli_txn_queue);

		lws_dll2_remove(&ww->dll2_cli_txn_queue);
		lws_dll2_add_tail(&ww->dll2_cli_txn_queue,
				  &wnew->dll2_cli_txn_queue_owner);

	} lws_end_foreach_dll_safe(d, d1);

	if (take_vh_lock)
		lws_vhost_unlock(wsi->a.vhost);

	/*
	 * The original leader who passed on all his powers already can die...
	 * in the call stack above us there are guys who still want to touch
	 * him, so have him die next time around the event loop, not now.
	 */

	wsi->already_did_cce = 1; /* so the close doesn't trigger a CCE */
	lws_set_timeout(wsi, 1, LWS_TO_KILL_ASYNC);

	/* after the first one, they can only be coming from the queue */
	wnew->transaction_from_pipeline_queue = 1;

	lwsl_wsi_notice(wsi, " pipeline queue passed -> %s", lws_wsi_tag(wnew));

	*_wsi = wnew; /* inform caller we swapped */

	return 1; /* new transaction */
}
#endif

int LWS_WARN_UNUSED_RESULT
lws_raw_transaction_completed(struct lws *wsi)
{
	if (lws_has_buffered_out(wsi)) {
		/*
		 * ...so he tried to send something large, but it went out
		 * as a partial, but he immediately called us to say he wants
		 * to close the connection.
		 *
		 * Defer the close until the last part of the partial is sent.
		 *
		 */

		lwsl_wsi_debug(wsi, "deferring due to partial");
		wsi->close_when_buffered_out_drained = 1;
		lws_callback_on_writable(wsi);

		return 0;
	}

	return -1;
}

int
lws_bind_protocol(struct lws *wsi, const struct lws_protocols *p,
		  const char *reason)
{
//	if (wsi->a.protocol == p)
//		return 0;
	const struct lws_protocols *vp = wsi->a.vhost->protocols, *vpo;

	if (wsi->a.protocol && wsi->protocol_bind_balance) {
		wsi->a.protocol->callback(wsi,
		       wsi->role_ops->protocol_unbind_cb[!!lwsi_role_server(wsi)],
					wsi->user_space, (void *)reason, 0);
		wsi->protocol_bind_balance = 0;
	}
	if (!wsi->user_space_externally_allocated)
		lws_free_set_NULL(wsi->user_space);

	lws_same_vh_protocol_remove(wsi);

	wsi->a.protocol = p;
	if (!p)
		return 0;

	if (lws_ensure_user_space(wsi))
		return 1;

	if (p > vp && p < &vp[wsi->a.vhost->count_protocols])
		lws_same_vh_protocol_insert(wsi, (int)(p - vp));
	else {
		int n = wsi->a.vhost->count_protocols;
		int hit = 0;

		vpo = vp;

		while (n--) {
			if (p->name && vp->name && !strcmp(p->name, vp->name)) {
				hit = 1;
				lws_same_vh_protocol_insert(wsi, (int)(vp - vpo));
				break;
			}
			vp++;
		}
		if (!hit)
			lwsl_err("%s: %p is not in vhost '%s' protocols list\n",
				 __func__, p, wsi->a.vhost->name);
	}

	if (wsi->a.protocol->callback(wsi, wsi->role_ops->protocol_bind_cb[
				    !!lwsi_role_server(wsi)],
				    wsi->user_space, NULL, 0))
		return 1;

	wsi->protocol_bind_balance = 1;

	return 0;
}

void
lws_http_close_immortal(struct lws *wsi)
{
	struct lws *nwsi;

	if (!wsi->mux_substream)
		return;

	assert(wsi->mux_stream_immortal);
	wsi->mux_stream_immortal = 0;

	nwsi = lws_get_network_wsi(wsi);
	lwsl_wsi_debug(wsi, "%s (%d)", lws_wsi_tag(nwsi),
				       nwsi->immortal_substream_count);
	assert(nwsi->immortal_substream_count);
	nwsi->immortal_substream_count--;
	if (!nwsi->immortal_substream_count)
		/*
		 * since we closed the only immortal stream on this nwsi, we
		 * need to reapply a normal timeout regime to the nwsi
		 */
		lws_set_timeout(nwsi, PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE,
				wsi->a.vhost->keepalive_timeout ?
				    wsi->a.vhost->keepalive_timeout : 31);
}

void
lws_mux_mark_immortal(struct lws *wsi)
{
	struct lws *nwsi;

	lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

	if (!wsi->mux_substream
#if defined(LWS_WITH_CLIENT)
			&& !wsi->client_mux_substream
#endif
	) {
		lwsl_wsi_err(wsi, "not mux substream");
		return;
	}

	if (wsi->mux_stream_immortal)
		/* only need to handle it once per child wsi */
		return;

	nwsi = lws_get_network_wsi(wsi);
	if (!nwsi)
		return;

	lwsl_wsi_debug(wsi, "%s (%d)\n", lws_wsi_tag(nwsi),
				    nwsi->immortal_substream_count);

	wsi->mux_stream_immortal = 1;
	assert(nwsi->immortal_substream_count < 255); /* largest count */
	nwsi->immortal_substream_count++;
	if (nwsi->immortal_substream_count == 1)
		lws_set_timeout(nwsi, NO_PENDING_TIMEOUT, 0);
}

int
lws_http_mark_sse(struct lws *wsi)
{
	if (!wsi)
		return 0;

	lws_http_headers_detach(wsi);
	lws_mux_mark_immortal(wsi);

	if (wsi->mux_substream)
		wsi->h2_stream_carries_sse = 1;

	return 0;
}

#if defined(LWS_WITH_CLIENT)

const char *
lws_wsi_client_stash_item(struct lws *wsi, int stash_idx, int hdr_idx)
{
	/* try the generic client stash */
	if (wsi->stash)
		return wsi->stash->cis[stash_idx];

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	/* if not, use the ah stash if applicable */
	return lws_hdr_simple_ptr(wsi, (enum lws_token_indexes)hdr_idx);
#else
	return NULL;
#endif
}
#endif

#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)

void
lws_wsi_mux_insert(struct lws *wsi, struct lws *parent_wsi, unsigned int sid)
{
	lwsl_wsi_info(wsi, "par %s: assign sid %d (curr %d)",
			lws_wsi_tag(parent_wsi), sid, wsi->mux.my_sid);

	if (wsi->mux.my_sid && wsi->mux.my_sid != (unsigned int)sid)
		assert(0);

	wsi->mux.my_sid = sid;
	wsi->mux.parent_wsi = parent_wsi;
	wsi->role_ops = parent_wsi->role_ops;

	/* new guy's sibling is whoever was the first child before */
	wsi->mux.sibling_list = parent_wsi->mux.child_list;

	/* first child is now the new guy */
	parent_wsi->mux.child_list = wsi;

	parent_wsi->mux.child_count++;
}

struct lws *
lws_wsi_mux_from_id(struct lws *parent_wsi, unsigned int sid)
{
	lws_start_foreach_ll(struct lws *, wsi, parent_wsi->mux.child_list) {
		if (wsi->mux.my_sid == sid)
			return wsi;
	} lws_end_foreach_ll(wsi, mux.sibling_list);

	return NULL;
}

void
lws_wsi_mux_dump_children(struct lws *wsi)
{
#if defined(_DEBUG)
	if (!wsi->mux.parent_wsi || !lwsl_visible(LLL_INFO))
		return;

	lws_start_foreach_llp(struct lws **, w,
			      wsi->mux.parent_wsi->mux.child_list) {
		lwsl_wsi_info(wsi, "   \\---- child %s %s\n",
				   (*w)->role_ops ? (*w)->role_ops->name : "?",
							   lws_wsi_tag(*w));
		assert(*w != (*w)->mux.sibling_list);
	} lws_end_foreach_llp(w, mux.sibling_list);
#endif
}

void
lws_wsi_mux_close_children(struct lws *wsi, int reason)
{
	struct lws *wsi2;
	struct lws **w;

	if (!wsi->mux.child_list)
		return;

	w = &wsi->mux.child_list;
	while (*w) {
		lwsl_wsi_info((*w), "   closing child");
		/* disconnect from siblings */
		wsi2 = (*w)->mux.sibling_list;
		assert (wsi2 != *w);
		(*w)->mux.sibling_list = NULL;
		(*w)->socket_is_permanently_unusable = 1;
		__lws_close_free_wsi(*w, (enum lws_close_status)reason, "mux child recurse");
		*w = wsi2;
	}
}


void
lws_wsi_mux_sibling_disconnect(struct lws *wsi)
{
	struct lws *wsi2;

	lws_start_foreach_llp(struct lws **, w,
			      wsi->mux.parent_wsi->mux.child_list) {

		/* disconnect from siblings */
		if (*w == wsi) {
			wsi2 = (*w)->mux.sibling_list;
			(*w)->mux.sibling_list = NULL;
			*w = wsi2;
			lwsl_wsi_debug(wsi, " disentangled from sibling %s",
					    lws_wsi_tag(wsi2));
			break;
		}
	} lws_end_foreach_llp(w, mux.sibling_list);
	wsi->mux.parent_wsi->mux.child_count--;

	wsi->mux.parent_wsi = NULL;
}

void
lws_wsi_mux_dump_waiting_children(struct lws *wsi)
{
#if defined(_DEBUG)
	lwsl_info("%s: %s: children waiting for POLLOUT service:\n",
		  __func__, lws_wsi_tag(wsi));

	wsi = wsi->mux.child_list;
	while (wsi) {
		lwsl_wsi_info(wsi, "  %c sid %u: 0x%x %s %s",
			  wsi->mux.requested_POLLOUT ? '*' : ' ',
			  wsi->mux.my_sid, lwsi_state(wsi),
			  wsi->role_ops->name,
			  wsi->a.protocol ? wsi->a.protocol->name : "noprotocol");

		wsi = wsi->mux.sibling_list;
	}
#endif
}

int
lws_wsi_mux_mark_parents_needing_writeable(struct lws *wsi)
{
	struct lws /* *network_wsi = lws_get_network_wsi(wsi), */ *wsi2;
	//int already = network_wsi->mux.requested_POLLOUT;

	/* mark everybody above him as requesting pollout */

	wsi2 = wsi;
	while (wsi2) {
		wsi2->mux.requested_POLLOUT = 1;
		lwsl_wsi_info(wsi2, "sid %u, pending writable",
							wsi2->mux.my_sid);
		wsi2 = wsi2->mux.parent_wsi;
	}

	return 0; // already;
}

struct lws *
lws_wsi_mux_move_child_to_tail(struct lws **wsi2)
{
	struct lws *w = *wsi2;

	while (w) {
		if (!w->mux.sibling_list) { /* w is the current last */
			lwsl_wsi_debug(w, "*wsi2 = %s\n", lws_wsi_tag(*wsi2));

			if (w == *wsi2) /* we are already last */
				break;

			/* last points to us as new last */
			w->mux.sibling_list = *wsi2;

			/* guy pointing to us until now points to
			 * our old next */
			*wsi2 = (*wsi2)->mux.sibling_list;

			/* we point to nothing because we are last */
			w->mux.sibling_list->mux.sibling_list = NULL;

			/* w becomes us */
			w = w->mux.sibling_list;
			break;
		}
		w = w->mux.sibling_list;
	}

	/* clear the waiting for POLLOUT on the guy that was chosen */

	if (w)
		w->mux.requested_POLLOUT = 0;

	return w;
}

int
lws_wsi_mux_action_pending_writeable_reqs(struct lws *wsi)
{
	struct lws *w = wsi->mux.child_list;

	while (w) {
		if (w->mux.requested_POLLOUT) {
			if (lws_change_pollfd(wsi, 0, LWS_POLLOUT))
				return -1;
			return 0;
		}
		w = w->mux.sibling_list;
	}

	if (lws_change_pollfd(wsi, LWS_POLLOUT, 0))
		return -1;

	return 0;
}

int
lws_wsi_txc_check_skint(struct lws_tx_credit *txc, int32_t tx_cr)
{
	if (txc->tx_cr <= 0) {
		/*
		 * If other side is not able to cope with us sending any DATA
		 * so no matter if we have POLLOUT on our side if it's DATA we
		 * want to send.
		 */

		if (!txc->skint)
			lwsl_info("%s: %p: skint (%d)\n", __func__, txc,
				  (int)txc->tx_cr);

		txc->skint = 1;

		return 1;
	}

	if (txc->skint)
		lwsl_info("%s: %p: unskint (%d)\n", __func__, txc,
			  (int)txc->tx_cr);

	txc->skint = 0;

	return 0;
}

#if defined(_DEBUG)
void
lws_wsi_txc_describe(struct lws_tx_credit *txc, const char *at, uint32_t sid)
{
	lwsl_info("%s: %p: %s: sid %d: %speer-to-us: %d, us-to-peer: %d\n",
		  __func__, txc, at, (int)sid, txc->skint ? "SKINT, " : "",
		  (int)txc->peer_tx_cr_est, (int)txc->tx_cr);
}
#endif

int
lws_wsi_tx_credit(struct lws *wsi, char peer_to_us, int add)
{
	if (wsi->role_ops && lws_rops_fidx(wsi->role_ops, LWS_ROPS_tx_credit))
		return lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_tx_credit).
				   tx_credit(wsi, peer_to_us, add);

	return 0;
}

/*
 * Let the protocol know about incoming tx credit window updates if it's
 * managing the flow control manually (it may want to proxy this information)
 */

int
lws_wsi_txc_report_manual_txcr_in(struct lws *wsi, int32_t bump)
{
	if (!wsi->txc.manual)
		/*
		 * If we don't care about managing it manually, no need to
		 * report it
		 */
		return 0;

	return user_callback_handle_rxflow(wsi->a.protocol->callback,
					   wsi, LWS_CALLBACK_WSI_TX_CREDIT_GET,
					   wsi->user_space, NULL, (size_t)bump);
}

#if defined(LWS_WITH_CLIENT)

int
lws_wsi_mux_apply_queue(struct lws *wsi)
{
	/* we have a transaction queue that wants to pipeline */

	lws_context_lock(wsi->a.context, __func__); /* -------------- cx { */
	lws_vhost_lock(wsi->a.vhost);

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   wsi->dll2_cli_txn_queue_owner.head) {
		struct lws *w = lws_container_of(d, struct lws,
						 dll2_cli_txn_queue);

#if defined(LWS_ROLE_H2)
		if (lwsi_role_http(wsi) &&
		    lwsi_state(w) == LRS_H2_WAITING_TO_SEND_HEADERS) {
			lwsl_wsi_info(w, "cli pipeq to be h2");

			lwsi_set_state(w, LRS_H1C_ISSUE_HANDSHAKE2);

			/* remove ourselves from client queue */
			lws_dll2_remove(&w->dll2_cli_txn_queue);

			/* attach ourselves as an h2 stream */
			lws_wsi_h2_adopt(wsi, w);
		}
#endif

#if defined(LWS_ROLE_MQTT)
		if (lwsi_role_mqtt(wsi) &&
		    lwsi_state(wsi) == LRS_ESTABLISHED) {
			lwsl_wsi_info(w, "cli pipeq to be mqtt\n");

			/* remove ourselves from client queue */
			lws_dll2_remove(&w->dll2_cli_txn_queue);

			/* attach ourselves as an h2 stream */
			lws_wsi_mqtt_adopt(wsi, w);
		}
#endif

	} lws_end_foreach_dll_safe(d, d1);

	lws_vhost_unlock(wsi->a.vhost);
	lws_context_unlock(wsi->a.context); /* } cx --------------  */

	return 0;
}

#endif

#endif
