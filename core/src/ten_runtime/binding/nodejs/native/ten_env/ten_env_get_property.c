//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/common/tsfn.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_peek_property_ctx_t {
  ten_string_t path;
  ten_nodejs_tsfn_t *js_cb;
} ten_env_notify_peek_property_ctx_t;

static ten_env_notify_peek_property_ctx_t *
ten_env_notify_peek_property_ctx_create(const void *path,
                                        ten_nodejs_tsfn_t *js_cb) {
  ten_env_notify_peek_property_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_peek_property_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_string_init_from_c_str_with_size(&ctx->path, path, strlen(path));
  ctx->js_cb = js_cb;

  return ctx;
}

static void ten_env_notify_peek_property_ctx_destroy(
    ten_env_notify_peek_property_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->path);

  TEN_FREE(ctx);
}

static void ten_env_proxy_notify_peek_property(ten_env_t *ten_env,
                                               void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_peek_property_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_nodejs_tsfn_t *js_cb = ctx->js_cb;
  TEN_ASSERT(js_cb && ten_nodejs_tsfn_check_integrity(js_cb, false),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *value =
      ten_env_peek_property(ten_env, ten_string_get_raw_str(&ctx->path), &err);

  ten_value_t *cloned_value = NULL;
  ten_error_t *cloned_error = NULL;
  if (value) {
    cloned_value = ten_value_clone(value);
  } else {
    cloned_error = ten_error_create();
    ten_error_copy(cloned_error, &err);
  }

  ten_nodejs_get_property_call_ctx_t *call_info =
      ten_nodejs_get_property_call_ctx_create(js_cb, cloned_value,
                                              cloned_error);
  TEN_ASSERT(call_info, "Should not happen.");

  bool rc = ten_nodejs_tsfn_invoke(js_cb, call_info);
  TEN_ASSERT(rc, "Should not happen.");

  ten_env_notify_peek_property_ctx_destroy(ctx);

  ten_error_deinit(&err);
}

bool ten_nodejs_ten_env_peek_property_value(ten_nodejs_ten_env_t *self,
                                            const char *path,
                                            ten_nodejs_tsfn_t *cb_tsfn,
                                            ten_error_t *error) {
  TEN_ASSERT(self && ten_nodejs_ten_env_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(path, "Invalid argument.");
  TEN_ASSERT(cb_tsfn && ten_nodejs_tsfn_check_integrity(cb_tsfn, true),
             "Invalid argument.");

  ten_env_notify_peek_property_ctx_t *ctx =
      ten_env_notify_peek_property_ctx_create(path, cb_tsfn);
  TEN_ASSERT(ctx, "Should not happen.");

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_peek_property, ctx, false,
                            error)) {
    ten_env_notify_peek_property_ctx_destroy(ctx);
    return false;
  }

  return true;
}
