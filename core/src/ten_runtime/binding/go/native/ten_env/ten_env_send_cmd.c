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
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/ten_env/internal/send.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"

typedef struct ten_env_notify_send_cmd_ctx_t {
  ten_shared_ptr_t *c_cmd;
  ten_go_handle_t handler_id;
  bool is_ex;
} ten_env_notify_send_cmd_ctx_t;

static ten_env_notify_send_cmd_ctx_t *ten_env_notify_send_cmd_ctx_create(
    ten_shared_ptr_t *c_cmd, ten_go_handle_t handler_id, bool is_ex) {
  TEN_ASSERT(c_cmd, "Invalid argument.");

  ten_env_notify_send_cmd_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_send_cmd_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_cmd = c_cmd;
  ctx->handler_id = handler_id;
  ctx->is_ex = is_ex;

  return ctx;
}

static void ten_env_notify_send_cmd_ctx_destroy(
    ten_env_notify_send_cmd_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_cmd) {
    ten_shared_ptr_destroy(ctx->c_cmd);
    ctx->c_cmd = NULL;
  }

  ctx->handler_id = 0;

  TEN_FREE(ctx);
}

static void proxy_send_cmd_callback(ten_env_t *ten_env,
                                    ten_shared_ptr_t *c_cmd_result,
                                    void *callback_info, ten_error_t *err) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(c_cmd_result && ten_cmd_base_check_integrity(c_cmd_result),
             "Should not happen.");
  TEN_ASSERT(callback_info, "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  ten_go_handle_t handler_id =
      ((ten_go_callback_ctx_t *)callback_info)->callback_id;

  // Same as Extension::OnCmd, the GO cmd result is only used for the GO
  // extension, so it can be created in GO world. We do not need to call GO
  // function to create the GO cmd result in C.
  ten_go_msg_t *cmd_result_bridge = ten_go_msg_create(c_cmd_result);
  uintptr_t cmd_result_bridge_addr = (uintptr_t)cmd_result_bridge;

  ten_go_error_t cgo_error;

  if (err) {
    ten_go_error_from_error(&cgo_error, err);
  } else {
    ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);
  }

  bool is_completed = ten_cmd_result_is_completed(c_cmd_result, NULL);

  tenGoOnCmdResult(ten_env_bridge->bridge.go_instance, cmd_result_bridge_addr,
                   handler_id, is_completed, cgo_error);

  if (is_completed) {
    ten_go_callback_ctx_destroy(callback_info);
  }
}

static void ten_env_proxy_notify_send_cmd(ten_env_t *ten_env, void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_send_cmd_ctx_t *notify_info = user_data;
  TEN_ASSERT(notify_info, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_send_cmd_options_t options = TEN_ENV_SEND_CMD_OPTIONS_INIT_VAL;
  if (notify_info->is_ex) {
    options.enable_multiple_results = true;
  }

  bool res = false;
  if (notify_info->handler_id == TEN_GO_NO_RESPONSE_HANDLER) {
    res = ten_env_send_cmd(ten_env, notify_info->c_cmd, NULL, NULL, &options,
                           &err);
  } else {
    ten_go_callback_ctx_t *ctx =
        ten_go_callback_ctx_create(notify_info->handler_id);
    res = ten_env_send_cmd(ten_env, notify_info->c_cmd, proxy_send_cmd_callback,
                           ctx, &options, &err);

    if (!res) {
      ten_go_callback_ctx_destroy(ctx);
    }
  }

  if (!res) {
    if (notify_info->handler_id != TEN_GO_NO_RESPONSE_HANDLER) {
      ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

      TEN_ASSERT(err.error_code != TEN_ERROR_CODE_OK, "Should not happen.");
      ten_go_error_t cgo_error;
      ten_go_error_from_error(&cgo_error, &err);

      tenGoOnCmdResult(ten_env_bridge->bridge.go_instance, 0,
                       notify_info->handler_id, true, cgo_error);
    }
  }

  ten_error_deinit(&err);

  ten_env_notify_send_cmd_ctx_destroy(notify_info);
}

ten_go_error_t ten_go_ten_env_send_cmd(uintptr_t bridge_addr,
                                       uintptr_t cmd_bridge_addr,
                                       ten_go_handle_t handler_id, bool is_ex) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");

  ten_go_msg_t *cmd = ten_go_msg_reinterpret(cmd_bridge_addr);
  TEN_ASSERT(cmd && ten_go_msg_check_integrity(cmd), "Should not happen.");
  TEN_ASSERT(ten_go_msg_c_msg(cmd), "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {
    ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
  });

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_notify_send_cmd_ctx_t *notify_info =
      ten_env_notify_send_cmd_ctx_create(
          ten_go_msg_move_c_msg(cmd),
          handler_id <= 0 ? TEN_GO_NO_RESPONSE_HANDLER : handler_id, is_ex);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_send_cmd, notify_info, false,
                            &err)) {
    ten_env_notify_send_cmd_ctx_destroy(notify_info);
    ten_go_error_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return cgo_error;
}
