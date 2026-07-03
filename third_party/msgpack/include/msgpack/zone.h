/*
 * MessagePack for C memory pool implementation
 *
 * Copyright (C) 2008-2010 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_ZONE_H
#define MSGPACK_ZONE_H

#include "sysdep.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup msgpack_zone Memory zone
 * @ingroup msgpack
 * @{
 */

typedef struct msgpack_zone_finalizer {
    void (*func)(void* data);
    void* data;
} msgpack_zone_finalizer;

typedef struct msgpack_zone_finalizer_array {
    msgpack_zone_finalizer* tail;
    msgpack_zone_finalizer* end;
    msgpack_zone_finalizer* array;
} msgpack_zone_finalizer_array;

struct msgpack_zone_chunk;
typedef struct msgpack_zone_chunk msgpack_zone_chunk;

typedef struct msgpack_zone_chunk_list {
    size_t free;
    char* ptr;
    msgpack_zone_chunk* head;
} msgpack_zone_chunk_list;

typedef struct msgpack_zone {
    msgpack_zone_chunk_list chunk_list;
    msgpack_zone_finalizer_array finalizer_array;
    size_t chunk_size;
} msgpack_zone;

#ifndef MSGPACK_ZONE_CHUNK_SIZE
#define MSGPACK_ZONE_CHUNK_SIZE 8192
#endif

MSGPACK_DLLEXPORT
bool msgpack_zone_init(msgpack_zone* zone, size_t chunk_size);
MSGPACK_DLLEXPORT
void msgpack_zone_destroy(msgpack_zone* zone);

MSGPACK_DLLEXPORT
msgpack_zone* msgpack_zone_new(size_t chunk_size);
MSGPACK_DLLEXPORT
void msgpack_zone_free(msgpack_zone* zone);

static inline void* msgpack_zone_malloc(msgpack_zone* zone, size_t size);
static inline void* msgpack_zone_malloc_no_align(msgpack_zone* zone, size_t size);

static inline bool msgpack_zone_push_finalizer(msgpack_zone* zone,
        void (*func)(void* data), void* data);

static inline void msgpack_zone_swap(msgpack_zone* a, msgpack_zone* b);

MSGPACK_DLLEXPORT
bool msgpack_zone_is_empty(msgpack_zone* zone);

MSGPACK_DLLEXPORT
void msgpack_zone_clear(msgpack_zone* zone);

/** @} */


#ifndef MSGPACK_ZONE_ALIGN
#define MSGPACK_ZONE_ALIGN sizeof(void*)
#endif

MSGPACK_DLLEXPORT
void* msgpack_zone_malloc_expand(msgpack_zone* zone, size_t size);

static inline void* msgpack_zone_malloc_no_align(msgpack_zone* zone, size_t size)
{
    char* ptr;
    msgpack_zone_chunk_list* cl = &zone->chunk_list;

    if(zone->chunk_list.free < size) {
        return msgpack_zone_malloc_expand(zone, size);
    }

    ptr = cl->ptr;
    cl->free -= size;
    cl->ptr  += size;

    return ptr;
}

static inline void* msgpack_zone_malloc(msgpack_zone* zone, size_t size)
{
    char* aligned =
        (char*)(
            (uintptr_t)(
                zone->chunk_list.ptr + (MSGPACK_ZONE_ALIGN - 1)
            ) & ~(uintptr_t)(MSGPACK_ZONE_ALIGN - 1)
        );
    size_t adjusted_size = size + (size_t)(aligned - zone->chunk_list.ptr);
    if(zone->chunk_list.free >= adjusted_size) {
        zone->chunk_list.free -= adjusted_size;
        zone->chunk_list.ptr  += adjusted_size;
        return aligned;
    }
    {
        void* ptr = msgpack_zone_malloc_expand(zone, size + (MSGPACK_ZONE_ALIGN - 1));
        if (ptr) {
            return (char*)((uintptr_t)(ptr) & ~(uintptr_t)(MSGPACK_ZONE_ALIGN - 1));
        }
    }
    return NULL;
}


bool msgpack_zone_push_finalizer_expand(msgpack_zone* zone,
        void (*func)(void* data), void* data);

static inline bool msgpack_zone_push_finalizer(msgpack_zone* zone,
        void (*func)(void* data), void* data)
{
    msgpack_zone_finalizer_array* const fa = &zone->finalizer_array;
    msgpack_zone_finalizer* fin = fa->tail;

    if(fin == fa->end) {
        return msgpack_zone_push_finalizer_expand(zone, func, data);
    }

    fin->func = func;
    fin->data = data;

    ++fa->tail;

    return true;
}

static inline void msgpack_zone_swap(msgpack_zone* a, msgpack_zone* b)
{
    msgpack_zone tmp = *a;
    *a = *b;
    *b = tmp;
}


#ifdef __cplusplus
}
#endif

#endif /* msgpack/zone.h */
