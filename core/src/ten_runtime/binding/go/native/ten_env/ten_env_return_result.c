//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env/internal/return.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"

typedef struct ten_env_notify_return_result_ctx_t {
  ten_shared_ptr_t *c_cmd;
  ten_go_handle_t handler_id;
} ten_env_notify_return_result_ctx_t;

static ten_env_notify_return_result_ctx_t *
ten_env_notify_return_result_ctx_create(ten_shared_ptr_t *c_cmd,
                                        ten_go_handle_t handler_id) {
  TEN_ASSERT(c_cmd, "Invalid argument.");

  ten_env_notify_return_result_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_return_result_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_cmd = c_cmd;
  ctx->handler_id = handler_id;

  return ctx;
}

static void ten_env_notify_return_result_ctx_destroy(
    ten_env_notify_return_result_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_cmd) {
    ten_shared_ptr_destroy(ctx->c_cmd);
    ctx->c_cmd = NULL;
  }

  ctx->handler_id = 0;

  TEN_FREE(ctx);
}

static void proxy_handle_return_error(ten_env_t *ten_env,
                                      ten_shared_ptr_t *c_cmd_result,
                                      void *user_data, ten_error_t *err) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_go_callback_ctx_t *callback_info = user_data;
  TEN_ASSERT(callback_info, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  if (err) {
    ten_go_error_from_error(&cgo_error, err);
  }

  TEN_ASSERT(callback_info->callback_id != TEN_GO_NO_RESPONSE_HANDLER,
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoOnError(ten_env_bridge->bridge.go_instance, callback_info->callback_id,
               cgo_error);

  ten_go_callback_ctx_destroy(callback_info);
}

static void ten_env_proxy_notify_return_result(ten_env_t *ten_env,
                                               void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_return_result_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = false;
  if (ctx->handler_id == TEN_GO_NO_RESPONSE_HANDLER) {
    rc = ten_env_return_result(ten_env, ctx->c_cmd, NULL, NULL, &err);
    TEN_ASSERT(rc, "Should not happen.");
  } else {
    ten_go_callback_ctx_t *callback_info =
        ten_go_callback_ctx_create(ctx->handler_id);
    rc = ten_env_return_result(ten_env, ctx->c_cmd, proxy_handle_return_error,
                               callback_info, &err);

    if (!rc) {
      // Prepare error information to pass to Go.
      ten_go_error_from_error(&cgo_error, &err);

      ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

      tenGoOnError(ten_env_bridge->bridge.go_instance, ctx->handler_id,
                   cgo_error);

      ten_go_callback_ctx_destroy(callback_info);
    }
  }

  ten_error_deinit(&err);

  ten_env_notify_return_result_ctx_destroy(ctx);
}

ten_go_error_t ten_go_ten_env_return_result(uintptr_t bridge_addr,
                                            uintptr_t cmd_result_bridge_addr,
                                            ten_go_handle_t handler_id) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");

  ten_go_msg_t *cmd_result = ten_go_msg_reinterpret(cmd_result_bridge_addr);
  TEN_ASSERT(cmd_result && ten_go_msg_check_integrity(cmd_result),
             "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
  });
  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_notify_return_result_ctx_t *return_result_info =
      ten_env_notify_return_result_ctx_create(
          ten_go_msg_move_c_msg(cmd_result),
          handler_id <= 0 ? TEN_GO_NO_RESPONSE_HANDLER : handler_id);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_return_result,
                            return_result_info, false, &err)) {
    ten_env_notify_return_result_ctx_destroy(return_result_info);
    ten_go_error_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return cgo_error;
}
