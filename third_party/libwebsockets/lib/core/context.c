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

#ifndef LWS_BUILD_HASH
#define LWS_BUILD_HASH "unknown-build-hash"
#endif

static const char *library_version = LWS_LIBRARY_VERSION;

#if defined(LWS_WITH_MBEDTLS)
extern const char *mbedtls_client_preload_filepath;
#endif

#if defined(LWS_HAVE_SYS_RESOURCE_H)
/* for setrlimit */
#include <sys/resource.h>
#endif

#if defined(LWS_WITH_NETWORK)
/* in ms */
static uint32_t default_backoff_table[] = { 1000, 3000, 9000, 17000 };
#endif

/**
 * lws_get_library_version: get version and git hash library built from
 *
 *	returns a const char * to a string like "1.1 178d78c"
 *	representing the library version followed by the git head hash it
 *	was built from
 */
const char *
lws_get_library_version(void)
{
	return library_version;
}

#if defined(LWS_WITH_NETWORK)

#if defined(LWS_WITH_SYS_STATE)

static const char * system_state_names[] = {
	"undef",
	"CONTEXT_CREATED",
	"INITIALIZED",
	"IFACE_COLDPLUG",
	"DHCP",
	"CPD_PRE_TIME",
	"TIME_VALID",
	"CPD_POST_TIME",
	"POLICY_VALID",
	"REGISTERED",
	"AUTH1",
	"AUTH2",
	"OPERATIONAL",
	"POLICY_INVALID",
	"DESTROYING"
};


/*
 * Handle provoking protocol init when we pass through the right system state
 */

static int
lws_state_notify_protocol_init(struct lws_state_manager *mgr,
			       struct lws_state_notify_link *link, int current,
			       int target)
{
	struct lws_context *context = lws_container_of(mgr, struct lws_context,
						       mgr_system);
#if defined(LWS_WITH_SECURE_STREAMS) && \
    defined(LWS_WITH_SECURE_STREAMS_SYS_AUTH_API_AMAZON_COM)
	lws_system_blob_t *ab0, *ab1;
#endif
	int n;

	/*
	 * Deal with any attachments that were waiting for the right state
	 * to come along
	 */

	for (n = 0; n < context->count_threads; n++)
		lws_system_do_attach(&context->pt[n]);

#if defined(LWS_WITH_SYS_DHCP_CLIENT)
	if (target == LWS_SYSTATE_DHCP) {
		/*
		 * Don't let it past here until at least one iface has been
		 * configured for operation with DHCP
		 */

		if (!lws_dhcpc_status(context, NULL))
			return 1;
	}
#endif

#if defined(LWS_WITH_SYS_NTPCLIENT)
	if (target == LWS_SYSTATE_TIME_VALID &&
	    lws_now_secs() < 1594017754) /* 06:42 Mon Jul 6 2020 UTC */ {
		lws_ntpc_trigger(context);

		return 1;
	}
#endif

#if defined(LWS_WITH_NETLINK)
	/*
	 * If we're going to use netlink routing data for DNS, we have to
	 * wait to collect it asynchronously from the platform first.  Netlink
	 * role init starts a ctx sul for 350ms (reset to 100ms each time some
	 * new netlink data comes) that sets nl_initial_done and tries to move
	 * us to OPERATIONAL
	 */

	if (target == LWS_SYSTATE_IFACE_COLDPLUG &&
	    context->netlink &&
	    !context->nl_initial_done) {
		lwsl_cx_info(context, "waiting for netlink coldplug");

		return 1;
	}
#endif

#if defined(LWS_WITH_SECURE_STREAMS) && \
    defined(LWS_WITH_SECURE_STREAMS_SYS_AUTH_API_AMAZON_COM)
	/*
	 * Skip this if we are running something without the policy for it
	 *
	 * If root token is empty, skip too.
	 */

	ab0 = lws_system_get_blob(context, LWS_SYSBLOB_TYPE_AUTH, 0);
	ab1 = lws_system_get_blob(context, LWS_SYSBLOB_TYPE_AUTH, 1);

	if (target == LWS_SYSTATE_AUTH1 &&
	    context->pss_policies && ab0 && ab1 &&
	    !lws_system_blob_get_size(ab0) &&
	    lws_system_blob_get_size(ab1)) {
		lwsl_cx_info(context,
			     "AUTH1 state triggering api.amazon.com auth");
		/*
		 * Start trying to acquire it if it's not already in progress
		 * returns nonzero if we determine it's not needed
		 */
		if (!lws_ss_sys_auth_api_amazon_com(context))
			return 1;
	}
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
#if defined(LWS_WITH_DRIVERS)
	/*
	 * See if we should do the SS Captive Portal Detection
	 */
	if (target == LWS_SYSTATE_CPD_PRE_TIME) {
		if (lws_system_cpd_state_get(context) == LWS_CPD_INTERNET_OK)
			return 0; /* allow it */

		/*
		 * Don't allow it to move past here until we get an IP and
		 * CPD passes, driven by SMD
		 */

		return 1;
	}
#endif

#if !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	/*
	 * Skip this if we are running something without the policy for it
	 */
	if (target == LWS_SYSTATE_POLICY_VALID &&
	    context->pss_policies && !context->policy_updated) {

		if (context->hss_fetch_policy)
			return 1;

		lwsl_cx_debug(context, "starting policy fetch");
		/*
		 * Start trying to acquire it if it's not already in progress
		 * returns nonzero if we determine it's not needed
		 */
		if (!lws_ss_sys_fetch_policy(context))
			/* we have it */
			return 0;

		/* deny while we fetch it */

		return 1;
	}
#endif
#endif

	/* protocol part */

	if (context->protocol_init_done)
		return 0;

	if (target != LWS_SYSTATE_POLICY_VALID)
		return 0;

	lwsl_cx_info(context, "doing protocol init on POLICY_VALID\n");

	return lws_protocol_init(context);
}

static void
lws_context_creation_completion_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context *context = lws_container_of(sul, struct lws_context,
						       sul_system_state);

	/* if nothing is there to intercept anything, go all the way */
	lws_state_transition_steps(&context->mgr_system,
				   LWS_SYSTATE_OPERATIONAL);
}
#endif /* WITH_SYS_STATE */

#if defined(LWS_WITH_SYS_SMD)
static int
lws_system_smd_cb(void *opaque, lws_smd_class_t _class, lws_usec_t timestamp,
		  void *buf, size_t len)
{
	struct lws_context *cx = (struct lws_context *)opaque;

	if (_class != LWSSMDCL_NETWORK)
		return 0;

	/* something external requested CPD check */

	if (!lws_json_simple_strcmp(buf, len, "\"trigger\":", "cpdcheck"))
		lws_system_cpd_start(cx);
	else
		/*
		 * IP acquisition on any interface triggers captive portal
		 * check on default route
		 */
		if (!lws_json_simple_strcmp(buf, len, "\"type\":", "ipacq"))
			lws_system_cpd_start(cx);

#if defined(LWS_WITH_SYS_NTPCLIENT)
	/*
	 * Captive portal detect showing internet workable triggers NTP Client
	 */
	if (!lws_json_simple_strcmp(buf, len, "\"type\":", "cps") &&
	    !lws_json_simple_strcmp(buf, len, "\"result\":", "OK") &&
	    lws_now_secs() < 1594017754) /* 06:42 Mon Jul 6 2020 UTC */
		lws_ntpc_trigger(cx);
#endif

#if defined(LWS_WITH_SYS_DHCP_CLIENT) && 0
	/*
	 * Any network interface linkup triggers DHCP
	 */
	if (!lws_json_simple_strcmp(buf, len, "\"type\":", "linkup"))
		lws_ntpc_trigger(cx);

#endif

#if defined(LWS_WITH_DRIVERS) && defined(LWS_WITH_NETWORK)
	lws_netdev_smd_cb(opaque, _class, timestamp, buf, len);
#endif

	return 0;
}
#endif



#endif /* NETWORK */

#if !defined(LWS_WITH_NO_LOGS)

static const char * const opts_str =
#if defined(LWS_WITH_NETWORK)
			"NET "
#else
			"NoNET "
#endif
#if defined(LWS_WITH_CLIENT)
			"CLI "
#endif
#if defined(LWS_WITH_SERVER)
			"SRV "
#endif
#if defined(LWS_ROLE_H1)
			"H1 "
#endif
#if defined(LWS_ROLE_H2)
			"H2 "
#endif
#if defined(LWS_ROLE_WS)
			"WS "
#endif
#if defined(LWS_ROLE_MQTT)
			"MQTT "
#endif
#if defined(LWS_WITH_SECURE_STREAMS) && !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
			"SS-JSON-POL "
#endif
#if defined(LWS_WITH_SECURE_STREAMS) && defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
			"SS-STATIC-POL "
#endif
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
			"SSPROX "
#endif

#if defined(LWS_WITH_MBEDTLS)
			"MbedTLS "
#endif
#if defined(LWS_WITH_CONMON)
			"ConMon "
#endif
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
			"FLTINJ "
#endif
#if defined(LWS_WITH_SYS_ASYNC_DNS)
			"ASYNC_DNS "
#endif
#if defined(LWS_WITH_SYS_NTPCLIENT)
			"NTPCLIENT "
#endif
#if defined(LWS_WITH_SYS_DHCP_CLIENT)
			"DHCP_CLIENT "
#endif
;

#endif

#if defined(LWS_WITH_EVLIB_PLUGINS) && defined(LWS_WITH_EVENT_LIBS)
static const struct lws_evlib_map {
	uint64_t	flag;
	const char	*name;
} map[] = {
	{ LWS_SERVER_OPTION_LIBUV,    "evlib_uv" },
	{ LWS_SERVER_OPTION_LIBEVENT, "evlib_event" },
	{ LWS_SERVER_OPTION_GLIB,     "evlib_glib" },
	{ LWS_SERVER_OPTION_LIBEV,    "evlib_ev" },
	{ LWS_SERVER_OPTION_SDEVENT,  "evlib_sd" },
	{ LWS_SERVER_OPTION_ULOOP,    "evlib_uloop" },
};
static const char * const dlist[] = {
	".",				/* Priority 1: plugins in cwd */
	LWS_INSTALL_LIBDIR,		/* Priority 2: plugins in install dir */
	NULL
};
#endif

struct lws_context *
lws_create_context(const struct lws_context_creation_info *info)
{
	struct lws_context *context = NULL;
#if !defined(LWS_WITH_NO_LOGS)
	const char *s = "IPv6-absent";
#endif
#if defined(LWS_WITH_FILE_OPS)
	struct lws_plat_file_ops *prev;
#endif
#ifndef LWS_NO_DAEMONIZE
	pid_t pid_daemon = get_daemonize_pid();
#endif
#if defined(LWS_WITH_NETWORK)
	const lws_plugin_evlib_t *plev = NULL;
	unsigned short count_threads = 1;
	uint8_t *u;
	uint16_t us_wait_resolution = 0;
#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
	struct lws_cache_creation_info ci;
#endif

#if defined(__ANDROID__)
	struct rlimit rt;
#endif
	size_t
#if defined(LWS_PLAT_FREERTOS)
		/* smaller default, can set in info->pt_serv_buf_size */
		s1 = 2048,
#else
		s1 = 4096,
#endif
		size = sizeof(struct lws_context);
#endif

	int n;
	unsigned int lpf = info->fd_limit_per_thread;
#if defined(LWS_WITH_EVLIB_PLUGINS) && defined(LWS_WITH_EVENT_LIBS)
	struct lws_plugin		*evlib_plugin_list = NULL;
#if defined(_DEBUG) && !defined(LWS_WITH_NO_LOGS)
	char		*ld_env;
#endif
#endif
#if defined(LWS_WITH_LIBUV)
	char fatal_exit_defer = 0;
#endif


	if (lws_fi(&info->fic, "ctx_createfail1"))
		goto early_bail;

	if (lpf) {
		lpf+= 2;
#if defined(LWS_WITH_SYS_ASYNC_DNS)
		lpf++;
#endif
#if defined(LWS_WITH_SYS_NTPCLIENT)
		lpf++;
#endif
#if defined(LWS_WITH_SYS_DHCP_CLIENT)
		lpf++;
#endif
	}

#if defined(LWS_WITH_IPV6) && !defined(LWS_WITH_NO_LOGS)
	if (!lws_check_opt(info->options, LWS_SERVER_OPTION_DISABLE_IPV6))
		s = "IPV6-on";
	else
		s = "IPV6-off";
#endif

	if (lws_plat_context_early_init())
		goto early_bail;

#if defined(LWS_WITH_NETWORK)
	if (info->count_threads)
		count_threads = (unsigned short)info->count_threads;

	if (count_threads > LWS_MAX_SMP)
		count_threads = LWS_MAX_SMP;

	if (info->pt_serv_buf_size)
		s1 = info->pt_serv_buf_size;

	/* pt fakewsi and the pt serv buf allocations ride after the context */
	size += count_threads * s1;
#if !defined(LWS_PLAT_FREERTOS)
	size += (count_threads * sizeof(struct lws));
#endif

	if (info->event_lib_custom) {
		plev = info->event_lib_custom;
		us_wait_resolution = 0;
	}
#if defined(LWS_WITH_POLL)
	else {
		extern const lws_plugin_evlib_t evlib_poll;
		plev = &evlib_poll;
#if !defined(LWS_PLAT_FREERTOS)
		/*
		 * ... freertos has us-resolution select()...
		 * others are to ms-resolution poll()
		 */
		us_wait_resolution = 1000;
#endif
	}
#endif

#if defined(LWS_WITH_EVLIB_PLUGINS) && defined(LWS_WITH_EVENT_LIBS)

	/*
	 * New style dynamically loaded event lib support
	 *
	 * We have to pick and load the event lib plugin before we allocate
	 * the context object, so we can overallocate it correctly
	 */

#if defined(_DEBUG) && !defined(LWS_WITH_NO_LOGS)
	ld_env = getenv("LD_LIBRARY_PATH");
	lwsl_info("%s: ev lib path %s, '%s'\n", __func__,
			LWS_INSTALL_LIBDIR, ld_env);
#endif

	for (n = 0; n < (int)LWS_ARRAY_SIZE(map); n++) {
		char ok = 0;

		if (!lws_check_opt(info->options, map[n].flag))
			continue;

		if (!lws_plugins_init(&evlib_plugin_list,
				     dlist, "lws_evlib_plugin",
				     map[n].name, NULL, NULL))
			ok = 1;

		if (!ok || lws_fi(&info->fic, "ctx_createfail_plugin_init")) {
			lwsl_err("%s: failed to load %s\n", __func__,
					map[n].name);
			goto bail;
		}

#if defined(LWS_WITH_LIBUV)
		if (!n) /* libuv */
			fatal_exit_defer = !!info->foreign_loops;
#endif

		if (!evlib_plugin_list ||
		    lws_fi(&info->fic, "ctx_createfail_evlib_plugin")) {
			lwsl_err("%s: unable to load evlib plugin %s\n",
					__func__, map[n].name);

			goto bail;
		}
		plev = (const lws_plugin_evlib_t *)evlib_plugin_list->hdr;
		break;
	}
#else
#if defined(LWS_WITH_EVENT_LIBS)
	/*
	 * set the context event loops ops struct
	 *
	 * after this, all event_loop actions use the generic ops
	 */

	/*
	 * oldstyle built-in event lib support
	 *
	 * We have composed them into the libwebsockets lib itself, we can
	 * just pick the ops we want and done
	 */

#if defined(LWS_WITH_LIBUV)
	if (lws_check_opt(info->options, LWS_SERVER_OPTION_LIBUV)) {
		extern const lws_plugin_evlib_t evlib_uv;
		plev = &evlib_uv;
		fatal_exit_defer = !!info->foreign_loops;
		us_wait_resolution = 0;
	}
#endif

#if defined(LWS_WITH_LIBEVENT)
	if (lws_check_opt(info->options, LWS_SERVER_OPTION_LIBEVENT)) {
		extern const lws_plugin_evlib_t evlib_event;
		plev = &evlib_event;
		us_wait_resolution = 0;
	}
#endif

#if defined(LWS_WITH_GLIB)
	if (lws_check_opt(info->options, LWS_SERVER_OPTION_GLIB)) {
		extern const lws_plugin_evlib_t evlib_glib;
		plev = &evlib_glib;
		us_wait_resolution = 0;
	}
#endif

#if defined(LWS_WITH_LIBEV)
	if (lws_check_opt(info->options, LWS_SERVER_OPTION_LIBEV)) {
		extern const lws_plugin_evlib_t evlib_ev;
		plev = &evlib_ev;
		us_wait_resolution = 0;
	}
#endif

#if defined(LWS_WITH_SDEVENT)
    if (lws_check_opt(info->options, LWS_SERVER_OPTION_SDEVENT)) {
        extern const lws_plugin_evlib_t evlib_sd;
        plev = &evlib_sd;
        us_wait_resolution = 0;
    }
#endif

#if defined(LWS_WITH_ULOOP)
    if (lws_check_opt(info->options, LWS_SERVER_OPTION_ULOOP)) {
        extern const lws_plugin_evlib_t evlib_uloop;
        plev = &evlib_uloop;
        us_wait_resolution = 0;
    }
#endif

#endif /* with event libs */

#endif /* not with ev plugins */

	if (!plev || lws_fi(&info->fic, "ctx_createfail_evlib_sel"))
		goto fail_event_libs;

#if defined(LWS_WITH_NETWORK)
	size += (size_t)plev->ops->evlib_size_ctx /* the ctx evlib priv */ +
		(count_threads * (size_t)plev->ops->evlib_size_pt) /* the pt evlib priv */;
#endif

	context = lws_zalloc(size, "context");
	if (!context || lws_fi(&info->fic, "ctx_createfail_oom_ctx")) {
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
		lws_free(context);
#endif
		lwsl_err("OOM");
		goto early_bail;
	}

#if defined(LWS_WITH_NETWORK)
	context->event_loop_ops = plev->ops;
	context->us_wait_resolution = us_wait_resolution;
#if defined(LWS_WITH_TLS_JIT_TRUST)
	{
		struct lws_cache_creation_info ci;

		memset(&ci, 0, sizeof(ci));
		ci.cx = context;
		ci.ops = &lws_cache_ops_heap;
		ci.name = "jitt";
		ci.max_footprint = info->jitt_cache_max_footprint;
		context->trust_cache = lws_cache_create(&ci);
	}
#endif
#endif
#if defined(LWS_WITH_EVENT_LIBS)
	/* at the very end */
	context->evlib_ctx = (uint8_t *)context + size -
					plev->ops->evlib_size_ctx;
#endif
#if defined(LWS_WITH_EVLIB_PLUGINS) && defined(LWS_WITH_EVENT_LIBS)
	context->evlib_plugin_list = evlib_plugin_list;
#endif

#if !defined(LWS_PLAT_FREERTOS)
	context->uid = info->uid;
	context->gid = info->gid;
	context->username = info->username;
	context->groupname = info->groupname;
#endif
	context->name			= info->vhost_name;
	if (info->log_cx)
		context->log_cx = info->log_cx;
	else
		context->log_cx = &log_cx;
	lwsl_refcount_cx(context->log_cx, 1);

	context->system_ops = info->system_ops;
	context->pt_serv_buf_size = (unsigned int)s1;
	context->protocols_copy = info->protocols;
#if defined(LWS_WITH_TLS_JIT_TRUST)
	context->vh_idle_grace_ms = info->vh_idle_grace_ms ?
			info->vh_idle_grace_ms : 5000;
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	context->fic.name = "ctx";
	if (info->fic.fi_owner.count)
		/*
		 * This moves all the lws_fi_t from info->fi to the context fi,
		 * leaving it empty, so no injection added to default vhost
		 */
		lws_fi_import(&context->fic, &info->fic);
#endif


#if defined(LWS_WITH_SYS_SMD)
	context->smd_ttl_us = info->smd_ttl_us ? info->smd_ttl_us :
#if defined(LWS_PLAT_FREERTOS)
			5000000;
#else
			2000000;
#endif
	context->smd_queue_depth = (uint16_t)(info->smd_queue_depth ?
						info->smd_queue_depth :
#if defined(LWS_PLAT_FREERTOS)
						20);
#else
						40);
#endif
#endif

#if defined(LWS_WITH_NETWORK)
	context->lcg[LWSLCG_WSI].tag_prefix = "wsi";
	context->lcg[LWSLCG_VHOST].tag_prefix = "vh";
	context->lcg[LWSLCG_WSI_SERVER].tag_prefix = "wsisrv"; /* adopted */

#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
	context->lcg[LWSLCG_WSI_MUX].tag_prefix = "mux"; /* a mux child wsi */
#endif

#if defined(LWS_WITH_CLIENT)
	context->lcg[LWSLCG_WSI_CLIENT].tag_prefix = "wsicli";
#endif

#if defined(LWS_WITH_SECURE_STREAMS)
#if defined(LWS_WITH_CLIENT)
	context->lcg[LWSLCG_SS_CLIENT].tag_prefix = "SScli";
#endif
#if defined(LWS_WITH_SERVER)
	context->lcg[LWSLCG_SS_SERVER].tag_prefix = "SSsrv";
#endif
#if defined(LWS_WITH_CLIENT)
	context->lcg[LWSLCG_WSI_SS_CLIENT].tag_prefix = "wsiSScli";
#endif
#if defined(LWS_WITH_SERVER)
	context->lcg[LWSLCG_WSI_SS_SERVER].tag_prefix = "wsiSSsrv";
#endif
#endif
#endif

#if defined(LWS_WITH_SYS_METRICS)
	/*
	 * If we're not using secure streams, we can still pass in a linked-
	 * list of metrics policies
	 */
	context->metrics_policies = info->metrics_policies;
	context->metrics_prefix = info->metrics_prefix;

	context->mt_service = lws_metric_create(context,
					LWSMTFL_REPORT_DUTY_WALLCLOCK_US |
					LWSMTFL_REPORT_ONLY_GO, "cpu.svc");

#if defined(LWS_WITH_CLIENT)

	context->mt_conn_dns = lws_metric_create(context,
						 LWSMTFL_REPORT_MEAN |
						 LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
						 "n.cn.dns");
	context->mt_conn_tcp = lws_metric_create(context,
						 LWSMTFL_REPORT_MEAN |
						 LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
						 "n.cn.tcp");
	context->mt_conn_tls = lws_metric_create(context,
						 LWSMTFL_REPORT_MEAN |
						 LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
						 "n.cn.tls");
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	context->mt_http_txn = lws_metric_create(context,
						 LWSMTFL_REPORT_MEAN |
						 LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
						 "n.http.txn");
#endif

	context->mth_conn_failures = lws_metric_create(context,
					LWSMTFL_REPORT_HIST, "n.cn.failures");

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	context->mt_adns_cache = lws_metric_create(context,
						   LWSMTFL_REPORT_MEAN |
						   LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
						   "n.cn.adns");
#endif
#if defined(LWS_WITH_SECURE_STREAMS)
	context->mth_ss_conn = lws_metric_create(context, LWSMTFL_REPORT_HIST,
						 "n.ss.conn");
#endif
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	context->mt_ss_cliprox_conn = lws_metric_create(context,
			LWSMTFL_REPORT_HIST,
							"n.ss.cliprox.conn");
	context->mt_ss_cliprox_paylat = lws_metric_create(context,
							  LWSMTFL_REPORT_MEAN |
							  LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
							  "n.ss.cliprox.paylat");
	context->mt_ss_proxcli_paylat = lws_metric_create(context,
							  LWSMTFL_REPORT_MEAN |
							  LWSMTFL_REPORT_DUTY_WALLCLOCK_US,
							  "n.ss.proxcli.paylat");
#endif

#endif /* network + metrics + client */

#if defined(LWS_WITH_SERVER)
	context->mth_srv = lws_metric_create(context,
					     LWSMTFL_REPORT_HIST, "n.srv");
#endif /* network + metrics + server */

#endif /* network + metrics */

#endif /* network */

	lwsl_cx_notice(context, "LWS: %s, %s%s", library_version, opts_str, s);
#if defined(LWS_WITH_NETWORK)
	lwsl_cx_info(context, "Event loop: %s", plev->ops->name);
#endif

	/*
	 * Proxy group
	 */

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
#if defined(LWS_WITH_CLIENT)
	context->lcg[LWSLCG_SSP_CLIENT].tag_prefix = "SSPcli";
#endif
#if defined(LWS_WITH_SERVER)
	context->lcg[LWSLCG_SSP_ONWARD].tag_prefix = "SSPonw";
#endif
#if defined(LWS_WITH_CLIENT)
	context->lcg[LWSLCG_WSI_SSP_CLIENT].tag_prefix = "wsiSSPcli";
#endif
#if defined(LWS_WITH_SERVER)
	context->lcg[LWSLCG_WSI_SSP_ONWARD].tag_prefix = "wsiSSPonw";
#endif
#endif


#if defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	/* directly use the user-provided policy object list */
	context->pss_policies = info->pss_policies;
#endif

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API) && defined(LWS_WITH_CLIENT)
	context->ss_proxy_bind = info->ss_proxy_bind;
	context->ss_proxy_port = info->ss_proxy_port;
	context->ss_proxy_address = info->ss_proxy_address;
	if (context->ss_proxy_bind && context->ss_proxy_address)
		lwsl_cx_notice(context, "ss proxy bind '%s', port %d, ads '%s'",
			context->ss_proxy_bind, context->ss_proxy_port,
			context->ss_proxy_address);
#endif

#if defined(LWS_WITH_NETWORK)
	context->undestroyed_threads = count_threads;
	context->count_threads = count_threads;

#if defined(LWS_ROLE_WS) && defined(LWS_WITHOUT_EXTENSIONS)
        if (info->extensions)
                lwsl_cx_warn(context, "WITHOUT_EXTENSIONS but exts ptr set");
#endif
#endif /* network */

#if defined(LWS_WITH_SECURE_STREAMS)
#if !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	context->pss_policies_json = info->pss_policies_json;
#endif
#if defined(LWS_WITH_SSPLUGINS)
	context->pss_plugins = info->pss_plugins;
#endif
#endif

	/* if he gave us names, set the uid / gid */
	if (lws_plat_drop_app_privileges(context, 0) ||
	    lws_fi(&context->fic, "ctx_createfail_privdrop"))
		goto free_context_fail2;

#if defined(LWS_WITH_TLS) && defined(LWS_WITH_NETWORK)
#if defined(LWS_WITH_MBEDTLS)
	context->tls_ops = &tls_ops_mbedtls;

	mbedtls_client_preload_filepath = info->mbedtls_client_preload_filepath;
#else
	context->tls_ops = &tls_ops_openssl;
#endif
#endif

#if LWS_MAX_SMP > 1
	lws_mutex_refcount_init(&context->mr);
#endif

#if defined(LWS_PLAT_FREERTOS)
#if defined(LWS_AMAZON_RTOS)
	context->last_free_heap = xPortGetFreeHeapSize();
#else
	context->last_free_heap = esp_get_free_heap_size();
#endif
#endif

#if defined(LWS_WITH_FILE_OPS)
	/* default to just the platform fops implementation */

	context->fops_platform.LWS_FOP_OPEN	= _lws_plat_file_open;
	context->fops_platform.LWS_FOP_CLOSE	= _lws_plat_file_close;
	context->fops_platform.LWS_FOP_SEEK_CUR	= _lws_plat_file_seek_cur;
	context->fops_platform.LWS_FOP_READ	= _lws_plat_file_read;
	context->fops_platform.LWS_FOP_WRITE	= _lws_plat_file_write;
	context->fops_platform.fi[0].sig	= NULL;

	/*
	 *  arrange a linear linked-list of fops starting from context->fops
	 *
	 * platform fops
	 * [ -> fops_zip (copied into context so .next settable) ]
	 * [ -> info->fops ]
	 */

	context->fops = &context->fops_platform;
	prev = (struct lws_plat_file_ops *)context->fops;

#if defined(LWS_WITH_ZIP_FOPS)
	/* make a soft copy so we can set .next */
	context->fops_zip = fops_zip;
	prev->next = &context->fops_zip;
	prev = (struct lws_plat_file_ops *)prev->next;
#endif

	/* if user provided fops, tack them on the end of the list */
	if (info->fops)
		prev->next = info->fops;
#endif

#if defined(LWS_WITH_SERVER)
	context->reject_service_keywords = info->reject_service_keywords;
#endif
	if (info->external_baggage_free_on_destroy)
		context->external_baggage_free_on_destroy =
			info->external_baggage_free_on_destroy;
#if defined(LWS_WITH_NETWORK)
	context->time_up = lws_now_usecs();
#endif
	context->pcontext_finalize = info->pcontext;

#if defined(LWS_WITH_TLS) && defined(LWS_WITH_NETWORK)
	context->simultaneous_ssl_restriction =
			info->simultaneous_ssl_restriction;
	context->simultaneous_ssl_handshake_restriction =
			info->simultaneous_ssl_handshake_restriction;
#endif

	context->options = info->options;

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE) && !defined(WIN32)
	/*
	 * If asked, try to set the rlimit / ulimit for process sockets / files.
	 * We read the effective limit in a moment, so we will find out the
	 * real limit according to system constraints then.
	 */
	if (info->rlimit_nofile) {
		struct rlimit rl;

		rl.rlim_cur = (unsigned int)info->rlimit_nofile;
		rl.rlim_max = (unsigned int)info->rlimit_nofile;
		setrlimit(RLIMIT_NOFILE, &rl);
	}
#endif

#ifndef LWS_NO_DAEMONIZE
	if (pid_daemon) {
		context->started_with_parent = pid_daemon;
		lwsl_cx_info(context, " Started with daemon pid %u",
				(unsigned int)pid_daemon);
	}
#endif
#if defined(__ANDROID__)
	n = getrlimit(RLIMIT_NOFILE, &rt);
	if (n == -1) {
		lwsl_cx_err(context, "Get RLIMIT_NOFILE failed!");

		goto free_context_fail2;
	}
	context->max_fds = (unsigned int)rt.rlim_cur;
#else
#if defined(WIN32) || defined(_WIN32) || defined(LWS_AMAZON_RTOS) || defined(LWS_ESP_PLATFORM)
	context->max_fds = getdtablesize();
#else
	{
		long l = sysconf(_SC_OPEN_MAX);

		context->max_fds = 2560;

		if (l > 10000000)
			lwsl_cx_warn(context, "unreasonable ulimit -n workaround");
		else
			if (l != -1l)
				context->max_fds = (unsigned int)l;
	}
#endif
	if ((int)context->max_fds < 0 ||
	     lws_fi(&context->fic, "ctx_createfail_maxfds")) {
		lwsl_cx_err(context, "problem getting process max files");

		goto free_context_fail2;
	}
#endif

	/*
	 * deal with any max_fds override, if it's reducing (setting it to
	 * more than ulimit -n is meaningless).  The platform init will
	 * figure out what if this is something it can deal with.
	 */
	if (info->fd_limit_per_thread) {
		unsigned int mf = lpf * context->count_threads;

		if (mf < context->max_fds) {
			context->max_fds_unrelated_to_ulimit = 1;
			context->max_fds = mf;
		}
	}

#if defined(LWS_WITH_NETWORK)
	context->token_limits = info->token_limits;
#endif


#if defined(LWS_WITH_TLS) && defined(LWS_WITH_NETWORK)
	time(&context->tls.last_cert_check_s);
	if (info->alpn)
		context->tls.alpn_default = info->alpn;
	else {
		char *p = context->tls.alpn_discovered, first = 1;

		LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar) {
			if (ar->alpn) {
				if (!first)
					*p++ = ',';
				p += lws_snprintf(p, (unsigned int)(
					(context->tls.alpn_discovered +
					sizeof(context->tls.alpn_discovered) -
					2) - p), "%s", ar->alpn);
				first = 0;
			}
		} LWS_FOR_EVERY_AVAILABLE_ROLE_END;

		context->tls.alpn_default = context->tls.alpn_discovered;
	}

#endif
#if defined(LWS_WITH_NETWORK)
	if (info->timeout_secs)
		context->timeout_secs = info->timeout_secs;
	else
#endif
		context->timeout_secs = 15;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	if (info->max_http_header_data)
		context->max_http_header_data = info->max_http_header_data;
	else
		if (info->max_http_header_data2)
			context->max_http_header_data =
					(unsigned short)info->max_http_header_data2;
		else
			context->max_http_header_data = LWS_DEF_HEADER_LEN;

	if (info->max_http_header_pool)
		context->max_http_header_pool = info->max_http_header_pool;
	else
		if (info->max_http_header_pool2)
			context->max_http_header_pool =
					(unsigned short)info->max_http_header_pool2;
		else
			context->max_http_header_pool = context->max_fds;
#endif

	if (info->fd_limit_per_thread)
		context->fd_limit_per_thread = lpf;
	else
		if (context->count_threads)
			context->fd_limit_per_thread = context->max_fds /
							context->count_threads;

#if defined(LWS_WITH_SYS_SMD)
	lws_mutex_init(context->smd.lock_messages);
	lws_mutex_init(context->smd.lock_peers);

	/* lws_system smd participant */

	if (!lws_smd_register(context, context, 0, LWSSMDCL_NETWORK,
			      lws_system_smd_cb)) {
		lwsl_cx_err(context, "early smd register failed");
	}

	/* user smd participant */

	if (info->early_smd_cb &&
	    !lws_smd_register(context, info->early_smd_opaque, 0,
			      info->early_smd_class_filter,
			      info->early_smd_cb)) {
		lwsl_cx_err(context, "early smd register failed");
	}
#endif

	n = 0;
#if defined(LWS_WITH_NETWORK)

	context->default_retry.retry_ms_table = default_backoff_table;
	context->default_retry.conceal_count =
			context->default_retry.retry_ms_table_count =
					LWS_ARRAY_SIZE(default_backoff_table);
	context->default_retry.jitter_percent = 20;
	context->default_retry.secs_since_valid_ping = 300;
	context->default_retry.secs_since_valid_hangup = 310;

	if (info->retry_and_idle_policy &&
	    info->retry_and_idle_policy->secs_since_valid_ping) {
		context->default_retry.secs_since_valid_ping =
				info->retry_and_idle_policy->secs_since_valid_ping;
		context->default_retry.secs_since_valid_hangup =
				info->retry_and_idle_policy->secs_since_valid_hangup;
	}

	/*
	 * Allocate the per-thread storage for scratchpad buffers,
	 * and header data pool
	 */
	u = (uint8_t *)&context[1];
	for (n = 0; n < context->count_threads; n++) {
		context->pt[n].serv_buf = u;
		u += context->pt_serv_buf_size;

		context->pt[n].context = context;
		context->pt[n].tid = (uint8_t)n;

#if !defined(LWS_PLAT_FREERTOS)
		/*
		 * We overallocated for a fakewsi (can't compose it in the
		 * pt because size isn't known at that time).  point to it
		 * and zero it down.  Fakewsis are needed to make callbacks work
		 * when the source of the callback is not actually from a wsi
		 * context.
		 */
		context->pt[n].fake_wsi = (struct lws *)u;
		u += sizeof(struct lws);

		memset(context->pt[n].fake_wsi, 0, sizeof(struct lws));
#endif

		context->pt[n].evlib_pt = u;
		u += plev->ops->evlib_size_pt;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
		context->pt[n].http.ah_list = NULL;
		context->pt[n].http.ah_pool_length = 0;
#endif
		lws_pt_mutex_init(&context->pt[n]);
#if defined(LWS_WITH_SEQUENCER)
		lws_seq_pt_init(&context->pt[n]);
#endif

#if defined(LWS_WITH_CGI)
		if (lws_rops_fidx(&role_ops_cgi, LWS_ROPS_pt_init_destroy))
			(lws_rops_func_fidx(&role_ops_cgi, LWS_ROPS_pt_init_destroy)).
				pt_init_destroy(context, info,
						&context->pt[n], 0);
#endif
	}

	if (!info->ka_interval && info->ka_time > 0) {
		lwsl_cx_err(context, "info->ka_interval can't be 0 if ka_time used");
		goto free_context_fail;
	}

#if defined(LWS_WITH_PEER_LIMITS)
	/* scale the peer hash table according to the max fds for the process,
	 * so that the max list depth averages 16.  Eg, 1024 fd -> 64,
	 * 102400 fd -> 6400
	 */

	context->pl_hash_elements =
		(context->count_threads * context->fd_limit_per_thread) / 16;
	context->pl_hash_table = lws_zalloc(sizeof(struct lws_peer *) *
			context->pl_hash_elements, "peer limits hash table");

	context->ip_limit_ah = info->ip_limit_ah;
	context->ip_limit_wsi = info->ip_limit_wsi;
	context->pl_notify_cb = info->pl_notify_cb;
#endif

	/*
	 * fds table contains pollfd structs for as many pollfds as we can
	 * handle... spread across as many service threads as we have going
	 */
	n = (int)(sizeof(struct lws_pollfd) * context->count_threads *
	    context->fd_limit_per_thread);
	context->pt[0].fds = lws_zalloc((unsigned int)n, "fds table");
	if (context->pt[0].fds == NULL ||
	    lws_fi(&context->fic, "ctx_createfail_oom_fds")) {
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
		lws_free(context->pt[0].fds);
#endif
		lwsl_cx_err(context, "OOM allocating %d fds\n", context->max_fds);
		goto free_context_fail;
	}
#endif

	lwsl_cx_info(context, "ctx: %5luB (%ld ctx + pt(%ld thr x %d)), "
		  "pt-fds: %d, fdmap: %d",
		  (long)sizeof(struct lws_context) +
		  (context->count_threads * context->pt_serv_buf_size),
		  (long)sizeof(struct lws_context),
		  (long)context->count_threads,
		  context->pt_serv_buf_size,
		  context->fd_limit_per_thread, n);

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	lwsl_cx_info(context, " http: ah_data: %u, ah: %lu, max count %u",
		    context->max_http_header_data,
		    (long)sizeof(struct allocated_headers),
		    context->max_http_header_pool);
#endif

#if defined(LWS_WITH_SERVER)
	if (info->server_string) {
		context->server_string = info->server_string;
		context->server_string_len = (short)
				strlen(context->server_string);
	}
#endif

#if LWS_MAX_SMP > 1
	/* each thread serves his own chunk of fds */
	for (n = 1; n < (int)context->count_threads; n++)
		context->pt[n].fds = context->pt[n - 1].fds +
				     context->fd_limit_per_thread;
#endif


	/*
	 * Past here, we may have added handles to the event lib
	 * loop and if libuv,  have to take care about how to unpick them...
	 */

	if (lws_plat_init(context, info) ||
	    lws_fi(&context->fic, "ctx_createfail_plat_init"))
		goto bail_libuv_aware;

#if defined(LWS_WITH_NETWORK)

	if (lws_fi(&context->fic, "ctx_createfail_evlib_init"))
		goto bail_libuv_aware;

	if (context->event_loop_ops->init_context)
		if (context->event_loop_ops->init_context(context, info))
			goto bail_libuv_aware;

	if (lws_fi(&context->fic, "ctx_createfail_evlib_pt"))
		goto bail_libuv_aware;

	if (context->event_loop_ops->init_pt)
		for (n = 0; n < context->count_threads; n++) {
			void *lp = NULL;

			if (info->foreign_loops)
				lp = info->foreign_loops[n];

			if (context->event_loop_ops->init_pt(context, lp, n))
				goto bail_libuv_aware;
		}

	lws_context_lock(context, __func__);
	n = __lws_create_event_pipes(context);
	lws_context_unlock(context);
	if (n)
		goto bail_libuv_aware;

	for (n = 0; n < context->count_threads; n++) {
		LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar) {
			if (lws_rops_fidx(ar, LWS_ROPS_pt_init_destroy))
				(lws_rops_func_fidx(ar, LWS_ROPS_pt_init_destroy)).
					pt_init_destroy(context, info,
							&context->pt[n], 0);
		} LWS_FOR_EVERY_AVAILABLE_ROLE_END;
	}
#endif

	lws_context_init_ssl_library(context, info);

	context->user_space = info->user;

#if defined(LWS_WITH_SERVER)
	strcpy(context->canonical_hostname, "unknown");
#if defined(LWS_WITH_NETWORK)
	lws_server_get_canonical_hostname(context, info);
#endif
#endif

#if defined(LWS_HAVE_SYS_CAPABILITY_H) && defined(LWS_HAVE_LIBCAP)
	memcpy(context->caps, info->caps, sizeof(context->caps));
	context->count_caps = info->count_caps;
#endif


#if defined(LWS_WITH_NETWORK)

#if defined(LWS_WITH_SYS_ASYNC_DNS) || defined(LWS_WITH_SYS_NTPCLIENT) || \
	defined(LWS_WITH_SYS_DHCP_CLIENT)
	{
		/*
		 * system vhost
		 */

		struct lws_context_creation_info ii;
		const struct lws_protocols *pp[4];
		struct lws_vhost *vh;
#if defined(LWS_WITH_SYS_ASYNC_DNS)
		extern const struct lws_protocols lws_async_dns_protocol;
#endif
#if defined(LWS_WITH_SYS_NTPCLIENT)
		extern const struct lws_protocols lws_system_protocol_ntpc;
#endif
#if defined(LWS_WITH_SYS_DHCP_CLIENT)
		extern const struct lws_protocols lws_system_protocol_dhcpc4;
#endif

		n = 0;
#if defined(LWS_WITH_SYS_ASYNC_DNS)
		pp[n++] = &lws_async_dns_protocol;
#endif
#if defined(LWS_WITH_SYS_NTPCLIENT)
		pp[n++] = &lws_system_protocol_ntpc;
#endif
#if defined(LWS_WITH_SYS_DHCP_CLIENT)
		pp[n++] = &lws_system_protocol_dhcpc4;
#endif
		pp[n] = NULL;

		memset(&ii, 0, sizeof(ii));
		ii.vhost_name = "system";
		ii.pprotocols = pp;
		ii.port = CONTEXT_PORT_NO_LISTEN;

		if (lws_fi(&context->fic, "ctx_createfail_sys_vh"))
			vh = NULL;
		else
			vh = lws_create_vhost(context, &ii);
		if (!vh) {
			lwsl_cx_err(context, "failed to create system vhost");
			goto bail_libuv_aware;
		}

		context->vhost_system = vh;

		if (lws_protocol_init_vhost(vh, NULL) ||
		    lws_fi(&context->fic, "ctx_createfail_sys_vh_init")) {
			lwsl_cx_err(context, "failed to init system vhost");
			goto bail_libuv_aware;
		}
#if defined(LWS_WITH_SYS_ASYNC_DNS)
		lws_async_dns_init(context);
			//goto bail_libuv_aware;
#endif
	}

#endif

#if defined(LWS_WITH_SYS_STATE)
	/*
	 * init the lws_state mgr for the system state
	 */

	context->mgr_system.state_names		= system_state_names;
	context->mgr_system.name		= "system";
	context->mgr_system.state		= LWS_SYSTATE_CONTEXT_CREATED;
	context->mgr_system.parent		= context;
	context->mgr_system.context		= context;
#if defined(LWS_WITH_SYS_SMD)
	context->mgr_system.smd_class		= LWSSMDCL_SYSTEM_STATE;
#endif

	context->protocols_notify.name		= "prot_init";
	context->protocols_notify.notify_cb	= lws_state_notify_protocol_init;

	lws_state_reg_notifier(&context->mgr_system, &context->protocols_notify);

	/*
	 * insert user notifiers here so they can participate with vetoing us
	 * trying to jump straight to operational, or at least observe us
	 * reaching 'operational', before we returned from context creation.
	 */

	lws_state_reg_notifier_list(&context->mgr_system,
				    info->register_notifier_list);
#endif

	/*
	 * if he's not saying he'll make his own vhosts later then act
	 * compatibly and make a default vhost using the data in the info
	 */
	if (!lws_check_opt(info->options, LWS_SERVER_OPTION_EXPLICIT_VHOSTS)) {
		if (!lws_create_vhost(context, info) ||
		    lws_fi(&context->fic, "ctx_createfail_def_vh")) {
			lwsl_cx_err(context, "Failed to create default vhost");

#if defined(LWS_WITH_PEER_LIMITS)
			lws_free_set_NULL(context->pl_hash_table);
#endif
			goto bail;
		}
	}

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
	if (info->http_nsc_filepath) {
		memset(&ci, 0, sizeof(ci));

		ci.cx			   = context;
		ci.ops			   = &lws_cache_ops_nscookiejar;
		ci.name			   = "NSC";
		ci.u.nscookiejar.filepath  = info->http_nsc_filepath;

		context->nsc = lws_cache_create(&ci);
		if (!context->nsc)
			goto bail;

		ci.ops			  = &lws_cache_ops_heap;
		ci.name			  = "L1";
		ci.parent		  = context->nsc;
		ci.max_footprint	  = info->http_nsc_heap_max_footprint;
		ci.max_items		  = info->http_nsc_heap_max_items;
		ci.max_payload		  = info->http_nsc_heap_max_payload;

		context->l1 = lws_cache_create(&ci);
		if (!context->l1) {
			lwsl_cx_err(context, "Failed to init cookiejar");
			goto bail;
		}
	}
#endif

#if defined(LWS_WITH_SECURE_STREAMS)

#if !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	if (context->pss_policies_json) {
		/*
		 * You must create your context with the explicit vhosts flag
		 * in order to use secure streams
		 */
		assert(lws_check_opt(info->options,
		       LWS_SERVER_OPTION_EXPLICIT_VHOSTS));

		if (lws_ss_policy_parse_begin(context, 0) ||
		    lws_fi(&context->fic, "ctx_createfail_ss_pol1")) {
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
			lws_ss_policy_parse_abandon(context);
#endif
			goto bail_libuv_aware;
		}

		n = lws_ss_policy_parse(context,
					(uint8_t *)context->pss_policies_json,
					strlen(context->pss_policies_json));
		if ((n != LEJP_CONTINUE && n < 0) ||
		    lws_fi(&context->fic, "ctx_createfail_ss_pol2")) {
			lws_ss_policy_parse_abandon(context);
			goto bail_libuv_aware;
		}

		if (lws_ss_policy_set(context, "hardcoded") ||
		    lws_fi(&context->fic, "ctx_createfail_ss_pol3")) {
			lwsl_cx_err(context, "policy set failed");
			goto bail_libuv_aware;
		}
	}
#else
	if (context->pss_policies) {
		/* user code set the policy objects directly, no parsing step */

		if (lws_ss_policy_set(context, "hardcoded") ||
		    lws_fi(&context->fic, "ctx_createfail_ss_pol3")) {
			lwsl_cx_err(context, "policy set failed");
			goto bail_libuv_aware;
		}
	}
#endif
#endif

	lws_context_init_extensions(info, context);

	lwsl_cx_info(context, " mem: per-conn:        %5lu bytes + protocol rx buf",
		    (unsigned long)sizeof(struct lws));

	/*
	 * drop any root privs for this process
	 * to listen on port < 1023 we would have needed root, but now we are
	 * listening, we don't want the power for anything else
	 */
	if (!lws_check_opt(info->options, LWS_SERVER_OPTION_EXPLICIT_VHOSTS))
		if (lws_plat_drop_app_privileges(context, 1) ||
		    lws_fi(&context->fic, "ctx_createfail_privdrop"))
			goto bail_libuv_aware;

#if defined(LWS_WITH_SYS_STATE)
	/*
	 * We want to move on the syste, state as far as it can go towards
	 * OPERATIONAL now.  But we have to return from here first so the user
	 * code that called us can set its copy of context, which it may be
	 * relying on to perform operations triggered by the state change.
	 *
	 * We set up a sul to come back immediately and do the state change.
	 */

	lws_sul_schedule(context, 0, &context->sul_system_state,
			 lws_context_creation_completion_cb, 1);
#endif

	/* expedite post-context init (eg, protocols) */
	lws_cancel_service(context);
#endif

	return context;

early_bail:
	lws_fi_destroy(&info->fic);

	return NULL;

#if defined(LWS_WITH_NETWORK)
bail:
	lws_fi_destroy(&info->fic);
	lws_context_destroy(context);

	return NULL;
#endif

bail_libuv_aware:
	lws_context_destroy(context);
#if defined(LWS_WITH_LIBUV)
	return fatal_exit_defer ? context : NULL;
#else
	return NULL;
#endif

#if defined(LWS_WITH_NETWORK)
fail_event_libs:
	if (context)
	lwsl_cx_err(context, "Requested event library support not configured");
#endif

#if defined(LWS_WITH_NETWORK)
free_context_fail:
	if (context) {
#if defined(LWS_WITH_SYS_SMD)
		_lws_smd_destroy(context);
#endif
	}
#endif
free_context_fail2:
	if (context) {
#if defined(LWS_WITH_SYS_METRICS)
		lws_metrics_destroy(context);
#endif
		lws_fi_destroy(&context->fic);
	}
	lws_fi_destroy(&info->fic);
	if (context) {
		lwsl_refcount_cx(context->log_cx, -1);
		lws_free(context);
	}

	return NULL;
}

#if defined(LWS_WITH_NETWORK)
int
lws_system_cpd_start(struct lws_context *cx)
{
	cx->captive_portal_detect = LWS_CPD_UNKNOWN;

	/* if there's a platform implementation, use it */

	if (lws_system_get_ops(cx) &&
	    lws_system_get_ops(cx)->captive_portal_detect_request)
		return lws_system_get_ops(cx)->captive_portal_detect_request(cx);

#if defined(LWS_WITH_SECURE_STREAMS)
	/*
	 * Otherwise try to use SS "captive_portal_detect" if that's enabled
	 */
	return lws_ss_sys_cpd(cx);
#else
	return 0;
#endif
}

static void
lws_system_deferred_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context *cx =
		     lws_container_of(sul, struct lws_context, sul_cpd_defer);

	lws_system_cpd_start(cx);
}

void
lws_system_cpd_start_defer(struct lws_context *cx, lws_usec_t defer_us)
{
	lws_sul_schedule(cx, 0, &cx->sul_cpd_defer,
			 lws_system_deferred_cb, defer_us);
}

#if (defined(LWS_WITH_SYS_STATE) && defined(LWS_WITH_SYS_SMD)) || !defined(LWS_WITH_NO_LOGS)
static const char *cname[] = { "Unknown", "OK", "Captive", "No internet" };
#endif

void
lws_system_cpd_set(struct lws_context *cx, lws_cpd_result_t result)
{
	if (cx->captive_portal_detect != LWS_CPD_UNKNOWN)
		return;

#if !defined(LWS_WITH_NO_LOGS)
	lwsl_cx_notice(cx, "setting CPD result %s", cname[result]);
#endif

	cx->captive_portal_detect = (uint8_t)result;

#if defined(LWS_WITH_SYS_STATE)
#if defined(LWS_WITH_SYS_SMD)
	lws_smd_msg_printf(cx, LWSSMDCL_NETWORK,
			   "{\"type\":\"cpd\",\"result\":\"%s\"}",
			   cname[cx->captive_portal_detect]);
#endif

	/* if nothing is there to intercept anything, go all the way */
	if (cx->mgr_system.state != LWS_SYSTATE_POLICY_INVALID)
		lws_state_transition_steps(&cx->mgr_system,
					   LWS_SYSTATE_OPERATIONAL);
#endif
}

lws_cpd_result_t
lws_system_cpd_state_get(struct lws_context *cx)
{
	return (lws_cpd_result_t)cx->captive_portal_detect;
}

#endif

int
lws_context_is_deprecated(struct lws_context *cx)
{
	return cx->deprecated;
}

/*
 * When using an event loop, the context destruction is in three separate
 * parts.  This is to cover both internal and foreign event loops cleanly.
 *
 *  - lws_context_destroy() simply starts a soft close of all wsi and
 *     related allocations.  The event loop continues.
 *
 *     As the closes complete in the event loop, reference counting is used
 *     to determine when everything is closed.  It then calls
 *     lws_context_destroy2().
 *
 *  - lws_context_destroy2() cleans up the rest of the higher-level logical
 *     lws pieces like vhosts.  If the loop was foreign, it then proceeds to
 *     lws_context_destroy3().  If it the loop is internal, it stops the
 *     internal loops and waits for lws_context_destroy() to be called again
 *     outside the event loop (since we cannot destroy the loop from
 *     within the loop).  That will cause lws_context_destroy3() to run
 *     directly.
 *
 *  - lws_context_destroy3() destroys any internal event loops and then
 *     destroys the context itself, setting what was info.pcontext to NULL.
 */


#if defined(LWS_WITH_NETWORK)
static void
lws_pt_destroy(struct lws_context_per_thread *pt)
{
	volatile struct lws_foreign_thread_pollfd *ftp, *next;
	volatile struct lws_context_per_thread *vpt;
#if defined(LWS_WITH_CGI)
	lws_ctx_t ctx = pt->context;

		if (lws_rops_fidx(&role_ops_cgi, LWS_ROPS_pt_init_destroy))
			(lws_rops_func_fidx(&role_ops_cgi, LWS_ROPS_pt_init_destroy)).
				pt_init_destroy(ctx, NULL, pt, 1);
#endif
	vpt = (volatile struct lws_context_per_thread *)pt;
	ftp = vpt->foreign_pfd_list;
	while (ftp) {
		next = ftp->next;
		lws_free((void *)ftp);
		ftp = next;
	}
	vpt->foreign_pfd_list = NULL;

	lws_pt_lock(pt, __func__);

	if (pt->pipe_wsi) {
		lws_destroy_event_pipe(pt->pipe_wsi);
		pt->pipe_wsi = NULL;
	}

	if (pt->dummy_pipe_fds[0]
#if !defined(WIN32)
	    && (int)pt->dummy_pipe_fds[0] != -1
#endif
	) {
		struct lws wsi;

		memset(&wsi, 0, sizeof(wsi));
		wsi.a.context = pt->context;
		wsi.tsi = (char)pt->tid;
		lws_plat_pipe_close(&wsi);
	}

#if defined(LWS_WITH_SECURE_STREAMS)
	lws_dll2_foreach_safe(&pt->ss_owner, NULL, lws_ss_destroy_dll);

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API) && defined(LWS_WITH_CLIENT)
	lws_dll2_foreach_safe(&pt->ss_client_owner, NULL, lws_sspc_destroy_dll);
#endif

#if defined(LWS_WITH_SEQUENCER)
	lws_seq_destroy_all_on_pt(pt);
#endif


#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
		while (pt->http.ah_list)
			_lws_destroy_ah(pt, pt->http.ah_list);
#endif

#endif

	lws_pt_unlock(pt);
	pt->pipe_wsi = NULL;

}
#endif

/*
 * Context destruction is now a state machine that's aware of SMP pts and
 * various event lib approaches.
 *
 * lws_context_destroy() expects to be called at the end of the user code's
 * usage of it.  But it can also be called non-finally, as a way to stop
 * service and exit the outer user service loop, and then complete in the
 * final call.
 *
 * For libuv, with async close, it must decide by refcounting the hamdles on
 * the loop if it has extricated itself from the loop and can be destroyed.
 *
 * The various entry states for the staged destroy
 *
 * LWSCD_NO_DESTROY: begin destroy process
 * 	- mark context as starting destroy process
 * 	- start vhost destroy
 * 	- stop any further user protocol service
 *
 * LWSCD_PT_WAS_DEFERRED: come back here if any pt inside service
 * 	- Check for pts that are inside service loop, mark deferral needed if so
 * 	- If not, close all wsi on the pt loop and start logical pt destroy
 * 	- If any deferred, set state to LWSCD_PT_WAS_DEFERRED and exit
 *
 * LWSCD_PT_WAIT_ALL_DESTROYED: come back here for async loop / pt closes
 * 	- exit if any pt not marked as unused, or destroyed
 * 	- if all pt down, call into evlib to advance context destroy
 * 	- finalize vhost destruction
 * 	- finalize pt destruction
 *	- if foreign loops, set state to LWSCD_FINALIZATION and exit
 *
 * LWSCD_FINALIZATION: come back here at final lws_destroy_context() call
 *	- destroy sundries
 *	- destroy and free the actual context
 */

void
lws_context_destroy(struct lws_context *context)
{
	struct lws_context **pcontext_finalize;
#if defined(LWS_WITH_NETWORK)
	struct lws_context_per_thread *pt;
	struct lws_vhost *vh = NULL, *vh1;
	int alive = 0, deferred_pt = 0;
#endif
#if defined(LWS_WITH_PEER_LIMITS)
	uint32_t nu;
#endif
	int n;

	if (!context || context->inside_context_destroy)
		return;

	pcontext_finalize = context->pcontext_finalize;

	lws_context_lock(context, __func__);
	context->inside_context_destroy = 1;

	lwsl_cx_info(context, "destroy_state %d", context->destroy_state);

	switch (context->destroy_state) {
	case LWSCD_NO_DESTROY:
		/*
		 * We're getting started
		 */

		lwsl_cx_info(context, "starting context destroy flow");
		context->being_destroyed = 1;

#if defined(LWS_WITH_NETWORK)

		/*
		 * Close any vhost listen wsi
		 *
		 * inform all the protocols that they are done and will have no
		 * more callbacks.
		 *
		 * We can't free things until after the event loop shuts down.
		 */

		if (context->protocol_init_done)
			vh = context->vhost_list;

		while (vh) {
			lwsl_vhost_info(vh, "start close");
			vh1 = vh->vhost_next;
			lws_vhost_destroy1(vh);
			vh = vh1;
		}
#endif

		lws_plat_context_early_destroy(context);

		context->service_no_longer_possible = 1;
		context->requested_stop_internal_loops = 1;

		/* fallthru */

	case LWSCD_PT_WAS_DEFERRED:

#if defined(LWS_WITH_NETWORK)

		/*
		 * We want to mark the pts as their destruction having been
		 * initiated, so they will reject any new wsi, and iterate all
		 * existing pt wsi starting to close them.
		 *
		 * If the event loop has async close, we have to return after
		 * this and try again when all the loops stop after all the
		 * refcounted wsi are gone.
		 */

		pt = context->pt;
		for (n = 0; n < context->count_threads; n++) {
			lws_pt_lock(pt, __func__);

			/* evlib will realize it needs to destroy pt */
			pt->destroy_self = 1;

			if (pt->inside_lws_service) {
				pt->event_loop_pt_unused = 1;
				deferred_pt = 1;
				goto next;
			}

			/*
			 * Close every handle in the fds
			 */

			while (pt->fds_count) {
				struct lws *wsi = wsi_from_fd(context,
							      pt->fds[0].fd);

				if (wsi) {

					lwsl_cx_debug(context,
						"pt %d: closing wsi %p: role %s",
						n, wsi, wsi->role_ops->name);

					lws_close_free_wsi(wsi,
						LWS_CLOSE_STATUS_NOSTATUS_CONTEXT_DESTROY,
						"ctx destroy"
						/* no protocol close */);

					if (pt->pipe_wsi == wsi)
						pt->pipe_wsi = NULL;
				}
			}

#if defined(LWS_WITH_CGI)
			(lws_rops_func_fidx(&role_ops_cgi,
					    LWS_ROPS_pt_init_destroy)).
					    pt_init_destroy(context, NULL,
							    pt, 1);
#endif

			/*
			 * This closes handles that belong to the evlib pt
			 * footprint, eg, timers, idle
			 */

			if (context->event_loop_ops->destroy_pt) {
				lwsl_cx_info(context,
					     "calling evlib destroy_pt %d\n", n);
				context->event_loop_ops->destroy_pt(context, n);
			}

next:
			lws_pt_unlock(pt);

			pt++;
		}

		if (deferred_pt) {
			context->destroy_state = LWSCD_PT_WAS_DEFERRED;
			lwsl_cx_notice(context, "destroy from inside service");
			lws_cancel_service(context);
			goto bail;
		}
#endif
		context->destroy_state = LWSCD_PT_WAIT_ALL_DESTROYED;

		/*
		 * We have different needs depending if foreign loop or not.
		 *
		 * 1) If foreign loop, we really want to advance the
		 *    destroy_context() past here, and block only for libuv-
		 *    style async close completion.
		 *
		 * 2a) If poll, and we exited by ourselves and are calling a
		 *     final destroy_context() outside of any service already,
		 *     we want to advance all the way in one step.
		 *
		 * 2b) If poll, and we are reacting to a SIGINT, service
		 *     thread(s) may be in poll wait or servicing.  We can't
		 *     advance the destroy_context() to the point it's freeing
		 *     things; we have to leave that for the final
		 *     destroy_context() after the service thread(s) are
		 *     finished calling for service.
		 */

#if defined(LWS_WITH_NETWORK)
		if (context->event_loop_ops->destroy_context1) {
			lwsl_cx_info(context, "do evlib destroy_context1 and wait");
			context->event_loop_ops->destroy_context1(context);

			goto bail;
		}

		/*
		 * ...if the more typical sync close, we can clean up the pts
		 * now ourselves...
		 */

		lwsl_cx_info(context, "manually destroying pts");

		pt = context->pt;
		for (n = 0; n < context->count_threads; n++, pt++) {
			pt->event_loop_pt_unused = 1;
			lws_pt_destroy(pt);
		}
#endif
		/* fallthru */

	case LWSCD_PT_WAIT_ALL_DESTROYED:

#if defined(LWS_WITH_NETWORK)

		for (n = 0; n < context->count_threads; n++)
			if (!context->pt[n].is_destroyed &&
			    !context->pt[n].event_loop_pt_unused)
				alive++;

		lwsl_cx_info(context, "PT_WAIT_ALL_DESTROYED: %d alive", alive);

		if (alive)
			break;

		/*
		 * With foreign loops, removing all our fds from the loop
		 * means there are no more ways for the foreign loop to give
		 * us any further CPU once we leave here... so we must make
		 * sure related service threads are exiting so we can pick up
		 * again at the original app thread and do the context
		 * destroy completion
		 */

		/*
		 * evlib specific loop destroy?
		 */
		if (context->event_loop_ops->destroy_context2)
			/*
			 * He returns nonzero to indicate the evlib must
			 * continue around the loop before destroy of it is
			 * completed so it can be freed
			 */
			context->event_loop_ops->destroy_context2(context);
		context->requested_stop_internal_loops = 1;
#endif

		/*
		 * Every pt and wsi that may depend on the logical vhosts
		 * is destroyed.  We can remove the logical vhosts.
		 */

#if defined(LWS_WITH_SYS_STATE) && defined(LWS_WITH_NETWORK)
	lws_state_transition(&context->mgr_system, LWS_SYSTATE_POLICY_INVALID);
#endif

#if defined(LWS_WITH_NETWORK)
		/*
		 * free all the per-vhost allocations
		 */

		vh = context->vhost_list;
		while (vh) {
			vh1 = vh->vhost_next;
		//	lwsl_vhost_debug(vh, "vh %s destroy2", vh->name);
			__lws_vhost_destroy2(vh);
			vh = vh1;
		}

		/* remove ourselves from the pending destruction list */

		while (context->vhost_pending_destruction_list)
			/* removes itself from list */
			__lws_vhost_destroy2(context->vhost_pending_destruction_list);
#endif

#if defined(LWS_WITH_NETWORK)
		lws_ssl_context_destroy(context);
#endif
		lws_plat_context_late_destroy(context);

#if defined(LWS_WITH_PEER_LIMITS)
		if (context->pl_hash_table)
			for (nu = 0; nu < context->pl_hash_elements; nu++)	{
				if (!context->pl_hash_table[nu])
					continue;
				lws_start_foreach_llp(struct lws_peer **, peer,
						      context->pl_hash_table[nu]) {
					struct lws_peer *df = *peer;
					*peer = df->next;
					lws_free(df);
					continue;
				} lws_end_foreach_llp(peer, next);
			}
		lws_free(context->pl_hash_table);
#endif

#if defined(LWS_WITH_NETWORK)

		for (n = 0; n < context->count_threads; n++) {
			struct lws_context_per_thread *pt = &context->pt[n];

			(void)pt;
#if defined(LWS_WITH_SEQUENCER)
			lws_seq_destroy_all_on_pt(pt);
#endif
			LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar) {
				if (lws_rops_fidx(ar, LWS_ROPS_pt_init_destroy))
					(lws_rops_func_fidx(ar, LWS_ROPS_pt_init_destroy)).
						pt_init_destroy(context, NULL, pt, 1);
			} LWS_FOR_EVERY_AVAILABLE_ROLE_END;

#if defined(LWS_WITH_CGI)
			lws_rops_func_fidx(&role_ops_cgi,
					   LWS_ROPS_pt_init_destroy).
					        pt_init_destroy(context, NULL,
					        		pt, 1);
#endif

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
			while (pt->http.ah_list)
				_lws_destroy_ah(pt, pt->http.ah_list);
#endif
			lwsl_cx_info(context, "pt destroy %d", n);
			lws_pt_destroy(pt);
		}
#endif /* NETWORK */

		context->destroy_state = LWSCD_FINALIZATION;

#if defined(LWS_WITH_NETWORK)

		if (context->pt[0].event_loop_foreign &&
		    context->event_loop_ops->destroy_context1) {

			lwsl_cx_info(context,
				    "leaving final context destruction"
					" for final call");
			goto bail;
		}

		if (context->event_loop_ops->destroy_context1 &&
		    !context->pt[0].event_loop_foreign) {
			lwsl_cx_notice(context, "waiting for internal loop exit");

			goto bail;
		}
#endif
		/* fallthru */

	case LWSCD_FINALIZATION:

#if defined(LWS_WITH_SYS_METRICS)
		lws_metrics_dump(context);
#endif

		context->evlib_finalize_destroy_after_int_loops_stop = 1;

#if defined(LWS_WITH_NETWORK)
		if (context->event_loop_ops->destroy_context2)
			context->event_loop_ops->destroy_context2(context);
#if defined(LWS_WITH_SYS_STATE)
		lws_state_transition_steps(&context->mgr_system,
					   LWS_SYSTATE_CONTEXT_DESTROYING);
#endif
		/*
		 * finalize destroy of pt and things hanging off it
		 */

		for (n = 0; n < context->count_threads; n++) {
			struct lws_context_per_thread *pt = &context->pt[n];

			/*
			 * Destroy the pt-roles
			 */

			LWS_FOR_EVERY_AVAILABLE_ROLE_START(ar) {
				if (lws_rops_fidx(ar, LWS_ROPS_pt_init_destroy))
					(lws_rops_func_fidx(ar, LWS_ROPS_pt_init_destroy)).
							pt_init_destroy(context, NULL, pt, 1);
			} LWS_FOR_EVERY_AVAILABLE_ROLE_END;

		#if defined(LWS_WITH_CGI)
			lws_rops_func_fidx(&role_ops_cgi, LWS_ROPS_pt_init_destroy).
						pt_init_destroy(context, NULL, pt, 1);
		#endif

			lws_pt_mutex_destroy(pt);
			assert(!pt->is_destroyed);
			pt->destroy_self = 0;
			pt->is_destroyed = 1;

			lwsl_cx_info(context, "pt %d fully destroyed",
					(int)(pt - pt->context->pt));
		}

		/*
		 * wsis are gone, pts are gone, vhosts are gone.
		 *
		 * clean up the context and things hanging off it
		 */

#if defined(LWS_WITH_TLS_JIT_TRUST)
		lws_cache_destroy(&context->trust_cache);
		lws_tls_jit_trust_inflight_destroy_all(context);
#endif

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
		lws_cache_destroy(&context->nsc);
		lws_cache_destroy(&context->l1);
#endif

#if defined(LWS_WITH_SYS_SMD)
		_lws_smd_destroy(context);
#endif

#if defined(LWS_WITH_SYS_ASYNC_DNS)
		lws_async_dns_deinit(&context->async_dns);
#endif
#if defined(LWS_WITH_SYS_DHCP_CLIENT)
		lws_dhcpc_remove(context, NULL);
#endif

		if (context->pt[0].fds)
			lws_free_set_NULL(context->pt[0].fds);
#endif
		lws_context_deinit_ssl_library(context);

#if defined(LWS_WITH_DETAILED_LATENCIES)
		if (context->latencies_fd != -1)
			compatible_close(context->latencies_fd);
#endif

		for (n = 0; n < LWS_SYSBLOB_TYPE_COUNT; n++)
			lws_system_blob_destroy(
					lws_system_get_blob(context, (lws_system_blob_item_t)n, 0));

#if defined(LWS_WITH_NETWORK) && defined(LWS_WITH_SECURE_STREAMS) && \
	!defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)

		while (context->server_der_list) {
			struct lws_ss_x509 *x = context->server_der_list;

			context->server_der_list = x->next;
			lws_free((void *)x->ca_der);
		}

		if (context->ac_policy)
			lwsac_free(&context->ac_policy);
#endif

		/*
		 * Context lock is about to go away
		 */

		lws_context_unlock(context);

#if LWS_MAX_SMP > 1
		lws_mutex_refcount_destroy(&context->mr);
#endif

#if defined(LWS_WITH_SYS_METRICS) && defined(LWS_WITH_NETWORK)
		lws_metrics_destroy(context);
#endif

		if (context->external_baggage_free_on_destroy)
			free(context->external_baggage_free_on_destroy);

#if defined(LWS_PLAT_FREERTOS)
#if defined(LWS_AMAZON_RTOS)
		context->last_free_heap = xPortGetFreeHeapSize();
#else
		context->last_free_heap = esp_get_free_heap_size();
#endif
#endif

#if defined(LWS_WITH_EVLIB_PLUGINS) && defined(LWS_WITH_EVENT_LIBS)
		if (context->evlib_plugin_list)
			lws_plugins_destroy(&context->evlib_plugin_list,
					    NULL, NULL);
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
		lws_fi_destroy(&context->fic);
#endif

		lwsl_refcount_cx(context->log_cx, -1);

		lws_free(context);

		if (pcontext_finalize)
			*pcontext_finalize = NULL;

		return;
	}

#if defined(LWS_WITH_NETWORK)
bail:
#endif
	lwsl_cx_info(context, "leaving");
	context->inside_context_destroy = 0;
	lws_context_unlock(context);
}

int
lws_context_is_being_destroyed(struct lws_context *context)
{
	return !!context->being_destroyed;
}

#if defined(LWS_WITH_SYS_STATE)
struct lws_context *
lws_system_context_from_system_mgr(lws_state_manager_t *mgr)
{
#if defined(LWS_WITH_NETWORK)
	return mgr->context;
#else
	return NULL;
#endif
}
#endif

void
lws_log_prepend_context(struct lws_log_cx *cx, void *obj, char **p, char *e)
{
	struct lws_context *lcx = (struct lws_context *)obj;

	if (lcx->name)
		*p += lws_snprintf(*p, lws_ptr_diff_size_t(e, (*p)), "%s: ",
				   lcx->name);
}

struct lws_log_cx *
lwsl_context_get_cx(struct lws_context *cx)
{
	if (!cx)
		return NULL;

	return cx->log_cx;
}
