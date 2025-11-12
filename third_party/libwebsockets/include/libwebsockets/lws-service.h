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

/** \defgroup service Built-in service loop entry
 *
 * ##Built-in service loop entry
 *
 * If you're not using libev / libuv, these apis are needed to enter the poll()
 * wait in lws and service any connections with pending events.
 */
///@{

/**
 * lws_service() - Service any pending websocket activity
 * \param context:	Websocket context
 * \param timeout_ms:	Set to 0; ignored; for backward compatibility
 *
 *	This function deals with any pending websocket traffic, for three
 *	kinds of event.  It handles these events on both server and client
 *	types of connection the same.
 *
 *	1) Accept new connections to our context's server
 *
 *	2) Call the receive callback for incoming frame data received by
 *	    server or client connections.
 *
 *  Since v3.2 internally the timeout wait is ignored, the lws scheduler is
 *  smart enough to stay asleep until an event is queued.
 */
LWS_VISIBLE LWS_EXTERN int
lws_service(struct lws_context *context, int timeout_ms);

/**
 * lws_service_tsi() - Service any pending websocket activity
 *
 * \param context:	Websocket context
 * \param timeout_ms:	Set to 0; ignored; for backwards compatibility
 * \param tsi:		Thread service index, starting at 0
 *
 * Same as lws_service(), but for a specific thread service index.  Only needed
 * if you are spawning multiple service threads that operate on the same lws_context.
 */
LWS_VISIBLE LWS_EXTERN int
lws_service_tsi(struct lws_context *context, int timeout_ms, int tsi);

/**
 * lws_cancel_service_pt() - Cancel servicing of pending socket activity
 *				on one thread
 * \param wsi:	Cancel service on the thread this wsi is serviced by
 *
 * Same as lws_cancel_service(), but targets a single service thread, the one
 * the wsi belongs to.  You probably want to use lws_cancel_service() instead.
 */
LWS_VISIBLE LWS_EXTERN void
lws_cancel_service_pt(struct lws *wsi);

/**
 * lws_cancel_service() - Cancel wait for new pending socket activity
 * \param context:	Websocket context
 *
 * This function creates an immediate "synchronous interrupt" to the lws poll()
 * wait or event loop.  As soon as possible in the serialzed service sequencing,
 * a LWS_CALLBACK_EVENT_WAIT_CANCELLED callback is sent to every protocol on
 * every vhost.
 *
 * lws_cancel_service() may be called from another thread while the context
 * exists, and its effect will be immediately serialized.
 */
LWS_VISIBLE LWS_EXTERN void
lws_cancel_service(struct lws_context *context);

/**
 * lws_service_fd() - Service polled socket with something waiting
 * \param context:	Websocket context
 * \param pollfd:	The pollfd entry describing the socket fd and which events
 *		happened
 *
 * This function takes a pollfd that has POLLIN or POLLOUT activity and
 * services it according to the state of the associated
 * struct lws.
 *
 * The one call deals with all "service" that might happen on a socket
 * including listen accepts, http files as well as websocket protocol.
 *
 * If a pollfd says it has something, you can just pass it to
 * lws_service_fd() whether it is a socket handled by lws or not.
 * If it sees it is a lws socket, the traffic will be handled and
 * pollfd->revents will be zeroed now.
 *
 * If the socket is foreign to lws, it leaves revents alone.  So you can
 * see if you should service yourself by checking the pollfd revents
 * after letting lws try to service it.
 *
 * lws before v3.2 allowed pollfd to be NULL, to indicate that background
 * periodic processing should be done.  Since v3.2, lws schedules any items
 * that need handling in the future using lws_sul and NULL is no longer valid.
 */
LWS_VISIBLE LWS_EXTERN int
lws_service_fd(struct lws_context *context, struct lws_pollfd *pollfd);

/**
 * lws_service_fd_tsi() - Service polled socket in specific service thread
 * \param context:	Websocket context
 * \param pollfd:	The pollfd entry describing the socket fd and which events
 *		happened.
 * \param tsi: thread service index
 *
 * Same as lws_service_fd() but used with multiple service threads
 */
LWS_VISIBLE LWS_EXTERN int
lws_service_fd_tsi(struct lws_context *context, struct lws_pollfd *pollfd,
		   int tsi);

/**
 * lws_service_adjust_timeout() - Check for any connection needing forced service
 * \param context:	Websocket context
 * \param timeout_ms:	The original poll timeout value.  You can just set this
 *			to 1 if you don't really have a poll timeout.
 * \param tsi: thread service index
 *
 * Under some conditions connections may need service even though there is no
 * pending network action on them, this is "forced service".  For default
 * poll() and libuv / libev, the library takes care of calling this and
 * dealing with it for you.  But for external poll() integration, you need
 * access to the apis.
 *
 * If anybody needs "forced service", returned timeout is zero.  In that case,
 * you can call lws_service_tsi() with a timeout of -1 to only service
 * guys who need forced service.
 */
LWS_VISIBLE LWS_EXTERN int
lws_service_adjust_timeout(struct lws_context *context, int timeout_ms, int tsi);

/* Backwards compatibility */
#define lws_plat_service_tsi lws_service_tsi

LWS_VISIBLE LWS_EXTERN int
lws_handle_POLLOUT_event(struct lws *wsi, struct lws_pollfd *pollfd);

///@}

/*! \defgroup uv libuv helpers
 *
 * ##libuv helpers
 *
 * APIs specific to libuv event loop itegration
 */
///@{
#if defined(LWS_WITH_LIBUV) && defined(UV_ERRNO_MAP)

/*
 * Any direct libuv allocations in lws protocol handlers must participate in the
 * lws reference counting scheme.  Two apis are provided:
 *
 * - lws_libuv_static_refcount_add(handle, context, tsi) to mark the handle with
 *  a pointer to the context and increment the global uv object counter
 *
 * - lws_libuv_static_refcount_del() which should be used as the close callback
 *   for your own libuv objects declared in the protocol scope.
 *
 * Using the apis allows lws to detach itself from a libuv loop completely
 * cleanly and at the moment all of its libuv objects have completed close.
 */

LWS_VISIBLE LWS_EXTERN uv_loop_t *
lws_uv_getloop(struct lws_context *context, int tsi);

LWS_VISIBLE LWS_EXTERN void
lws_libuv_static_refcount_add(uv_handle_t *, struct lws_context *context,
				int tsi);

LWS_VISIBLE LWS_EXTERN void
lws_libuv_static_refcount_del(uv_handle_t *);

#endif /* LWS_WITH_LIBUV */

#if defined(LWS_PLAT_FREERTOS)
#define lws_libuv_static_refcount_add(_a, _b, _c)
#define lws_libuv_static_refcount_del NULL
#endif
///@}
