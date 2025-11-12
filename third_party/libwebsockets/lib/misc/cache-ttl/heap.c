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

#include <private-lib-core.h>
#include "private-lib-misc-cache-ttl.h"

#if defined(write)
#undef write
#endif

static void
update_sul(lws_cache_ttl_lru_t_heap_t *cache);

static int
lws_cache_heap_invalidate(struct lws_cache_ttl_lru *_c, const char *key);

static int
sort_expiry(const lws_dll2_t *a, const lws_dll2_t *b)
{
	const lws_cache_ttl_item_heap_t
		*c = lws_container_of(a, lws_cache_ttl_item_heap_t, list_expiry),
		*d = lws_container_of(b, lws_cache_ttl_item_heap_t, list_expiry);

	if (c->expiry > d->expiry)
		return 1;
	if (c->expiry < d->expiry)
		return -1;

	return 0;
}

static void
_lws_cache_heap_item_destroy(lws_cache_ttl_lru_t_heap_t *cache,
			     lws_cache_ttl_item_heap_t *item)
{
	lwsl_cache("%s: %s (%s)\n", __func__, cache->cache.info.name,
			(const char *)&item[1] + item->size);

	lws_dll2_remove(&item->list_expiry);
	lws_dll2_remove(&item->list_lru);

	cache->cache.current_footprint -= item->size;

	update_sul(cache);

	if (cache->cache.info.cb)
		cache->cache.info.cb((void *)((uint8_t *)&item[1]), item->size);

	lws_free(item);
}

static void
lws_cache_heap_item_destroy(lws_cache_ttl_lru_t_heap_t *cache,
			    lws_cache_ttl_item_heap_t *item, int parent_too)
{
	struct lws_cache_ttl_lru *backing = &cache->cache;
	const char *tag = ((const char *)&item[1]) + item->size;

	/*
	 * We're destroying a normal item?
	 */

	if (*tag == META_ITEM_LEADING)
		/* no, nothing to check here then */
		goto post;

	if (backing->info.parent)
		backing = backing->info.parent;

	/*
	 * We need to check any cached meta-results from lookups that
	 * include this normal item, and if any, invalidate the meta-results
	 * since they have to be recalculated before being used again.
	 */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   cache->items_lru.head) {
		lws_cache_ttl_item_heap_t *i = lws_container_of(d,
						lws_cache_ttl_item_heap_t,
						list_lru);
		const char *iname = ((const char *)&item[1]) + item->size;
		uint8_t *pay = (uint8_t *)&item[1], *end = pay + item->size;

		if (*iname == META_ITEM_LEADING) {
			size_t taglen = strlen(iname);

			/*
			 * If the item about to be destroyed makes an
			 * appearance on the meta results list, we must kill
			 * the meta result item to force recalc next time
			 */

			while (pay < end) {
				uint32_t tlen = lws_ser_ru32be(pay + 4);

				if (tlen == taglen &&
				    !strcmp((const char *)pay + 8, iname)) {
#if defined(_DEBUG)
					/*
					 * Sanity check that the item tag is
					 * really a match for that meta results
					 * item
					 */

					assert (!backing->info.ops->tag_match(
						 backing, iname + 1, tag, 1));
#endif
					_lws_cache_heap_item_destroy(cache, i);
					break;
				}
				pay += 8 + tlen + 1;
			}

#if defined(_DEBUG)
			/*
			 * Sanity check that the item tag really isn't a match
			 * for that meta results item
			 */

			assert (backing->info.ops->tag_match(backing, iname + 1,
							  tag, 1));
#endif
		}

	} lws_end_foreach_dll_safe(d, d1);

post:
	_lws_cache_heap_item_destroy(cache, item);
}

static void
lws_cache_item_evict_lru(lws_cache_ttl_lru_t_heap_t *cache)
{
	lws_cache_ttl_item_heap_t *ei;

	if (!cache->items_lru.head)
		return;

	ei = lws_container_of(cache->items_lru.head,
			      lws_cache_ttl_item_heap_t, list_lru);

	lws_cache_heap_item_destroy(cache, ei, 0);
}

/*
 * We need to weed out expired entries in the backing file
 */

static void
expiry_cb(lws_sorted_usec_list_t *sul)
{
	lws_cache_ttl_lru_t_heap_t *cache = lws_container_of(sul,
					lws_cache_ttl_lru_t_heap_t, cache.sul);
	lws_usec_t now = lws_now_usecs();

	lwsl_cache("%s: %s\n", __func__, cache->cache.info.name);

	while (cache->items_expiry.head) {
		lws_cache_ttl_item_heap_t *item;

		item = lws_container_of(cache->items_expiry.head,
					lws_cache_ttl_item_heap_t, list_expiry);

		if (item->expiry > now)
			return;

		lws_cache_heap_item_destroy(cache, item, 1);
	}
}

/*
 * Let's figure out what the earliest next expiry is
 */

static int
earliest_expiry(lws_cache_ttl_lru_t_heap_t *cache, lws_usec_t *pearliest)
{
	lws_cache_ttl_item_heap_t *item;

	if (!cache->items_expiry.head)
		return 1;

	item = lws_container_of(cache->items_expiry.head,
				lws_cache_ttl_item_heap_t, list_expiry);

	*pearliest = item->expiry;

	return 0;
}

static void
update_sul(lws_cache_ttl_lru_t_heap_t *cache)
{
	lws_usec_t earliest;

	/* weed out any newly-expired */
	expiry_cb(&cache->cache.sul);

	/* figure out the next soonest expiring item */
	if (earliest_expiry(cache, &earliest)) {
		lws_sul_cancel(&cache->cache.sul);
		return;
	}

	lwsl_debug("%s: setting exp %llu\n", __func__,
			(unsigned long long)earliest);

	if (earliest)
		lws_cache_schedule(&cache->cache, expiry_cb, earliest);
}

static lws_cache_ttl_item_heap_t *
lws_cache_heap_specific(lws_cache_ttl_lru_t_heap_t *cache,
			const char *specific_key)
{
	lws_start_foreach_dll(struct lws_dll2 *, d, cache->items_lru.head) {
		lws_cache_ttl_item_heap_t *item = lws_container_of(d,
						lws_cache_ttl_item_heap_t,
						list_lru);
		const char *iname = ((const char *)&item[1]) + item->size;

		if (!strcmp(specific_key, iname))
			return item;

	} lws_end_foreach_dll(d);

	return NULL;
}

static int
lws_cache_heap_tag_match(struct lws_cache_ttl_lru *cache, const char *wc,
				const char *tag, char lookup_rules)
{
	return lws_strcmp_wildcard(wc, strlen(wc), tag, strlen(tag));
}

static int
lws_cache_heap_lookup(struct lws_cache_ttl_lru *_c, const char *wildcard_key,
		      lws_dll2_owner_t *results_owner)
{
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;
	size_t sklen = strlen(wildcard_key);

	lws_start_foreach_dll(struct lws_dll2 *, d, cache->items_lru.head) {
		lws_cache_ttl_item_heap_t *item = lws_container_of(d,
						lws_cache_ttl_item_heap_t,
						list_lru);
		const char *iname = ((const char *)&item[1]) + item->size;

		if (!lws_strcmp_wildcard(wildcard_key, sklen, iname,
					 strlen(iname))) {
			size_t ilen = strlen(iname);
			lws_cache_match_t *m;
			char hit = 0;

			/*
			 * It musn't already be on the list from an earlier
			 * cache level
			 */

			lws_start_foreach_dll(struct lws_dll2 *, e,
					results_owner->head) {
				lws_cache_match_t *i = lws_container_of(e,
							lws_cache_match_t, list);
				if (i->tag_size == ilen &&
				    !strcmp(iname, ((const char *)&i[1]))) {
					hit = 1;
					break;
				}
			} lws_end_foreach_dll(e);

			if (!hit) {

				/*
				 * it's unique, instantiate a record for it
				 */

				m = lws_fi(&_c->info.cx->fic,
					   "cache_lookup_oom") ? NULL :
					lws_malloc(sizeof(*m) + ilen + 1,
						   __func__);
				if (!m) {
					lws_cache_clear_matches(results_owner);
					return 1;
				}

				memset(&m->list, 0, sizeof(m->list));
				m->tag_size = ilen;
				memcpy(&m[1], iname, ilen + 1);

				lws_dll2_add_tail(&m->list, results_owner);
			}
		}

	} lws_end_foreach_dll(d);

	return 0;
}

static int
lws_cache_heap_write(struct lws_cache_ttl_lru *_c, const char *specific_key,
		     const uint8_t *source, size_t size, lws_usec_t expiry,
		     void **ppvoid)
{
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;
	struct lws_cache_ttl_lru *backing = _c;
	lws_cache_ttl_item_heap_t *item, *ei;
	size_t kl = strlen(specific_key);
	char *p;

	lwsl_cache("%s: %s: len %d\n", __func__, _c->info.name, (int)size);

	/*
	 * Is this new tag going to invalidate any existing cached meta-results?
	 *
	 * If so, let's destroy any of those first to recover the heap
	 */

	if (backing->info.parent)
		backing = backing->info.parent;

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   cache->items_lru.head) {
		lws_cache_ttl_item_heap_t *i = lws_container_of(d,
						lws_cache_ttl_item_heap_t,
						list_lru);
		const char *iname = ((const char *)&i[1]) + i->size;

		if (*iname == META_ITEM_LEADING) {

			/*
			 * If the item about to be added would match any cached
			 * results from before it was added, we have to
			 * invalidate them.  To check this, we have to use the
			 * matching rules at the backing store level
			 */

			if (!strcmp(iname + 1, specific_key))
				_lws_cache_heap_item_destroy(cache, i);
		}

	} lws_end_foreach_dll_safe(d, d1);


	/*
	 * Keep us under the limit if possible... note this will always allow
	 * caching a single large item even if it is above the limits
	 */

	while ((cache->cache.info.max_footprint &&
	        cache->cache.current_footprint + size >
					     cache->cache.info.max_footprint) ||
	       (cache->cache.info.max_items &&
		cache->items_lru.count + 1 > cache->cache.info.max_items))
		lws_cache_item_evict_lru(cache);

	/* remove any existing entry of the same key */

	lws_cache_heap_invalidate(&cache->cache, specific_key);

	item = lws_fi(&_c->info.cx->fic, "cache_write_oom") ? NULL :
			lws_malloc(sizeof(*item) + kl + 1u + size, __func__);
	if (!item)
		return 1;

	cache->cache.current_footprint += item->size;

	/* only need to zero down our item object */
	memset(item, 0, sizeof(*item));

	p = (char *)&item[1];
	if (ppvoid)
		*ppvoid = p;

	/* copy the payload into place */
	if (source)
		memcpy(p, source, size);

	/* copy the key string into place, with terminating NUL */
	memcpy(p + size, specific_key, kl + 1);

	item->expiry = expiry;
	item->key_len = kl;
	item->size = size;

	if (expiry) {
		/* adding to expiry is optional, on nonzero expiry */
		lws_dll2_add_sorted(&item->list_expiry, &cache->items_expiry,
				    sort_expiry);
		ei = lws_container_of(cache->items_expiry.head,
				      lws_cache_ttl_item_heap_t, list_expiry);
		lwsl_debug("%s: setting exp %llu\n", __func__,
						(unsigned long long)ei->expiry);
		lws_cache_schedule(&cache->cache, expiry_cb, ei->expiry);
	}

	/* always add outselves to head of lru list */
	lws_dll2_add_head(&item->list_lru, &cache->items_lru);

	return 0;
}

static int
lws_cache_heap_get(struct lws_cache_ttl_lru *_c, const char *specific_key,
		   const void **pdata, size_t *psize)
{
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;
	lws_cache_ttl_item_heap_t *item;

	item = lws_cache_heap_specific(cache, specific_key);
	if (!item)
		return 1;

	/* we are using it, move it to lru head */
	lws_dll2_remove(&item->list_lru);
	lws_dll2_add_head(&item->list_lru, &cache->items_lru);

	if (pdata) {
		*pdata = (const void *)&item[1];
		*psize = item->size;
	}

	return 0;
}

static int
lws_cache_heap_invalidate(struct lws_cache_ttl_lru *_c, const char *specific_key)
{
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;
	struct lws_cache_ttl_lru *backing = _c;
	lws_cache_ttl_item_heap_t *item;
	const void *user;
	size_t size;

	if (lws_cache_heap_get(_c, specific_key, &user, &size))
		return 0;

	if (backing->info.parent)
		backing = backing->info.parent;

	item = (lws_cache_ttl_item_heap_t *)(((uint8_t *)user) - sizeof(*item));

	/*
	 * We must invalidate any cached results that would have included this
	 */

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   cache->items_lru.head) {
		lws_cache_ttl_item_heap_t *i = lws_container_of(d,
						lws_cache_ttl_item_heap_t,
						list_lru);
		const char *iname = ((const char *)&i[1]) + i->size;

		if (*iname == META_ITEM_LEADING) {

			/*
			 * If the item about to be added would match any cached
			 * results from before it was added, we have to
			 * invalidate them.  To check this, we have to use the
			 * matching rules at the backing store level
			 */

			if (!backing->info.ops->tag_match(backing, iname + 1,
							  specific_key, 1))
				_lws_cache_heap_item_destroy(cache, i);
		}

	} lws_end_foreach_dll_safe(d, d1);

	lws_cache_heap_item_destroy(cache, item, 0);

	return 0;
}

static struct lws_cache_ttl_lru *
lws_cache_heap_create(const struct lws_cache_creation_info *info)
{
	lws_cache_ttl_lru_t_heap_t *cache;

	assert(info->cx);
	assert(info->name);

	cache = lws_fi(&info->cx->fic, "cache_createfail") ? NULL :
					lws_zalloc(sizeof(*cache), __func__);
	if (!cache)
		return NULL;

	cache->cache.info = *info;
	if (info->parent)
		info->parent->child = &cache->cache;

	// lwsl_cache("%s: create %s\n", __func__, info->name);

	return (struct lws_cache_ttl_lru *)cache;
}

static int
destroy_dll(struct lws_dll2 *d, void *user)
{
	lws_cache_ttl_lru_t *_c = (struct lws_cache_ttl_lru *)user;
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;
	lws_cache_ttl_item_heap_t *item =
		       lws_container_of(d, lws_cache_ttl_item_heap_t, list_lru);

	lws_cache_heap_item_destroy(cache, item, 0);

	return 0;
}

static int
lws_cache_heap_expunge(struct lws_cache_ttl_lru *_c)
{
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;

	lws_dll2_foreach_safe(&cache->items_lru, cache, destroy_dll);

	return 0;
}

static void
lws_cache_heap_destroy(struct lws_cache_ttl_lru **_cache)
{
	lws_cache_ttl_lru_t *c = *_cache;
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)c;

	if (!cache)
		return;

	lws_sul_cancel(&c->sul);

	lws_dll2_foreach_safe(&cache->items_lru, cache, destroy_dll);

	lws_free_set_NULL(*_cache);
}

#if defined(_DEBUG)
static int
dump_dll(struct lws_dll2 *d, void *user)
{
	lws_cache_ttl_item_heap_t *item =
		       lws_container_of(d, lws_cache_ttl_item_heap_t, list_lru);

	lwsl_cache("  %s: size %d, exp %llu\n",
		   (const char *)&item[1] + item->size,
		   (int)item->size, (unsigned long long)item->expiry);

	lwsl_hexdump_cache((const char *)&item[1], item->size);

	return 0;
}

static void
lws_cache_heap_debug_dump(struct lws_cache_ttl_lru *_c)
{
	lws_cache_ttl_lru_t_heap_t *cache = (lws_cache_ttl_lru_t_heap_t *)_c;
#if !defined(LWS_WITH_NO_LOGS)
	lws_cache_ttl_item_heap_t *item = NULL;

	lws_dll2_t *d = cache->items_expiry.head;

	if (d)
		item = lws_container_of(d, lws_cache_ttl_item_heap_t,
						list_expiry);

	lwsl_cache("%s: %s: items %d, earliest %llu\n", __func__,
			cache->cache.info.name, (int)cache->items_lru.count,
			item ? (unsigned long long)item->expiry : 0ull);
#endif

	lws_dll2_foreach_safe(&cache->items_lru, cache, dump_dll);
}
#endif

const struct lws_cache_ops lws_cache_ops_heap = {
	.create			= lws_cache_heap_create,
	.destroy		= lws_cache_heap_destroy,
	.expunge		= lws_cache_heap_expunge,

	.write			= lws_cache_heap_write,
	.tag_match		= lws_cache_heap_tag_match,
	.lookup			= lws_cache_heap_lookup,
	.invalidate		= lws_cache_heap_invalidate,
	.get			= lws_cache_heap_get,
#if defined(_DEBUG)
	.debug_dump		= lws_cache_heap_debug_dump,
#endif
};
