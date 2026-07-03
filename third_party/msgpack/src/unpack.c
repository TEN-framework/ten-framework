/*
 * MessagePack for C unpacking routine
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#include "msgpack/unpack.h"
#include "msgpack/unpack_define.h"
#include "msgpack/util.h"
#include <stdlib.h>

#ifdef _msgpack_atomic_counter_header
#include _msgpack_atomic_counter_header
#endif


typedef struct {
    msgpack_zone** z;
    bool referenced;
} unpack_user;


#define msgpack_unpack_struct(name) \
    struct template ## name

#define msgpack_unpack_func(ret, name) \
    ret template ## name

#define msgpack_unpack_callback(name) \
    template_callback ## name

#define msgpack_unpack_object msgpack_object

#define msgpack_unpack_user unpack_user


struct template_context;
typedef struct template_context template_context;

static void template_init(template_context* ctx);

static msgpack_object template_data(template_context* ctx);

static int template_execute(
    template_context* ctx, const char* data, size_t len, size_t* off);


static inline msgpack_object template_callback_root(unpack_user* u)
{
    msgpack_object o;
    MSGPACK_UNUSED(u);
    o.type = MSGPACK_OBJECT_NIL;
    return o;
}

static inline int template_callback_uint8(unpack_user* u, uint8_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
    o->via.u64 = d;
    return 0;
}

static inline int template_callback_uint16(unpack_user* u, uint16_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
    o->via.u64 = d;
    return 0;
}

static inline int template_callback_uint32(unpack_user* u, uint32_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
    o->via.u64 = d;
    return 0;
}

static inline int template_callback_uint64(unpack_user* u, uint64_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
    o->via.u64 = d;
    return 0;
}

static inline int template_callback_int8(unpack_user* u, int8_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    if(d >= 0) {
        o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
        o->via.u64 = (uint64_t)d;
        return 0;
    }
    else {
        o->type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
        o->via.i64 = d;
        return 0;
    }
}

static inline int template_callback_int16(unpack_user* u, int16_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    if(d >= 0) {
        o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
        o->via.u64 = (uint64_t)d;
        return 0;
    }
    else {
        o->type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
        o->via.i64 = d;
        return 0;
    }
}

static inline int template_callback_int32(unpack_user* u, int32_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    if(d >= 0) {
        o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
        o->via.u64 = (uint64_t)d;
        return 0;
    }
    else {
        o->type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
        o->via.i64 = d;
        return 0;
    }
}

static inline int template_callback_int64(unpack_user* u, int64_t d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    if(d >= 0) {
        o->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
        o->via.u64 = (uint64_t)d;
        return 0;
    }
    else {
        o->type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
        o->via.i64 = d;
        return 0;
    }
}

static inline int template_callback_float(unpack_user* u, float d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_FLOAT32;
    o->via.f64 = d;
    return 0;
}

static inline int template_callback_double(unpack_user* u, double d, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_FLOAT64;
    o->via.f64 = d;
    return 0;
}

static inline int template_callback_nil(unpack_user* u, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_NIL;
    return 0;
}

static inline int template_callback_true(unpack_user* u, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_BOOLEAN;
    o->via.boolean = true;
    return 0;
}

static inline int template_callback_false(unpack_user* u, msgpack_object* o)
{
    MSGPACK_UNUSED(u);
    o->type = MSGPACK_OBJECT_BOOLEAN;
    o->via.boolean = false;
    return 0;
}

static inline int template_callback_array(unpack_user* u, unsigned int n, msgpack_object* o)
{
    size_t size;
    // Let's leverage the fact that sizeof(msgpack_object) is a compile time constant
    // to check for int overflows.
    // Note - while n is constrained to 32-bit, the product of n * sizeof(msgpack_object)
    // might not be constrained to 4GB on 64-bit systems
#if SIZE_MAX == UINT_MAX
    if (n > SIZE_MAX/sizeof(msgpack_object))
        return MSGPACK_UNPACK_NOMEM_ERROR;
#endif

    o->type = MSGPACK_OBJECT_ARRAY;
    o->via.array.size = 0;

    size = n * sizeof(msgpack_object);

    if (*u->z == NULL) {
        *u->z = msgpack_zone_new(MSGPACK_ZONE_CHUNK_SIZE);
        if(*u->z == NULL) {
            return MSGPACK_UNPACK_NOMEM_ERROR;
        }
    }

    // Unsure whether size = 0 should be an error, and if so, what to return
    o->via.array.ptr = (msgpack_object*)msgpack_zone_malloc(*u->z, size);
    if(o->via.array.ptr == NULL) { return MSGPACK_UNPACK_NOMEM_ERROR; }
    return 0;
}

static inline int template_callback_array_item(unpack_user* u, msgpack_object* c, msgpack_object o)
{
    MSGPACK_UNUSED(u);
#if defined(__GNUC__) && !defined(__clang__)
    memcpy(&c->via.array.ptr[c->via.array.size], &o, sizeof(msgpack_object));
#else  /* __GNUC__ && !__clang__ */
    c->via.array.ptr[c->via.array.size] = o;
#endif /* __GNUC__ && !__clang__ */
    ++c->via.array.size;
    return 0;
}

static inline int template_callback_map(unpack_user* u, unsigned int n, msgpack_object* o)
{
    size_t size;
    // Let's leverage the fact that sizeof(msgpack_object_kv) is a compile time constant
    // to check for int overflows
    // Note - while n is constrained to 32-bit, the product of n * sizeof(msgpack_object)
    // might not be constrained to 4GB on 64-bit systems

    // Note - this will always be false on 64-bit systems
#if SIZE_MAX == UINT_MAX
    if (n > SIZE_MAX/sizeof(msgpack_object_kv))
        return MSGPACK_UNPACK_NOMEM_ERROR;
#endif

    o->type = MSGPACK_OBJECT_MAP;
    o->via.map.size = 0;

    size = n * sizeof(msgpack_object_kv);

    if (*u->z == NULL) {
        *u->z = msgpack_zone_new(MSGPACK_ZONE_CHUNK_SIZE);
        if(*u->z == NULL) {
            return MSGPACK_UNPACK_NOMEM_ERROR;
        }
    }

    // Should size = 0 be an error? If so, what error to return?
    o->via.map.ptr = (msgpack_object_kv*)msgpack_zone_malloc(*u->z, size);
    if(o->via.map.ptr == NULL) { return MSGPACK_UNPACK_NOMEM_ERROR; }
    return 0;
}

static inline int template_callback_map_item(unpack_user* u, msgpack_object* c, msgpack_object k, msgpack_object v)
{
    MSGPACK_UNUSED(u);
#if defined(__GNUC__) && !defined(__clang__)
    memcpy(&c->via.map.ptr[c->via.map.size].key, &k, sizeof(msgpack_object));
    memcpy(&c->via.map.ptr[c->via.map.size].val, &v, sizeof(msgpack_object));
#else  /* __GNUC__ && !__clang__ */
    c->via.map.ptr[c->via.map.size].key = k;
    c->via.map.ptr[c->via.map.size].val = v;
#endif /* __GNUC__ && !__clang__ */
    ++c->via.map.size;
    return 0;
}

static inline int template_callback_str(unpack_user* u, const char* b, const char* p, unsigned int l, msgpack_object* o)
{
    MSGPACK_UNUSED(b);
    if (*u->z == NULL) {
        *u->z = msgpack_zone_new(MSGPACK_ZONE_CHUNK_SIZE);
        if(*u->z == NULL) {
            return MSGPACK_UNPACK_NOMEM_ERROR;
        }
    }
    o->type = MSGPACK_OBJECT_STR;
    o->via.str.ptr = p;
    o->via.str.size = l;
    u->referenced = true;
    return 0;
}

static inline int template_callback_bin(unpack_user* u, const char* b, const char* p, unsigned int l, msgpack_object* o)
{
    MSGPACK_UNUSED(b);
    if (*u->z == NULL) {
        *u->z = msgpack_zone_new(MSGPACK_ZONE_CHUNK_SIZE);
        if(*u->z == NULL) {
            return MSGPACK_UNPACK_NOMEM_ERROR;
        }
    }
    o->type = MSGPACK_OBJECT_BIN;
    o->via.bin.ptr = p;
    o->via.bin.size = l;
    u->referenced = true;
    return 0;
}

static inline int template_callback_ext(unpack_user* u, const char* b, const char* p, unsigned int l, msgpack_object* o)
{
    MSGPACK_UNUSED(b);
    if (l == 0) {
        return MSGPACK_UNPACK_PARSE_ERROR;
    }
    if (*u->z == NULL) {
        *u->z = msgpack_zone_new(MSGPACK_ZONE_CHUNK_SIZE);
        if(*u->z == NULL) {
            return MSGPACK_UNPACK_NOMEM_ERROR;
        }
    }
    o->type = MSGPACK_OBJECT_EXT;
    o->via.ext.type = *p;
    o->via.ext.ptr = p + 1;
    o->via.ext.size = l - 1;
    u->referenced = true;
    return 0;
}

#include "msgpack/unpack_template.h"


#define CTX_CAST(m) ((template_context*)(m))
#define CTX_REFERENCED(mpac) CTX_CAST((mpac)->ctx)->user.referenced

#define COUNTER_SIZE (sizeof(_msgpack_atomic_counter_t))


static inline void init_count(void* buffer)
{
    *(volatile _msgpack_atomic_counter_t*)buffer = 1;
}

static inline void decr_count(void* buffer)
{
    // atomic if(--*(_msgpack_atomic_counter_t*)buffer == 0) { free(buffer); }
    if(_msgpack_sync_decr_and_fetch((volatile _msgpack_atomic_counter_t*)buffer) == 0) {
        free(buffer);
    }
}

static inline void incr_count(void* buffer)
{
    // atomic ++*(_msgpack_atomic_counter_t*)buffer;
    _msgpack_sync_incr_and_fetch((volatile _msgpack_atomic_counter_t*)buffer);
}

static inline _msgpack_atomic_counter_t get_count(void* buffer)
{
    return *(volatile _msgpack_atomic_counter_t*)buffer;
}

bool msgpack_unpacker_init(msgpack_unpacker* mpac, size_t initial_buffer_size)
{
    char* buffer;
    void* ctx;

    if(initial_buffer_size < COUNTER_SIZE) {
        initial_buffer_size = COUNTER_SIZE;
    }

    buffer = (char*)malloc(initial_buffer_size);
    if(buffer == NULL) {
        return false;
    }

    ctx = malloc(sizeof(template_context));
    if(ctx == NULL) {
        free(buffer);
        return false;
    }

    mpac->buffer = buffer;
    mpac->used = COUNTER_SIZE;
    mpac->free = initial_buffer_size - mpac->used;
    mpac->off = COUNTER_SIZE;
    mpac->parsed = 0;
    mpac->initial_buffer_size = initial_buffer_size;
    mpac->z = NULL;
    mpac->ctx = ctx;

    init_count(mpac->buffer);

    template_init(CTX_CAST(mpac->ctx));
    CTX_CAST(mpac->ctx)->user.z = &mpac->z;
    CTX_CAST(mpac->ctx)->user.referenced = false;

    return true;
}

void msgpack_unpacker_destroy(msgpack_unpacker* mpac)
{
    msgpack_zone_free(mpac->z);
    free(mpac->ctx);
    decr_count(mpac->buffer);
}

msgpack_unpacker* msgpack_unpacker_new(size_t initial_buffer_size)
{
    msgpack_unpacker* mpac = (msgpack_unpacker*)malloc(sizeof(msgpack_unpacker));
    if(mpac == NULL) {
        return NULL;
    }

    if(!msgpack_unpacker_init(mpac, initial_buffer_size)) {
        free(mpac);
        return NULL;
    }

    return mpac;
}

void msgpack_unpacker_free(msgpack_unpacker* mpac)
{
    msgpack_unpacker_destroy(mpac);
    free(mpac);
}

bool msgpack_unpacker_expand_buffer(msgpack_unpacker* mpac, size_t size)
{
    if(mpac->used == mpac->off && get_count(mpac->buffer) == 1
            && !CTX_REFERENCED(mpac)) {
        // rewind buffer
        mpac->free += mpac->used - COUNTER_SIZE;
        mpac->used = COUNTER_SIZE;
        mpac->off = COUNTER_SIZE;

        if(mpac->free >= size) {
            return true;
        }
    }

    if(mpac->off == COUNTER_SIZE) {
        char* tmp;
        size_t next_size = (mpac->used + mpac->free) * 2;  // include COUNTER_SIZE
        while(next_size < size + mpac->used) {
            size_t tmp_next_size = next_size * 2;
            if (tmp_next_size <= next_size) {
                next_size = size + mpac->used;
                break;
            }
            next_size = tmp_next_size;
        }

        tmp = (char*)realloc(mpac->buffer, next_size);
        if(tmp == NULL) {
            return false;
        }

        mpac->buffer = tmp;
        mpac->free = next_size - mpac->used;

    } else {
        char* tmp;
        size_t next_size = mpac->initial_buffer_size;  // include COUNTER_SIZE
        size_t not_parsed = mpac->used - mpac->off;
        while(next_size < size + not_parsed + COUNTER_SIZE) {
            size_t tmp_next_size = next_size * 2;
            if (tmp_next_size <= next_size) {
                next_size = size + not_parsed + COUNTER_SIZE;
                break;
            }
            next_size = tmp_next_size;
        }

        tmp = (char*)malloc(next_size);
        if(tmp == NULL) {
            return false;
        }

        init_count(tmp);

        memcpy(tmp+COUNTER_SIZE, mpac->buffer+mpac->off, not_parsed);

        if(CTX_REFERENCED(mpac)) {
            if(!msgpack_zone_push_finalizer(mpac->z, decr_count, mpac->buffer)) {
                free(tmp);
                return false;
            }
            CTX_REFERENCED(mpac) = false;
        } else {
            decr_count(mpac->buffer);
        }

        mpac->buffer = tmp;
        mpac->used = not_parsed + COUNTER_SIZE;
        mpac->free = next_size - mpac->used;
        mpac->off = COUNTER_SIZE;
    }

    return true;
}

int msgpack_unpacker_execute(msgpack_unpacker* mpac)
{
    size_t off = mpac->off;
    int ret = template_execute(CTX_CAST(mpac->ctx),
            mpac->buffer, mpac->used, &mpac->off);
    if(mpac->off > off) {
        mpac->parsed += mpac->off - off;
    }
    return ret;
}

msgpack_object msgpack_unpacker_data(msgpack_unpacker* mpac)
{
    return template_data(CTX_CAST(mpac->ctx));
}

msgpack_zone* msgpack_unpacker_release_zone(msgpack_unpacker* mpac)
{
    msgpack_zone* old = mpac->z;

    if (old == NULL) return NULL;
    if(!msgpack_unpacker_flush_zone(mpac)) {
        return NULL;
    }

    mpac->z = NULL;
    CTX_CAST(mpac->ctx)->user.z = &mpac->z;

    return old;
}

void msgpack_unpacker_reset_zone(msgpack_unpacker* mpac)
{
    msgpack_zone_clear(mpac->z);
}

bool msgpack_unpacker_flush_zone(msgpack_unpacker* mpac)
{
    if(CTX_REFERENCED(mpac)) {
        if(!msgpack_zone_push_finalizer(mpac->z, decr_count, mpac->buffer)) {
            return false;
        }
        CTX_REFERENCED(mpac) = false;

        incr_count(mpac->buffer);
    }

    return true;
}

void msgpack_unpacker_reset(msgpack_unpacker* mpac)
{
    template_init(CTX_CAST(mpac->ctx));
    // don't reset referenced flag
    mpac->parsed = 0;
}

static inline msgpack_unpack_return unpacker_next(msgpack_unpacker* mpac,
                                                  msgpack_unpacked* result)
{
    int ret;

    msgpack_unpacked_destroy(result);

    ret = msgpack_unpacker_execute(mpac);

    if(ret < 0) {
        result->zone = NULL;
        memset(&result->data, 0, sizeof(msgpack_object));
        return (msgpack_unpack_return)ret;
    }

    if(ret == 0) {
        return MSGPACK_UNPACK_CONTINUE;
    }
    result->zone = msgpack_unpacker_release_zone(mpac);
    result->data = msgpack_unpacker_data(mpac);

    return MSGPACK_UNPACK_SUCCESS;
}

msgpack_unpack_return msgpack_unpacker_next(msgpack_unpacker* mpac,
                                            msgpack_unpacked* result)
{
    msgpack_unpack_return ret;

    ret = unpacker_next(mpac, result);
    if (ret == MSGPACK_UNPACK_SUCCESS) {
        msgpack_unpacker_reset(mpac);
    }

    return ret;
}

msgpack_unpack_return
msgpack_unpacker_next_with_size(msgpack_unpacker* mpac,
                                msgpack_unpacked* result, size_t *p_bytes)
{
    msgpack_unpack_return ret;

    ret = unpacker_next(mpac, result);
    if (ret == MSGPACK_UNPACK_SUCCESS || ret == MSGPACK_UNPACK_CONTINUE) {
        *p_bytes = mpac->parsed;
    }

    if (ret == MSGPACK_UNPACK_SUCCESS) {
        msgpack_unpacker_reset(mpac);
    }

    return ret;
}

msgpack_unpack_return
msgpack_unpack(const char* data, size_t len, size_t* off,
        msgpack_zone* result_zone, msgpack_object* result)
{
    size_t noff = 0;
    if(off != NULL) { noff = *off; }

    if(len <= noff) {
        // FIXME
        return MSGPACK_UNPACK_CONTINUE;
    }
    else {
        int e;
        template_context ctx;
        template_init(&ctx);

        ctx.user.z = &result_zone;
        ctx.user.referenced = false;

        e = template_execute(&ctx, data, len, &noff);
        if(e < 0) {
            return (msgpack_unpack_return)e;
        }

        if(off != NULL) { *off = noff; }

        if(e == 0) {
            return MSGPACK_UNPACK_CONTINUE;
        }

        *result = template_data(&ctx);

        if(noff < len) {
            return MSGPACK_UNPACK_EXTRA_BYTES;
        }

        return MSGPACK_UNPACK_SUCCESS;
    }
}

msgpack_unpack_return
msgpack_unpack_next(msgpack_unpacked* result,
        const char* data, size_t len, size_t* off)
{
    size_t noff = 0;
    msgpack_unpacked_destroy(result);

    if(off != NULL) { noff = *off; }

    if(len <= noff) {
        return MSGPACK_UNPACK_CONTINUE;
    }

    {
        int e;
        template_context ctx;
        template_init(&ctx);

        ctx.user.z = &result->zone;
        ctx.user.referenced = false;

        e = template_execute(&ctx, data, len, &noff);

        if(off != NULL) { *off = noff; }

        if(e < 0) {
            msgpack_zone_free(result->zone);
            result->zone = NULL;
            return (msgpack_unpack_return)e;
        }

        if(e == 0) {
            return MSGPACK_UNPACK_CONTINUE;
        }

        result->data = template_data(&ctx);

        return MSGPACK_UNPACK_SUCCESS;
    }
}

#if defined(MSGPACK_OLD_COMPILER_BUS_ERROR_WORKAROUND)
// FIXME: Dirty hack to avoid a bus error caused by OS X's old gcc.
static void dummy_function_to_avoid_bus_error()
{
}
#endif
