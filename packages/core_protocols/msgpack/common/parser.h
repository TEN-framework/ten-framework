//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "msgpack.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_msgpack_parser_t {
  msgpack_unpacker unpacker;
  msgpack_unpacked unpacked;
} ten_msgpack_parser_t;

TEN_RUNTIME_PRIVATE_API void ten_msgpack_parser_init(
    ten_msgpack_parser_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msgpack_parser_deinit(
    ten_msgpack_parser_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msgpack_parser_feed_data(
    ten_msgpack_parser_t *self, const char *data, size_t data_size);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_msgpack_parser_parse_data(
    ten_msgpack_parser_t *self);
