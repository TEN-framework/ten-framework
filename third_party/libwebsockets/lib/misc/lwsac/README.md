## LWS Allocated Chunks

![lwsac flow](/doc-assets/lwsac.svg)

These apis provide a way to manage a linked-list of allocated chunks...

[ HEAD alloc ] -> [ next alloc ] -> [ next alloc ] -> [ curr alloc ]

... and sub-allocate trivially inside the chunks.  These sub-allocations are
not tracked by lwsac at all, there is a "used" high-water mark for each chunk
that's simply advanced by the amount sub-allocated.  If the allocation size
matches the platform pointer alignment, there is zero overhead to sub-allocate
(otherwise the allocation is padded to the next platform pointer alignment
automatically).

If you have an unknown amount of relatively little things to allocate, including
strings or other unstructured data, lwsac is significantly more efficient than
individual allocations using malloc or so.

[lwsac full public api](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-lwsac.h)

## lwsac_use() api

```
/**
 * lwsac_use - allocate / use some memory from a lwsac
 *
 * \param head: pointer to the lwsac list object
 * \param ensure: the number of bytes we want to use
 * \param chunk_size: 0, or the size of the chunk to (over)allocate if
 *			what we want won't fit in the current tail chunk.  If
 *			0, the default value of 4000 is used. If ensure is
 *			larger, it is used instead.
 *
 * This also serves to init the lwsac if *head is NULL.  Basically it does
 * whatever is necessary to return you a pointer to ensure bytes of memory
 * reserved for the caller.
 *
 * Returns NULL if OOM.
 */
LWS_VISIBLE LWS_EXTERN void *
lwsac_use(struct lwsac **head, size_t ensure, size_t chunk_size);
```

When you make an sub-allocation using `lwsac_use()`, you can either
set the `chunk_size` arg to zero, defaulting to 4000, or a specific chunk size.
In the event the requested sub-allocation exceeds the chunk size, the chunk
size is increated to match it automatically for this allocation only.

Subsequent `lwsac_use()` calls will advance internal pointers to use up the
remaining space inside the current chunk if possible; if not enough remaining
space it is skipped, a new allocation is chained on and the request pointed to
there.

Lwsac does not store information about sub-allocations.  There is really zero
overhead for individual sub-allocations (unless their size is not
pointer-aligned, in which case the actual amount sub-allocated is rounded up to
the next pointer alignment automatically).  For structs, which are pointer-
aligned naturally, and a chunk size relatively large for the sub-allocation
size, lwsac is extremely efficient even for huge numbers of small allocations.

This makes lwsac very effective when the total amount of allocation needed is
not known at the start and may be large... it will simply add on chunks to cope
with whatever happens.

## lwsac_free() api

```
/**
 * lwsac_free - deallocate all chunks in the lwsac and set head NULL
 *
 * \param head: pointer to the lwsac list object
 *
 * This deallocates all chunks in the lwsac, then sets *head to NULL.  All
 * lwsac_use() pointers are invalidated in one hit without individual frees.
 */
LWS_VISIBLE LWS_EXTERN void
lwsac_free(struct lwsac **head);
```

When you are finished with the lwsac, you simply free the chain of allocated
chunks using lwsac_free() on the lwsac head.  There's no tracking or individual
destruction of suballocations - the whole chain of chunks the suballocations
live in are freed and invalidated all together.

If the structs stored in the lwsac allocated things **outside** the lwsac, then the
user must unwind through them and perform the frees.  But the idea of lwsac is
things stored in the lwsac also suballocate into the lwsac, and point into the
lwsac if they need to, avoiding any need to visit them during destroy.  It's
like clearing up after a kids' party by gathering up a disposable tablecloth:
no matter what was left on the table, it's all gone in one step.

## `lws_list_ptr` helpers

```
/* sort may be NULL if you don't care about order */
LWS_VISIBLE LWS_EXTERN void
lws_list_ptr_insert(lws_list_ptr *phead, lws_list_ptr *add,
		    lws_list_ptr_sort_func_t sort);
```

A common pattern needed with sub-allocated structs is they are on one or more
linked-list.  To make that simple to do cleanly, `lws_list...` apis are provided
along with a generic insertion function that can take a sort callback.  These
allow a struct to participate on multiple linked-lists simultaneously.

## common const string and blob folding

In some cases the input to be stored in the lwsac may repeat the same tokens
multiple times... if the pattern is to store the string or blob in the lwsac
and then point to it, you can make use of a helper api

```
uint8_t *
lwsac_scan_extant(struct lwsac *head, uint8_t *find, size_t len, int nul);
```

This lets you check in all previous used parts of the lwsac for the same
string or blob, plus optionally a terminal NUL afterwards.  If not found,
it returns `NULL` and you can copy it into the lwsac as usual.  If it is
found, a pointer is returned, and you can use this directly without copying
the string or blob in again.

## optimizations to minimize overhead

If the lwsac will persist in the system for some time, it's desirable to reduce
the memory needed as overhead.  Overhead is created

 - once per chunk... in addition to the malloc overhead, there's an lwsac
   chunk header of 2 x pointers and 2 x size_t
   
 - at the unused part at the end that was allocated but not used
 
A good strategy is to make the initial allocation reflect the minimum expected
size of the overall lwsac in one hit.  Then use a chunk size that is a tradeoff
between the number of chunks that might be needed and the fact that on average,
you can expect to waste half a chunk.  For example if the storage is typically
between 4K - 6K, you could allocate 4K or 4.5K for the first chunk and then fill
in using 256 or 512 byte chunks.

You can measure the overhead in an lwsac using `lwsac_total_overhead()`.

The lwsac apis look first in the unused part of previous chunks, if any, and
will place new allocations there preferentially if they fit.  This helps for the
case lwsac was forced to allocate a new chunk because you asked for something
large, while there was actually significant free space left in the old chunk,
just not enough for that particular allocation.  Subsequent lwsac use can then
"backfill" smaller things there to make best use of allocated space.
