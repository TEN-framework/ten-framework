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

static int
lws_get_idlest_tsi(struct lws_context *context)
{
	unsigned int lowest = ~0u;
	int n = 0, hit = -1;

	for (; n < context->count_threads; n++) {
		lwsl_cx_debug(context, "%d %d\n", context->pt[n].fds_count,
				context->fd_limit_per_thread - 1);
		if ((unsigned int)context->pt[n].fds_count !=
		    context->fd_limit_per_thread - 1 &&
		    (unsigned int)context->pt[n].fds_count < lowest) {
			lowest = context->pt[n].fds_count;
			hit = n;
		}
	}

	return hit;
}

struct lws *
lws_create_new_server_wsi(struct lws_vhost *vhost, int fixed_tsi, const char *desc)
{
	struct lws *new_wsi;
	int n = fixed_tsi;

	if (n < 0)
		n = lws_get_idlest_tsi(vhost->context);

	if (n < 0) {
		lwsl_vhost_err(vhost, "no space for new conn");
		return NULL;
	}

	lws_context_lock(vhost->context, __func__);
	new_wsi = __lws_wsi_create_with_role(vhost->context, n, NULL,
					     vhost->lc.log_cx);
	lws_context_unlock(vhost->context);
	if (new_wsi == NULL) {
		lwsl_vhost_err(vhost, "OOM");
		return NULL;
	}

	lws_wsi_fault_timedclose(new_wsi);

	__lws_lc_tag(vhost->context, &vhost->context->lcg[
#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
	strcmp(desc, "adopted") ? LWSLCG_WSI_MUX :
#endif
	LWSLCG_WSI_SERVER], &new_wsi->lc, desc);

	new_wsi->wsistate |= LWSIFR_SERVER;
	new_wsi->tsi = (char)n;
	lwsl_wsi_debug(new_wsi, "joining vh %s, tsi %d",
			vhost->name, new_wsi->tsi);

	lws_vhost_bind_wsi(vhost, new_wsi);
	new_wsi->rxflow_change_to = LWS_RXFLOW_ALLOW;
	new_wsi->retry_policy = vhost->retry_policy;

	/* initialize the instance struct */

	lwsi_set_state(new_wsi, LRS_UNCONNECTED);
	new_wsi->hdr_parsing_completed = 0;

#ifdef LWS_WITH_TLS
	new_wsi->tls.use_ssl = LWS_SSL_ENABLED(vhost);
#endif

	/*
	 * these can only be set once the protocol is known
	 * we set an un-established connection's protocol pointer
	 * to the start of the supported list, so it can look
	 * for matching ones during the handshake
	 */
	new_wsi->a.protocol = vhost->protocols;
	new_wsi->user_space = NULL;

	/*
	 * outermost create notification for wsi
	 * no user_space because no protocol selection
	 */
	vhost->protocols[0].callback(new_wsi, LWS_CALLBACK_WSI_CREATE, NULL,
				     NULL, 0);

	return new_wsi;
}


/* if not a socket, it's a raw, non-ssl file descriptor
 * req cx lock, acq pt lock, acq vh lock
 */

static struct lws *
__lws_adopt_descriptor_vhost1(struct lws_vhost *vh, lws_adoption_type type,
			    const char *vh_prot_name, struct lws *parent,
			    void *opaque, const char *fi_wsi_name)
{
	struct lws_context *context;
	struct lws_context_per_thread *pt;
	struct lws *new_wsi;
	int n;

	/*
	 * Notice that in SMP case, the wsi may be being created on an
	 * entirely different pt / tsi for load balancing.  In that case as
	 * we initialize it, it may become "live" concurrently unexpectedly...
	 */

	if (!vh)
		return NULL;

	context = vh->context;

	lws_context_assert_lock_held(vh->context);

	n = -1;
	if (parent)
		n = parent->tsi;
	new_wsi = lws_create_new_server_wsi(vh, n, "adopted");
	if (!new_wsi)
		return NULL;

	/* bring in specific fault injection rules early */
	lws_fi_inherit_copy(&new_wsi->fic, &context->fic, "wsi", fi_wsi_name);

	if (lws_fi(&new_wsi->fic, "createfail")) {
		lws_fi_destroy(&new_wsi->fic);

		return NULL;
	}

	new_wsi->a.opaque_user_data = opaque;

	pt = &context->pt[(int)new_wsi->tsi];
	lws_pt_lock(pt, __func__);

	if (parent) {
		new_wsi->parent = parent;
		new_wsi->sibling_list = parent->child_list;
		parent->child_list = new_wsi;
	}

	if (vh_prot_name) {
		new_wsi->a.protocol = lws_vhost_name_to_protocol(new_wsi->a.vhost,
							       vh_prot_name);
		if (!new_wsi->a.protocol) {
			lwsl_vhost_err(new_wsi->a.vhost, "Protocol %s not enabled",
						      vh_prot_name);
			goto bail;
		}
		if (lws_ensure_user_space(new_wsi)) {
			lwsl_wsi_notice(new_wsi, "OOM");
			goto bail;
		}
	}

	if (!LWS_SSL_ENABLED(new_wsi->a.vhost) ||
	    !(type & LWS_ADOPT_SOCKET))
		type &= (unsigned int)~LWS_ADOPT_ALLOW_SSL;

	if (lws_role_call_adoption_bind(new_wsi, (int)type, vh_prot_name)) {
		lwsl_wsi_err(new_wsi, "no role for desc type 0x%x", type);
		goto bail;
	}

#if defined(LWS_WITH_SERVER)
	if (new_wsi->role_ops) {
		lws_metrics_tag_wsi_add(new_wsi, "role", new_wsi->role_ops->name);
	}
#endif

	lws_pt_unlock(pt);

	/*
	 * he's an allocated wsi, but he's not on any fds list or child list,
	 * join him to the vhost's list of these kinds of incomplete wsi until
	 * he gets another identity (he may do async dns now...)
	 */
	lws_vhost_lock(new_wsi->a.vhost);
	lws_dll2_add_head(&new_wsi->vh_awaiting_socket,
			  &new_wsi->a.vhost->vh_awaiting_socket_owner);
	lws_vhost_unlock(new_wsi->a.vhost);

	return new_wsi;

bail:
	lwsl_wsi_notice(new_wsi, "exiting on bail");
	if (parent)
		parent->child_list = new_wsi->sibling_list;
	if (new_wsi->user_space)
		lws_free(new_wsi->user_space);

	lws_fi_destroy(&new_wsi->fic);

	lws_pt_unlock(pt);
	__lws_vhost_unbind_wsi(new_wsi); /* req cx, acq vh lock */

	lws_free(new_wsi);

	return NULL;
}

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SECURE_STREAMS)

/*
 * If the incoming wsi is bound to a vhost that is a ss server, this creates
 * an accepted ss bound to the wsi.
 *
 * For h1 or raw, we can do the binding here, but for muxed protocols like h2
 * or mqtt we have to do it not on the nwsi but on the stream.  And for h2 we
 * start off bound to h1 role, since we don't know if we will upgrade to h2
 * until we meet the server.
 *
 * 1) No tls is assumed to mean no muxed protocol so can do it at adopt.
 *
 * 2) After alpn if not muxed we can do it.
 *
 * 3) For muxed, do it at the nwsi migration and on new stream
 */

int
lws_adopt_ss_server_accept(struct lws *new_wsi)
{
	struct lws_context_per_thread *pt =
			&new_wsi->a.context->pt[(int)new_wsi->tsi];
	lws_ss_handle_t *h;
	void *pv, **ppv;

	if (!new_wsi->a.vhost->ss_handle)
		return 0;

	pv = (char *)&new_wsi->a.vhost->ss_handle[1];

	/*
	 * Yes... the vhost is pointing to its secure stream representing the
	 * server... we want to create an accepted SS and bind it to new_wsi,
	 * the info/ssi from the server SS (so the SS callbacks defined there),
	 * the opaque_user_data of the server object and the policy of it.
	 */

	ppv = (void **)((char *)pv +
	      new_wsi->a.vhost->ss_handle->info.opaque_user_data_offset);

	/*
	 * indicate we are an accepted connection referencing the
	 * server object
	 */

	new_wsi->a.vhost->ss_handle->info.flags |= LWSSSINFLAGS_SERVER;

	if (lws_ss_create(new_wsi->a.context, new_wsi->tsi,
			  &new_wsi->a.vhost->ss_handle->info,
			  *ppv, &h, NULL, NULL)) {
		lwsl_wsi_err(new_wsi, "accept ss creation failed");
		goto fail1;
	}

	/*
	 * We made a fresh accepted SS conn from the server pieces,
	 * now bind the wsi... the problem is, this is the nwsi if it's
	 * h2.
	 */

	h->wsi = new_wsi;
	new_wsi->a.opaque_user_data = h;
	h->info.flags |= LWSSSINFLAGS_ACCEPTED;
	/* indicate wsi should invalidate any ss link to it on close */
	new_wsi->for_ss = 1;

	// lwsl_wsi_notice(new_wsi, "%s: opaq %p, role %s",
	//			     new_wsi->a.opaque_user_data,
	//			     new_wsi->role_ops->name);

	h->policy = new_wsi->a.vhost->ss_handle->policy;

	/* apply requested socket options */
	if (lws_plat_set_socket_options_ip(new_wsi->desc.sockfd,
					   h->policy->priority,
		      (LCCSCF_IP_LOW_LATENCY *
		       !!(h->policy->flags & LWSSSPOLF_ATTR_LOW_LATENCY)) |
		      (LCCSCF_IP_HIGH_THROUGHPUT *
		       !!(h->policy->flags & LWSSSPOLF_ATTR_HIGH_THROUGHPUT)) |
		      (LCCSCF_IP_HIGH_RELIABILITY *
		       !!(h->policy->flags & LWSSSPOLF_ATTR_HIGH_RELIABILITY)) |
		      (LCCSCF_IP_LOW_COST *
		       !!(h->policy->flags & LWSSSPOLF_ATTR_LOW_COST))))
		lwsl_wsi_warn(new_wsi, "unable to set ip options");

	/*
	 * add us to the list of clients that came in from the server
	 */

	lws_pt_lock(pt, __func__);
	lws_dll2_add_tail(&h->cli_list, &new_wsi->a.vhost->ss_handle->src_list);
	lws_pt_unlock(pt);

	/*
	 * Let's give it appropriate state notifications
	 */

	if (lws_ss_event_helper(h, LWSSSCS_CREATING))
		goto fail;
	if (lws_ss_event_helper(h, LWSSSCS_CONNECTING))
		goto fail;

	/* defer CONNECTED until we see if he is upgrading */

//	if (lws_ss_event_helper(h, LWSSSCS_CONNECTED))
//		goto fail;

	// lwsl_notice("%s: accepted ss complete, pcol %s\n", __func__,
	//		new_wsi->a.protocol->name);

	return 0;

fail:
	lws_ss_destroy(&h);
fail1:
	return 1;
}

#endif


static struct lws *
lws_adopt_descriptor_vhost2(struct lws *new_wsi, lws_adoption_type type,
			    lws_sock_file_fd_type fd)
{
	struct lws_context_per_thread *pt =
			&new_wsi->a.context->pt[(int)new_wsi->tsi];
	int n;

	/* enforce that every fd is nonblocking */

	if (type & LWS_ADOPT_SOCKET) {
		if (lws_plat_set_nonblocking(fd.sockfd)) {
			lwsl_wsi_err(new_wsi, "unable to set sockfd %d nonblocking",
				     fd.sockfd);
			goto fail;
		}
	}
#if !defined(WIN32)
	else
		if (lws_plat_set_nonblocking(fd.filefd)) {
			lwsl_wsi_err(new_wsi, "unable to set filefd nonblocking");
			goto fail;
		}
#endif

	new_wsi->desc = fd;

	if (!LWS_SSL_ENABLED(new_wsi->a.vhost) ||
	    !(type & LWS_ADOPT_SOCKET))
		type &= (unsigned int)~LWS_ADOPT_ALLOW_SSL;

	/*
	 * A new connection was accepted. Give the user a chance to
	 * set properties of the newly created wsi. There's no protocol
	 * selected yet so we issue this to the vhosts's default protocol,
	 * itself by default protocols[0]
	 */
	new_wsi->wsistate |= LWSIFR_SERVER;
	n = LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED;
	if (new_wsi->role_ops->adoption_cb[lwsi_role_server(new_wsi)])
		n = new_wsi->role_ops->adoption_cb[lwsi_role_server(new_wsi)];

	if (new_wsi->a.context->event_loop_ops->sock_accept)
		if (new_wsi->a.context->event_loop_ops->sock_accept(new_wsi))
			goto fail;

#if LWS_MAX_SMP > 1
	/*
	 * Caution: after this point the wsi is live on its service thread
	 * which may be concurrent to this.  We mark the wsi as still undergoing
	 * init in another pt so the assigned pt leaves it alone.
	 */
	new_wsi->undergoing_init_from_other_pt = 1;
#endif

	if (!(type & LWS_ADOPT_ALLOW_SSL)) {
		lws_pt_lock(pt, __func__);
		if (__insert_wsi_socket_into_fds(new_wsi->a.context, new_wsi)) {
			lws_pt_unlock(pt);
			lwsl_wsi_err(new_wsi, "fail inserting socket");
			goto fail;
		}
		lws_pt_unlock(pt);
	}
#if defined(LWS_WITH_SERVER)
	 else
		if (lws_server_socket_service_ssl(new_wsi, fd.sockfd, 0)) {
			lwsl_wsi_info(new_wsi, "fail ssl negotiation");

			goto fail;
		}
#endif

	lws_vhost_lock(new_wsi->a.vhost);
	/* he has fds visibility now, remove from vhost orphan list */
	lws_dll2_remove(&new_wsi->vh_awaiting_socket);
	lws_vhost_unlock(new_wsi->a.vhost);

	/*
	 *  by deferring callback to this point, after insertion to fds,
	 * lws_callback_on_writable() can work from the callback
	 */
	if ((new_wsi->a.protocol->callback)(new_wsi, (enum lws_callback_reasons)n, new_wsi->user_space,
					  NULL, 0))
		goto fail;

	/* role may need to do something after all adoption completed */

	lws_role_call_adoption_bind(new_wsi, (int)type | _LWS_ADOPT_FINISH,
				    new_wsi->a.protocol->name);

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SECURE_STREAMS)
	/*
	 * Did we come from an accepted client connection to a ss server?
	 *
	 * !!! For mux protocols, this will cause an additional inactive ss
	 * representing the nwsi.  Doing that allows us to support both h1
	 * (here) and h2 (at __lws_wsi_server_new())
	 */

	lwsl_wsi_info(new_wsi, "vhost %s", new_wsi->a.vhost->lc.gutag);

	if (lws_adopt_ss_server_accept(new_wsi))
		goto fail;
#endif

#if LWS_MAX_SMP > 1
	/* its actual pt can service it now */

	new_wsi->undergoing_init_from_other_pt = 0;
#endif

	lws_cancel_service_pt(new_wsi);

	return new_wsi;

fail:
	if (type & LWS_ADOPT_SOCKET)
		lws_close_free_wsi(new_wsi, LWS_CLOSE_STATUS_NOSTATUS,
				   "adopt skt fail");

	return NULL;
}


/* if not a socket, it's a raw, non-ssl file descriptor */

struct lws *
lws_adopt_descriptor_vhost(struct lws_vhost *vh, lws_adoption_type type,
			   lws_sock_file_fd_type fd, const char *vh_prot_name,
			   struct lws *parent)
{
	lws_adopt_desc_t info;

	memset(&info, 0, sizeof(info));

	info.vh = vh;
	info.type = type;
	info.fd = fd;
	info.vh_prot_name = vh_prot_name;
	info.parent = parent;

	return lws_adopt_descriptor_vhost_via_info(&info);
}

struct lws *
lws_adopt_descriptor_vhost_via_info(const lws_adopt_desc_t *info)
{
	socklen_t slen = sizeof(lws_sockaddr46);
	struct lws *new_wsi;

#if defined(LWS_WITH_PEER_LIMITS)
	struct lws_peer *peer = NULL;

	if (info->type & LWS_ADOPT_SOCKET) {
		peer = lws_get_or_create_peer(info->vh, info->fd.sockfd);

		if (peer && info->vh->context->ip_limit_wsi &&
		    peer->count_wsi >= info->vh->context->ip_limit_wsi) {
			lwsl_info("Peer reached wsi limit %d\n",
					info->vh->context->ip_limit_wsi);
			if (info->vh->context->pl_notify_cb)
				info->vh->context->pl_notify_cb(
							info->vh->context,
							info->fd.sockfd,
							&peer->sa46);
			compatible_close(info->fd.sockfd);
			return NULL;
		}
	}
#endif

	lws_context_lock(info->vh->context, __func__);

	new_wsi = __lws_adopt_descriptor_vhost1(info->vh, info->type,
					      info->vh_prot_name, info->parent,
					      info->opaque, info->fi_wsi_name);
	if (!new_wsi) {
		if (info->type & LWS_ADOPT_SOCKET)
			compatible_close(info->fd.sockfd);
		goto bail;
	}

	if (info->type & LWS_ADOPT_SOCKET &&
	    getpeername(info->fd.sockfd, (struct sockaddr *)&new_wsi->sa46_peer,
								    &slen) < 0)
		lwsl_info("%s: getpeername failed\n", __func__);

#if defined(LWS_WITH_PEER_LIMITS)
	if (peer)
		lws_peer_add_wsi(info->vh->context, peer, new_wsi);
#endif

	new_wsi = lws_adopt_descriptor_vhost2(new_wsi, info->type, info->fd);

bail:
	lws_context_unlock(info->vh->context);

	return new_wsi;
}

struct lws *
lws_adopt_socket_vhost(struct lws_vhost *vh, lws_sockfd_type accept_fd)
{
	lws_sock_file_fd_type fd;

	fd.sockfd = accept_fd;
	return lws_adopt_descriptor_vhost(vh, LWS_ADOPT_SOCKET |
			LWS_ADOPT_HTTP | LWS_ADOPT_ALLOW_SSL, fd, NULL, NULL);
}

struct lws *
lws_adopt_socket(struct lws_context *context, lws_sockfd_type accept_fd)
{
	return lws_adopt_socket_vhost(context->vhost_list, accept_fd);
}

/* Common read-buffer adoption for lws_adopt_*_readbuf */
static struct lws*
adopt_socket_readbuf(struct lws *wsi, const char *readbuf, size_t len)
{
	struct lws_context_per_thread *pt;
	struct lws_pollfd *pfd;
	int n;

	if (!wsi)
		return NULL;

	if (!readbuf || len == 0)
		return wsi;

	if (wsi->position_in_fds_table == LWS_NO_FDS_POS)
		return wsi;

	pt = &wsi->a.context->pt[(int)wsi->tsi];

	n = lws_buflist_append_segment(&wsi->buflist, (const uint8_t *)readbuf,
				       len);
	if (n < 0)
		goto bail;
	if (n)
		lws_dll2_add_head(&wsi->dll_buflist, &pt->dll_buflist_owner);

	/*
	 * we can't process the initial read data until we can attach an ah.
	 *
	 * if one is available, get it and place the data in his ah rxbuf...
	 * wsi with ah that have pending rxbuf get auto-POLLIN service.
	 *
	 * no autoservice because we didn't get a chance to attach the
	 * readbuf data to wsi or ah yet, and we will do it next if we get
	 * the ah.
	 */
	if (wsi->http.ah || !lws_header_table_attach(wsi, 0)) {

		lwsl_notice("%s: calling service on readbuf ah\n", __func__);

		/*
		 * unlike a normal connect, we have the headers already
		 * (or the first part of them anyway).
		 * libuv won't come back and service us without a network
		 * event, so we need to do the header service right here.
		 */
		pfd = &pt->fds[wsi->position_in_fds_table];
		pfd->revents |= LWS_POLLIN;
		lwsl_err("%s: calling service\n", __func__);
		if (lws_service_fd_tsi(wsi->a.context, pfd, wsi->tsi))
			/* service closed us */
			return NULL;

		return wsi;
	}
	lwsl_err("%s: deferring handling ah\n", __func__);

	return wsi;

bail:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS,
			   "adopt skt readbuf fail");

	return NULL;
}

#if defined(LWS_WITH_UDP)
#if defined(LWS_WITH_CLIENT)

/*
 * This is the ASYNC_DNS callback target for udp client, it's analogous to
 * connect3()
 */

static struct lws *
lws_create_adopt_udp2(struct lws *wsi, const char *ads,
		      const struct addrinfo *r, int n, void *opaque)
{
	lws_sock_file_fd_type sock;
	int bc = 1, m;

	assert(wsi);

	if (ads && (n < 0 || !r)) {
		/*
		 * DNS lookup failed: there are no usable results.  Fail the
		 * overall connection request.
		 */
		lwsl_notice("%s: bad: n %d, r %p\n", __func__, n, r);

		goto bail;
	}

	m = lws_sort_dns(wsi, r);
#if defined(LWS_WITH_SYS_ASYNC_DNS)
	lws_async_dns_freeaddrinfo(&r);
#else
	freeaddrinfo((struct addrinfo *)r);
#endif
	if (m)
		goto bail;

	while (lws_dll2_get_head(&wsi->dns_sorted_list)) {
		lws_dns_sort_t *s = lws_container_of(
				lws_dll2_get_head(&wsi->dns_sorted_list),
				lws_dns_sort_t, list);

		/*
		 * Remove it from the head, but don't free it yet... we are
		 * taking responsibility to free it
		 */
		lws_dll2_remove(&s->list);

		/*
		 * We have done the dns lookup, identify the result we want
		 * if any, and then complete the adoption by binding wsi to
		 * socket opened on it.
		 *
		 * Ignore the weak assumptions about protocol driven by port
		 * number and force to DGRAM / UDP since that's what this
		 * function is for.
		 */

#if !defined(__linux__)
		sock.sockfd = socket(s->dest.sa4.sin_family,
				     SOCK_DGRAM, IPPROTO_UDP);
#else
		/* PF_PACKET is linux-only */
		sock.sockfd = socket(wsi->pf_packet ? PF_PACKET :
						s->dest.sa4.sin_family,
				     SOCK_DGRAM, wsi->pf_packet ?
					htons(0x800) : IPPROTO_UDP);
#endif
		if (sock.sockfd == LWS_SOCK_INVALID)
			goto resume;

		/* ipv6 udp!!! */

		if (s->af == AF_INET)
			s->dest.sa4.sin_port = htons(wsi->c_port);
#if defined(LWS_WITH_IPV6)
		else
			s->dest.sa6.sin6_port = htons(wsi->c_port);
#endif

		if (setsockopt(sock.sockfd, SOL_SOCKET, SO_REUSEADDR,
			       (const char *)&bc, sizeof(bc)) < 0)
			lwsl_err("%s: failed to set reuse\n", __func__);

		if (wsi->do_broadcast &&
		    setsockopt(sock.sockfd, SOL_SOCKET, SO_BROADCAST,
			       (const char *)&bc, sizeof(bc)) < 0)
			lwsl_err("%s: failed to set broadcast\n", __func__);

		/* Bind the udp socket to a particular network interface */

		if (opaque &&
		    lws_plat_BINDTODEVICE(sock.sockfd, (const char *)opaque))
			goto resume;

		if (wsi->do_bind &&
		    bind(sock.sockfd, sa46_sockaddr(&s->dest),
#if defined(_WIN32)
			 (int)sa46_socklen(&s->dest)
#else
			 sizeof(struct sockaddr)
#endif
		) == -1) {
			lwsl_err("%s: bind failed\n", __func__);
			goto resume;
		}

		if (!wsi->do_bind && !wsi->pf_packet) {
#if !defined(__APPLE__)
			if (connect(sock.sockfd, sa46_sockaddr(&s->dest),
				    sa46_socklen(&s->dest)) == -1 &&
			    errno != EADDRNOTAVAIL /* openbsd */ ) {
				lwsl_err("%s: conn fd %d fam %d %s:%u failed "
					 "errno %d\n", __func__, sock.sockfd,
					 s->dest.sa4.sin_family,
					 ads ? ads : "null", wsi->c_port,
					 LWS_ERRNO);
				compatible_close(sock.sockfd);
				goto resume;
			}
#endif
		}

		if (wsi->udp)
			wsi->udp->sa46 = s->dest;
		wsi->sa46_peer = s->dest;

		/* we connected: complete the udp socket adoption flow */

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	if (wsi->a.context->async_dns.wsi == wsi)
		wsi->a.context->async_dns.dns_server_connected = 1;
#endif

		lws_free(s);
		lws_addrinfo_clean(wsi);
		return lws_adopt_descriptor_vhost2(wsi,
						LWS_ADOPT_RAW_SOCKET_UDP, sock);

resume:
		lws_free(s);
	}

	lwsl_err("%s: unable to create INET socket %d\n", __func__, LWS_ERRNO);
	lws_addrinfo_clean(wsi);

#if defined(LWS_WITH_SYS_ASYNC_DNS)
	if (wsi->a.context->async_dns.wsi == wsi)
		lws_async_dns_drop_server(wsi->a.context);
#endif

bail:

	/* caller must close */

	return NULL;
}

struct lws *
lws_create_adopt_udp(struct lws_vhost *vhost, const char *ads, int port,
		     int flags, const char *protocol_name, const char *ifname,
		     struct lws *parent_wsi, void *opaque,
		     const lws_retry_bo_t *retry_policy, const char *fi_wsi_name)
{
#if !defined(LWS_PLAT_OPTEE)
	struct lws *wsi;
	int n;

	lwsl_info("%s: %s:%u\n", __func__, ads ? ads : "null", port);

	/* create the logical wsi without any valid fd */

	lws_context_lock(vhost->context, __func__);

	wsi = __lws_adopt_descriptor_vhost1(vhost, LWS_ADOPT_SOCKET |
						 LWS_ADOPT_RAW_SOCKET_UDP,
					  protocol_name, parent_wsi, opaque,
					  fi_wsi_name);

	lws_context_unlock(vhost->context);
	if (!wsi) {
		lwsl_err("%s: udp wsi creation failed\n", __func__);
		goto bail;
	}

	// lwsl_notice("%s: role %s\n", __func__, wsi->role_ops->name);

	wsi->do_bind = !!(flags & LWS_CAUDP_BIND);
	wsi->do_broadcast = !!(flags & LWS_CAUDP_BROADCAST);
	wsi->pf_packet = !!(flags & LWS_CAUDP_PF_PACKET);
	wsi->c_port = (uint16_t)(unsigned int)port;
	if (retry_policy)
		wsi->retry_policy = retry_policy;
	else
		wsi->retry_policy = vhost->retry_policy;

#if !defined(LWS_WITH_SYS_ASYNC_DNS)
	{
		struct addrinfo *r, h;
		char buf[16];

		memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
		h.ai_socktype = SOCK_DGRAM;
		h.ai_protocol = IPPROTO_UDP;
#if defined(AI_PASSIVE)
		h.ai_flags = AI_PASSIVE;
#endif
#ifdef AI_ADDRCONFIG
		h.ai_flags |= AI_ADDRCONFIG;
#endif

		/* if the dns lookup is synchronous, do the whole thing now */
		lws_snprintf(buf, sizeof(buf), "%u", port);
		n = getaddrinfo(ads, buf, &h, &r);
		if (n) {

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#if !defined(LWS_PLAT_FREERTOS)
			lwsl_info("%s: getaddrinfo error: %s\n", __func__,
				  gai_strerror(n));
#else

			lwsl_info("%s: getaddrinfo error: %s\n", __func__,
					strerror(n));
#endif
#endif
			//freeaddrinfo(r);
			goto bail1;
		}
		/*
		 * With synchronous dns, complete it immediately after the
		 * blocking dns lookup finished... free r when connect either
		 * completed or failed
		 */
		wsi = lws_create_adopt_udp2(wsi, ads, r, 0, NULL);

		return wsi;
	}
#else
	if (ads) {
		/*
		 * with async dns, use the wsi as the point about which to do
		 * the dns lookup and have it call the second part when it's
		 * done.
		 *
		 * Keep a refcount on the results and free it when we connected
		 * or definitively failed.
		 *
		 * Notice wsi has no socket at this point (we don't know what
		 * kind to ask for until we get the dns back).  But it is bound
		 * to a vhost and can be cleaned up from that at vhost destroy.
		 */
		n = lws_async_dns_query(vhost->context, 0, ads,
					LWS_ADNS_RECORD_A,
					lws_create_adopt_udp2, wsi,
					(void *)ifname);
		// lwsl_notice("%s: dns query returned %d\n", __func__, n);
		if (n == LADNS_RET_FAILED) {
			lwsl_err("%s: async dns failed\n", __func__);
			wsi = NULL;
			/*
			 * It was already closed by calling callback with error
			 * from lws_async_dns_query()
			 */
			goto bail;
		}
	} else {
		lwsl_debug("%s: udp adopt has no ads\n", __func__);
		wsi = lws_create_adopt_udp2(wsi, ads, NULL, 0, (void *)ifname);
	}

	/* dns lookup is happening asynchronously */

	// lwsl_notice("%s: returning wsi %p\n", __func__, wsi);

	return wsi;
#endif
#if !defined(LWS_WITH_SYS_ASYNC_DNS)
bail1:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "adopt udp2 fail");
	wsi = NULL;
#endif
bail:
	return wsi;
#else
	return NULL;
#endif
}
#endif
#endif

struct lws *
lws_adopt_socket_readbuf(struct lws_context *context, lws_sockfd_type accept_fd,
			 const char *readbuf, size_t len)
{
        return adopt_socket_readbuf(lws_adopt_socket(context, accept_fd),
				    readbuf, len);
}

struct lws *
lws_adopt_socket_vhost_readbuf(struct lws_vhost *vhost,
			       lws_sockfd_type accept_fd,
			       const char *readbuf, size_t len)
{
        return adopt_socket_readbuf(lws_adopt_socket_vhost(vhost, accept_fd),
				    readbuf, len);
}
