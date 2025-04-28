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
#include "private-lib-event-libs-libev.h"

#define pt_to_priv_ev(_pt) ((struct lws_pt_eventlibs_libev *)(_pt)->evlib_pt)
#define vh_to_priv_ev(_vh) ((struct lws_vh_eventlibs_libev *)(_vh)->evlib_vh)
#define wsi_to_priv_ev(_w) ((struct lws_wsi_eventlibs_libev *)(_w)->evlib_wsi)

static void
lws_ev_hrtimer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents)
{
	struct lws_pt_eventlibs_libev *ptpr = lws_container_of(watcher,
					struct lws_pt_eventlibs_libev, hrtimer);
	struct lws_context_per_thread *pt = ptpr->pt;
	lws_usec_t us;

	lws_pt_lock(pt, __func__);
	us = __lws_sul_service_ripe(pt->pt_sul_owner, LWS_COUNT_PT_SUL_OWNERS,
				    lws_now_usecs());
	if (us) {
		ev_timer_set(&ptpr->hrtimer, ((float)us) / 1000000.0, 0);
		ev_timer_start(ptpr->io_loop, &ptpr->hrtimer);
	}
	lws_pt_unlock(pt);
}

static void
lws_ev_idle_cb(struct ev_loop *loop, struct ev_idle *handle, int revents)
{
	struct lws_pt_eventlibs_libev *ptpr = lws_container_of(handle,
					struct lws_pt_eventlibs_libev, idle);
	struct lws_context_per_thread *pt = ptpr->pt;
	int reschedule = 0;
	lws_usec_t us;

	lws_service_do_ripe_rxflow(pt);

	/*
	 * is there anybody with pending stuff that needs service forcing?
	 */
	if (!lws_service_adjust_timeout(pt->context, 1, pt->tid))
		/* -1 timeout means just do forced service */
		reschedule = _lws_plat_service_forced_tsi(pt->context, pt->tid);

	/* account for hrtimer */

	lws_pt_lock(pt, __func__);
	us = __lws_sul_service_ripe(pt->pt_sul_owner, LWS_COUNT_PT_SUL_OWNERS,
				    lws_now_usecs());
	if (us) {
		ev_timer_set(&ptpr->hrtimer, ((float)us) / 1000000.0, 0);
		ev_timer_start(ptpr->io_loop, &ptpr->hrtimer);
	}
	lws_pt_unlock(pt);

	/* there is nobody who needs service forcing, shut down idle */
	if (!reschedule)
		ev_idle_stop(loop, handle);

	if (pt->destroy_self)
		lws_context_destroy(pt->context);
}

static void
lws_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	struct lws_io_watcher_libev *lws_io = lws_container_of(watcher,
					struct lws_io_watcher_libev, watcher);
	struct lws_context *context = lws_io->context;
	struct lws_pt_eventlibs_libev *ptpr;
	struct lws_context_per_thread *pt;
	struct lws_pollfd eventfd;
	struct lws *wsi;

	if (revents & EV_ERROR)
		return;

	eventfd.fd = watcher->fd;
	eventfd.events = 0;
	eventfd.revents = EV_NONE;

	if (revents & EV_READ) {
		eventfd.events |= LWS_POLLIN;
		eventfd.revents |= LWS_POLLIN;
	}
	if (revents & EV_WRITE) {
		eventfd.events |= LWS_POLLOUT;
		eventfd.revents |= LWS_POLLOUT;
	}

	wsi = wsi_from_fd(context, watcher->fd);
	pt = &context->pt[(int)wsi->tsi];
	ptpr = pt_to_priv_ev(pt);

	lws_service_fd_tsi(context, &eventfd, (int)wsi->tsi);

	ev_idle_start(ptpr->io_loop, &ptpr->idle);
}

void
lws_ev_sigint_cb(struct ev_loop *loop, struct ev_signal *watcher, int revents)
{
	struct lws_context *context = watcher->data;

	if (context->eventlib_signal_cb) {
		context->eventlib_signal_cb((void *)watcher, watcher->signum);

		return;
	}
	ev_break(loop, EVBREAK_ALL);
}

static int
elops_listen_init_ev(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);
	struct lws_context *context = (struct lws_context *)user;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_pt_eventlibs_libev *ptpr = pt_to_priv_ev(pt);
	struct lws_wsi_eventlibs_libev *w = wsi_to_priv_ev(wsi);
	struct lws_vhost *vh = wsi->a.vhost;

	w->w_read.context = context;
	w->w_write.context = context;
	vh_to_priv_ev(vh)->w_accept.context = context;

	ev_io_init(&vh_to_priv_ev(vh)->w_accept.watcher,
		   lws_accept_cb, wsi->desc.sockfd, EV_READ);
	ev_io_start(ptpr->io_loop, &vh_to_priv_ev(vh)->w_accept.watcher);

	return 0;
}

static int
elops_init_pt_ev(struct lws_context *context, void *_loop, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_libev *ptpr = pt_to_priv_ev(pt);
	struct ev_signal *w_sigint = &ptpr->w_sigint.watcher;
	struct ev_loop *loop = (struct ev_loop *)_loop;
	const char *backend_name;
	unsigned int backend;
	int status = 0;

	lwsl_cx_info(context, "loop %p", _loop);

	ptpr->pt = pt;

	if (!loop)
		loop = ev_loop_new(0);
	else
		context->pt[tsi].event_loop_foreign = 1;

	if (!loop) {
		lwsl_cx_err(context, "creating event base failed");

		return -1;
	}

	ptpr->io_loop = loop;

	lws_vhost_foreach_listen_wsi(context, context, elops_listen_init_ev);

	/* Register the signal watcher unless it's a foreign loop */
	if (!context->pt[tsi].event_loop_foreign) {
		ev_signal_init(w_sigint, lws_ev_sigint_cb, SIGINT);
		w_sigint->data = context;
		ev_signal_start(loop, w_sigint);
	}

	backend = ev_backend(loop);
	switch (backend) {
	case EVBACKEND_SELECT:
		backend_name = "select";
		break;
	case EVBACKEND_POLL:
		backend_name = "poll";
		break;
	case EVBACKEND_EPOLL:
		backend_name = "epoll";
		break;
#if defined(LWS_HAVE_EVBACKEND_LINUXAIO)
       case EVBACKEND_LINUXAIO:
               backend_name = "Linux AIO";
               break;
#endif
#if defined(LWS_HAVE_EVBACKEND_IOURING)
       case EVBACKEND_IOURING:
               backend_name = "Linux io_uring";
               break;
#endif
       case EVBACKEND_KQUEUE:
		backend_name = "kqueue";
		break;
	case EVBACKEND_DEVPOLL:
		backend_name = "/dev/poll";
		break;
	case EVBACKEND_PORT:
		backend_name = "Solaris 10 \"port\"";
		break;
	default:
		backend_name = "Unknown libev backend";
		break;
	}

	lwsl_cx_info(context, " libev backend: %s", backend_name);
	(void)backend_name;

	ev_timer_init(&ptpr->hrtimer, lws_ev_hrtimer_cb, 0, 0);
	ptpr->hrtimer.data = pt;

	ev_idle_init(&ptpr->idle, lws_ev_idle_cb);

	return status;
}

static int
elops_listen_destroy_ev(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);
	struct lws_context *context = (struct lws_context *)user;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_pt_eventlibs_libev *ptpr = pt_to_priv_ev(pt);
	struct lws_vhost *vh = wsi->a.vhost;

	ev_io_stop(ptpr->io_loop, &vh_to_priv_ev(vh)->w_accept.watcher);

	return 0;
}

static void
elops_destroy_pt_ev(struct lws_context *context, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_libev *ptpr = pt_to_priv_ev(pt);

	lws_vhost_foreach_listen_wsi(context, context, elops_listen_destroy_ev);

	/* static assets */

	ev_timer_stop(ptpr->io_loop, &ptpr->hrtimer);
	ev_idle_stop(ptpr->io_loop, &ptpr->idle);

	if (!pt->event_loop_foreign)
		ev_signal_stop(ptpr->io_loop, &ptpr->w_sigint.watcher);
}

static int
elops_init_context_ev(struct lws_context *context,
		      const struct lws_context_creation_info *info)
{
	int n;

	context->eventlib_signal_cb = info->signal_cb;

	for (n = 0; n < context->count_threads; n++)
		pt_to_priv_ev(&context->pt[n])->w_sigint.context = context;

	return 0;
}

static int
elops_accept_ev(struct lws *wsi)
{
	struct lws_wsi_eventlibs_libev *w = wsi_to_priv_ev(wsi);
	int fd;

	if (wsi->role_ops->file_handle)
		fd = wsi->desc.filefd;
	else
		fd = wsi->desc.sockfd;

	w->w_read.context = wsi->a.context;
	w->w_write.context = wsi->a.context;

	ev_io_init(&w->w_read.watcher, lws_accept_cb, fd, EV_READ);
	ev_io_init(&w->w_write.watcher, lws_accept_cb, fd, EV_WRITE);

	return 0;
}

static void
elops_io_ev(struct lws *wsi, unsigned int flags)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_pt_eventlibs_libev *ptpr = pt_to_priv_ev(pt);
	struct lws_wsi_eventlibs_libev *w = wsi_to_priv_ev(wsi);

	lwsl_wsi_debug(wsi, "%s flags 0x%x %p %d", wsi->role_ops->name, flags,
						   ptpr->io_loop,
						   pt->is_destroyed);

	if (!ptpr->io_loop || pt->is_destroyed)
		return;

	assert((flags & (LWS_EV_START | LWS_EV_STOP)) &&
	       (flags & (LWS_EV_READ | LWS_EV_WRITE)));

	if (flags & LWS_EV_START) {
		if (flags & LWS_EV_WRITE)
			ev_io_start(ptpr->io_loop, &w->w_write.watcher);
		if (flags & LWS_EV_READ)
			ev_io_start(ptpr->io_loop, &w->w_read.watcher);
	} else {
		if (flags & LWS_EV_WRITE)
			ev_io_stop(ptpr->io_loop, &w->w_write.watcher);
		if (flags & LWS_EV_READ)
			ev_io_stop(ptpr->io_loop, &w->w_read.watcher);
	}

	if (pt->destroy_self)
		lws_context_destroy(pt->context);
}

static void
elops_run_pt_ev(struct lws_context *context, int tsi)
{
	if (pt_to_priv_ev(&context->pt[tsi])->io_loop)
		ev_run(pt_to_priv_ev(&context->pt[tsi])->io_loop, 0);
}

static int
elops_destroy_context2_ev(struct lws_context *context)
{
	struct lws_context_per_thread *pt;
	struct lws_pt_eventlibs_libev *ptpr;
	int n, m;

	for (n = 0; n < context->count_threads; n++) {
		int budget = 1000;

		pt = &context->pt[n];
		ptpr = pt_to_priv_ev(pt);

		/* only for internal loops... */

		if (pt->event_loop_foreign || !ptpr->io_loop)
			continue;

		if (!context->evlib_finalize_destroy_after_int_loops_stop) {
			ev_break(ptpr->io_loop, EVBREAK_ONE);
			continue;
		}
		while (budget-- &&
		       (m = ev_run(ptpr->io_loop, 0)))
			;

		ev_loop_destroy(ptpr->io_loop);
	}

	return 0;
}

static int
elops_init_vhost_listen_wsi_ev(struct lws *wsi)
{
	struct lws_wsi_eventlibs_libev *w;
	int fd;

	if (!wsi) {
		assert(0);
		return 0;
	}

	w = wsi_to_priv_ev(wsi);
	w->w_read.context = wsi->a.context;
	w->w_write.context = wsi->a.context;

	if (wsi->role_ops->file_handle)
		fd = wsi->desc.filefd;
	else
		fd = wsi->desc.sockfd;

	ev_io_init(&w->w_read.watcher, lws_accept_cb, fd, EV_READ);
	//ev_io_init(&w->w_write.watcher, lws_accept_cb, fd, EV_WRITE);

	elops_io_ev(wsi, LWS_EV_START | LWS_EV_READ);

	return 0;
}

static void
elops_destroy_wsi_ev(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_pt_eventlibs_libev *ptpr = pt_to_priv_ev(pt);
	struct lws_wsi_eventlibs_libev *w = wsi_to_priv_ev(wsi);

	ev_io_stop(ptpr->io_loop, &w->w_read.watcher);
	ev_io_stop(ptpr->io_loop, &w->w_write.watcher);
}

static const struct lws_event_loop_ops event_loop_ops_ev = {
	/* name */			"libev",
	/* init_context */		elops_init_context_ev,
	/* destroy_context1 */		NULL,
	/* destroy_context2 */		elops_destroy_context2_ev,
	/* init_vhost_listen_wsi */	elops_init_vhost_listen_wsi_ev,
	/* init_pt */			elops_init_pt_ev,
	/* wsi_logical_close */		NULL,
	/* check_client_connect_ok */	NULL,
	/* close_handle_manually */	NULL,
	/* accept */			elops_accept_ev,
	/* io */			elops_io_ev,
	/* run_pt */			elops_run_pt_ev,
	/* destroy_pt */		elops_destroy_pt_ev,
	/* destroy wsi */		elops_destroy_wsi_ev,
	/* foreign_thread */		NULL,

	/* flags */			0,

	/* evlib_size_ctx */	0,
	/* evlib_size_pt */	sizeof(struct lws_pt_eventlibs_libev),
	/* evlib_size_vh */	sizeof(struct lws_vh_eventlibs_libev),
	/* evlib_size_wsi */	sizeof(struct lws_wsi_eventlibs_libev),
};

#if defined(LWS_WITH_EVLIB_PLUGINS)
LWS_VISIBLE
#endif
const lws_plugin_evlib_t evlib_ev = {
	.hdr = {
		"libev event loop",
		"lws_evlib_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.ops	= &event_loop_ops_ev
};
