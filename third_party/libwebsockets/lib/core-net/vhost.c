/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

void
lws_tls_session_vh_destroy(struct lws_vhost *vh);

const struct lws_role_ops *available_roles[] = {
#if defined(LWS_ROLE_H2)
	&role_ops_h2,
#endif
#if defined(LWS_ROLE_H1)
	&role_ops_h1,
#endif
#if defined(LWS_ROLE_WS)
	&role_ops_ws,
#endif
#if defined(LWS_ROLE_DBUS)
	&role_ops_dbus,
#endif
#if defined(LWS_ROLE_RAW_PROXY)
	&role_ops_raw_proxy,
#endif
#if defined(LWS_ROLE_MQTT) && defined(LWS_WITH_CLIENT)
	&role_ops_mqtt,
#endif
#if defined(LWS_WITH_NETLINK)
	&role_ops_netlink,
#endif
	NULL
};

#if defined(LWS_WITH_ABSTRACT)
const struct lws_protocols *available_abstract_protocols[] = {
#if defined(LWS_ROLE_RAW)
	&protocol_abs_client_raw_skt,
#endif
	NULL
};
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
const struct lws_protocols *available_secstream_protocols[] = {
#if defined(LWS_ROLE_H1)
	&protocol_secstream_h1,
#endif
#if defined(LWS_ROLE_H2)
	&protocol_secstream_h2,
#endif
#if defined(LWS_ROLE_WS)
	&protocol_secstream_ws,
#endif
#if defined(LWS_ROLE_MQTT)
	&protocol_secstream_mqtt,
#endif
	&protocol_secstream_raw,
	NULL
};
#endif

static const char * const mount_protocols[] = {
	"http://",
	"https://",
	"file://",
	"cgi://",
	">http://",
	">https://",
	"callback://"
};

const struct lws_role_ops *
lws_role_by_name(const char *name)
{
	LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar)
		if (!strcmp(ar->name, name))
			return ar;
	LWS_FOR_EVERY_AVAILABLE_ROLE_END;

	if (!strcmp(name, role_ops_raw_skt.name))
		return &role_ops_raw_skt;

#if defined(LWS_ROLE_RAW_FILE)
	if (!strcmp(name, role_ops_raw_file.name))
		return &role_ops_raw_file;
#endif

	return NULL;
}

int
lws_role_call_alpn_negotiated(struct lws *wsi, const char *alpn)
{
#if defined(LWS_WITH_TLS)
	if (!alpn)
		return 0;

#if !defined(LWS_ESP_PLATFORM)
	lwsl_wsi_info(wsi, "'%s'", alpn);
#endif

	LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar)
		if (ar->alpn && !strcmp(ar->alpn, alpn) &&
		    lws_rops_fidx(ar, LWS_ROPS_alpn_negotiated)) {
#if defined(LWS_WITH_SERVER)
			lws_metrics_tag_wsi_add(wsi, "upg", ar->name);
#endif
			return (lws_rops_func_fidx(ar, LWS_ROPS_alpn_negotiated)).
						   alpn_negotiated(wsi, alpn);
		}
	LWS_FOR_EVERY_AVAILABLE_ROLE_END;
#endif
	return 0;
}

int
lws_role_call_adoption_bind(struct lws *wsi, int type, const char *prot)
{
	int n;

	/*
	 * if the vhost is told to bind accepted sockets to a given role,
	 * then look it up by name and try to bind to the specific role.
	 */
	if (lws_check_opt(wsi->a.vhost->options,
			  LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG) &&
	    wsi->a.vhost->listen_accept_role) {
		const struct lws_role_ops *role =
			lws_role_by_name(wsi->a.vhost->listen_accept_role);

		if (!prot)
			prot = wsi->a.vhost->listen_accept_protocol;

		if (!role)
			lwsl_wsi_err(wsi, "can't find role '%s'",
					  wsi->a.vhost->listen_accept_role);

		if (!strcmp(wsi->a.vhost->listen_accept_role, "raw-proxy"))
			type |= LWS_ADOPT_FLAG_RAW_PROXY;

		if (role && lws_rops_fidx(role, LWS_ROPS_adoption_bind)) {
			n = (lws_rops_func_fidx(role, LWS_ROPS_adoption_bind)).
						adoption_bind(wsi, type, prot);
			if (n < 0)
				return -1;
			if (n) /* did the bind */
				return 0;
		}

		if (type & _LWS_ADOPT_FINISH) {
			lwsl_wsi_debug(wsi, "leaving bound to role %s",
					    wsi->role_ops->name);
			return 0;
		}

		lwsl_wsi_warn(wsi, "adoption bind to role '%s', "
			  "protocol '%s', type 0x%x, failed",
			  wsi->a.vhost->listen_accept_role, prot, type);
	}

	/*
	 * Otherwise ask each of the roles in order of preference if they
	 * want to bind to this accepted socket
	 */

	LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar)
		if (lws_rops_fidx(ar, LWS_ROPS_adoption_bind) &&
		    (lws_rops_func_fidx(ar, LWS_ROPS_adoption_bind)).
					    adoption_bind(wsi, type, prot))
			return 0;
	LWS_FOR_EVERY_AVAILABLE_ROLE_END;

	/* fall back to raw socket role if, eg, h1 not configured */

	if (lws_rops_fidx(&role_ops_raw_skt, LWS_ROPS_adoption_bind) &&
	    (lws_rops_func_fidx(&role_ops_raw_skt, LWS_ROPS_adoption_bind)).
				    adoption_bind(wsi, type, prot))
		return 0;

#if defined(LWS_ROLE_RAW_FILE)

	lwsl_wsi_notice(wsi, "falling back to raw file role bind");

	/* fall back to raw file role if, eg, h1 not configured */

	if (lws_rops_fidx(&role_ops_raw_file, LWS_ROPS_adoption_bind) &&
	    (lws_rops_func_fidx(&role_ops_raw_file, LWS_ROPS_adoption_bind)).
				    adoption_bind(wsi, type, prot))
		return 0;
#endif

	return 1;
}

#if defined(LWS_WITH_CLIENT)
int
lws_role_call_client_bind(struct lws *wsi,
			  const struct lws_client_connect_info *i)
{
	LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar)
		if (lws_rops_fidx(ar, LWS_ROPS_client_bind)) {
			int m = (lws_rops_func_fidx(ar, LWS_ROPS_client_bind)).
							client_bind(wsi, i);

			if (m < 0)
				return m;
			if (m)
				return 0;
		}
	LWS_FOR_EVERY_AVAILABLE_ROLE_END;

	/* fall back to raw socket role if, eg, h1 not configured */

	if (lws_rops_fidx(&role_ops_raw_skt, LWS_ROPS_client_bind) &&
	    (lws_rops_func_fidx(&role_ops_raw_skt, LWS_ROPS_client_bind)).
					client_bind(wsi, i))
		return 0;

	return 1;
}
#endif

void *
lws_protocol_vh_priv_zalloc(struct lws_vhost *vhost,
			    const struct lws_protocols *prot, int size)
{
	int n = 0;

	if (!vhost || !prot || !vhost->protocols || !prot->name)
		return NULL;

	/* allocate the vh priv array only on demand */
	if (!vhost->protocol_vh_privs) {
		vhost->protocol_vh_privs = (void **)lws_zalloc(
				(size_t)vhost->count_protocols * sizeof(void *),
				"protocol_vh_privs");

		if (!vhost->protocol_vh_privs)
			return NULL;
	}

	while (n < vhost->count_protocols && &vhost->protocols[n] != prot)
		n++;

	if (n == vhost->count_protocols) {
		n = 0;
		while (n < vhost->count_protocols) {
			if (vhost->protocols[n].name &&
			    !strcmp(vhost->protocols[n].name, prot->name))
				break;
			n++;
		}

		if (n == vhost->count_protocols) {
			lwsl_vhost_err(vhost, "unknown protocol %p", prot);
			return NULL;
		}
	}

	vhost->protocol_vh_privs[n] = lws_zalloc((size_t)size, "vh priv");
	return vhost->protocol_vh_privs[n];
}

void *
lws_protocol_vh_priv_get(struct lws_vhost *vhost,
			 const struct lws_protocols *prot)
{
	int n = 0;

	if (!vhost || !vhost->protocols ||
	    !vhost->protocol_vh_privs || !prot || !prot->name)
		return NULL;

	while (n < vhost->count_protocols && &vhost->protocols[n] != prot)
		n++;

	if (n == vhost->count_protocols) {
		n = 0;
		while (n < vhost->count_protocols) {
			if (vhost->protocols[n].name &&
			    !strcmp(vhost->protocols[n].name, prot->name))
				break;
			n++;
		}

		if (n == vhost->count_protocols) {
			lwsl_vhost_err(vhost, "unknown protocol %p", prot);
			return NULL;
		}
	}

	return vhost->protocol_vh_privs[n];
}

void *
lws_vhd_find_by_pvo(struct lws_context *cx, const char *protname,
		    const char *pvo_name, const char *pvo_value)
{
	struct lws_vhost *vh;
	int n;

	/* let's go through all the vhosts */

	vh = cx->vhost_list;
	while (vh) {

		if (vh->protocol_vh_privs) {

		for (n = 0; n < vh->count_protocols; n++) {
			const struct lws_protocol_vhost_options *pv;

			if (strcmp(vh->protocols[n].name, protname))
				continue;

			/* this vh has an instance of the required protocol */

			pv = lws_pvo_search(vh->pvo, protname);
			if (!pv)
				continue;

			pv = lws_pvo_search(pv->options, pvo_name);
			if (!pv)
				continue;

			/* ... he also has a pvo of the right name... */
			if (!strcmp(pv->value, pvo_value))
				/*
				 * ... yes, the pvo has the right value too,
				 * return a pointer to this vhost-protocol
				 * private alloc (ie, its "vhd")
				 */
				return vh->protocol_vh_privs[n];
		}
		} else
			lwsl_vhost_notice(vh, "no privs yet");
		vh = vh->vhost_next;
	}

	return NULL;
}

const struct lws_protocol_vhost_options *
lws_vhost_protocol_options(struct lws_vhost *vh, const char *name)
{
	const struct lws_protocol_vhost_options *pvo = vh->pvo;

	if (!name)
		return NULL;

	while (pvo) {
		if (!strcmp(pvo->name, name))
			return pvo;
		pvo = pvo->next;
	}

	return NULL;
}

int
lws_protocol_init_vhost(struct lws_vhost *vh, int *any)
{
	const struct lws_protocol_vhost_options *pvo, *pvo1;
	int n;
#if defined(LWS_PLAT_FREERTOS)
	struct lws_a _lwsa, *lwsa = &_lwsa;

	memset(&_lwsa, 0, sizeof(_lwsa));
#else
	struct lws _lws;
	struct lws_a *lwsa = &_lws.a;

	memset(&_lws, 0, sizeof(_lws));
#endif

	lwsa->context = vh->context;
	lwsa->vhost = vh;

	/* initialize supported protocols on this vhost */

	for (n = 0; n < vh->count_protocols; n++) {
		lwsa->protocol = &vh->protocols[n];
		if (!vh->protocols[n].name)
			continue;
		pvo = lws_vhost_protocol_options(vh, vh->protocols[n].name);
		if (pvo) {
			/*
			 * linked list of options specific to
			 * vh + protocol
			 */
			pvo1 = pvo;
			pvo = pvo1->options;

			while (pvo) {
				lwsl_vhost_debug(vh, "protocol \"%s\", "
						     "option \"%s\"",
						     vh->protocols[n].name,
						     pvo->name);

				if (!strcmp(pvo->name, "default")) {
					lwsl_vhost_info(vh, "Setting default "
							     "protocol to %s",
							     vh->protocols[n].name);
					vh->default_protocol_index = (unsigned char)n;
				}
				if (!strcmp(pvo->name, "raw")) {
					lwsl_vhost_info(vh, "Setting raw "
							     "protocol to %s",
							     vh->protocols[n].name);
					vh->raw_protocol_index = (unsigned char)n;
				}
				pvo = pvo->next;
			}
		} else
			lwsl_vhost_debug(vh, "not instantiating %s",
					     vh->protocols[n].name);

#if defined(LWS_WITH_TLS)
		if (any)
			*any |= !!vh->tls.ssl_ctx;
#endif

		pvo = lws_vhost_protocol_options(vh, vh->protocols[n].name);

		/*
		 * inform all the protocols that they are doing their
		 * one-time initialization if they want to.
		 *
		 * NOTE the fakewsi is garbage, except the key pointers that are
		 * prepared in case the protocol handler wants to touch them
		 */

		if (pvo
#if !defined(LWS_WITH_PLUGINS)
				/*
				 * with plugins, you have to explicitly
				 * instantiate them per-vhost with pvos.
				 *
				 * Without plugins, not setting the vhost pvo
				 * list at creation enables all the protocols
				 * by default, for backwards compatibility
				 */
				|| !vh->pvo
#endif
		) {
			lwsl_vhost_info(vh, "init %s.%s", vh->name,
					vh->protocols[n].name);
			if (vh->protocols[n].callback((struct lws *)lwsa,
				LWS_CALLBACK_PROTOCOL_INIT, NULL,
#if !defined(LWS_WITH_PLUGINS)
				(void *)(pvo ? pvo->options : NULL),
#else
				(void *)pvo->options,
#endif
				0)) {
				if (vh->protocol_vh_privs && vh->protocol_vh_privs[n]) {
					lws_free(vh->protocol_vh_privs[n]);
					vh->protocol_vh_privs[n] = NULL;
				}
			lwsl_vhost_err(vh, "protocol %s failed init",
					vh->protocols[n].name);

				return 1;
			}
		}
	}

	vh->created_vhost_protocols = 1;

	return 0;
}

/*
 * inform every vhost that hasn't already done it, that
 * his protocols are initializing
 */
int
lws_protocol_init(struct lws_context *context)
{
	struct lws_vhost *vh = context->vhost_list;
	int any = 0, r = 0;

	if (context->doing_protocol_init)
		return 0;

	context->doing_protocol_init = 1;

	lwsl_cx_info(context, "\n");

	while (vh) {

		/* only do the protocol init once for a given vhost */
		if (vh->created_vhost_protocols ||
		    (lws_check_opt(vh->options, LWS_SERVER_OPTION_SKIP_PROTOCOL_INIT)))
			goto next;

		if (lws_protocol_init_vhost(vh, &any)) {
			lwsl_vhost_warn(vh, "init vhost %s failed", vh->name);
			r = -1;
		}
next:
		vh = vh->vhost_next;
	}

	context->doing_protocol_init = 0;

	if (r)
		lwsl_cx_warn(context, "some protocols did not init");

	if (!context->protocol_init_done) {

		context->protocol_init_done = 1;
		lws_finalize_startup(context);

		return 0;
	}

#if defined(LWS_WITH_SERVER)
	if (any) {
		lws_tls_check_all_cert_lifetimes(context);
	}
#endif

	return 0;
}


/* list of supported protocols and callbacks */

static const struct lws_protocols protocols_dummy[] = {
	/* first protocol must always be HTTP handler */

	{
		"http-only",			/* name */
		lws_callback_http_dummy,	/* callback */
		0,				/* per_session_data_size */
		0,				/* rx_buffer_size */
		0,				/* id */
		NULL,				/* user */
		0				/* tx_packet_size */
	},
	/*
	 * the other protocols are provided by lws plugins
	 */
	{ NULL, NULL, 0, 0, 0, NULL, 0} /* terminator */
};


#ifdef LWS_PLAT_OPTEE
#undef LWS_HAVE_GETENV
#endif

struct lws_vhost *
lws_create_vhost(struct lws_context *context,
		 const struct lws_context_creation_info *info)
{
	struct lws_vhost *vh, **vh1 = &context->vhost_list;
	const struct lws_http_mount *mounts;
	const struct lws_protocols *pcols = info->protocols;
#ifdef LWS_WITH_PLUGINS
	struct lws_plugin *plugin = context->plugin_list;
#endif
	struct lws_protocols *lwsp;
	int m, f = !info->pvo, fx = 0, abs_pcol_count = 0, sec_pcol_count = 0;
	const char *name = "default";
	char buf[96];
	char *p;
#if defined(LWS_WITH_SYS_ASYNC_DNS)
	extern struct lws_protocols lws_async_dns_protocol;
#endif
	int n;

	if (info->vhost_name)
		name = info->vhost_name;

	if (lws_fi(&info->fic, "vh_create_oom"))
		vh = NULL;
	else
		vh = lws_zalloc(sizeof(*vh) + strlen(name) + 1
#if defined(LWS_WITH_EVENT_LIBS)
			+ context->event_loop_ops->evlib_size_vh
#endif
			, __func__);
	if (!vh)
		goto early_bail;

	if (info->log_cx)
		vh->lc.log_cx = info->log_cx;
	else
		vh->lc.log_cx = &log_cx;

#if defined(LWS_WITH_EVENT_LIBS)
	vh->evlib_vh = (void *)&vh[1];
	vh->name = (const char *)vh->evlib_vh +
			context->event_loop_ops->evlib_size_vh;
#else
	vh->name = (const char *)&vh[1];
#endif
	memcpy((char *)vh->name, name, strlen(name) + 1);

#if LWS_MAX_SMP > 1
	lws_mutex_refcount_init(&vh->mr);
#endif

	if (!pcols && !info->pprotocols)
		pcols = &protocols_dummy[0];

	vh->context = context;
	{
		char *end = buf + sizeof(buf) - 1;
		p = buf;

		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%s", vh->name);
		if (info->iface)
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "|%s", info->iface);
		if (info->port && !(info->port & 0xffff))
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "|%u", info->port);
	}

	__lws_lc_tag(context, &context->lcg[LWSLCG_VHOST], &vh->lc, "%s|%s|%d",
		     buf, info->iface ? info->iface : "", info->port);

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	vh->fic.name = "vh";
	if (info->fic.fi_owner.count)
		/*
		 * This moves all the lws_fi_t from info->fi to the vhost fi,
		 * leaving it empty
		 */
		lws_fi_import(&vh->fic, &info->fic);

	lws_fi_inherit_copy(&vh->fic, &context->fic, "vh", vh->name);
	if (lws_fi(&vh->fic, "vh_create_oom"))
		goto bail;
#endif

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	vh->http.error_document_404 = info->error_document_404;
#endif

	if (lws_check_opt(info->options, LWS_SERVER_OPTION_ONLY_RAW))
		lwsl_vhost_info(vh, "set to only support RAW");

	vh->iface = info->iface;
#if !defined(LWS_PLAT_FREERTOS) && !defined(OPTEE_TA) && !defined(WIN32)
	vh->bind_iface = info->bind_iface;
#endif
#if defined(LWS_WITH_CLIENT)
	if (info->connect_timeout_secs)
		vh->connect_timeout_secs = (int)info->connect_timeout_secs;
	else
		vh->connect_timeout_secs = 20;
#endif
	/* apply the context default lws_retry */

	if (info->retry_and_idle_policy)
		vh->retry_policy = info->retry_and_idle_policy;
	else
		vh->retry_policy = &context->default_retry;

	/*
	 * let's figure out how many protocols the user is handing us, using the
	 * old or new way depending on what he gave us
	 */

	if (!pcols)
		for (vh->count_protocols = 0;
			info->pprotocols[vh->count_protocols];
			vh->count_protocols++)
			;
	else
		for (vh->count_protocols = 0;
			pcols[vh->count_protocols].callback;
			vh->count_protocols++)
				;

	vh->options			= info->options;
	vh->pvo				= info->pvo;
	vh->headers			= info->headers;
	vh->user			= info->user;
	vh->finalize			= info->finalize;
	vh->finalize_arg		= info->finalize_arg;
	vh->listen_accept_role		= info->listen_accept_role;
	vh->listen_accept_protocol	= info->listen_accept_protocol;
	vh->unix_socket_perms		= info->unix_socket_perms;
	vh->fo_listen_queue		= info->fo_listen_queue;

	LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar)
	if (lws_rops_fidx(ar, LWS_ROPS_init_vhost) &&
	    (lws_rops_func_fidx(ar, LWS_ROPS_init_vhost)).init_vhost(vh, info))
		return NULL;
	LWS_FOR_EVERY_AVAILABLE_ROLE_END;


	if (info->keepalive_timeout)
		vh->keepalive_timeout = info->keepalive_timeout;
	else
		vh->keepalive_timeout = 5;

	if (info->timeout_secs_ah_idle)
		vh->timeout_secs_ah_idle = (int)info->timeout_secs_ah_idle;
	else
		vh->timeout_secs_ah_idle = 10;

#if defined(LWS_WITH_TLS)

	vh->tls.alpn = info->alpn;
	vh->tls.ssl_info_event_mask = info->ssl_info_event_mask;

	if (info->ecdh_curve)
		lws_strncpy(vh->tls.ecdh_curve, info->ecdh_curve,
			    sizeof(vh->tls.ecdh_curve));

	/* carefully allocate and take a copy of cert + key paths if present */
	n = 0;
	if (info->ssl_cert_filepath)
		n += (int)strlen(info->ssl_cert_filepath) + 1;
	if (info->ssl_private_key_filepath)
		n += (int)strlen(info->ssl_private_key_filepath) + 1;

	if (n) {
		vh->tls.key_path = vh->tls.alloc_cert_path =
					lws_malloc((unsigned int)n, "vh paths");
		if (info->ssl_cert_filepath) {
			n = (int)strlen(info->ssl_cert_filepath) + 1;
			memcpy(vh->tls.alloc_cert_path,
			       info->ssl_cert_filepath, (unsigned int)n);
			vh->tls.key_path += n;
		}
		if (info->ssl_private_key_filepath)
			memcpy(vh->tls.key_path, info->ssl_private_key_filepath,
			       strlen(info->ssl_private_key_filepath) + 1);
	}
#endif

#if defined(LWS_WITH_HTTP_PROXY) && defined(LWS_ROLE_WS)
	fx = 1;
#endif
#if defined(LWS_WITH_ABSTRACT)
	abs_pcol_count = (int)LWS_ARRAY_SIZE(available_abstract_protocols) - 1;
#endif
#if defined(LWS_WITH_SECURE_STREAMS)
	sec_pcol_count = (int)LWS_ARRAY_SIZE(available_secstream_protocols) - 1;
#endif

	/*
	 * give the vhost a unified list of protocols including:
	 *
	 * - internal, async_dns if enabled (first vhost only)
	 * - internal, abstracted ones
	 * - the ones that came from plugins
	 * - his user protocols
	 */

	if (lws_fi(&vh->fic, "vh_create_pcols_oom"))
		lwsp = NULL;
	else
		lwsp = lws_zalloc(sizeof(struct lws_protocols) *
				((unsigned int)vh->count_protocols +
				   (unsigned int)abs_pcol_count +
				   (unsigned int)sec_pcol_count +
				   (unsigned int)context->plugin_protocol_count +
				   (unsigned int)fx + 1), "vh plugin table");
	if (!lwsp) {
		lwsl_err("OOM\n");
		goto bail;
	}

	/*
	 * 1: user protocols (from pprotocols or protocols)
	 */

	m = vh->count_protocols;
	if (!pcols) {
		for (n = 0; n < m; n++)
			memcpy(&lwsp[n], info->pprotocols[n], sizeof(lwsp[0]));
	} else
		memcpy(lwsp, pcols, sizeof(struct lws_protocols) * (unsigned int)m);

	/*
	 * 2: abstract protocols
	 */
#if defined(LWS_WITH_ABSTRACT)
	for (n = 0; n < abs_pcol_count; n++) {
		memcpy(&lwsp[m++], available_abstract_protocols[n],
		       sizeof(*lwsp));
		vh->count_protocols++;
	}
#endif
	/*
	 * 3: async dns protocol (first vhost only)
	 */
#if defined(LWS_WITH_SYS_ASYNC_DNS)
	if (!context->vhost_list) {
		memcpy(&lwsp[m++], &lws_async_dns_protocol,
		       sizeof(struct lws_protocols));
		vh->count_protocols++;
	}
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
	for (n = 0; n < sec_pcol_count; n++) {
		memcpy(&lwsp[m++], available_secstream_protocols[n],
		       sizeof(*lwsp));
		vh->count_protocols++;
	}
#endif

	/*
	 * 3: For compatibility, all protocols enabled on vhost if only
	 * the default vhost exists.  Otherwise only vhosts who ask
	 * for a protocol get it enabled.
	 */

	if (context->options & LWS_SERVER_OPTION_EXPLICIT_VHOSTS)
		f = 0;
	(void)f;
#ifdef LWS_WITH_PLUGINS
	if (plugin) {
		while (plugin) {
			const lws_plugin_protocol_t *plpr =
				(const lws_plugin_protocol_t *)plugin->hdr;

			for (n = 0; n < plpr->count_protocols; n++) {
				/*
				 * for compatibility's sake, no pvo implies
				 * allow all protocols
				 */
				if (f || lws_vhost_protocol_options(vh,
						plpr->protocols[n].name)) {
					memcpy(&lwsp[m],
					       &plpr->protocols[n],
					       sizeof(struct lws_protocols));
					m++;
					vh->count_protocols++;
				}
			}
			plugin = plugin->list;
		}
	}
#endif

#if defined(LWS_WITH_HTTP_PROXY) && defined(LWS_ROLE_WS)
	memcpy(&lwsp[m++], &lws_ws_proxy, sizeof(*lwsp));
	vh->count_protocols++;
#endif

	vh->protocols = lwsp;
	vh->allocated_vhost_protocols = 1;

	vh->same_vh_protocol_owner = (struct lws_dll2_owner *)
			lws_zalloc(sizeof(struct lws_dll2_owner) *
				   (unsigned int)vh->count_protocols, "same vh list");
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	vh->http.mount_list = info->mounts;
#endif

#if defined(LWS_WITH_SYS_METRICS) && defined(LWS_WITH_SERVER)
	{
		char *end = buf + sizeof(buf) - 1;
		p = buf;

		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "vh.%s", vh->name);
		if (info->iface)
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ".%s", info->iface);
		if (info->port && !(info->port & 0xffff))
			p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ".%u", info->port);
		p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ".rx");
		vh->mt_traffic_rx = lws_metric_create(context, 0, buf);
		p[-2] = 't';
		vh->mt_traffic_tx = lws_metric_create(context, 0, buf);
	}
#endif

#ifdef LWS_WITH_UNIX_SOCK
	if (LWS_UNIX_SOCK_ENABLED(vh)) {
		lwsl_vhost_info(vh, "Creating '%s' path \"%s\", %d protocols",
				vh->name, vh->iface, vh->count_protocols);
	} else
#endif
	{
		switch(info->port) {
		case CONTEXT_PORT_NO_LISTEN:
			strcpy(buf, "(serving disabled)");
			break;
		case CONTEXT_PORT_NO_LISTEN_SERVER:
			strcpy(buf, "(no listener)");
			break;
		default:
			lws_snprintf(buf, sizeof(buf), "port %u", info->port);
			break;
		}
		lwsl_vhost_info(vh, "Creating Vhost '%s' %s, %d protocols, IPv6 %s",
			    vh->name, buf, vh->count_protocols,
			    LWS_IPV6_ENABLED(vh) ? "on" : "off");
	}
	mounts = info->mounts;
	while (mounts) {
		(void)mount_protocols[0];
		lwsl_vhost_info(vh, "   mounting %s%s to %s",
			  mount_protocols[mounts->origin_protocol],
			  mounts->origin ? mounts->origin : "none",
			  mounts->mountpoint);

		mounts = mounts->mount_next;
	}

	vh->listen_port = info->port;

#if defined(LWS_WITH_SOCKS5)
	vh->socks_proxy_port = 0;
	vh->socks_proxy_address[0] = '\0';
#endif

#if defined(LWS_WITH_CLIENT) && defined(LWS_CLIENT_HTTP_PROXYING)
	/* either use proxy from info, or try get it from env var */
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	vh->http.http_proxy_port = 0;
	vh->http.http_proxy_address[0] = '\0';
	/* http proxy */
	if (info->http_proxy_address) {
		/* override for backwards compatibility */
		if (info->http_proxy_port)
			vh->http.http_proxy_port = info->http_proxy_port;
		lws_set_proxy(vh, info->http_proxy_address);
	} else
#endif
	{
#ifdef LWS_HAVE_GETENV
#if defined(__COVERITY__)
		p = NULL;
#else
		p = getenv("http_proxy"); /* coverity[tainted_scalar] */
		if (p) {
			lws_strncpy(buf, p, sizeof(buf));
			lws_set_proxy(vh, buf);
		}
#endif
#endif
	}
#endif
#if defined(LWS_WITH_SOCKS5)
	lws_socks5c_ads_server(vh, info);
#endif

	vh->ka_time = info->ka_time;
	vh->ka_interval = info->ka_interval;
	vh->ka_probes = info->ka_probes;

	if (vh->options & LWS_SERVER_OPTION_STS)
		lwsl_vhost_notice(vh, "   STS enabled");

#ifdef LWS_WITH_ACCESS_LOG
	if (info->log_filepath) {
		if (lws_fi(&vh->fic, "vh_create_access_log_open_fail"))
			vh->log_fd = (int)LWS_INVALID_FILE;
		else
			vh->log_fd = lws_open(info->log_filepath,
				  O_CREAT | O_APPEND | O_RDWR, 0600);
		if (vh->log_fd == (int)LWS_INVALID_FILE) {
			lwsl_vhost_err(vh, "unable to open log filepath %s",
					   info->log_filepath);
			goto bail;
		}
#ifndef WIN32
		if (context->uid != (uid_t)-1)
			if (chown(info->log_filepath, context->uid,
				  context->gid) == -1)
				lwsl_vhost_err(vh, "unable to chown log file %s",
						   info->log_filepath);
#endif
	} else
		vh->log_fd = (int)LWS_INVALID_FILE;
#endif
	if (lws_fi(&vh->fic, "vh_create_ssl_srv") ||
	    lws_context_init_server_ssl(info, vh)) {
		lwsl_vhost_err(vh, "lws_context_init_server_ssl failed");
		goto bail1;
	}
	if (lws_fi(&vh->fic, "vh_create_ssl_cli") ||
	    lws_context_init_client_ssl(info, vh)) {
		lwsl_vhost_err(vh, "lws_context_init_client_ssl failed");
		goto bail1;
	}
#if defined(LWS_WITH_SERVER)
	lws_context_lock(context, __func__);
	if (lws_fi(&vh->fic, "vh_create_srv_init"))
		n = -1;
	else
		n = _lws_vhost_init_server(info, vh);
	lws_context_unlock(context);
	if (n < 0) {
		lwsl_vhost_err(vh, "init server failed\n");
		goto bail1;
	}
#endif

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	n = !!context->vhost_list;
#endif

	while (1) {
		if (!(*vh1)) {
			*vh1 = vh;
			break;
		}
		vh1 = &(*vh1)->vhost_next;
	};

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	if (!n)
		lws_async_dns_init(context);
#endif

	/* for the case we are adding a vhost much later, after server init */

	if (context->protocol_init_done)
		if (lws_fi(&vh->fic, "vh_create_protocol_init") ||
		    lws_protocol_init(context)) {
			lwsl_vhost_err(vh, "lws_protocol_init failed");
			goto bail1;
		}

	return vh;

bail1:
	lws_vhost_destroy(vh);

	return NULL;

bail:
	__lws_lc_untag(vh->context, &vh->lc);
	lws_fi_destroy(&vh->fic);
	lws_free(vh);

early_bail:
	lws_fi_destroy(&info->fic);

	return NULL;
}

int
lws_init_vhost_client_ssl(const struct lws_context_creation_info *info,
			  struct lws_vhost *vhost)
{
	struct lws_context_creation_info i;

	memcpy(&i, info, sizeof(i));
	i.port = CONTEXT_PORT_NO_LISTEN;

	return lws_context_init_client_ssl(&i, vhost);
}

void
lws_cancel_service_pt(struct lws *wsi)
{
	lws_plat_pipe_signal(wsi->a.context, wsi->tsi);
}

void
lws_cancel_service(struct lws_context *context)
{
	struct lws_context_per_thread *pt = &context->pt[0];
	short m;

	if (context->service_no_longer_possible)
		return;

	lwsl_cx_debug(context, "\n");

	for (m = 0; m < context->count_threads; m++) {
		if (pt->pipe_wsi)
			lws_plat_pipe_signal(pt->context, m);
		pt++;
	}
}

int
__lws_create_event_pipes(struct lws_context *context)
{
	struct lws_context_per_thread *pt;
	struct lws *wsi;
	int n;

	/*
	 * Create the pt event pipes... these are unique in that they are
	 * not bound to a vhost or protocol (both are NULL)
	 */

#if LWS_MAX_SMP > 1
	for (n = 0; n < context->count_threads; n++) {
#else
	n = 0;
	{
#endif
		pt = &context->pt[n];

		if (pt->pipe_wsi)
			return 0;

		wsi = __lws_wsi_create_with_role(context, n, &role_ops_pipe,
							NULL);
		if (!wsi)
			return 1;

		__lws_lc_tag(context, &context->lcg[LWSLCG_WSI], &wsi->lc,
				"pipe");

		wsi->event_pipe = 1;
		pt->pipe_wsi = wsi;

		if (!lws_plat_pipe_create(wsi)) {
			/*
			 * platform code returns 0 if it actually created pipes
			 * and initialized pt->dummy_pipe_fds[].  If it used
			 * some other mechanism outside of signaling in the
			 * normal event loop, we skip treating the pipe as
			 * related to dummy_pipe_fds[], adding it to the fds,
			 * etc.
			 */

			wsi->desc.sockfd = context->pt[n].dummy_pipe_fds[0];
			// lwsl_debug("event pipe fd %d\n", wsi->desc.sockfd);

			if (lws_wsi_inject_to_loop(pt, wsi))
					goto bail;
		}
	}

	return 0;

bail:

	return 1;
}

void
lws_destroy_event_pipe(struct lws *wsi)
{
	int n;

	lwsl_wsi_info(wsi, "in");

	n = lws_wsi_extract_from_loop(wsi);
	lws_plat_pipe_close(wsi);
	if (!n)
		lws_free(wsi);
}

/*
 * Start close process for any wsi bound to this vhost that belong to the
 * service thread we are called from.  Because of async event lib close, or
 * protocol staged close on wsi, latency with pts joining in closing their
 * wsi on the vhost, this may take some time.
 *
 * When the wsi count bound to the vhost (from all pts) drops to zero, the
 * vhost destruction will be finalized.
 */

void
__lws_vhost_destroy_pt_wsi_dieback_start(struct lws_vhost *vh)
{
#if LWS_MAX_SMP > 1
	/* calling pt thread has done its wsi dieback */
	int tsi = lws_pthread_self_to_tsi(vh->context);
#else
	int tsi = 0;
#endif
	struct lws_context *ctx = vh->context;
	struct lws_context_per_thread *pt = &ctx->pt[tsi];
	unsigned int n;

#if LWS_MAX_SMP > 1
	if (vh->close_flow_vs_tsi[lws_pthread_self_to_tsi(vh->context)])
		/* this pt has already done its bit */
		return;
#endif

#if defined(LWS_WITH_CLIENT)
	/*
	 * destroy any wsi that are associated with us but have no socket
	 * (and will otherwise be missed for destruction)
	 */
	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
			      vh->vh_awaiting_socket_owner.head) {
		struct lws *w =
			lws_container_of(d, struct lws, vh_awaiting_socket);

		if (w->tsi == tsi) {

			lwsl_vhost_debug(vh, "closing aso");
			lws_close_free_wsi(w, LWS_CLOSE_STATUS_NOSTATUS,
					   "awaiting skt");
		}

	} lws_end_foreach_dll_safe(d, d1);
#endif

	/*
	 * Close any wsi on this pt bound to the vhost
	 */

	n = 0;
	while (n < pt->fds_count) {
		struct lws *wsi = wsi_from_fd(ctx, pt->fds[n].fd);

		if (wsi && wsi->tsi == tsi && wsi->a.vhost == vh) {

			lwsl_wsi_debug(wsi, "pt %d: closin, role %s", tsi,
					    wsi->role_ops->name);

			lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);

			if (pt->pipe_wsi == wsi)
				pt->pipe_wsi = NULL;
		}
		n++;
	}

#if LWS_MAX_SMP > 1
	/* calling pt thread has done its wsi dieback */
	vh->close_flow_vs_tsi[lws_pthread_self_to_tsi(vh->context)] = 1;
#endif
}

#if defined(LWS_WITH_NETWORK)

/* returns nonzero if v1 and v2 can share listen sockets */
int
lws_vhost_compare_listen(struct lws_vhost *v1, struct lws_vhost *v2)
{
	return ((!v1->iface && !v2->iface) ||
		 (v1->iface && v2->iface && !strcmp(v1->iface, v2->iface))) &&
		v1->listen_port == v2->listen_port;
}

/* helper to interate every listen socket on any vhost and call cb on it */
int
lws_vhost_foreach_listen_wsi(struct lws_context *cx, void *arg,
			     lws_dll2_foreach_cb_t cb)
{
	struct lws_vhost *v = cx->vhost_list;
	int n;

	while (v) {

		n = lws_dll2_foreach_safe(&v->listen_wsi, arg, cb);
		if (n)
			return n;

		v = v->vhost_next;
	}

	return 0;
}

#endif

/*
 * Mark the vhost as being destroyed, so things trying to use it abort.
 *
 * Dispose of the listen socket.
 */

void
lws_vhost_destroy1(struct lws_vhost *vh)
{
	struct lws_context *context = vh->context;
	int n;

	lwsl_vhost_info(vh, "\n");

	lws_context_lock(context, "vhost destroy 1"); /* ---------- context { */

	if (vh->being_destroyed)
		goto out;

	/*
	 * let's lock all the pts, to enforce pt->vh order... pt is refcounted
	 * so it's OK if we acquire it later inside this
	 */

	for (n = 0; n < context->count_threads; n++)
		lws_pt_lock((&context->pt[n]), __func__);

	lws_vhost_lock(vh); /* -------------- vh { */

#if defined(LWS_WITH_TLS_SESSIONS) && defined(LWS_WITH_TLS)
	lws_tls_session_vh_destroy(vh);
#endif

	vh->being_destroyed = 1;
	lws_dll2_add_tail(&vh->vh_being_destroyed_list,
			  &context->owner_vh_being_destroyed);

#if defined(LWS_WITH_NETWORK) && defined(LWS_WITH_SERVER)
	/*
	 * PHASE 1: take down or reassign any listen wsi
	 *
	 * Are there other vhosts that are piggybacking on our listen sockets?
	 * If so we need to hand each listen socket off to one of the others
	 * so it will remain open.
	 *
	 * If not, close the listen socket now.
	 *
	 * Either way the listen socket response to the vhost close is
	 * immediately performed.
	 */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
			      lws_dll2_get_head(&vh->listen_wsi)) {
		struct lws *wsi = lws_container_of(d, struct lws, listen_list);

		/*
		 * For each of our listen sockets, check every other vhost to
		 * see if another vhost should be given our listen socket.
		 *
		 * ipv4 and ipv6 sockets will both match and be migrated.
		 */

		lws_start_foreach_ll(struct lws_vhost *, v,
				     context->vhost_list) {
			if (v != vh && !v->being_destroyed &&
			    lws_vhost_compare_listen(v, vh)) {
				/*
				 * this can only be a listen wsi, which is
				 * restricted... it has no protocol or other
				 * bindings or states.  So we can simply
				 * swap it to a vhost that has the same
				 * iface + port, but is not closing.
				 */

				lwsl_vhost_notice(vh, "listen skt migrate -> %s",
						      lws_vh_tag(v));

				lws_dll2_remove(&wsi->listen_list);
				lws_dll2_add_tail(&wsi->listen_list,
						  &v->listen_wsi);

				/* req cx + vh lock */
				/*
				 * If the vhost sees it's being destroyed and
				 * in the unbind the number of wsis bound to
				 * it falls to zero, it will destroy the
				 * vhost opportunistically before we can
				 * complete the transfer.  Add a fake wsi
				 * bind temporarily to disallow this...
				 */
				v->count_bound_wsi++;
				__lws_vhost_unbind_wsi(wsi);
				lws_vhost_bind_wsi(v, wsi);
				/*
				 * ... remove the fake wsi bind
				 */
				v->count_bound_wsi--;
				break;
			}
		} lws_end_foreach_ll(v, vhost_next);

	} lws_end_foreach_dll_safe(d, d1);

	/*
	 * If any listen wsi left we couldn't pass to other vhosts, close them
	 */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
			           lws_dll2_get_head(&vh->listen_wsi)) {
		struct lws *wsi = lws_container_of(d, struct lws, listen_list);

		lws_dll2_remove(&wsi->listen_list);
		lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);

	} lws_end_foreach_dll_safe(d, d1);

#endif
#if defined(LWS_WITH_TLS_JIT_TRUST)
	lws_sul_cancel(&vh->sul_unref);
#endif

	lws_vhost_unlock(vh); /* } vh -------------- */

	for (n = 0; n < context->count_threads; n++)
		lws_pt_unlock((&context->pt[n]));

out:
	lws_context_unlock(context); /* --------------------------- context { */
}

#if defined(LWS_WITH_ABSTRACT)
static int
destroy_ais(struct lws_dll2 *d, void *user)
{
	lws_abs_t *ai = lws_container_of(d, lws_abs_t, abstract_instances);

	lws_abs_destroy_instance(&ai);

	return 0;
}
#endif

/*
 * Either start close or destroy any wsi on the vhost that belong to this pt,
 * if SMP mark the vh that we have done it for
 *
 * Must not have lock on vh
 */

void
__lws_vhost_destroy2(struct lws_vhost *vh)
{
	const struct lws_protocols *protocol = NULL;
	struct lws_context *context = vh->context;
	struct lws wsi;
	int n;

	vh->being_destroyed = 0;

	// lwsl_info("%s: %s\n", __func__, vh->name);

	/*
	 * let the protocols destroy the per-vhost protocol objects
	 */

	memset(&wsi, 0, sizeof(wsi));
	wsi.a.context = vh->context;
	wsi.a.vhost = vh; /* not a real bound wsi */
	protocol = vh->protocols;
	if (protocol && vh->created_vhost_protocols) {
		n = 0;
		while (n < vh->count_protocols) {
			wsi.a.protocol = protocol;

			lwsl_vhost_debug(vh, "protocol destroy");

			if (protocol->callback)
				protocol->callback(&wsi, LWS_CALLBACK_PROTOCOL_DESTROY,
					   NULL, NULL, 0);
			protocol++;
			n++;
		}
	}

	/*
	 * remove vhost from context list of vhosts
	 */

	lws_start_foreach_llp(struct lws_vhost **, pv, context->vhost_list) {
		if (*pv == vh) {
			*pv = vh->vhost_next;
			break;
		}
	} lws_end_foreach_llp(pv, vhost_next);

	/* add ourselves to the pending destruction list */

	if (vh->context->vhost_pending_destruction_list != vh) {
		vh->vhost_next = vh->context->vhost_pending_destruction_list;
		vh->context->vhost_pending_destruction_list = vh;
	}

	//lwsl_debug("%s: do dfl '%s'\n", __func__, vh->name);

	/* remove ourselves from the pending destruction list */

	lws_start_foreach_llp(struct lws_vhost **, pv,
			      context->vhost_pending_destruction_list) {
		if ((*pv) == vh) {
			*pv = (*pv)->vhost_next;
			break;
		}
	} lws_end_foreach_llp(pv, vhost_next);

	/*
	 * Free all the allocations associated with the vhost
	 */

	protocol = vh->protocols;
	if (protocol) {
		n = 0;
		while (n < vh->count_protocols) {
			if (vh->protocol_vh_privs &&
			    vh->protocol_vh_privs[n]) {
				lws_free(vh->protocol_vh_privs[n]);
				vh->protocol_vh_privs[n] = NULL;
			}
			protocol++;
			n++;
		}
	}
	if (vh->protocol_vh_privs)
		lws_free(vh->protocol_vh_privs);
	lws_ssl_SSL_CTX_destroy(vh);
	lws_free(vh->same_vh_protocol_owner);

	if (
#if defined(LWS_WITH_PLUGINS)
		context->plugin_list ||
#endif
	    (context->options & LWS_SERVER_OPTION_EXPLICIT_VHOSTS) ||
	    vh->allocated_vhost_protocols)
		lws_free((void *)vh->protocols);
#if defined(LWS_WITH_NETWORK)
	LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar)
	if (lws_rops_fidx(ar, LWS_ROPS_destroy_vhost))
		lws_rops_func_fidx(ar, LWS_ROPS_destroy_vhost).
							destroy_vhost(vh);
	LWS_FOR_EVERY_AVAILABLE_ROLE_END;
#endif

#ifdef LWS_WITH_ACCESS_LOG
	if (vh->log_fd != (int)LWS_INVALID_FILE)
		close(vh->log_fd);
#endif

#if defined (LWS_WITH_TLS)
	lws_free_set_NULL(vh->tls.alloc_cert_path);
#endif

#if LWS_MAX_SMP > 1
	lws_mutex_refcount_destroy(&vh->mr);
#endif

#if defined(LWS_WITH_UNIX_SOCK)
	if (LWS_UNIX_SOCK_ENABLED(vh)) {
		n = unlink(vh->iface);
		if (n)
			lwsl_vhost_info(vh, "Closing unix socket %s: errno %d\n",
				  vh->iface, errno);
	}
#endif
	/*
	 * although async event callbacks may still come for wsi handles with
	 * pending close in the case of asycn event library like libuv,
	 * they do not refer to the vhost.  So it's safe to free.
	 */

	if (vh->finalize)
		vh->finalize(vh, vh->finalize_arg);

#if defined(LWS_WITH_ABSTRACT)
	/*
	 * abstract instances
	 */

	lws_dll2_foreach_safe(&vh->abstract_instances_owner, NULL, destroy_ais);
#endif

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SYS_METRICS)
	lws_metric_destroy(&vh->mt_traffic_rx, 0);
	lws_metric_destroy(&vh->mt_traffic_tx, 0);
#endif

	lws_dll2_remove(&vh->vh_being_destroyed_list);

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_destroy(&vh->fic);
#endif
#if defined(LWS_WITH_TLS_JIT_TRUST)
	lws_sul_cancel(&vh->sul_unref);
#endif

	__lws_lc_untag(vh->context, &vh->lc);

	memset(vh, 0, sizeof(*vh));
	lws_free(vh);
}

/*
 * Starts the vhost destroy process
 *
 * Vhosts are not simple to deal with because they are an abstraction that
 * crosses SMP thread boundaries, a wsi on any pt can bind to any vhost.  If we
 * want another pt to do something to its wsis safely, we have to asynchronously
 * ask it to do it.
 *
 * In addition, with event libs, closing any handles (which are bound to vhosts
 * in their wsi) can happens asynchronously, so we can't just linearly do some
 * cleanup flow and free it in one step.
 *
 * The vhost destroy is cut into two pieces:
 *
 * 1) dispose of the listen socket, either by passing it on to another vhost
 *    that was already sharing it, or just closing it.
 *
 *    If any wsi bound to the vhost, mark the vhost as in the process of being
 *    destroyed, triggering each pt to close all wsi bound to the vhost next
 *    time around the event loop.  Call lws_cancel_service() so all the pts wake
 *    to deal with this without long poll waits making delays.
 *
 * 2) When the number of wsis bound to the vhost reaches zero, do the final
 *    vhost destroy flow, this can be triggered from any pt.
 */

void
lws_vhost_destroy(struct lws_vhost *vh)
{
	struct lws_context *context = vh->context;

	lws_context_lock(context, __func__); /* ------ context { */

	/* dispose of the listen socket one way or another */
	lws_vhost_destroy1(vh);

	/* start async closure of all wsi on this pt thread attached to vh */
	__lws_vhost_destroy_pt_wsi_dieback_start(vh);

	lwsl_vhost_info(vh, "count_bound_wsi %d", vh->count_bound_wsi);

	/* if there are none, finalize now since no further chance */
	if (!vh->count_bound_wsi) {
		__lws_vhost_destroy2(vh);

		goto out;
	}

	/*
	 * We have some wsi bound to this vhost, we have to wait for these to
	 * complete close and unbind before progressing the vhost removal.
	 *
	 * When the last bound wsi on this vh is destroyed we will auto-call
	 * __lws_vhost_destroy2() to finalize vh destruction
	 */

#if LWS_MAX_SMP > 1
	/* alert other pts they also need to do dieback flow for their wsi */
	lws_cancel_service(context);
#endif

out:
	lws_context_unlock(context); /* } context ------------------- */
}


void *
lws_vhost_user(struct lws_vhost *vhost)
{
	return vhost->user;
}

int
lws_get_vhost_listen_port(struct lws_vhost *vhost)
{
	return vhost->listen_port;
}

#if defined(LWS_WITH_SERVER)
void
lws_context_deprecate(struct lws_context *cx, lws_reload_func cb)
{
	struct lws_vhost *vh = cx->vhost_list;

	/*
	 * "deprecation" means disable the cx from accepting any new
	 * connections and free up listen sockets to be used by a replacement
	 * cx.
	 *
	 * Otherwise the deprecated cx remains operational, until its
	 * number of connected sockets falls to zero, when it is deleted.
	 *
	 * So, for each vhost, close his listen sockets
	 */

	while (vh) {

		lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
					   lws_dll2_get_head(&vh->listen_wsi)) {
			struct lws *wsi = lws_container_of(d, struct lws,
							   listen_list);

			wsi->socket_is_permanently_unusable = 1;
			lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
					   __func__);
			cx->deprecation_pending_listen_close_count++;

		} lws_end_foreach_dll_safe(d, d1);

		vh = vh->vhost_next;
	}

	cx->deprecated = 1;
	cx->deprecation_cb = cb;
}
#endif

#if defined(LWS_WITH_NETWORK)

struct lws_vhost *
lws_get_vhost_by_name(struct lws_context *context, const char *name)
{
	lws_start_foreach_ll(struct lws_vhost *, v,
			     context->vhost_list) {
		if (!v->being_destroyed && !strcmp(v->name, name))
			return v;

	} lws_end_foreach_ll(v, vhost_next);

	return NULL;
}


#if defined(LWS_WITH_CLIENT)
/*
 * This is the logic checking to see if the new connection wsi should have a
 * pipelining or muxing relationship with an existing "active connection" to
 * the same endpoint under the same conditions.
 *
 * This was originally in the client code but since the list is held on the
 * vhost (to ensure the same client tls ctx is involved) it's cleaner in vhost.c
 *
 * ACTIVE_CONNS_QUEUED: We're queued on an active connection, set *nwsi to that
 * ACTIVE_CONNS_MUXED: We are joining an active mux conn *nwsi as a child
 * ACTIVE_CONNS_SOLO: There's no existing conn to join either way
 */

int
lws_vhost_active_conns(struct lws *wsi, struct lws **nwsi, const char *adsin)
{
#if defined(LWS_WITH_TLS)
	const char *my_alpn = lws_wsi_client_stash_item(wsi, CIS_ALPN,
							_WSI_TOKEN_CLIENT_ALPN);
#endif
#if defined(LWS_WITH_TLS)
	char newconn_cannot_use_h1 = 0;

	if ((wsi->tls.use_ssl & LCCSCF_USE_SSL) &&
	    my_alpn && !strstr(my_alpn, "http/1.1"))
		/*
		 * new guy wants to use tls, he specifies the alpn and he does
		 * not list h1 as a choice ==> he can't bind to existing h1
		 */
		newconn_cannot_use_h1 = 1;
#endif

	if (!lws_dll2_is_detached(&wsi->dll2_cli_txn_queue)) {
		struct lws *w = lws_container_of(
				wsi->dll2_cli_txn_queue.owner, struct lws,
				dll2_cli_txn_queue_owner);
		*nwsi = w;

		return ACTIVE_CONNS_QUEUED;
	}

#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
	if (wsi->mux.parent_wsi) {
		/*
		 * We already decided...
		 */

		*nwsi = wsi->mux.parent_wsi;

		return ACTIVE_CONNS_MUXED;
	}
#endif

	lws_context_lock(wsi->a.context, __func__); /* -------------- cx { */
	lws_vhost_lock(wsi->a.vhost); /* ----------------------------------- { */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   wsi->a.vhost->dll_cli_active_conns_owner.head) {
		struct lws *w = lws_container_of(d, struct lws,
						 dll_cli_active_conns);

		lwsl_wsi_debug(wsi, "check %s %s %s %d %d",
				    lws_wsi_tag(w), adsin,
				    w->cli_hostname_copy ? w->cli_hostname_copy :
							    "null",
				    wsi->c_port, w->c_port);

		if (w != wsi &&
		    /*
		     * "same internet protocol"... this is a bit tricky,
		     * since h2 start out as h1, and may stay at h1.
		     *
		     * But an idle h1 connection cannot be used by a connection
		     * request that doesn't have http/1.1 in its alpn list...
		     */
		    (w->role_ops == wsi->role_ops ||
		     (lwsi_role_http(w) && lwsi_role_http(wsi))) &&
		     /* ... same role, or at least both some kind of http */
		    w->cli_hostname_copy && !strcmp(adsin, w->cli_hostname_copy) &&
		    /* same endpoint hostname */
#if defined(LWS_WITH_TLS)
		   !(newconn_cannot_use_h1 && w->role_ops == &role_ops_h1) &&
		   /* if we can't use h1, old guy must not be h1 */
		    (wsi->tls.use_ssl & LCCSCF_USE_SSL) ==
		     (w->tls.use_ssl & LCCSCF_USE_SSL) &&
		     /* must both agree on tls use or not */
#endif
		    wsi->c_port == w->c_port) {
			/* same endpoint port */

			/*
			 * There's already an active connection.
			 *
			 * The server may have told the existing active
			 * connection that it doesn't support pipelining...
			 */
			if (w->keepalive_rejected) {
				lwsl_wsi_notice(w, "defeating pipelining");
				goto solo;
			}

#if defined(LWS_WITH_HTTP2)
			/*
			 * h2: if in usable state already: just use it without
			 *     going through the queue
			 */
			if (w->client_h2_alpn && w->client_mux_migrated &&
			    (lwsi_state(w) == LRS_H2_WAITING_TO_SEND_HEADERS ||
			     lwsi_state(w) == LRS_ESTABLISHED ||
			     lwsi_state(w) == LRS_IDLING)) {

				lwsl_wsi_notice(w, "just join h2 directly 0x%x",
						   lwsi_state(w));

				if (lwsi_state(w) == LRS_IDLING)
					_lws_generic_transaction_completed_active_conn(&w, 0);

				//lwsi_set_state(w, LRS_H1C_ISSUE_HANDSHAKE2);

				wsi->client_h2_alpn = 1;
				lws_wsi_h2_adopt(w, wsi);
				lws_vhost_unlock(wsi->a.vhost); /* } ---------- */
				lws_context_unlock(wsi->a.context); /* -------------- cx { */

				*nwsi = w;

				return ACTIVE_CONNS_MUXED;
			}
#endif

#if defined(LWS_ROLE_MQTT)
			/*
			 * MQTT: if in usable state already: just use it without
			 *	 going through the queue
			 */

			if (lwsi_role_mqtt(wsi) && w->client_mux_migrated &&
			    lwsi_state(w) == LRS_ESTABLISHED) {

				if (lws_wsi_mqtt_adopt(w, wsi)) {
					lwsl_wsi_notice(w, "join mqtt directly");
					lws_dll2_remove(&wsi->dll2_cli_txn_queue);
					wsi->client_mux_substream = 1;

					lws_vhost_unlock(wsi->a.vhost); /* } ---------- */
					lws_context_unlock(wsi->a.context); /* -------------- cx { */

					return ACTIVE_CONNS_MUXED;
				}
			}
#endif

			/*
			 * If the connection is viable but not yet in a usable
			 * state, let's attach ourselves to it and wait for it
			 * to get there or fail.
			 */

			lwsl_wsi_notice(wsi, "apply txn queue %s, state 0x%lx",
					     lws_wsi_tag(w),
					     (unsigned long)w->wsistate);
			/*
			 * ...let's add ourselves to his transaction queue...
			 * we are adding ourselves at the TAIL
			 */
			lws_dll2_add_tail(&wsi->dll2_cli_txn_queue,
					  &w->dll2_cli_txn_queue_owner);

			if (lwsi_state(w) == LRS_IDLING)
				_lws_generic_transaction_completed_active_conn(&w, 0);

			/*
			 * For eg, h1 next we'd pipeline our headers out on him,
			 * and wait for our turn at client transaction_complete
			 * to take over parsing the rx.
			 */
			lws_vhost_unlock(wsi->a.vhost); /* } ---------- */
			lws_context_unlock(wsi->a.context); /* -------------- cx { */

			*nwsi = w;

			return ACTIVE_CONNS_QUEUED;
		}

	} lws_end_foreach_dll_safe(d, d1);

solo:
	lws_vhost_unlock(wsi->a.vhost); /* } ---------------------------------- */
	lws_context_unlock(wsi->a.context); /* -------------- cx { */

	/* there is nobody already connected in the same way */

	return ACTIVE_CONNS_SOLO;
}
#endif
#endif

const char *
lws_vh_tag(struct lws_vhost *vh)
{
	return lws_lc_tag(&vh->lc);
}

struct lws_log_cx *
lwsl_vhost_get_cx(struct lws_vhost *vh)
{
	if (!vh)
		return NULL;

	return vh->lc.log_cx;
}

void
lws_log_prepend_vhost(struct lws_log_cx *cx, void *obj, char **p, char *e)
{
	struct lws_vhost *vh = (struct lws_vhost *)obj;

	*p += lws_snprintf(*p, lws_ptr_diff_size_t(e, (*p)), "%s: ",
							lws_vh_tag(vh));
}
