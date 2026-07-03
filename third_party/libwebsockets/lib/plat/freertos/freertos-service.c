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

int
lws_plat_service(struct lws_context *context, int timeout_ms)
{
	int n = _lws_plat_service_tsi(context, timeout_ms, 0);

#if !defined(LWS_AMAZON_RTOS)
	esp_task_wdt_reset();
#endif

	return n;
}


int
_lws_plat_service_tsi(struct lws_context *context, int timeout_ms, int tsi)
{
	volatile struct lws_context_per_thread *vpt;
	struct lws_context_per_thread *pt;
	lws_usec_t timeout_us;
	int n = -1, m, c, a = 0;

	/* stay dead once we are dead */

	if (!context)
		return 1;

	pt = &context->pt[tsi];
	vpt = (volatile struct lws_context_per_thread *)pt;

	{
		unsigned long m = lws_now_secs();

		if (m > context->time_last_state_dump) {
			context->time_last_state_dump = m;
#if defined(LWS_ESP_PLATFORM)
			n = esp_get_free_heap_size();
#else
			n = xPortGetFreeHeapSize();
#endif
			if ((unsigned int)n != context->last_free_heap) {
				if ((unsigned int)n > context->last_free_heap)
					lwsl_debug(" heap :%ld (+%ld)\n",
						    (unsigned long)n,
						    (unsigned long)(n -
						      context->last_free_heap));
				else
					lwsl_debug(" heap :%ld (-%ld)\n",
						    (unsigned long)n,
						    (unsigned long)(
						      context->last_free_heap -
						      n));
				context->last_free_heap = n;
			}
		}
	}

	if (timeout_ms < 0)
		timeout_ms = 0;
	else
		/* force a default timeout of 23 days */
		timeout_ms = 2000000000;
	timeout_us = ((lws_usec_t)timeout_ms) * LWS_US_PER_MS;

	if (!pt->service_tid_detected && context->vhost_list) {
		lws_fakewsi_def_plwsa(pt);

		lws_fakewsi_prep_plwsa_ctx(context);

		pt->service_tid = context->vhost_list->protocols[0].callback(
			(struct lws *)plwsa, LWS_CALLBACK_GET_THREAD_ID,
			NULL, NULL, 0);
		pt->service_tid_detected = 1;
	}

	/*
	 * is there anybody with pending stuff that needs service forcing?
	 */
#if !defined(LWS_AMAZON_RTOS)
again:
#endif
	n = 0;
	if (lws_service_adjust_timeout(context, 1, tsi)) {
#if defined(LWS_AMAZON_RTOS)
again:
#endif /* LWS_AMAZON_RTOS */

		a = 0;
		if (timeout_us) {
			lws_usec_t us;

			lws_pt_lock(pt, __func__);
			/* don't stay in poll wait longer than next hr timeout */
			us = __lws_sul_service_ripe(pt->pt_sul_owner,
						    LWS_COUNT_PT_SUL_OWNERS,
						    lws_now_usecs());
			if (us && us < timeout_us)
				timeout_us = us;

			lws_pt_unlock(pt);
		}

	//	n = poll(pt->fds, pt->fds_count, timeout_ms);
		{
			fd_set readfds, writefds, errfds;
			struct timeval tv = { timeout_us / LWS_US_PER_SEC,
					      timeout_us % LWS_US_PER_SEC }, *ptv = &tv;
			int max_fd = 0;
			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&errfds);

			for (n = 0; n < (int)pt->fds_count; n++) {
				pt->fds[n].revents = 0;
				if (pt->fds[n].fd >= max_fd)
					max_fd = pt->fds[n].fd;
				if (pt->fds[n].events & LWS_POLLIN)
					FD_SET(pt->fds[n].fd, &readfds);
				if (pt->fds[n].events & LWS_POLLOUT)
					FD_SET(pt->fds[n].fd, &writefds);
				FD_SET(pt->fds[n].fd, &errfds);
			}

			vpt->inside_poll = 1;
			lws_memory_barrier();
			n = select(max_fd + 1, &readfds, &writefds, &errfds, ptv);
			vpt->inside_poll = 0;
			lws_memory_barrier();
			n = 0;

			for (m = 0; m < (int)pt->fds_count; m++) {
				c = 0;
				if (FD_ISSET(pt->fds[m].fd, &readfds)) {
					pt->fds[m].revents |= LWS_POLLIN;
					c = 1;
				}
				if (FD_ISSET(pt->fds[m].fd, &writefds)) {
					pt->fds[m].revents |= LWS_POLLOUT;
					c = 1;
				}
				if (FD_ISSET(pt->fds[m].fd, &errfds)) {
					// lwsl_notice("errfds %d\n", pt->fds[m].fd);
					pt->fds[m].revents |= LWS_POLLHUP;
					c = 1;
				}

				if (c)
					n++;
			}
		}

		m = 0;

	#if defined(LWS_ROLE_WS) && !defined(LWS_WITHOUT_EXTENSIONS)
		m |= !!pt->ws.rx_draining_ext_list;
	#endif

#if defined(LWS_WITH_TLS)
		if (pt->context->tls_ops &&
		    pt->context->tls_ops->fake_POLLIN_for_buffered)
			m |= pt->context->tls_ops->fake_POLLIN_for_buffered(pt);
#endif
		if (!m && !n)
			return 0;
	} else
		a = 1;

	m = lws_service_flag_pending(context, tsi);
	c = m ? -1 : n;

	/* any socket with events to service? */
	for (n = 0; n < (int)pt->fds_count && c; n++) {
		if (!pt->fds[n].revents)
			continue;

		c--;

		m = lws_service_fd_tsi(context, &pt->fds[n], tsi);
		if (m < 0)
			return -1;
		/* if something closed, retry this slot */
		if (m)
			n--;
	}

	if (a)
		goto again;

	return 0;
}
