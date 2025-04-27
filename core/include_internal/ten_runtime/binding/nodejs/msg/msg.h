//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <node_api.h>

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "ten_utils/lib/signature.h"

#define TEN_NODEJS_MSG_SIGNATURE 0x544E62A3726AAF6EU

typedef struct ten_nodejs_msg_t {
  ten_signature_t signature;

  ten_nodejs_bridge_t bridge;

  ten_shared_ptr_t *msg;
} ten_nodejs_msg_t;

TEN_RUNTIME_PRIVATE_API void ten_nodejs_msg_init_from_c_msg(
    ten_nodejs_msg_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_msg_deinit(ten_nodejs_msg_t *self);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_msg_module_init(napi_env env, napi_value exports);
