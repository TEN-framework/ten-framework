/*
 * lws-minimal-http-server-eventlib-foreign
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The libev specific code
 */

#include <libwebsockets.h>

#include <ev.h>

#include <string.h>
#include <signal.h>

#include "private.h"

static struct ev_loop *loop_ev;
static struct ev_timer timer_outer_ev;
static struct ev_signal sighandler_ev;

static void
timer_cb_ev(struct ev_loop *loop, struct ev_timer *watcher, int revents)
{
	foreign_timer_service(loop_ev);
}

static void
signal_cb_ev(struct ev_loop *loop, struct ev_signal *watcher, int revents)
{
	signal_cb(watcher->signum);
}

static void
foreign_event_loop_init_and_run_libev(void)
{
	/* we create and start our "foreign loop" */

	loop_ev = ev_loop_new(0);

	ev_signal_init(&sighandler_ev, signal_cb_ev, SIGINT);
	ev_signal_start(loop_ev, &sighandler_ev);

	ev_timer_init(&timer_outer_ev, timer_cb_ev, 0, 1);
	ev_timer_start(loop_ev, &timer_outer_ev);

	ev_run(loop_ev, 0);
}

static void
foreign_event_loop_stop_libev(void)
{
	ev_break(loop_ev, EVBREAK_ALL);
}

static void
foreign_event_loop_cleanup_libev(void)
{
	/* cleanup the foreign loop assets */

	ev_timer_stop(loop_ev, &timer_outer_ev);
	ev_signal_stop(loop_ev, &sighandler_ev);

	ev_run(loop_ev, 0);
	ev_loop_destroy(loop_ev);
}

const struct ops ops_libev = {
	foreign_event_loop_init_and_run_libev,
	foreign_event_loop_stop_libev,
	foreign_event_loop_cleanup_libev
};

