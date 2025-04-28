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

#include <uv.h>

/*
 * libuv's async destroy cb means that asking to close something doesn't mean
 * you can destroy it or parent things until after the close completes.
 *
 * So we must reference-count creation and close completions with libuv.
 *
 * All "static" (per-pt or per-context) uv handles must
 *
 *  - have their .data set to point to the context
 *
 *  - contribute to context->uv_count_static_asset_handles
 *    counting
 */
#define LWS_UV_REFCOUNT_STATIC_HANDLE_NEW(_x, _pt) \
		{ uv_handle_t *_uht = (uv_handle_t *)(_x); _uht->data = _pt; \
		_pt->count_event_loop_static_asset_handles++; }
#define LWS_UV_REFCOUNT_STATIC_HANDLE_TO_PT(_x) \
		((struct lws_context_per_thread *)((uv_handle_t *)((_x)->data)))
#define LWS_UV_REFCOUNT_STATIC_HANDLE_DESTROYED(_x) \
		(--(LWS_UV_REFCOUNT_STATIC_HANDLE_TO_PT(_x)-> \
				count_event_loop_static_asset_handles))

struct lws_signal_watcher_libuv {
	uv_signal_t watcher;
	struct lws_context *context;
};

struct lws_pt_eventlibs_libuv {
	uv_loop_t			*io_loop;
	struct lws_context_per_thread	*pt;
	uv_signal_t			signals[8];
	uv_timer_t			sultimer;
	uv_idle_t			idle;

	uv_thread_t			uv_thread;

	struct lws_signal_watcher_libuv w_sigint;
	int				extant_handles;

	char				thread_valid;
};

struct lws_context_eventlibs_libuv {
	uv_loop_t			loop;
};

struct lws_io_watcher_libuv {
	uv_poll_t			*pwatcher;
	struct lws_context		*context;
	uint8_t				actual_events;
};

struct lws_wsi_eventlibs_libuv {
	struct lws_io_watcher_libuv	w_read;
};

uv_loop_t *
lws_uv_getloop(struct lws_context *context, int tsi);

int
lws_uv_plugins_init(struct lws_context *context, const char * const *d);

int
lws_uv_plugins_destroy(struct lws_context *context);
