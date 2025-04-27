//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/msg/msg.h"

typedef struct ten_msg_t ten_msg_t;
typedef struct msgpack_packer msgpack_packer;
typedef struct msgpack_unpacker msgpack_unpacker;
typedef struct msgpack_unpacked msgpack_unpacked;

TEN_RUNTIME_PRIVATE_API void ten_msgpack_msg_type_serialize(
    ten_msg_t *self, msgpack_packer *pck);

TEN_RUNTIME_PRIVATE_API TEN_MSG_TYPE ten_msgpack_deserialize_msg_type(
    msgpack_unpacker *unpacker, msgpack_unpacked *unpacked);
