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
 *
 * We mainly focus on the routing table / gateways because those are the
 * elements that decide if we can get on to the internet or not.
 *
 * We also need to understand the source addresses of possible outgoing routes,
 * and follow LINK down (ifconfig down) to clean up routes on the interface idx
 * going down that are not otherwise cleaned.
 */

#include <private-lib-core.h>

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

/* work around CentOS 7 -Wconversion problem */
#undef RTA_ALIGNTO
#define RTA_ALIGNTO 4U

//#define lwsl_netlink lwsl_notice
#define lwsl_cx_netlink lwsl_cx_info

static void
lws_netlink_coldplug_done_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context *ctx = lws_container_of(sul, struct lws_context,
						   sul_nl_coldplug);
	ctx->nl_initial_done = 1;
#if defined(LWS_WITH_SYS_STATE)
	/* if nothing is there to intercept anything, go all the way */
	lws_state_transition_steps(&ctx->mgr_system, LWS_SYSTATE_OPERATIONAL);
#endif
}

static int
rops_handle_POLLIN_netlink(struct lws_context_per_thread *pt, struct lws *wsi,
			   struct lws_pollfd *pollfd)
{
	struct lws_context	*cx = pt->context;
	uint8_t s[4096]
#if defined(_DEBUG)
	        , route_change = 0
#endif
#if defined(LWS_WITH_SYS_SMD)
		, gateway_change = 0
#endif
			;
	struct sockaddr_nl	nladdr;
	lws_route_t		robj, *rou, *rmat;
	struct nlmsghdr		*h;
	struct msghdr		msg;
	struct iovec		iov;
	unsigned int		n;
	char			buf[72];

	if (!(pollfd->revents & LWS_POLLIN))
		return LWS_HPI_RET_HANDLED;

	memset(&msg, 0, sizeof(msg));

	iov.iov_base		= (void *)s;
	iov.iov_len		= sizeof(s);

	msg.msg_name		= (void *)&(nladdr);
	msg.msg_namelen		= sizeof(nladdr);

	msg.msg_iov		= &iov;
	msg.msg_iovlen		= 1;

	n = (unsigned int)recvmsg(wsi->desc.sockfd, &msg, 0);
	if ((int)n < 0) {
		lwsl_cx_notice(cx, "recvmsg failed");
		return LWS_HPI_RET_PLEASE_CLOSE_ME;
	}

	// lwsl_hexdump_notice(s, (size_t)n);

	h = (struct nlmsghdr *)s;

	/* we can get a bunch of messages coalesced in one read*/

	for ( ; NLMSG_OK(h, n); h = NLMSG_NEXT(h, n)) {
		struct ifaddrmsg *ifam;
		struct rtattr *ra;
		struct rtmsg *rm;
#if !defined(LWS_WITH_NO_LOGS) && defined(_DEBUG)
		struct ndmsg *nd;
#endif
		unsigned int ra_len;
		uint8_t *p;

		struct ifinfomsg *ifi;
		struct rtattr *attribute;
		unsigned int len;

		lwsl_cx_netlink(cx, "RTM %d", h->nlmsg_type);

		memset(&robj, 0, sizeof(robj));
		robj.if_idx = -1;
		robj.priority = -1;
		rm = (struct rtmsg *)NLMSG_DATA(h);

		/*
		 * We have to care about NEWLINK so we can understand when a
		 * network interface went down, and clear the related routes.
		 *
		 * We don't get individual DELROUTEs for these.
		 */

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:

			ifi = NLMSG_DATA(h);
			len = (unsigned int)(h->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi)));

			/* loop over all attributes for the NEWLINK message */
			for (attribute = IFLA_RTA(ifi); RTA_OK(attribute, len);
					 attribute = RTA_NEXT(attribute, len)) {
				lwsl_cx_netlink(cx, "if attr %d",
					    (int)attribute->rta_type);
				switch(attribute->rta_type) {
				case IFLA_IFNAME:
					lwsl_cx_netlink(cx, "NETLINK ifidx %d : %s",
						     ifi->ifi_index,
						     (char *)RTA_DATA(attribute));
					break;
				default:
					break;
				} /* switch */
			} /* for loop */

			lwsl_cx_netlink(cx, "NEWLINK ifi_index %d, flags 0x%x",
					ifi->ifi_index, ifi->ifi_flags);

			/*
			 * Despite "New"link this is actually telling us there
			 * is some change on the network interface IFF_ state
			 */

			if (!(ifi->ifi_flags & IFF_UP)) {
				/*
				 * Interface is down, so scrub all routes that
				 * applied to it
				 */
				lwsl_cx_netlink(cx, "NEWLINK: ifdown %d",
						ifi->ifi_index);
				lws_pt_lock(pt, __func__);
				_lws_route_table_ifdown(pt, ifi->ifi_index);
				lws_pt_unlock(pt);
			}
			continue; /* ie, not break, no second half */

		case RTM_NEWADDR:
		case RTM_DELADDR:

			ifam = (struct ifaddrmsg *)NLMSG_DATA(h);

			robj.source_ads = 1;
			robj.dest_len = ifam->ifa_prefixlen;
			robj.if_idx = (int)ifam->ifa_index;
			robj.scope = ifam->ifa_scope;
			robj.ifa_flags = ifam->ifa_flags;
			robj.dest.sa4.sin_family = ifam->ifa_family;

			/* address attributes */
			ra = (struct rtattr *)IFA_RTA(ifam);
			ra_len = (unsigned int)IFA_PAYLOAD(h);

			lwsl_cx_netlink(cx, "%s",
				     h->nlmsg_type == RTM_NEWADDR ?
						     "NEWADDR" : "DELADDR");

			/*
			 * almost nothing interesting within IFA_* attributes:
			 * so skip it and goto to the second half
			 */
			goto second_half;

		case RTM_NEWROUTE:
		case RTM_DELROUTE:

			lwsl_cx_netlink(cx, "%s",
				     h->nlmsg_type == RTM_NEWROUTE ?
						     "NEWROUTE" : "DELROUTE");

			/* route attributes */
			ra = (struct rtattr *)RTM_RTA(rm);
			ra_len = (unsigned int)RTM_PAYLOAD(h);
			break;

		case RTM_DELNEIGH:
		case RTM_NEWNEIGH:
			lwsl_cx_netlink(cx, "%s", h->nlmsg_type ==
						RTM_NEWNEIGH ? "NEWNEIGH" :
							       "DELNEIGH");
#if !defined(LWS_WITH_NO_LOGS) && defined(_DEBUG)
			nd = (struct ndmsg *)rm;
			lwsl_cx_netlink(cx, "fam %u, ifidx %u, flags 0x%x",
				    nd->ndm_family, nd->ndm_ifindex,
				    nd->ndm_flags);
#endif
			ra = (struct rtattr *)RTM_RTA(rm);
			ra_len = (unsigned int)RTM_PAYLOAD(h);
			for ( ; RTA_OK(ra, ra_len); ra = RTA_NEXT(ra, ra_len)) {
				lwsl_cx_netlink(cx, "atr %d", ra->rta_type);
				switch (ra->rta_type) {
				case NDA_DST:
					lwsl_cx_netlink(cx, "dst len %d",
							ra->rta_len);
					break;
				}
			}
			lws_pt_lock(pt, __func__);
			_lws_route_pt_close_unroutable(pt);
			lws_pt_unlock(pt);
			continue;

		default:
			lwsl_cx_netlink(cx, "*** Unknown RTM_%d",
					h->nlmsg_type);
			continue;
		} /* switch */

		robj.proto = rm->rtm_protocol;

		// iterate over route attributes
		for ( ; RTA_OK(ra, ra_len); ra = RTA_NEXT(ra, ra_len)) {
			// lwsl_netlink("%s: atr %d\n", __func__, ra->rta_type);
			switch (ra->rta_type) {
			case RTA_PREFSRC: /* protocol ads: preferred src ads */
			case RTA_SRC:
				lws_sa46_copy_address(&robj.src, RTA_DATA(ra),
							rm->rtm_family);
				robj.src_len = rm->rtm_src_len;
				lws_sa46_write_numeric_address(&robj.src, buf, sizeof(buf));
				lwsl_cx_netlink(cx, "RTA_SRC: %s", buf);
				break;
			case RTA_DST:
				/* check if is local addr -> considering it as src addr too */
				if (rm->rtm_type == RTN_LOCAL &&
				    ((rm->rtm_family == AF_INET && rm->rtm_dst_len == 32) ||
				     (rm->rtm_family == AF_INET6 && rm->rtm_dst_len == 128))) {
					lws_sa46_copy_address(&robj.src, RTA_DATA(ra),
							rm->rtm_family);
					lwsl_cx_netlink(cx, "Local addr: RTA_DST -> added to RTA_SRC");
				}

				lws_sa46_copy_address(&robj.dest, RTA_DATA(ra),
							rm->rtm_family);
				robj.dest_len = rm->rtm_dst_len;
				lws_sa46_write_numeric_address(&robj.dest, buf, sizeof(buf));
				lwsl_cx_netlink(cx, "RTA_DST: %s", buf);
				break;
			case RTA_GATEWAY:
				lws_sa46_copy_address(&robj.gateway,
						      RTA_DATA(ra),
						      rm->rtm_family);
#if defined(LWS_WITH_SYS_SMD)
				gateway_change = 1;
#endif
				break;
			case RTA_IIF: /* int: input interface index */
			case RTA_OIF: /* int: output interface index */
				robj.if_idx = *(int *)RTA_DATA(ra);
				lwsl_cx_netlink(cx, "ifidx %d", robj.if_idx);
				break;
			case RTA_PRIORITY: /* int: priority of route */
				p = RTA_DATA(ra);
				robj.priority = p[3] << 24 | p[2] << 16 |
						 p[1] << 8  | p[0];
				break;
			case RTA_CACHEINFO: /* struct rta_cacheinfo */
				break;
#if defined(LWS_HAVE_RTA_PREF)
			case RTA_PREF: /* char: RFC4191 v6 router preference */
				break;
#endif
			case RTA_TABLE: /* int */
				break;

			default:
				lwsl_cx_info(cx, "unknown attr type %d",
					     ra->rta_type);
				break;
			}
		} /* for */

		/*
		 * the second half, once all the attributes were collected
		 */
second_half:
		switch (h->nlmsg_type) {

		case RTM_DELROUTE:
			/*
			 * This will also take down wsi marked as using it
			 */
			lwsl_cx_netlink(cx, "DELROUTE: if_idx %d",
					robj.if_idx);
			lws_pt_lock(pt, __func__);
			_lws_route_remove(pt, &robj, 0);
			lws_pt_unlock(pt);
			goto inform;

		case RTM_NEWROUTE:

			lwsl_cx_netlink(cx, "NEWROUTE rtm_type %d",
					rm->rtm_type);

			/*
			 * We don't want any routing debris like /32 or broadcast
			 * in our routing table... we will collect source addresses
			 * bound to interfaces via NEWADDR
			 */

			if (rm->rtm_type != RTN_UNICAST &&
			    rm->rtm_type != RTN_LOCAL)
				break;

			if (rm->rtm_flags & RTM_F_CLONED)
				break;

			goto ana;

		case RTM_DELADDR:
			lwsl_cx_notice(cx, "DELADDR");
#if defined(_DEBUG)
			_lws_routing_entry_dump(cx, &robj);
#endif
			lws_pt_lock(pt, __func__);
			_lws_route_remove(pt, &robj, LRR_MATCH_SRC | LRR_IGNORE_PRI);
			_lws_route_pt_close_unroutable(pt);
			lws_pt_unlock(pt);
			break;

		case RTM_NEWADDR:

			lwsl_cx_netlink(cx, "NEWADDR");
ana:

			/*
			 * Is robj a dupe in the routing table already?
			 *
			 * match on pri ignore == set pri and skip
			 * no match == add
			 */

			lws_pt_lock(pt, __func__);

			/* returns zero on match already in table */
			rmat = _lws_route_remove(pt, &robj, h->nlmsg_type == RTM_NEWROUTE ?
					LRR_MATCH_DST : LRR_MATCH_SRC | LRR_IGNORE_PRI);
			lws_pt_unlock(pt);

			if (rmat) {
				rmat->priority = robj.priority;
				break;
			}

			rou = lws_malloc(sizeof(*rou), __func__);
			if (!rou) {
				lwsl_cx_err(cx, "oom");
				return LWS_HPI_RET_HANDLED;
			}

			*rou = robj;

			lws_pt_lock(pt, __func__);

			/*
			 * We lock the pt before getting the uidx, so it
			 * cannot race
			 */

			rou->uidx = _lws_route_get_uidx(cx);
			lws_dll2_add_tail(&rou->list, &cx->routing_table);
			lwsl_cx_info(cx, "route list size %u",
					cx->routing_table.count);

			_lws_route_pt_close_unroutable(pt);

			lws_pt_unlock(pt);

inform:
#if defined(_DEBUG)
			route_change = 1;
#endif
#if defined(LWS_WITH_SYS_SMD)
			/*
			 * Reflect the route add / del event using SMD.
			 * Participants interested can refer to the pt
			 * routing table
			 */
			(void)lws_smd_msg_printf(cx, LWSSMDCL_NETWORK,
				   "{\"rt\":\"%s\"}\n",
				   (h->nlmsg_type == RTM_DELROUTE) ?
						"del" : "add");
#endif

			break;

		default:
			// lwsl_info("%s: unknown msg type %d\n", __func__,
			//		h->nlmsg_type);
			break;
		}
	} /* message iterator */

#if defined(LWS_WITH_SYS_SMD)
	if (gateway_change)
		/*
		 * If a route with a gw was added or deleted, retrigger captive
		 * portal detection if we have that
		 */
		(void)lws_smd_msg_printf(cx, LWSSMDCL_NETWORK,
				   "{\"trigger\": \"cpdcheck\", "
				   "\"src\":\"gw-change\"}");
#endif

#if defined(_DEBUG)
	if (route_change) {
		lws_context_lock(cx, __func__);
		_lws_routing_table_dump(cx);
		lws_context_unlock(cx);
	}
#endif

	if (!cx->nl_initial_done &&
	    pt == &cx->pt[0] &&
	    cx->routing_table.count) {
		/*
		 * While netlink info still coming, keep moving the timer for
		 * calling it "done" to +100ms until after it stops coming
		 */
		lws_context_lock(cx, __func__);
		lws_sul_schedule(cx, 0, &cx->sul_nl_coldplug,
				 lws_netlink_coldplug_done_cb,
				 100 * LWS_US_PER_MS);
		lws_context_unlock(cx);
	}

	return LWS_HPI_RET_HANDLED;
}

struct nl_req_s {
	struct nlmsghdr hdr;
	struct rtmsg gen;
};

int
rops_pt_init_destroy_netlink(struct lws_context *context,
			     const struct lws_context_creation_info *info,
			     struct lws_context_per_thread *pt, int destroy)
{
	struct sockaddr_nl sanl;
	struct nl_req_s req;
	struct msghdr msg;
	struct iovec iov;
	struct lws *wsi;
	int n, ret = 1;

	if (destroy) {

		/*
		 * pt netlink wsi closed + freed as part of pt's destroy
		 * wsi mass close, just need to take down the routing table
		 */
		_lws_route_table_empty(pt);

		return 0;
	}

	if (context->netlink)
		return 0;

	if (pt > &context->pt[0])
		/* we can only have one netlink socket */
		return 0;

	lwsl_cx_info(context, "creating netlink skt");

	/*
	 * We want a netlink socket per pt as well
	 */

	lws_context_lock(context, __func__);
	wsi = __lws_wsi_create_with_role(context, (int)(pt - &context->pt[0]),
				       &role_ops_netlink, NULL);
	lws_context_unlock(context);
	if (!wsi)
		goto bail;

	wsi->desc.sockfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (wsi->desc.sockfd == LWS_SOCK_INVALID) {
		lwsl_cx_err(context, "unable to open netlink");
		goto bail1;
	}

	lws_plat_set_nonblocking(wsi->desc.sockfd);

	__lws_lc_tag(context, &context->lcg[LWSLCG_VHOST], &wsi->lc,
			"netlink");

	memset(&sanl, 0, sizeof(sanl));
	sanl.nl_family		= AF_NETLINK;
	sanl.nl_pid		= (uint32_t)getpid();
	sanl.nl_groups		= RTMGRP_LINK | RTMGRP_IPV4_ROUTE | RTMGRP_IPV4_IFADDR
#if defined(LWS_WITH_IPV6)
				  | RTMGRP_IPV6_ROUTE | RTMGRP_IPV6_IFADDR
#endif
				 ;

	if (lws_fi(&context->fic, "netlink_bind") ||
	    bind(wsi->desc.sockfd, (struct sockaddr*)&sanl, sizeof(sanl)) < 0) {
		lwsl_cx_warn(context, "netlink bind failed");
		ret = 0; /* some systems deny access, just ignore */
		goto bail2;
	}

	context->netlink = wsi;
	if (lws_wsi_inject_to_loop(pt, wsi))
		goto bail2;

/*	if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
		lwsl_err("%s: pollfd in fail\n", __func__);
		goto bail2;
	}
*/
	/*
	 * Since we're starting the PT, ask to be sent all the existing routes.
	 *
	 * This requires CAP_ADMIN, or root... we do this early before dropping
	 * privs
	 */

	memset(&sanl, 0, sizeof(sanl));
	memset(&msg, 0, sizeof(msg));
	memset(&req, 0, sizeof(req));

	sanl.nl_family		= AF_NETLINK;

	req.hdr.nlmsg_len	= NLMSG_LENGTH(sizeof(req.gen));
	req.hdr.nlmsg_type	= RTM_GETROUTE;
	req.hdr.nlmsg_flags	= NLM_F_REQUEST | NLM_F_DUMP;
	req.hdr.nlmsg_seq	= 1;
	req.hdr.nlmsg_pid	= (uint32_t)getpid();
	req.gen.rtm_family	= AF_PACKET;
	req.gen.rtm_table	= RT_TABLE_DEFAULT;

	iov.iov_base		= &req;
	iov.iov_len		= req.hdr.nlmsg_len;
	msg.msg_iov		= &iov;
	msg.msg_iovlen		= 1;
	msg.msg_name		= &sanl;
	msg.msg_namelen		= sizeof(sanl);

	n = (int)sendmsg(wsi->desc.sockfd, (struct msghdr *)&msg, 0);
	if (n < 0) {
		lwsl_cx_notice(context, "rt dump req failed... permissions? errno %d",
				LWS_ERRNO);
	}

	/*
	 * Responses are going to come asynchronously, let's block moving
	 * off state IFACE_COLDPLUG until we have had them.  This is important
	 * since if we don't hold there, when we do get the responses we may
	 * cull any ongoing connections as unroutable otherwise
	 */

	lwsl_cx_debug(context, "starting netlink coldplug wait");

	return 0;

bail2:
	__lws_lc_untag(wsi->a.context, &wsi->lc);
	compatible_close(wsi->desc.sockfd);
bail1:
	lws_free(wsi);
bail:
	return ret;
}

static const lws_rops_t rops_table_netlink[] = {
	/*  1 */ { .pt_init_destroy	= rops_pt_init_destroy_netlink },
	/*  2 */ { .handle_POLLIN	= rops_handle_POLLIN_netlink },
};

const struct lws_role_ops role_ops_netlink = {
	/* role name */			"netlink",
	/* alpn id */			NULL,

	/* rops_table */		rops_table_netlink,
	/* rops_idx */			{
	  /* LWS_ROPS_check_upgrades */
	  /* LWS_ROPS_pt_init_destroy */		0x01,
	  /* LWS_ROPS_init_vhost */
	  /* LWS_ROPS_destroy_vhost */			0x00,
	  /* LWS_ROPS_service_flag_pending */
	  /* LWS_ROPS_handle_POLLIN */			0x02,
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
	/* file_handle */		0,
};
