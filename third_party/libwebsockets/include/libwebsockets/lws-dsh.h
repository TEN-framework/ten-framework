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

/*
 * lws_dsh (Disordered Shared Heap) is an opaque abstraction supporting a single
 * linear buffer (overallocated at end of the lws_dsh_t) which may contain
 * multiple kinds of packets that are retired out of order, and tracked by kind.
 *
 * Each kind of packet has an lws_dll2 list of its kind of packets and acts as
 * a FIFO; packets of a particular type are always retired in order.  But there
 * is no requirement about the order types are retired matching the original
 * order they arrived.
 * 
 * Gaps are tracked as just another kind of "packet" list.
 *
 * "allocations" (including gaps) are prepended by an lws_dsh_object_t.
 *
 * dsh may themselves be on an lws_dll2_owner list, and under memory pressure
 * allocate into other buffers on the list.
 *
 * All management structures exist inside the allocated buffer.
 */

/**
 * lws_dsh_create() - Allocate a DSH buffer
 *
 * \param owner: the owning list this dsh belongs on, or NULL if standalone
 * \param buffer_size: the allocation in bytes
 * \param count_kinds: how many separately-tracked fifos use the buffer
 *
 * This makes a single heap allocation that includes internal tracking objects
 * in the buffer.  Sub-allocated objects are bound to a "kind" index and
 * managed via a FIFO for each kind.
 *
 * Every "kind" of allocation shares the same buffer space.
 *
 * Multiple buffers may be bound together in an lws_dll2 list, and if an
 * allocation cannot be satisfied by the local buffer, space can be borrowed
 * from other dsh in the same list (the local dsh FIFO tracks these "foreign"
 * allocations as if they were local).
 *
 * Returns an opaque pointer to the dsh, or NULL if allocation failed.
 */
LWS_VISIBLE LWS_EXTERN struct lws_dsh *
lws_dsh_create(lws_dll2_owner_t *owner, size_t buffer_size, int count_kinds);

/**
 * lws_dsh_destroy() - Destroy a DSH buffer
 *
 * \param pdsh: pointer to the dsh pointer
 *
 * Deallocates the DSH and sets *pdsh to NULL.
 *
 * Before destruction, any foreign buffer usage on the part of this dsh are
 * individually freed.  All dsh on the same list are walked and checked if they
 * have their own foreign allocations on the dsh buffer being destroyed.  If so,
 * it attempts to migrate the allocation to a dsh that is not currently being
 * destroyed.  If all else fails (basically the buffer memory is being shrunk)
 * unmigratable objects are cleanly destroyed.
 */
LWS_VISIBLE LWS_EXTERN void
lws_dsh_destroy(struct lws_dsh **pdsh);

/**
 * lws_dsh_alloc_tail() - make a suballocation inside a dsh
 *
 * \param dsh: the dsh tracking the allocation
 * \param kind: the kind of allocation
 * \param src1: the first source data to copy
 * \param size1: the size of the first source data
 * \param src2: the second source data to copy (after the first), or NULL
 * \param size2: the size of the second source data
 *
 * Allocates size1 + size2 bytes in a dsh (it prefers the given dsh but will
 * borrow space from other dsh on the same list if necessary) and copies size1
 * bytes into it from src1, followed by size2 bytes from src2 if src2 isn't
 * NULL.  The actual suballocation is a bit larger because of alignment and a
 * prepended management header.
 *
 * The suballocation is added to the kind-specific FIFO at the tail.
 */
LWS_VISIBLE LWS_EXTERN int
lws_dsh_alloc_tail(struct lws_dsh *dsh, int kind, const void *src1,
		   size_t size1, const void *src2, size_t size2);

/**
 * lws_dsh_free() - free a suballocation from the dsh
 *
 * \param obj: a pointer to a void * that pointed to the allocated payload
 *
 * This returns the space used by \p obj in the dsh buffer to the free list
 * of the dsh the allocation came from.
 */
LWS_VISIBLE LWS_EXTERN void
lws_dsh_free(void **obj);

LWS_VISIBLE LWS_EXTERN size_t
lws_dsh_get_size(struct lws_dsh *dsh, int kind);

/**
 * lws_dsh_get_head() - get the head allocation inside the dsh
 *
 * \param dsh: the dsh tracking the allocation
 * \param kind: the kind of allocation
 * \param obj: pointer to a void * to be set to the payload
 * \param size: set to the size of the allocation
 *
 * This gets the "next" object in the kind FIFO for the dsh, and returns 0 if
 * any.  If none, returns nonzero.
 *
 * This is nondestructive of the fifo or the payload.  Use lws_dsh_free on
 * obj to remove the entry from the kind fifo and return the payload to the
 * free list.
 */
LWS_VISIBLE LWS_EXTERN int
lws_dsh_get_head(struct lws_dsh *dsh, int kind, void **obj, size_t *size);

/**
 * lws_dsh_describe() - DEBUG BUILDS ONLY dump the dsh to the logs
 *
 * \param dsh: the dsh to dump
 * \param desc: text that appears at the top of the dump
 *
 * Useful information for debugging lws_dsh
 */
LWS_VISIBLE LWS_EXTERN void
lws_dsh_describe(struct lws_dsh *dsh, const char *desc);
