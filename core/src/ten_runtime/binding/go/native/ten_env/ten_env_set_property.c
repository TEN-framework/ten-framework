//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/internal/json.h"
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
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

typedef struct ten_env_notify_set_property_ctx_t {
  ten_string_t path;
  ten_value_t *c_value;
  uintptr_t callback_handle;
} ten_env_notify_set_property_ctx_t;

static ten_env_notify_set_property_ctx_t *
ten_env_notify_set_property_ctx_create(const void *path, int path_len,
                                       ten_value_t *value,
                                       uintptr_t callback_handle) {
  ten_env_notify_set_property_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_set_property_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_string_init_formatted(&ctx->path, "%.*s", path_len, path);
  ctx->c_value = value;
  ctx->callback_handle = callback_handle;

  return ctx;
}

static void ten_env_notify_set_property_ctx_destroy(
    ten_env_notify_set_property_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->path);
  if (ctx->c_value) {
    ten_value_destroy(ctx->c_value);
    ctx->c_value = NULL;
  }

  TEN_FREE(ctx);
}

static void ten_env_proxy_notify_set_property(ten_env_t *ten_env,
                                              void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_set_property_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool res = ten_env_set_property(ten_env, ten_string_get_raw_str(&ctx->path),
                                  ctx->c_value, &err);
  if (res) {
    // The ownership of the C value has been successfully transferred to the TEN
    // runtime.
    ctx->c_value = NULL;
  } else {
    // Prepare error information to pass to Go.
    ten_go_error_from_error(&cgo_error, &err);
  }

  // Call back into Go to signal that the async operation in C is complete.
  tenGoCAsyncApiCallback(ctx->callback_handle, cgo_error);

  ten_error_deinit(&err);

  ten_env_notify_set_property_ctx_destroy(ctx);
}

static ten_go_error_t ten_go_ten_env_set_property(ten_go_ten_env_t *self,
                                                  const void *path,
                                                  int path_len,
                                                  ten_value_t *value,
                                                  uintptr_t callback_handle) {
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {
    ten_value_destroy(value);
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
  });

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_notify_set_property_ctx_t *ctx =
      ten_env_notify_set_property_ctx_create(path, path_len, value,
                                             callback_handle);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_set_property, ctx, false,
                            &err)) {
    // Failed to invoke ten_env_proxy_notify.
    ten_env_notify_set_property_ctx_destroy(ctx);
    ten_go_error_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return cgo_error;
}

ten_go_error_t ten_go_ten_env_set_property_bool(uintptr_t bridge_addr,
                                                const void *path, int path_len,
                                                bool value,
                                                uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_bool(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_int8(uintptr_t bridge_addr,
                                                const void *path, int path_len,
                                                int8_t value,
                                                uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_int8(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_int16(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 int16_t value,
                                                 uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_int16(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_int32(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 int32_t value,
                                                 uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_int32(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_int64(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 int64_t value,
                                                 uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_int64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_uint8(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 uint8_t value,
                                                 uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_uint8(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_uint16(uintptr_t bridge_addr,
                                                  const void *path,
                                                  int path_len, uint16_t value,
                                                  uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_uint16(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_uint32(uintptr_t bridge_addr,
                                                  const void *path,
                                                  int path_len, uint32_t value,
                                                  uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_uint32(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_uint64(uintptr_t bridge_addr,
                                                  const void *path,
                                                  int path_len, uint64_t value,
                                                  uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_uint64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_float32(uintptr_t bridge_addr,
                                                   const void *path,
                                                   int path_len, float value,
                                                   uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_float32(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_float64(uintptr_t bridge_addr,
                                                   const void *path,
                                                   int path_len, double value,
                                                   uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_value_create_float64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_string(
    uintptr_t bridge_addr, const void *path, int path_len, const void *value,
    int value_len, uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  const char *str_value = "";

  // According to the document of `unsafe.StringData()`, the underlying data
  // (i.e., value here) of an empty GO string is unspecified. So it's unsafe to
  // access. We should handle this case explicitly.
  if (value_len > 0) {
    str_value = (const char *)value;
  }

  ten_value_t *c_value =
      ten_value_create_string_with_size(str_value, value_len);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_buf(uintptr_t bridge_addr,
                                               const void *path, int path_len,
                                               void *value, int value_len,
                                               uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_go_ten_value_create_buf(value, value_len);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_ptr(uintptr_t bridge_addr,
                                               const void *path, int path_len,
                                               ten_go_handle_t value,
                                               uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path && path_len > 0, "Should not happen.");

  ten_value_t *c_value = ten_go_ten_value_create_ptr(value);
  TEN_ASSERT(c_value, "Should not happen.");

  return ten_go_ten_env_set_property(self, path, path_len, c_value,
                                     callback_handle);
}

ten_go_error_t ten_go_ten_env_set_property_json_bytes(
    uintptr_t bridge_addr, const void *path, int path_len, const void *json_str,
    int json_str_len, uintptr_t callback_handle) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(path_len > 0 && path, "Should not happen.");
  TEN_ASSERT(json_str && json_str_len > 0, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_json_t *json = ten_go_json_loads(json_str, json_str_len, &cgo_error);
  if (json == NULL) {
    return cgo_error;
  }

  ten_value_t *value = ten_value_from_json(json);
  ten_json_destroy(json);

  return ten_go_ten_env_set_property(self, path, path_len, value,
                                     callback_handle);
}
