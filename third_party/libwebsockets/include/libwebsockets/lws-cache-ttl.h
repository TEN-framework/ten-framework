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

/** \defgroup lws_cache_ttl Cache supporting expiry
 * ##Cache supporting expiry
 *
 * These apis let you quickly and reliably implement caches of named objects,
 * that have a "destroy-by date" and cache limits that will be observed.
 *
 * You can instantiate as many caches as you need.  The first one must be an
 * L1 / heap cache type, it can have parents and grandparents of other types
 * which are accessible why writing / looking up and getting from the L1 cache.
 * The outer "cache" layer may persistently store items to a backing store.
 *
 * Allocated object memory is entirely for the use of user code, up to the
 * requested size.
 *
 * The key name for the listed objects may be any string chosen by the user,
 * there is no special length limit as it is also allocated.
 *
 * Both expiry and LRU orderings are kept so it is easy to find out usage
 * ordering and when the next object that will expire.
 *
 * Cached objects may be destroyed any time you go around the event loop, when
 * you allocate new objects (to keep the whole cache under the specified limit),
 * or when their expiry time arrives.  So you shouldn't keep copies of pointers
 * to cached objects after returning to the event loop.
 */
///@{


struct lws_cache_ttl_lru;

/**
 * lws_cache_write_through() - add a new cache item object in all layers
 *
 * \param cache: the existing cache to allocate the object in
 * \param specific_key: a key string that identifies the item in the cache
 * \param source: optional payload for the cached item, NULL means caller will
 *		  write the payload
 * \param size: the size of the object to allocate
 * \param expiry: the usec time that the object will autodestroy
 * \param ppay: NULL, or a pointer to a void * to be set to the L1 payload
 *
 * If an item with the key already exists, it is destroyed before allocating a
 * new one.
 *
 * Returns 0 if successful.  The written entry will be scheduled to be auto-
 * destroyed when \p expiry occurs.
 *
 * Adding or removing cache items may cause invalidation of cached queries.
 */
LWS_VISIBLE LWS_EXTERN int /* only valid until return to event loop */
lws_cache_write_through(struct lws_cache_ttl_lru *cache,
			const char *specific_key, const uint8_t *source,
			size_t size, lws_usec_t expiry, void **ppay);

typedef struct lws_cache_match {
	lws_dll2_t			list;
	lws_usec_t			expiry;
	/* earliest expiry amongst results */
	size_t				payload_size;
	/**< the payload is not attached here.  This is a hint about what
	 * (*get)() will return for this tag name.
	 */
	size_t				tag_size;

	/* tag name + NUL is overcommitted */
} lws_cache_match_t;

/**
 * lws_cache_heap_lookup() - get a list of matching items
 *
 * \param cache: the cache to search for the key
 * \param wildcard_key: the item key string, may contain wildcards
 * \param pdata: pointer to pointer to be set to the serialized result list
 * \param psize: pointer to size_t to receive length of serialized result list
 *
 * This finds all unique items in the final cache that match search_key, which
 * may contain wildcards.  It does not return the payloads for matching items,
 * just a list of specific tags in the that match.
 *
 * If successful, results are provided in a serialized list format, in no
 * particular order, each result has the following fields
 *
 * - BE32: payload size in bytes (payload itself is not included)
 * - BE32: specific tag name length in bytes
 * - chars: tag name with terminating NUL
 *
 * These serialized results are themselves cached in L1 cache (only) and the
 * result pointers are set pointing into that.  If the results are still in L1
 * cache next time this api is called, the results will be returned directly
 * from that without repeating the expensive lookup on the backup store.  That
 * is why the results are provided in serialized form.
 *
 * The cached results list expiry is set to the earliest expiry of any listed
 * item.  Additionally any cached results are invalidated on addition or
 * deletion (update is done as addition + deletion) of any item that would
 * match the results' original wildcard_key.  For the typical case new items
 * are rare compared to lookups, this is efficient.
 *
 * Lookup matching does not itself affect LRU or cache status of the result
 * itsems.  Typically user code will get the lookup results, and then perform
 * get operations on each item in its desired order, that will bring the items
 * to the head of the LRU list and occupy L1 cache.
 *
 * Returns 0 if proceeded alright, or nonzero if error.  If there was an error,
 * any partial results set has been deallocated cleanly before returning.
 */
LWS_VISIBLE LWS_EXTERN int
lws_cache_lookup(struct lws_cache_ttl_lru *cache, const char *wildcard_key,
		 const void **pdata, size_t *psize);

/**
 * lws_cache_item_get() - bring a specific item into L1 and get payload info
 *
 * \param cache: the cache to search for the key
 * \param specific_key: the key string of the item to get
 * \param pdata: pointer to a void * to be set to the payload in L1 cache
 * \param psize: pointer to a size_t to be set to the payload size
 *
 * If the cache still has an item matching the key string, it will be destroyed.
 *
 * Adding or removing cache items may cause invalidation of cached queries.
 *
 * Notice the cache payload is a blob of the given size.  If you are storing
 * strings, there is no NUL termination unless you stored them with it.
 *
 * Returns 0 if successful.
 */
LWS_VISIBLE LWS_EXTERN int
lws_cache_item_get(struct lws_cache_ttl_lru *cache, const char *specific_key,
		   const void **pdata, size_t *psize);

/**
 * lws_cache_item_remove() - remove item from all cache levels
 *
 * \param cache: the cache to search for the key
 * \param wildcard_key: the item key string
 *
 * Removes any copy of any item matching the \p wildcard_key from any cache
 * level in one step.
 *
 * Adding or removing cache items may cause invalidation of cached queries
 * that could refer to the removed item.
 */
LWS_VISIBLE LWS_EXTERN int
lws_cache_item_remove(struct lws_cache_ttl_lru *cache, const char *wildcard_key);

/**
 * lws_cache_footprint() - query the amount of storage used by the cache layer
 *
 * \param cache: cache to query
 *
 * Returns number of payload bytes stored in cache currently
 */
LWS_VISIBLE LWS_EXTERN uint64_t
lws_cache_footprint(struct lws_cache_ttl_lru *cache);

/**
 * lws_cache_debug_dump() - if built in debug mode dump cache contents to log
 *
 * \param cache: cache to dump
 *
 * If lws was built in debug mode, dump cache to log, otherwise a NOP.
 */
LWS_VISIBLE LWS_EXTERN void
lws_cache_debug_dump(struct lws_cache_ttl_lru *cache);

typedef struct lws_cache_results {
	const uint8_t		*ptr; /* set before using walk api */
	size_t			size; /* set before using walk api */

	size_t			payload_len;
	size_t			tag_len;
	const uint8_t		*tag;
} lws_cache_results_t;

/**
 * lws_cache_results_walk() - parse next result
 *
 * \param walk_ctx: the context of the results blob to walk
 *
 * Caller must initialize \p walk_ctx.ptr and \p walk_ctx.size before calling.
 * These are set to the results returned from a _lookup api call.
 *
 * The call returns 0 if the struct elements have been set to a result, or 1
 * if there where no more results in the blob to walk.
 *
 * If successful, after the call \p payload_len is set to the length of the
 * payload related to this result key (the payload itself is not present),
 * \p tag_len is set to the length of the result key name, and \p tag is set
 * to the result tag name, with a terminating NUL.
 */
LWS_VISIBLE LWS_EXTERN int
lws_cache_results_walk(lws_cache_results_t *walk_ctx);

typedef void (*lws_cache_item_destroy_cb)(void *item, size_t size);
struct lws_cache_creation_info {
	struct lws_context		*cx;
	/**< Mandatory: the lws_context */
	const char			*name;
	/**< Mandatory: short cache name */
	lws_cache_item_destroy_cb	cb;
	/**< NULL, or a callback that can hook cache item destory */
	struct lws_cache_ttl_lru	*parent;
	/**< NULL, or next cache level */
	const struct lws_cache_ops	*ops;
	/**< NULL for default, heap-based ops, else custom cache storage and
	 * query implementation */

	union {
		struct {
			const char 	*filepath;
			/**< the filepath to store items in */
		} nscookiejar;
	} u;
	/**< these are extra configuration for specific cache types */

	size_t				max_footprint;
	/**< 0, or the max heap allocation allowed before destroying
	 *   lru items to keep it under the limit */
	size_t				max_items;
	/**< 0, or the max number of items allowed in the cache before
	 *   destroying lru items to keep it under the limit */
	size_t				max_payload;
	/**< 0, or the max allowed payload size for one item */
	int				tsi;
	/**< 0 unless using SMP, then tsi to bind sul to */
};

struct lws_cache_ops {
	struct lws_cache_ttl_lru *
	(*create)(const struct lws_cache_creation_info *info);
	/**< create an instance of the cache type specified in info */

	void
	(*destroy)(struct lws_cache_ttl_lru **_cache);
	/**< destroy the logical cache instance pointed to by *_cache, doesn't
	 * affect any NV backing storage */

	int
	(*expunge)(struct lws_cache_ttl_lru *cache);
	/**< completely delete any backing storage related to the cache
	 * instance, eg, delete the backing file */

	int
	(*write)(struct lws_cache_ttl_lru *cache, const char *specific_key,
		 const uint8_t *source, size_t size, lws_usec_t expiry,
		 void **ppvoid);
	/**< create an entry in the cache level according to the given info */
	int
	(*tag_match)(struct lws_cache_ttl_lru *cache, const char *wc,
		     const char *tag, char lookup_rules);
	/**< Just tell us if tag would match wildcard, using whatever special
	 * rules the backing store might use for tag matching.  0 indicates
	 * it is a match on wildcard, nonzero means does not match.
	 */
	int
	(*lookup)(struct lws_cache_ttl_lru *cache, const char *wildcard_key,
		      lws_dll2_owner_t *results_owner);
	/**+ add keys for search_key matches not already listed in the results
	 * owner */
	int
	(*invalidate)(struct lws_cache_ttl_lru *cache, const char *wildcard_key);
	/**< remove matching item(s) from cache level */

	int
	(*get)(struct lws_cache_ttl_lru *cache, const char *specific_key,
	       const void **pdata, size_t *psize);
	/**< if it has the item, fills L1 with item. updates LRU, and returns
	 * pointer to payload in L1 */

	void
	(*debug_dump)(struct lws_cache_ttl_lru *cache);
	/**< Helper to dump the whole cache contents to log, useful for debug */
};

/**
 * lws_cache_create() - create an empty cache you can allocate items in
 *
 * \param info: a struct describing the cache to create
 *
 * Create an empty cache you can allocate items in.  The cache will be kept
 * below the max_footprint and max_items limits if they are nonzero, by
 * destroying least-recently-used items until it remains below the limits.
 *
 * Items will auto-destroy when their expiry time is reached.
 *
 * When items are destroyed from the cache, if \p cb is non-NULL, it will be
 * called back with the item pointer after it has been removed from the cache,
 * but before it is deallocated and destroyed.
 *
 * context and tsi are used when scheduling expiry callbacks
 */
LWS_VISIBLE LWS_EXTERN struct lws_cache_ttl_lru *
lws_cache_create(const struct lws_cache_creation_info *info);

/**
 * lws_cache_destroy() - destroy a previously created cache
 *
 * \param cache: pointer to the cache
 *
 * Everything in the cache is destroyed, then the cache itself is destroyed,
 * and *cache set to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_cache_destroy(struct lws_cache_ttl_lru **cache);

/**
 * lws_cache_expunge() - destroy all items in cache and parents
 *
 * \param cache: pointer to the cache
 *
 * Everything in the cache and parents is destroyed, leaving it empty.
 * If the cache has a backing store, it is deleted.
 *
 * Returns 0 if no problems reported at any cache layer, else nonzero.
 */
LWS_VISIBLE LWS_EXTERN int
lws_cache_expunge(struct lws_cache_ttl_lru *cache);

LWS_VISIBLE extern const struct lws_cache_ops lws_cache_ops_heap,
					      lws_cache_ops_nscookiejar;

///@}

