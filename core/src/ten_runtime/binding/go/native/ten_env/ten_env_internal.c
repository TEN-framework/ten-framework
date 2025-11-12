//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"

#include <stdint.h>

#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"

ten_go_callback_ctx_t *ten_go_callback_ctx_create(ten_go_handle_t handler_id) {
  ten_go_callback_ctx_t *ctx =
      (ten_go_callback_ctx_t *)TEN_MALLOC(sizeof(ten_go_callback_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->callback_id = handler_id;

  return ctx;
}

void ten_go_callback_ctx_destroy(ten_go_callback_ctx_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  TEN_FREE(self);
}
