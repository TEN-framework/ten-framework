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
#include <errno.h>

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE)
static int
interface_to_sa(struct lws_vhost *vh, const char *ifname,
		struct sockaddr_in *addr, size_t addrlen, int allow_ipv6)
{
	int ipv6 = 0;
#ifdef LWS_WITH_IPV6
	if (allow_ipv6)
		ipv6 = LWS_IPV6_ENABLED(vh);
#endif
	(void)vh;

	return lws_interface_to_sa(ipv6, ifname, addr, addrlen);
}
#endif

#ifndef LWS_PLAT_OPTEE
static int
lws_get_addresses(struct lws_vhost *vh, void *ads, char *name,
		  int name_len, char *rip, int rip_len)
{
	struct addrinfo ai, *res;
	struct sockaddr_in addr4;

	rip[0] = '\0';
	name[0] = '\0';
	addr4.sin_family = AF_UNSPEC;

#ifdef LWS_WITH_IPV6
	if (LWS_IPV6_ENABLED(vh)) {
		if (!lws_plat_inet_ntop(AF_INET6,
					&((struct sockaddr_in6 *)ads)->sin6_addr,
					rip, (socklen_t)rip_len)) {
			lwsl_vhost_err(vh, "inet_ntop: %s", strerror(LWS_ERRNO));
			return -1;
		}

		// Strip off the IPv4 to IPv6 header if one exists
		if (strncmp(rip, "::ffff:", 7) == 0)
			memmove(rip, rip + 7, strlen(rip) - 6);

		getnameinfo((struct sockaddr *)ads, sizeof(struct sockaddr_in6),
			    name,
#if defined(__ANDROID__)
			    (size_t)name_len,
#else
			    (socklen_t)name_len,
#endif
			    NULL, 0, 0);

		return 0;
	} else
#endif
	{
		struct addrinfo *result;

		memset(&ai, 0, sizeof ai);
		ai.ai_family = PF_UNSPEC;
		ai.ai_socktype = SOCK_STREAM;
#if !defined(LWS_PLAT_FREERTOS)
		if (getnameinfo((struct sockaddr *)ads,
				sizeof(struct sockaddr_in),
				name,
#if defined(__ANDROID__)
				(size_t)name_len,
#else
				(socklen_t)name_len,
#endif
				NULL, 0, 0))
			return -1;
#endif

		if (getaddrinfo(name, NULL, &ai, &result))
			return -1;

		res = result;
		while (addr4.sin_family == AF_UNSPEC && res) {
			switch (res->ai_family) {
			case AF_INET:
				addr4.sin_addr =
				 ((struct sockaddr_in *)res->ai_addr)->sin_addr;
				addr4.sin_family = AF_INET;
				break;
			}

			res = res->ai_next;
		}
		freeaddrinfo(result);
	}

	if (addr4.sin_family == AF_UNSPEC)
		return -1;

	if (lws_plat_inet_ntop(AF_INET, &addr4.sin_addr, rip,
			       (socklen_t)rip_len) == NULL)
		return -1;

	return 0;
}

const char *
lws_get_peer_simple_fd(lws_sockfd_type fd, char *name, size_t namelen)
{
	lws_sockaddr46 sa46;
	socklen_t len = sizeof(sa46);

	if (getpeername(fd, (struct sockaddr *)&sa46, &len) < 0) {
		lws_snprintf(name, namelen, "getpeername: %s",
				strerror(LWS_ERRNO));
		return name;
	}

	lws_sa46_write_numeric_address(&sa46, name, namelen);

	return name;
}

const char *
lws_get_peer_simple(struct lws *wsi, char *name, size_t namelen)
{
	wsi = lws_get_network_wsi(wsi);
	return lws_get_peer_simple_fd(wsi->desc.sockfd, name, namelen);
}
#endif

void
lws_get_peer_addresses(struct lws *wsi, lws_sockfd_type fd, char *name,
		       int name_len, char *rip, int rip_len)
{
#ifndef LWS_PLAT_OPTEE
	socklen_t len;
#ifdef LWS_WITH_IPV6
	struct sockaddr_in6 sin6;
#endif
	struct sockaddr_in sin4;
	void *p;

	rip[0] = '\0';
	name[0] = '\0';

#ifdef LWS_WITH_IPV6
	if (LWS_IPV6_ENABLED(wsi->a.vhost)) {
		len = sizeof(sin6);
		p = &sin6;
	} else
#endif
	{
		len = sizeof(sin4);
		p = &sin4;
	}

	if (getpeername(fd, p, &len) < 0) {
		lwsl_wsi_warn(wsi, "getpeername: %s", strerror(LWS_ERRNO));
		goto bail;
	}

	lws_get_addresses(wsi->a.vhost, p, name, name_len, rip, rip_len);

bail:
#endif
	(void)wsi;
	(void)fd;
	(void)name;
	(void)name_len;
	(void)rip;
	(void)rip_len;
}



/* note: this returns a random port, or one of these <= 0 return codes:
 *
 * LWS_ITOSA_USABLE:     the interface is usable, returned if so and sockfd invalid
 * LWS_ITOSA_NOT_EXIST:  the requested iface does not even exist
 * LWS_ITOSA_NOT_USABLE: the requested iface exists but is not usable (eg, no IP)
 * LWS_ITOSA_BUSY:       the port at the requested iface + port is already in use
 */

int
lws_socket_bind(struct lws_vhost *vhost, struct lws *wsi,
		lws_sockfd_type sockfd, int port, const char *iface,
		int af)
{
#ifdef LWS_WITH_UNIX_SOCK
	struct sockaddr_un serv_unix;
#endif
#ifdef LWS_WITH_IPV6
	struct sockaddr_in6 serv_addr6;
#endif
	struct sockaddr_in serv_addr4;
#ifndef LWS_PLAT_OPTEE
	socklen_t len = sizeof(struct sockaddr_storage);
#endif
	int n = 0;
#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE)
	int m;
#endif
	struct sockaddr_storage sin, *psin = &sin;
	struct sockaddr *v;

	memset(&sin, 0, sizeof(sin));

	/* if there's a wsi, we want to mark it with our source ads:port */
	if (wsi)
		psin = (struct sockaddr_storage *)&wsi->sa46_local;

	switch (af) {
#if defined(LWS_WITH_UNIX_SOCK)
	case AF_UNIX:
		v = (struct sockaddr *)&serv_unix;
		memset(&serv_unix, 0, sizeof(serv_unix));
		serv_unix.sun_family = AF_UNIX;
		if (!iface)
			return LWS_ITOSA_NOT_EXIST;
		if (sizeof(serv_unix.sun_path) <= strlen(iface)) {
			lwsl_wsi_err(wsi, "\"%s\" too long for UNIX domain socket",
			         iface);
			return LWS_ITOSA_NOT_EXIST;
		}
		n = (int)(sizeof(uint16_t) + strlen(iface));
		strcpy(serv_unix.sun_path, iface);
		if (serv_unix.sun_path[0] == '@')
			serv_unix.sun_path[0] = '\0';
		else
			unlink(serv_unix.sun_path);

		// lwsl_hexdump_notice(v, n);
		break;
#endif
#if defined(LWS_WITH_IPV6) && !defined(LWS_PLAT_FREERTOS)
	case AF_INET6:
		v = (struct sockaddr *)&serv_addr6;
		n = sizeof(struct sockaddr_in6);

		memset(&serv_addr6, 0, sizeof(serv_addr6));
		serv_addr6.sin6_family = AF_INET6;
		if (iface) {
			m = interface_to_sa(vhost, iface,
				    (struct sockaddr_in *)v, (unsigned int)n, 1);
			if (m == LWS_ITOSA_NOT_USABLE) {
				lwsl_wsi_info(wsi, "netif %s: Not usable",
						   iface);
				return m;
			}
			if (m == LWS_ITOSA_NOT_EXIST) {
				lwsl_wsi_info(wsi, "netif %s: Does not exist",
						   iface);
				return m;
			}
			serv_addr6.sin6_scope_id = (unsigned int)htonl((uint32_t)
					lws_get_addr_scope(wsi, iface));
		}

		serv_addr6.sin6_port = (uint16_t)htons((uint16_t)port);
		break;
#endif

	case AF_INET:
		v = (struct sockaddr *)&serv_addr4;
		n = sizeof(serv_addr4);
		memset(&serv_addr4, 0, sizeof(serv_addr4));
		serv_addr4.sin_addr.s_addr = INADDR_ANY;
		serv_addr4.sin_family = AF_INET;

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE)
		if (iface) {
		    m = interface_to_sa(vhost, iface,
				    (struct sockaddr_in *)v, (unsigned int)n, 0);
			if (m == LWS_ITOSA_NOT_USABLE) {
				lwsl_wsi_info(wsi, "netif %s: Not usable",
						   iface);
				return m;
			}
			if (m == LWS_ITOSA_NOT_EXIST) {
				lwsl_wsi_info(wsi, "netif %s: Does not exist",
						   iface);
				return m;
			}
		}
#endif
		serv_addr4.sin_port = htons((uint16_t)(unsigned int)port);
		break;
	default:
		return -1;
	} /* switch */

	/* just checking for the interface extant */
	if (sockfd == LWS_SOCK_INVALID)
		return LWS_ITOSA_USABLE;

	n = bind(sockfd, v, (socklen_t)n);
#ifdef LWS_WITH_UNIX_SOCK
	if (n < 0 && af == AF_UNIX) {
		lwsl_wsi_err(wsi, "ERROR on binding fd %d to \"%s\" (%d %d)",
				  sockfd, iface, n, LWS_ERRNO);

		return LWS_ITOSA_NOT_EXIST;
	} else
#endif
	if (n < 0) {
		int _lws_errno = LWS_ERRNO;

		lwsl_wsi_err(wsi, "ERROR on binding fd %d to port %d (%d %d)",
				  sockfd, port, n, _lws_errno);

		/* if something already listening, tell caller to fail permanently */

		if (_lws_errno == LWS_EADDRINUSE)
			return LWS_ITOSA_BUSY;

		/* otherwise ask caller to retry later */

		return LWS_ITOSA_NOT_EXIST;
	}

#if defined(LWS_WITH_UNIX_SOCK) && !defined(WIN32)
	if (af == AF_UNIX) {
		uid_t uid = vhost->context->uid;
		gid_t gid = vhost->context->gid;

		if (vhost->unix_socket_perms) {
			if (lws_plat_user_colon_group_to_ids(
				vhost->unix_socket_perms, &uid, &gid)) {
				lwsl_wsi_err(wsi, "Failed to translate %s",
						   vhost->unix_socket_perms);
				return LWS_ITOSA_NOT_EXIST;
			}
		}
		if (iface && iface[0] != '@' && uid && gid) {
			if (chown(iface, uid, gid)) {
				lwsl_wsi_err(wsi, "failed to set %s perms %u:%u",
						  iface, (unsigned int)uid,
						  (unsigned int)gid);

				return LWS_ITOSA_NOT_EXIST;
			}
			lwsl_wsi_notice(wsi, "vh %s unix skt %s perms %u:%u",
					      vhost->name, iface,
					      (unsigned int)uid,
					      (unsigned int)gid);

			if (chmod(iface, 0660)) {
				lwsl_wsi_err(wsi, "0600 mode on %s fail", iface);

				return LWS_ITOSA_NOT_EXIST;
			}
		}
	}
#endif

#ifndef LWS_PLAT_OPTEE
	if (getsockname(sockfd, (struct sockaddr *)psin, &len) == -1)
		lwsl_wsi_warn(wsi, "getsockname: %s", strerror(LWS_ERRNO));
	else
#endif
#if defined(LWS_WITH_IPV6)
		port = (sin.ss_family == AF_INET6) ?
			ntohs(((struct sockaddr_in6 *)psin)->sin6_port) :
			ntohs(((struct sockaddr_in *)psin)->sin_port);
#else
		{
			struct sockaddr_in sain;
			memcpy(&sain, psin, sizeof(sain));
			port = ntohs(sain.sin_port);
		}
#endif

		{
			char buf[72];
			lws_sa46_write_numeric_address((lws_sockaddr46 *)psin,
							buf, sizeof(buf));

			lwsl_vhost_notice(vhost, "source ads %s", buf);
		}

	return port;
}

#if defined(LWS_WITH_CLIENT)

unsigned int
lws_retry_get_delay_ms(struct lws_context *context,
		       const lws_retry_bo_t *retry, uint16_t *ctry,
		       char *conceal)
{
	uint64_t ms = 3000, pc = 30; /* sane-ish defaults if no retry table */
	uint16_t ra;

	if (conceal)
		*conceal = 0;

	if (retry) {
		if (retry->retry_ms_table_count) {
			if (*ctry < retry->retry_ms_table_count)
				ms = retry->retry_ms_table[*ctry];
			else
				ms = retry->retry_ms_table[
					retry->retry_ms_table_count - 1];
		}

		/* if no percent given, use the default 30% */
		if (retry->jitter_percent)
			pc = retry->jitter_percent;
	}

	if (lws_get_random(context, &ra, sizeof(ra)) == sizeof(ra))
		ms += ((ms * pc * ra) >> 16) / 100;
	else
		assert(0);

	if (*ctry < 0xffff)
		(*ctry)++;

	if (retry && conceal)
		*conceal = (int)*ctry <= retry->conceal_count;

	return (unsigned int)ms;
}

int
lws_retry_sul_schedule(struct lws_context *context, int tid,
		       lws_sorted_usec_list_t *sul,
		       const lws_retry_bo_t *retry, sul_cb_t cb, uint16_t *ctry)
{
	char conceal;
	uint64_t ms = lws_retry_get_delay_ms(context, retry, ctry, &conceal);

	if (!conceal)
		return 1;

	lwsl_cx_info(context, "sul %p: scheduling retry in %dms", sul, (int)ms);

	lws_sul_schedule(context, tid, sul, cb, (int64_t)(ms * 1000));

	return 0;
}

int
lws_retry_sul_schedule_retry_wsi(struct lws *wsi, lws_sorted_usec_list_t *sul,
				 sul_cb_t cb, uint16_t *ctry)
{
	char conceal;
	lws_usec_t us = lws_retry_get_delay_ms(wsi->a.context,
					       wsi->retry_policy, ctry,
					       &conceal) * LWS_US_PER_MS;

	if (!conceal)
		/* if our reties are up, they're up... */
		return 1;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	if (
#if defined(LWS_ROLE_H1)
		wsi->role_ops == &role_ops_h1
#endif
#if defined(LWS_ROLE_H1) && defined(LWS_ROLE_H2)
		||
#endif
#if defined(LWS_ROLE_H2)
		wsi->role_ops == &role_ops_h2
#endif
		)
		/*
		 * Since we're doing it by wsi, we're in a position to check for
		 * http retry-after, it will increase us accordingly if found
		 */
		lws_http_check_retry_after(wsi, &us);
#endif
	lws_sul_schedule(wsi->a.context, wsi->tsi, sul, cb, us);

	return 0;
}

#endif

#if defined(LWS_WITH_IPV6)
unsigned long
lws_get_addr_scope(struct lws *wsi, const char *ifname_or_ipaddr)
{
	unsigned long scope;
	char ip[NI_MAXHOST];
	unsigned int i;
#if !defined(WIN32)
	struct ifaddrs *addrs, *addr;
#else
	PIP_ADAPTER_ADDRESSES adapter, addrs = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS addr;
	struct sockaddr_in6 *sockaddr;
	ULONG size = 0;
	int found = 0;
	DWORD ret;
#endif

	/*
	 * First see if we can look the string up as a network interface name...
	 * windows vista+ also has this
	 */

	scope = if_nametoindex(ifname_or_ipaddr);
	if (scope > 0)
		/* we found it from the interface name lookup */
		return scope;

	/*
	 * if not, try to look it up as an IP -> interface -> interface index
	 */

	scope = 0;

#if !defined(WIN32)

	getifaddrs(&addrs);
	for (addr = addrs; addr; addr = addr->ifa_next) {
		if (!addr->ifa_addr ||
			addr->ifa_addr->sa_family != AF_INET6)
			continue;

		ip[0] = '\0';
		getnameinfo(addr->ifa_addr, sizeof(struct sockaddr_in6),
			    ip, sizeof(ip), NULL, 0, NI_NUMERICHOST);

		i = 0;
		while (ip[i])
			if (ip[i++] == '%') {
				ip[i - 1] = '\0';
				break;
			}

		if (!strcmp(ip, ifname_or_ipaddr)) {
			scope = if_nametoindex(addr->ifa_name);
			break;
		}
	}
	freeifaddrs(addrs);
#else

	for (i = 0; i < 5; i++) {
		ret = GetAdaptersAddresses(AF_INET6, GAA_FLAG_INCLUDE_PREFIX,
					   NULL, addrs, &size);
		if (ret == NO_ERROR || ret == ERROR_NO_DATA)
			break;

		if (addrs)
			free(addrs);

		if (ret != ERROR_BUFFER_OVERFLOW) {
			addrs = NULL;
			lwsl_wsi_err(wsi, "Get IPv6 ads table fail (%d)", ret);
			break;
		}

		addrs = (IP_ADAPTER_ADDRESSES *)malloc(size);
	}

	if ((ret == NO_ERROR) && (addrs)) {
		adapter = addrs;
		while (adapter && !found) {
			addr = adapter->FirstUnicastAddress;
			while (addr && !found) {
				if (addr->Address.lpSockaddr->sa_family ==
				    AF_INET6) {
					sockaddr = (struct sockaddr_in6 *)
						(addr->Address.lpSockaddr);

					lws_plat_inet_ntop(sockaddr->sin6_family,
							&sockaddr->sin6_addr,
							ip, sizeof(ip));

					if (!strcmp(ip, ifname_or_ipaddr)) {
						scope = sockaddr->sin6_scope_id;
						found = 1;
						break;
					}
				}
				addr = addr->Next;
			}
			adapter = adapter->Next;
		}
	}
	if (addrs)
		free(addrs);
#endif

	return scope;
}
#endif

/*
 * https://en.wikipedia.org/wiki/IPv6_address
 *
 * An IPv6 address is represented as eight groups of four hexadecimal digits,
 * each group representing 16 bits (two octets, a group sometimes also called a
 * hextet[6][7]). The groups are separated by colons (:). An example of an IPv6
 * address is:
 *
 *    2001:0db8:85a3:0000:0000:8a2e:0370:7334
 *
 * The hexadecimal digits are case-insensitive, but IETF recommendations suggest
 * the use of lower case letters. The full representation of eight 4-digit
 * groups may be simplified by several techniques, eliminating parts of the
 * representation.
 *
 * Leading zeroes in a group may be omitted, but each group must retain at least
 * one hexadecimal digit.[1] Thus, the example address may be written as:
 *
 *    2001:db8:85a3:0:0:8a2e:370:7334
 *
 * One or more consecutive groups containing zeros only may be replaced with a
 * single empty group, using two consecutive colons (::).[1] The substitution
 * may only be applied once in the address, however, because multiple
 * occurrences would create an ambiguous representation. Thus, the example
 * address can be further simplified:
 *
 *    2001:db8:85a3::8a2e:370:7334
 *
 * The localhost (loopback) address, 0:0:0:0:0:0:0:1, and the IPv6 unspecified
 * address, 0:0:0:0:0:0:0:0, are reduced to ::1 and ::, respectively.
 *
 * During the transition of the Internet from IPv4 to IPv6, it is typical to
 * operate in a mixed addressing environment. For such use cases, a special
 * notation has been introduced, which expresses IPv4-mapped and IPv4-compatible
 * IPv6 addresses by writing the least-significant 32 bits of an address in the
 * familiar IPv4 dot-decimal notation, whereas the other 96 (most significant)
 * bits are written in IPv6 format. For example, the IPv4-mapped IPv6 address
 * ::ffff:c000:0280 is written as ::ffff:192.0.2.128, thus expressing clearly
 * the original IPv4 address that was mapped to IPv6.
 */

int
lws_parse_numeric_address(const char *ads, uint8_t *result, size_t max_len)
{
	struct lws_tokenize ts;
	uint8_t *orig = result, temp[16];
	int sects = 0, ipv6 = !!strchr(ads, ':'), skip_point = -1, dm = 0;
	char t[5];
	size_t n;
	long u;

	lws_tokenize_init(&ts, ads, LWS_TOKENIZE_F_NO_INTEGERS |
				    LWS_TOKENIZE_F_MINUS_NONTERM);
	ts.len = strlen(ads);
	if (!ipv6 && ts.len < 7)
		return -1;

	if (ipv6 && ts.len < 2)
		return -2;

	if (!ipv6 && max_len < 4)
		return -3;

	if (ipv6 && max_len < 16)
		return -4;

	if (ipv6)
		memset(result, 0, max_len);

	do {
		ts.e = (int8_t)lws_tokenize(&ts);
		switch (ts.e) {
		case LWS_TOKZE_TOKEN:
			dm = 0;
			if (ipv6) {
				if (ts.token_len > 4)
					return -1;
				memcpy(t, ts.token, ts.token_len);
				t[ts.token_len] = '\0';
				for (n = 0; n < ts.token_len; n++)
					if (t[n] < '0' || t[n] > 'f' ||
					    (t[n] > '9' && t[n] < 'A') ||
					    (t[n] > 'F' && t[n] < 'a'))
						return -1;
				u = strtol(t, NULL, 16);
				if (u > 0xffff)
					return -5;
				*result++ = (uint8_t)(u >> 8);
			} else {
				if (ts.token_len > 3)
					return -1;
				memcpy(t, ts.token, ts.token_len);
				t[ts.token_len] = '\0';
				for (n = 0; n < ts.token_len; n++)
					if (t[n] < '0' || t[n] > '9')
						return -1;
				u = strtol(t, NULL, 10);
				if (u > 0xff)
					return -6;
			}
			if (u < 0)
				return -7;
			*result++ = (uint8_t)u;
			sects++;
			break;

		case LWS_TOKZE_DELIMITER:
			if (dm++) {
				if (dm > 2)
					return -8;
				if (*ts.token != ':')
					return -9;
				/* back to back : */
				*result++ = 0;
				*result++ = 0;
				skip_point = lws_ptr_diff(result, orig);
				break;
			}
			if (ipv6 && orig[2] == 0xff && orig[3] == 0xff &&
			    skip_point == 2) {
				/* ipv4 backwards compatible format */
				ipv6 = 0;
				memset(orig, 0, max_len);
				orig[10] = 0xff;
				orig[11] = 0xff;
				skip_point = -1;
				result = &orig[12];
				sects = 0;
				break;
			}
			if (ipv6 && *ts.token != ':')
				return -10;
			if (!ipv6 && *ts.token != '.')
				return -11;
			break;

		case LWS_TOKZE_ENDED:
			if (!ipv6 && sects == 4)
				return lws_ptr_diff(result, orig);
			if (ipv6 && sects == 8)
				return lws_ptr_diff(result, orig);
			if (skip_point != -1) {
				int ow = lws_ptr_diff(result, orig);
				/*
				 * contains ...::...
				 */
				if (ow == 16)
					return 16;
				memcpy(temp, &orig[skip_point], (unsigned int)(ow - skip_point));
				memset(&orig[skip_point], 0, (unsigned int)(16 - skip_point));
				memcpy(&orig[16 - (ow - skip_point)], temp,
						   (unsigned int)(ow - skip_point));

				return 16;
			}
			return -12;

		default: /* includes ENDED */
			lwsl_err("%s: malformed ip address\n",
				 __func__);

			return -13;
		}
	} while (ts.e > 0 && result - orig <= (int)max_len);

	lwsl_err("%s: ended on e %d\n", __func__, ts.e);

	return -14;
}

int
lws_sa46_parse_numeric_address(const char *ads, lws_sockaddr46 *sa46)
{
	uint8_t a[16];
	int n;

	n = lws_parse_numeric_address(ads, a, sizeof(a));
	if (n < 0)
		return -1;

#if defined(LWS_WITH_IPV6)
	if (n == 16) {
		sa46->sa6.sin6_family = AF_INET6;
		memcpy(sa46->sa6.sin6_addr.s6_addr, a,
		       sizeof(sa46->sa6.sin6_addr.s6_addr));

		return 0;
	}
#endif

	if (n != 4)
		return -1;

	sa46->sa4.sin_family = AF_INET;
	memcpy(&sa46->sa4.sin_addr.s_addr, a,
	       sizeof(sa46->sa4.sin_addr.s_addr));

	return 0;
}

int
lws_write_numeric_address(const uint8_t *ads, int size, char *buf, size_t len)
{
	char c, elided = 0, soe = 0, zb = (char)-1, n, ipv4 = 0;
	const char *e = buf + len;
	char *obuf = buf;
	int q = 0;

	if (size == 4)
		return lws_snprintf(buf, len, "%u.%u.%u.%u",
				    ads[0], ads[1], ads[2], ads[3]);

	if (size != 16)
		return -1;

	for (c = 0; c < (char)size / 2; c++) {
		uint16_t v = (uint16_t)((ads[q] << 8) | ads[q + 1]);

		if (buf + 8 > e)
			return -1;

		q += 2;
		if (soe) {
			if (v)
				*buf++ = ':';
				/* fall thru to print hex value */
		} else
			if (!elided && !soe && !v) {
				elided = soe = 1;
				zb = c;
				continue;
			}

		if (ipv4) {
			n = (char)lws_snprintf(buf, lws_ptr_diff_size_t(e, buf), "%u.%u",
					ads[q - 2], ads[q - 1]);
			buf += n;
			if (c == 6)
				*buf++ = '.';
		} else {
			if (soe && !v)
				continue;
			if (c)
				*buf++ = ':';

			buf += lws_snprintf(buf, lws_ptr_diff_size_t(e, buf), "%x", v);

			if (soe && v) {
				soe = 0;
				if (c == 5 && v == 0xffff && !zb) {
					ipv4 = 1;
					*buf++ = ':';
				}
			}
		}
	}
	if (buf + 3 > e)
		return -1;

	if (soe) { /* as is the case for all zeros */
		*buf++ = ':';
		*buf++ = ':';
		*buf = '\0';
	}

	return lws_ptr_diff(buf, obuf);
}

int
lws_sa46_write_numeric_address(lws_sockaddr46 *sa46, char *buf, size_t len)
{
	*buf = '\0';
#if defined(LWS_WITH_IPV6)
	if (sa46->sa4.sin_family == AF_INET6)
		return lws_write_numeric_address(
				(uint8_t *)&sa46->sa6.sin6_addr, 16, buf, len);
#endif
	if (sa46->sa4.sin_family == AF_INET)
		return lws_write_numeric_address(
				(uint8_t *)&sa46->sa4.sin_addr, 4, buf, len);

#if defined(LWS_WITH_UNIX_SOCK)
	if (sa46->sa4.sin_family == AF_UNIX)
		return lws_snprintf(buf, len, "(unix skt)");
#endif

	if (!sa46->sa4.sin_family)
		return lws_snprintf(buf, len, "(unset)");

	if (sa46->sa4.sin_family == AF_INET6)
		return lws_snprintf(buf, len, "(ipv6 unsupp)");

	lws_snprintf(buf, len, "(AF%d unsupp)", (int)sa46->sa4.sin_family);

	return -1;
}

int
lws_sa46_compare_ads(const lws_sockaddr46 *sa46a, const lws_sockaddr46 *sa46b)
{
	if (sa46a->sa4.sin_family != sa46b->sa4.sin_family)
		return 1;

#if defined(LWS_WITH_IPV6)
	if (sa46a->sa4.sin_family == AF_INET6)
		return memcmp(&sa46a->sa6.sin6_addr, &sa46b->sa6.sin6_addr, 16);
#endif

	if (sa46a->sa4.sin_family == AF_INET)
		return sa46a->sa4.sin_addr.s_addr != sa46b->sa4.sin_addr.s_addr;

	return 0;
}

void
lws_4to6(uint8_t *v6addr, const uint8_t *v4addr)
{
	v6addr[12] = v4addr[0];
	v6addr[13] = v4addr[1];
	v6addr[14] = v4addr[2];
	v6addr[15] = v4addr[3];

	memset(v6addr, 0, 10);

	v6addr[10] = v6addr[11] = 0xff;
}

#if defined(LWS_WITH_IPV6)
void
lws_sa46_4to6(lws_sockaddr46 *sa46, const uint8_t *v4addr, uint16_t port)
{
	sa46->sa4.sin_family = AF_INET6;

	lws_4to6((uint8_t *)&sa46->sa6.sin6_addr.s6_addr[0], v4addr);

	sa46->sa6.sin6_port = htons(port);
}
#endif

int
lws_sa46_on_net(const lws_sockaddr46 *sa46a, const lws_sockaddr46 *sa46_net,
		int net_len)
{
	uint8_t mask = 0xff, norm[16];
	const uint8_t *p1, *p2;

	if (sa46a->sa4.sin_family == AF_INET) {
		p1 = (uint8_t *)&sa46a->sa4.sin_addr;
		if (sa46_net->sa4.sin_family == AF_INET6) {
			/* ip is v4, net is v6, promote ip to v6 */

			lws_4to6(norm, p1);
			p1 = norm;
		}
#if defined(LWS_WITH_IPV6)
	} else
		if (sa46a->sa4.sin_family == AF_INET6) {
			p1 = (uint8_t *)&sa46a->sa6.sin6_addr;
#endif
		} else
			return 1;

	if (sa46_net->sa4.sin_family == AF_INET) {
		p2 = (uint8_t *)&sa46_net->sa4.sin_addr;
		if (sa46a->sa4.sin_family == AF_INET6) {
			/* ip is v6, net is v4, promote net to v6 */

			lws_4to6(norm, p2);
			p2 = norm;
			/* because the mask length is for net v4 address */
			net_len += 12 * 8;
		}
#if defined(LWS_WITH_IPV6)
	} else
		if (sa46a->sa4.sin_family == AF_INET6) {
			p2 = (uint8_t *)&sa46_net->sa6.sin6_addr;
#endif
		} else
			return 1;

	while (net_len > 0) {
		if (net_len < 8)
			mask = (uint8_t)(mask << (8 - net_len));

		if (((*p1++) & mask) != ((*p2++) & mask))
			return 1;

		net_len -= 8;
	}

	return 0;
}

void
lws_sa46_copy_address(lws_sockaddr46 *sa46a, const void *in, int af)
{
	sa46a->sa4.sin_family = (sa_family_t)af;

	if (af == AF_INET)
		memcpy(&sa46a->sa4.sin_addr, in, 4);
#if defined(LWS_WITH_IPV6)
	else if (af == AF_INET6)
		memcpy(&sa46a->sa6.sin6_addr, in, sizeof(sa46a->sa6.sin6_addr));
#endif
}

#if defined(LWS_WITH_SYS_STATE)
lws_state_manager_t *
lws_system_get_state_manager(struct lws_context *context)
{
	return &context->mgr_system;
}
#endif
