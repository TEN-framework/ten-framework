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
rops_handle_POLLIN_listen(struct lws_context_per_thread *pt, struct lws *wsi,
			  struct lws_pollfd *pollfd)
{
	struct lws_context *context = wsi->a.context;
	struct lws_filter_network_conn_args filt;
	lws_sock_file_fd_type fd;

	memset(&filt, 0, sizeof(filt));

	/* if our vhost is going down, ignore it */

	if (wsi->a.vhost->being_destroyed)
		return LWS_HPI_RET_HANDLED;

	/* pollin means a client has connected to us then
	 *
	 * pollout is a hack on esp32 for background accepts signalling
	 * they completed
	 */

	do {
		struct lws *cwsi;
		int opts = LWS_ADOPT_SOCKET | LWS_ADOPT_ALLOW_SSL;

		if (!(pollfd->revents & (LWS_POLLIN | LWS_POLLOUT)) ||
		    !(pollfd->events & LWS_POLLIN))
			break;

#if defined(LWS_WITH_TLS)
		/*
		 * can we really accept it, with regards to SSL limit?
		 * another vhost may also have had POLLIN on his
		 * listener this round and used it up already
		 */
		if (wsi->a.vhost->tls.use_ssl &&
		    context->simultaneous_ssl_restriction &&
		    context->simultaneous_ssl ==
				  context->simultaneous_ssl_restriction)
			/*
			 * no... ignore it, he won't come again until
			 * we are below the simultaneous_ssl_restriction
			 * limit and POLLIN is enabled on him again
			 */
			break;
#endif
		/* listen socket got an unencrypted connection... */

		filt.clilen = sizeof(filt.cli_addr);

		/*
		 * We cannot identify the peer who is in the listen
		 * socket connect queue before we accept it; even if
		 * we could, not accepting it due to PEER_LIMITS would
		 * block the connect queue for other legit peers.
		 */

		filt.accept_fd = accept((int)pollfd->fd,
					(struct sockaddr *)&filt.cli_addr,
					&filt.clilen);
		if (filt.accept_fd == LWS_SOCK_INVALID) {
			if (LWS_ERRNO == LWS_EAGAIN ||
			    LWS_ERRNO == LWS_EWOULDBLOCK) {
				break;
			}
			lwsl_err("accept: errno %d\n", LWS_ERRNO);

			return LWS_HPI_RET_HANDLED;
		}

		if (context->being_destroyed) {
			compatible_close(filt.accept_fd);

			return LWS_HPI_RET_PLEASE_CLOSE_ME;
		}

		lws_plat_set_socket_options(wsi->a.vhost, filt.accept_fd, 0);

#if defined(LWS_WITH_IPV6)
		lwsl_debug("accepted new conn port %u on fd=%d\n",
			((filt.cli_addr.ss_family == AF_INET6) ?
			ntohs(((struct sockaddr_in6 *) &filt.cli_addr)->sin6_port) :
			ntohs(((struct sockaddr_in *) &filt.cli_addr)->sin_port)),
			filt.accept_fd);
#else
		{
		struct sockaddr_in sain;

		memcpy(&sain, &filt.cli_addr, sizeof(sain));
		lwsl_debug("accepted new conn port %u on fd=%d\n",
			   ntohs(sain.sin_port),
			   filt.accept_fd);
		}
#endif

		/*
		 * look at who we connected to and give user code a
		 * chance to reject based on client IP.  There's no
		 * protocol selected yet so we issue this to
		 * protocols[0]
		 */
		if ((wsi->a.vhost->protocols[0].callback)(wsi,
				LWS_CALLBACK_FILTER_NETWORK_CONNECTION,
				(void *)&filt,
				(void *)(lws_intptr_t)filt.accept_fd, 0)) {
			lwsl_debug("Callback denied net connection\n");
			compatible_close(filt.accept_fd);
			return LWS_HPI_RET_HANDLED;
		}

		if (!(wsi->a.vhost->options &
			LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG))
			opts |= LWS_ADOPT_HTTP;

#if defined(LWS_WITH_TLS)
		if (!wsi->a.vhost->tls.use_ssl)
#endif
			opts &= ~LWS_ADOPT_ALLOW_SSL;

		fd.sockfd = filt.accept_fd;
		cwsi = lws_adopt_descriptor_vhost(wsi->a.vhost, (lws_adoption_type)opts, fd,
				wsi->a.vhost->listen_accept_protocol, NULL);
		if (!cwsi) {
			lwsl_info("%s: vh %s: adopt failed\n", __func__,
					wsi->a.vhost->name);

			/* already closed cleanly as necessary */
			return LWS_HPI_RET_WSI_ALREADY_DIED;
		}
/*
		if (lws_server_socket_service_ssl(cwsi, accept_fd, 1)) {
			lws_close_free_wsi(cwsi, LWS_CLOSE_STATUS_NOSTATUS,
					   "listen svc fail");
			return LWS_HPI_RET_WSI_ALREADY_DIED;
		}

		lwsl_info("%s: new %s: wsistate 0x%lx, role_ops %s\n",
			    __func__, lws_wsi_tag(cwsi), (unsigned long)cwsi->wsistate,
			    cwsi->role_ops->name);
*/

	} while (pt->fds_count < context->fd_limit_per_thread - 1 &&
		 wsi->position_in_fds_table != LWS_NO_FDS_POS &&
		 lws_poll_listen_fd(&pt->fds[wsi->position_in_fds_table]) > 0);

	return LWS_HPI_RET_HANDLED;
}

int rops_handle_POLLOUT_listen(struct lws *wsi)
{
	return LWS_HP_RET_USER_SERVICE;
}

static const lws_rops_t rops_table_listen[] = {
	/*  1 */ { .handle_POLLIN	  = rops_handle_POLLIN_listen },
	/*  2 */ { .handle_POLLOUT	  = rops_handle_POLLOUT_listen },
};

const struct lws_role_ops role_ops_listen = {
	/* role name */			"listen",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_listen,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x00,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x01,
	  /* LWS_ROPS_handle_POLLOUT */
	  /* LWS_ROPS_perform_user_POLLOUT */		0x20,
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
	/* file_handle */		0,
};
