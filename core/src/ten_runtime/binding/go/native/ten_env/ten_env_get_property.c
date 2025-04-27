//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <stdint.h>

#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/binding/go/value/value.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/binding/go/interface/ten_runtime/value.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

typedef struct ten_go_ten_env_notify_peek_property_ctx_t {
  ten_string_t path;

  uint8_t *type;
  uintptr_t *size;
  uintptr_t *value_addr;
  uintptr_t callback_handle;
} ten_go_ten_env_notify_peek_property_ctx_t;

static ten_go_ten_env_notify_peek_property_ctx_t *
ten_env_notify_peek_property_ctx_create(const void *path, int path_len,
                                        uint8_t *type, uintptr_t *size,
                                        uintptr_t *value_addr,
                                        uintptr_t callback_handle) {
  ten_go_ten_env_notify_peek_property_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_go_ten_env_notify_peek_property_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_string_init_formatted(&ctx->path, "%.*s", path_len, path);
  ctx->type = type;
  ctx->size = size;
  ctx->value_addr = value_addr;
  ctx->callback_handle = callback_handle;

  return ctx;
}

static void ten_env_notify_peek_property_ctx_destroy(
    ten_go_ten_env_notify_peek_property_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->path);
  ctx->type = NULL;
  ctx->size = NULL;
  ctx->value_addr = NULL;
  ctx->callback_handle = 0;

  TEN_FREE(ctx);
}

static void ten_env_proxy_notify_peek_property(ten_env_t *ten_env,
                                               void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_go_ten_env_notify_peek_property_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // In the extension thread now.

  // The value shall be cloned (the third parameter of ten_get_property is
  // `true`) to ensure the value integrity.
  //
  // Imagine the following scenario:
  //
  // 1. There are two goroutine in one extension. Goroutine A wants to get the
  //    property "p" from the ten instance bound to the extension, and goroutine
  //    B wants to update the property "p" in the same ten instance. Goroutine A
  //    and B run in parallel, that A runs on thread M1 and B runs on thread M2
  //    in GO world.
  //
  // 2. Then the `get` and `set` operations are executed in the extension thread
  //    in order.
  //
  // 3. The `get` operation is executed first, then a `ten_value_t*` is passed
  //    to M1, and the extension thread starts to execute the `set` operation.
  //    If the `ten_value_t*` is not cloned from the extension thread, then a
  //    read operation from M1 and a write operation from the extension thread
  //    on the same `ten_value_t*` might happen in parallel.

  ten_value_t *c_value =
      ten_env_peek_property(ten_env, ten_string_get_raw_str(&ctx->path), &err);

  if (!c_value) {
    ten_go_error_from_error(&cgo_error, &err);
  } else {
    // Because this value will be passed out of the TEN world and back into the
    // GO world, and these two worlds are in different threads, copy semantics
    // are used to avoid thread safety issues.
    ten_value_t *cloned_value = ten_value_clone(c_value);

    ten_go_ten_value_get_type_and_size(cloned_value, ctx->type, ctx->size);
    *ctx->value_addr = (uintptr_t)cloned_value;
  }

  // Call back into Go to signal that the async operation in C is complete.
  tenGoCAsyncApiCallback(ctx->callback_handle, cgo_error);

  ten_error_deinit(&err);

  ten_env_notify_peek_property_ctx_destroy(ctx);
}

static ten_go_error_t ten_go_ten_env_peek_property(
    ten_go_ten_env_t *self, const void *path, int path_len, uint8_t *type,
    uintptr_t *size, uintptr_t *value_addr, uintptr_t callback_handle) {
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
  });

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_go_ten_env_notify_peek_property_ctx_t *ctx =
      ten_env_notify_peek_property_ctx_create(path, path_len, type, size,
                                              value_addr, callback_handle);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_peek_property, ctx, false,
                            &err)) {
    // Failed to invoke ten_env_proxy_notify.
    ten_env_notify_peek_property_ctx_destroy(ctx);
    ten_go_error_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return cgo_error;
}

ten_go_error_t ten_go_ten_env_get_property_type_and_size(
    uintptr_t bridge_addr, const void *path, int path_len, uint8_t *type,
    uintptr_t *size, uintptr_t *value_addr, uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(type && size, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
  });

  cgo_error = ten_go_ten_env_peek_property(self, path, path_len, type, size,
                                           value_addr, callback_handle);
  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return cgo_error;
}
