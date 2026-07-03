/*
 * lws-minimal-http-server-eventlib-foreign
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The libuv specific code
 */

#include <libwebsockets.h>

#include <string.h>
#include <signal.h>

#include <uv.h>
#ifdef LWS_HAVE_UV_VERSION_H
#include <uv-version.h>
#endif
#ifdef LWS_HAVE_NEW_UV_VERSION_H
#include <uv/version.h>
#endif

#include "private.h"

static uv_loop_t loop_uv;
static uv_timer_t timer_outer_uv;
static uv_signal_t sighandler_uv;

static void
timer_cb_uv(uv_timer_t *t)
{
	foreign_timer_service(&loop_uv);
}

static void
signal_cb_uv(uv_signal_t *watcher, int signum)
{
	signal_cb(signum);
}

static void
foreign_event_loop_init_and_run_libuv(void)
{
	/* we create and start our "foreign loop" */

#if (UV_VERSION_MAJOR > 0) // Travis...
	uv_loop_init(&loop_uv);
#endif
	uv_signal_init(&loop_uv, &sighandler_uv);
	uv_signal_start(&sighandler_uv, signal_cb_uv, SIGINT);

	uv_timer_init(&loop_uv, &timer_outer_uv);
#if (UV_VERSION_MAJOR > 0) // Travis...
	uv_timer_start(&timer_outer_uv, timer_cb_uv, 0, 1000);
#else
	(void)timer_cb_uv;
#endif

	uv_run(&loop_uv, UV_RUN_DEFAULT);
}

static void
foreign_event_loop_stop_libuv(void)
{
	uv_stop(&loop_uv);
}

static void
foreign_event_loop_cleanup_libuv(void)
{
	/* cleanup the foreign loop assets */

	uv_timer_stop(&timer_outer_uv);
	uv_close((uv_handle_t*)&timer_outer_uv, NULL);
	uv_signal_stop(&sighandler_uv);
	uv_close((uv_handle_t *)&sighandler_uv, NULL);

	uv_run(&loop_uv, UV_RUN_DEFAULT);
#if (UV_VERSION_MAJOR > 0) // Travis...
	uv_loop_close(&loop_uv);
#endif
}

const struct ops ops_libuv = {
	foreign_event_loop_init_and_run_libuv,
	foreign_event_loop_stop_libuv,
	foreign_event_loop_cleanup_libuv
};

