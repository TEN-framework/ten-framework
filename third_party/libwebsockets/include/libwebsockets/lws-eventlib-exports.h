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
 * IN THE SOFTWARE.
 *
 * These are exports needed by event lib plugins.
 */

enum lws_event_lib_ops_flags {
	LELOF_ISPOLL				= (1 >> 0),
	LELOF_DESTROY_FINAL			= (1 >> 1),
};

enum {
	LWS_EV_READ				= (1 << 0),
	LWS_EV_WRITE				= (1 << 1),
	LWS_EV_START				= (1 << 2),
	LWS_EV_STOP				= (1 << 3),
};

struct lws_event_loop_ops {
	const char *name;
	/* event loop-specific context init during context creation */
	int (*init_context)(struct lws_context *context,
			    const struct lws_context_creation_info *info);
	/* called during lws_destroy_context */
	int (*destroy_context1)(struct lws_context *context);
	/* called during lws_destroy_context2 */
	int (*destroy_context2)(struct lws_context *context);
	/* init vhost listening wsi */
	int (*init_vhost_listen_wsi)(struct lws *wsi);
	/* init the event loop for a pt */
	int (*init_pt)(struct lws_context *context, void *_loop, int tsi);
	/* called at end of first phase of close_free_wsi()  */
	int (*wsi_logical_close)(struct lws *wsi);
	/* return nonzero if client connect not allowed  */
	int (*check_client_connect_ok)(struct lws *wsi);
	/* close handle manually  */
	void (*close_handle_manually)(struct lws *wsi);
	/* event loop accept processing  */
	int (*sock_accept)(struct lws *wsi);
	/* control wsi active events  */
	void (*io)(struct lws *wsi, unsigned int flags);
	/* run the event loop for a pt */
	void (*run_pt)(struct lws_context *context, int tsi);
	/* called before pt is destroyed */
	void (*destroy_pt)(struct lws_context *context, int tsi);
	/* called just before wsi is freed  */
	void (*destroy_wsi)(struct lws *wsi);
	/* return nonzero if caller thread is not loop service thread  */
	int (*foreign_thread)(struct lws_context *context, int tsi);

	uint8_t	flags;

	uint16_t	evlib_size_ctx;
	uint16_t	evlib_size_pt;
	uint16_t	evlib_size_vh;
	uint16_t	evlib_size_wsi;
};

LWS_VISIBLE LWS_EXTERN void *
lws_evlib_wsi_to_evlib_pt(struct lws *wsi);

LWS_VISIBLE LWS_EXTERN void *
lws_evlib_tsi_to_evlib_pt(struct lws_context *ctx, int tsi);

 /*
  * You should consider these opaque for normal user code.
  */

LWS_VISIBLE LWS_EXTERN void *
lws_realloc(void *ptr, size_t size, const char *reason);

LWS_VISIBLE LWS_EXTERN void
lws_vhost_destroy1(struct lws_vhost *vh);

LWS_VISIBLE LWS_EXTERN void
lws_close_free_wsi(struct lws *wsi, enum lws_close_status reason,
		   const char *caller);

LWS_VISIBLE LWS_EXTERN int
lws_vhost_foreach_listen_wsi(struct lws_context *cx, void *arg,
			     lws_dll2_foreach_cb_t cb);

struct lws_context_per_thread;
LWS_VISIBLE LWS_EXTERN void
lws_service_do_ripe_rxflow(struct lws_context_per_thread *pt);

#if !defined(wsi_from_fd) && !defined(WIN32) && !defined(_WIN32)
struct lws_context;
LWS_VISIBLE LWS_EXTERN struct lws *
wsi_from_fd(const struct lws_context *context, int fd);
#endif

LWS_VISIBLE LWS_EXTERN int
_lws_plat_service_forced_tsi(struct lws_context *context, int tsi);

LWS_VISIBLE LWS_EXTERN void
lws_context_destroy2(struct lws_context *context);

LWS_VISIBLE LWS_EXTERN void
lws_destroy_event_pipe(struct lws *wsi);

LWS_VISIBLE LWS_EXTERN void
__lws_close_free_wsi_final(struct lws *wsi);

#if LWS_MAX_SMP > 1

struct lws_mutex_refcount {
	pthread_mutex_t lock;
	pthread_t lock_owner;
	const char *last_lock_reason;
	char lock_depth;
	char metadata;
};

LWS_VISIBLE LWS_EXTERN void
lws_mutex_refcount_assert_held(struct lws_mutex_refcount *mr);

LWS_VISIBLE LWS_EXTERN void
lws_mutex_refcount_init(struct lws_mutex_refcount *mr);

LWS_VISIBLE LWS_EXTERN void
lws_mutex_refcount_destroy(struct lws_mutex_refcount *mr);

LWS_VISIBLE LWS_EXTERN void
lws_mutex_refcount_lock(struct lws_mutex_refcount *mr, const char *reason);

LWS_VISIBLE LWS_EXTERN void
lws_mutex_refcount_unlock(struct lws_mutex_refcount *mr);

#endif
