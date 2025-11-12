/*
 * MessagePack for C zero-copy buffer implementation
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_VREFBUFFER_H
#define MSGPACK_VREFBUFFER_H

#include "zone.h"
#include <stdlib.h>
#include <assert.h>

#if defined(unix) || defined(__unix) || defined(__linux__) || defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__QNX__) || defined(__QNXTO__) || defined(__HAIKU__)
#include <sys/uio.h>
typedef struct iovec msgpack_iovec;
#else
struct msgpack_iovec {
    void  *iov_base;
    size_t iov_len;
};
typedef struct msgpack_iovec msgpack_iovec;
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup msgpack_vrefbuffer Vectored Referencing buffer
 * @ingroup msgpack_buffer
 * @{
 */

struct msgpack_vrefbuffer_chunk;
typedef struct msgpack_vrefbuffer_chunk msgpack_vrefbuffer_chunk;

typedef struct msgpack_vrefbuffer_inner_buffer {
    size_t free;
    char*  ptr;
    msgpack_vrefbuffer_chunk* head;
} msgpack_vrefbuffer_inner_buffer;

typedef struct msgpack_vrefbuffer {
    msgpack_iovec* tail;
    msgpack_iovec* end;
    msgpack_iovec* array;

    size_t chunk_size;
    size_t ref_size;

    msgpack_vrefbuffer_inner_buffer inner_buffer;
} msgpack_vrefbuffer;


#ifndef MSGPACK_VREFBUFFER_REF_SIZE
#define MSGPACK_VREFBUFFER_REF_SIZE 32
#endif

#ifndef MSGPACK_VREFBUFFER_CHUNK_SIZE
#define MSGPACK_VREFBUFFER_CHUNK_SIZE 8192
#endif

MSGPACK_DLLEXPORT
bool msgpack_vrefbuffer_init(msgpack_vrefbuffer* vbuf,
        size_t ref_size, size_t chunk_size);
MSGPACK_DLLEXPORT
void msgpack_vrefbuffer_destroy(msgpack_vrefbuffer* vbuf);

static inline msgpack_vrefbuffer* msgpack_vrefbuffer_new(size_t ref_size, size_t chunk_size);
static inline void msgpack_vrefbuffer_free(msgpack_vrefbuffer* vbuf);

static inline int msgpack_vrefbuffer_write(void* data, const char* buf, size_t len);

static inline const msgpack_iovec* msgpack_vrefbuffer_vec(const msgpack_vrefbuffer* vref);
static inline size_t msgpack_vrefbuffer_veclen(const msgpack_vrefbuffer* vref);

MSGPACK_DLLEXPORT
int msgpack_vrefbuffer_append_copy(msgpack_vrefbuffer* vbuf,
        const char* buf, size_t len);

MSGPACK_DLLEXPORT
int msgpack_vrefbuffer_append_ref(msgpack_vrefbuffer* vbuf,
        const char* buf, size_t len);

MSGPACK_DLLEXPORT
int msgpack_vrefbuffer_migrate(msgpack_vrefbuffer* vbuf, msgpack_vrefbuffer* to);

MSGPACK_DLLEXPORT
void msgpack_vrefbuffer_clear(msgpack_vrefbuffer* vref);

/** @} */


static inline msgpack_vrefbuffer* msgpack_vrefbuffer_new(size_t ref_size, size_t chunk_size)
{
    msgpack_vrefbuffer* vbuf = (msgpack_vrefbuffer*)malloc(sizeof(msgpack_vrefbuffer));
    if (vbuf == NULL) return NULL;
    if(!msgpack_vrefbuffer_init(vbuf, ref_size, chunk_size)) {
        free(vbuf);
        return NULL;
    }
    return vbuf;
}

static inline void msgpack_vrefbuffer_free(msgpack_vrefbuffer* vbuf)
{
    if(vbuf == NULL) { return; }
    msgpack_vrefbuffer_destroy(vbuf);
    free(vbuf);
}

static inline int msgpack_vrefbuffer_write(void* data, const char* buf, size_t len)
{
    msgpack_vrefbuffer* vbuf = (msgpack_vrefbuffer*)data;
    assert(buf || len == 0);

    if(!buf) return 0;

    if(len < vbuf->ref_size) {
        return msgpack_vrefbuffer_append_copy(vbuf, buf, len);
    } else {
        return msgpack_vrefbuffer_append_ref(vbuf, buf, len);
    }
}

static inline const msgpack_iovec* msgpack_vrefbuffer_vec(const msgpack_vrefbuffer* vref)
{
    return vref->array;
}

static inline size_t msgpack_vrefbuffer_veclen(const msgpack_vrefbuffer* vref)
{
    return (size_t)(vref->tail - vref->array);
}


#ifdef __cplusplus
}
#endif

#endif /* msgpack/vrefbuffer.h */
