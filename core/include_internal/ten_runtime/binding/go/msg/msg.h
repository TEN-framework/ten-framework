//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "src/ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_utils/lib/signature.h"

#define TEN_GO_MSG_SIGNATURE 0xB0E144BC5D3B1AB9U

typedef struct ten_go_msg_t {
  ten_signature_t signature;

  ten_shared_ptr_t *c_msg;
  ten_go_handle_t go_msg;
} ten_go_msg_t;

TEN_RUNTIME_PRIVATE_API ten_go_msg_t *ten_go_msg_reinterpret(uintptr_t msg);

TEN_RUNTIME_PRIVATE_API bool ten_go_msg_check_integrity(ten_go_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_msg_go_handle(ten_go_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_go_msg_c_msg(ten_go_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_go_msg_move_c_msg(
    ten_go_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_msg_t *ten_go_msg_create(
    ten_shared_ptr_t *c_msg);

TEN_RUNTIME_PRIVATE_API void ten_go_msg_set_go_handle(
    ten_go_msg_t *self, ten_go_handle_t go_handle);
