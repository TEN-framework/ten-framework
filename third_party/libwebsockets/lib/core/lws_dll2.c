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

#ifdef LWS_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

int
lws_dll2_is_detached(const struct lws_dll2 *d)
{
	if (d->owner)
		return 0;

	if (d->next || d->prev) {
		lwsl_err("%s: dll2 %p: detached but next %p, prev %p\n",
				__func__, d, d->next, d->prev);
		/*
		 * New lws_dll2 objects and removed lws_dll2 objects
		 * have .owner, .next and .prev all set to NULL, so we
		 * can just check .owner to see if we are detached.
		 *
		 * We assert here if we encounter an lws_dll2 in the illegal
		 * state of NULL .owner, but non-NULL in .next or .prev,
		 * it's evidence of corruption, use-after-free, threads
		 * contending on accessing without locking etc.
		 */
		assert(0);
	}

	return 1;
}

int
lws_dll2_foreach_safe(struct lws_dll2_owner *owner, void *user,
		      int (*cb)(struct lws_dll2 *d, void *user))
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp, owner->head) {
		if (cb(p, user))
			return 1;
	} lws_end_foreach_dll_safe(p, tp);

	return 0;
}

void
lws_dll2_add_head(struct lws_dll2 *d, struct lws_dll2_owner *owner)
{
	if (!lws_dll2_is_detached(d)) {
		assert(0); /* only wholly detached things can be added */
		return;
	}

	/* our next guy is current first guy, if any */
	if (owner->head != d)
		d->next = owner->head;

	/* if there is a next guy, set his prev ptr to our next ptr */
	if (d->next)
		d->next->prev = d;
	/* there is nobody previous to us, we are the head */
	d->prev = NULL;

	/* set the first guy to be us */
	owner->head = d;

	if (!owner->tail)
		owner->tail = d;

	d->owner = owner;
	owner->count++;
}

/*
 * add us to the list that 'after' is in, just before him
 */

void
lws_dll2_add_before(struct lws_dll2 *d, struct lws_dll2 *after)
{
	lws_dll2_owner_t *owner = after->owner;

	if (!lws_dll2_is_detached(d)) {
		assert(0); /* only wholly detached things can be added */
		return;
	}

	if (lws_dll2_is_detached(after)) {
		assert(0); /* can't add after something detached */
		return;
	}

	d->owner = owner;

	/* we need to point forward to after */

	d->next = after;

	/* we need to point back to after->prev */

	d->prev = after->prev;

	/* guy that used to point to after, needs to point to us */

	if (after->prev)
		after->prev->next = d;
	else
		owner->head = d;

	/* then after needs to point back to us */

	after->prev = d;

	owner->count++;
}

void
lws_dll2_add_tail(struct lws_dll2 *d, struct lws_dll2_owner *owner)
{
	if (!lws_dll2_is_detached(d)) {
		assert(0); /* only wholly detached things can be added */
		return;
	}

	/* our previous guy is current last guy */
	d->prev = owner->tail;
	/* if there is a prev guy, set his next ptr to our prev ptr */
	if (d->prev)
		d->prev->next = d;
	/* our next ptr is NULL */
	d->next = NULL;
	/* set the last guy to be us */
	owner->tail = d;

	/* list head is also us if we're the first */
	if (!owner->head)
		owner->head = d;

	d->owner = owner;
	owner->count++;
}

void
lws_dll2_remove(struct lws_dll2 *d)
{
	if (lws_dll2_is_detached(d))
		return;

	/* if we have a next guy, set his prev to our prev */
	if (d->next)
		d->next->prev = d->prev;

	/* if we have a previous guy, set his next to our next */
	if (d->prev)
		d->prev->next = d->next;

	/* if we have phead, track the tail and head if it points to us... */

	if (d->owner->tail == d)
		d->owner->tail = d->prev;

	if (d->owner->head == d)
		d->owner->head = d->next;

	d->owner->count--;

	/* we're out of the list, we should not point anywhere any more */
	d->owner = NULL;
	d->prev = NULL;
	d->next = NULL;
}

void
lws_dll2_clear(struct lws_dll2 *d)
{
	d->owner = NULL;
	d->prev = NULL;
	d->next = NULL;
}

void
lws_dll2_owner_clear(struct lws_dll2_owner *d)
{
	d->head = NULL;
	d->tail = NULL;
	d->count = 0;
}

void
lws_dll2_add_sorted_priv(lws_dll2_t *d, lws_dll2_owner_t *own, void *priv,
			 int (*compare3)(void *priv, const lws_dll2_t *d,
					const lws_dll2_t *i))
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   lws_dll2_get_head(own)) {
		assert(p != d);

		if (compare3(priv, p, d) >= 0) {
			/* drop us in before this guy */
			lws_dll2_add_before(d, p);

			return;
		}
	} lws_end_foreach_dll_safe(p, tp);

	/*
	 * Either nobody on the list yet to compare him to, or he's the
	 * furthest away timeout... stick him at the tail end
	 */

	lws_dll2_add_tail(d, own);
}

void
lws_dll2_add_sorted(lws_dll2_t *d, lws_dll2_owner_t *own,
		    int (*compare)(const lws_dll2_t *d, const lws_dll2_t *i))
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   lws_dll2_get_head(own)) {
		assert(p != d);

		if (compare(p, d) >= 0) {
			/* drop us in before this guy */
			lws_dll2_add_before(d, p);

			return;
		}
	} lws_end_foreach_dll_safe(p, tp);

	/*
	 * Either nobody on the list yet to compare him to, or he's the
	 * furthest away timeout... stick him at the tail end
	 */

	lws_dll2_add_tail(d, own);
}

void *
_lws_dll2_search_sz_pl(lws_dll2_owner_t *own, const char *name, size_t namelen,
		       size_t dll2_ofs, size_t ptr_ofs)
{
	lws_start_foreach_dll(struct lws_dll2 *, p, lws_dll2_get_head(own)) {
		uint8_t *ref = ((uint8_t *)p) - dll2_ofs;
		/*
		 * We have to read the const char * at the computed place and
		 * the string is where that points
		 */
		const char *str = *((const char **)(ref + ptr_ofs));

		if (str && !strncmp(str, name, namelen) && !str[namelen])
			return (void *)ref;
	} lws_end_foreach_dll(p);

	return NULL;
}

#if defined(_DEBUG)

void
lws_dll2_describe(lws_dll2_owner_t *owner, const char *desc)
{
#if _LWS_ENABLED_LOGS & LLL_INFO
	int n = 1;

	lwsl_info("%s: %s: owner %p: count %d, head %p, tail %p\n",
		    __func__, desc, owner, (int)owner->count, owner->head, owner->tail);

	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   lws_dll2_get_head(owner)) {
		lwsl_info("%s:    %d: %p: owner %p, prev %p, next %p\n",
			    __func__, n++, p, p->owner, p->prev, p->next);
	} lws_end_foreach_dll_safe(p, tp);
#endif
}

#endif
