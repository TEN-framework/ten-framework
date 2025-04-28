/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
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

#include <ev.h>

#define LWS_EV_REFCOUNT_STATIC_HANDLE_NEW(_x, _ctx) \
		{ (_x)->data = _ctx; \
		_ctx->count_event_loop_static_asset_handles++; }
#define LWS_EV_REFCOUNT_STATIC_HANDLE_TO_CONTEXT(_x) \
			((struct lws_context *)(_x)->data)))
#define LWS_EV_REFCOUNT_STATIC_HANDLE_DESTROYED(_x) \
		(--(LWS_UV_REFCOUNT_STATIC_HANDLE_TO_CONTEXT(_x)-> \
				count_event_loop_static_asset_handles))

struct lws_signal_watcher_libev {
	ev_signal watcher;
	struct lws_context *context;
};

struct lws_pt_eventlibs_libev {
	struct ev_loop *io_loop;
	struct ev_timer hrtimer;
	struct ev_idle idle;
	struct lws_signal_watcher_libev w_sigint;
	struct lws_context_per_thread *pt;
};

struct lws_io_watcher_libev {
	ev_io watcher;
	struct lws_context *context;
};

struct lws_vh_eventlibs_libev {
	struct lws_io_watcher_libev w_accept;
};

struct lws_wsi_eventlibs_libev {
	struct lws_io_watcher_libev w_read;
	struct lws_io_watcher_libev w_write;
};

