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
#include "private-lib-tls-openssl.h"

int openssl_websocket_private_data_index,
	   openssl_SSL_CTX_private_data_index;

/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

int lws_openssl_describe_cipher(struct lws *wsi)
{
#if !defined(LWS_WITH_NO_LOGS) && !defined(USE_WOLFSSL)
	int np = -1;
	SSL *s = wsi->tls.ssl;

	SSL_get_cipher_bits(s, &np);
	lwsl_info("%s: %s: %s, %s, %d bits, %s\n", __func__, lws_wsi_tag(wsi),
			SSL_get_cipher_name(s), SSL_get_cipher(s), np,
			SSL_get_cipher_version(s));
#endif

	return 0;
}

int lws_ssl_get_error(struct lws *wsi, int n)
{
	int m;

	if (!wsi->tls.ssl)
		return 99;

	m = SSL_get_error(wsi->tls.ssl, n);
       lwsl_debug("%s: %p %d -> %d (errno %d)\n", __func__, wsi->tls.ssl, n, m, LWS_ERRNO);
	if (m == SSL_ERROR_SSL)
		lws_tls_err_describe_clear();

       // assert (LWS_ERRNO != 9);

	return m;
}

#if defined(LWS_WITH_SERVER)
static int
lws_context_init_ssl_pem_passwd_cb(char *buf, int size, int rwflag,
				   void *userdata)
{
	struct lws_context_creation_info * info =
			(struct lws_context_creation_info *)userdata;

	strncpy(buf, info->ssl_private_key_password, (unsigned int)size);
	buf[size - 1] = '\0';

	return (int)strlen(buf);
}
#endif

#if defined(LWS_WITH_CLIENT)
static int
lws_context_init_ssl_pem_passwd_client_cb(char *buf, int size, int rwflag,
					  void *userdata)
{
	struct lws_context_creation_info * info =
			(struct lws_context_creation_info *)userdata;
	const char *p = info->ssl_private_key_password;

	if (info->client_ssl_private_key_password)
		p = info->client_ssl_private_key_password;

	strncpy(buf, p, (unsigned int)size);
	buf[size - 1] = '\0';

	return (int)strlen(buf);
}
#endif

void
lws_ssl_bind_passphrase(SSL_CTX *ssl_ctx, int is_client,
			const struct lws_context_creation_info *info)
{
	if (
#if defined(LWS_WITH_SERVER)
		!info->ssl_private_key_password
#endif
#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_CLIENT)
			&&
#endif
#if defined(LWS_WITH_CLIENT)
	    !info->client_ssl_private_key_password
#endif
	    )
		return;
	/*
	 * password provided, set ssl callback and user data
	 * for checking password which will be trigered during
	 * SSL_CTX_use_PrivateKey_file function
	 */
	SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (void *)info);
	SSL_CTX_set_default_passwd_cb(ssl_ctx, is_client ?
#if defined(LWS_WITH_CLIENT)
				      lws_context_init_ssl_pem_passwd_client_cb:
#else
					NULL:
#endif
#if defined(LWS_WITH_SERVER)
				      lws_context_init_ssl_pem_passwd_cb
#else
				      	NULL
#endif
				  );
}

#if defined(LWS_WITH_CLIENT)
static void
lws_ssl_destroy_client_ctx(struct lws_vhost *vhost)
{
	if (vhost->tls.user_supplied_ssl_ctx || !vhost->tls.ssl_client_ctx)
		return;

	if (vhost->tls.tcr && --vhost->tls.tcr->refcount)
		return;

	SSL_CTX_free(vhost->tls.ssl_client_ctx);
	vhost->tls.ssl_client_ctx = NULL;

	vhost->context->tls.count_client_contexts--;

	if (vhost->tls.tcr) {
		lws_dll2_remove(&vhost->tls.tcr->cc_list);
		lws_free(vhost->tls.tcr);
		vhost->tls.tcr = NULL;
	}
}
#endif
void
lws_ssl_destroy(struct lws_vhost *vhost)
{
	if (!lws_check_opt(vhost->context->options,
			   LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT))
		return;

	if (vhost->tls.ssl_ctx)
		SSL_CTX_free(vhost->tls.ssl_ctx);
#if defined(LWS_WITH_CLIENT)
	lws_ssl_destroy_client_ctx(vhost);
#endif

// after 1.1.0 no need
#if (OPENSSL_VERSION_NUMBER <  0x10100000)
// <= 1.0.1f = old api, 1.0.1g+ = new api
#if (OPENSSL_VERSION_NUMBER <= 0x1000106f) || defined(USE_WOLFSSL)
	ERR_remove_state(0);
#else
#if OPENSSL_VERSION_NUMBER >= 0x1010005f && \
    !defined(LIBRESSL_VERSION_NUMBER) && \
    !defined(OPENSSL_IS_BORINGSSL)
	ERR_remove_thread_state();
#else
	ERR_remove_thread_state(NULL);
#endif
#endif
	/* not needed after 1.1.0 */
#if  (OPENSSL_VERSION_NUMBER >= 0x10002000) && \
     (OPENSSL_VERSION_NUMBER <= 0x10100000)
	SSL_COMP_free_compression_methods();
#endif
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
#endif
}

int
lws_ssl_capable_read(struct lws *wsi, unsigned char *buf, size_t len)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	int n = 0, m;

	if (!wsi->tls.ssl)
		return lws_ssl_capable_read_no_ssl(wsi, buf, len);

#ifndef WIN32
	errno = 0;
#else
  WSASetLastError(0);
#endif
	ERR_clear_error();
	n = SSL_read(wsi->tls.ssl, buf, (int)(ssize_t)len);
#if defined(LWS_PLAT_FREERTOS)
	if (!n && errno == LWS_ENOTCONN) {
		lwsl_debug("%s: SSL_read ENOTCONN\n", lws_wsi_tag(wsi));
		return LWS_SSL_CAPABLE_ERROR;
	}
#endif

	lwsl_debug("%s: SSL_read says %d\n", lws_wsi_tag(wsi), n);
	/* manpage: returning 0 means connection shut down
	 *
	 * 2018-09-10: https://github.com/openssl/openssl/issues/1903
	 *
	 * So, in summary, if you get a 0 or -1 return from SSL_read() /
	 * SSL_write(), you should call SSL_get_error():
	 *
	 *  - If you get back SSL_ERROR_RETURN_ZERO then you know the connection
	 *    has been cleanly shutdown by the peer. To fully close the
	 *    connection you may choose to call SSL_shutdown() to send a
	 *    close_notify back.
	 *
	 *  - If you get back SSL_ERROR_SSL then some kind of internal or
	 *    protocol error has occurred. More details will be on the SSL error
	 *    queue. You can also call SSL_get_shutdown(). If this indicates a
	 *    state of SSL_RECEIVED_SHUTDOWN then you know a fatal alert has
	 *    been received from the peer (if it had been a close_notify then
	 *    SSL_get_error() would have returned SSL_ERROR_RETURN_ZERO).
	 *    SSL_ERROR_SSL is considered fatal - you should not call
	 *    SSL_shutdown() in this case.
	 *
	 *  - If you get back SSL_ERROR_SYSCALL then some kind of fatal (i.e.
	 *    non-retryable) error has occurred in a system call.
	 */
	if (n <= 0) {
		m = lws_ssl_get_error(wsi, n);
               lwsl_debug("%s: ssl err %d errno %d\n", lws_wsi_tag(wsi), m, LWS_ERRNO);
		if (m == SSL_ERROR_ZERO_RETURN) /* cleanly shut down */
			goto do_err;

		/* hm not retryable.. could be 0 size pkt or error  */

		if (m == SSL_ERROR_SSL || m == SSL_ERROR_SYSCALL ||
        LWS_ERRNO == LWS_ENOTCONN) {

			/* unclean, eg closed conn */

			wsi->socket_is_permanently_unusable = 1;
do_err:
#if defined(LWS_WITH_SYS_METRICS)
		if (wsi->a.vhost)
			lws_metric_event(wsi->a.vhost->mt_traffic_rx,
					 METRES_NOGO, 0);
#endif
			return LWS_SSL_CAPABLE_ERROR;
		}

		/* retryable? */

		if (SSL_want_read(wsi->tls.ssl)) {
			lwsl_debug("%s: WANT_READ\n", __func__);
			lwsl_debug("%s: LWS_SSL_CAPABLE_MORE_SERVICE\n", lws_wsi_tag(wsi));
			return LWS_SSL_CAPABLE_MORE_SERVICE;
		}
		if (SSL_want_write(wsi->tls.ssl)) {
			lwsl_info("%s: WANT_WRITE\n", __func__);
			lwsl_debug("%s: LWS_SSL_CAPABLE_MORE_SERVICE\n", lws_wsi_tag(wsi));
			wsi->tls_read_wanted_write = 1;
			lws_callback_on_writable(wsi);
			return LWS_SSL_CAPABLE_MORE_SERVICE;
		}

		/* keep on trucking it seems */
	}

#if defined(LWS_TLS_LOG_PLAINTEXT_RX)
	/*
	 * If using openssl type tls library, this is the earliest point for all
	 * paths to dump what was received as decrypted data from the tls tunnel
	 */
	lwsl_notice("%s: len %d\n", __func__, n);
	lwsl_hexdump_notice(buf, (unsigned int)n);
#endif

#if defined(LWS_WITH_SYS_METRICS)
	if (wsi->a.vhost)
		lws_metric_event(wsi->a.vhost->mt_traffic_rx, METRES_GO, (u_mt_t)n);
#endif

	/*
	 * if it was our buffer that limited what we read,
	 * check if SSL has additional data pending inside SSL buffers.
	 *
	 * Because these won't signal at the network layer with POLLIN
	 * and if we don't realize, this data will sit there forever
	 */
	if (n != (int)(ssize_t)len)
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
	 * If using OpenSSL type tls library, this is the last point for all
	 * paths before sending data into the tls tunnel, where you can dump it
	 * and see what is being sent.
	 */
	lwsl_notice("%s: len %u\n", __func__, (unsigned int)len);
	lwsl_hexdump_notice(buf, len);
#endif

	if (!wsi->tls.ssl)
		return lws_ssl_capable_write_no_ssl(wsi, buf, len);

	errno = 0;
	ERR_clear_error();
	n = SSL_write(wsi->tls.ssl, buf, (int)(ssize_t)len);
	if (n > 0) {
#if defined(LWS_WITH_SYS_METRICS)
		if (wsi->a.vhost)
			lws_metric_event(wsi->a.vhost->mt_traffic_tx,
					 METRES_GO, (u_mt_t)n);
#endif
		return n;
	}

	m = lws_ssl_get_error(wsi, n);
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

	lwsl_debug("%s failed: %s\n",__func__, ERR_error_string((unsigned int)m, NULL));
	lws_tls_err_describe_clear();

	wsi->socket_is_permanently_unusable = 1;

#if defined(LWS_WITH_SYS_METRICS)
		if (wsi->a.vhost)
			lws_metric_event(wsi->a.vhost->mt_traffic_tx,
					 METRES_NOGO, 0);
#endif

	return LWS_SSL_CAPABLE_ERROR;
}

void
lws_ssl_info_callback(const SSL *ssl, int where, int ret)
{
	struct lws *wsi;
	struct lws_context *context;
	struct lws_ssl_info si;
	int fd;

#ifndef USE_WOLFSSL
	context = (struct lws_context *)SSL_CTX_get_ex_data(
					SSL_get_SSL_CTX(ssl),
					openssl_SSL_CTX_private_data_index);
#else
	context = (struct lws_context *)SSL_CTX_get_ex_data(
					SSL_get_SSL_CTX((SSL*) ssl),
					openssl_SSL_CTX_private_data_index);
#endif
	if (!context)
		return;

	fd = SSL_get_fd(ssl);
	if (fd < 0 || (fd - lws_plat_socket_offset()) < 0)
		return;

	wsi = wsi_from_fd(context, fd);
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
	/* kill ssl callbacks, because we will remove the fd from the
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

	// lwsl_notice("%s: ssl restr %d, simul %d\n", __func__,
	//		wsi->a.context->simultaneous_ssl_restriction,
	//		wsi->a.context->simultaneous_ssl);

	return 1; /* handled */
}

void
lws_ssl_SSL_CTX_destroy(struct lws_vhost *vhost)
{
	if (vhost->tls.ssl_ctx)
		SSL_CTX_free(vhost->tls.ssl_ctx);

#if defined(LWS_WITH_CLIENT)
	lws_ssl_destroy_client_ctx(vhost);
#endif

#if defined(LWS_WITH_ACME)
	lws_tls_acme_sni_cert_destroy(vhost);
#endif
}

void
lws_ssl_context_destroy(struct lws_context *context)
{
// after 1.1.0 no need
#if (OPENSSL_VERSION_NUMBER <  0x10100000)
// <= 1.0.1f = old api, 1.0.1g+ = new api
#if (OPENSSL_VERSION_NUMBER <= 0x1000106f) || defined(USE_WOLFSSL)
	ERR_remove_state(0);
#else
#if OPENSSL_VERSION_NUMBER >= 0x1010005f && \
    !defined(LIBRESSL_VERSION_NUMBER) && \
    !defined(OPENSSL_IS_BORINGSSL)
	ERR_remove_thread_state();
#else
	ERR_remove_thread_state(NULL);
#endif
#endif
	// after 1.1.0 no need
#if  (OPENSSL_VERSION_NUMBER >= 0x10002000) && (OPENSSL_VERSION_NUMBER <= 0x10100000)
	SSL_COMP_free_compression_methods();
#endif
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
#endif
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
	int n;

#ifndef WIN32
	errno = 0;
#else
  WSASetLastError(0);
#endif
	ERR_clear_error();
	n = SSL_shutdown(wsi->tls.ssl);
	lwsl_debug("SSL_shutdown=%d for fd %d\n", n, wsi->desc.sockfd);
	switch (n) {
	case 1: /* successful completion */
		n = shutdown(wsi->desc.sockfd, SHUT_WR);
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
tops_fake_POLLIN_for_buffered_openssl(struct lws_context_per_thread *pt)
{
	return lws_tls_fake_POLLIN_for_buffered(pt);
}

const struct lws_tls_ops tls_ops_openssl = {
	/* fake_POLLIN_for_buffered */	tops_fake_POLLIN_for_buffered_openssl,
};
