//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdint.h>

#include "msgpack.h"

typedef enum TEN_MSGPACK_EXT_TYPE {
  TEN_MSGPACK_EXT_TYPE_INVALID,

  TEN_MSGPACK_EXT_TYPE_MSG,
} TEN_MSGPACK_EXT_TYPE;

#define MSGPACK_DATA_TYPE (unpacked->data.type)

#define MSGPACK_DATA_BOOL (unpacked->data.via.boolean)
#define MSGPACK_DATA_I64 (unpacked->data.via.i64)
#define MSGPACK_DATA_U64 (unpacked->data.via.u64)

#define MSGPACK_DATA_F64 (unpacked->data.via.f64)

// It's a hack to prevent clang-tidy to issue the following warning:
// warning: Null pointer passed to 2nd parameter expecting 'nonnull'
#define MSGPACK_DATA_STR_PTR \
  (unpacked->data.via.str.ptr ? unpacked->data.via.str.ptr : "")
#define MSGPACK_DATA_STR_SIZE (unpacked->data.via.str.size)

#define MSGPACK_DATA_BIN_PTR \
  (unpacked->data.via.bin.ptr ? unpacked->data.via.bin.ptr : "")
#define MSGPACK_DATA_BIN_SIZE (unpacked->data.via.bin.size)

#define MSGPACK_DATA_MAP_SIZE (unpacked->data.via.map.size)

#define MSGPACK_DATA_ARRAY_SIZE (unpacked->data.via.array.size)

#define MSGPACK_DATA_ARRAY_ITEM_TYPE(idx) \
  (unpacked->data.via.array.ptr[idx].type)
#define MSGPACK_DATA_ARRAY_ITEM_BIN_PTR(idx) \
  (unpacked->data.via.array.ptr[idx].via.bin.ptr)
#define MSGPACK_DATA_ARRAY_ITEM_BIN_SIZE(idx) \
  (unpacked->data.via.array.ptr[idx].via.bin.size)

#define MSGPACK_DATA_ARRAY_ITEM_STR_PTR(idx) \
  (unpacked->data.via.array.ptr[idx].via.str.ptr)
#define MSGPACK_DATA_ARRAY_ITEM_STR_SIZE(idx) \
  (unpacked->data.via.array.ptr[idx].via.str.size)
