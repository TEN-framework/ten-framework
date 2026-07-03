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
 *
 *
 *  Either the libc getaddrinfo() or ASYNC_DNS provides a chain of addrinfo,
 *  we use lws_sort_dns() to convert it to an lws_dll2 of lws_dns_sort_t, after
 *  which the addrinfo results are freed.
 *
 *  If the system has no routing table info (from, eg, NETLINK), then that's
 *  it the sorted results are bound to the wsi and used.
 *
 *  If the system has routing table info, we study the routing table and the
 *  DNS results in order to sort the lws_dns_sort_t result linked-list into
 *  most desirable at the head, and strip results we can't see a way to route.
 */

#include "private-lib-core.h"

#if defined(__linux__)
#include <linux/if_addr.h>
#endif

#if defined(__FreeBSD__)
#include <net/if.h>
#include <netinet6/in6_var.h>
#endif

#if defined(LWS_WITH_IPV6) && defined(LWS_WITH_NETLINK)

/*
 * RFC6724 default policy table
 *
 *      Prefix        Precedence Label
 *    ::1/128               50     0
 *    ::/0                  40     1
 *    ::ffff:0:0/96         35     4  (override prec to 100 to prefer ipv4)
 *    2002::/16             30     2
 *    2001::/32              5     5
 *    fc00::/7               3    13
 *    ::/96                  1     3
 *    fec0::/10              1    11
 *    3ffe::/16              1    12
 *
 * implemented using offsets into a combined 40-byte table below
 */

static const uint8_t ma[] = {
	/*  0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	/* 16 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff,
	/* 28 */ 0x20, 0x02,
	/* 30 */ 0x20, 0x01, 0x00, 0x00,
	/* 34 */ 0xfc, 0x00,
	/* 36 */ 0xfe, 0xc0,
	/* 38 */ 0x3f, 0xfe
};

static const uint8_t frac[] = {
	0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe
};

/* 9 x 4 byte = 36 byte policy index table */

static const struct score_policy {
	uint8_t			ma_ofs;
	uint8_t			prefix;
	lws_dns_score_t		score;
} rfc6724_policy[] = {

	{  0,	128,	{  50,  0 } },		/* ::1/128 */
	{  0,	  0,	{  40,  1 } },		/* ::0 */
#if 1
	/* favour ipv6 as a general policy */
	{ 16,	 96,	{  35,  4 } },		/* ::ffff:0:0/96 */
#else
	/* favour ipv4 as a general policy */
	{ 16,	 96,	{ 100,  4 } },		/* ::ffff:0:0/96 */
#endif
	{ 28,	 16,	{  30,  2 } },		/* 2002::/16 */
	{ 30,	 32,	{   5,  5 } },		/* 2001::/32 */
	{ 34,	  7,	{   3, 13 } },		/* fc00::/7 */
	{  0,	 96,	{   1,  3 } },		/* ::/96 */
	{ 36,	 10,	{   1, 11 } },		/* fec0::/10 */
	{ 38,	 16,	{   1, 12 } },		/* 3ffe::/16 */

};

static int
lws_ipv6_prefix_match_len(const struct sockaddr_in6 *a,
			  const struct sockaddr_in6 *b)
{
	const uint8_t *ads_a = (uint8_t *)&a->sin6_addr,
		      *ads_b = (uint8_t *)&b->sin6_addr;
	int n = 0, match = 0;

	for (n = 0; n < 16; n++) {
		if (ads_a[n] == ads_b[n])
			match += 8;
		else
			break;
	}

	if (match != 128) {
		int m;

		for (m = 1; m < 8; m++) {
			if ((ads_a[n] & frac[m]) == (ads_b[n] & frac[m]))
				match++;
			else
				break;
		}
	}

	return match;
}

static int
lws_ipv6_unicast_scope(const struct sockaddr_in6 *sa)
{
	uint64_t *u;

	u = (uint64_t *)&sa->sin6_addr;
	if (*u == 0xfe80000000000000ull)
		return 2; /* link-local */

	return 0xe;
}

static int
lws_sort_dns_scope(lws_sockaddr46 *sa46)
{
	if (sa46->sa4.sin_family == AF_INET) {
		uint8_t *p = (uint8_t *)&sa46->sa4.sin_addr;

		/* RFC6724 3.2 */

		if (p[0] == 127 || (p[0] == 169 && p[1] == 254))
			return 2; /* link-local */

		return 0xe; /* global */
	}

	return lws_ipv6_unicast_scope(&sa46->sa6);
}

static int
lws_sort_dns_classify(lws_sockaddr46 *sa46, lws_dns_score_t *score)
{
	const struct score_policy *pol = rfc6724_policy;
	const uint8_t *p, *po;
	lws_sockaddr46 s;
	int n, m;

	memset(score, 0, sizeof(*score));

	if (sa46->sa4.sin_family == AF_INET) {
		memset(&s, 0, sizeof(s));
		s.sa6.sin6_family = AF_INET6;
		lws_4to6((uint8_t *)s.sa6.sin6_addr.s6_addr,
			 (const uint8_t *)&sa46->sa4.sin_addr);

		/* use the v6 version of the v4 address */
		sa46 = &s;
	}

	for (n = 0; n < (int)LWS_ARRAY_SIZE(rfc6724_policy); n++) {
		po = (uint8_t *)&sa46->sa6.sin6_addr.s6_addr;
		p = &ma[pol->ma_ofs];
		for (m = 0; m < pol->prefix >> 3; m++)
			if (*p++ != *po++)
				goto next;

		if ((pol->prefix & 7) && (*p & frac[pol->prefix & 7]) !=
					  (*po & frac[pol->prefix & 7]))
			goto next;

		*score = pol->score;

		return 0;

next:
		pol++;
	}

	return 1;
}


enum {
	SAS_PREFER_A	=  1,
	SAS_SAME	=  0,
	SAS_PREFER_B	= -1
};

/* ifa is laid out with types for ipv4, if it's AF_INET6 case to sockaddr_in6 */
#define to_v6_sa(x) ((struct sockaddr_in6 *)x)
#define to_sa46_sa(x) ((lws_sockaddr46 *)x)

/*
 * The source address selection algorithm produces as output a single
 * source address for use with a given destination address.  This
 * algorithm only applies to IPv6 destination addresses, not IPv4
 * addresses.
 *
 * This implements RFC6724 Section 5.
 *
 * Either or both sa and sb can be dest or gateway routes
 */

static int
lws_sort_dns_scomp(struct lws_context_per_thread *pt, const lws_route_t *sa,
		   const lws_route_t *sb, const struct sockaddr_in6 *dst)
{
	const struct sockaddr_in6 *sa6 = to_v6_sa(&sa->dest),
				  *sb6 = to_v6_sa(&sb->dest);
	lws_dns_score_t scorea, scoreb, scoredst;
	int scopea, scopeb, scoped, mla, mlb;
	lws_route_t *rd;

	if (!sa->dest.sa4.sin_family)
		sa6 = to_v6_sa(&sa->gateway);
	if (!sb->dest.sa4.sin_family)
		sb6 = to_v6_sa(&sb->gateway);

	/*
	 * We shouldn't come here unless sa and sb both have AF_INET6 addresses
	 */

	assert(sa6->sin6_family == AF_INET6);
	assert(sb6->sin6_family == AF_INET6);

	/*
	 * Rule 1: Prefer same address.
	 * If SA = D, then prefer SA.  Similarly, if SB = D, then prefer SB.
	 */

	if (!memcmp(&sa6->sin6_addr, &dst->sin6_addr, 16))
		return SAS_PREFER_A;
	if (!memcmp(&sb6->sin6_addr, &dst->sin6_addr, 16))
		return SAS_PREFER_B;

	/*
	 * Rule 2: Prefer appropriate scope.
	 * If Scope(SA) < Scope(SB): If Scope(SA) < Scope(D), then prefer SB
	 * and otherwise prefer SA.
	 *
	 * Similarly, if Scope(SB) < Scope(SA): If Scope(SB) < Scope(D), then
	 * prefer SA and otherwise prefer SB.
	 */

	scopea = lws_sort_dns_scope(to_sa46_sa(sa6));
	scopeb = lws_sort_dns_scope(to_sa46_sa(sb6));
	scoped = lws_sort_dns_scope(to_sa46_sa(dst));

	if (scopea < scopeb)
		return scopea < scoped ? SAS_PREFER_B : SAS_PREFER_A;

	if (scopeb < scopea)
		return scopeb < scoped ? SAS_PREFER_A : SAS_PREFER_B;

	/*
	 * Rule 3: Avoid deprecated addresses.
	 * If one of the two source addresses is "preferred" and one of them
	 * is "deprecated" (in the RFC 4862 sense), then prefer the one that
	 * is "preferred".
	 */

	if (!(sa->ifa_flags & IFA_F_DEPRECATED) &&
	     (sb->ifa_flags & IFA_F_DEPRECATED))
		return SAS_PREFER_A;

	if ( (sa->ifa_flags & IFA_F_DEPRECATED) &&
	    !(sb->ifa_flags & IFA_F_DEPRECATED))
		return SAS_PREFER_B;

	/*
	 * Rule 4: Prefer home addresses.
	 * If SA is simultaneously a home address and care-of address and SB is
	 * not, then prefer SA.  Similarly, if SB is simultaneously a home
	 * address and care-of address and SA is not, then prefer SB.  If SA is
	 * just a home address and SB is just a care-of address, then prefer SA.
	 * Similarly, if SB is just a home address and SA is just a care-of
	 * address, then prefer SB.
	 *
	 * !!! not sure how to determine if care-of address
	 */

	if ( (sa->ifa_flags & IFA_F_HOMEADDRESS) &&
	    !(sb->ifa_flags & IFA_F_HOMEADDRESS))
		return SAS_PREFER_A;

	if (!(sa->ifa_flags & IFA_F_HOMEADDRESS) &&
	     (sb->ifa_flags & IFA_F_HOMEADDRESS))
		return SAS_PREFER_B;

	/*
	 * Rule 5: Prefer outgoing interface.
	 * If SA is assigned to the interface that will be used to send to D
	 * and SB is assigned to a different interface, then prefer SA.
	 * Similarly, if SB is assigned to the interface that will be used
	 * to send to D and SA is assigned to a different interface, then
	 * prefer SB.
	 */

	rd = _lws_route_est_outgoing(pt, (lws_sockaddr46 *)dst);
	if (rd) {
		if (rd->if_idx == sa->if_idx)
			return SAS_PREFER_A;
		if (rd->if_idx == sb->if_idx)
			return SAS_PREFER_B;
	}

	/*
	 * Rule 6: Prefer matching label.
	 * If Label(SA) = Label(D) and Label(SB) <> Label(D), then prefer SA.
	 * Similarly, if Label(SB) = Label(D) and Label(SA) <> Label(D), then
	 * prefer SB.
	 */

	lws_sort_dns_classify(to_sa46_sa(sa6), &scorea);
	lws_sort_dns_classify(to_sa46_sa(sb6), &scoreb);
	lws_sort_dns_classify(to_sa46_sa(dst), &scoredst);

	if (scorea.label == scoredst.label && scoreb.label != scoredst.label)
		return SAS_PREFER_A;
	if (scoreb.label == scoredst.label && scorea.label != scoredst.label)
		return SAS_PREFER_B;

	/*
	 * Rule 7: Prefer temporary addresses.
	 * If SA is a temporary address and SB is a public address, then
	 * prefer SA.  Similarly, if SB is a temporary address and SA is a
	 * public address, then prefer SB.
	 */

	if ( (sa->ifa_flags & IFA_F_TEMPORARY) &&
	    !(sb->ifa_flags & IFA_F_TEMPORARY))
		return SAS_PREFER_A;

	if (!(sa->ifa_flags & IFA_F_TEMPORARY) &&
	     (sb->ifa_flags & IFA_F_TEMPORARY))
		return SAS_PREFER_B;

	/*
	 * Rule 8: Use longest matching prefix.
	 * If CommonPrefixLen(SA, D) > CommonPrefixLen(SB, D), then prefer SA.
	 * Similarly, if CommonPrefixLen(SB, D) > CommonPrefixLen(SA, D), then
	 * prefer SB.
	 */

	mla = lws_ipv6_prefix_match_len(sa6, dst);
	mlb = lws_ipv6_prefix_match_len(sb6, dst);

	if (mla > mlb)
		return SAS_PREFER_A;

	return SAS_SAME;
}

/*
 * Given two possible source addresses and the destination address, we attempt
 * to pick which one is "better".
 *
 * This implements RFC6724 Section 6.
 */

static int
lws_sort_dns_dcomp(const lws_dns_sort_t *da, const lws_dns_sort_t *db)
{
	int scopea, scopeb, scope_srca, scope_srcb, cpla, cplb;
	const uint8_t *da_ads = (const uint8_t *)&da->dest.sa6.sin6_addr,
		      *db_ads = (const uint8_t *)&db->dest.sa6.sin6_addr;
	lws_dns_score_t score_srca, score_srcb;

	/*
	 * Rule 1: Avoid unusable destinations
	 *
	 * We already strip destinations with no usable source
	 */

	/*
	 * Rule 2: Prefer matching scope
	 *
	 * If Scope(DA) = Scope(Source(DA)) and Scope(DB) <> Scope(Source(DB)),
	 * then prefer DA.  Similarly, if Scope(DA) <> Scope(Source(DA)) and
	 * Scope(DB) = Scope(Source(DB)), then prefer DB.
	 */

	scopea = lws_ipv6_unicast_scope(to_v6_sa(&da->dest));
	scopeb = lws_ipv6_unicast_scope(to_v6_sa(&db->dest));
	scope_srca = lws_ipv6_unicast_scope(to_v6_sa(&da->source));
	scope_srcb = lws_ipv6_unicast_scope(to_v6_sa(&db->source));

	if (scopea == scope_srca && scopeb != scope_srcb)
		return SAS_PREFER_A;

	if (scopea != scope_srca && scopeb == scope_srcb)
		return SAS_PREFER_B;

#if defined(IFA_F_DEPRECATED)
	/*
	 * Rule 3: Avoid deprecated addresses.
	 *
	 * If Source(DA) is deprecated and Source(DB) is not, then prefer DB.
	 * Similarly, if Source(DA) is not deprecated and Source(DB) is
	 * deprecated, then prefer DA.
	 */

	if (!(da->ifa_flags & IFA_F_DEPRECATED) &&
	     (db->ifa_flags & IFA_F_DEPRECATED))
		return SAS_PREFER_A;

	if ( (da->ifa_flags & IFA_F_DEPRECATED) &&
	    !(db->ifa_flags & IFA_F_DEPRECATED))
		return SAS_PREFER_B;
#endif

	/*
	 * Rule 4: Prefer home addresses.
	 *
	 * If Source(DA) is simultaneously a home address and care-of address
	 * and Source(DB) is not, then prefer DA.  Similarly, if Source(DB) is
	 * simultaneously a home address and care-of address and Source(DA) is
	 * not, then prefer DB.
	 *
	 * If Source(DA) is just a home address and Source(DB) is just a care-of
	 * address, then prefer DA.  Similarly, if Source(DA) is just a care-of
	 * address and Source(DB) is just a home address, then prefer DB.
	 *
	 * !!! not sure how to determine if care-of address
	 */

	if ( (da->ifa_flags & IFA_F_HOMEADDRESS) &&
	    !(db->ifa_flags & IFA_F_HOMEADDRESS))
		return SAS_PREFER_A;

	if (!(da->ifa_flags & IFA_F_HOMEADDRESS) &&
	     (db->ifa_flags & IFA_F_HOMEADDRESS))
		return SAS_PREFER_B;

	/*
	 * Rule 5: Prefer matching label.
	 *
	 * If Label(Source(DA)) = Label(DA) and Label(Source(DB)) <> Label(DB),
	 * then prefer DA.  Similarly, if Label(Source(DA)) <> Label(DA) and
	 * Label(Source(DB)) = Label(DB), then prefer DB
	 */

	if (!da->source)
		return SAS_PREFER_B;
	if (!db->source)
		return SAS_PREFER_A;

	lws_sort_dns_classify(&da->source->dest, &score_srca);
	lws_sort_dns_classify(&db->source->dest, &score_srcb);

	if (score_srca.label == da->score.label &&
	    score_srcb.label != db->score.label)
		return SAS_PREFER_A;
	if (score_srca.label != da->score.label &&
	    score_srcb.label == db->score.label)
		return SAS_PREFER_B;

	/*
	 * Rule 6: Prefer higher precedence.
	 *
	 * If Precedence(DA) > Precedence(DB), then prefer DA.  Similarly, if
	 * Precedence(DA) < Precedence(DB), then prefer DB.
	 */

	if (da->score.precedence > db->score.precedence)
		return SAS_PREFER_A;

	if (da->score.precedence < db->score.precedence)
		return SAS_PREFER_B;

	/*
	 * Rule 7: Prefer native transport.
	 * If DA is reached via an encapsulating transition mechanism (e.g.,
	 * IPv6 in IPv4) and DB is not, then prefer DB.  Similarly, if DB is
	 * reached via encapsulation and DA is not, then prefer DA.
	 */

	if (!memcmp(&ma[16], da_ads, 12) && memcmp(&ma[16], db_ads, 12))
		return SAS_PREFER_B;

	if (memcmp(&ma[16], da_ads, 12) && !memcmp(&ma[16], db_ads, 12))
		return SAS_PREFER_A;

	/*
	 * Rule 8: Prefer smaller scope.
	 * If Scope(DA) < Scope(DB), then prefer DA.  Similarly, if Scope(DA) >
	 * Scope(DB), then prefer DB.
	 */

	if (scopea < scopeb)
		return SAS_PREFER_A;

	if (scopea > scopeb)
		return SAS_PREFER_B;

	/*
	 * Rule 9: Use longest matching prefix.
	 * When DA and DB belong to the same address family (both are IPv6 or
	 * both are IPv4): If CommonPrefixLen(Source(DA), DA) >
	 * CommonPrefixLen(Source(DB), DB), then prefer DA.  Similarly, if
	 * CommonPrefixLen(Source(DA), DA) < CommonPrefixLen(Source(DB), DB),
	 * then prefer DB.
	 */

	cpla = lws_ipv6_prefix_match_len(&da->source->dest.sa6, &da->dest.sa6);
	cplb = lws_ipv6_prefix_match_len(&db->source->dest.sa6, &db->dest.sa6);

	if (cpla > cplb)
		return SAS_PREFER_A;

	if (cpla < cplb)
		return SAS_PREFER_B;

	/*
	 * Rule 10: Otherwise, leave the order unchanged.
	 */

	return SAS_SAME;
}

static int
lws_sort_dns_compare(const lws_dll2_t *a, const lws_dll2_t *b)
{
	const lws_dns_sort_t *sa = lws_container_of(a, lws_dns_sort_t, list),
			     *sb = lws_container_of(b, lws_dns_sort_t, list);

	return lws_sort_dns_dcomp(sa, sb);
}

#endif /* ipv6 + netlink */

#if defined(_DEBUG)

static void
lws_sort_dns_dump(struct lws *wsi)
{
	int n = 1;

	(void)n; /* nologs */

	if (!lws_dll2_get_head(&wsi->dns_sorted_list))
		lwsl_wsi_notice(wsi, "empty");

	lws_start_foreach_dll(struct lws_dll2 *, d,
			      lws_dll2_get_head(&wsi->dns_sorted_list)) {
		lws_dns_sort_t *s = lws_container_of(d, lws_dns_sort_t, list);
		char dest[48], gw[48];

		lws_sa46_write_numeric_address(&s->dest, dest, sizeof(dest));
		lws_sa46_write_numeric_address(&s->gateway, gw, sizeof(gw));

		lwsl_wsi_info(wsi, "%d: (%d)%s, gw (%d)%s, idi: %d, "
				"lbl: %d, prec: %d", n++,
			    s->dest.sa4.sin_family, dest,
			    s->gateway.sa4.sin_family, gw,
			    s->if_idx, s->score.label, s->score.precedence);

	} lws_end_foreach_dll(d);
}

#endif

int
lws_sort_dns(struct lws *wsi, const struct addrinfo *result)
{
#if defined(LWS_WITH_NETLINK)
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
#endif
	const struct addrinfo *ai = result;

	lwsl_wsi_info(wsi, "sort_dns: %p", result);

	/*
	 * We're going to take the dns results and produce our own linked-list
	 * of them, if we can sorted into descending preferability order, and
	 * possibly filtered.
	 *
	 * First let's just convert the addrinfo list into our expanded
	 * lws_dns_sort_t list, we can discard the addrinfo list then
	 */

	while (ai) {
#if defined(LWS_WITH_NETLINK) || \
	(defined(LWS_WITH_NETLINK) && defined(LWS_WITH_IPV6))
		lws_route_t
#if defined(LWS_WITH_NETLINK)
			*estr = NULL
#endif
#if defined(LWS_WITH_NETLINK) && defined(LWS_WITH_IPV6)
			, *bestsrc = NULL
#endif
		;
#endif
		lws_dns_sort_t *ds;
		char afip[48];

		/*
		 * Only transfer address families we can cope with
		 */
		if ((int)ai->ai_addrlen > (int)sizeof(lws_sockaddr46) ||
		    (ai->ai_family != AF_INET && ai->ai_family != AF_INET6))
			goto next;

		ds = lws_zalloc(sizeof(*ds), __func__);
		if (!ds)
			return 1;

		memcpy(&ds->dest, ai->ai_addr, (size_t)ai->ai_addrlen);
		ds->dest.sa4.sin_family = (sa_family_t)ai->ai_family;

		lws_sa46_write_numeric_address(&ds->dest, afip, sizeof(afip));

		lwsl_wsi_info(wsi, "unsorted entry (af %d) %s",
				   ds->dest.sa4.sin_family, afip);

#if defined(LWS_WITH_NETLINK)

		/*
		 * Let's assess this DNS result in terms of route
		 * selection, eg, if no usable net route or gateway for it,
		 * we don't have a way to use it if we listed it
		 */

		if (pt->context->routing_table.count) {

			estr = _lws_route_est_outgoing(pt, &ds->dest);
			if (!estr) {
				lws_free(ds);
				lwsl_wsi_notice(wsi, "%s has no route out\n",
						afip);
				/*
				 * There's no outbound route for this, it's
				 * unusable, so don't add it to the list
				 */
				goto next;
			}

			ds->if_idx = estr->if_idx;
			ds->uidx = estr->uidx;

			/*
			 * ...evidently, there's a way for it to go out...
			 */
		}
#endif

#if defined(LWS_WITH_NETLINK) && defined(LWS_WITH_IPV6)

		/*
		 * These sorting rules only apply to ipv6.  If we have ipv4
		 * dest and estimate we will use an ipv4 source address to
		 * route it, then skip this.
		 *
		 * However if we have ipv4 dest and estimate we will use an
		 * ipv6 source address to route it, because of ipv6-only
		 * egress, then promote it to ipv6 and sort it
		 */

		if (ds->dest.sa4.sin_family == AF_INET) {
			if (!estr ||
			    estr->dest.sa4.sin_family == AF_INET ||
			    estr->gateway.sa4.sin_family == AF_INET)
				/*
				 * No estimated route, or v4 estimated route,
				 * just add it to sorted list
				 */
				goto just_add;

			/*
			 * v4 dest on estimated v6 source ads route, because
			 * eg, there's no active v4 source ads just ipv6...
			 * promote v4 -> v6 address using ::ffff:xx:yy
			 */

			lwsl_wsi_info(wsi, "promoting v4->v6");

			lws_sa46_4to6(&ds->dest,
				      (uint8_t *)&ds->dest.sa4.sin_addr, 0);
		}

		/* first, classify this destination ads */
		lws_sort_dns_classify(&ds->dest, &ds->score);

		/*
		 * RFC6724 Section 5: Source Address Selection
		 *
		 * Go through the source options choosing the best for this
		 * destination... this can only operate on ipv6 destination
		 * address
		 */

		lws_start_foreach_dll(struct lws_dll2 *, d,
				      lws_dll2_get_head(&pt->context->routing_table)) {
			lws_route_t *r = lws_container_of(d, lws_route_t, list);

			/* gateway routes are skipped here */

			if (ds->dest.sa6.sin6_family == AF_INET6 &&
			    r->dest.sa4.sin_family == AF_INET6 && (!bestsrc ||
			    lws_sort_dns_scomp(pt, bestsrc, r, &ds->dest.sa6) ==
							    SAS_PREFER_B))
				bestsrc = r;

		} lws_end_foreach_dll(d);

		/* bestsrc is the best source route, or NULL if none */

		if (!bestsrc && pt->context->routing_table.count) {
			/* drop it, no usable source route */
			lws_free(ds);
			goto next;
		}

just_add:
		if (!bestsrc) {
			lws_dll2_add_tail(&ds->list, &wsi->dns_sorted_list);
			goto next;
		}

		ds->source = bestsrc;

		/*
		 * RFC6724 Section 6: Destination Address Selection
		 *
		 * Insert the destination into the list at a position reflecting
		 * its preferability, so the head entry is the most preferred
		 */

		lws_dll2_add_sorted(&ds->list, &wsi->dns_sorted_list,
				    lws_sort_dns_compare);
#else
		/*
		 * We don't have the routing table + source address details in
		 * order to sort the DNS results... simply make entries in the
		 * order of the addrinfo results
		 */

		lws_dll2_add_tail(&ds->list, &wsi->dns_sorted_list);
#endif

next:
		ai = ai->ai_next;
	}

	//lwsl_notice("%s: sorted table: %d\n", __func__,
	//		wsi->dns_sorted_list.count);

#if defined(_DEBUG)
	lws_sort_dns_dump(wsi);
#endif

	return !wsi->dns_sorted_list.count;
}
