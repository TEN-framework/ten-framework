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

/*
 * notice this returns number of bytes consumed, or -1
 */
int
lws_issue_raw(struct lws *wsi, unsigned char *buf, size_t len)
{
	struct lws_context *context = lws_get_context(wsi);
	size_t real_len = len;
	unsigned int n, m;

	/*
	 * If you're looking to dump data being sent down the tls tunnel, see
	 * lws_ssl_capable_write() in lib/tls/mbedtls/mbedtls-ssl.c or
	 * lib/tls/openssl/openssl-ssl.c.
	 *
	 * There's also a corresponding lws_ssl_capable_read() in those files
	 * where you can enable a dump of decrypted data as soon as it was
	 * read.
	 */

	/* just ignore sends after we cleared the truncation buffer */
	if (lwsi_state(wsi) == LRS_FLUSHING_BEFORE_CLOSE &&
	    !lws_has_buffered_out(wsi)
#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	    && !wsi->http.comp_ctx.may_have_more
#endif
	    )
		return (int)len;

	if (buf && lws_has_buffered_out(wsi)) {
		lwsl_wsi_info(wsi, "** prot: %s, incr buflist_out by %lu",
				   wsi->a.protocol->name, (unsigned long)len);

		/*
		 * already buflist ahead of this, add it on the tail of the
		 * buflist, then ignore it for now and act like we're flushing
		 * the buflist...
		 */

		if (lws_buflist_append_segment(&wsi->buflist_out, buf, len))
			return -1;

		buf = NULL;
		len = 0;
	}

	if (wsi->buflist_out) {
		/* we have to drain the earliest buflist_out stuff first */

		len = lws_buflist_next_segment_len(&wsi->buflist_out, &buf);
		real_len = len;

		lwsl_wsi_debug(wsi, "draining %d", (int)len);
	}

	if (!len || !buf)
		return 0;

	if (!wsi->mux_substream && !lws_socket_is_valid(wsi->desc.sockfd))
		lwsl_wsi_err(wsi, "invalid sock");

	/* limit sending */
	if (wsi->a.protocol->tx_packet_size)
		n = (unsigned int)wsi->a.protocol->tx_packet_size;
	else {
		n = (unsigned int)wsi->a.protocol->rx_buffer_size;
		if (!n)
			n = context->pt_serv_buf_size;
	}
	n += LWS_PRE + 4;
	if (n > len)
		n = (unsigned int)len;

	/* nope, send it on the socket directly */

	if (lws_fi(&wsi->fic, "sendfail"))
		m = (unsigned int)LWS_SSL_CAPABLE_ERROR;
	else
		m = (unsigned int)lws_ssl_capable_write(wsi, buf, n);

	lwsl_wsi_info(wsi, "ssl_capable_write (%d) says %d", n, m);

	/* something got written, it can have been truncated now */
	wsi->could_have_pending = 1;

	switch ((int)m) {
	case LWS_SSL_CAPABLE_ERROR:
		/* we're going to close, let close know sends aren't possible */
		wsi->socket_is_permanently_unusable = 1;
		return -1;
	case LWS_SSL_CAPABLE_MORE_SERVICE:
		/*
		 * nothing got sent, not fatal.  Retry the whole thing later,
		 * ie, implying treat it was a truncated send so it gets
		 * retried
		 */
		m = 0;
		break;
	}

	if ((int)m < 0)
		m = 0;

	/*
	 * we were sending this from buflist_out?  Then not sending everything
	 * is a small matter of advancing ourselves only by the amount we did
	 * send in the buflist.
	 */
	if (lws_has_buffered_out(wsi)) {
		if (m) {
			lwsl_wsi_info(wsi, "partial adv %d (vs %ld)",
					   m, (long)real_len);
			lws_buflist_use_segment(&wsi->buflist_out, m);
		}

		if (!lws_has_buffered_out(wsi)) {
			lwsl_wsi_info(wsi, "buflist_out flushed");

			m = (unsigned int)real_len;
			if (lwsi_state(wsi) == LRS_FLUSHING_BEFORE_CLOSE) {
				lwsl_wsi_info(wsi, "*signalling to close now");
				return -1; /* retry closing now */
			}

			if (wsi->close_when_buffered_out_drained) {
				wsi->close_when_buffered_out_drained = 0;
				return -1;
			}

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
#if defined(LWS_WITH_SERVER)
			if (wsi->http.deferred_transaction_completed) {
				lwsl_wsi_notice(wsi, "partial completed, doing "
					    "deferred transaction completed");
				wsi->http.deferred_transaction_completed = 0;
				return lws_http_transaction_completed(wsi) ?
							-1 : (int)real_len;
			}
#endif
#endif
#if defined(LWS_ROLE_WS)
			/* Since buflist_out flushed, we're not inside a frame any more */
			if (wsi->ws)
				wsi->ws->inside_frame = 0;
#endif
		}
		/* always callback on writeable */
		lws_callback_on_writable(wsi);

		return (int)m;
	}

#if defined(LWS_WITH_HTTP_STREAM_COMPRESSION)
	if (wsi->http.comp_ctx.may_have_more)
		lws_callback_on_writable(wsi);
#endif

	if (m == real_len)
		/* what we just sent went out cleanly */
		return (int)m;

	/*
	 * We were not able to send everything... and we were not sending from
	 * an existing buflist_out.  So we are starting a fresh buflist_out, by
	 * buffering the unsent remainder on it.
	 * (it will get first priority next time the socket is writable).
	 */
	lwsl_wsi_debug(wsi, "new partial sent %d from %lu total",
			    m, (unsigned long)real_len);

	if (lws_buflist_append_segment(&wsi->buflist_out, buf + m,
				       real_len - m) < 0)
		return -1;

#if defined(LWS_WITH_UDP)
	if (lws_wsi_is_udp(wsi))
		/* stash original destination for fulfilling UDP partials */
		wsi->udp->sa46_pending = wsi->udp->sa46;
#endif

	/* since something buffered, force it to get another chance to send */
	lws_callback_on_writable(wsi);

	return (int)real_len;
}

int
lws_write(struct lws *wsi, unsigned char *buf, size_t len,
	  enum lws_write_protocol wp)
{
	int m;

	if ((int)len < 0) {
		lwsl_wsi_err(wsi, "suspicious len int %d, ulong %lu",
				  (int)len, (unsigned long)len);
		return -1;
	}

#ifdef LWS_WITH_ACCESS_LOG
	wsi->http.access_log.sent += len;
#endif

	assert(wsi->role_ops);

	if (!lws_rops_fidx(wsi->role_ops, LWS_ROPS_write_role_protocol))
		m = lws_issue_raw(wsi, buf, len);
	else
		m = lws_rops_func_fidx(wsi->role_ops, LWS_ROPS_write_role_protocol).
				write_role_protocol(wsi, buf, len, &wp);

#if defined(LWS_WITH_SYS_METRICS)
	if (wsi->a.vhost)
		lws_metric_event(wsi->a.vhost->mt_traffic_tx, (char)
				 (m < 0 ? METRES_NOGO : METRES_GO), len);
#endif

	return m;
}

int
lws_ssl_capable_read_no_ssl(struct lws *wsi, unsigned char *buf, size_t len)
{
	int n = 0, en;

	errno = 0;
#if defined(LWS_WITH_UDP)
	if (lws_wsi_is_udp(wsi)) {
		socklen_t slt = sizeof(wsi->udp->sa46);

		n = (int)recvfrom(wsi->desc.sockfd, (char *)buf,
#if defined(WIN32)
				(int)
#endif
				len, 0,
				sa46_sockaddr(&wsi->udp->sa46), &slt);
	} else
#endif
		n = (int)recv(wsi->desc.sockfd, (char *)buf,
#if defined(WIN32)
				(int)
#endif
				len, 0);
	en = LWS_ERRNO;
	if (n >= 0) {

		if (!n && wsi->unix_skt)
			goto do_err;

		/*
		 * See https://libwebsockets.org/
		 * pipermail/libwebsockets/2019-March/007857.html
		 */
		if (!n && !wsi->unix_skt)
			goto do_err;

#if defined(LWS_WITH_SYS_METRICS) && defined(LWS_WITH_SERVER)
		if (wsi->a.vhost)
			lws_metric_event(wsi->a.vhost->mt_traffic_rx,
					 METRES_GO /* rx */, (unsigned int)n);
#endif

		return n;
	}

	if (en == LWS_EAGAIN ||
	    en == LWS_EWOULDBLOCK ||
	    en == LWS_EINTR)
		return LWS_SSL_CAPABLE_MORE_SERVICE;

do_err:
#if defined(LWS_WITH_SYS_METRICS) && defined(LWS_WITH_SERVER)
	if (wsi->a.vhost)
		lws_metric_event(wsi->a.vhost->mt_traffic_rx, METRES_NOGO, 0u);
#endif

	lwsl_wsi_info(wsi, "error on reading from skt : %d, errno %d", n, en);

	return LWS_SSL_CAPABLE_ERROR;
}

int
lws_ssl_capable_write_no_ssl(struct lws *wsi, unsigned char *buf, size_t len)
{
	int n = 0;
#if defined(LWS_PLAT_OPTEE)
	ssize_t send(int sockfd, const void *buf, size_t len, int flags);
#endif

#if defined(LWS_WITH_UDP)
	if (lws_wsi_is_udp(wsi)) {

		if (lws_fi(&wsi->fic, "udp_tx_loss")) {
			/* pretend it was sent */
			n = (int)(ssize_t)len;
			goto post_send;
		}

		if (lws_has_buffered_out(wsi))
			n = (int)sendto(wsi->desc.sockfd, (const char *)buf,
#if defined(WIN32)
				(int)
#endif
				   len, 0, sa46_sockaddr(&wsi->udp->sa46_pending),
				   sa46_socklen(&wsi->udp->sa46_pending));
		else
			n = (int)sendto(wsi->desc.sockfd, (const char *)buf,
#if defined(WIN32)
				(int)
#endif
				   len, 0, sa46_sockaddr(&wsi->udp->sa46),
				   sa46_socklen(&wsi->udp->sa46));
	} else
#endif
		if (wsi->role_ops->file_handle)
			n = (int)write((int)(lws_intptr_t)wsi->desc.filefd, buf,
#if defined(WIN32)
				(int)
#endif
					len);
		else
			n = (int)send(wsi->desc.sockfd, (char *)buf,
#if defined(WIN32)
				(int)
#endif
					len, MSG_NOSIGNAL);
//	lwsl_info("%s: sent len %d result %d", __func__, len, n);

#if defined(LWS_WITH_UDP)
post_send:
#endif
	if (n >= 0)
		return n;

	if (LWS_ERRNO == LWS_EAGAIN ||
	    LWS_ERRNO == LWS_EWOULDBLOCK ||
	    LWS_ERRNO == LWS_EINTR) {
		if (LWS_ERRNO == LWS_EWOULDBLOCK) {
			lws_set_blocking_send(wsi);
		}

		return LWS_SSL_CAPABLE_MORE_SERVICE;
	}

	lwsl_wsi_debug(wsi, "ERROR writing len %d to skt fd %d err %d / errno %d",
			    (int)(ssize_t)len, wsi->desc.sockfd, n, LWS_ERRNO);

	return LWS_SSL_CAPABLE_ERROR;
}

int
lws_ssl_pending_no_ssl(struct lws *wsi)
{
	(void)wsi;
#if defined(LWS_PLAT_FREERTOS)
	return 100;
#else
	return 0;
#endif
}
