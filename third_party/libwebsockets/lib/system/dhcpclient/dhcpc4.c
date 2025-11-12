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
 * The protocol part of dhcp4 client
 */

#include "private-lib-core.h"
#include "private-lib-system-dhcpclient.h"

#define LDHC_OP_BOOTREQUEST 1
#define LDHC_OP_BOOTREPLY 2

/*
 *  IPv4... max total 576
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     op (1)    |   htype (1)   |   hlen (1)    |   hops (1)    |
 * +---------------+---------------+---------------+---------------+
 * |  +04                       xid (4)                            |
 * +-------------------------------+-------------------------------+
 * |  +08      secs (2)            |  +0a         flags (2)        |
 * +-------------------------------+-------------------------------+
 * |  +0C                     ciaddr  (4)      client IP           |
 * +---------------------------------------------------------------+
 * |  +10                     yiaddr  (4)      your IP             |
 * +---------------------------------------------------------------+
 * |  +14                     siaddr  (4)      server IP           |
 * +---------------------------------------------------------------+
 * |  +18                     giaddr  (4)      gateway IP          |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |  +1C                     chaddr  (16)     client HWADDR       |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |  +2C                     sname   (64)                         |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |  +6C                     file    (128)                        |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |  +EC                     options (variable)                   |
 * +---------------------------------------------------------------+
 */

static const uint8_t rawdisc4[] = {
	0x45, 0x00, 0, 0, 0, 0, 0x40, 0, 0x2e, IPPROTO_UDP,
	0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
	0, 68, 0, 67, 0, 0, 0, 0
};

static const uint32_t botable2[] = { 1500, 1750, 5000 /* in case dog slow */ };
static const lws_retry_bo_t bo2 = {
	botable2, LWS_ARRAY_SIZE(botable2), LWS_RETRY_CONCEAL_ALWAYS, 0, 0, 20 };

static int
lws_dhcpc4_prep(uint8_t *start, unsigned int bufsiz, lws_dhcpc_req_t *r, int op)
{
	uint8_t *p = start;

	memset(start, 0, bufsiz);

	*p++ = 1;
	*p++ = 1;
	*p++ = 6; /* sizeof ethernet MAC */

	memcpy(p + 1, r->xid, 4);

//	p[7] = 0x80; /* broadcast flag */

	p += 0x1c - 3;

	if (lws_plat_ifname_to_hwaddr(r->wsi_raw->desc.sockfd,
				      (const char *)&r[1], r->is.mac, 6) < 0)
		return -1;

	memcpy(p, r->is.mac, 6);

	p += 16 + 64 + 128;

	*p++ = 0x63; /* RFC2132 Magic Cookie indicates start of options */
	*p++ = 0x82;
	*p++ = 0x53;
	*p++ = 0x63;

	*p++ = LWSDHC4POPT_MESSAGE_TYPE;
	*p++ = 1;	/* length */
	*p++ = (uint8_t)op;

	switch (op) {
	case LWSDHC4PDISCOVER:
		*p++ = LWSDHC4POPT_PARAM_REQ_LIST;
		*p++ = 4; 	/* length */
		*p++ = LWSDHC4POPT_SUBNET_MASK;
		*p++ = LWSDHC4POPT_ROUTER;
		*p++ = LWSDHC4POPT_DNSERVER;
		*p++ = LWSDHC4POPT_DOMAIN_NAME;
		break;

	case LWSDHC4PREQUEST:
		if (r->is.sa46[LWSDH_SA46_IP].sa4.sin_family != AF_INET)
			break;
		*p++ = LWSDHC4POPT_REQUESTED_ADS;
		*p++ = 4; 	/* length */
		lws_ser_wu32be(p, r->is.sa46[LWSDH_SA46_IP].sa4.sin_addr.s_addr);
		p += 4;
		*p++ = LWSDHC4POPT_SERVER_ID;
		*p++ = 4; 	/* length */
		lws_ser_wu32be(p, r->is.sa46[LWSDH_SA46_DHCP_SERVER].sa4.sin_addr.s_addr);
		p += 4;
		break;
	}

	*p++ = LWSDHC4POPT_END_OPTIONS;

	return lws_ptr_diff(p, start);
}

static int
callback_dhcpc4(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	       void *in, size_t len)
{
	lws_dhcpc_req_t *r = (lws_dhcpc_req_t *)user;
	uint8_t pkt[LWS_PRE + 576], *p = pkt + LWS_PRE;
	int n, m;

	switch (reason) {

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_debug("%s: LWS_CALLBACK_RAW_ADOPT\n", __func__);
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("%s: udp conn failed\n", __func__);

		/* fallthru */
	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_debug("%s: LWS_CALLBACK_RAW_CLOSE\n", __func__);
		if (!r)
			break;
		r->wsi_raw = NULL;
		lws_sul_cancel(&r->sul_write);
		if (r->state != LDHC_BOUND) {
			r->state = LDHC_INIT;
			lws_retry_sul_schedule(r->context, 0, &r->sul_conn,
					       &bo2, lws_dhcpc4_retry_conn,
					       &r->retry_count_conn);
		}
		break;

	case LWS_CALLBACK_RAW_RX:

		if (lws_dhcpc4_parse(r, in, len))
			break;

		/*
		 * that's it... commit to the configuration
		 */

		/* set up our network interface as offered */

		if (lws_plat_ifconfig(r->wsi_raw->desc.sockfd, &r->is))
			/*
			 * Problem setting the IP... maybe something
			 * transient like racing with NetworkManager?
			 * Since the sul retries are still around it
			 * will retry
			 */
			return -1;

		/* clear timeouts related to the broadcast socket */

		lws_sul_cancel(&r->sul_write);
		lws_sul_cancel(&r->sul_conn);

		lwsl_notice("%s: DHCP configured %s\n", __func__,
				(const char *)&r[1]);
		r->state = LDHC_BOUND;

		lws_state_transition_steps(&wsi->a.context->mgr_system,
					   LWS_SYSTATE_OPERATIONAL);

		r->cb(r->opaque, &r->is);

		r->wsi_raw = NULL;

		return -1; /* close the broadcast wsi */

	case LWS_CALLBACK_RAW_WRITEABLE:

		if (!r)
			break;

		/*
		 * UDP is not reliable, it can be locally dropped, or dropped
		 * by any intermediary or the remote peer.  So even though we
		 * will do the write in a moment, we schedule another request
		 * for rewrite according to the wsi retry policy.
		 *
		 * If the result came before, we'll cancel it in the close flow.
		 *
		 * If we have already reached the end of our concealed retries
		 * in the policy, just close without another write.
		 */
		if (lws_dll2_is_detached(&r->sul_write.list) &&
		    lws_retry_sul_schedule_retry_wsi(wsi, &r->sul_write,
						     lws_dhcpc_retry_write,
						     &r->retry_count_write)) {
			/* we have reached the end of our concealed retries */
			lwsl_warn("%s: concealed retries done, failing\n",
				  __func__);
			goto retry_conn;
		}

		switch (r->state) {
		case LDHC_INIT:
			n = LWSDHC4PDISCOVER;
			goto bcast;

		case LDHC_REQUESTING:
			n = LWSDHC4PREQUEST;

			/* fallthru */
bcast:
			n = lws_dhcpc4_prep(p + 28, (unsigned int)
					(sizeof(pkt) - LWS_PRE - 28), r, n);
			if (n < 0) {
				lwsl_err("%s: failed to prep\n", __func__);
				break;
			}

			m = lws_plat_rawudp_broadcast(p, rawdisc4,
						      LWS_ARRAY_SIZE(rawdisc4),
						      (size_t)(n + 28),
						      r->wsi_raw->desc.sockfd,
						      (const char *)&r[1]);
			if (m < 0)
				lwsl_err("%s: Failed to write dhcp client req: "
					 "%d %d, errno %d\n", __func__,
					 n, m, LWS_ERRNO);
			break;
		default:
			break;
		}

		return 0;

retry_conn:
		lws_retry_sul_schedule(wsi->a.context, 0, &r->sul_conn, &bo2,
				       lws_dhcpc4_retry_conn,
				       &r->retry_count_conn);

		return -1;

	default:
		break;
	}

	return 0;
}

struct lws_protocols lws_system_protocol_dhcpc4 =
	{ "lws-dhcp4client", callback_dhcpc4, 0, 128, 0, NULL, 0 };

void
lws_dhcpc4_retry_conn(struct lws_sorted_usec_list *sul)
{
	lws_dhcpc_req_t *r = lws_container_of(sul, lws_dhcpc_req_t, sul_conn);

	if (r->wsi_raw || !lws_dll2_is_detached(&r->sul_conn.list))
		return;

	/* create the UDP socket aimed at the server */

	r->retry_count_write = 0;
	r->wsi_raw = lws_create_adopt_udp(r->context->vhost_system, "0.0.0.0",
					  68, LWS_CAUDP_PF_PACKET |
					      LWS_CAUDP_BROADCAST,
					  "lws-dhcp4client", (const char *)&r[1],
					  NULL, NULL, &bo2, "dhcpc");
	lwsl_debug("%s: created wsi_raw: %s\n", __func__, lws_wsi_tag(r->wsi_raw));
	if (!r->wsi_raw) {
		lwsl_err("%s: unable to create udp skt\n", __func__);

		lws_retry_sul_schedule(r->context, 0, &r->sul_conn, &bo2,
				       lws_dhcpc4_retry_conn,
				       &r->retry_count_conn);

		return;
	}

	/* force the network if up */
	lws_plat_if_up((const char *)&r[1], r->wsi_raw->desc.sockfd, 0);
	lws_plat_if_up((const char *)&r[1], r->wsi_raw->desc.sockfd, 1);

	r->wsi_raw->user_space = r;
	r->wsi_raw->user_space_externally_allocated = 1;

	lws_get_random(r->wsi_raw->a.context, r->xid, 4);
}

static void
lws_sa46_set_ipv4(lws_dhcpc_req_t *r, unsigned int which, uint8_t *p)
{
	r->is.sa46[which].sa4.sin_family = AF_INET;
	r->is.sa46[which].sa4.sin_addr.s_addr = ntohl(lws_ser_ru32be(p));
}

int
lws_dhcpc4_parse(lws_dhcpc_req_t *r, void *in, size_t len)
{
	uint8_t pkt[LWS_PRE + 576], *p = pkt + LWS_PRE, *end;
	int n, m;

	switch (r->state) {
	case LDHC_INIT:		/* expect DHCPOFFER */
	case LDHC_REQUESTING:	/* expect DHCPACK */
		/*
		 * We should check carefully if we like what we were
		 * sent... anything can spam us with crafted replies
		 */
		if (len < 0x100)
			break;

		p = (uint8_t *)in + 28; /* skip to UDP payload */
		if (p[0] != 2 || p[1] != 1 || p[2] != 6)
			break;

		if (memcmp(&p[4], r->xid, 4))	/* must be our xid */
			break;

		if (memcmp(&p[0x1c], r->is.mac, 6)) /* our netif mac? */
			break;

		/* the DHCP magic cookie must be in place */
		if (lws_ser_ru32be(&p[0xec]) != 0x63825363)
			break;

		/* "your" client IP address */
		lws_sa46_set_ipv4(r, LWSDH_SA46_IP, p + 0x10);
		/* IP of next server used in bootstrap */
		lws_sa46_set_ipv4(r, LWSDH_SA46_DHCP_SERVER, p + 0x14);

		/* it looks legit so far... look at the options */

		end = (uint8_t *)in + len;
		p += 0xec + 4;
		while (p < end) {
			uint8_t c = *p++;
			uint8_t l = 0;

			if (c && c != 0xff) {
				/* pad 0 and EOT 0xff have no length */
				l = *p++;
				if (!l) {
					lwsl_err("%s: zero length\n",
							__func__);
					goto broken;
				}
				if (p + l > end) {
					/* ...nice try... */
					lwsl_err("%s: bad len\n",
							__func__);
					goto broken;
				}
			}

			if (c == 0xff) /* end of options */
				break;

			m = 0;
			switch (c) {
			case LWSDHC4POPT_SUBNET_MASK:
				n = LWSDH_IPV4_SUBNET_MASK;
				goto get_ipv4;

			case LWSDHC4POPT_ROUTER:
				lws_sa46_set_ipv4(r, LWSDH_SA46_IPV4_ROUTER, p);
				break;

			case LWSDHC4POPT_TIME_SERVER:
				lws_sa46_set_ipv4(r, LWSDH_SA46_NTP_SERVER, p);
				break;

			case LWSDHC4POPT_BROADCAST_ADS:
				n = LWSDH_IPV4_BROADCAST;
				goto get_ipv4;

			case LWSDHC4POPT_LEASE_TIME:
				n = LWSDH_LEASE_SECS;
				goto get_ipv4;

			case LWSDHC4POPT_RENEWAL_TIME: /* AKA T1 */
				n = LWSDH_RENEWAL_SECS;
				goto get_ipv4;

			case LWSDHC4POPT_REBINDING_TIME: /* AKA T2 */
				n = LWSDH_REBINDING_SECS;
				goto get_ipv4;

			case LWSDHC4POPT_DNSERVER:
				if (l & 3)
					break;
				m = LWSDH_SA46_DNS_SRV_1;
				while (l && m - LWSDH_SA46_DNS_SRV_1 < 4) {
					lws_sa46_set_ipv4(r, (unsigned int)m++, p);
					l = (uint8_t)(l - 4);
					p += 4;
				}
				break;

			case LWSDHC4POPT_DOMAIN_NAME:
				m = l;
				if (m > (int)sizeof(r->is.domain) - 1)
					m = sizeof(r->is.domain) - 1;
				lws_strnncpy(r->is.domain, (const char *)p,
					 (unsigned int)m, sizeof(r->is.domain));
				break;

			case LWSDHC4POPT_MESSAGE_TYPE:
				/*
				 * Confirm this is the right message
				 * for the state of the negotiation
				 */
				if (r->state == LDHC_INIT && *p != LWSDHC4POFFER)
					goto broken;
				if (r->state == LDHC_REQUESTING &&
				    *p != LWSDHC4PACK)
					goto broken;
				break;

			default:
				break;
			}

			p += l;
			continue;
get_ipv4:
			if (l >= 4)
				r->is.nums[n] = ntohl(lws_ser_ru32be(p));
			p += l;
			continue;
broken:
			memset(r->is.sa46, 0, sizeof(r->is.sa46));
			break;
		}

#if defined(_DEBUG)
		/* dump what we have parsed out */

		for (n = 0; n < (int)_LWSDH_NUMS_COUNT; n++) {
			m = (int)ntohl(r->is.nums[n]);
			lwsl_info("%s: %d: 0x%x\n", __func__, n, m);
		}

		for (n = 0; n < (int)_LWSDH_SA46_COUNT; n++) {
			lws_sa46_write_numeric_address(&r->is.sa46[n],
						       (char *)pkt, 48);
			lwsl_info("%s: %d: %s\n", __func__, n, pkt);
		}
#endif

		/*
		 * Having seen everything in there... do we really feel
		 * we could use it?  Everything critical is there?
		 */

		if (!r->is.sa46[LWSDH_SA46_IP].sa4.sin_family ||
		    !r->is.sa46[LWSDH_SA46_DHCP_SERVER].sa4.sin_family ||
		    !r->is.sa46[LWSDH_SA46_IPV4_ROUTER].sa4.sin_family ||
		    !r->is.nums[LWSDH_IPV4_SUBNET_MASK] ||
		    !r->is.nums[LWSDH_LEASE_SECS] ||
		    !r->is.sa46[LWSDH_SA46_DNS_SRV_1].sa4.sin_family) {
			lwsl_notice("%s: rejecting on incomplete\n", __func__);
			memset(r->is.sa46, 0, sizeof(r->is.sa46));
			break;
		}

		/*
		 * Network layout has to be internally consistent...
		 * DHCP server has to be reachable by broadcast and
		 * default route has to be on same subnet
		 */

		if ((r->is.sa46[LWSDH_SA46_IP].sa4.sin_addr.s_addr &
					r->is.nums[LWSDH_IPV4_SUBNET_MASK]) !=
		    (r->is.sa46[LWSDH_SA46_DHCP_SERVER].sa4.sin_addr.s_addr &
				        r->is.nums[LWSDH_IPV4_SUBNET_MASK])) {
			lwsl_notice("%s: rejecting on srv %x reachable on mask %x\n",
					__func__, r->is.sa46[LWSDH_SA46_IP].sa4.sin_addr.s_addr,
					r->is.nums[LWSDH_IPV4_SUBNET_MASK]);
			break;
		}

		if (r->state == LDHC_INIT) {
			lwsl_info("%s: moving to REQ\n", __func__);
			r->state = LDHC_REQUESTING;
			lws_callback_on_writable(r->wsi_raw);
			//break;
		}

		return 0;

	default:
		break;
	}

	return 1;
}

