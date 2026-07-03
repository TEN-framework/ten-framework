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

#include <private-lib-core.h>

static int
rops_handle_POLLIN_pipe(struct lws_context_per_thread *pt, struct lws *wsi,
			struct lws_pollfd *pollfd)
{
#if defined(LWS_HAVE_EVENTFD)
	eventfd_t value;
	int n;

	n = eventfd_read(wsi->desc.sockfd, &value);
	if (n < 0) {
		lwsl_notice("%s: eventfd read %d bailed errno %d\n", __func__,
				wsi->desc.sockfd, LWS_ERRNO);
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}
#elif !defined(WIN32) && !defined(_WIN32)
	char s[100];
	int n;

	/*
	 * discard the byte(s) that signaled us
	 * We really don't care about the number of bytes, but coverity
	 * thinks we should.
	 */
	n = (int)read(wsi->desc.sockfd, s, sizeof(s));
	(void)n;
	if (n < 0)
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
#elif defined(WIN32)
	char s[100];
	int n;

	n = recv(wsi->desc.sockfd, s, sizeof(s), 0);
	if (n == SOCKET_ERROR)
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
#endif

#if defined(LWS_WITH_THREADPOOL) && defined(LWS_HAVE_PTHREAD_H)
	/*
	 * threadpools that need to call for on_writable callbacks do it by
	 * marking the task as needing one for its wsi, then cancelling service.
	 *
	 * Each tsi will call this to perform the actual callback_on_writable
	 * from the correct service thread context
	 */
	lws_threadpool_tsi_context(pt->context, pt->tid);
#endif

#if LWS_MAX_SMP > 1

	/*
	 * Other pts need to take care of their own wsi bound to a vhost that
	 * is going down
	 */

	if (pt->context->owner_vh_being_destroyed.head) {

		lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				      pt->context->owner_vh_being_destroyed.head) {
			struct lws_vhost *v =
				lws_container_of(d, struct lws_vhost,
						 vh_being_destroyed_list);

			lws_vhost_lock(v); /* -------------- vh { */
			__lws_vhost_destroy_pt_wsi_dieback_start(v);
			lws_vhost_unlock(v); /* } vh -------------- */

		} lws_end_foreach_dll_safe(d, d1);
	}

#endif

#if defined(LWS_WITH_SECURE_STREAMS)
	lws_dll2_foreach_safe(&pt->ss_owner, NULL, lws_ss_cancel_notify_dll);
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API) && defined(LWS_WITH_CLIENT)
	lws_dll2_foreach_safe(&pt->ss_client_owner, NULL, lws_sspc_cancel_notify_dll);
#endif
#endif

	/*
	 * the poll() wait, or the event loop for libuv etc is a
	 * process-wide resource that we interrupted.  So let every
	 * protocol that may be interested in the pipe event know that
	 * it happened.
	 */
	if (lws_broadcast(pt, LWS_CALLBACK_EVENT_WAIT_CANCELLED, NULL, 0)) {
		lwsl_info("closed in event cancel\n");
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	return LWS_HPI_RET_HANDLED;
}

static const lws_rops_t rops_table_pipe[] = {
	/*  1 */ { .handle_POLLIN	= rops_handle_POLLIN_pipe },
};


const struct lws_role_ops role_ops_pipe = {
	/* role name */			"pipe",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_pipe,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x00,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x01,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x00,
	  /* LWS_ROPS_callback_on_writable */
	  /* LWS_ROPS_tx_credit */			0x00,
	  /* LWS_ROPS_write_role_protocol */
	  /* LWS_ROPS_encapsulation_parent */		0x00,
	  /* LWS_ROPS_alpn_negotiated */
	  /* LWS_ROPS_close_via_role_protocol */	0x00,
	  /* LWS_ROPS_close_role */
	  /* LWS_ROPS_close_kill_connection */		0x00,
	  /* LWS_ROPS_destroy_role */
	  /* LWS_ROPS_adoption_bind */			0x00,
	  /* LWS_ROPS_client_bind */
	  /* LWS_ROPS_issue_keepalive */		0x00,
					},

	/* adoption_cb clnt, srv */	{ 0, 0 },
	/* rx_cb clnt, srv */		{ 0, 0 },
	/* writeable cb clnt, srv */	{ 0, 0 },
	/* close cb clnt, srv */	{ 0, 0 },
	/* protocol_bind_cb c,s */	{ 0, 0 },
	/* protocol_unbind_cb c,s */	{ 0, 0 },
#if defined(WIN32)
	/* file_handle (no, UDP) */	0,
#else
	/* file_handle */		1,
#endif
};
