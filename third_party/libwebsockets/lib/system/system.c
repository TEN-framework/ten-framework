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

/*
 * It's either a buflist (.is_direct = 0) or
 * a direct pointer + len (.is_direct = 1)
 */

const lws_system_ops_t *
lws_system_get_ops(struct lws_context *context)
{
	return context->system_ops;
}


void
lws_system_blob_direct_set(lws_system_blob_t *b, const uint8_t *ptr, size_t len)
{
	b->is_direct = 1;
	b->u.direct.ptr = ptr;
	b->u.direct.len = len;
}

void
lws_system_blob_heap_empty(lws_system_blob_t *b)
{
	b->is_direct = 0;
	lws_buflist_destroy_all_segments(&b->u.bl);
}

int
lws_system_blob_heap_append(lws_system_blob_t *b, const uint8_t *buf, size_t len)
{
	assert(!b->is_direct);

	lwsl_debug("%s: blob %p\n", __func__, b);

	if (lws_buflist_append_segment(&b->u.bl, buf, len) < 0)
		return -1;

	return 0;
}

size_t
lws_system_blob_get_size(lws_system_blob_t *b)
{
	if (b->is_direct)
		return b->u.direct.len;

	return lws_buflist_total_len(&b->u.bl);
}

int
lws_system_blob_get(lws_system_blob_t *b, uint8_t *buf, size_t *len, size_t ofs)
{
	int n;

	if (b->is_direct) {

		assert(b->u.direct.ptr);

		if (ofs >= b->u.direct.len) {
			*len = 0;
			return 1;
		}

		if (*len > b->u.direct.len - ofs)
			*len = b->u.direct.len - ofs;

		memcpy(buf, b->u.direct.ptr + ofs, *len);

		return 0;
	}

	n = lws_buflist_linear_copy(&b->u.bl, ofs, buf, *len);
	if (n < 0)
		return -2;

	*len = (unsigned int)n;

	return 0;
}

int
lws_system_blob_get_single_ptr(lws_system_blob_t *b, const uint8_t **ptr)
{
	if (b->is_direct) {
		*ptr = b->u.direct.ptr;
		return 0;
	}

	if (!b->u.bl)
		return -1;

	if (b->u.bl->next)
		return -1;  /* multipart buflist, no single pointer to it all */

	*ptr = (const uint8_t *)&b->u.bl[1] + LWS_PRE;

	return 0;
}

void
lws_system_blob_destroy(lws_system_blob_t *b)
{
	if (!b)
		return;
	// lwsl_info("%s: blob %p\n", __func__, b);
	if (!b->is_direct)
		lws_buflist_destroy_all_segments(&b->u.bl);
}

lws_system_blob_t *
lws_system_get_blob(struct lws_context *context, lws_system_blob_item_t type,
		    int idx)
{
	if (idx < 0 ||
	    idx >= (int)LWS_ARRAY_SIZE(context->system_blobs))
		return NULL;

	return &context->system_blobs[type + (unsigned int)idx];
}

#if defined(LWS_WITH_NETWORK)

/*
 * Caller must protect the whole call with system-specific locking
 */

int
__lws_system_attach(struct lws_context *context, int tsi, lws_attach_cb_t cb,
		    lws_system_states_t state, void *opaque,
		    struct lws_attach_item **get)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	struct lws_attach_item *item;

	if (!get) {
		/*
		 * allocate and add to the head of the pt's attach list
		 */

		item = lws_zalloc(sizeof(*item), __func__);
		if (!item)
			return 1;

		item->cb = cb;
		item->opaque = opaque;
		item->state = state;

		lws_dll2_add_head(&item->list, &pt->attach_owner);

		lws_cancel_service(context);

		return 0;
	}

	*get = NULL;
#if defined(LWS_WITH_SYS_STATE)
	if (!pt->attach_owner.count)
		return 0;

	/*
	 * If any, return the first guy whose state requirement matches
	 */

	lws_start_foreach_dll(struct lws_dll2 *, d,
			      lws_dll2_get_head(&pt->attach_owner)) {
		item = lws_container_of(d, lws_attach_item_t, list);

		if (pt->context->mgr_system.state >= (int)item->state) {
			*get = item;
			lws_dll2_remove(d);

			/*
			 * We detached it, but the caller now has the
			 * responsibility to lws_free() *get.
			 */

			return 0;
		}
	} lws_end_foreach_dll(d);
#endif

	/* nobody ready to go... leave *get as NULL and return cleanly */

	return 0;
}

int
lws_system_do_attach(struct lws_context_per_thread *pt)
{
	/*
	 * If nothing to do, we just return immediately
	 */

	while (pt->attach_owner.count) {

		struct lws_attach_item *item;

		/*
		 * If anybody used the attach apis, there must be an
		 * implementation of the (*attach) lws_system op function
		 */

		assert(pt->context->system_ops->attach);
		if (!pt->context->system_ops->attach) {
			lwsl_err("%s: define (*attach)\n", __func__);
			return 1;
		}

		/*
		 * System locking is applied only around this next call, while
		 * we detach and get a pointer to the tail attach item.  We
		 * become responsible to free what we have detached.
		 */

		if (pt->context->system_ops->attach(pt->context, pt->tid, NULL,
						    0, NULL, &item)) {
			lwsl_err("%s: attach problem\n", __func__);
			return 1;
		}

		if (!item)
			/* there's nothing more to do at the moment */
			return 0;

		/*
		 * Do the callback from the lws event loop thread
		 */

		item->cb(pt->context, pt->tid, item->opaque);

		/* it's done, destroy the item */

		lws_free(item);
	}

	return 0;
}

#endif
