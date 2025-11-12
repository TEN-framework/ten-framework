//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <msgpack/pack.h>
#include <msgpack/unpack.h>

#include "ten_utils/value/value_kv.h"

TEN_RUNTIME_PRIVATE_API bool ten_msgpack_value_deserialize(
    ten_value_t *value, msgpack_unpacker *unpacker, msgpack_unpacked *unpacked);

TEN_RUNTIME_PRIVATE_API bool ten_msgpack_value_deserialize_inplace(
    ten_value_t *value, msgpack_unpacker *unpacker, msgpack_unpacked *unpacked);

TEN_RUNTIME_PRIVATE_API ten_value_t *
ten_msgpack_create_value_through_deserialization(msgpack_unpacker *unpacker,
                                                 msgpack_unpacked *unpacked);

TEN_RUNTIME_PRIVATE_API ten_value_kv_t *
ten_msgpack_create_value_kv_through_deserialization(msgpack_unpacker *unpacker,
                                                    msgpack_unpacked *unpacked);

TEN_RUNTIME_PRIVATE_API void ten_msgpack_value_serialize(ten_value_t *value,
                                                         msgpack_packer *pck);

TEN_RUNTIME_PRIVATE_API void ten_msgpack_value_kv_serialize(
    ten_value_kv_t *kv, msgpack_packer *pck);
