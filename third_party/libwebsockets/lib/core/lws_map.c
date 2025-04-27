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

typedef struct lws_map_hashtable {
	struct lws_map			*map_owner; /* so items can find map */
	lws_dll2_owner_t		ho;
} lws_map_hashtable_t;

typedef struct lws_map {
	lws_map_info_t			info;

	/* array of info.modulo x lws_map_hashtable_t overallocated */
} lws_map_t;

typedef struct lws_map_item {
	lws_dll2_t			list; /* owned by hashtable */

	size_t				keylen;
	size_t				valuelen;

	/* key then value is overallocated */
} lws_map_item_t;

/*
 * lwsac-aware allocator
 */

void *
lws_map_alloc_lwsac(struct lws_map *map, size_t x)
{
	return lwsac_use((struct lwsac **)map->info.opaque, x,
					(size_t)map->info.aux);
}

void
lws_map_free_lwsac(void *v)
{
}

/*
 * Default allocation / free if none given in info
 */

void *
lws_map_alloc_lws_malloc(struct lws_map *mo, size_t x)
{
	return lws_malloc(x, __func__);
}

void
lws_map_free_lws_free(void *v)
{
	lws_free(v);
}

/*
 * This just needs to approximate a flat distribution, it's not related to
 * security at all.
 */

lws_map_hash_t
lws_map_hash_from_key_default(const lws_map_key_t key, size_t kl)
{
	lws_map_hash_t h = 0x12345678;
	const uint8_t *u = (const uint8_t *)key;

	while (kl--)
		h = ((((h << 7) | (h >> 25)) + 0xa1b2c3d4) ^ (*u++)) ^ h;

	return h;
}

int
lws_map_compare_key_default(const lws_map_key_t key1, size_t kl1,
			    const lws_map_value_t key2, size_t kl2)
{
	if (kl1 != kl2)
		return 1;

	return memcmp(key1, key2, kl1);
}

lws_map_t *
lws_map_create(const lws_map_info_t *info)
{
	lws_map_t *map;
	lws_map_alloc_t a = info->_alloc;
	size_t modulo = info->modulo;
	lws_map_hashtable_t *ht;
	size_t size;

	if (!a)
		a = lws_map_alloc_lws_malloc;

	if (!modulo)
		modulo = 8;

	size = sizeof(*map) + (modulo * sizeof(lws_map_hashtable_t));
	map = lws_malloc(size, __func__);
	if (!map)
		return NULL;

	memset(map, 0, size);

	map->info = *info;

	map->info._alloc = a;
	map->info.modulo = modulo;
	if (!info->_free)
		map->info._free = lws_map_free_lws_free;
	if (!info->_hash)
		map->info._hash = lws_map_hash_from_key_default;
	if (!info->_compare)
		map->info._compare = lws_map_compare_key_default;

	ht = (lws_map_hashtable_t *)&map[1];
	while (modulo--)
		ht[modulo].map_owner = map;

	return map;
}

static int
ho_free_item(struct lws_dll2 *d, void *user)
{
	lws_map_item_t *i = lws_container_of(d, lws_map_item_t, list);

	lws_map_item_destroy(i);

	return 0;
}

void
lws_map_destroy(lws_map_t **pmap)
{
	lws_map_hashtable_t *ht;
	lws_map_t *map = *pmap;

	if (!map)
		return;

	/* empty out all the hashtables */

	ht = (lws_map_hashtable_t *)&(map[1]);
	while (map->info.modulo--) {
		lws_dll2_foreach_safe(&ht->ho, ht, ho_free_item);
		ht++;
	}

	/* free the map itself */

	lws_free_set_NULL(*pmap);
}

lws_map_item_t *
lws_map_item_create(lws_map_t *map,
		    const lws_map_key_t key, size_t keylen,
		    const lws_map_value_t value, size_t valuelen)
{
	lws_map_hashtable_t *ht;
	lws_map_item_t *item;
	lws_map_hash_t h;
	size_t hti;
	uint8_t *u;

	item = lws_map_item_lookup(map, key, keylen);
	if (item)
		lws_map_item_destroy(item);

	item = map->info._alloc(map, sizeof(*item) + keylen + valuelen);
	if (!item)
		return NULL;

	lws_dll2_clear(&item->list);
	item->keylen = keylen;
	item->valuelen = valuelen;

	u = (uint8_t *)&item[1];
	memcpy(u, key, keylen);
	u += keylen;
	if (value)
		memcpy(u, value, valuelen);

	h = map->info._hash(key, keylen);

	hti = h % map->info.modulo;
	ht = (lws_map_hashtable_t *)&map[1];

	lws_dll2_add_head(&item->list, &ht[hti].ho);

	return item;
}

void
lws_map_item_destroy(lws_map_item_t *item)
{
	lws_map_hashtable_t *ht = lws_container_of(item->list.owner,
						   lws_map_hashtable_t, ho);

	lws_dll2_remove(&item->list);
	ht->map_owner->info._free(item);
}

lws_map_item_t *
lws_map_item_lookup(lws_map_t *map, const lws_map_key_t key, size_t keylen)
{
	lws_map_hash_t h = map->info._hash(key, keylen);
	lws_map_hashtable_t *ht = (lws_map_hashtable_t *)&map[1];

	lws_start_foreach_dll(struct lws_dll2 *, p,
			      ht[h % map->info.modulo].ho.head) {
		lws_map_item_t *i = lws_container_of(p, lws_map_item_t, list);

		if (!map->info._compare(key, keylen, &i[1], i->keylen))
			return i;
	} lws_end_foreach_dll(p);

	return NULL;
}

const void *
lws_map_item_key(lws_map_item_t *_item)
{
	return ((void *)&_item[1]);
}

const void *
lws_map_item_value(lws_map_item_t *_item)
{
	return (void *)(((uint8_t *)&_item[1]) + _item->keylen);
}

size_t
lws_map_item_key_len(lws_map_item_t *_item)
{
	return _item->keylen;
}

size_t
lws_map_item_value_len(lws_map_item_t *_item)
{
	return _item->valuelen;
}
