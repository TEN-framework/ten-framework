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
 */

#include <libwebsockets.h>
#include "private-lib-core.h"

/* requires context->lock */
static void
__lws_peer_remove_from_peer_wait_list(struct lws_context *context,
				      struct lws_peer *peer)
{
	struct lws_peer *df;

	lws_start_foreach_llp(struct lws_peer **, p, context->peer_wait_list) {
		if (*p == peer) {
			df = *p;

			*p = df->peer_wait_list;
			df->peer_wait_list = NULL;

			if (!context->peer_wait_list)
				lws_sul_cancel(&context->pt[0].sul_peer_limits);

			return;
		}
	} lws_end_foreach_llp(p, peer_wait_list);
}

void
lws_sul_peer_limits_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context_per_thread *pt = lws_container_of(sul,
			struct lws_context_per_thread, sul_peer_limits);

	lws_peer_cull_peer_wait_list(pt->context);

	lws_sul_schedule(pt->context, 0, &pt->context->pt[0].sul_peer_limits,
			 lws_sul_peer_limits_cb, 10 * LWS_US_PER_SEC);
}

/* requires context->lock */
static void
__lws_peer_add_to_peer_wait_list(struct lws_context *context,
				 struct lws_peer *peer)
{
	__lws_peer_remove_from_peer_wait_list(context, peer);

	peer->peer_wait_list = context->peer_wait_list;
	context->peer_wait_list = peer;

	if (!context->pt[0].sul_peer_limits.list.owner)
		lws_sul_schedule(context, 0, &context->pt[0].sul_peer_limits,
				lws_sul_peer_limits_cb, 10 * LWS_US_PER_SEC);
}


struct lws_peer *
lws_get_or_create_peer(struct lws_vhost *vhost, lws_sockfd_type sockfd)
{
	struct lws_context *context = vhost->context;
	struct lws_peer *peer;
	lws_sockaddr46 sa46;
	socklen_t rlen = 0;
	uint32_t hash = 0;
	uint8_t *q8;
	void *q;
	int n;

	if (vhost->options & LWS_SERVER_OPTION_UNIX_SOCK)
		return NULL;

	rlen = sizeof(sa46);
	if (getpeername(sockfd, (struct sockaddr*)&sa46, &rlen))
		/* eg, udp doesn't have to have a peer */
		return NULL;

#ifdef LWS_WITH_IPV6
	if (sa46.sa4.sin_family == AF_INET6) {
		q = &sa46.sa6.sin6_addr;
		rlen = sizeof(sa46.sa6.sin6_addr);
	} else
#endif
	{
		q = &sa46.sa4.sin_addr;
		rlen = sizeof(sa46.sa4.sin_addr);
	}

	q8 = q;
	for (n = 0; n < (int)rlen; n++)
		hash = (uint32_t)((((hash << 4) | (hash >> 28)) * (uint32_t)n) ^ q8[n]);

	if (!context->pl_hash_elements)
		return NULL;

	hash = hash % context->pl_hash_elements;

	lws_context_lock(context, "peer search"); /* <======================= */

	lws_start_foreach_ll(struct lws_peer *, peerx,
			     context->pl_hash_table[hash]) {
		if (peerx->sa46.sa4.sin_family == sa46.sa4.sin_family) {
#if defined(LWS_WITH_IPV6)
			if (sa46.sa4.sin_family == AF_INET6 &&
			    !memcmp(q, &peerx->sa46.sa6.sin6_addr, rlen))
				goto hit;
#endif
			if (sa46.sa4.sin_family == AF_INET &&
			    !memcmp(q, &peerx->sa46.sa4.sin_addr, rlen)) {
#if defined(LWS_WITH_IPV6)
hit:
#endif
				lws_context_unlock(context); /* === */

				return peerx;
			}
		}
	} lws_end_foreach_ll(peerx, next);

	lwsl_info("%s: creating new peer\n", __func__);

	peer = lws_zalloc(sizeof(*peer), "peer");
	if (!peer) {
		lws_context_unlock(context); /* === */
		lwsl_err("%s: OOM for new peer\n", __func__);
		return NULL;
	}

	context->count_peers++;
	peer->next = context->pl_hash_table[hash];
	peer->hash = hash;
	peer->sa46 = sa46;
	context->pl_hash_table[hash] = peer;
	time(&peer->time_created);
	/*
	 * On creation, the peer has no wsi attached, so is created on the
	 * wait list.  When a wsi is added it is removed from the wait list.
	 */
	time(&peer->time_closed_all);
	__lws_peer_add_to_peer_wait_list(context, peer);

	lws_context_unlock(context); /* ====================================> */

	return peer;
}

/* requires context->lock */
static int
__lws_peer_destroy(struct lws_context *context, struct lws_peer *peer)
{
	lws_start_foreach_llp(struct lws_peer **, p,
			      context->pl_hash_table[peer->hash]) {
		if (*p == peer) {
			struct lws_peer *df = *p;
			*p = df->next;
			lws_free(df);
			context->count_peers--;

			return 0;
		}
	} lws_end_foreach_llp(p, next);

	return 1;
}

void
lws_peer_cull_peer_wait_list(struct lws_context *context)
{
	struct lws_peer *df;
	time_t t;

	time(&t);

	if (context->next_cull && t < context->next_cull)
		return;

	lws_context_lock(context, "peer cull"); /* <========================= */

	context->next_cull = t + 5;

	lws_start_foreach_llp(struct lws_peer **, p, context->peer_wait_list) {
		if (t - (*p)->time_closed_all > 10) {
			df = *p;

			/* remove us from the peer wait list */
			*p = df->peer_wait_list;
			df->peer_wait_list = NULL;

			__lws_peer_destroy(context, df);
			continue; /* we already point to next, if any */
		}
	} lws_end_foreach_llp(p, peer_wait_list);

	lws_context_unlock(context); /* ====================================> */
}

void
lws_peer_add_wsi(struct lws_context *context, struct lws_peer *peer,
		 struct lws *wsi)
{
	if (!peer)
		return;

	lws_context_lock(context, "peer add"); /* <========================== */

	peer->count_wsi++;
	wsi->peer = peer;
	__lws_peer_remove_from_peer_wait_list(context, peer);

	lws_context_unlock(context); /* ====================================> */
}

void
lws_peer_dump_from_wsi(struct lws *wsi)
{
	struct lws_peer *peer;

	if (!wsi || !wsi->peer)
		return;

	peer = wsi->peer;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	lwsl_notice("%s: %s: created %llu: wsi: %d/%d, ah %d/%d\n",
			__func__, lws_wsi_tag(wsi),
			(unsigned long long)peer->time_created,
			peer->count_wsi, peer->total_wsi,
			peer->http.count_ah, peer->http.total_ah);
#else
	lwsl_notice("%s: %s: created %llu: wsi: %d/%d\n", __func__,
			lws_wsi_tag(wsi),
			(unsigned long long)peer->time_created,
			peer->count_wsi, peer->total_wsi);
#endif
}

void
lws_peer_track_wsi_close(struct lws_context *context, struct lws_peer *peer)
{
	if (!peer)
		return;

	lws_context_lock(context, "peer wsi close"); /* <==================== */

	assert(peer->count_wsi);
	peer->count_wsi--;

	if (!peer->count_wsi
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
			&& !peer->http.count_ah
#endif
	) {
		/*
		 * in order that we can accumulate peer activity correctly
		 * allowing for periods when the peer has no connections,
		 * we don't synchronously destroy the peer when his last
		 * wsi closes.  Instead we mark the time his last wsi
		 * closed and add him to a peer_wait_list to be reaped
		 * later if no further activity is coming.
		 */
		time(&peer->time_closed_all);
		__lws_peer_add_to_peer_wait_list(context, peer);
	}

	lws_context_unlock(context); /* ====================================> */
}

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
int
lws_peer_confirm_ah_attach_ok(struct lws_context *context,
			      struct lws_peer *peer)
{
	if (!peer)
		return 0;

	if (context->ip_limit_ah &&
	    peer->http.count_ah >= context->ip_limit_ah) {
		lwsl_info("peer reached ah limit %d, deferring\n",
				context->ip_limit_ah);

		return 1;
	}

	return 0;
}

void
lws_peer_track_ah_detach(struct lws_context *context, struct lws_peer *peer)
{
	if (!peer)
		return;

	lws_context_lock(context, "peer ah detach"); /* <==================== */
	assert(peer->http.count_ah);
	peer->http.count_ah--;
	lws_context_unlock(context); /* ====================================> */
}
#endif
