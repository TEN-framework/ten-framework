//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/binding/go/test/env_tester.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env/internal/send.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_go_ten_env_tester_send_cmd_ctx_t {
  ten_shared_ptr_t *c_cmd;
  ten_go_handle_t handler_id;
  bool is_ex;
} ten_go_ten_env_tester_send_cmd_ctx_t;

static ten_go_ten_env_tester_send_cmd_ctx_t *
ten_go_ten_env_tester_send_cmd_ctx_create(ten_shared_ptr_t *c_cmd,
                                          ten_go_handle_t handler_id,
                                          bool is_ex) {
  TEN_ASSERT(c_cmd, "Invalid argument.");

  ten_go_ten_env_tester_send_cmd_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_go_ten_env_tester_send_cmd_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_cmd = c_cmd;
  ctx->handler_id = handler_id;
  ctx->is_ex = is_ex;

  return ctx;
}

static void ten_go_ten_env_tester_send_cmd_ctx_destroy(
    ten_go_ten_env_tester_send_cmd_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_cmd) {
    ten_shared_ptr_destroy(ctx->c_cmd);
    ctx->c_cmd = NULL;
  }

  ctx->handler_id = 0;

  TEN_FREE(ctx);
}

static void proxy_send_cmd_callback(ten_env_tester_t *ten_env_tester,
                                    ten_shared_ptr_t *c_cmd_result,
                                    void *callback_info, ten_error_t *err) {
  TEN_ASSERT(ten_env_tester, "Should not happen.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Should not happen.");
  TEN_ASSERT(c_cmd_result && ten_cmd_base_check_integrity(c_cmd_result),
             "Should not happen.");
  TEN_ASSERT(callback_info, "Should not happen.");

  ten_go_ten_env_tester_t *ten_env_bridge =
      ten_go_ten_env_tester_wrap(ten_env_tester);
  ten_go_handle_t handler_id =
      ((ten_go_callback_ctx_t *)callback_info)->callback_id;

  ten_go_msg_t *cmd_result_bridge = ten_go_msg_create(c_cmd_result);
  uintptr_t cmd_result_bridge_addr = (uintptr_t)cmd_result_bridge;

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  if (err) {
    ten_go_error_set_from_error(&cgo_error, err);
  } else {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_OK);
  }

  bool is_completed = ten_cmd_result_is_completed(c_cmd_result, NULL);

  tenGoTesterOnCmdResult(ten_env_bridge->bridge.go_instance,
                         cmd_result_bridge_addr, handler_id, is_completed,
                         cgo_error);

  if (is_completed) {
    ten_go_callback_ctx_destroy(callback_info);
  }
}

static void ten_env_tester_proxy_notify_send_cmd(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  TEN_ASSERT(ten_env_tester, "Should not happen.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Should not happen.");

  ten_go_ten_env_tester_send_cmd_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_send_cmd_options_t options = TEN_ENV_SEND_CMD_OPTIONS_INIT_VAL;
  if (ctx->is_ex) {
    options.enable_multiple_results = true;
  }

  bool res = false;
  if (ctx->handler_id == TEN_GO_NO_RESPONSE_HANDLER) {
    res = ten_env_tester_send_cmd(ten_env_tester, ctx->c_cmd, NULL, NULL,
                                  &options, &err);
  } else {
    ten_go_callback_ctx_t *callback_ctx =
        ten_go_callback_ctx_create(ctx->handler_id);
    res = ten_env_tester_send_cmd(ten_env_tester, ctx->c_cmd,
                                  proxy_send_cmd_callback, callback_ctx,
                                  &options, &err);
    if (!res) {
      ten_go_callback_ctx_destroy(callback_ctx);
    }
  }

  if (!res) {
    if (ctx->handler_id != TEN_GO_NO_RESPONSE_HANDLER) {
      ten_go_ten_env_tester_t *ten_env_bridge =
          ten_go_ten_env_tester_wrap(ten_env_tester);

      TEN_ASSERT(err.error_code != TEN_ERROR_CODE_OK, "Should not happen.");
      ten_go_error_t cgo_error;
      TEN_GO_ERROR_INIT(cgo_error);

      ten_go_error_set_from_error(&cgo_error, &err);

      tenGoTesterOnCmdResult(ten_env_bridge->bridge.go_instance, 0,
                             ctx->handler_id, true, cgo_error);
    }
  }

  ten_error_deinit(&err);

  ten_go_ten_env_tester_send_cmd_ctx_destroy(ctx);
}

ten_go_error_t ten_go_ten_env_tester_send_cmd(uintptr_t bridge_addr,
                                              uintptr_t cmd_bridge_addr,
                                              ten_go_handle_t handler_id,
                                              bool is_ex) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_go_msg_t *cmd = ten_go_msg_reinterpret(cmd_bridge_addr);
  TEN_ASSERT(cmd, "Should not happen.");
  TEN_ASSERT(ten_go_msg_check_integrity(cmd), "Should not happen.");
  TEN_ASSERT(ten_go_msg_c_msg(cmd), "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  if (!self->c_ten_env_tester_proxy) {
    ten_go_error_set(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED,
                     "ten_env_tester.send_cmd() failed because the TEN is "
                     "closed.");
    return cgo_error;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_go_ten_env_tester_send_cmd_ctx_t *ctx =
      ten_go_ten_env_tester_send_cmd_ctx_create(
          ten_go_msg_move_c_msg(cmd),
          handler_id <= 0 ? TEN_GO_NO_RESPONSE_HANDLER : handler_id, is_ex);

  if (!ten_env_tester_proxy_notify(self->c_ten_env_tester_proxy,
                                   ten_env_tester_proxy_notify_send_cmd, ctx,
                                   &err)) {
    ten_go_ten_env_tester_send_cmd_ctx_destroy(ctx);
    ten_go_error_set_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  return cgo_error;
}
