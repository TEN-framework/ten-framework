/*
 * lws-minimal-http-server-eventlib-foreign
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The glib specific code
 */

#include <libwebsockets.h>

#include <string.h>
#include <signal.h>

#include <glib-2.0/glib.h>
#include <glib-unix.h>

#include "private.h"

#if !defined(G_SOURCE_FUNC)
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))
#endif

typedef struct lws_glib_tag {
	GSource			*gs;
	guint			tag;
} lws_glib_tag_t;

#define lws_gs_valid(t)		  (t.gs)
#define lws_gs_destroy(t)	  if (lws_gs_valid(t)) { \
					g_source_remove(t.tag); \
					g_source_unref(t.gs); \
					t.gs = NULL; t.tag = 0; }

static GMainLoop *loop_glib;
static lws_glib_tag_t timer_outer_glib, sighandler_glib;

static int
timer_cb_glib(void *p)
{
	foreign_timer_service(loop_glib);
	return 1;
}

static void
signal_cb_glib(void *p)
{
	signal_cb(SIGINT);
}

static void
foreign_event_loop_init_and_run_glib(void)
{
	/* we create and start our "foreign loop" */

	loop_glib = g_main_loop_new(NULL, 0);

	sighandler_glib.gs = g_unix_signal_source_new(SIGINT);
	g_source_set_callback(sighandler_glib.gs, G_SOURCE_FUNC(signal_cb_glib),
			      NULL, NULL);
	sighandler_glib.tag = g_source_attach(sighandler_glib.gs,
					    g_main_loop_get_context(loop_glib));

	timer_outer_glib.gs = g_timeout_source_new(1000);
	g_source_set_callback(timer_outer_glib.gs, timer_cb_glib, NULL, NULL);
	timer_outer_glib.tag = g_source_attach(timer_outer_glib.gs,
					   g_main_loop_get_context(loop_glib));

	g_main_loop_run(loop_glib);
}

static void
foreign_event_loop_stop_glib(void)
{
	g_main_loop_quit(loop_glib);
}

static void
foreign_event_loop_cleanup_glib(void)
{
	/* cleanup the foreign loop assets */

	lws_gs_destroy(sighandler_glib);
	lws_gs_destroy(timer_outer_glib);

	g_main_loop_unref(loop_glib);
	loop_glib = NULL;
}

const struct ops ops_glib = {
	foreign_event_loop_init_and_run_glib,
	foreign_event_loop_stop_glib,
	foreign_event_loop_cleanup_glib
};
