/*
 * lws-minimal-http-server-eventlib-foreign
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The uloop specific code
 */

#include <libwebsockets.h>

#include <libubox/uloop.h>

#include <string.h>
#include <signal.h>

#include "private.h"

static struct uloop_timeout timer_outer_uloop;

static void
timer_cb_uloop(struct uloop_timeout *ti)
{
	foreign_timer_service(NULL);
	uloop_timeout_set(&timer_outer_uloop, 1090);
}

static void
foreign_event_loop_init_and_run_uloop(void)
{
	uloop_init();

	timer_outer_uloop.cb = timer_cb_uloop;
	uloop_timeout_add(&timer_outer_uloop);

	uloop_timeout_set(&timer_outer_uloop, 1090);

	uloop_run();
}

static void
foreign_event_loop_stop_uloop(void)
{
	uloop_end();
}

static void
foreign_event_loop_cleanup_uloop(void)
{
	uloop_timeout_cancel(&timer_outer_uloop);
}

const struct ops ops_uloop = {
	foreign_event_loop_init_and_run_uloop,
	foreign_event_loop_stop_uloop,
	foreign_event_loop_cleanup_uloop
};
