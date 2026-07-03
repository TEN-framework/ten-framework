 /*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
 * IN THE SOFTWARE
 */

#include <private-lib-core.h>
#include "private-lib-event-libs-poll.h"

static int
elops_foreign_thread_poll(struct lws_context *cx, int tsi)
{
	struct lws_context_per_thread *pt = &cx->pt[tsi];
	volatile struct lws_context_per_thread *vpt =
				(volatile struct lws_context_per_thread *)pt;

	/*
	 * To avoid mandating a specific threading library, we can check
	 * probabilistically by seeing if the lws default wait is still asleep
	 * at the time we are checking, if it is then we cannot be being called
	 * by the event loop loop thread.
	 */

	return vpt->inside_poll;
}

struct lws_event_loop_ops event_loop_ops_poll = {
	.name				= "poll",

	.foreign_thread			= elops_foreign_thread_poll,

	.flags				= LELOF_ISPOLL,
};

const lws_plugin_evlib_t evlib_poll = {
	.hdr = {
		"poll",
		"lws_evlib_plugin",
		"n/a",
		LWS_PLUGIN_API_MAGIC
	},

	.ops	= &event_loop_ops_poll
};
