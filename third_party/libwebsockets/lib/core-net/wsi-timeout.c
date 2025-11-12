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

#include "private-lib-core.h"

void
__lws_wsi_remove_from_sul(struct lws *wsi)
{
	lws_sul_cancel(&wsi->sul_timeout);
	lws_sul_cancel(&wsi->sul_hrtimer);
	lws_sul_cancel(&wsi->sul_validity);
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_sul_cancel(&wsi->sul_fault_timedclose);
#endif
}

/*
 * hrtimer
 */

static void
lws_sul_hrtimer_cb(lws_sorted_usec_list_t *sul)
{
	struct lws *wsi = lws_container_of(sul, struct lws, sul_hrtimer);

	if (wsi->a.protocol &&
	    wsi->a.protocol->callback(wsi, LWS_CALLBACK_TIMER,
				    wsi->user_space, NULL, 0))
		__lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
				     "hrtimer cb errored");
}

void
__lws_set_timer_usecs(struct lws *wsi, lws_usec_t us)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	wsi->sul_hrtimer.cb = lws_sul_hrtimer_cb;
	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &wsi->sul_hrtimer, us);
}

void
lws_set_timer_usecs(struct lws *wsi, lws_usec_t usecs)
{
	__lws_set_timer_usecs(wsi, usecs);
}

/*
 * wsi timeout
 */

static void
lws_sul_wsitimeout_cb(lws_sorted_usec_list_t *sul)
{
	struct lws *wsi = lws_container_of(sul, struct lws, sul_timeout);
	struct lws_context *cx = wsi->a.context;
	struct lws_context_per_thread *pt = &cx->pt[(int)wsi->tsi];

	/* no need to log normal idle keepalive timeout */
//		if (wsi->pending_timeout != PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE)
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	if (wsi->pending_timeout != PENDING_TIMEOUT_USER_OK)
		lwsl_wsi_info(wsi, "TIMEDOUT WAITING %d, dhdr %d, ah %p, wl %d",
				   wsi->pending_timeout,
				   wsi->hdr_parsing_completed, wsi->http.ah,
				   pt->http.ah_wait_list_length);
#if defined(LWS_WITH_CGI)
	if (wsi->http.cgi)
		lwsl_wsi_notice(wsi, "CGI timeout: %s", wsi->http.cgi->summary);
#endif
#else
	if (wsi->pending_timeout != PENDING_TIMEOUT_USER_OK)
		lwsl_wsi_info(wsi, "TIMEDOUT WAITING on %d ",
				   wsi->pending_timeout);
#endif
	/* cgi timeout */
	if (wsi->pending_timeout != PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE)
		/*
		 * Since he failed a timeout, he already had a chance to
		 * do something and was unable to... that includes
		 * situations like half closed connections.  So process
		 * this "failed timeout" close as a violent death and
		 * don't try to do protocol cleanup like flush partials.
		 */
		wsi->socket_is_permanently_unusable = 1;
#if defined(LWS_WITH_CLIENT)
	if (lwsi_state(wsi) == LRS_WAITING_SSL)
		lws_inform_client_conn_fail(wsi,
			(void *)"Timed out waiting SSL", 21);
	if (lwsi_state(wsi) == LRS_WAITING_SERVER_REPLY)
		lws_inform_client_conn_fail(wsi,
			(void *)"Timed out waiting server reply", 30);
#endif

	lws_context_lock(cx, __func__);
	lws_pt_lock(pt, __func__);
	__lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "timeout");
	lws_pt_unlock(pt);
	lws_context_unlock(cx);
}

void
__lws_set_timeout(struct lws *wsi, enum pending_timeout reason, int secs)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	wsi->sul_timeout.cb = lws_sul_wsitimeout_cb;
	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &wsi->sul_timeout,
			    ((lws_usec_t)secs) * LWS_US_PER_SEC);

	lwsl_wsi_debug(wsi, "%d secs, reason %d\n", secs, reason);

	wsi->pending_timeout = (char)reason;
}

void
lws_set_timeout(struct lws *wsi, enum pending_timeout reason, int secs)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	lws_context_lock(pt->context, __func__);
	lws_pt_lock(pt, __func__);
	lws_dll2_remove(&wsi->sul_timeout.list);
	lws_pt_unlock(pt);

	if (!secs)
		goto bail;

	if (secs == LWS_TO_KILL_SYNC) {
		lwsl_wsi_debug(wsi, "TO_KILL_SYNC");
		lws_context_unlock(pt->context);
		lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
				   "to sync kill");
		return;
	}

	if (secs == LWS_TO_KILL_ASYNC)
		secs = 0;

	// assert(!secs || !wsi->mux_stream_immortal);
	if (secs && wsi->mux_stream_immortal)
		lwsl_wsi_err(wsi, "on immortal stream %d %d", reason, secs);

	lws_pt_lock(pt, __func__);
	__lws_set_timeout(wsi, reason, secs);
	lws_pt_unlock(pt);

bail:
	lws_context_unlock(pt->context);
}

void
lws_set_timeout_us(struct lws *wsi, enum pending_timeout reason, lws_usec_t us)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	lws_pt_lock(pt, __func__);
	lws_dll2_remove(&wsi->sul_timeout.list);
	lws_pt_unlock(pt);

	if (!us)
		return;

	lws_pt_lock(pt, __func__);
	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &wsi->sul_timeout, us);

	lwsl_wsi_notice(wsi, "%llu us, reason %d",
			     (unsigned long long)us, reason);

	wsi->pending_timeout = (char)reason;
	lws_pt_unlock(pt);
}

static void
lws_validity_cb(lws_sorted_usec_list_t *sul)
{
	struct lws *wsi = lws_container_of(sul, struct lws, sul_validity);
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	const lws_retry_bo_t *rbo = wsi->retry_policy;

	/* one of either the ping or hangup validity threshold was crossed */

	if (wsi->validity_hup) {
		lwsl_wsi_info(wsi, "validity too old");
		struct lws_context *cx = wsi->a.context;
		struct lws_context_per_thread *pt = &cx->pt[(int)wsi->tsi];

		lws_context_lock(cx, __func__);
		lws_pt_lock(pt, __func__);
		__lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
				     "validity timeout");
		lws_pt_unlock(pt);
		lws_context_unlock(cx);
		return;
	}

	/* schedule a protocol-dependent ping */

	lwsl_wsi_info(wsi, "scheduling validity check");

	if (lws_rops_fidx(wsi->role_ops, LWS_ROPS_issue_keepalive))
		lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_issue_keepalive).
							issue_keepalive(wsi, 0);

	/*
	 * We arrange to come back here after the additional ping to hangup time
	 * and do the hangup, unless we get validated (by, eg, a PONG) and
	 * reset the timer
	 */

	assert(rbo->secs_since_valid_hangup > rbo->secs_since_valid_ping);

	wsi->validity_hup = 1;
	__lws_sul_insert_us(&pt->pt_sul_owner[!!wsi->conn_validity_wakesuspend],
			    &wsi->sul_validity,
			    ((uint64_t)rbo->secs_since_valid_hangup -
				 rbo->secs_since_valid_ping) * LWS_US_PER_SEC);
}

/*
 * The role calls this back to actually confirm validity on a particular wsi
 * (which may not be the original wsi)
 */

void
_lws_validity_confirmed_role(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	const lws_retry_bo_t *rbo = wsi->retry_policy;

	if (!rbo || !rbo->secs_since_valid_hangup)
		return;

	wsi->validity_hup = 0;
	wsi->sul_validity.cb = lws_validity_cb;

	wsi->validity_hup = rbo->secs_since_valid_ping >=
			    rbo->secs_since_valid_hangup;

	lwsl_wsi_info(wsi, "setting validity timer %ds (hup %d)",
			   wsi->validity_hup ? rbo->secs_since_valid_hangup :
					    rbo->secs_since_valid_ping,
			   wsi->validity_hup);

	__lws_sul_insert_us(&pt->pt_sul_owner[!!wsi->conn_validity_wakesuspend],
			    &wsi->sul_validity,
			    ((uint64_t)(wsi->validity_hup ?
				rbo->secs_since_valid_hangup :
				rbo->secs_since_valid_ping)) * LWS_US_PER_SEC);
}

void
lws_validity_confirmed(struct lws *wsi)
{
	/*
	 * This may be a stream inside a muxed network connection... leave it
	 * to the role to figure out who actually needs to understand their
	 * validity was confirmed.
	 */
	if (!wsi->h2_stream_carries_ws && /* only if not encapsulated */
	    wsi->role_ops &&
	    lws_rops_fidx(wsi->role_ops, LWS_ROPS_issue_keepalive))
		lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_issue_keepalive).
							issue_keepalive(wsi, 1);
}
