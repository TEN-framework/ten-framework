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

#if defined(LWS_WITH_SERVER)

static void
lws_sul_tls_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context_per_thread *pt = lws_container_of(sul,
			struct lws_context_per_thread, sul_tls);

	lws_tls_check_all_cert_lifetimes(pt->context);

	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &pt->sul_tls,
			    (lws_usec_t)24 * 3600 * LWS_US_PER_SEC);
}

int
lws_context_init_server_ssl(const struct lws_context_creation_info *info,
			    struct lws_vhost *vhost)
{
	struct lws_context *context = vhost->context;
	lws_fakewsi_def_plwsa(&vhost->context->pt[0]);

	lws_fakewsi_prep_plwsa_ctx(vhost->context);

	if (!lws_check_opt(info->options,
			   LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT)) {
		vhost->tls.use_ssl = 0;

		return 0;
	}

	/*
	 * If he is giving a server cert, take it as a sign he wants to use
	 * it on this vhost.  User code can leave the cert filepath NULL and
	 * set the LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX option itself, in
	 * which case he's expected to set up the cert himself at
	 * LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS, which
	 * provides the vhost SSL_CTX * in the user parameter.
	 */
	if (info->ssl_cert_filepath || info->server_ssl_cert_mem)
		vhost->options |= LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX;

	if (info->port != CONTEXT_PORT_NO_LISTEN) {

		vhost->tls.use_ssl = lws_check_opt(vhost->options,
					LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX);

		if (vhost->tls.use_ssl && info->ssl_cipher_list)
			lwsl_notice(" SSL ciphers: '%s'\n",
						info->ssl_cipher_list);

		lwsl_notice(" Vhost '%s' using %sTLS mode\n",
			    vhost->name, vhost->tls.use_ssl ? "" : "non-");
	}

	/*
	 * give him a fake wsi with context + vhost set, so he can use
	 * lws_get_context() in the callback
	 */
	plwsa->vhost = vhost; /* not a real bound wsi */

	/*
	 * as a server, if we are requiring clients to identify themselves
	 * then set the backend up for it
	 */
	if (lws_check_opt(info->options,
			  LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT))
		/* Normally SSL listener rejects non-ssl, optionally allow */
		vhost->tls.allow_non_ssl_on_ssl_port = 1;

	/*
	 * give user code a chance to load certs into the server
	 * allowing it to verify incoming client certs
	 */
	if (vhost->tls.use_ssl) {
		if (lws_tls_server_vhost_backend_init(info, vhost, (struct lws *)plwsa))
			return -1;

		lws_tls_server_client_cert_verify_config(vhost);

		if (vhost->protocols[0].callback((struct lws *)plwsa,
			    LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS,
			    vhost->tls.ssl_ctx, vhost, 0))
			return -1;
	}

	if (vhost->tls.use_ssl)
		lws_context_init_alpn(vhost);

	/* check certs in a few seconds (after protocol init) and then once a day */

	context->pt[0].sul_tls.cb = lws_sul_tls_cb;
	__lws_sul_insert_us(&context->pt[0].pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &context->pt[0].sul_tls,
			    (lws_usec_t)5 * LWS_US_PER_SEC);

	return 0;
}
#endif

int
lws_server_socket_service_ssl(struct lws *wsi, lws_sockfd_type accept_fd, char from_pollin)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_vhost *vh;
	ssize_t s;
	int n;

	if (!LWS_SSL_ENABLED(wsi->a.vhost))
		return 0;

	switch (lwsi_state(wsi)) {
	case LRS_SSL_INIT:

		if (wsi->tls.ssl)
			lwsl_err("%s: leaking ssl\n", __func__);
		if (accept_fd == LWS_SOCK_INVALID)
			assert(0);

		if (lws_tls_restrict_borrow(wsi)) {
			lwsl_err("%s: failed on ssl restriction\n", __func__);
			return 1;
		}

		if (lws_tls_server_new_nonblocking(wsi, accept_fd)) {
			lwsl_err("%s: failed on lws_tls_server_new_nonblocking\n", __func__);
			if (accept_fd != LWS_SOCK_INVALID)
				compatible_close(accept_fd);
			lws_tls_restrict_return(wsi);
			goto fail;
		}

		/*
		 * we are not accepted yet, but we need to enter ourselves
		 * as a live connection.  That way we can retry when more
		 * pieces come if we're not sorted yet
		 */
		lwsi_set_state(wsi, LRS_SSL_ACK_PENDING);

		lws_pt_lock(pt, __func__);
		if (__insert_wsi_socket_into_fds(context, wsi)) {
			lwsl_err("%s: failed to insert into fds\n", __func__);
			goto fail;
		}
		lws_pt_unlock(pt);

		lws_set_timeout(wsi, PENDING_TIMEOUT_SSL_ACCEPT,
				(int)context->timeout_secs);

		lwsl_debug("inserted SSL accept into fds, trying SSL_accept\n");

		/* fallthru */

	case LRS_SSL_ACK_PENDING:

		if (lws_change_pollfd(wsi, LWS_POLLOUT, 0)) {
			lwsl_err("%s: lws_change_pollfd failed\n", __func__);
			goto fail;
		}

		if (wsi->a.vhost->tls.allow_non_ssl_on_ssl_port && !wsi->skip_fallback) {
			/*
			 * We came here by POLLIN, so there is supposed to be
			 * something to read...
			 */

			s = recv(wsi->desc.sockfd, (char *)pt->serv_buf,
				 context->pt_serv_buf_size, MSG_PEEK);
			/*
			 * We have LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT..
			 * this just means don't hang up on him because of no
			 * tls hello... what happens next is driven by
			 * additional option flags:
			 *
			 * none: fail the connection
			 *
			 * LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS:
			 *     Destroy the TLS, issue a redirect using plaintext
			 *     http (this may not be accepted by a client that
			 *     has visited the site before and received an STS
			 *     header).
			 *
			 * LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER:
			 *     Destroy the TLS, continue and serve normally
			 *     using http
			 *
			 * LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG:
			 *     Destroy the TLS, apply whatever role and protocol
			 *     were told in the vhost info struct
			 *     .listen_accept_role / .listen_accept_protocol and
			 *     continue with that
			 */

			if (s >= 1 && pt->serv_buf[0] >= ' ') {
				/*
				* TLS content-type for Handshake is 0x16, and
				* for ChangeCipherSpec Record, it's 0x14
				*
				* A non-ssl session will start with the HTTP
				* method in ASCII.  If we see it's not a legit
				* SSL handshake kill the SSL for this
				* connection and try to handle as a HTTP
				* connection upgrade directly.
				*/
				wsi->tls.use_ssl = 0;

				lws_tls_server_abort_connection(wsi);
				/*
				 * care... this creates wsi with no ssl when ssl
				 * is enabled and normally mandatory
				 */
				wsi->tls.ssl = NULL;

				if (lws_check_opt(wsi->a.vhost->options,
				    LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS)) {
					lwsl_info("%s: redirecting from http "
						  "to https\n", __func__);
					wsi->tls.redirect_to_https = 1;
					goto notls_accepted;
				}

				if (lws_check_opt(wsi->a.vhost->options,
				LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER)) {
					lwsl_info("%s: allowing unencrypted "
						  "http service on tls port\n",
						  __func__);
					goto notls_accepted;
				}

				if (lws_check_opt(wsi->a.vhost->options,
		    LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG)) {
					if (lws_http_to_fallback(wsi, NULL, 0))
						goto fail;
					lwsl_info("%s: allowing non-tls "
						  "fallback\n", __func__);
					goto notls_accepted;
				}

				lwsl_notice("%s: client did not send a valid "
					    "tls hello (default vhost %s)\n",
					    __func__, wsi->a.vhost->name);
				goto fail;
			}
			if (!s) {
				/*
				 * POLLIN but nothing to read is supposed to
				 * mean the connection is gone, we should
				 * fail out...
				 *
				 */
				lwsl_debug("%s: PEEKed 0 (from_pollin %d)\n",
					  __func__, from_pollin);
				if (!from_pollin)
					/*
					 * If this wasn't actually info from a
					 * pollin let it go around again until
					 * either data came or we still get told
					 * zero length peek AND POLLIN
					 */
					goto punt;

				/*
				 * treat as remote closed
				 */

				goto fail;
			}
			if (s < 0 && (LWS_ERRNO == LWS_EAGAIN ||
				      LWS_ERRNO == LWS_EWOULDBLOCK)) {

punt:
				/*
				 * well, we get no way to know ssl or not
				 * so go around again waiting for something
				 * to come and give us a hint, or timeout the
				 * connection.
				 */
				if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
					lwsl_err("%s: change_pollfd failed\n",
						  __func__);
					return -1;
				}

				lwsl_info("SSL_ERROR_WANT_READ\n");
				return 0;
			}
		}

		/* normal SSL connection processing path */

		errno = 0;
		n = lws_tls_server_accept(wsi);
		lwsl_info("SSL_accept says %d\n", n);
		switch (n) {
		case LWS_SSL_CAPABLE_DONE:
			lws_tls_restrict_return_handshake(wsi);
			break;
		case LWS_SSL_CAPABLE_ERROR:
			lws_tls_restrict_return_handshake(wsi);
	                lwsl_info("%s: SSL_accept failed socket %u: %d\n",
	                		__func__, wsi->desc.sockfd, n);
			wsi->socket_is_permanently_unusable = 1;
			goto fail;

		default: /* MORE_SERVICE */
			return 0;
		}

		/* adapt our vhost to match the SNI SSL_CTX that was chosen */
		vh = context->vhost_list;
		while (vh) {
			if (!vh->being_destroyed && wsi->tls.ssl &&
			    vh->tls.ssl_ctx == lws_tls_ctx_from_wsi(wsi)) {
				lwsl_info("setting wsi to vh %s\n", vh->name);
				lws_vhost_bind_wsi(vh, wsi);
				break;
			}
			vh = vh->vhost_next;
		}

		/* OK, we are accepted... give him some time to negotiate */
		lws_set_timeout(wsi, PENDING_TIMEOUT_ESTABLISH_WITH_SERVER,
				(int)context->timeout_secs);

		lwsi_set_state(wsi, LRS_ESTABLISHED);
		if (lws_tls_server_conn_alpn(wsi)) {
			lwsl_warn("%s: fail on alpn\n", __func__);
			goto fail;
		}
		lwsl_debug("accepted new SSL conn\n");
		break;

	default:
		break;
	}

	return 0;

notls_accepted:
	lwsi_set_state(wsi, LRS_ESTABLISHED);

	return 0;

fail:
	return 1;
}

