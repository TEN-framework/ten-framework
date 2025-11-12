//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdint.h>

#include "core_protocols/msgpack/common/parser.h"
#include "include_internal/ten_runtime/msg/msg.h"

typedef struct ten_msg_deserialize_info_t {
  msgpack_unpacker *unpacker;
  msgpack_unpacked *unpacked;
} ten_msg_deserialize_info_t;

TEN_RUNTIME_PRIVATE_API ten_msg_deserialize_info_t *
ten_msg_deserialize_info_create(msgpack_unpacker *unpacker,
                                msgpack_unpacked *unpacked);

TEN_RUNTIME_PRIVATE_API void ten_msg_deserialize_info_destroy(
    ten_msg_deserialize_info_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_msgpack_serialize_msg(ten_shared_ptr_t *self,
                                                       msgpack_packer *pck,
                                                       ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_msgpack_deserialize_msg(
    msgpack_unpacker *unpacker, msgpack_unpacked *unpacked);

TEN_RUNTIME_API ten_buf_t ten_msgpack_serialize_msgs(ten_list_t *msgs,
                                                     ten_error_t *err);

TEN_RUNTIME_API void ten_msgpack_deserialize_msgs(ten_msgpack_parser_t *parser,
                                                  ten_buf_t input_buf,
                                                  ten_list_t *result_msgs);
