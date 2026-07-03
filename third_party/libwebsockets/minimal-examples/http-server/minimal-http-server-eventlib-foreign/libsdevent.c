/*
 * lws-minimal-http-server-eventlib-foreign
 *
 * Written in 2020 by Christian Fuchs <christian.fuchs@scs.ch>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The sdevent specific code
 */

#include <libwebsockets.h>

#include <string.h>
#include <signal.h>

#include <systemd/sd-event.h>

#include "private.h"

static struct sd_event *sd_loop;
static sd_event_source *sd_timer;
static sd_event_source *sd_signal;

static int
timer_cb_sd(sd_event_source *source, uint64_t now, void *user)
{
	foreign_timer_service(sd_loop);

	if (sd_timer) {
		sd_event_source_set_time(sd_timer, now + 1000000);
		sd_event_source_set_enabled(sd_timer, SD_EVENT_ON);
	}

	return 0;
}

static int
signal_cb_sd(sd_event_source *source, const struct signalfd_siginfo *si,
             void *user)
{
	signal_cb((int)si->ssi_signo);
	return 0;
}

static void
foreign_event_loop_init_and_run_libsdevent(void)
{
	uint64_t now;

	/* we create and start our "foreign loop" */

	sd_event_default(&sd_loop);
	sd_event_add_signal(sd_loop, &sd_signal, SIGINT, signal_cb_sd, NULL);

	sd_event_now(sd_loop, CLOCK_MONOTONIC, &now);
	sd_event_add_time(sd_loop, &sd_timer, CLOCK_MONOTONIC, now,
			  (uint64_t) 1000, timer_cb_sd, NULL);

	sd_event_loop(sd_loop);
}

static void
foreign_event_loop_stop_libsdevent(void)
{
	sd_event_exit(sd_loop, 0);
}

static void
foreign_event_loop_cleanup_libsdevent(void)
{
	sd_event_source_set_enabled(sd_timer, SD_EVENT_OFF);
	sd_timer = sd_event_source_unref(sd_timer);

	sd_event_source_set_enabled(sd_signal, SD_EVENT_OFF);
	sd_signal = sd_event_source_unref(sd_signal);

	sd_loop = sd_event_unref(sd_loop);
}

const struct ops ops_sdevent = {
	foreign_event_loop_init_and_run_libsdevent,
	foreign_event_loop_stop_libsdevent,
	foreign_event_loop_cleanup_libsdevent
};

