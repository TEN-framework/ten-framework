/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
lws_client_conn_wait_timeout(lws_sorted_usec_list_t *sul)
{
	struct lws *wsi = lws_container_of(sul, struct lws,
					   sul_connect_timeout);

	/*
	 * This is used to constrain the time we're willing to wait for a
	 * connection before giving up on it and retrying.
	 */

	lwsl_wsi_info(wsi, "connect wait timeout has fired");
	lws_client_connect_3_connect(wsi, NULL, NULL, 0, NULL);
}

void
lws_client_dns_retry_timeout(lws_sorted_usec_list_t *sul)
{
	struct lws *wsi = lws_container_of(sul, struct lws,
					   sul_connect_timeout);

	/*
	 * This limits the amount of dns lookups we will try before
	 * giving up and failing... it reuses sul_connect_timeout, which
	 * isn't officially used until we connected somewhere.
	 */

	lwsl_wsi_info(wsi, "dns retry");
	if (!lws_client_connect_2_dnsreq(wsi))
		lwsl_wsi_notice(wsi, "DNS lookup failed");
}

/*
 * Figure out if an ongoing connect() has arrived at a final disposition or not
 *
 * We can check using getsockopt if our connect actually completed.
 * Posix connect() allows nonblocking to redo the connect to
 * find out if it succeeded.
 */

typedef enum {
	LCCCR_CONNECTED			= 1,
	LCCCR_CONTINUE			= 0,
	LCCCR_FAILED			= -1,
} lcccr_t;

static lcccr_t
lws_client_connect_check(struct lws *wsi, int *real_errno)
{
	int en = 0;
#if !defined(WIN32)
	int e;
	socklen_t sl = sizeof(e);
#endif

	(void)en;

	/*
	 * This resets SO_ERROR after reading it.  If there's an error
	 * condition, the connect definitively failed.
	 */

#if !defined(WIN32)
	if (!getsockopt(wsi->desc.sockfd, SOL_SOCKET, SO_ERROR, &e, &sl)) {
		en = LWS_ERRNO;
		if (!e) {
			lwsl_wsi_debug(wsi, "getsockopt: conn OK errno %d", en);

			return LCCCR_CONNECTED;
		}

		lwsl_wsi_notice(wsi, "getsockopt fd %d says e %d",
							wsi->desc.sockfd, e);

		*real_errno = e;

		return LCCCR_FAILED;
	}

#else

	if (!connect(wsi->desc.sockfd, (const struct sockaddr *)&wsi->sa46_peer.sa4, 0))
		return LCCCR_CONNECTED;

	en = LWS_ERRNO;

	if (en == WSAEISCONN) /* already connected */
		return LCCCR_CONNECTED;

	if (en == WSAEALREADY) {
		/* reset the POLLOUT wait */
		if (lws_change_pollfd(wsi, 0, LWS_POLLOUT))
			lwsl_wsi_notice(wsi, "pollfd failed");
	}

	if (!en || en == WSAEINVAL ||
		   en == WSAEWOULDBLOCK ||
		   en == WSAEALREADY) {
		lwsl_wsi_debug(wsi, "errno %d", en);
		return LCCCR_CONTINUE;
	}
#endif

	lwsl_wsi_notice(wsi, "connect check FAILED: %d",
			*real_errno || en);

	return LCCCR_FAILED;
}

/*
 * We come here to fire off a connect, and to check its disposition later.
 *
 * If it did not complete before the individual attempt timeout, we will try to
 * connect again with the next dns result.
 */

struct lws *
lws_client_connect_3_connect(struct lws *wsi, const char *ads,
			     const struct addrinfo *result, int n, void *opaque)
{
#if defined(LWS_WITH_UNIX_SOCK)
	struct sockaddr_un sau;
#endif
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	const char *cce = "Unable to connect", *iface;
	const struct sockaddr *psa = NULL;
	uint16_t port = wsi->conn_port;
	lws_dns_sort_t *curr;
	ssize_t plen = 0;
	lws_dll2_t *d;
	char dcce[48];
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	int cfail;
#endif
	int m, af = 0;

	/*
	 * If we come here with result set, we need to convert getaddrinfo
	 * results to a lws_dns_sort_t list one time and free the results.
	 *
	 * We use this pattern because ASYNC_DNS will callback here with the
	 * results when it gets them (and may come here more than once, eg, for
	 * AAAA then A or vice-versa)
	 */

	if (result) {
		lws_sul_cancel(&wsi->sul_connect_timeout);

#if defined(LWS_WITH_CONMON)
		/* append a copy from before the sorting */
		lws_conmon_append_copy_new_dns_results(wsi, result);
#endif

		lws_sort_dns(wsi, result);
#if defined(LWS_WITH_SYS_ASYNC_DNS)
		lws_async_dns_freeaddrinfo(&result);
#else
		freeaddrinfo((struct addrinfo *)result);
#endif
		result = NULL;
	}

#if defined(LWS_WITH_UNIX_SOCK)
	memset(&sau, 0, sizeof(sau));
#endif

	/*
	 * async dns calls back here for everybody who cares when it gets a
	 * result... but if we are piggybacking, we do not want to connect
	 * ourselves
	 */

	if (!lws_dll2_is_detached(&wsi->dll2_cli_txn_queue))
		return wsi;

	if (n &&  /* calling back with a problem */
	    !wsi->dns_sorted_list.count && /* there's no results */
	    !lws_socket_is_valid(wsi->desc.sockfd) && /* no attempt ongoing */
	    !wsi->speculative_connect_owner.count /* no spec attempt */ ) {
		lwsl_wsi_notice(wsi, "dns lookup failed %d", n);

		/*
		 * DNS lookup itself failed... let's try again until we
		 * timeout
		 */

		lwsi_set_state(wsi, LRS_UNCONNECTED);
		lws_sul_schedule(wsi->a.context, 0, &wsi->sul_connect_timeout,
				 lws_client_dns_retry_timeout,
						 LWS_USEC_PER_SEC);
		return wsi;

//		cce = "dns lookup failed";
//		goto oom4;
	}

	/*
	 * We come back here again when we think the connect() may have
	 * completed one way or the other, we can't proceed until we know we
	 * actually connected.
	 */

	if (lwsi_state(wsi) == LRS_WAITING_CONNECT &&
	    lws_socket_is_valid(wsi->desc.sockfd)) {

		if (!wsi->dns_sorted_list.count &&
		    !wsi->sul_connect_timeout.list.owner)
			/* no dns results and no ongoing timeout for one */
			goto connect_to;

		/*
		 * If the connection failed, the OS-level errno may be
		 * something like EINPROGRESS rather than the actual problem
		 * that prevented a connection. This value will represent the
		 * “real” problem that we should report to the caller.
		 */
		int real_errno = 0;

		switch (lws_client_connect_check(wsi, &real_errno)) {
		case LCCCR_CONNECTED:
			/*
			 * Oh, it has happened...
			 */
			goto conn_good;
		case LCCCR_CONTINUE:
			return NULL;
		default:
			if (!real_errno)
				real_errno = LWS_ERRNO;
			lws_snprintf(dcce, sizeof(dcce), "conn fail: %d",
				     real_errno);

			cce = dcce;
			lwsl_wsi_debug(wsi, "%s", dcce);
			lws_metrics_caliper_report(wsi->cal_conn, METRES_NOGO);
			goto try_next_dns_result_fds;
		}
	}

#if defined(LWS_WITH_UNIX_SOCK)

	if (ads && *ads == '+') {
		ads++;
		memset(&wsi->sa46_peer, 0, sizeof(wsi->sa46_peer));
		af = sau.sun_family = AF_UNIX;
		strncpy(sau.sun_path, ads, sizeof(sau.sun_path));
		sau.sun_path[sizeof(sau.sun_path) - 1] = '\0';

		lwsl_wsi_info(wsi, "Unix skt: %s", ads);

		if (sau.sun_path[0] == '@')
			sau.sun_path[0] = '\0';

		goto ads_known;
	}
#endif

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	if (n == LADNS_RET_FAILED) {
		lwsl_wsi_notice(wsi, "adns failed %s", ads);
		/*
		 * Caller that is giving us LADNS_RET_FAILED will deal
		 * with cleanup
		 */
		return NULL;
	}
#endif

	/*
	 * Let's try directly connecting to each of the results in turn until
	 * one works, or we run out of results...
	 *
	 * We have a sorted dll2 list with the head one most preferable
	 */

next_dns_result:

	cce = "Unable to connect";

	if (!wsi->dns_sorted_list.count)
		goto failed1;

	/*
	 * Copy the wsi head sorted dns result into the wsi->sa46_peer, and
	 * remove and free the original from the sorted list
	 */

	d = lws_dll2_get_head(&wsi->dns_sorted_list);
	curr = lws_container_of(d, lws_dns_sort_t, list);

	lws_dll2_remove(&curr->list);
	wsi->sa46_peer = curr->dest;
#if defined(LWS_WITH_NETLINK)
	wsi->peer_route_uidx = curr->uidx;
	lwsl_wsi_info(wsi, "peer_route_uidx %d", wsi->peer_route_uidx);
#endif

	lws_free(curr);

	sa46_sockport(&wsi->sa46_peer, htons(port));

	psa = sa46_sockaddr(&wsi->sa46_peer);
	n = (int)sa46_socklen(&wsi->sa46_peer);

#if defined(LWS_WITH_UNIX_SOCK)
ads_known:
#endif

	/*
	 * Now we prepared psa, if not already connecting, create the related
	 * socket and add to the fds
	 */

	if (!lws_socket_is_valid(wsi->desc.sockfd)) {

		if (wsi->a.context->event_loop_ops->check_client_connect_ok &&
		    wsi->a.context->event_loop_ops->check_client_connect_ok(wsi)
		) {
			cce = "waiting for event loop watcher to close";
			goto oom4;
		}

#if defined(LWS_WITH_UNIX_SOCK)
		af = 0;
		if (wsi->unix_skt) {
			af = AF_UNIX;
			wsi->desc.sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
		}
		else
#endif
		{
			af = wsi->sa46_peer.sa4.sin_family;
			wsi->desc.sockfd = socket(wsi->sa46_peer.sa4.sin_family,
						  SOCK_STREAM, 0);
		}

		if (!lws_socket_is_valid(wsi->desc.sockfd)) {
			lws_snprintf(dcce, sizeof(dcce),
				     "conn fail: skt creation: errno %d",
								LWS_ERRNO);
			cce = dcce;
			lwsl_wsi_warn(wsi, "%s", dcce);
			goto try_next_dns_result;
		}

		if (lws_plat_set_socket_options(wsi->a.vhost, wsi->desc.sockfd,
#if defined(LWS_WITH_UNIX_SOCK)
						wsi->unix_skt)) {
#else
						0)) {
#endif
			lws_snprintf(dcce, sizeof(dcce),
				     "conn fail: skt options: errno %d",
								LWS_ERRNO);
			cce = dcce;
			lwsl_wsi_warn(wsi, "%s", dcce);
			goto try_next_dns_result_closesock;
		}

		/* apply requested socket options */
		if (lws_plat_set_socket_options_ip(wsi->desc.sockfd,
						   wsi->c_pri, wsi->flags))
			lwsl_wsi_warn(wsi, "unable to set ip options");

		lwsl_wsi_debug(wsi, "WAITING_CONNECT");
		lwsi_set_state(wsi, LRS_WAITING_CONNECT);

		if (wsi->a.context->event_loop_ops->sock_accept)
			if (wsi->a.context->event_loop_ops->sock_accept(wsi)) {
				lws_snprintf(dcce, sizeof(dcce),
					     "conn fail: sock accept");
				cce = dcce;
				lwsl_wsi_warn(wsi, "%s", dcce);
				goto try_next_dns_result_closesock;
			}

		lws_pt_lock(pt, __func__);
		if (__insert_wsi_socket_into_fds(wsi->a.context, wsi)) {
			lws_snprintf(dcce, sizeof(dcce),
				     "conn fail: insert fd");
			cce = dcce;
			lws_pt_unlock(pt);
			goto try_next_dns_result_closesock;
		}
		lws_pt_unlock(pt);

		/*
		 * The fd + wsi combination is entered into the wsi tables
		 * at this point, with a pollfd
		 *
		 * Past here, we can't simply free the structs as error
		 * handling as oom4 does.
		 *
		 * We can run the whole close flow, or unpick the fds inclusion
		 * and anything else we have done.
		 */

		if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
			lws_snprintf(dcce, sizeof(dcce),
				     "conn fail: change pollfd");
			cce = dcce;
			goto try_next_dns_result_fds;
		}

		if (!wsi->a.protocol)
			wsi->a.protocol = &wsi->a.vhost->protocols[0];

		lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_CONNECT_RESPONSE,
				wsi->a.vhost->connect_timeout_secs);

		iface = lws_wsi_client_stash_item(wsi, CIS_IFACE,
						  _WSI_TOKEN_CLIENT_IFACE);

		if (iface && *iface) {
			m = lws_socket_bind(wsi->a.vhost, wsi, wsi->desc.sockfd,
					    0, iface, af);
			if (m < 0) {
				lws_snprintf(dcce, sizeof(dcce),
					     "conn fail: socket bind");
				cce = dcce;
				goto try_next_dns_result_fds;
			}
		}
	}

#if defined(LWS_WITH_UNIX_SOCK)
	if (wsi->unix_skt) {
		psa = (const struct sockaddr *)&sau;
		if (sau.sun_path[0])
			n = (int)(sizeof(uint16_t) + strlen(sau.sun_path));
		else
			n = (int)(sizeof(uint16_t) +
					strlen(&sau.sun_path[1]) + 1);
	} else
#endif

	if (!psa) /* coverity */
		goto try_next_dns_result_fds;

	/*
	 * The actual connection attempt
	 */

#if defined(LWS_ESP_PLATFORM)
	errno = 0;
#endif

	/* grab a copy for peer tracking */
#if defined(LWS_WITH_UNIX_SOCK)
	if (!wsi->unix_skt)
#endif
		memmove(&wsi->sa46_peer, psa, (unsigned int)n);

	/*
	 * Finally, make the actual connection attempt
	 */

#if defined(LWS_WITH_SYS_METRICS)
	if (wsi->cal_conn.mt) {
		lws_metrics_caliper_report(wsi->cal_conn, METRES_NOGO);
	}
	lws_metrics_caliper_bind(wsi->cal_conn, wsi->a.context->mt_conn_tcp);
#endif

	wsi->socket_is_permanently_unusable = 0;

	if (lws_fi(&wsi->fic, "conn_cb_rej") ||
	    user_callback_handle_rxflow(wsi->a.protocol->callback, wsi,
			LWS_CALLBACK_CONNECTING, wsi->user_space,
			(void *)(intptr_t)wsi->desc.sockfd, 0)) {
		lwsl_wsi_info(wsi, "CONNECTION CB closed");
		goto failed1;
	}

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	cfail = lws_fi(&wsi->fic, "connfail");
	if (cfail)
		m = -1;
	else
#endif
		m = connect(wsi->desc.sockfd, (const struct sockaddr *)psa,
			    (socklen_t)n);

#if defined(LWS_WITH_CONMON)
	wsi->conmon_datum = lws_now_usecs();
	wsi->conmon.ciu_sockconn = 0;
#endif

	if (m == -1) {
		/*
		 * Since we're nonblocking, connect not having completed is not
		 * necessarily indicating any problem... we have to look at
		 * either errno or the socket to understand if we actually
		 * failed already...
		 */

		int errno_copy = LWS_ERRNO;

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
		if (cfail)
			/* fake an abnormal, fatal situation */
			errno_copy = 999;
#endif

		lwsl_wsi_debug(wsi, "connect: fd %d errno: %d",
				wsi->desc.sockfd, errno_copy);

		if (errno_copy &&
		    errno_copy != LWS_EALREADY &&
		    errno_copy != LWS_EINPROGRESS &&
		    errno_copy != LWS_EWOULDBLOCK
#ifdef _WIN32
		 && errno_copy != WSAEINVAL
                 && errno_copy != WSAEISCONN
#endif
		) {
			/*
			 * The connect() failed immediately...
			 */

#if defined(LWS_WITH_CONMON)
			wsi->conmon.ciu_sockconn = (lws_conmon_interval_us_t)
					(lws_now_usecs() - wsi->conmon_datum);
#endif

			lws_metrics_caliper_report(wsi->cal_conn, METRES_NOGO);

#if defined(_DEBUG)
#if defined(LWS_WITH_UNIX_SOCK)
			if (!wsi->unix_skt) {
#endif

			char nads[48];

			lws_sa46_write_numeric_address(&wsi->sa46_peer, nads,
						       sizeof(nads));

			lws_snprintf(dcce, sizeof(dcce),
				     "conn fail: errno %d: %s:%d",
						errno_copy, nads, port);
			cce = dcce;

			wsi->sa46_peer.sa4.sin_family = 0;
			lwsl_wsi_info(wsi, "%s", cce);
#if defined(LWS_WITH_UNIX_SOCK)
			} else {
				lws_snprintf(dcce, sizeof(dcce),
					     "conn fail: errno %d: UDS %s",
							       errno_copy, ads);
				cce = dcce;
			}
#endif
#endif

			goto try_next_dns_result_fds;
		}

#if defined(WIN32)
		if (lws_plat_check_connection_error(wsi))
			goto try_next_dns_result_fds;

		if (errno_copy == WSAEISCONN)
			goto conn_good;
#endif

		/*
		 * The connection attempt is ongoing asynchronously... let's set
		 * a specialized timeout for this connect attempt completion, it
		 * uses wsi->sul_connect_timeout just for this purpose
		 */

		lws_sul_schedule(wsi->a.context, 0, &wsi->sul_connect_timeout,
				 lws_client_conn_wait_timeout,
				 wsi->a.context->timeout_secs *
						 LWS_USEC_PER_SEC);

		/*
		 * must do specifically a POLLOUT poll to hear
		 * about the connect completion
		 */
		if (lws_change_pollfd(wsi, 0, LWS_POLLOUT))
			goto try_next_dns_result_fds;

		return wsi;
	}

conn_good:

	/*
	 * The connection has happened
	 */

#if defined(LWS_WITH_CONMON)
	wsi->conmon.ciu_sockconn = (lws_conmon_interval_us_t)
					(lws_now_usecs() - wsi->conmon_datum);
#endif

#if !defined(LWS_PLAT_OPTEE)
	{
		socklen_t salen = sizeof(wsi->sa46_local);
#if defined(_DEBUG)
		char buf[64];
#endif
		if (getsockname((int)wsi->desc.sockfd,
				(struct sockaddr *)&wsi->sa46_local,
				&salen) == -1)
			lwsl_warn("getsockname: %s\n", strerror(LWS_ERRNO));
#if defined(_DEBUG)
#if defined(LWS_WITH_UNIX_SOCK)
		if (wsi->unix_skt)
			buf[0] = '\0';
		else
#endif
			lws_sa46_write_numeric_address(&wsi->sa46_local, buf, sizeof(buf));

		lwsl_wsi_info(wsi, "source ads %s", buf);
#endif
	}
#endif

	lws_sul_cancel(&wsi->sul_connect_timeout);
	lws_metrics_caliper_report(wsi->cal_conn, METRES_GO);

	lws_addrinfo_clean(wsi);

	if (wsi->a.protocol)
		wsi->a.protocol->callback(wsi, LWS_CALLBACK_WSI_CREATE,
					  wsi->user_space, NULL, 0);

	lwsl_wsi_debug(wsi, "going into connect_4");

	return lws_client_connect_4_established(wsi, NULL, plen);

oom4:
	/*
	 * We get here if we're trying to clean up a connection attempt that
	 * didn't make it as far as getting inserted into the wsi / fd tables
	 */

	if (lwsi_role_client(wsi) && wsi->a.protocol
				/* && lwsi_state_est(wsi) */)
		lws_inform_client_conn_fail(wsi,(void *)cce, strlen(cce));

	/* take care that we might be inserted in fds already */
	if (wsi->position_in_fds_table != LWS_NO_FDS_POS)
		/* do the full wsi close flow */
		goto failed1;

	lws_metrics_caliper_report(wsi->cal_conn, METRES_NOGO);

	/*
	 * We can't be an active client connection any more, if we thought
	 * that was what we were going to be doing.  It should be if we are
	 * failing by oom4 path, we are still called by
	 * lws_client_connect_via_info() and will be returning NULL to that,
	 * so nobody else should have had a chance to queue on us.
	 */
	{
		struct lws_vhost *vhost = wsi->a.vhost;
		lws_sockfd_type sfd = wsi->desc.sockfd;

		//lws_vhost_lock(vhost);
		__lws_free_wsi(wsi); /* acquires vhost lock in wsi reset */
		//lws_vhost_unlock(vhost);

		sanity_assert_no_wsi_traces(vhost->context, wsi);
		sanity_assert_no_sockfd_traces(vhost->context, sfd);
	}

	return NULL;

connect_to:
	/*
	 * It looks like the sul_connect_timeout fired
	 */
	lwsl_wsi_info(wsi, "abandoning connect due to timeout");

try_next_dns_result_fds:
	lws_pt_lock(pt, __func__);
	__remove_wsi_socket_from_fds(wsi);
	lws_pt_unlock(pt);

try_next_dns_result_closesock:
	/*
	 * We are killing the socket but leaving
	 */
	compatible_close(wsi->desc.sockfd);
	wsi->desc.sockfd = LWS_SOCK_INVALID;

try_next_dns_result:
	lws_sul_cancel(&wsi->sul_connect_timeout);
	if (lws_dll2_get_head(&wsi->dns_sorted_list))
		goto next_dns_result;

	lws_addrinfo_clean(wsi);
	lws_inform_client_conn_fail(wsi, (void *)cce, strlen(cce));

failed1:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "client_connect3");

	return NULL;
}
