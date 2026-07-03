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

static const unsigned char lextable_h1[] = {
	#include "lextable.h"
};

#define FAIL_CHAR 0x08

#if defined(LWS_WITH_CUSTOM_HEADERS)

#define UHO_NLEN	0
#define UHO_VLEN	2
#define UHO_LL		4
#define UHO_NAME	8

#endif

static struct allocated_headers *
_lws_create_ah(struct lws_context_per_thread *pt, ah_data_idx_t data_size)
{
	struct allocated_headers *ah = lws_zalloc(sizeof(*ah), "ah struct");

	if (!ah)
		return NULL;

	ah->data = lws_malloc(data_size, "ah data");
	if (!ah->data) {
		lws_free(ah);

		return NULL;
	}
	ah->next = pt->http.ah_list;
	pt->http.ah_list = ah;
	ah->data_length = data_size;
	pt->http.ah_pool_length++;

	lwsl_info("%s: created ah %p (size %d): pool length %u\n", __func__,
		    ah, (int)data_size, (unsigned int)pt->http.ah_pool_length);

	return ah;
}

int
_lws_destroy_ah(struct lws_context_per_thread *pt, struct allocated_headers *ah)
{
	lws_start_foreach_llp(struct allocated_headers **, a, pt->http.ah_list) {
		if ((*a) == ah) {
			*a = ah->next;
			pt->http.ah_pool_length--;
			lwsl_info("%s: freed ah %p : pool length %u\n",
				    __func__, ah,
				    (unsigned int)pt->http.ah_pool_length);
			if (ah->data)
				lws_free(ah->data);
			lws_free(ah);

			return 0;
		}
	} lws_end_foreach_llp(a, next);

	return 1;
}

void
_lws_header_table_reset(struct allocated_headers *ah)
{
	/* init the ah to reflect no headers or data have appeared yet */
	memset(ah->frag_index, 0, sizeof(ah->frag_index));
	memset(ah->frags, 0, sizeof(ah->frags));
	ah->nfrag = 0;
	ah->pos = 0;
	ah->http_response = 0;
	ah->parser_state = WSI_TOKEN_NAME_PART;
	ah->lextable_pos = 0;
	ah->unk_pos = 0;
#if defined(LWS_WITH_CUSTOM_HEADERS)
	ah->unk_ll_head = 0;
	ah->unk_ll_tail = 0;
#endif
}

// doesn't scrub the ah rxbuffer by default, parent must do if needed

void
__lws_header_table_reset(struct lws *wsi, int autoservice)
{
	struct allocated_headers *ah = wsi->http.ah;
	struct lws_context_per_thread *pt;
	struct lws_pollfd *pfd;

	/* if we have the idea we're resetting 'our' ah, must be bound to one */
	assert(ah);
	/* ah also concurs with ownership */
	assert(ah->wsi == wsi);

	_lws_header_table_reset(ah);

	/* since we will restart the ah, our new headers are not completed */
	wsi->hdr_parsing_completed = 0;

	/* while we hold the ah, keep a timeout on the wsi */
	__lws_set_timeout(wsi, PENDING_TIMEOUT_HOLDING_AH,
			  wsi->a.vhost->timeout_secs_ah_idle);

	time(&ah->assigned);

	if (wsi->position_in_fds_table != LWS_NO_FDS_POS &&
	    lws_buflist_next_segment_len(&wsi->buflist, NULL) &&
	    autoservice) {
		lwsl_debug("%s: service on readbuf ah\n", __func__);

		pt = &wsi->a.context->pt[(int)wsi->tsi];
		/*
		 * Unlike a normal connect, we have the headers already
		 * (or the first part of them anyway)
		 */
		pfd = &pt->fds[wsi->position_in_fds_table];
		pfd->revents |= LWS_POLLIN;
		lwsl_err("%s: calling service\n", __func__);
		lws_service_fd_tsi(wsi->a.context, pfd, wsi->tsi);
	}
}

void
lws_header_table_reset(struct lws *wsi, int autoservice)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	lws_pt_lock(pt, __func__);

	__lws_header_table_reset(wsi, autoservice);

	lws_pt_unlock(pt);
}

static void
_lws_header_ensure_we_are_on_waiting_list(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws_pollargs pa;
	struct lws **pwsi = &pt->http.ah_wait_list;

	while (*pwsi) {
		if (*pwsi == wsi)
			return;
		pwsi = &(*pwsi)->http.ah_wait_list;
	}

	lwsl_info("%s: wsi: %s\n", __func__, lws_wsi_tag(wsi));
	wsi->http.ah_wait_list = pt->http.ah_wait_list;
	pt->http.ah_wait_list = wsi;
	pt->http.ah_wait_list_length++;

	/* we cannot accept input then */

	_lws_change_pollfd(wsi, LWS_POLLIN, 0, &pa);
}

static int
__lws_remove_from_ah_waiting_list(struct lws *wsi)
{
        struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct lws **pwsi =&pt->http.ah_wait_list;

	while (*pwsi) {
		if (*pwsi == wsi) {
			lwsl_info("%s: wsi %s\n", __func__, lws_wsi_tag(wsi));
			/* point prev guy to our next */
			*pwsi = wsi->http.ah_wait_list;
			/* we shouldn't point anywhere now */
			wsi->http.ah_wait_list = NULL;
			pt->http.ah_wait_list_length--;

			return 1;
		}
		pwsi = &(*pwsi)->http.ah_wait_list;
	}

	return 0;
}

int LWS_WARN_UNUSED_RESULT
lws_header_table_attach(struct lws *wsi, int autoservice)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_pollargs pa;
	int n;

#if defined(LWS_ROLE_MQTT) && defined(LWS_WITH_CLIENT)
	if (lwsi_role_mqtt(wsi))
		goto connect_via_info2;
#endif

	lwsl_info("%s: %s: ah %p (tsi %d, count = %d) in\n", __func__,
		  lws_wsi_tag(wsi), (void *)wsi->http.ah, wsi->tsi,
		  pt->http.ah_count_in_use);

	if (!lwsi_role_http(wsi)) {
		lwsl_err("%s: bad role %s\n", __func__, wsi->role_ops->name);
		assert(0);
		return -1;
	}

	lws_pt_lock(pt, __func__);

	/* if we are already bound to one, just clear it down */
	if (wsi->http.ah) {
		lwsl_info("%s: cleardown\n", __func__);
		goto reset;
	}

	n = pt->http.ah_count_in_use == (int)context->max_http_header_pool;
#if defined(LWS_WITH_PEER_LIMITS)
	if (!n)
		n = lws_peer_confirm_ah_attach_ok(context, wsi->peer);
#endif
	if (n) {
		/*
		 * Pool is either all busy, or we don't want to give this
		 * particular guy an ah right now...
		 *
		 * Make sure we are on the waiting list, and return that we
		 * weren't able to provide the ah
		 */
		_lws_header_ensure_we_are_on_waiting_list(wsi);

		goto bail;
	}

	__lws_remove_from_ah_waiting_list(wsi);

	wsi->http.ah = _lws_create_ah(pt, context->max_http_header_data);
	if (!wsi->http.ah) { /* we could not create an ah */
		_lws_header_ensure_we_are_on_waiting_list(wsi);

		goto bail;
	}

	wsi->http.ah->in_use = 1;
	wsi->http.ah->wsi = wsi; /* mark our owner */
	pt->http.ah_count_in_use++;

#if defined(LWS_WITH_PEER_LIMITS) && (defined(LWS_ROLE_H1) || \
    defined(LWS_ROLE_H2))
	lws_context_lock(context, "ah attach"); /* <========================= */
	if (wsi->peer)
		wsi->peer->http.count_ah++;
	lws_context_unlock(context); /* ====================================> */
#endif

	_lws_change_pollfd(wsi, 0, LWS_POLLIN, &pa);

	lwsl_info("%s: did attach wsi %s: ah %p: count %d (on exit)\n", __func__,
		  lws_wsi_tag(wsi), (void *)wsi->http.ah, pt->http.ah_count_in_use);

reset:
	__lws_header_table_reset(wsi, autoservice);

	lws_pt_unlock(pt);

#if defined(LWS_WITH_CLIENT)
#if defined(LWS_ROLE_MQTT)
connect_via_info2:
#endif
	if (lwsi_role_client(wsi) && lwsi_state(wsi) == LRS_UNCONNECTED)
		if (!lws_http_client_connect_via_info2(wsi))
			/* our client connect has failed, the wsi
			 * has been closed
			 */
			return -1;
#endif

	return 0;

bail:
	lws_pt_unlock(pt);

	return 1;
}

int __lws_header_table_detach(struct lws *wsi, int autoservice)
{
	struct lws_context *context = wsi->a.context;
	struct allocated_headers *ah = wsi->http.ah;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	struct lws_pollargs pa;
	struct lws **pwsi, **pwsi_eligible;
	time_t now;

	__lws_remove_from_ah_waiting_list(wsi);

	if (!ah)
		return 0;

	lwsl_info("%s: %s: ah %p (tsi=%d, count = %d)\n", __func__,
		  lws_wsi_tag(wsi), (void *)ah, wsi->tsi,
		  pt->http.ah_count_in_use);

	/* we did have an ah attached */
	time(&now);
	if (ah->assigned && now - ah->assigned > 3) {
		/*
		 * we're detaching the ah, but it was held an
		 * unreasonably long time
		 */
		lwsl_debug("%s: %s: ah held %ds, role/state 0x%lx 0x%x,"
			    "\n", __func__, lws_wsi_tag(wsi),
			    (int)(now - ah->assigned),
			    (unsigned long)lwsi_role(wsi), lwsi_state(wsi));
	}

	ah->assigned = 0;

	/* if we think we're detaching one, there should be one in use */
	assert(pt->http.ah_count_in_use > 0);
	/* and this specific one should have been in use */
	assert(ah->in_use);
	memset(&wsi->http.ah, 0, sizeof(wsi->http.ah));

#if defined(LWS_WITH_PEER_LIMITS)
	if (ah->wsi)
		lws_peer_track_ah_detach(context, wsi->peer);
#endif
	ah->wsi = NULL; /* no owner */
	wsi->http.ah = NULL;

	pwsi = &pt->http.ah_wait_list;

	/* oh there is nobody on the waiting list... leave the ah unattached */
	if (!*pwsi)
		goto nobody_usable_waiting;

	/*
	 * at least one wsi on the same tsi is waiting, give it to oldest guy
	 * who is allowed to take it (if any)
	 */
	lwsl_info("%s: pt wait list %s\n", __func__, lws_wsi_tag(*pwsi));
	wsi = NULL;
	pwsi_eligible = NULL;

	while (*pwsi) {
#if defined(LWS_WITH_PEER_LIMITS)
		/* are we willing to give this guy an ah? */
		if (!lws_peer_confirm_ah_attach_ok(context, (*pwsi)->peer))
#endif
		{
			wsi = *pwsi;
			pwsi_eligible = pwsi;
		}

		pwsi = &(*pwsi)->http.ah_wait_list;
	}

	if (!wsi) /* everybody waiting already has too many ah... */
		goto nobody_usable_waiting;

	lwsl_info("%s: transferring ah to last eligible wsi in wait list "
		  "%s (wsistate 0x%lx)\n", __func__, lws_wsi_tag(wsi),
		  (unsigned long)wsi->wsistate);

	wsi->http.ah = ah;
	ah->wsi = wsi; /* new owner */

	__lws_header_table_reset(wsi, autoservice);
#if defined(LWS_WITH_PEER_LIMITS) && (defined(LWS_ROLE_H1) || \
    defined(LWS_ROLE_H2))
	lws_context_lock(context, "ah detach"); /* <========================= */
	if (wsi->peer)
		wsi->peer->http.count_ah++;
	lws_context_unlock(context); /* ====================================> */
#endif

	/* clients acquire the ah and then insert themselves in fds table... */
	if (wsi->position_in_fds_table != LWS_NO_FDS_POS) {
		lwsl_info("%s: Enabling %s POLLIN\n", __func__, lws_wsi_tag(wsi));

		/* he has been stuck waiting for an ah, but now his wait is
		 * over, let him progress */

		_lws_change_pollfd(wsi, 0, LWS_POLLIN, &pa);
	}

	/* point prev guy to next guy in list instead */
	*pwsi_eligible = wsi->http.ah_wait_list;
	/* the guy who got one is out of the list */
	wsi->http.ah_wait_list = NULL;
	pt->http.ah_wait_list_length--;

#if defined(LWS_WITH_CLIENT)
	if (lwsi_role_client(wsi) && lwsi_state(wsi) == LRS_UNCONNECTED) {
		lws_pt_unlock(pt);

		if (!lws_http_client_connect_via_info2(wsi)) {
			/* our client connect has failed, the wsi
			 * has been closed
			 */

			return -1;
		}
		return 0;
	}
#endif

	assert(!!pt->http.ah_wait_list_length ==
			!!(lws_intptr_t)pt->http.ah_wait_list);
bail:
	lwsl_info("%s: %s: ah %p (tsi=%d, count = %d)\n", __func__,
		  lws_wsi_tag(wsi), (void *)ah, pt->tid, pt->http.ah_count_in_use);

	return 0;

nobody_usable_waiting:
	lwsl_info("%s: nobody usable waiting\n", __func__);
	_lws_destroy_ah(pt, ah);
	pt->http.ah_count_in_use--;

	goto bail;
}

int lws_header_table_detach(struct lws *wsi, int autoservice)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	int n;

	lws_pt_lock(pt, __func__);
	n = __lws_header_table_detach(wsi, autoservice);
	lws_pt_unlock(pt);

	return n;
}

int
lws_hdr_fragment_length(struct lws *wsi, enum lws_token_indexes h, int frag_idx)
{
	int n;

	if (!wsi->http.ah)
		return 0;

	n = wsi->http.ah->frag_index[h];
	if (!n)
		return 0;
	do {
		if (!frag_idx)
			return wsi->http.ah->frags[n].len;
		n = wsi->http.ah->frags[n].nfrag;
	} while (frag_idx-- && n);

	return 0;
}

int lws_hdr_total_length(struct lws *wsi, enum lws_token_indexes h)
{
	int n;
	int len = 0;

	if (!wsi->http.ah)
		return 0;

	n = wsi->http.ah->frag_index[h];
	if (!n)
		return 0;
	do {
		len += wsi->http.ah->frags[n].len;
		n = wsi->http.ah->frags[n].nfrag;

		if (n)
			len++;

	} while (n);

	return len;
}

int lws_hdr_copy_fragment(struct lws *wsi, char *dst, int len,
				      enum lws_token_indexes h, int frag_idx)
{
	int n = 0;
	int f;

	if (!wsi->http.ah)
		return -1;

	f = wsi->http.ah->frag_index[h];

	if (!f)
		return -1;

	while (n < frag_idx) {
		f = wsi->http.ah->frags[f].nfrag;
		if (!f)
			return -1;
		n++;
	}

	if (wsi->http.ah->frags[f].len >= len)
		return -1;

	memcpy(dst, wsi->http.ah->data + wsi->http.ah->frags[f].offset,
	       wsi->http.ah->frags[f].len);
	dst[wsi->http.ah->frags[f].len] = '\0';

	return wsi->http.ah->frags[f].len;
}

int lws_hdr_copy(struct lws *wsi, char *dst, int len,
			     enum lws_token_indexes h)
{
	int toklen = lws_hdr_total_length(wsi, h), n, comma;

	*dst = '\0';
	if (!toklen)
		return 0;

	if (toklen >= len)
		return -1;

	if (!wsi->http.ah)
		return -1;

	n = wsi->http.ah->frag_index[h];
	if (!n)
		return 0;
	do {
		comma = (wsi->http.ah->frags[n].nfrag) ? 1 : 0;

		if (h == WSI_TOKEN_HTTP_URI_ARGS)
			lwsl_notice("%s: WSI_TOKEN_HTTP_URI_ARGS '%.*s'\n",
				    __func__, (int)wsi->http.ah->frags[n].len,
				    &wsi->http.ah->data[
				                wsi->http.ah->frags[n].offset]);

		if (wsi->http.ah->frags[n].len + comma >= len) {
			lwsl_notice("blowout len\n");
			return -1;
		}
		strncpy(dst, &wsi->http.ah->data[wsi->http.ah->frags[n].offset],
		        wsi->http.ah->frags[n].len);
		dst += wsi->http.ah->frags[n].len;
		len -= wsi->http.ah->frags[n].len;
		n = wsi->http.ah->frags[n].nfrag;

		/*
		 * Note if you change this logic, take care about updating len
		 * and make sure lws_hdr_total_length() gives the same resulting
		 * length
		 */

		if (comma) {
			if (h == WSI_TOKEN_HTTP_COOKIE ||
			    h == WSI_TOKEN_HTTP_SET_COOKIE)
				*dst++ = ';';
			else
				if (h == WSI_TOKEN_HTTP_URI_ARGS)
					*dst++ = '&';
				else
					*dst++ = ',';
			len--;
		}
				
	} while (n);
	*dst = '\0';

	if (h == WSI_TOKEN_HTTP_URI_ARGS)
		lwsl_err("%s: WSI_TOKEN_HTTP_URI_ARGS toklen %d\n", __func__, (int)toklen);

	return toklen;
}

#if defined(LWS_WITH_CUSTOM_HEADERS)
int
lws_hdr_custom_length(struct lws *wsi, const char *name, int nlen)
{
	ah_data_idx_t ll;

	if (!wsi->http.ah || wsi->mux_substream)
		return -1;

	ll = wsi->http.ah->unk_ll_head;
	while (ll) {
		if (ll >= wsi->http.ah->data_length)
			return -1;
		if (nlen == lws_ser_ru16be(
			(uint8_t *)&wsi->http.ah->data[ll + UHO_NLEN]) &&
		    !strncmp(name, &wsi->http.ah->data[ll + UHO_NAME], (unsigned int)nlen))
			return lws_ser_ru16be(
				(uint8_t *)&wsi->http.ah->data[ll + UHO_VLEN]);

		ll = lws_ser_ru32be((uint8_t *)&wsi->http.ah->data[ll + UHO_LL]);
	}

	return -1;
}

int
lws_hdr_custom_copy(struct lws *wsi, char *dst, int len, const char *name,
		    int nlen)
{
	ah_data_idx_t ll;
	int n;

	if (!wsi->http.ah || wsi->mux_substream)
		return -1;

	*dst = '\0';

	ll = wsi->http.ah->unk_ll_head;
	while (ll) {
		if (ll >= wsi->http.ah->data_length)
			return -1;
		if (nlen == lws_ser_ru16be(
			(uint8_t *)&wsi->http.ah->data[ll + UHO_NLEN]) &&
		    !strncmp(name, &wsi->http.ah->data[ll + UHO_NAME], (unsigned int)nlen)) {
			n = lws_ser_ru16be(
				(uint8_t *)&wsi->http.ah->data[ll + UHO_VLEN]);
			if (n + 1 > len)
				return -1;
			strncpy(dst, &wsi->http.ah->data[ll + UHO_NAME + (unsigned int)nlen], (unsigned int)n);
			dst[n] = '\0';

			return n;
		}
		ll = lws_ser_ru32be((uint8_t *)&wsi->http.ah->data[ll + UHO_LL]);
	}

	return -1;
}

int
lws_hdr_custom_name_foreach(struct lws *wsi, lws_hdr_custom_fe_cb_t cb,
			    void *custom)
{
	ah_data_idx_t ll;

	if (!wsi->http.ah || wsi->mux_substream)
		return -1;

	ll = wsi->http.ah->unk_ll_head;

	while (ll) {
		if (ll >= wsi->http.ah->data_length)
			return -1;

		cb(&wsi->http.ah->data[ll + UHO_NAME],
		   lws_ser_ru16be((uint8_t *)&wsi->http.ah->data[ll + UHO_NLEN]),
		   custom);

		ll = lws_ser_ru32be((uint8_t *)&wsi->http.ah->data[ll + UHO_LL]);
	}

	return 0;
}
#endif

char *lws_hdr_simple_ptr(struct lws *wsi, enum lws_token_indexes h)
{
	int n;

	if (!wsi->http.ah)
		return NULL;

	n = wsi->http.ah->frag_index[h];
	if (!n)
		return NULL;

	return wsi->http.ah->data + wsi->http.ah->frags[n].offset;
}

static int LWS_WARN_UNUSED_RESULT
lws_pos_in_bounds(struct lws *wsi)
{
	if (!wsi->http.ah)
		return -1;

	if (wsi->http.ah->pos <
	    (unsigned int)wsi->a.context->max_http_header_data)
		return 0;

	if ((int)wsi->http.ah->pos >= (int)wsi->a.context->max_http_header_data - 1) {
		lwsl_err("Ran out of header data space\n");
		return 1;
	}

	/*
	 * with these tests everywhere, it should never be able to exceed
	 * the limit, only meet it
	 */
	lwsl_err("%s: pos %ld, limit %ld\n", __func__,
		 (unsigned long)wsi->http.ah->pos,
		 (unsigned long)wsi->a.context->max_http_header_data);
	assert(0);

	return 1;
}

int LWS_WARN_UNUSED_RESULT
lws_hdr_simple_create(struct lws *wsi, enum lws_token_indexes h, const char *s)
{
	if (!*s) {
		/*
		 * If we get an empty string, then remove any entry for the
		 * header
		 */
		wsi->http.ah->frag_index[h] = 0;

		return 0;
	}

	wsi->http.ah->nfrag++;
	if (wsi->http.ah->nfrag == LWS_ARRAY_SIZE(wsi->http.ah->frags)) {
		lwsl_warn("More hdr frags than we can deal with, dropping\n");
		return -1;
	}

	wsi->http.ah->frag_index[h] = wsi->http.ah->nfrag;

	wsi->http.ah->frags[wsi->http.ah->nfrag].offset = wsi->http.ah->pos;
	wsi->http.ah->frags[wsi->http.ah->nfrag].len = 0;
	wsi->http.ah->frags[wsi->http.ah->nfrag].nfrag = 0;

	do {
		if (lws_pos_in_bounds(wsi))
			return -1;

		wsi->http.ah->data[wsi->http.ah->pos++] = *s;
		if (*s)
			wsi->http.ah->frags[wsi->http.ah->nfrag].len++;
	} while (*s++);

	return 0;
}

static int LWS_WARN_UNUSED_RESULT
issue_char(struct lws *wsi, unsigned char c)
{
	unsigned short frag_len;

	if (lws_pos_in_bounds(wsi))
		return -1;

	frag_len = wsi->http.ah->frags[wsi->http.ah->nfrag].len;
	/*
	 * If we haven't hit the token limit, just copy the character into
	 * the header
	 */
	if (!wsi->http.ah->current_token_limit ||
	    frag_len < wsi->http.ah->current_token_limit) {
		wsi->http.ah->data[wsi->http.ah->pos++] = (char)c;
		wsi->http.ah->frags[wsi->http.ah->nfrag].len++;
		return 0;
	}

	/* Insert a null character when we *hit* the limit: */
	if (frag_len == wsi->http.ah->current_token_limit) {
		if (lws_pos_in_bounds(wsi))
			return -1;

		wsi->http.ah->data[wsi->http.ah->pos++] = '\0';
		lwsl_warn("header %li exceeds limit %ld\n",
			  (long)wsi->http.ah->parser_state,
			  (long)wsi->http.ah->current_token_limit);
	}

	return 1;
}

int
lws_parse_urldecode(struct lws *wsi, uint8_t *_c)
{
	struct allocated_headers *ah = wsi->http.ah;
	unsigned int enc = 0;
	uint8_t c = *_c;

	// lwsl_notice("ah->ups %d\n", ah->ups);

	/*
	 * PRIORITY 1
	 * special URI processing... convert %xx
	 */
	switch (ah->ues) {
	case URIES_IDLE:
		if (c == '%') {
			ah->ues = URIES_SEEN_PERCENT;
			goto swallow;
		}
		break;
	case URIES_SEEN_PERCENT:
		if (char_to_hex((char)c) < 0)
			/* illegal post-% char */
			goto forbid;

		ah->esc_stash = (char)c;
		ah->ues = URIES_SEEN_PERCENT_H1;
		goto swallow;

	case URIES_SEEN_PERCENT_H1:
		if (char_to_hex((char)c) < 0)
			/* illegal post-% char */
			goto forbid;

		*_c = (uint8_t)(unsigned int)((char_to_hex(ah->esc_stash) << 4) |
				char_to_hex((char)c));
		c = *_c;
		enc = 1;
		ah->ues = URIES_IDLE;
		break;
	}

	/*
	 * PRIORITY 2
	 * special URI processing...
	 *  convert /.. or /... or /../ etc to /
	 *  convert /./ to /
	 *  convert // or /// etc to /
	 *  leave /.dir or whatever alone
	 */

	if (!c && (!ah->frag_index[WSI_TOKEN_HTTP_URI_ARGS] ||
		   !ah->post_literal_equal)) {
		/*
		 * Since user code is typically going to parse the path using
		 * NUL-terminated apis, it's too dangerous to allow NUL
		 * injection here.
		 *
		 * It's allowed in the urlargs, because the apis to access
		 * those only allow retreival with explicit length.
		 */
		lwsl_warn("%s: saw NUL outside of uri args\n", __func__);
		return -1;
	}

	switch (ah->ups) {
	case URIPS_IDLE:

		/* genuine delimiter */
		if ((c == '&' || c == ';') && !enc) {
			if (issue_char(wsi, '\0') < 0)
				return -1;
			/* don't account for it */
			wsi->http.ah->frags[wsi->http.ah->nfrag].len--;
			/* link to next fragment */
			ah->frags[ah->nfrag].nfrag = (uint8_t)(ah->nfrag + 1);
			ah->nfrag++;
			if (ah->nfrag >= LWS_ARRAY_SIZE(ah->frags))
				goto excessive;
			/* start next fragment after the & */
			ah->post_literal_equal = 0;
			ah->frags[ah->nfrag].offset = ++ah->pos;
			ah->frags[ah->nfrag].len = 0;
			ah->frags[ah->nfrag].nfrag = 0;
			goto swallow;
		}
		/* uriencoded = in the name part, disallow */
		if (c == '=' && enc &&
		    ah->frag_index[WSI_TOKEN_HTTP_URI_ARGS] &&
		    !ah->post_literal_equal) {
			c = '_';
			*_c =c;
		}

		/* after the real =, we don't care how many = */
		if (c == '=' && !enc)
			ah->post_literal_equal = 1;

		/* + to space */
		if (c == '+' && !enc) {
			c = ' ';
			*_c = c;
		}
		/* issue the first / always */
		if (c == '/' && !ah->frag_index[WSI_TOKEN_HTTP_URI_ARGS])
			ah->ups = URIPS_SEEN_SLASH;
		break;
	case URIPS_SEEN_SLASH:
		/* swallow subsequent slashes */
		if (c == '/')
			goto swallow;
		/* track and swallow the first . after / */
		if (c == '.') {
			ah->ups = URIPS_SEEN_SLASH_DOT;
			goto swallow;
		}
		ah->ups = URIPS_IDLE;
		break;
	case URIPS_SEEN_SLASH_DOT:
		/* swallow second . */
		if (c == '.') {
			ah->ups = URIPS_SEEN_SLASH_DOT_DOT;
			goto swallow;
		}
		/* change /./ to / */
		if (c == '/') {
			ah->ups = URIPS_SEEN_SLASH;
			goto swallow;
		}
		/* it was like /.dir ... regurgitate the . */
		ah->ups = URIPS_IDLE;
		if (issue_char(wsi, '.') < 0)
			return -1;
		break;

	case URIPS_SEEN_SLASH_DOT_DOT:

		/* /../ or /..[End of URI] --> backup to last / */
		if (c == '/' || c == '?') {
			/*
			 * back up one dir level if possible
			 * safe against header fragmentation because
			 * the method URI can only be in 1 fragment
			 */
			if (ah->frags[ah->nfrag].len > 2) {
				ah->pos--;
				ah->frags[ah->nfrag].len--;
				do {
					ah->pos--;
					ah->frags[ah->nfrag].len--;
				} while (ah->frags[ah->nfrag].len > 1 &&
					 ah->data[ah->pos] != '/');
			}
			ah->ups = URIPS_SEEN_SLASH;
			if (ah->frags[ah->nfrag].len > 1)
				break;
			goto swallow;
		}

		/*  /..[^/] ... regurgitate and allow */

		if (issue_char(wsi, '.') < 0)
			return -1;
		if (issue_char(wsi, '.') < 0)
			return -1;
		ah->ups = URIPS_IDLE;
		break;
	}

	if (c == '?' && !enc &&
	    !ah->frag_index[WSI_TOKEN_HTTP_URI_ARGS]) { /* start of URI args */
		if (ah->ues != URIES_IDLE)
			goto forbid;

		/* seal off uri header */
		if (issue_char(wsi, '\0') < 0)
			return -1;

		/* don't account for it */
		wsi->http.ah->frags[wsi->http.ah->nfrag].len--;

		/* move to using WSI_TOKEN_HTTP_URI_ARGS */
		ah->nfrag++;
		if (ah->nfrag >= LWS_ARRAY_SIZE(ah->frags))
			goto excessive;
		ah->frags[ah->nfrag].offset = ++ah->pos;
		ah->frags[ah->nfrag].len = 0;
		ah->frags[ah->nfrag].nfrag = 0;

		ah->post_literal_equal = 0;
		ah->frag_index[WSI_TOKEN_HTTP_URI_ARGS] = ah->nfrag;
		ah->ups = URIPS_IDLE;
		goto swallow;
	}

	return LPUR_CONTINUE;

swallow:
	return LPUR_SWALLOW;

forbid:
	return LPUR_FORBID;

excessive:
	return LPUR_EXCESSIVE;
}

static const unsigned char methods[] = {
	WSI_TOKEN_GET_URI,
	WSI_TOKEN_POST_URI,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS)
	WSI_TOKEN_OPTIONS_URI,
	WSI_TOKEN_PUT_URI,
	WSI_TOKEN_PATCH_URI,
	WSI_TOKEN_DELETE_URI,
#endif
	WSI_TOKEN_CONNECT,
	WSI_TOKEN_HEAD_URI,
};

/*
 * possible returns:, -1 fail, 0 ok or 2, transition to raw
 */

lws_parser_return_t LWS_WARN_UNUSED_RESULT
lws_parse(struct lws *wsi, unsigned char *buf, int *len)
{
	struct allocated_headers *ah = wsi->http.ah;
	struct lws_context *context = wsi->a.context;
	unsigned int n, m;
	unsigned char c;
	int r, pos;

	assert(wsi->http.ah);

	do {
		(*len)--;
		c = *buf++;

		switch (ah->parser_state) {
#if defined(LWS_WITH_CUSTOM_HEADERS)
		case WSI_TOKEN_UNKNOWN_VALUE_PART:

			if (c == '\r')
				break;
			if (c == '\n') {
				lws_ser_wu16be((uint8_t *)&ah->data[ah->unk_pos + 2],
					       (uint16_t)(ah->pos - ah->unk_value_pos));
				ah->parser_state = WSI_TOKEN_NAME_PART;
				ah->unk_pos = 0;
				ah->lextable_pos = 0;
				break;
			}

			/* trim leading whitespace */
			if (ah->pos != ah->unk_value_pos ||
			    (c != ' ' && c != '\t')) {

				if (lws_pos_in_bounds(wsi))
					return LPR_FAIL;

				ah->data[ah->pos++] = (char)c;
			}
			pos = ah->lextable_pos;
			break;
#endif
		default:

			lwsl_parser("WSI_TOK_(%d) '%c'\n", ah->parser_state, c);

			/* collect into malloc'd buffers */
			/* optional initial space swallow */
			if (!ah->frags[ah->frag_index[ah->parser_state]].len &&
			    c == ' ')
				break;

			for (m = 0; m < LWS_ARRAY_SIZE(methods); m++)
				if (ah->parser_state == methods[m])
					break;
			if (m == LWS_ARRAY_SIZE(methods))
				/* it was not any of the methods */
				goto check_eol;

			/* special URI processing... end at space */

			if (c == ' ') {
				/* enforce starting with / */
				if (!ah->frags[ah->nfrag].len)
					if (issue_char(wsi, '/') < 0)
						return LPR_FAIL;

				if (ah->ups == URIPS_SEEN_SLASH_DOT_DOT) {
					/*
					 * back up one dir level if possible
					 * safe against header fragmentation
					 * because the method URI can only be
					 * in 1 fragment
					 */
					if (ah->frags[ah->nfrag].len > 2) {
						ah->pos--;
						ah->frags[ah->nfrag].len--;
						do {
							ah->pos--;
							ah->frags[ah->nfrag].len--;
						} while (ah->frags[ah->nfrag].len > 1 &&
							 ah->data[ah->pos] != '/');
					}
				}

				/* begin parsing HTTP version: */
				if (issue_char(wsi, '\0') < 0)
					return LPR_FAIL;
				/* don't account for it */
				wsi->http.ah->frags[wsi->http.ah->nfrag].len--;
				ah->parser_state = WSI_TOKEN_HTTP;
				goto start_fragment;
			}

			r = lws_parse_urldecode(wsi, &c);
			switch (r) {
			case LPUR_CONTINUE:
				break;
			case LPUR_SWALLOW:
				goto swallow;
			case LPUR_FORBID:
				goto forbid;
			case LPUR_EXCESSIVE:
				goto excessive;
			default:
				return LPR_FAIL;
			}
check_eol:
			/* bail at EOL */
			if (ah->parser_state != WSI_TOKEN_CHALLENGE &&
			    (c == '\x0d' || c == '\x0a')) {
				if (ah->ues != URIES_IDLE)
					goto forbid;

				if (c == '\x0a') {
					/* broken peer */
					ah->parser_state = WSI_TOKEN_NAME_PART;
					ah->unk_pos = 0;
					ah->lextable_pos = 0;
				} else
					ah->parser_state = WSI_TOKEN_SKIPPING_SAW_CR;

				c = '\0';
				lwsl_parser("*\n");
			}

			n = (unsigned int)issue_char(wsi, c);
			if ((int)n < 0)
				return LPR_FAIL;
			if (n > 0)
				ah->parser_state = WSI_TOKEN_SKIPPING;
			else {
				/*
				 * Explicit zeroes are legal in URI ARGS.
				 * They can only exist as a safety terminator
				 * after the valid part of the token contents
				 * for other types.
				 */
				if (!c && ah->parser_state != WSI_TOKEN_HTTP_URI_ARGS)
					/* don't account for safety terminator */
					wsi->http.ah->frags[wsi->http.ah->nfrag].len--;
			}

swallow:
			/* per-protocol end of headers management */

			if (ah->parser_state == WSI_TOKEN_CHALLENGE)
				goto set_parsing_complete;
			break;

			/* collecting and checking a name part */
		case WSI_TOKEN_NAME_PART:
			lwsl_parser("WSI_TOKEN_NAME_PART '%c' 0x%02X "
				    "(role=0x%lx) "
				    "wsi->lextable_pos=%d\n", c, c,
				    (unsigned long)lwsi_role(wsi),
				    ah->lextable_pos);

			if (!ah->unk_pos && c == '\x0a')
				/* broken peer */
				goto set_parsing_complete;

			if (c >= 'A' && c <= 'Z')
				c = (unsigned char)(c + 'a' - 'A');
			/*
			 * ...in case it's an unknown header, speculatively
			 * store it as the name comes in.  If we recognize it as
			 * a known header, we'll snip this.
			 */

			if (!wsi->mux_substream && !ah->unk_pos) {
				ah->unk_pos = ah->pos;

#if defined(LWS_WITH_CUSTOM_HEADERS)
				/*
				 * Prepare new unknown header linked-list entry
				 *
				 *  - 16-bit BE: name part length
				 *  - 16-bit BE: value part length
				 *  - 32-bit BE: data offset of next, or 0
				 */
				for (n = 0; n < 8; n++)
					if (!lws_pos_in_bounds(wsi))
						ah->data[ah->pos++] = 0;
#endif
			}

			if (lws_pos_in_bounds(wsi))
				return LPR_FAIL;

			ah->data[ah->pos++] = (char)c;
			pos = ah->lextable_pos;

#if defined(LWS_WITH_CUSTOM_HEADERS)
			if (!wsi->mux_substream && pos < 0 && c == ':') {
#if defined(_DEBUG)
				char dotstar[64];
				int uhlen;
#endif

				/*
				 * process unknown headers
				 *
				 * register us in the unknown hdr ll
				 */

				if (!ah->unk_ll_head)
					ah->unk_ll_head = ah->unk_pos;

				if (ah->unk_ll_tail)
					lws_ser_wu32be(
				(uint8_t *)&ah->data[ah->unk_ll_tail + UHO_LL],
						       ah->unk_pos);

				ah->unk_ll_tail = ah->unk_pos;

#if defined(_DEBUG)
				uhlen = (int)(ah->pos - (ah->unk_pos + UHO_NAME));
				lws_strnncpy(dotstar,
					&ah->data[ah->unk_pos + UHO_NAME],
					uhlen, sizeof(dotstar));
				lwsl_debug("%s: unk header %d '%s'\n",
					    __func__,
					    ah->pos - (ah->unk_pos + UHO_NAME),
					    dotstar);
#endif

				/* set the unknown header name part length */

				lws_ser_wu16be((uint8_t *)&ah->data[ah->unk_pos],
					       (uint16_t)((ah->pos - ah->unk_pos) - UHO_NAME));

				ah->unk_value_pos = ah->pos;

				/*
				 * collect whatever's coming for the unknown header
				 * argument until the next CRLF
				 */
				ah->parser_state = WSI_TOKEN_UNKNOWN_VALUE_PART;
				break;
			}
#endif
			if (pos < 0)
				break;

			while (1) {
				if (lextable_h1[pos] & (1 << 7)) {
					/* 1-byte, fail on mismatch */
					if ((lextable_h1[pos] & 0x7f) != c) {
nope:
						ah->lextable_pos = -1;
						break;
					}
					/* fall thru */
					pos++;
					if (lextable_h1[pos] == FAIL_CHAR)
						goto nope;

					ah->lextable_pos = (int16_t)pos;
					break;
				}

				if (lextable_h1[pos] == FAIL_CHAR)
					goto nope;

				/* b7 = 0, end or 3-byte */
				if (lextable_h1[pos] < FAIL_CHAR) {
					if (!wsi->mux_substream) {
						/*
						 * We hit a terminal marker, so
						 * we recognized this header...
						 * drop the speculative name
						 * part storage
						 */
						ah->pos = ah->unk_pos;
						ah->unk_pos = 0;
					}

					ah->lextable_pos = (int16_t)pos;
					break;
				}

				if (lextable_h1[pos] == c) { /* goto */
					ah->lextable_pos = (int16_t)(pos +
						(lextable_h1[pos + 1]) +
						(lextable_h1[pos + 2] << 8));
					break;
				}

				/* fall thru goto */
				pos += 3;
				/* continue */
			}

			/*
			 * If it's h1, server needs to be on the look out for
			 * unknown methods...
			 */
			if (ah->lextable_pos < 0 && lwsi_role_h1(wsi) &&
			    lwsi_role_server(wsi)) {
				/*
				 * this is not a header we know about... did
				 * we get a valid method (GET, POST etc)
				 * already, or is this the bogus method?
				 */
				for (m = 0; m < LWS_ARRAY_SIZE(methods); m++)
					if (ah->frag_index[methods[m]]) {
						/*
						 * already had the method
						 */
#if !defined(LWS_WITH_CUSTOM_HEADERS)
						ah->parser_state = WSI_TOKEN_SKIPPING;
#endif
						if (wsi->mux_substream)
							ah->parser_state = WSI_TOKEN_SKIPPING;
						break;
					}

				if (m != LWS_ARRAY_SIZE(methods)) {
#if defined(LWS_WITH_CUSTOM_HEADERS)
					/*
					 * We have the method, this is just an
					 * unknown header then
					 */
					if (!wsi->mux_substream)
						goto unknown_hdr;
					else
						break;
#else
					break;
#endif
				}
				/*
				 * ...it's an unknown http method from a client
				 * in fact, it cannot be valid http.
				 *
				 * Are we set up to transition to another role
				 * in these cases?
				 */
				if (lws_check_opt(wsi->a.vhost->options,
		    LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG)) {
					lwsl_notice("%s: http fail fallback\n",
						    __func__);
					 /* transition to other role */
					return LPR_DO_FALLBACK;
				}

				lwsl_info("Unknown method - dropping\n");
				goto forbid;
			}
			if (ah->lextable_pos < 0) {
				/*
				 * It's not a header that lws knows about...
				 */
#if defined(LWS_WITH_CUSTOM_HEADERS)
				if (!wsi->mux_substream)
					goto unknown_hdr;
#endif
				/*
				 * ...otherwise for a client, let him ignore
				 * unknown headers coming from the server
				 */
				ah->parser_state = WSI_TOKEN_SKIPPING;
				break;
			}

			if (lextable_h1[ah->lextable_pos] < FAIL_CHAR) {
				/* terminal state */

				n = ((unsigned int)lextable_h1[ah->lextable_pos] << 8) |
						lextable_h1[ah->lextable_pos + 1];

				lwsl_parser("known hdr %d\n", n);
				for (m = 0; m < LWS_ARRAY_SIZE(methods); m++)
					if (n == methods[m] &&
					    ah->frag_index[methods[m]]) {
						lwsl_warn("Duplicated method\n");
						return LPR_FAIL;
					}

				if (!wsi->mux_substream) {
					/*
					 * Whether we are collecting unknown names or not,
					 * if we matched an internal header we can dispense
					 * with the header name part we were keeping
					 */
					ah->pos = ah->unk_pos;
					ah->unk_pos = 0;
				}

#if defined(LWS_ROLE_WS)
				/*
				 * WSORIGIN is protocol equiv to ORIGIN,
				 * JWebSocket likes to send it, map to ORIGIN
				 */
				if (n == WSI_TOKEN_SWORIGIN)
					n = WSI_TOKEN_ORIGIN;
#endif

				ah->parser_state = (uint8_t)
							(WSI_TOKEN_GET_URI + n);
				ah->ups = URIPS_IDLE;

				if (context->token_limits)
					ah->current_token_limit = context->
						token_limits->token_limit[
							      ah->parser_state];
				else
					ah->current_token_limit =
						wsi->a.context->max_http_header_data;

				if (ah->parser_state == WSI_TOKEN_CHALLENGE)
					goto set_parsing_complete;

				goto start_fragment;
			}
			break;

#if defined(LWS_WITH_CUSTOM_HEADERS)
unknown_hdr:
			//ah->parser_state = WSI_TOKEN_SKIPPING;
			//break;
			if (!wsi->mux_substream)
				break;
#endif

start_fragment:
			ah->nfrag++;
excessive:
			if (ah->nfrag == LWS_ARRAY_SIZE(ah->frags)) {
				lwsl_warn("More hdr frags than we can deal with\n");
				return LPR_FAIL;
			}

			ah->frags[ah->nfrag].offset = ah->pos;
			ah->frags[ah->nfrag].len = 0;
			ah->frags[ah->nfrag].nfrag = 0;
			ah->frags[ah->nfrag].flags = 2;

			n = ah->frag_index[ah->parser_state];
			if (!n) { /* first fragment */
				ah->frag_index[ah->parser_state] = ah->nfrag;
				ah->hdr_token_idx = ah->parser_state;
				break;
			}
			/* continuation */
			while (ah->frags[n].nfrag)
				n = ah->frags[n].nfrag;
			ah->frags[n].nfrag = ah->nfrag;

			if (issue_char(wsi, ' ') < 0)
				return LPR_FAIL;
			break;

			/* skipping arg part of a name we didn't recognize */
		case WSI_TOKEN_SKIPPING:
			lwsl_parser("WSI_TOKEN_SKIPPING '%c'\n", c);

			if (c == '\x0a') {
				/* broken peer */
				ah->parser_state = WSI_TOKEN_NAME_PART;
				ah->unk_pos = 0;
				ah->lextable_pos = 0;
			}

			if (c == '\x0d')
				ah->parser_state = WSI_TOKEN_SKIPPING_SAW_CR;
			break;

		case WSI_TOKEN_SKIPPING_SAW_CR:
			lwsl_parser("WSI_TOKEN_SKIPPING_SAW_CR '%c'\n", c);
			if (ah->ues != URIES_IDLE)
				goto forbid;
			if (c == '\x0a') {
				ah->parser_state = WSI_TOKEN_NAME_PART;
				ah->unk_pos = 0;
				ah->lextable_pos = 0;
			} else
				ah->parser_state = WSI_TOKEN_SKIPPING;
			break;
			/* we're done, ignore anything else */

		case WSI_PARSING_COMPLETE:
			lwsl_parser("WSI_PARSING_COMPLETE '%c'\n", c);
			break;
		}

	} while (*len);

	return LPR_OK;

set_parsing_complete:
	if (ah->ues != URIES_IDLE)
		goto forbid;

	if (lws_hdr_total_length(wsi, WSI_TOKEN_UPGRADE)) {
#if defined(LWS_ROLE_WS)
		const char *pv = lws_hdr_simple_ptr(wsi, WSI_TOKEN_VERSION);
		if (pv)
			wsi->rx_frame_type = (char)atoi(pv);

		lwsl_parser("v%02d hdrs done\n", wsi->rx_frame_type);
#endif
	}
	ah->parser_state = WSI_PARSING_COMPLETE;
	wsi->hdr_parsing_completed = 1;

	return LPR_OK;

forbid:
	lwsl_info(" forbidding on uri sanitation\n");
#if defined(LWS_WITH_SERVER)
	lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
#endif

	return LPR_FORBIDDEN;
}

int
lws_http_cookie_get(struct lws *wsi, const char *name, char *buf,
		    size_t *max_len)
{
	size_t max = *max_len, bl = strlen(name);
	char *p, *bo = buf;
	int n;

	n = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COOKIE);
	if ((unsigned int)n < bl + 1)
		return 1;

	/*
	 * This can come to us two ways, in ah fragments (h2) or as a single
	 * semicolon-delimited string (h1)
	 */

#if defined(LWS_ROLE_H2)
	if (lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COLON_METHOD)) {

		/*
		 * The h2 way...
		 */

		int f = wsi->http.ah->frag_index[WSI_TOKEN_HTTP_COOKIE];
		size_t fl;

		while (f) {
			p = wsi->http.ah->data + wsi->http.ah->frags[f].offset;
			fl = (size_t)wsi->http.ah->frags[f].len;
			if (fl >= bl + 1 &&
			    p[bl] == '=' &&
			    !memcmp(p, name, bl)) {
				fl -= bl + 1;
				if (max - 1 < fl)
					fl = max - 1;
				if (fl)
					memcpy(buf, p + bl + 1, fl);
				*max_len = fl;
				buf[fl] = '\0';

				return 0;
			}
			f = wsi->http.ah->frags[f].nfrag;
		}

		return -1;
	}
#endif

	/*
	 * The h1 way...
	 */

	p = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COOKIE);
	if (!p)
		return 1;

	p += bl;
	n -= (int)bl;
	while (n-- > 0) {
		if (*p == '=' && !memcmp(p - bl, name, (unsigned int)bl)) {
			p++;
			while (*p != ';' && n-- && max) {
				*buf++ = *p++;
				max--;
			}
			if (!max)
				return 2;

			*buf = '\0';
			*max_len = lws_ptr_diff_size_t(buf, bo);

			return 0;
		}
		p++;
	}

	return 1;
}

#if defined(LWS_WITH_JOSE)

#define MAX_JWT_SIZE 1024

int
lws_jwt_get_http_cookie_validate_jwt(struct lws *wsi,
				     struct lws_jwt_sign_set_cookie *i,
				     char *out, size_t *out_len)
{
	char temp[MAX_JWT_SIZE * 2];
	size_t cml = *out_len;
	const char *cp;

	/* first use out to hold the encoded JWT */

	if (lws_http_cookie_get(wsi, i->cookie_name, out, out_len)) {
		lwsl_debug("%s: cookie %s not provided\n", __func__,
				i->cookie_name);
		return 1;
	}

	/* decode the JWT into temp */

	if (lws_jwt_signed_validate(wsi->a.context, i->jwk, i->alg, out,
				    *out_len, temp, sizeof(temp), out, &cml)) {
		lwsl_info("%s: jwt validation failed\n", __func__);
		return 1;
	}

	/*
	 * Copy out the decoded JWT payload into out, overwriting the
	 * original encoded JWT taken from the cookie (that has long ago been
	 * translated into allocated buffers in the JOSE object)
	 */

	if (lws_jwt_token_sanity(out, cml, i->iss, i->aud, i->csrf_in,
				 i->sub, sizeof(i->sub),
				 &i->expiry_unix_time)) {
		lwsl_notice("%s: jwt sanity failed\n", __func__);
		return 1;
	}

	/*
	 * If he's interested in his private JSON part, point him to that in
	 * the args struct (it's pointing to the data in out
	 */

	cp = lws_json_simple_find(out, cml, "\"ext\":", &i->extra_json_len);
	if (cp)
		i->extra_json = cp;

	if (!cp)
		lwsl_notice("%s: no ext JWT payload\n", __func__);

	return 0;
}

int
lws_jwt_sign_token_set_http_cookie(struct lws *wsi,
				   const struct lws_jwt_sign_set_cookie *i,
				   uint8_t **p, uint8_t *end)
{
	char plain[MAX_JWT_SIZE + 1], temp[MAX_JWT_SIZE * 2], csrf[17];
	size_t pl = sizeof(plain);
	unsigned long long ull;
	int n;

	/*
	 * Create a 16-char random csrf token with the same lifetime as the JWT
	 */

	lws_hex_random(wsi->a.context, csrf, sizeof(csrf));
	ull = lws_now_secs();
	if (lws_jwt_sign_compact(wsi->a.context, i->jwk, i->alg, plain, &pl,
			         temp, sizeof(temp),
			         "{\"iss\":\"%s\",\"aud\":\"%s\","
			          "\"iat\":%llu,\"nbf\":%llu,\"exp\":%llu,"
			          "\"csrf\":\"%s\",\"sub\":\"%s\"%s%s%s}",
			         i->iss, i->aud, ull, ull - 60,
			         ull + i->expiry_unix_time,
			         csrf, i->sub,
			         i->extra_json ? ",\"ext\":{" : "",
			         i->extra_json ? i->extra_json : "",
			         i->extra_json ? "}" : "")) {
		lwsl_err("%s: failed to create JWT\n", __func__);

		return 1;
	}

	/*
	 * There's no point the browser holding on to a JWT beyond the JWT's
	 * expiry time, so set it to be the same.
	 */

	n = lws_snprintf(temp, sizeof(temp), "__Host-%s=%s;"
			 "HttpOnly;"
			 "Secure;"
			 "SameSite=strict;"
			 "Path=/;"
			 "Max-Age=%lu",
			 i->cookie_name, plain, i->expiry_unix_time);

	if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_SET_COOKIE,
					 (uint8_t *)temp, n, p, end)) {
		lwsl_err("%s: failed to add JWT cookie header\n", __func__);
		return 1;
	}

	return 0;
}
#endif
