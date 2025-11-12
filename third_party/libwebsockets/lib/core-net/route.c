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
 * We mainly focus on the routing table / gateways because those are the
 * elements that decide if we can get on to the internet or not.
 *
 * Everything here is _ because the caller needs to hold the pt lock in order
 * to access the pt routing table safely
 */

#include <private-lib-core.h>

#if defined(_DEBUG)



void
_lws_routing_entry_dump(struct lws_context *cx, lws_route_t *rou)
{
	char sa[48], fin[192], *end = &fin[sizeof(fin)];
	char *it = fin;
	int n;

	fin[0] = '\0';

	if (rou->dest.sa4.sin_family) {
		lws_sa46_write_numeric_address(&rou->dest, sa, sizeof(sa));
		n = lws_snprintf(it, lws_ptr_diff_size_t(end, it),
				  "dst: %s/%d, ", sa, rou->dest_len);
		it = it + n;
	}

	if (rou->src.sa4.sin_family) {
		lws_sa46_write_numeric_address(&rou->src, sa, sizeof(sa));
		n = lws_snprintf(it, lws_ptr_diff_size_t(end, it),
				  "src: %s/%d, ", sa, rou->src_len);
		it = it + n;
	}

	if (rou->gateway.sa4.sin_family) {
		lws_sa46_write_numeric_address(&rou->gateway, sa, sizeof(sa));
		n = lws_snprintf(it, lws_ptr_diff_size_t(end, it),
				  "gw: %s, ", sa);
		it = it + n;
	}

	lwsl_cx_info(cx, " %s ifidx: %d, pri: %d, proto: %d\n", fin,
		  rou->if_idx, rou->priority, rou->proto);
}

void
_lws_routing_table_dump(struct lws_context *cx)
{
	lwsl_cx_info(cx, "\n");
	lws_start_foreach_dll(struct lws_dll2 *, d,
			      lws_dll2_get_head(&cx->routing_table)) {
		lws_route_t *rou = lws_container_of(d, lws_route_t, list);

		_lws_routing_entry_dump(cx, rou);
	} lws_end_foreach_dll(d);
}
#endif

/*
 * We will provide a "fingerprint ordinal" as the route uidx that is unique in
 * the routing table.  Wsi that connect mark themselves with the uidx of the
 * route they are estimated to be using.
 *
 * This lets us detect things like gw changes, eg when switching from wlan to
 * lte there may still be a valid gateway route, but all existing tcp
 * connections previously using the wlan gateway will be broken, since their
 * connections are from its gateway to the peer.
 *
 * So when we take down a route, we take care to look for any wsi that was
 * estimated to be using that route, eg, for gateway, and close those wsi.
 *
 * It's OK if the route uidx wraps, we explicitly confirm nobody else is using
 * the uidx before assigning one to a new route.
 *
 * We won't use uidx 0, so it can be understood to mean the uidx was never set.
 */

lws_route_uidx_t
_lws_route_get_uidx(struct lws_context *cx)
{
	lws_route_uidx_t ou;

	if (!cx->route_uidx)
		cx->route_uidx++;

	ou = cx->route_uidx;

	do {
		uint8_t again = 0;

		/* Anybody in the table already uses the pt's next uidx? */

		lws_start_foreach_dll(struct lws_dll2 *, d,
				      lws_dll2_get_head(&cx->routing_table)) {
			lws_route_t *rou = lws_container_of(d, lws_route_t, list);

			if (rou->uidx == cx->route_uidx) {
				/* if so, bump and restart the check */
				cx->route_uidx++;
				if (!cx->route_uidx)
					cx->route_uidx++;
				if (cx->route_uidx == ou) {
					assert(0); /* we have filled up the 8-bit uidx space? */
					return 0;
				}
				again = 1;
				break;
			}
		} lws_end_foreach_dll(d);

		if (!again)
			return cx->route_uidx++;
	} while (1);
}

lws_route_t *
_lws_route_remove(struct lws_context_per_thread *pt, lws_route_t *robj, int flags)
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
			      lws_dll2_get_head(&pt->context->routing_table)) {
		lws_route_t *rou = lws_container_of(d, lws_route_t, list);

		if ((!(flags & LRR_MATCH_SRC) || !lws_sa46_compare_ads(&robj->src, &rou->src)) &&
		    (!(flags & LRR_MATCH_DST) || !lws_sa46_compare_ads(&robj->dest, &rou->dest)) &&
		    (!robj->gateway.sa4.sin_family ||
		     !lws_sa46_compare_ads(&robj->gateway, &rou->gateway)) &&
		    robj->dest_len <= rou->dest_len &&
		    robj->if_idx == rou->if_idx &&
		    ((flags & LRR_IGNORE_PRI) ||
		      robj->priority == rou->priority)
		    ) {
			lwsl_cx_info(pt->context, "deleting route");
			_lws_route_pt_close_route_users(pt, robj->uidx);
			lws_dll2_remove(&rou->list);
			lws_free(rou);
		}

	} lws_end_foreach_dll_safe(d, d1);

	return NULL;
}

void
_lws_route_table_empty(struct lws_context_per_thread *pt)
{

	if (!pt->context)
		return;

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   lws_dll2_get_head(&pt->context->routing_table)) {
		lws_route_t *rou = lws_container_of(d, lws_route_t, list);

		lws_dll2_remove(&rou->list);
		lws_free(rou);

	} lws_end_foreach_dll_safe(d, d1);
}

void
_lws_route_table_ifdown(struct lws_context_per_thread *pt, int idx)
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   lws_dll2_get_head(&pt->context->routing_table)) {
		lws_route_t *rou = lws_container_of(d, lws_route_t, list);

		if (rou->if_idx == idx) {
			lws_dll2_remove(&rou->list);
			lws_free(rou);
		}

	} lws_end_foreach_dll_safe(d, d1);
}

lws_route_t *
_lws_route_est_outgoing(struct lws_context_per_thread *pt,
		        const lws_sockaddr46 *dest)
{
	lws_route_t *best_gw = NULL;
	int best_gw_priority = INT_MAX;

	if (!dest->sa4.sin_family) {
		lwsl_cx_notice(pt->context, "dest has 0 AF");
		/* leave it alone */
		return NULL;
	}

	/*
	 * Given the dest address and the current routing table, select the
	 * route we think it would go out on... if we find a matching network
	 * route, just return that, otherwise find the "best" gateway by
	 * looking at the priority of them.
	 */

	lws_start_foreach_dll(struct lws_dll2 *, d,
			      lws_dll2_get_head(&pt->context->routing_table)) {
		lws_route_t *rou = lws_container_of(d, lws_route_t, list);

		// _lws_routing_entry_dump(rou);

		if (rou->dest.sa4.sin_family &&
		    !lws_sa46_on_net(dest, &rou->dest, rou->dest_len))
			/*
			 * Yes, he has a matching network route, it beats out
			 * any gateway route.  This is like finding a route for
			 * 192.168.0.0/24 when dest is 192.168.0.1.
			 */
			return rou;

		lwsl_cx_debug(pt->context, "dest af %d, rou gw af %d, pri %d",
			      dest->sa4.sin_family, rou->gateway.sa4.sin_family,
			      rou->priority);

		if (rou->gateway.sa4.sin_family &&

			/*
			 *  dest  gw
			 *   4     4    OK
			 *   4     6    OK with ::ffff:x:x
			 *   6     4    not supported directly
			 *   6     6    OK
			 */

		    (dest->sa4.sin_family == rou->gateway.sa4.sin_family ||
			(dest->sa4.sin_family == AF_INET &&
			 rou->gateway.sa4.sin_family == AF_INET6)) &&
		    rou->priority < best_gw_priority) {
			lwsl_cx_info(pt->context, "gw hit");
			best_gw_priority = rou->priority;
			best_gw = rou;
		}

	} lws_end_foreach_dll(d);

	/*
	 * Either best_gw is the best gw route and we set *best_gw_priority to
	 * the best one's priority, or we're returning NULL as no network or
	 * gw route for dest.
	 */

	lwsl_cx_info(pt->context, "returning %p", best_gw);

	return best_gw;
}

/*
 * Determine if the source still exists
 */

lws_route_t *
_lws_route_find_source(struct lws_context_per_thread *pt,
		       const lws_sockaddr46 *src)
{
	lws_start_foreach_dll(struct lws_dll2 *, d,
			      lws_dll2_get_head(&pt->context->routing_table)) {
		lws_route_t *rou = lws_container_of(d, lws_route_t, list);

		// _lws_routing_entry_dump(rou);

		if (rou->src.sa4.sin_family &&
		    !lws_sa46_compare_ads(src, &rou->src))
			/*
			 * Source route still exists
			 */
			return rou;

	} lws_end_foreach_dll(d);

	return NULL;
}

int
_lws_route_check_wsi(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	char buf[72];

	if (!wsi->sa46_peer.sa4.sin_family ||
#if defined(LWS_WITH_UNIX_SOCK)
	     wsi->unix_skt ||
	     wsi->sa46_peer.sa4.sin_family == AF_UNIX ||
#endif
	    wsi->desc.sockfd == LWS_SOCK_INVALID)
		/* not a socket, cannot judge by route, or not connected,
		 * leave it alone */
		return 0; /* OK */

	/* the route to the peer is still workable? */

	if (!_lws_route_est_outgoing(pt, &wsi->sa46_peer)) {
		/* no way to talk to the peer */
		lwsl_wsi_notice(wsi, "dest route gone");
		return 1;
	}

	/* the source address is still workable? */

	lws_sa46_write_numeric_address(&wsi->sa46_local,
				       buf, sizeof(buf));
	//lwsl_notice("%s: %s sa46_local %s fam %d\n", __func__, wsi->lc.gutag,
	//		buf, wsi->sa46_local.sa4.sin_family);

	if (wsi->sa46_local.sa4.sin_family &&
	    !_lws_route_find_source(pt, &wsi->sa46_local)) {

		lws_sa46_write_numeric_address(&wsi->sa46_local,
					       buf, sizeof(buf));
		lwsl_wsi_notice(wsi, "source %s gone", buf);

		return 1;
	}

	lwsl_wsi_debug(wsi, "source + dest OK");

	return 0;
}

int
_lws_route_pt_close_unroutable(struct lws_context_per_thread *pt)
{
	struct lws *wsi;
	unsigned int n;

	if (!pt->context->nl_initial_done
#if defined(LWS_WITH_SYS_STATE)
		       	||
	    pt->context->mgr_system.state < LWS_SYSTATE_IFACE_COLDPLUG
#endif
	)
		return 0;

	lwsl_cx_debug(pt->context, "in");
#if defined(_DEBUG)
	_lws_routing_table_dump(pt->context);
#endif

	for (n = 0; n < pt->fds_count; n++) {
		wsi = wsi_from_fd(pt->context, pt->fds[n].fd);
		if (!wsi)
			continue;

		if (_lws_route_check_wsi(wsi)) {
			lwsl_wsi_info(wsi, "culling wsi");
			lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);
		}
	}

	return 0;
}

int
_lws_route_pt_close_route_users(struct lws_context_per_thread *pt,
				lws_route_uidx_t uidx)
{
	struct lws *wsi;
	unsigned int n;

	if (!uidx)
		return 0;

	lwsl_cx_info(pt->context, "closing users of route %d", uidx);

	for (n = 0; n < pt->fds_count; n++) {
		wsi = wsi_from_fd(pt->context, pt->fds[n].fd);
		if (!wsi)
			continue;

		if (wsi->desc.sockfd != LWS_SOCK_INVALID &&
#if defined(LWS_WITH_UNIX_SOCK)
		    !wsi->unix_skt &&
		    wsi->sa46_peer.sa4.sin_family != AF_UNIX &&
#endif
		    wsi->sa46_peer.sa4.sin_family &&
		    wsi->peer_route_uidx == uidx) {
			lwsl_wsi_notice(wsi, "culling wsi");
			lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);
		}
	}

	return 0;
}
