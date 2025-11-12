/*
 * MessagePack for C dynamic typing routine
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_OBJECT_H
#define MSGPACK_OBJECT_H

#include "zone.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup msgpack_object Dynamically typed object
 * @ingroup msgpack
 * @{
 */

typedef enum {
    MSGPACK_OBJECT_NIL                  = 0x00,
    MSGPACK_OBJECT_BOOLEAN              = 0x01,
    MSGPACK_OBJECT_POSITIVE_INTEGER     = 0x02,
    MSGPACK_OBJECT_NEGATIVE_INTEGER     = 0x03,
    MSGPACK_OBJECT_FLOAT32              = 0x0a,
    MSGPACK_OBJECT_FLOAT64              = 0x04,
    MSGPACK_OBJECT_FLOAT                = 0x04,
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
    MSGPACK_OBJECT_DOUBLE               = MSGPACK_OBJECT_FLOAT, /* obsolete */
#endif /* MSGPACK_USE_LEGACY_NAME_AS_FLOAT */
    MSGPACK_OBJECT_STR                  = 0x05,
    MSGPACK_OBJECT_ARRAY                = 0x06,
    MSGPACK_OBJECT_MAP                  = 0x07,
    MSGPACK_OBJECT_BIN                  = 0x08,
    MSGPACK_OBJECT_EXT                  = 0x09
} msgpack_object_type;


struct msgpack_object;
struct msgpack_object_kv;

typedef struct {
    uint32_t size;
    struct msgpack_object* ptr;
} msgpack_object_array;

typedef struct {
    uint32_t size;
    struct msgpack_object_kv* ptr;
} msgpack_object_map;

typedef struct {
    uint32_t size;
    const char* ptr;
} msgpack_object_str;

typedef struct {
    uint32_t size;
    const char* ptr;
} msgpack_object_bin;

typedef struct {
    int8_t type;
    uint32_t size;
    const char* ptr;
} msgpack_object_ext;

typedef union {
    bool boolean;
    uint64_t u64;
    int64_t  i64;
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
    double   dec; /* obsolete*/
#endif /* MSGPACK_USE_LEGACY_NAME_AS_FLOAT */
    double   f64;
    msgpack_object_array array;
    msgpack_object_map map;
    msgpack_object_str str;
    msgpack_object_bin bin;
    msgpack_object_ext ext;
} msgpack_object_union;

typedef struct msgpack_object {
    msgpack_object_type type;
    msgpack_object_union via;
} msgpack_object;

typedef struct msgpack_object_kv {
    msgpack_object key;
    msgpack_object val;
} msgpack_object_kv;

MSGPACK_DLLEXPORT
void msgpack_object_init_nil(msgpack_object* d);

MSGPACK_DLLEXPORT
void msgpack_object_init_boolean(msgpack_object* d, bool v);

MSGPACK_DLLEXPORT
void msgpack_object_init_unsigned_integer(msgpack_object* d, uint64_t v);

MSGPACK_DLLEXPORT
void msgpack_object_init_signed_integer(msgpack_object* d, int64_t v);

MSGPACK_DLLEXPORT
void msgpack_object_init_float32(msgpack_object* d, float v);

MSGPACK_DLLEXPORT
void msgpack_object_init_float64(msgpack_object* d, double v);

MSGPACK_DLLEXPORT
void msgpack_object_init_str(msgpack_object* d, const char* data, uint32_t size);

MSGPACK_DLLEXPORT
void msgpack_object_init_bin(msgpack_object* d, const char* data, uint32_t size);

MSGPACK_DLLEXPORT
void msgpack_object_init_ext(msgpack_object* d, int8_t type, const char* data, uint32_t size);

MSGPACK_DLLEXPORT
void msgpack_object_init_array(msgpack_object* d, msgpack_object* data, uint32_t size);

MSGPACK_DLLEXPORT
void msgpack_object_init_map(msgpack_object* d, msgpack_object_kv* data, uint32_t size);

#if !defined(_KERNEL_MODE)
MSGPACK_DLLEXPORT
void msgpack_object_print(FILE* out, msgpack_object o);
#endif

MSGPACK_DLLEXPORT
int msgpack_object_print_buffer(char *buffer, size_t buffer_size, msgpack_object o);

MSGPACK_DLLEXPORT
bool msgpack_object_equal(const msgpack_object x, const msgpack_object y);

/** @} */


#ifdef __cplusplus
}
#endif

#endif /* msgpack/object.h */
