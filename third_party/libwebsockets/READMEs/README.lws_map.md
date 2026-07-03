# lws_map generic map abstraction

|||
|---|---|---|
|cmake|core feature|
|Header| ./include/libwebsockets/lws-map.h|
|api-test| ./minimal-examples/api-tests/api-test-lws_map/|

lws_map provides a robust abstraction for containing a collection of items that
map key objects to value objects, where both the key and value objects may
differ in size each time and have any type.

Its one-level linked-list hashtables are useful up to hundreds or low thousands
of items in the map on may platforms.

The map itself and the items inside it are opaque.

## Creating and destroying the map

The user should prepare a `lws_map_info_t` object, it's legal for it to be
all zeros to select defaults, an 8-way hashtable with item allocation from heap,
simple bytewise key comparison, and xor / shift key hashing.  These are often
what you want simplifying construction.

The info object allows user override of item allocator, freeing, key comparison
and object hashing, allowing custom objects to be keys if desired.

Custom allocator / free implementations for using lwsac for item allocation are
provided to simplify that case.

Just call `lws_map_create()` with the info struct to create the map, later it
and all its contents can be destroyed with `lws_map_destroy()`.  The info struct
can go out of scope immediately after the create call.

```
lws_map_t *
lws_map_create(const lws_map_info_t *info);
void
lws_map_destroy(lws_map_t **pmap);
```

## Keys in lws_map

Items are managed in the map by a key, this may be, eg, a string, but it also
can be an arbitrary object itself.  If comparing keys takes more than a simple
bytewise comparison, the map creation info struct ._compare() operation should
be overridden with a user-supplied one that knows how to use the user's
custom key objects.

Keys are not required to be the same length, so objects with variable size
overallocation can be used as keys.

Keys and values are copied into allocations inside the map, the original objects
they are copied from may go out of scope after item creation assuming there are
no pointers to them copied in the objects themselves.

## Adding items to a map

The map's info._alloc allocator is used for creating items.  By default that
just creates into the heap.

If you create a new item with the same key as an existing one, the existing one
is destroyed before the new one is created.  So there is only one item allowed
at a given key at a time.

To allocate and create a new item containing the key and value, use
`lws_map_item_create()`

```
lws_map_item_t *
lws_map_item_create(lws_map_t *map,
		    const lws_map_key_t key, size_t keylen,
		    const lws_map_value_t value, size_t valuelen);
```

Eg,

```
	if (!lws_map_item_create(map, (lws_map_key_t)&my_key,
				      sizeof(my_key),
				     (lws_map_value_t)"4567", 4))
		/* fail */
```


In the case the key is a string, there is a ..._ks wrapper to simplify usage

```
	if (!lws_map_item_create_ks(map, "123", (lws_map_value_t)"4567", 4))
		/* fail */
```

## Lookups in the map

You can retreive a pointer to an item in the map with a give key using

```
lws_map_item_t *
lws_map_item_lookup(lws_map_t *map, const lws_map_key_t key, size_t keylen);
```

The item is opaque, but there are accessors

|Accessor|Function|
|---|---|
|`lws_map_item_key(lws_map_item_t *_item)`|get a pointer to the item key|
|`lws_map_item_value(lws_map_item_t *_item)`|get a pointer to the item value|
|`lws_map_item_key_len(lws_map_item_t *_item)`|get the key length|
|`lws_map_item_value_len(lws_map_item_t *_item)`|get the value length|

Again there is a ..._ks() helper to simplify C strings as keys

```
	item = lws_map_item_lookup_ks(map, "abc");
	if (!item)
		/* fail */
```
