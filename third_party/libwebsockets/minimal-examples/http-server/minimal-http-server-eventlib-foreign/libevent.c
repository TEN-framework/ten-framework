/*
 * lws-minimal-http-server-eventlib-foreign
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The libevent specific code
 */

#include <libwebsockets.h>

#include <event2/event.h>

#include <string.h>
#include <signal.h>

#include "private.h"

static struct event_base *loop_event;
static struct event *timer_outer_event;
static struct event *sighandler_event;

static void
timer_cb_event(evutil_socket_t fd, short event, void *arg)
{
	foreign_timer_service(loop_event);
}

static void
signal_cb_event(evutil_socket_t fd, short event, void *arg)
{
	signal_cb((int)(lws_intptr_t)arg);
}

static void
foreign_event_loop_init_and_run_libevent(void)
{
	struct timeval tv;

	/* we create and start our "foreign loop" */

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	event_enable_debug_mode();

	loop_event = event_base_new();
	sighandler_event = evsignal_new((struct event_base *)loop_event, SIGINT, signal_cb_event,
                                       (void*)SIGINT);

	timer_outer_event = event_new((struct event_base *)loop_event, -1, EV_PERSIST,
                                     timer_cb_event, NULL);
	//evtimer_new(loop_event, timer_cb_event, NULL);
	evtimer_add(timer_outer_event, &tv);

	event_base_loop(loop_event, 0);
}

static void
foreign_event_loop_stop_libevent(void)
{
	event_base_loopexit(loop_event, NULL);
}

static void
foreign_event_loop_cleanup_libevent(void)
{
	/* cleanup the foreign loop assets */

	evtimer_del(timer_outer_event);
	event_free(timer_outer_event);
	evsignal_del(sighandler_event);
	event_free(sighandler_event);

	event_base_loop(loop_event, 0);
	event_base_free(loop_event);
}

const struct ops ops_libevent = {
	foreign_event_loop_init_and_run_libevent,
	foreign_event_loop_stop_libevent,
	foreign_event_loop_cleanup_libevent
};
