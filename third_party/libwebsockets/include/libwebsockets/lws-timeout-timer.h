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

/*! \defgroup timeout Connection timeouts

    APIs related to setting connection timeouts
*/
//@{

/*
 * NOTE: These public enums are part of the abi.  If you want to add one,
 * add it at where specified so existing users are unaffected.
 */
enum pending_timeout {
	NO_PENDING_TIMEOUT					=  0,
	PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE			=  1,
	PENDING_TIMEOUT_AWAITING_CONNECT_RESPONSE		=  2,
	PENDING_TIMEOUT_ESTABLISH_WITH_SERVER			=  3,
	PENDING_TIMEOUT_AWAITING_SERVER_RESPONSE		=  4,
	PENDING_TIMEOUT_AWAITING_PING				=  5,
	PENDING_TIMEOUT_CLOSE_ACK				=  6,
	PENDING_TIMEOUT_UNUSED1					=  7,
	PENDING_TIMEOUT_SENT_CLIENT_HANDSHAKE			=  8,
	PENDING_TIMEOUT_SSL_ACCEPT				=  9,
	PENDING_TIMEOUT_HTTP_CONTENT				= 10,
	PENDING_TIMEOUT_AWAITING_CLIENT_HS_SEND			= 11,
	PENDING_FLUSH_STORED_SEND_BEFORE_CLOSE			= 12,
	PENDING_TIMEOUT_SHUTDOWN_FLUSH				= 13,
	PENDING_TIMEOUT_CGI					= 14,
	PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE			= 15,
	PENDING_TIMEOUT_WS_PONG_CHECK_SEND_PING			= 16,
	PENDING_TIMEOUT_WS_PONG_CHECK_GET_PONG			= 17,
	PENDING_TIMEOUT_CLIENT_ISSUE_PAYLOAD			= 18,
	PENDING_TIMEOUT_AWAITING_SOCKS_GREETING_REPLY	        = 19,
	PENDING_TIMEOUT_AWAITING_SOCKS_CONNECT_REPLY		= 20,
	PENDING_TIMEOUT_AWAITING_SOCKS_AUTH_REPLY		= 21,
	PENDING_TIMEOUT_KILLED_BY_SSL_INFO			= 22,
	PENDING_TIMEOUT_KILLED_BY_PARENT			= 23,
	PENDING_TIMEOUT_CLOSE_SEND				= 24,
	PENDING_TIMEOUT_HOLDING_AH				= 25,
	PENDING_TIMEOUT_UDP_IDLE				= 26,
	PENDING_TIMEOUT_CLIENT_CONN_IDLE			= 27,
	PENDING_TIMEOUT_LAGGING					= 28,
	PENDING_TIMEOUT_THREADPOOL				= 29,
	PENDING_TIMEOUT_THREADPOOL_TASK				= 30,
	PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE		= 31,
	PENDING_TIMEOUT_USER_OK					= 32,

	/****** add new things just above ---^ ******/

	PENDING_TIMEOUT_USER_REASON_BASE			= 1000
};

#define lws_time_in_microseconds lws_now_usecs

#define LWS_TO_KILL_ASYNC -1
/**< If LWS_TO_KILL_ASYNC is given as the timeout sec in a lws_set_timeout()
 * call, then the connection is marked to be killed at the next timeout
 * check.  This is how you should force-close the wsi being serviced if
 * you are doing it outside the callback (where you should close by nonzero
 * return).
 */
#define LWS_TO_KILL_SYNC -2
/**< If LWS_TO_KILL_SYNC is given as the timeout sec in a lws_set_timeout()
 * call, then the connection is closed before returning (which may delete
 * the wsi).  This should only be used where the wsi being closed is not the
 * wsi currently being serviced.
 */
/**
 * lws_set_timeout() - marks the wsi as subject to a timeout some seconds hence
 *
 * \param wsi:	Websocket connection instance
 * \param reason:	timeout reason
 * \param secs:	how many seconds.  You may set to LWS_TO_KILL_ASYNC to
 *		force the connection to timeout at the next opportunity, or
 *		LWS_TO_KILL_SYNC to close it synchronously if you know the
 *		wsi is not the one currently being serviced.
 */
LWS_VISIBLE LWS_EXTERN void
lws_set_timeout(struct lws *wsi, enum pending_timeout reason, int secs);

/**
 * lws_set_timeout_us() - marks the wsi as subject to a timeout some us hence
 *
 * \param wsi:	Websocket connection instance
 * \param reason:	timeout reason
 * \param us:	0 removes the timeout, otherwise number of us to wait
 *
 * Higher-resolution version of lws_set_timeout().  Actual resolution depends
 * on platform and load, usually ms.
 */
void
lws_set_timeout_us(struct lws *wsi, enum pending_timeout reason, lws_usec_t us);

/* helper for clearer LWS_TO_KILL_ASYNC / LWS_TO_KILL_SYNC usage */
#define lws_wsi_close(w, to_kill) lws_set_timeout(w, 1, to_kill)


#define LWS_SET_TIMER_USEC_CANCEL ((lws_usec_t)-1ll)
#define LWS_USEC_PER_SEC ((lws_usec_t)1000000)

/**
 * lws_set_timer_usecs() - schedules a callback on the wsi in the future
 *
 * \param wsi:	Websocket connection instance
 * \param usecs:  LWS_SET_TIMER_USEC_CANCEL removes any existing scheduled
 *		  callback, otherwise number of microseconds in the future
 *		  the callback will occur at.
 *
 * NOTE: event loop support for this:
 *
 *  default poll() loop:   yes
 *  libuv event loop:      yes
 *  libev:    not implemented (patch welcome)
 *  libevent: not implemented (patch welcome)
 *
 * After the deadline expires, the wsi will get a callback of type
 * LWS_CALLBACK_TIMER and the timer is exhausted.  The deadline may be
 * continuously deferred by further calls to lws_set_timer_usecs() with a later
 * deadline, or cancelled by lws_set_timer_usecs(wsi, -1).
 *
 * If the timer should repeat, lws_set_timer_usecs() must be called again from
 * LWS_CALLBACK_TIMER.
 *
 * Accuracy depends on the platform and the load on the event loop or system...
 * all that's guaranteed is the callback will come after the requested wait
 * period.
 */
LWS_VISIBLE LWS_EXTERN void
lws_set_timer_usecs(struct lws *wsi, lws_usec_t usecs);

struct lws_sorted_usec_list;

typedef void (*sul_cb_t)(struct lws_sorted_usec_list *sul);

typedef struct lws_sorted_usec_list {
	struct lws_dll2 list;	/* simplify the code by keeping this at start */
	lws_usec_t	us;
	sul_cb_t	cb;
	uint32_t	latency_us;	/* us it may safely be delayed */
} lws_sorted_usec_list_t;

/*
 * There are multiple sul owners to allow accounting for, a) events that must
 * wake from suspend, and b) events that can be missued due to suspend
 */
#define LWS_COUNT_PT_SUL_OWNERS			2

#define LWSSULLI_MISS_IF_SUSPENDED		0
#define LWSSULLI_WAKE_IF_SUSPENDED		1

/*
 * lws_sul2_schedule() - schedule a callback
 *
 * \param context: the lws_context
 * \param tsi: the thread service index (usually 0)
 * \param flags: LWSSULLI_...
 * \param sul: pointer to the sul element
 *
 * Generic callback-at-a-later time function.  The callback happens on the
 * event loop thread context.
 *
 * Although the api has us resultion, the actual resolution depends on the
 * platform and may be, eg, 1ms.
 *
 * This doesn't allocate and doesn't fail.
 *
 * If flags contains LWSSULLI_WAKE_IF_SUSPENDED, the scheduled event is placed
 * on a sul owner list that, if the system has entered low power suspend mode,
 * tries to arrange that the system should wake from platform suspend just
 * before the event is due.  Scheduled events without this flag will be missed
 * in the case the system is in suspend and nothing else happens to have woken
 * it.
 *
 * You can call it again with another us value to change the delay or move the
 * event to a different owner (ie, wake or miss on suspend).
 */
LWS_VISIBLE LWS_EXTERN void
lws_sul2_schedule(struct lws_context *context, int tsi, int flags,
		  lws_sorted_usec_list_t *sul);

/*
 * lws_sul_cancel() - cancel scheduled callback
 *
 * \param sul: pointer to the sul element
 *
 * If it's scheduled, remove the sul from its owning sorted list.
 * If not scheduled, it's a NOP.
 */
LWS_VISIBLE LWS_EXTERN void
lws_sul_cancel(lws_sorted_usec_list_t *sul);

/*
 * lws_sul_earliest_wakeable_event() - get earliest wake-from-suspend event
 *
 * \param ctx: the lws context
 * \param pearliest: pointer to lws_usec_t to take the result
 *
 * Either returns 1 if no pending event, or 0 and sets *pearliest to the
 * MONOTONIC time of the current earliest next expected event.
 */
LWS_VISIBLE LWS_EXTERN int
lws_sul_earliest_wakeable_event(struct lws_context *ctx, lws_usec_t *pearliest);

/*
 * For backwards compatibility
 *
 * If us is LWS_SET_TIMER_USEC_CANCEL, the sul is removed from the scheduler.
 * New code can use lws_sul_cancel()
 */

LWS_VISIBLE LWS_EXTERN void
lws_sul_schedule(struct lws_context *ctx, int tsi, lws_sorted_usec_list_t *sul,
		 sul_cb_t _cb, lws_usec_t _us);
LWS_VISIBLE LWS_EXTERN void
lws_sul_schedule_wakesuspend(struct lws_context *ctx, int tsi,
			     lws_sorted_usec_list_t *sul, sul_cb_t _cb,
			     lws_usec_t _us);

#if defined(LWS_WITH_SUL_DEBUGGING)
/**
 * lws_sul_debug_zombies() - assert there are no scheduled sul in a given object
 *
 * \param ctx: lws_context
 * \param po: pointer to the object that is about to be destroyed
 * \param len: length of the object that is about to be destroyed
 * \param destroy_description: string clue what any failure is related to
 *
 * This is an optional debugging helper that walks the sul scheduler lists
 * confirming that there are no suls scheduled that live inside the object
 * footprint described by po and len.  When internal objects are about to be
 * destroyed, like wsi / user_data or secure stream handles, if
 * LWS_WITH_SUL_DEBUGGING is enabled the scheduler is checked for anything
 * in the object being destroyed.  If something found, an error is printed and
 * an assert fired.
 *
 * Internal sul like timeouts should always be cleaned up correctly, but user
 * suls in, eg, wsi user_data area, or in secure stream user allocation, may be
 * the cause of difficult to find bugs if valgrind not available and the user
 * code left a sul in the scheduler after destroying the object the sul was
 * living in.
 */
LWS_VISIBLE LWS_EXTERN void
lws_sul_debug_zombies(struct lws_context *ctx, void *po, size_t len,
		      const char *destroy_description);
#else
#define lws_sul_debug_zombies(_a, _b, _c, _d)
#endif

/*
 * lws_validity_confirmed() - reset the validity timer for a network connection
 *
 * \param wsi: the connection that saw traffic proving the connection valid
 *
 * Network connections are subject to intervals defined by the context, the
 * vhost if server connections, or the client connect info if a client
 * connection.  If the connection goes longer than the specified time since
 * last observing traffic that can only happen if traffic is passing in both
 * directions, then lws will try to create a PING transaction on the network
 * connection.
 *
 * If the connection reaches the specified `.secs_since_valid_hangup` time
 * still without any proof of validity, the connection will be closed.
 *
 * If the PONG comes, or user code observes traffic that satisfies the proof
 * that both directions are passing traffic to the peer and calls this api,
 * the connection validity timer is reset and the scheme repeats.
 */
LWS_VISIBLE LWS_EXTERN void
lws_validity_confirmed(struct lws *wsi);

/*
 * These are not normally needed, they're exported for the case there's code
 * using lws_sul for which lws is an optional link dependency.
 */

LWS_VISIBLE LWS_EXTERN int
__lws_sul_insert(lws_dll2_owner_t *own, lws_sorted_usec_list_t *sul);

LWS_VISIBLE LWS_EXTERN lws_usec_t
__lws_sul_service_ripe(lws_dll2_owner_t *own, int own_len, lws_usec_t usnow);

///@}
