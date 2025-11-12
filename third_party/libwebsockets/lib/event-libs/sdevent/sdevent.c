#include <systemd/sd-event.h>

#include <private-lib-core.h>
#include "private-lib-event-libs-sdevent.h"

#define pt_to_priv_sd(_pt) ((struct lws_pt_eventlibs_sdevent *)(_pt)->evlib_pt)
#define wsi_to_priv_sd(_w) ((struct lws_wsi_watcher_sdevent *)(_w)->evlib_wsi)

struct lws_pt_eventlibs_sdevent {
	struct lws_context_per_thread *pt;
	struct sd_event *io_loop;
	struct sd_event_source *sultimer;
	struct sd_event_source *idletimer;
};

struct lws_wsi_watcher_sdevent {
	struct sd_event_source *source;
	uint32_t events;
};

static int
sultimer_handler(sd_event_source *s, uint64_t usec, void *userdata)
{
	struct lws_context_per_thread *pt = (struct lws_context_per_thread *)userdata;

	lws_usec_t us;

	lws_context_lock(pt->context, __func__);
	lws_pt_lock(pt, __func__);

	us = __lws_sul_service_ripe(pt->pt_sul_owner, LWS_COUNT_PT_SUL_OWNERS,
				    lws_now_usecs());
	if (us) {
		uint64_t at;

		sd_event_now(sd_event_source_get_event(s), CLOCK_MONOTONIC, &at);
		at += (uint64_t)us;
		sd_event_source_set_time(pt_to_priv_sd(pt)->sultimer, at);
		sd_event_source_set_enabled(pt_to_priv_sd(pt)->sultimer,
					    SD_EVENT_ONESHOT);
	}

	lws_pt_unlock(pt);
	lws_context_unlock(pt->context);

	return 0;
}

static int
idle_handler(sd_event_source *s, uint64_t usec, void *userdata)
{
	struct lws_context_per_thread *pt = (struct lws_context_per_thread *)userdata;

	lws_usec_t us;

	lws_service_do_ripe_rxflow(pt);

	lws_context_lock(pt->context, __func__);
	lws_pt_lock(pt, __func__);

	/*
	 * is there anybody with pending stuff that needs service forcing?
	 */
	 if (!lws_service_adjust_timeout(pt->context, 1, pt->tid))
		 /* -1 timeout means just do forced service */
		 _lws_plat_service_forced_tsi(pt->context, pt->tid);

	 /* account for sultimer */

	 us = __lws_sul_service_ripe(pt->pt_sul_owner, LWS_COUNT_PT_SUL_OWNERS,
				     lws_now_usecs());

	 if (us) {
		 uint64_t at;

		 sd_event_now(sd_event_source_get_event(s), CLOCK_MONOTONIC, &at);
		 at += (uint64_t)us;
		 sd_event_source_set_time(pt_to_priv_sd(pt)->sultimer, at);
		 sd_event_source_set_enabled(pt_to_priv_sd(pt)->sultimer,
					     SD_EVENT_ONESHOT);
	 }

	 sd_event_source_set_enabled(pt_to_priv_sd(pt)->idletimer, SD_EVENT_OFF);

	 lws_pt_unlock(pt);
	 lws_context_unlock(pt->context);

	 return 0;
}

static int
sock_accept_handler(sd_event_source *s, int fd, uint32_t revents, void *userdata)
{
	struct lws *wsi = (struct lws *)userdata;
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct sd_event_source *idletimer, *watcher;
	struct lws_pollfd eventfd;

	lws_context_lock(pt->context, __func__);
	lws_pt_lock(pt, __func__);

	if (pt->is_destroyed)
		goto bail;

	eventfd.fd = fd;
	eventfd.events = 0;
	eventfd.revents = 0;

	if (revents & EPOLLIN) {
		eventfd.events |= LWS_POLLIN;
		eventfd.revents |= LWS_POLLIN;
	}

	if (revents & EPOLLOUT) {
		eventfd.events |= LWS_POLLOUT;
		eventfd.revents |= LWS_POLLOUT;
	}

	lws_pt_unlock(pt);
	lws_context_unlock(pt->context);

	lws_service_fd_tsi(context, &eventfd, wsi->tsi);

	if (pt->destroy_self) {
		lws_context_destroy(pt->context);
		return -1;
	}

	/* fire idle handler */
	idletimer = pt_to_priv_sd(pt)->idletimer;
	if (idletimer) {
		sd_event_source_set_time(idletimer, (uint64_t) 0);
		sd_event_source_set_enabled(idletimer, SD_EVENT_ON);
	}

	/*
	 * allow further events
	 *
	 * Note:
	 * do not move the assignment up, lws_service_fd_tsi may invalidate it!
	 */
	watcher = wsi_to_priv_sd(wsi)->source;
	if (watcher)
		sd_event_source_set_enabled(watcher, SD_EVENT_ONESHOT);

	return 0;

bail:
	lws_pt_unlock(pt);
	lws_context_unlock(pt->context);

	return -1;
}

static void
io_sd(struct lws *wsi, unsigned int flags)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	/*
	 * Only manipulate if there is an event source, and if
	 * the pt is still alive
	 */
	if (!pt_to_priv_sd(pt)->io_loop ||
	    !wsi_to_priv_sd(wsi)->source ||
	    pt->is_destroyed)
		return;

	// assert that the requested flags do not contain anything unexpected
	if (!((flags & (LWS_EV_START | LWS_EV_STOP)) &&
	    (flags & (LWS_EV_READ | LWS_EV_WRITE)))) {
		lwsl_wsi_err(wsi, "assert: flags %d", flags);
		assert(0);
	}

	// we are overdoing a bit here, so it resembles the structure in libuv.c
	if (flags & LWS_EV_START) {
		if (flags & LWS_EV_WRITE)
			wsi_to_priv_sd(wsi)->events |= EPOLLOUT;

		if (flags & LWS_EV_READ)
			wsi_to_priv_sd(wsi)->events |= EPOLLIN;

		sd_event_source_set_io_events(wsi_to_priv_sd(wsi)->source,
					      wsi_to_priv_sd(wsi)->events);
		sd_event_source_set_enabled(wsi_to_priv_sd(wsi)->source,
					    SD_EVENT_ONESHOT);
	} else {
		if (flags & LWS_EV_WRITE)
			wsi_to_priv_sd(wsi)->events =
				wsi_to_priv_sd(wsi)->events &
					(uint32_t)(~EPOLLOUT);

		if (flags & LWS_EV_READ)
			wsi_to_priv_sd(wsi)->events =
				wsi_to_priv_sd(wsi)->events &
					(uint32_t)(~EPOLLIN);

		sd_event_source_set_io_events(wsi_to_priv_sd(wsi)->source,
					      wsi_to_priv_sd(wsi)->events);

		if (!(wsi_to_priv_sd(wsi)->events & (EPOLLIN | EPOLLOUT)))
			sd_event_source_set_enabled(wsi_to_priv_sd(wsi)->source,
						    SD_EVENT_ONESHOT);
		else
			sd_event_source_set_enabled(wsi_to_priv_sd(wsi)->source,
						    SD_EVENT_OFF);
	}
}

static int
init_vhost_listen_wsi_sd(struct lws *wsi)
{
	struct lws_context_per_thread *pt;

	if (!wsi)
		return 0;

	pt = &wsi->a.context->pt[(int)wsi->tsi];

	sd_event_add_io(pt_to_priv_sd(pt)->io_loop,
			&wsi_to_priv_sd(wsi)->source,
			wsi->desc.sockfd,
			wsi_to_priv_sd(wsi)->events,
			sock_accept_handler,
			wsi);

	io_sd(wsi, LWS_EV_START | LWS_EV_READ);

	return 0;
}

static int
elops_listen_init_sdevent(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);

	if (init_vhost_listen_wsi_sd(wsi) == -1)
		return -1;

	return 0;
}

static int
init_pt_sd(struct lws_context *context, void *_loop, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_sdevent *ptpriv = pt_to_priv_sd(pt);
	struct sd_event *loop = (struct sd_event *)_loop;
	int first = 1;  /* first to create and initialize the loop */

	ptpriv->pt = pt;

	/* make sure we have an event loop */
	if (!ptpriv->io_loop) {
		if (!loop) {
			if (sd_event_default(&loop) < 0) {
				lwsl_cx_err(context, "sd_event_default failed");

				return -1;
			}
			pt->event_loop_foreign = 0;
		} else {
			sd_event_ref(loop);
			pt->event_loop_foreign = 1;
		}

		ptpriv->io_loop = loop;
	} else
		 /*
		  * If the loop was initialized before, we do not need to
		  * do full initialization
		  */
		first = 0;

	lws_vhost_foreach_listen_wsi(context, NULL, elops_listen_init_sdevent);

	if (first) {

		if (0 > sd_event_add_time(loop,
				&ptpriv->sultimer,
				CLOCK_MONOTONIC,
				UINT64_MAX,
				0,
				sultimer_handler,
				(void*) pt
		))
			return -1;

		if (0 > sd_event_add_time(loop,
				&ptpriv->idletimer,
				CLOCK_MONOTONIC,
				0,
				0,
				idle_handler,
				(void *)pt))
			return -1;

		sd_event_source_set_enabled(ptpriv->idletimer, SD_EVENT_ON);

		if (0 > sd_event_source_set_priority(ptpriv->idletimer,
						     SD_EVENT_PRIORITY_IDLE))
			return -1;

	}

	return 0;
}

static void
wsi_destroy_sd(struct lws *wsi)
{
	if (!wsi)
		return;

	io_sd(wsi, LWS_EV_STOP | (LWS_EV_READ | LWS_EV_WRITE));

	if (wsi_to_priv_sd(wsi)->source) {
		sd_event_source_set_enabled(wsi_to_priv_sd(wsi)->source,
					    SD_EVENT_OFF);
		sd_event_source_unref(wsi_to_priv_sd(wsi)->source);
		wsi_to_priv_sd(wsi)->source = NULL;
	}
}

static int
wsi_logical_close_sd(struct lws *wsi)
{
	wsi_destroy_sd(wsi);

	return 0;
}

static int
sock_accept_sd(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	if (wsi->role_ops->file_handle)
		sd_event_add_io(pt_to_priv_sd(pt)->io_loop,
				&wsi_to_priv_sd(wsi)->source,
				wsi->desc.filefd,
				wsi_to_priv_sd(wsi)->events,
				sock_accept_handler,
				wsi);
	else
		sd_event_add_io(pt_to_priv_sd(pt)->io_loop,
				&wsi_to_priv_sd(wsi)->source,
				wsi->desc.sockfd,
				wsi_to_priv_sd(wsi)->events,
				sock_accept_handler,
				wsi);

	return 0;
}

static void
run_pt_sd(struct lws_context *context, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_sdevent *ptpriv = pt_to_priv_sd(pt);

	if (ptpriv->io_loop)
		sd_event_run(ptpriv->io_loop, (uint64_t) -1);
}

static int
elops_listen_destroy_sdevent(struct lws_dll2 *d, void *user)
{
	struct lws *wsi = lws_container_of(d, struct lws, listen_list);

	wsi_logical_close_sd(wsi);

	return 0;
}

static void
destroy_pt_sd(struct lws_context *context, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_pt_eventlibs_sdevent *ptpriv = pt_to_priv_sd(pt);

	lws_vhost_foreach_listen_wsi(context, NULL, elops_listen_destroy_sdevent);

	if (ptpriv->sultimer) {
		sd_event_source_set_enabled(ptpriv->sultimer,
					    SD_EVENT_OFF);
		sd_event_source_unref(ptpriv->sultimer);
		ptpriv->sultimer = NULL;
	}

	if (ptpriv->idletimer) {
		sd_event_source_set_enabled(ptpriv->idletimer,
					    SD_EVENT_OFF);
		sd_event_source_unref(ptpriv->idletimer);
		ptpriv->idletimer = NULL;
	}

	if (ptpriv->io_loop) {
		sd_event_unref(ptpriv->io_loop);
		ptpriv->io_loop = NULL;
	}
}

const struct lws_event_loop_ops event_loop_ops_sdevent = {
		.name				= "sdevent",
		.init_context			= NULL,
		.destroy_context1		= NULL,
		.destroy_context2		= NULL,
		.init_vhost_listen_wsi		= init_vhost_listen_wsi_sd,
		.init_pt			= init_pt_sd,
		.wsi_logical_close		= wsi_logical_close_sd,
		.check_client_connect_ok	= NULL,
		.close_handle_manually		= NULL,
		.sock_accept			= sock_accept_sd,
		.io				= io_sd,
		.run_pt				= run_pt_sd,
		.destroy_pt			= destroy_pt_sd,
		.destroy_wsi			= wsi_destroy_sd,

		.flags				= 0,

		.evlib_size_ctx			= 0,
		.evlib_size_pt			= sizeof(struct lws_pt_eventlibs_sdevent),
		.evlib_size_vh			= 0,
		.evlib_size_wsi			= sizeof(struct lws_wsi_watcher_sdevent),
};

#if defined(LWS_WITH_EVLIB_PLUGINS)
LWS_VISIBLE
#endif
const lws_plugin_evlib_t evlib_sd = {
		.hdr = {
				"systemd event loop",
				"lws_evlib_plugin",
				LWS_BUILD_HASH,
				LWS_PLUGIN_API_MAGIC
		},

		.ops	= &event_loop_ops_sdevent
};
