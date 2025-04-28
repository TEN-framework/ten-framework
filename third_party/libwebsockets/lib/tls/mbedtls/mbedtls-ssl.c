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
#include "private-lib-tls-mbedtls.h"

void
lws_ssl_destroy(struct lws_vhost *vhost)
{
	if (!lws_check_opt(vhost->context->options,
			   LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT))
		return;

	if (vhost->tls.ssl_ctx)
		SSL_CTX_free(vhost->tls.ssl_ctx);
	if (!vhost->tls.user_supplied_ssl_ctx && vhost->tls.ssl_client_ctx)
		SSL_CTX_free(vhost->tls.ssl_client_ctx);

	if (vhost->tls.x509_client_CA)
		X509_free(vhost->tls.x509_client_CA);
}

int
lws_ssl_capable_read(struct lws *wsi, unsigned char *buf, size_t len)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	int n = 0, m;

	if (!wsi->tls.ssl)
		return lws_ssl_capable_read_no_ssl(wsi, buf, len);

	errno = 0;
	n = SSL_read(wsi->tls.ssl, buf, (int)len);
#if defined(LWS_PLAT_FREERTOS)
	if (!n && errno == LWS_ENOTCONN) {
		lwsl_debug("%s: SSL_read ENOTCONN\n", lws_wsi_tag(wsi));
		return LWS_SSL_CAPABLE_ERROR;
	}
#endif

	lwsl_debug("%s: %s: SSL_read says %d\n", __func__, lws_wsi_tag(wsi), n);
	/* manpage: returning 0 means connection shut down */
	if (!n) {
		wsi->socket_is_permanently_unusable = 1;

		return LWS_SSL_CAPABLE_ERROR;
	}

	if (n < 0) {
		m = SSL_get_error(wsi->tls.ssl, n);
		lwsl_debug("%s: %s: ssl err %d errno %d\n", __func__, lws_wsi_tag(wsi), m, errno);
		if (errno == LWS_ENOTCONN)
			/* If the socket isn't connected anymore, bail out. */
			goto do_err1;

#if defined(LWS_PLAT_FREERTOS)
		if (errno == LWS_ECONNABORTED)
			goto do_err1;
#endif

		if (m == SSL_ERROR_ZERO_RETURN ||
		    m == SSL_ERROR_SYSCALL)
			goto do_err;

		if (m == SSL_ERROR_WANT_READ || SSL_want_read(wsi->tls.ssl)) {
			lwsl_debug("%s: WANT_READ\n", __func__);
			lwsl_debug("%s: LWS_SSL_CAPABLE_MORE_SERVICE\n", lws_wsi_tag(wsi));
			return LWS_SSL_CAPABLE_MORE_SERVICE;
		}
		if (m == SSL_ERROR_WANT_WRITE || SSL_want_write(wsi->tls.ssl)) {
			lwsl_info("%s: WANT_WRITE\n", __func__);
			lwsl_debug("%s: LWS_SSL_CAPABLE_MORE_SERVICE\n", lws_wsi_tag(wsi));
			wsi->tls_read_wanted_write = 1;
			lws_callback_on_writable(wsi);
			return LWS_SSL_CAPABLE_MORE_SERVICE;
		}

do_err1:
		wsi->socket_is_permanently_unusable = 1;

do_err:
#if defined(LWS_WITH_SYS_METRICS)
	if (wsi->a.vhost)
		lws_metric_event(wsi->a.vhost->mt_traffic_rx, METRES_NOGO, 0);
#endif

		return LWS_SSL_CAPABLE_ERROR;
	}

#if defined(LWS_TLS_LOG_PLAINTEXT_RX)
	/*
	 * If using mbedtls type tls library, this is the earliest point for all
	 * paths to dump what was received as decrypted data from the tls tunnel
	 */
	lwsl_notice("%s: len %d\n", __func__, n);
	lwsl_hexdump_notice(buf, (size_t)n);
#endif

#if defined(LWS_WITH_SYS_METRICS)
	if (wsi->a.vhost)
		lws_metric_event(wsi->a.vhost->mt_traffic_rx,
				 METRES_GO /* rx */, (u_mt_t)n);
#endif

	/*
	 * if it was our buffer that limited what we read,
	 * check if SSL has additional data pending inside SSL buffers.
	 *
	 * Because these won't signal at the network layer with POLLIN
	 * and if we don't realize, this data will sit there forever
	 */
	if (n != (int)len)
		goto bail;
	if (!wsi->tls.ssl)
		goto bail;

	if (SSL_pending(wsi->tls.ssl)) {
		if (lws_dll2_is_detached(&wsi->tls.dll_pending_tls))
			lws_dll2_add_head(&wsi->tls.dll_pending_tls,
					  &pt->tls.dll_pending_tls_owner);
	} else
		__lws_ssl_remove_wsi_from_buffered_list(wsi);

	return n;
bail:
	lws_ssl_remove_wsi_from_buffered_list(wsi);

	return n;
}

int
lws_ssl_pending(struct lws *wsi)
{
	if (!wsi->tls.ssl)
		return 0;

	return SSL_pending(wsi->tls.ssl);
}

int
lws_ssl_capable_write(struct lws *wsi, unsigned char *buf, size_t len)
{
	int n, m;

#if defined(LWS_TLS_LOG_PLAINTEXT_TX)
	/*
	 * If using mbedtls type tls library, this is the last point for all
	 * paths before sending data into the tls tunnel, where you can dump it
	 * and see what is being sent.
	 */
	lwsl_notice("%s: len %d\n", __func__, (int)len);
	lwsl_hexdump_notice(buf, len);
#endif

	if (!wsi->tls.ssl)
		return lws_ssl_capable_write_no_ssl(wsi, buf, len);

	n = SSL_write(wsi->tls.ssl, buf, (int)len);
	if (n > 0) {
#if defined(LWS_WITH_SYS_METRICS)
		if (wsi->a.vhost)
			lws_metric_event(wsi->a.vhost->mt_traffic_tx,
					 METRES_GO, (u_mt_t)n);
#endif
		return n;
	}

	m = SSL_get_error(wsi->tls.ssl, n);
	if (m != SSL_ERROR_SYSCALL) {
		if (m == SSL_ERROR_WANT_READ || SSL_want_read(wsi->tls.ssl)) {
			lwsl_notice("%s: want read\n", __func__);

			return LWS_SSL_CAPABLE_MORE_SERVICE;
		}

		if (m == SSL_ERROR_WANT_WRITE || SSL_want_write(wsi->tls.ssl)) {
			lws_set_blocking_send(wsi);
			lwsl_debug("%s: want write\n", __func__);

			return LWS_SSL_CAPABLE_MORE_SERVICE;
		}
	}

	lwsl_debug("%s failed: %d\n",__func__, m);
	wsi->socket_is_permanently_unusable = 1;

#if defined(LWS_WITH_SYS_METRICS)
		if (wsi->a.vhost)
			lws_metric_event(wsi->a.vhost->mt_traffic_tx,
					 METRES_NOGO, (u_mt_t)n);
#endif

	return LWS_SSL_CAPABLE_ERROR;
}

int openssl_SSL_CTX_private_data_index;

void
lws_ssl_info_callback(const SSL *ssl, int where, int ret)
{
	struct lws *wsi;
	struct lws_context *context;
	struct lws_ssl_info si;

	context = (struct lws_context *)SSL_CTX_get_ex_data(
					SSL_get_SSL_CTX(ssl),
					openssl_SSL_CTX_private_data_index);
	if (!context)
		return;
	wsi = wsi_from_fd(context, SSL_get_fd(ssl));
	if (!wsi)
		return;

	if (!(where & wsi->a.vhost->tls.ssl_info_event_mask))
		return;

	si.where = where;
	si.ret = ret;

	if (user_callback_handle_rxflow(wsi->a.protocol->callback,
					wsi, LWS_CALLBACK_SSL_INFO,
					wsi->user_space, &si, 0))
		lws_set_timeout(wsi, PENDING_TIMEOUT_KILLED_BY_SSL_INFO, -1);
}


int
lws_ssl_close(struct lws *wsi)
{
	lws_sockfd_type n;

	if (!wsi->tls.ssl)
		return 0; /* not handled */

#if defined (LWS_HAVE_SSL_SET_INFO_CALLBACK)
	/* kill ssl callbacks, becausse we will remove the fd from the
	 * table linking it to the wsi
	 */
	if (wsi->a.vhost->tls.ssl_info_event_mask)
		SSL_set_info_callback(wsi->tls.ssl, NULL);
#endif

#if defined(LWS_TLS_SYNTHESIZE_CB)
	lws_sul_cancel(&wsi->tls.sul_cb_synth);
	/*
	 * ... check the session in case it did not live long enough to get
	 * the scheduled callback to sample it
	 */
	lws_sess_cache_synth_cb(&wsi->tls.sul_cb_synth);
#endif

	n = SSL_get_fd(wsi->tls.ssl);
	if (!wsi->socket_is_permanently_unusable)
		SSL_shutdown(wsi->tls.ssl);
	compatible_close(n);
	SSL_free(wsi->tls.ssl);
	wsi->tls.ssl = NULL;

	lws_tls_restrict_return(wsi);

	return 1; /* handled */
}

void
lws_ssl_SSL_CTX_destroy(struct lws_vhost *vhost)
{
	if (vhost->tls.ssl_ctx)
		SSL_CTX_free(vhost->tls.ssl_ctx);

	if (!vhost->tls.user_supplied_ssl_ctx && vhost->tls.ssl_client_ctx)
		SSL_CTX_free(vhost->tls.ssl_client_ctx);
#if defined(LWS_WITH_ACME)
	lws_tls_acme_sni_cert_destroy(vhost);
#endif
}

void
lws_ssl_context_destroy(struct lws_context *context)
{
}

lws_tls_ctx *
lws_tls_ctx_from_wsi(struct lws *wsi)
{
	if (!wsi->tls.ssl)
		return NULL;

	return SSL_get_SSL_CTX(wsi->tls.ssl);
}

enum lws_ssl_capable_status
__lws_tls_shutdown(struct lws *wsi)
{
	int n = SSL_shutdown(wsi->tls.ssl);

	lwsl_debug("SSL_shutdown=%d for fd %d\n", n, wsi->desc.sockfd);

	switch (n) {
	case 1: /* successful completion */
		(void)shutdown(wsi->desc.sockfd, SHUT_WR);
		return LWS_SSL_CAPABLE_DONE;

	case 0: /* needs a retry */
		__lws_change_pollfd(wsi, 0, LWS_POLLIN);
		return LWS_SSL_CAPABLE_MORE_SERVICE;

	default: /* fatal error, or WANT */
		n = SSL_get_error(wsi->tls.ssl, n);
		if (n != SSL_ERROR_SYSCALL && n != SSL_ERROR_SSL) {
			if (SSL_want_read(wsi->tls.ssl)) {
				lwsl_debug("(wants read)\n");
				__lws_change_pollfd(wsi, 0, LWS_POLLIN);
				return LWS_SSL_CAPABLE_MORE_SERVICE_READ;
			}
			if (SSL_want_write(wsi->tls.ssl)) {
				lwsl_debug("(wants write)\n");
				__lws_change_pollfd(wsi, 0, LWS_POLLOUT);
				return LWS_SSL_CAPABLE_MORE_SERVICE_WRITE;
			}
		}
		return LWS_SSL_CAPABLE_ERROR;
	}
}


static int
tops_fake_POLLIN_for_buffered_mbedtls(struct lws_context_per_thread *pt)
{
	return lws_tls_fake_POLLIN_for_buffered(pt);
}

const struct lws_tls_ops tls_ops_mbedtls = {
	/* fake_POLLIN_for_buffered */	tops_fake_POLLIN_for_buffered_mbedtls,
};
