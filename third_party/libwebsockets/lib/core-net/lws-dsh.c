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
 */

#include "private-lib-core.h"

struct lws_dsh_search {
	size_t		required;
	int		kind;
	lws_dsh_obj_t	*best;
	lws_dsh_t	*dsh;

	lws_dsh_t	*already_checked;
	lws_dsh_t	*this_dsh;
};

static int
_lws_dsh_alloc_tail(lws_dsh_t *dsh, int kind, const void *src1, size_t size1,
		    const void *src2, size_t size2, lws_dll2_t *replace);

static size_t
lws_dsh_align(size_t length)
{
	size_t align = sizeof(int *);

	if (length & (align - 1))
		length += align - (length & (align - 1));

	return length;
}

lws_dsh_t *
lws_dsh_create(lws_dll2_owner_t *owner, size_t buf_len, int count_kinds)
{
	size_t oha_len = sizeof(lws_dsh_obj_head_t) * (unsigned int)(++count_kinds);
	lws_dsh_obj_t *obj;
	lws_dsh_t *dsh;
	int n;

	assert(buf_len);
	assert(count_kinds > 1);
	assert(buf_len > sizeof(lws_dsh_t) + oha_len);
	buf_len += 64;

	dsh = lws_malloc(sizeof(lws_dsh_t) + buf_len + oha_len, __func__);
	if (!dsh)
		return NULL;

	/* set convenience pointers to the overallocated parts */

	dsh->oha = (lws_dsh_obj_head_t *)&dsh[1];
	dsh->buf = ((uint8_t *)dsh->oha) + oha_len;
	dsh->count_kinds = count_kinds;
	dsh->buffer_size = buf_len;
	dsh->being_destroyed = 0;

	/* clear down the obj heads array */

	memset(dsh->oha, 0, oha_len);
	for (n = 0; n < count_kinds; n++) {
		dsh->oha[n].kind = n;
		dsh->oha[n].total_size = 0;
	}

	/* initially the whole buffer is on the free kind (0) list */

	obj = (lws_dsh_obj_t *)dsh->buf;
	memset(obj, 0, sizeof(*obj));
	obj->asize = buf_len - sizeof(*obj);

	lws_dll2_add_head(&obj->list, &dsh->oha[0].owner);

	dsh->locally_free = obj->asize;
	dsh->locally_in_use = 0;

	lws_dll2_clear(&dsh->list);
	if (owner)
		lws_dll2_add_head(&dsh->list, owner);

	// lws_dsh_describe(dsh, "post-init");

	return dsh;
}

static int
search_best_free(struct lws_dll2 *d, void *user)
{
	struct lws_dsh_search *s = (struct lws_dsh_search *)user;
	lws_dsh_obj_t *obj = lws_container_of(d, lws_dsh_obj_t, list);

	lwsl_debug("%s: obj %p, asize %zu (req %zu)\n", __func__, obj,
			obj->asize, s->required);

	if (obj->asize >= s->required &&
	    (!s->best || obj->asize < s->best->asize)) {
		s->best = obj;
		s->dsh = s->this_dsh;
	}

	return 0;
}

void
lws_dsh_destroy(lws_dsh_t **pdsh)
{
	lws_dsh_t *dsh = *pdsh;

	if (!dsh)
		return;

	dsh->being_destroyed = 1;

	lws_dll2_remove(&dsh->list);

	/* everything else is in one heap allocation */

	lws_free_set_NULL(*pdsh);
}

size_t
lws_dsh_get_size(struct lws_dsh *dsh, int kind)
{
	kind++;
	assert(kind < dsh->count_kinds);

	return dsh->oha[kind].total_size;
}

static int
_lws_dsh_alloc_tail(lws_dsh_t *dsh, int kind, const void *src1, size_t size1,
		    const void *src2, size_t size2, lws_dll2_t *replace)
{
	size_t asize = sizeof(lws_dsh_obj_t) + lws_dsh_align(size1 + size2);
	struct lws_dsh_search s;

	assert(kind >= 0);
	kind++;
	assert(!dsh || kind < dsh->count_kinds);

	/*
	 * Search our free list looking for the smallest guy who will fit
	 * what we want to allocate
	 */
	s.required = asize;
	s.kind = kind;
	s.best = NULL;
	s.already_checked = NULL;
	s.this_dsh = dsh;

	if (dsh && !dsh->being_destroyed)
		lws_dll2_foreach_safe(&dsh->oha[0].owner, &s, search_best_free);

	if (!s.best) {
		lwsl_notice("%s: no buffer has space\n", __func__);

		return 1;
	}

	/* anything coming out of here must be aligned */
	assert(!(((unsigned long)s.best) & (sizeof(int *) - 1)));

	if (s.best->asize < asize + (2 * sizeof(*s.best))) {
		/*
		 * Exact fit, or close enough we can't / don't want to have to
		 * track the little bit of free area that would be left.
		 *
		 * Move the object from the free list to the oha of the
		 * desired kind
		 */
		lws_dll2_remove(&s.best->list);
		s.best->dsh = s.dsh;
		s.best->kind = kind;
		s.best->size = size1 + size2;
		memcpy(&s.best[1], src1, size1);
		if (src2)
			memcpy((uint8_t *)&s.best[1] + size1, src2, size2);

		if (replace) {
			s.best->list.prev = replace->prev;
			s.best->list.next = replace->next;
			s.best->list.owner = replace->owner;
			if (replace->prev)
				replace->prev->next = &s.best->list;
			if (replace->next)
				replace->next->prev = &s.best->list;
		} else
			if (dsh) {
				assert(!(((unsigned long)(intptr_t)(s.best)) & (sizeof(int *) - 1)));
				lws_dll2_add_tail(&s.best->list, &dsh->oha[kind].owner);
			}

		assert(s.dsh->locally_free >= s.best->asize);
		s.dsh->locally_free -= s.best->asize;
		s.dsh->locally_in_use += s.best->asize;
		dsh->oha[kind].total_size += s.best->asize;
		assert(s.dsh->locally_in_use <= s.dsh->buffer_size);
	} else {
		lws_dsh_obj_t *obj;

		/*
		 * Free area was oversize enough that we need to split it.
		 *
		 * Leave the first part of the free area where it is and
		 * reduce its extent by our asize.  Use the latter part of
		 * the original free area as the allocation.
		 */
		lwsl_debug("%s: splitting... free reduce %zu -> %zu\n",
				__func__, s.best->asize, s.best->asize - asize);

		s.best->asize -= asize;

		/* latter part becomes new object */

		obj = (lws_dsh_obj_t *)(((uint8_t *)s.best) + lws_dsh_align(s.best->asize));

		lws_dll2_clear(&obj->list);
		obj->dsh = s.dsh;
		obj->kind = kind;
		obj->size = size1 + size2;
		obj->asize = asize;

		memcpy(&obj[1], src1, size1);
		if (src2)
			memcpy((uint8_t *)&obj[1] + size1, src2, size2);

		if (replace) {
			s.best->list.prev = replace->prev;
			s.best->list.next = replace->next;
			s.best->list.owner = replace->owner;
			if (replace->prev)
				replace->prev->next = &s.best->list;
			if (replace->next)
				replace->next->prev = &s.best->list;
		} else
			if (dsh) {
				assert(!(((unsigned long)(intptr_t)(obj)) & (sizeof(int *) - 1)));
				lws_dll2_add_tail(&obj->list, &dsh->oha[kind].owner);
			}

		assert(s.dsh->locally_free >= asize);
		s.dsh->locally_free -= asize;
		s.dsh->locally_in_use += asize;
		dsh->oha[kind].total_size += asize;
		assert(s.dsh->locally_in_use <= s.dsh->buffer_size);
	}

	// lws_dsh_describe(dsh, "post-alloc");

	return 0;
}

int
lws_dsh_alloc_tail(lws_dsh_t *dsh, int kind, const void *src1, size_t size1,
		   const void *src2, size_t size2)
{
	return _lws_dsh_alloc_tail(dsh, kind, src1, size1, src2, size2, NULL);
}

static int
buf_compare(const lws_dll2_t *d, const lws_dll2_t *i)
{
	return (int)lws_ptr_diff(d, i);
}

void
lws_dsh_free(void **pobj)
{
	lws_dsh_obj_t *_o = (lws_dsh_obj_t *)((uint8_t *)(*pobj) - sizeof(*_o)),
			*_o2;
	lws_dsh_t *dsh = _o->dsh;

	/* anything coming out of here must be aligned */
	assert(!(((unsigned long)_o) & (sizeof(int *) - 1)));

	/*
	 * Remove the object from its list and place on the free list of the
	 * dsh the buffer space belongs to
	 */

	lws_dll2_remove(&_o->list);
	*pobj = NULL;

	assert(dsh->locally_in_use >= _o->asize);
	dsh->locally_free += _o->asize;
	dsh->locally_in_use -= _o->asize;
	dsh->oha[_o->kind].total_size -= _o->asize; /* account for usage by kind */
	assert(dsh->locally_in_use <= dsh->buffer_size);

	/*
	 * The free space list is sorted in buffer address order, so detecting
	 * coalescing opportunities is cheap.  Because the free list should be
	 * continuously tending to reduce by coalescing, the sorting should not
	 * be expensive to maintain.
	 */
	_o->size = 0; /* not meaningful when on free list */
	lws_dll2_add_sorted(&_o->list, &_o->dsh->oha[0].owner, buf_compare);

	/* First check for already-free block at the end we can subsume.
	 * Because the free list is sorted, if there is such a guy he is
	 * already our list.next */

	_o2 = (lws_dsh_obj_t *)_o->list.next;
	if (_o2 && (uint8_t *)_o + _o->asize == (uint8_t *)_o2) {
		/*
		 * since we are freeing _obj, we can coalesce with a
		 * free area immediately ahead of it
		 *
		 *  [ _o (being freed) ][ _o2 (free) ]  -> [ larger _o ]
		 */
		_o->asize += _o2->asize;

		/* guy next to us was absorbed into us */
		lws_dll2_remove(&_o2->list);
	}

	/* Then check if we can be subsumed by a free block behind us.
	 * Because the free list is sorted, if there is such a guy he is
	 * already our list.prev */

	_o2 = (lws_dsh_obj_t *)_o->list.prev;
	if (_o2 && (uint8_t *)_o2 + _o2->asize == (uint8_t *)_o) {
		/*
		 * since we are freeing obj, we can coalesce it with
		 * the previous free area that abuts it
		 *
		 *  [ _o2 (free) ][ _o (being freed) ] -> [ larger _o2 ]
		 */
		_o2->asize += _o->asize;

		/* we were absorbed! */
		lws_dll2_remove(&_o->list);
	}

	// lws_dsh_describe(dsh, "post-alloc");
}

int
lws_dsh_get_head(lws_dsh_t *dsh, int kind, void **obj, size_t *size)
{
	lws_dsh_obj_t *_obj;

	if (!dsh)
		return 1;

	_obj = (lws_dsh_obj_t *)lws_dll2_get_head(&dsh->oha[kind + 1].owner);

	if (!_obj) {
		*obj = 0;
		*size = 0;

		return 1;	/* there is no head */
	}

	*obj = (void *)(&_obj[1]);
	*size = _obj->size;

	/* anything coming out of here must be aligned */
	assert(!(((unsigned long)(intptr_t)(*obj)) & (sizeof(int *) - 1)));

	return 0;	/* we returned the head */
}

#if defined(_DEBUG) && !defined(LWS_WITH_NO_LOGS)

static int
describe_kind(struct lws_dll2 *d, void *user)
{
	lws_dsh_obj_t *obj = lws_container_of(d, lws_dsh_obj_t, list);

	lwsl_info("    _obj %p - %p, dsh %p, size %zu, asize %zu\n",
			obj, (uint8_t *)obj + obj->asize,
			obj->dsh, obj->size, obj->asize);

	return 0;
}

void
lws_dsh_describe(lws_dsh_t *dsh, const char *desc)
{
	int n = 0;

	lwsl_info("%s: dsh %p, bufsize %zu, kinds %d, lf: %zu, liu: %zu, %s\n",
		    __func__, dsh, dsh->buffer_size, dsh->count_kinds,
		    dsh->locally_free, dsh->locally_in_use, desc);

	for (n = 0; n < dsh->count_kinds; n++) {
		lwsl_info("  Kind %d:\n", n);
		lws_dll2_foreach_safe(&dsh->oha[n].owner, dsh, describe_kind);
	}
}
#endif
