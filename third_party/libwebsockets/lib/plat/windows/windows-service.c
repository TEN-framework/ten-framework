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
 */

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include "private-lib-core.h"


int
_lws_plat_service_forced_tsi(struct lws_context *context, int tsi)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	int m, n, r;

	r = lws_service_flag_pending(context, tsi);

	/* any socket with events to service? */
	for (n = 0; n < (int)pt->fds_count; n++) {
		if (!pt->fds[n].revents)
			continue;

		m = lws_service_fd_tsi(context, &pt->fds[n], tsi);
		if (m < 0)
			return -1;
		/* if something closed, retry this slot */
		if (m)
			n--;
	}

	lws_service_do_ripe_rxflow(pt);

	return r;
}

extern void lws_client_conn_wait_timeout(lws_sorted_usec_list_t *sul);

int
_lws_plat_service_tsi(struct lws_context *context, int timeout_ms, int tsi)
{
	struct lws_context_per_thread *pt;
	struct lws_pollfd *pfd;
	lws_usec_t timeout_us;
	struct lws *wsi;
	unsigned int i;
	int n;

	/* stay dead once we are dead */
	if (context == NULL)
		return 1;

	pt = &context->pt[tsi];

	if (!pt->service_tid_detected && context->vhost_list) {
		lws_fakewsi_def_plwsa(pt);

		lws_fakewsi_prep_plwsa_ctx(context);

		pt->service_tid = context->vhost_list->
			protocols[0].callback((struct lws *)plwsa,
					LWS_CALLBACK_GET_THREAD_ID,
						  NULL, NULL, 0);
		pt->service_tid_detected = 1;
	}

	if (timeout_ms < 0)
		timeout_ms = 0;
	else
		/* force a default timeout of 23 days */
		timeout_ms = 2000000000;
	timeout_us = ((lws_usec_t)timeout_ms) * LWS_US_PER_MS;

	if (context->event_loop_ops->run_pt)
		context->event_loop_ops->run_pt(context, tsi);

	for (i = 0; i < pt->fds_count; ++i) {
		pfd = &pt->fds[i];

		if (!(pfd->events & LWS_POLLOUT))
			continue;

		wsi = wsi_from_fd(context, pfd->fd);
		if (!wsi || wsi->listener)
			continue;
		if (wsi->sock_send_blocking)
			continue;
		pfd->revents = LWS_POLLOUT;
		n = lws_service_fd(context, pfd);
		if (n < 0)
			return -1;

		/*
		 * Force WSAWaitForMultipleEvents() to check events
		 * and then return immediately.
		 */
		timeout_us = 0;

		/* if something closed, retry this slot */
		if (n)
			i--;
	}

	/*
	 * service pending callbacks and get maximum wait time
	 */
	{
		lws_usec_t us;

		lws_pt_lock(pt, __func__);
		/* don't stay in poll wait longer than next hr timeout */
		us = __lws_sul_service_ripe(pt->pt_sul_owner,
					    LWS_COUNT_PT_SUL_OWNERS,
					    lws_now_usecs());
		if (us && us < timeout_us)
			/*
			 * If something wants zero wait, that's OK, but if the next sul
			 * coming ripe is an interval less than our wait resolution,
			 * bump it to be the wait resolution.
			 */
			timeout_us = us < context->us_wait_resolution ?
					context->us_wait_resolution : us;

		lws_pt_unlock(pt);
	}

	if (_lws_plat_service_forced_tsi(context, tsi))
		timeout_us = 0;

	/*
	 * is there anybody with pending stuff that needs service forcing?
	 */

	if (!lws_service_adjust_timeout(context, 1, tsi))
		timeout_us = 0;

//	lwsl_notice("%s: in %dms, count %d\n", __func__, (int)(timeout_us / 1000), pt->fds_count);
//	for (n = 0; n < (int)pt->fds_count; n++)
//		lwsl_notice("%s: fd %d ev 0x%x POLLIN %d, POLLOUT %d\n", __func__, (int)pt->fds[n].fd, (int)pt->fds[n].events, POLLIN, POLLOUT);
	int d = WSAPoll((WSAPOLLFD *)&pt->fds[0], pt->fds_count, (int)(timeout_us / LWS_US_PER_MS));
	if (d < 0) {
		lwsl_err("%s: WSAPoll failed: count %d, err %d: %d\n", __func__, pt->fds_count, d, WSAGetLastError());
		return 0;
	}
//	lwsl_notice("%s: out\n", __func__);

#if defined(LWS_WITH_TLS)
	if (pt->context->tls_ops &&
	    pt->context->tls_ops->fake_POLLIN_for_buffered)
		pt->context->tls_ops->fake_POLLIN_for_buffered(pt);
#endif

	for (n = 0; n < (int)pt->fds_count; n++)
		if (pt->fds[n].fd != LWS_SOCK_INVALID && pt->fds[n].revents) {
//			lwsl_notice("%s: idx %d, revents 0x%x\n", __func__, n, pt->fds[n].revents);
			lws_service_fd_tsi(context, &pt->fds[n], tsi);
		}

	return 0;
}

int
lws_plat_service(struct lws_context *context, int timeout_ms)
{
	return _lws_plat_service_tsi(context, timeout_ms, 0);
}
