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

#include <glib-unix.h>

#include "private-lib-event-libs-glib.h"

#if !defined(G_SOURCE_FUNC)
#define G_SOURCE_FUNC(f)	  ((GSourceFunc) (void (*)(void)) (f))
#endif

#define pt_to_priv_glib(_pt) ((struct lws_pt_eventlibs_glib *)(_pt)->evlib_pt)
#define wsi_to_priv_glib(_w) ((struct lws_wsi_eventlibs_glib *)(_w)->evlib_wsi)

#define wsi_to_subclass(_w)	  (wsi_to_priv_glib(_w)->w_read.source)
#define wsi_to_gsource(_w)	  ((GSource *)wsi_to_subclass(_w))
#define pt_to_loop(_pt)		  (pt_to_priv_glib(_pt)->loop)
#define pt_to_g_main_context(_pt) g_main_loop_get_context(pt_to_loop(_pt))

#define lws_gs_valid(t)		  (t.gs)
#define lws_gs_destroy(t)	  if (lws_gs_valid(t)) { \
					g_source_destroy(t.gs); \
					g_source_unref(t.gs); \
					t.gs = NULL; t.tag = 0; }

static gboolean
lws_glib_idle_timer_cb(void *p);

static gboolean
lws_glib_hrtimer_cb(void *p);

static gboolean
lws_glib_check(GSource *src)
{
	struct lws_io_watcher_glib_subclass *sub =
			(struct lws_io_watcher_glib_subclass *)src;

	return !!g_source_query_unix_fd(src, sub->tag);
}

/*
 * These helpers attach only to the main_context that belongs to the pt's glib
 * mainloop.  The simpler g_timeout_add() and g_idle_add() are forbidden
 * because they implicitly choose the default main context to attach to
 * instead of specifically the loop bound to the pt.
 *
 * https://developer.gnome.org/programming-guidelines/unstable/main-contexts.html.en#what-is-gmaincontext
 */

static int
lws_glib_set_idle(struct lws_context_per_thread *pt)
{
	if (lws_gs_valid(pt_to_priv_glib(pt)->idle))
		return 0;

	pt_to_priv_glib(pt)->idle.gs = g_idle_source_new();
	if (!pt_to_priv_glib(pt)->idle.gs)
		return 1;

	g_source_set_callback(pt_to_priv_glib(pt)->idle.gs,
			      lws_glib_idle_timer_cb, pt, NULL);
	pt_to_priv_glib(pt)->idle.tag = g_source_attach(
			pt_to_priv_glib(pt)->idle.gs, pt_to_g_main_context(pt));

	return 0;
}

static int
lws_glib_set_timeout(struct lws_context_per_thread *pt, unsigned int ms)
{
	lws_gs_destroy(pt_to_priv_glib(pt)->hrtimer);

	pt_to_priv_glib(pt)->hrtimer.gs = g_timeout_source_new(ms);
	if (!pt_to_priv_glib(pt)->hrtimer.gs)
		return 1;

	g_source_set_callback(pt_to_priv_glib(pt)->hrtimer.gs,
			      lws_glib_hrtimer_cb, pt, NULL);
	pt_to_priv_glib(pt)->hrtimer.tag = g_source_attach(
						pt_to_priv_glib(pt)->hrtimer.gs,
					        pt_to_g_main_context(pt));

	return 0;
}

static gboolean
lws_glib_dispatch(GSource *src, GSourceFunc x, gpointer userData)
{
	struct lws_io_watcher_glib_subclass *sub =
			(struct lws_io_watcher_glib_subclass *)src;
	struct lws_context_per_thread *pt;
	struct lws_pollfd eventfd;
	GIOCondition cond;

	cond = g_source_query_unix_fd(src, sub->tag);
	eventfd.revents = (short)cond;

	/* translate from glib event namespace to platform */

	if (cond & G_IO_IN)
		eventfd.revents |= LWS_POLLIN;
	if (cond & G_IO_OUT)
		eventfd.revents |= LWS_POLLOUT;
	if (cond & G_IO_ERR)
		eventfd.revents |= LWS_POLLHUP;
	if (cond & G_IO_HUP)
		eventfd.revents |= LWS_POLLHUP;

	eventfd.events = eventfd.revents;
	eventfd.fd = sub->wsi->desc.sockfd;

	lwsl_wsi_debug(sub->wsi, "fd %d, events %d",
				 eventfd.fd, eventfd.revents);

	pt = &sub->wsi->a.context->pt[(int)sub->wsi->tsi];
	if (pt->is_destroyed)
		return G_SOURCE_CONTINUE;

	lws_service_fd_tsi(sub->wsi->a.context, &eventfd, sub->wsi->tsi);

	if (!lws_gs_valid(pt_to_priv_glib(pt)->idle))
		lws_glib_set_idle(pt);

	if (pt->destroy_self)
		lws_context_destroy(pt->context);

	return G_SOURCE_CONTINUE;
}

static const GSourceFuncs lws_glib_source_ops = {
    .prepare	= NULL,
    .check	= lws_glib_check,
    .dispatch	= lws_glib_dispatch,
    .finalize	= NULL,
};

/*
 * This is the callback for a timer object that is set to the earliest scheduled
 * lws event... it services any lws scheduled events that are ready, and then
 * resets the event loop timer to the earliest remaining event, if any.
 */

static gboolean
lws_glib_hrtimer_cb(void *p)
{
	struct lws_context_per_thread *pt = (struct lws_context_per_thread *)p;
	unsigned int ms;
	lws_usec_t us;

	lws_pt_lock(pt, __func__);

	lws_gs_destroy(pt_to_priv_glib(pt)->hrtimer);

	us = __lws_sul_service_ripe(pt->pt_sul_owner, LWS_COUNT_PT_SUL_OWNERS,
				    lws_now_usecs());
	if (us) {
		ms = (unsigned int)(us / LWS_US_PER_MS);
		if (!ms)
			ms = 1;

		lws_glib_set_timeout(pt, ms);
	}

	lws_pt_unlock(pt);

	lws_glib_set_idle(pt);

	return FALSE; /* stop it repeating */
}

static gboolean
lws_glib_idle_timer_cb(void *p)
{
	struct lws_context_per_thread *pt = (struct lws_context_per_thread *)p;

	if (pt->is_destroyed)
		return FALSE;

	lws_service_do_ripe_rxflow(pt);
	lws_glib_hrtimer_cb(pt);

	/*
	 * is there anybody with pending stuff that needs service forcing?
	 */
	if (!lws_service_adjust_timeout(pt->context, 1, pt->tid)) {
		/* -1 timeout means just do forced service */
		_lws_plat_service_forced_tsi(pt->context, pt->tid);
		/* still somebody left who wants forced service? */
		if (!lws_service_adjust_timeout(pt->context, 1, pt->tid))
			return TRUE;
	}

	if (pt->destroy_self)
		lws_context_destroy(pt->context);

	/*
	 * For glib, this disables the idle callback.  Otherwise we keep
	 * coming back here immediately endlessly.
	 *
	 * We reenable the idle callback on the next network or scheduled event
	 */

	lws_gs_destroy(pt_to_priv_glib(pt)->idle);

	return FALSE;
}

void
lws_glib_sigint_cb(void *ctx)
{
	struct lws_context_per_thread *pt = ctx;

	pt->inside_service = 1;

	if (pt->context->eventlib_signal_cb) {
		pt->context->eventlib_signal_cb(NULL, 0);

		return;
	}
	if (!pt->event_loop_foreign)
		g_main_loop_quit(pt_to_loop(pt));
}

static int
elops_init_context_glib(struct lws_context *context,
			 const struct lws_context_creation_info *info)
{
//	int n;

	context->eventlib_signal_cb = info->signal_cb;

//	for (n = 0; n < context->count_threads; n++)
//		pt_to_priv_glib(&context->pt[n])->w_sigint.context = context;

	return 0;
}

static int
elops_accept_glib(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_wsi_eventlibs_glib *wsipr = wsi_to_priv_glib(wsi);
	int fd;

	assert(!wsi_to_subclass(wsi));

	wsi_to_subclass(wsi) = (struct lws_io_watcher_glib_subclass *)
			g_source_new((GSourceFuncs *)&lws_glib_source_ops,
						sizeof(*wsi_to_subclass(wsi)));
	if (!wsi_to_subclass(wsi))
		return 1;

	wsipr->w_read.context = wsi->a.context;
	wsi_to_subclass(wsi)->wsi = wsi;

	if (wsi->role_ops->file_handle)
		fd = wsi->desc.filefd;
	else
		fd = wsi->desc.sockfd;

	wsi_to_subclass(wsi)->tag = g_source_add_unix_fd(wsi_to_gsource(wsi),
						fd, (GIOCondition)LWS_POLLIN);
	wsipr->w_read.actual_events = LWS_POLLIN;

	g_source_set_callback(wsi_to_gsource(wsi),
			G_SOURCE_FUNC(lws_service_fd), wsi->a.context, NULL);

	g_source_attach(wsi_to_gsource(wsi), pt_to_g_main_context(pt));

	return 0;
}

static int
elops_listen_init_glib(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);

	elops_accept_glib(wsi);

	return 0;
}

static int
elops_init_pt_glib(struct lws_context *context, void *_loop, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_glib *ptpr = pt_to_priv_glib(pt);
	GMainLoop *loop = (GMainLoop *)_loop;

	if (!loop)
		loop = g_main_loop_new(NULL, 0);
	else
		context->pt[tsi].event_loop_foreign = 1;

	if (!loop) {
		lwsl_cx_err(context, "creating glib loop failed");

		return -1;
	}

	ptpr->loop = loop;

	lws_vhost_foreach_listen_wsi(context, NULL, elops_listen_init_glib);

	lws_glib_set_idle(pt);

	/* Register the signal watcher unless it's a foreign loop */

	if (pt->event_loop_foreign)
		return 0;

	ptpr->sigint.tag = g_unix_signal_add(SIGINT,
					G_SOURCE_FUNC(lws_glib_sigint_cb), pt);

	return 0;
}

/*
 * We are changing the event wait for this guy
 */

static void
elops_io_glib(struct lws *wsi, unsigned int flags)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_wsi_eventlibs_glib *wsipr = wsi_to_priv_glib(wsi);
	GIOCondition cond = wsipr->w_read.actual_events | G_IO_ERR;

	if (!pt_to_loop(pt) || wsi->a.context->being_destroyed ||
	    pt->is_destroyed)
		return;

	if (!wsi_to_subclass(wsi))
		return;

	/*
	 * We are being given individual set / clear operations using
	 * LWS_EV_ common namespace, convert them to glib namespace bitfield
	 */

	if (flags & LWS_EV_READ) {
		if (flags & LWS_EV_STOP)
			cond &= (unsigned int)~(G_IO_IN | G_IO_HUP);
		else
			cond |= G_IO_IN | G_IO_HUP;
	}

	if (flags & LWS_EV_WRITE) {
		if (flags & LWS_EV_STOP)
			cond &= (unsigned int)~G_IO_OUT;
		else
			cond |= G_IO_OUT;
	}

	wsipr->w_read.actual_events = (uint8_t)cond;

	lwsl_wsi_debug(wsi, "fd %d, 0x%x/0x%x", wsi->desc.sockfd,
						flags, (int)cond);

	g_source_modify_unix_fd(wsi_to_gsource(wsi), wsi_to_subclass(wsi)->tag,
				cond);
}

static void
elops_run_pt_glib(struct lws_context *context, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];

	if (pt_to_loop(pt))
		g_main_loop_run(pt_to_loop(pt));
}

static void
elops_destroy_wsi_glib(struct lws *wsi)
{
	struct lws_context_per_thread *pt;

	if (!wsi)
		return;

	pt = &wsi->a.context->pt[(int)wsi->tsi];
	if (pt->is_destroyed)
		return;

	if (!wsi_to_gsource(wsi))
		return;

	if (wsi_to_subclass(wsi)->tag) {
		g_source_remove_unix_fd(wsi_to_gsource(wsi),
					wsi_to_subclass(wsi)->tag);
		wsi_to_subclass(wsi)->tag = NULL;
	}

	g_source_destroy(wsi_to_gsource(wsi));
	g_source_unref(wsi_to_gsource(wsi));
	wsi_to_subclass(wsi) = NULL;
}

static int
elops_listen_destroy_glib(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);

	elops_destroy_wsi_glib(wsi);

	return 0;
}

static void
elops_destroy_pt_glib(struct lws_context *context, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_glib *ptpr = pt_to_priv_glib(pt);

	if (!pt_to_loop(pt))
		return;

	lws_vhost_foreach_listen_wsi(context, NULL, elops_listen_destroy_glib);

	lws_gs_destroy(ptpr->idle);
	lws_gs_destroy(ptpr->hrtimer);

	if (!pt->event_loop_foreign) {
		g_main_loop_quit(pt_to_loop(pt));
		lws_gs_destroy(ptpr->sigint);
		g_main_loop_unref(pt_to_loop(pt));
	}

	pt_to_loop(pt) = NULL;
}

static int
elops_destroy_context2_glib(struct lws_context *context)
{
	struct lws_context_per_thread *pt = &context->pt[0];
	int n;

	for (n = 0; n < (int)context->count_threads; n++) {
		if (!pt->event_loop_foreign)
			g_main_loop_quit(pt_to_loop(pt));
		pt++;
	}

	return 0;
}

static int
elops_wsi_logical_close_glib(struct lws *wsi)
{
	elops_destroy_wsi_glib(wsi);

	return 0;
}

static const struct lws_event_loop_ops event_loop_ops_glib = {
	/* name */			"glib",
	/* init_context */		elops_init_context_glib,
	/* destroy_context1 */		NULL,
	/* destroy_context2 */		elops_destroy_context2_glib,
	/* init_vhost_listen_wsi */	elops_accept_glib,
	/* init_pt */			elops_init_pt_glib,
	/* wsi_logical_close */		elops_wsi_logical_close_glib,
	/* check_client_connect_ok */	NULL,
	/* close_handle_manually */	NULL,
	/* accept */			elops_accept_glib,
	/* io */			elops_io_glib,
	/* run_pt */			elops_run_pt_glib,
	/* destroy_pt */		elops_destroy_pt_glib,
	/* destroy wsi */		elops_destroy_wsi_glib,
	/* foreign_thread */		NULL,

	/* flags */			LELOF_DESTROY_FINAL,

	/* evlib_size_ctx */	0,
	/* evlib_size_pt */	sizeof(struct lws_pt_eventlibs_glib),
	/* evlib_size_vh */	0,
	/* evlib_size_wsi */	sizeof(struct lws_io_watcher_glib),
};

#if defined(LWS_WITH_EVLIB_PLUGINS)
LWS_VISIBLE
#endif
const lws_plugin_evlib_t evlib_glib = {
	.hdr = {
		"glib event loop",
		"lws_evlib_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.ops	= &event_loop_ops_glib
};
