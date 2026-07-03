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

#define lwsl_cache lwsl_debug
#define lwsl_hexdump_cache lwsl_hexdump_debug

#define LWS_CACHE_MAX_LEVELS 3

/*
 * If we need structure inside the cache tag names, use this character as a
 * separator
 */
#define LWSCTAG_SEP '|'

/*
 * Our synthetic cache result items all have tags starting with this char
 */
#define META_ITEM_LEADING '!'

typedef struct lws_cache_ttl_item_heap {
	lws_dll2_t			list_expiry;
	lws_dll2_t			list_lru;

	lws_usec_t			expiry;
	size_t				key_len;
	size_t				size;

	/*
	 * len + key_len + 1 bytes of data overcommitted, user object first
	 * so it is well-aligned, then the NUL-terminated key name
	 */
} lws_cache_ttl_item_heap_t;

/* this is a "base class", all cache implementations have one at the start */

typedef struct lws_cache_ttl_lru {
	struct lws_cache_creation_info	info;
	lws_sorted_usec_list_t		sul;
	struct lws_cache_ttl_lru	*child;
	uint64_t			current_footprint;
} lws_cache_ttl_lru_t;

/*
 * The heap-backed cache uses lws_dll2 linked-lists to track items that are
 * in it.
 */

typedef struct lws_cache_ttl_lru_heap {
	lws_cache_ttl_lru_t		cache;

	lws_dll2_owner_t		items_expiry;
	lws_dll2_owner_t		items_lru;
} lws_cache_ttl_lru_t_heap_t;

/*
 * We want to be able to work with a large file-backed implementation even on
 * devices that don't have heap to track what is in it.  It means if lookups
 * reach this cache layer, we will be scanning a potentially large file.
 *
 * L1 caching of lookups (including null result list) reduces the expense of
 * this on average.  We keep a copy of the last computed earliest expiry.
 *
 * We can't keep an open file handle here.  Because other processes may change
 * the cookie file by deleting and replacing it, we have to open it fresh each
 * time.
 */
typedef struct lws_cache_nscookiejar {
	lws_cache_ttl_lru_t		cache;

	lws_usec_t			earliest_expiry;
} lws_cache_nscookiejar_t;

void
lws_cache_clear_matches(lws_dll2_owner_t *results_owner);

void
lws_cache_schedule(struct lws_cache_ttl_lru *cache, sul_cb_t cb, lws_usec_t e);
