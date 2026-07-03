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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "private-lib-core.h"

#include <pwd.h>
#include <grp.h>

#ifdef LWS_WITH_PLUGINS
#include <dlfcn.h>
#endif
#include <dirent.h>

#if defined(LWS_WITH_NETWORK)
static void
lws_sul_plat_unix(lws_sorted_usec_list_t *sul)
{
	struct lws_context_per_thread *pt =
		lws_container_of(sul, struct lws_context_per_thread, sul_plat);
	struct lws_context *context = pt->context;
	int n = 0, m = 0;

#if !defined(LWS_NO_DAEMONIZE)
	/* if our parent went down, don't linger around */
	if (pt->context->started_with_parent &&
	    kill(pt->context->started_with_parent, 0) < 0)
		kill(getpid(), SIGTERM);
#endif

	for (n = 0; n < context->count_threads; n++)
		m = m | (int)pt->fds_count;

	if (context->deprecated && !m) {
		lwsl_notice("%s: ending deprecated context\n", __func__);
		kill(getpid(), SIGINT);
		return;
	}

#if defined(LWS_WITH_SERVER)
	lws_context_lock(context, "periodic checks");
	lws_start_foreach_llp(struct lws_vhost **, pv,
			      context->no_listener_vhost_list) {
		struct lws_vhost *v = *pv;
		lwsl_debug("deferred iface: checking if on vh %s\n", (*pv)->name);
		if (_lws_vhost_init_server(NULL, *pv) == 0) {
			/* became happy */
			lwsl_notice("vh %s: became connected\n", v->name);
			*pv = v->no_listener_vhost_list;
			v->no_listener_vhost_list = NULL;
			break;
		}
	} lws_end_foreach_llp(pv, no_listener_vhost_list);
	lws_context_unlock(context);
#endif

	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &pt->sul_plat, 30 * LWS_US_PER_SEC);
}
#endif

#if defined(LWS_WITH_PLUGINS)
static int
protocol_plugin_cb(struct lws_plugin *pin, void *each_user)
{
	struct lws_context *context = (struct lws_context *)each_user;
	const lws_plugin_protocol_t *plpr =
			(const lws_plugin_protocol_t *)pin->hdr;

	context->plugin_protocol_count = (short)(context->plugin_protocol_count +
						 plpr->count_protocols);
	context->plugin_extension_count = (short)(context->plugin_extension_count +
						  plpr->count_extensions);

	return 0;
}
#endif

int
lws_plat_init(struct lws_context *context,
	      const struct lws_context_creation_info *info)
{
	int fd;
#if defined(LWS_WITH_NETWORK)
	/*
	 * context has the process-global fd lookup array.  This can be
	 * done two different ways now; one or the other is done depending on if
	 * info->fd_limit_per_thread was snonzero
	 *
	 *  - default: allocate a worst-case lookup array sized for ulimit -n
	 *             and use the fd directly as an index into it
	 *
	 *  - slow:    allocate context->max_fds entries only (which can be
	 *             forced at context creation time to be
	 *             info->fd_limit_per_thread * the number of threads)
	 *             and search the array to lookup fds
	 *
	 * the default way is optimized for server, if you only use one or two
	 * client wsi the slow way may save a lot of memory.
	 *
	 * Both ways allocate an array of struct lws *... one allocates it for
	 * all possible fd indexes the process could produce and uses it as a
	 * map, the other allocates for an amount of wsi the lws context is
	 * expected to use and searches through it to manipulate it.
	 */

	context->lws_lookup = lws_zalloc(sizeof(struct lws *) *
					 context->max_fds, "lws_lookup");

	if (!context->lws_lookup) {
		lwsl_cx_err(context, "OOM on alloc lws_lookup array for %d conn",
			 context->max_fds);
		return 1;
	}

#if defined(LWS_WITH_MBEDTLS)
	{
		int n;

		/* initialize platform random through mbedtls */
		mbedtls_entropy_init(&context->mec);
		mbedtls_ctr_drbg_init(&context->mcdc);

		n = mbedtls_ctr_drbg_seed(&context->mcdc, mbedtls_entropy_func,
					  &context->mec, NULL, 0);
		if (n)
			lwsl_err("%s: mbedtls_ctr_drbg_seed() returned 0x%x\n",
				 __func__, n);
#if 0
		else {
			uint8_t rtest[16];
			lwsl_notice("%s: started drbg\n", __func__);
			if (mbedtls_ctr_drbg_random(&context->mcdc, rtest,
							sizeof(rtest)))
				lwsl_err("%s: get random failed\n", __func__);
			else
				lwsl_hexdump_notice(rtest, sizeof(rtest));
		}
#endif
	}
#endif

	lwsl_cx_info(context, " mem: platform fd map: %5lu B",
		    (unsigned long)(sizeof(struct lws *) * context->max_fds));
#endif
#if defined(LWS_WITH_FILE_OPS)
	fd = lws_open(SYSTEM_RANDOM_FILEPATH, O_RDONLY);
#else
	fd = open(SYSTEM_RANDOM_FILEPATH, O_RDONLY);
#endif
	context->fd_random = fd;
	if (context->fd_random < 0) {
		lwsl_err("Unable to open random device %s %d, errno %d\n",
			 SYSTEM_RANDOM_FILEPATH, context->fd_random, errno);
		return 1;
	}

#if defined(LWS_WITH_PLUGINS)
	{
		char *ld_env = getenv("LD_LIBRARY_PATH");

		if (ld_env) {
			const char *pp[2] = { ld_env, NULL };

			lws_plugins_init(&context->plugin_list, pp,
					 "lws_protocol_plugin", NULL,
					 protocol_plugin_cb, context);
		}

		if (info->plugin_dirs)
			lws_plugins_init(&context->plugin_list,
					 info->plugin_dirs,
					 "lws_protocol_plugin", NULL,
					 protocol_plugin_cb, context);
	}
#endif


#if defined(LWS_WITH_NETWORK)
	/* we only need to do this on pt[0] */

	context->pt[0].sul_plat.cb = lws_sul_plat_unix;
	__lws_sul_insert_us(&context->pt[0].pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &context->pt[0].sul_plat, 30 * LWS_US_PER_SEC);
#endif

	return 0;
}

int
lws_plat_context_early_init(void)
{
#if !defined(LWS_AVOID_SIGPIPE_IGN)
	signal(SIGPIPE, SIG_IGN);
#endif

	return 0;
}

void
lws_plat_context_early_destroy(struct lws_context *context)
{
}

void
lws_plat_context_late_destroy(struct lws_context *context)
{
#if defined(LWS_WITH_PLUGINS)
	if (context->plugin_list)
		lws_plugins_destroy(&context->plugin_list, NULL, NULL);
#endif
#if defined(LWS_WITH_NETWORK)
	if (context->lws_lookup)
		lws_free_set_NULL(context->lws_lookup);
#endif
	if (!context->fd_random)
		lwsl_err("ZERO RANDOM FD\n");
	if (context->fd_random != LWS_INVALID_FILE)
		close(context->fd_random);
}
