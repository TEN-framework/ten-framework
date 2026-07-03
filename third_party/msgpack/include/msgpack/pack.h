/*
 * MessagePack for C packing routine
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_PACK_H
#define MSGPACK_PACK_H

#include "pack_define.h"
#include "object.h"
#include "timestamp.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup msgpack_buffer Buffers
 * @ingroup msgpack
 * @{
 * @}
 */

/**
 * @defgroup msgpack_pack Serializer
 * @ingroup msgpack
 * @{
 */

typedef int (*msgpack_packer_write)(void* data, const char* buf, size_t len);

typedef struct msgpack_packer {
    void* data;
    msgpack_packer_write callback;
} msgpack_packer;

static void msgpack_packer_init(msgpack_packer* pk, void* data, msgpack_packer_write callback);

static msgpack_packer* msgpack_packer_new(void* data, msgpack_packer_write callback);
static void msgpack_packer_free(msgpack_packer* pk);

static int msgpack_pack_char(msgpack_packer* pk, char d);

static int msgpack_pack_signed_char(msgpack_packer* pk, signed char d);
static int msgpack_pack_short(msgpack_packer* pk, short d);
static int msgpack_pack_int(msgpack_packer* pk, int d);
static int msgpack_pack_long(msgpack_packer* pk, long d);
static int msgpack_pack_long_long(msgpack_packer* pk, long long d);
static int msgpack_pack_unsigned_char(msgpack_packer* pk, unsigned char d);
static int msgpack_pack_unsigned_short(msgpack_packer* pk, unsigned short d);
static int msgpack_pack_unsigned_int(msgpack_packer* pk, unsigned int d);
static int msgpack_pack_unsigned_long(msgpack_packer* pk, unsigned long d);
static int msgpack_pack_unsigned_long_long(msgpack_packer* pk, unsigned long long d);

static int msgpack_pack_uint8(msgpack_packer* pk, uint8_t d);
static int msgpack_pack_uint16(msgpack_packer* pk, uint16_t d);
static int msgpack_pack_uint32(msgpack_packer* pk, uint32_t d);
static int msgpack_pack_uint64(msgpack_packer* pk, uint64_t d);
static int msgpack_pack_int8(msgpack_packer* pk, int8_t d);
static int msgpack_pack_int16(msgpack_packer* pk, int16_t d);
static int msgpack_pack_int32(msgpack_packer* pk, int32_t d);
static int msgpack_pack_int64(msgpack_packer* pk, int64_t d);

static int msgpack_pack_fix_uint8(msgpack_packer* pk, uint8_t d);
static int msgpack_pack_fix_uint16(msgpack_packer* pk, uint16_t d);
static int msgpack_pack_fix_uint32(msgpack_packer* pk, uint32_t d);
static int msgpack_pack_fix_uint64(msgpack_packer* pk, uint64_t d);
static int msgpack_pack_fix_int8(msgpack_packer* pk, int8_t d);
static int msgpack_pack_fix_int16(msgpack_packer* pk, int16_t d);
static int msgpack_pack_fix_int32(msgpack_packer* pk, int32_t d);
static int msgpack_pack_fix_int64(msgpack_packer* pk, int64_t d);

static int msgpack_pack_float(msgpack_packer* pk, float d);
static int msgpack_pack_double(msgpack_packer* pk, double d);

static int msgpack_pack_nil(msgpack_packer* pk);
static int msgpack_pack_true(msgpack_packer* pk);
static int msgpack_pack_false(msgpack_packer* pk);

static int msgpack_pack_array(msgpack_packer* pk, size_t n);

static int msgpack_pack_map(msgpack_packer* pk, size_t n);

static int msgpack_pack_str(msgpack_packer* pk, size_t l);
static int msgpack_pack_str_body(msgpack_packer* pk, const void* b, size_t l);
static int msgpack_pack_str_with_body(msgpack_packer* pk, const void* b, size_t l);

static int msgpack_pack_v4raw(msgpack_packer* pk, size_t l);
static int msgpack_pack_v4raw_body(msgpack_packer* pk, const void* b, size_t l);

static int msgpack_pack_bin(msgpack_packer* pk, size_t l);
static int msgpack_pack_bin_body(msgpack_packer* pk, const void* b, size_t l);
static int msgpack_pack_bin_with_body(msgpack_packer* pk, const void* b, size_t l);

static int msgpack_pack_ext(msgpack_packer* pk, size_t l, int8_t type);
static int msgpack_pack_ext_body(msgpack_packer* pk, const void* b, size_t l);
static int msgpack_pack_ext_with_body(msgpack_packer* pk, const void* b, size_t l, int8_t type);

static int msgpack_pack_timestamp(msgpack_packer* pk, const msgpack_timestamp* d);

MSGPACK_DLLEXPORT
int msgpack_pack_object(msgpack_packer* pk, msgpack_object d);


/** @} */


#define msgpack_pack_inline_func(name) \
    inline int msgpack_pack ## name

#define msgpack_pack_inline_func_cint(name) \
    inline int msgpack_pack ## name

#define msgpack_pack_inline_func_fixint(name) \
    inline int msgpack_pack_fix ## name

#define msgpack_pack_user msgpack_packer*

#define msgpack_pack_append_buffer(user, buf, len) \
    return (*(user)->callback)((user)->data, (const char*)buf, len)

#include "pack_template.h"

inline void msgpack_packer_init(msgpack_packer* pk, void* data, msgpack_packer_write callback)
{
    pk->data = data;
    pk->callback = callback;
}

inline msgpack_packer* msgpack_packer_new(void* data, msgpack_packer_write callback)
{
    msgpack_packer* pk = (msgpack_packer*)calloc(1, sizeof(msgpack_packer));
    if(!pk) { return NULL; }
    msgpack_packer_init(pk, data, callback);
    return pk;
}

inline void msgpack_packer_free(msgpack_packer* pk)
{
    free(pk);
}

inline int msgpack_pack_str_with_body(msgpack_packer* pk, const void* b, size_t l)
 {
     int ret = msgpack_pack_str(pk, l);
     if (ret != 0) { return ret; }
     return msgpack_pack_str_body(pk, b, l);
 }

 inline int msgpack_pack_bin_with_body(msgpack_packer* pk, const void* b, size_t l)
 {
     int ret = msgpack_pack_bin(pk, l);
     if (ret != 0) { return ret; }
     return msgpack_pack_bin_body(pk, b, l);
 }

 inline int msgpack_pack_ext_with_body(msgpack_packer* pk, const void* b, size_t l, int8_t type)
 {
     int ret = msgpack_pack_ext(pk, l, type);
     if (ret != 0) { return ret; }
     return msgpack_pack_ext_body(pk, b, l);
 }
 
#ifdef __cplusplus
}
#endif

#endif /* msgpack/pack.h */
