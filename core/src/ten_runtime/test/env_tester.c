//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/test/env_tester.h"

#include <inttypes.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "include_internal/ten_runtime/test/extension_tester.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/msg/cmd/close_app/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

bool ten_env_tester_check_integrity(ten_env_tester_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_ENV_TESTER_SIGNATURE) {
    TEN_ASSERT(0, "Failed to pass ten_env_tester signature checking: %" PRId64,
               self->signature);
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->tester->thread_check)) {
    return false;
  }

  return true;
}

ten_env_tester_t *ten_env_tester_create(ten_extension_tester_t *tester) {
  TEN_ASSERT(tester && ten_extension_tester_check_integrity(tester, true),
             "Invalid argument.");

  ten_env_tester_t *self = TEN_MALLOC(sizeof(ten_env_tester_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->binding_handle.me_in_target_lang = NULL;

  ten_signature_set(&self->signature, TEN_ENV_TESTER_SIGNATURE);

  self->tester = tester;

  self->close_handler = NULL;
  self->destroy_handler = NULL;

  return self;
}

void ten_env_tester_destroy(ten_env_tester_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: In TEN world, the destroy operations need to
                 // be performed in any threads.
                 ten_env_tester_check_integrity(self, false),
             "Invalid argument.");

  if (self->destroy_handler && self->binding_handle.me_in_target_lang) {
    self->destroy_handler(self->binding_handle.me_in_target_lang);
  }

  TEN_FREE(self);
}

typedef struct ten_env_tester_send_cmd_ctx_t {
  ten_extension_tester_t *tester;
  ten_shared_ptr_t *cmd;
  ten_env_tester_transfer_msg_result_handler_func_t handler;
  void *handler_user_data;
  ten_env_send_cmd_options_t options;
  ten_error_t *err;
} ten_env_tester_send_cmd_ctx_t;

typedef struct ten_env_tester_send_cmd_callback_ctx_t {
  ten_shared_ptr_t *cmd_result;
  ten_error_t *err;
  ten_env_tester_send_cmd_ctx_t *send_cmd_ctx;
} ten_env_tester_send_cmd_callback_ctx_t;

typedef struct ten_env_tester_send_msg_ctx_t {
  ten_extension_tester_t *tester;
  ten_shared_ptr_t *msg;
  ten_env_tester_transfer_msg_result_handler_func_t handler;
  void *handler_user_data;
  ten_error_t *err;
} ten_env_tester_send_msg_ctx_t;

typedef struct ten_env_tester_return_result_ctx_t {
  ten_extension_tester_t *tester;
  ten_shared_ptr_t *result;
  ten_env_tester_transfer_msg_result_handler_func_t handler;
  void *handler_user_data;
  ten_error_t *err;
} ten_env_tester_return_result_ctx_t;

typedef struct ten_env_tester_notify_log_ctx_t {
  ten_env_tester_t *ten_env_tester;
  TEN_LOG_LEVEL level;
  ten_string_t func_name;
  ten_string_t file_name;
  size_t line_no;
  ten_string_t msg;
} ten_env_tester_notify_log_ctx_t;

static ten_env_tester_send_cmd_ctx_t *ten_env_tester_send_cmd_ctx_create(
    ten_extension_tester_t *tester, ten_shared_ptr_t *cmd,
    ten_env_tester_transfer_msg_result_handler_func_t handler,
    void *handler_user_data, ten_env_send_cmd_options_t *options) {
  TEN_ASSERT(
      tester && ten_extension_tester_check_integrity(tester, true) && cmd,
      "Invalid argument.");

  ten_env_tester_send_cmd_ctx_t *self =
      TEN_MALLOC(sizeof(ten_env_tester_send_cmd_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->tester = tester;
  self->cmd = cmd;
  self->handler = handler;
  self->handler_user_data = handler_user_data;
  if (options) {
    self->options = *options;
  }
  self->err = NULL;

  return self;
}

static void ten_env_tester_send_cmd_ctx_destroy(
    ten_env_tester_send_cmd_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->cmd) {
    ten_shared_ptr_destroy(self->cmd);
  }

  if (self->err) {
    ten_error_destroy(self->err);
  }

  TEN_FREE(self);
}

static ten_env_tester_send_cmd_callback_ctx_t *
ten_env_tester_send_cmd_callback_ctx_create(
    ten_env_tester_send_cmd_ctx_t *send_cmd_ctx) {
  TEN_ASSERT(send_cmd_ctx, "Invalid argument.");

  ten_env_tester_send_cmd_callback_ctx_t *self =
      TEN_MALLOC(sizeof(ten_env_tester_send_cmd_callback_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->cmd_result = NULL;
  self->err = NULL;
  self->send_cmd_ctx = send_cmd_ctx;

  return self;
}

static void ten_env_tester_send_cmd_callback_ctx_destroy(
    ten_env_tester_send_cmd_callback_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->cmd_result) {
    ten_shared_ptr_destroy(self->cmd_result);
  }

  if (self->err) {
    ten_error_destroy(self->err);
  }

  TEN_FREE(self);
}

static ten_env_tester_send_msg_ctx_t *ten_extension_tester_send_msg_ctx_create(
    ten_extension_tester_t *tester, ten_shared_ptr_t *msg,
    ten_env_tester_transfer_msg_result_handler_func_t handler,
    void *user_data) {
  TEN_ASSERT(
      tester && ten_extension_tester_check_integrity(tester, true) && msg,
      "Invalid argument.");

  ten_env_tester_send_msg_ctx_t *self =
      TEN_MALLOC(sizeof(ten_env_tester_send_msg_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->tester = tester;
  self->msg = msg;
  self->handler = handler;
  self->handler_user_data = user_data;
  self->err = NULL;

  return self;
}

static void ten_extension_tester_send_msg_ctx_destroy(
    ten_env_tester_send_msg_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->msg) {
    ten_shared_ptr_destroy(self->msg);
  }

  if (self->err) {
    ten_error_destroy(self->err);
  }

  TEN_FREE(self);
}

static ten_env_tester_return_result_ctx_t *
ten_extension_tester_return_result_ctx_create(
    ten_extension_tester_t *tester, ten_shared_ptr_t *result,
    ten_env_tester_transfer_msg_result_handler_func_t handler,
    void *user_data) {
  TEN_ASSERT(
      tester && ten_extension_tester_check_integrity(tester, true) && result,
      "Invalid argument.");

  ten_env_tester_return_result_ctx_t *self =
      TEN_MALLOC(sizeof(ten_env_tester_return_result_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->tester = tester;
  self->result = result;
  self->handler = handler;
  self->handler_user_data = user_data;
  self->err = NULL;

  return self;
}

static void ten_extension_tester_return_result_ctx_destroy(
    ten_env_tester_return_result_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->result) {
    ten_shared_ptr_destroy(self->result);
  }

  if (self->err) {
    ten_error_destroy(self->err);
  }

  TEN_FREE(self);
}

static ten_env_tester_notify_log_ctx_t *ten_env_tester_notify_log_ctx_create(
    ten_env_tester_t *ten_env_tester, TEN_LOG_LEVEL level,
    const char *func_name, const char *file_name, size_t line_no,
    const char *msg) {
  TEN_ASSERT(ten_env_tester, "Invalid argument.");

  ten_env_tester_notify_log_ctx_t *self =
      TEN_MALLOC(sizeof(ten_env_tester_notify_log_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->ten_env_tester = ten_env_tester;
  self->level = level;
  self->line_no = line_no;

  if (func_name) {
    ten_string_init_from_c_str_with_size(&self->func_name, func_name,
                                         strlen(func_name));
  } else {
    TEN_STRING_INIT(self->func_name);
  }

  if (file_name) {
    ten_string_init_from_c_str_with_size(&self->file_name, file_name,
                                         strlen(file_name));
  } else {
    TEN_STRING_INIT(self->file_name);
  }

  if (msg) {
    ten_string_init_from_c_str_with_size(&self->msg, msg, strlen(msg));
  } else {
    TEN_STRING_INIT(self->msg);
  }

  return self;
}

static void ten_env_tester_notify_log_ctx_destroy(
    ten_env_tester_notify_log_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->func_name);
  ten_string_deinit(&ctx->file_name);
  ten_string_deinit(&ctx->msg);

  TEN_FREE(ctx);
}

static void ten_extension_tester_execute_error_handler_task(void *self,
                                                            void *arg) {
  ten_extension_tester_t *tester = self;
  TEN_ASSERT(tester && ten_extension_tester_check_integrity(tester, true),
             "Invalid argument.");

  ten_env_tester_send_msg_ctx_t *ctx = arg;
  TEN_ASSERT(ctx, "Invalid argument.");

  ctx->handler(tester->ten_env_tester, NULL, ctx->handler_user_data, ctx->err);

  ten_extension_tester_send_msg_ctx_destroy(ctx);
}

static void ten_extension_tester_execute_cmd_result_handler_task(void *self,
                                                                 void *arg) {
  ten_extension_tester_t *tester = self;
  TEN_ASSERT(tester && ten_extension_tester_check_integrity(tester, true),
             "Invalid argument.");

  ten_env_tester_send_cmd_callback_ctx_t *ctx = arg;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_env_tester_send_cmd_ctx_t *send_cmd_ctx = ctx->send_cmd_ctx;
  TEN_ASSERT(send_cmd_ctx, "Invalid argument.");

  send_cmd_ctx->handler(tester->ten_env_tester, ctx->cmd_result,
                        send_cmd_ctx->handler_user_data, ctx->err);

  if (ctx->err || ten_cmd_result_is_completed(ctx->cmd_result, NULL)) {
    ten_env_tester_send_cmd_ctx_destroy(send_cmd_ctx);
  }

  ten_env_tester_send_cmd_callback_ctx_destroy(ctx);
}

static void ten_extension_tester_execute_return_result_handler_task(void *self,
                                                                    void *arg) {
  ten_extension_tester_t *tester = self;
  TEN_ASSERT(tester && ten_extension_tester_check_integrity(tester, true),
             "Invalid argument.");

  ten_env_tester_return_result_ctx_t *return_result_info = arg;
  TEN_ASSERT(return_result_info, "Invalid argument.");

  return_result_info->handler(
      tester->ten_env_tester, return_result_info->result,
      return_result_info->handler_user_data, return_result_info->err);

  ten_extension_tester_return_result_ctx_destroy(return_result_info);
}

static void send_cmd_callback(ten_env_t *ten_env, ten_shared_ptr_t *cmd_result,
                              void *callback_user_data, ten_error_t *err) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_send_cmd_ctx_t *send_cmd_ctx = callback_user_data;
  TEN_ASSERT(send_cmd_ctx, "Invalid argument.");

  if (!send_cmd_ctx->handler) {
    if (err || ten_cmd_result_is_completed(cmd_result, NULL)) {
      ten_env_tester_send_cmd_ctx_destroy(send_cmd_ctx);
    }
    return;
  }

  ten_env_tester_send_cmd_callback_ctx_t *ctx =
      ten_env_tester_send_cmd_callback_ctx_create(send_cmd_ctx);

  if (err) {
    ten_error_t *err_clone = ten_error_create();
    ten_error_copy(err_clone, err);

    ctx->err = err_clone;

    // Inject cmd result into the extension_tester thread to ensure thread
    // safety.
    int rc = ten_runloop_post_task_tail(
        send_cmd_ctx->tester->tester_runloop,
        ten_extension_tester_execute_cmd_result_handler_task,
        send_cmd_ctx->tester, ctx);
    if (rc) {
      TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
  } else {
    TEN_ASSERT(cmd_result && ten_cmd_base_check_integrity(cmd_result),
               "Should not happen.");

    ctx->cmd_result = ten_shared_ptr_clone(cmd_result);

    // Inject cmd result into the extension_tester thread to ensure thread
    // safety.
    int rc = ten_runloop_post_task_tail(
        send_cmd_ctx->tester->tester_runloop,
        ten_extension_tester_execute_cmd_result_handler_task,
        send_cmd_ctx->tester, ctx);
    if (rc) {
      TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
  }
}

static void send_data_like_msg_callback(ten_env_t *ten_env,
                                        TEN_UNUSED ten_shared_ptr_t *cmd_result,
                                        void *callback_user_data,
                                        ten_error_t *err) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_send_msg_ctx_t *send_msg_info = callback_user_data;
  TEN_ASSERT(send_msg_info, "Invalid argument.");

  if (!send_msg_info->handler) {
    ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
    return;
  }

  if (err) {
    ten_error_t *err_clone = ten_error_create();
    ten_error_copy(err_clone, err);

    send_msg_info->err = err_clone;
  }

  int rc = ten_runloop_post_task_tail(
      send_msg_info->tester->tester_runloop,
      ten_extension_tester_execute_error_handler_task, send_msg_info->tester,
      send_msg_info);
  if (rc) {
    TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void return_result_callback(ten_env_t *self,
                                   TEN_UNUSED ten_shared_ptr_t *cmd_result,
                                   void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Should not happen.");

  ten_env_tester_return_result_ctx_t *return_result_info = user_data;
  TEN_ASSERT(return_result_info, "Invalid argument.");

  if (!return_result_info->handler) {
    ten_extension_tester_return_result_ctx_destroy(return_result_info);
    return;
  }

  if (err) {
    ten_error_t *err_clone = ten_error_create();
    ten_error_copy(err_clone, err);

    return_result_info->err = err_clone;
  }

  int rc = ten_runloop_post_task_tail(
      return_result_info->tester->tester_runloop,
      ten_extension_tester_execute_return_result_handler_task,
      return_result_info->tester, return_result_info);
  if (rc) {
    TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void test_extension_ten_env_send_cmd(ten_env_t *ten_env,
                                            void *user_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_send_cmd_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Invalid argument.");
  TEN_ASSERT(ctx->cmd && ten_msg_check_integrity(ctx->cmd),
             "Should not happen.");

  ten_error_t *err = ten_error_create();

  bool rc = ten_env_send_cmd(ten_env, ctx->cmd, send_cmd_callback, ctx,
                             &ctx->options, err);
  if (!rc) {
    if (ctx->handler) {
      // Move the error to the send_cmd_info.
      ctx->err = err;
      err = NULL;

      // Inject cmd result into the extension_tester thread to ensure thread
      // safety.
      int rc = ten_runloop_post_task_tail(
          ctx->tester->tester_runloop,
          ten_extension_tester_execute_cmd_result_handler_task, ctx->tester,
          ctx);
      if (rc) {
        TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
        TEN_ASSERT(0, "Should not happen.");
      }
    } else {
      ten_env_tester_send_cmd_ctx_destroy(ctx);
    }
  }

  if (err) {
    ten_error_destroy(err);
  }
}

static void test_extension_ten_env_return_result(ten_env_t *ten_env,
                                                 void *user_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_return_result_ctx_t *return_result_info = user_data;
  TEN_ASSERT(return_result_info, "Invalid argument.");

  ten_error_t *err = ten_error_create();

  bool rc =
      ten_env_return_result(ten_env, return_result_info->result,
                            return_result_callback, return_result_info, err);
  if (!rc) {
    if (return_result_info->handler) {
      // Move the error to the return_result_info.
      return_result_info->err = err;
      err = NULL;

      // Inject cmd result into the extension_tester thread to ensure thread
      // safety.
      int rc = ten_runloop_post_task_tail(
          return_result_info->tester->tester_runloop,
          ten_extension_tester_execute_return_result_handler_task,
          return_result_info->tester, return_result_info);
      if (rc) {
        TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
        TEN_ASSERT(0, "Should not happen.");
      }
    } else {
      ten_extension_tester_return_result_ctx_destroy(return_result_info);
    }
  }

  if (err) {
    ten_error_destroy(err);
  }
}

static void test_extension_ten_env_send_data(ten_env_t *ten_env,
                                             void *user_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_send_msg_ctx_t *send_msg_info = user_data;
  TEN_ASSERT(send_msg_info, "Invalid argument.");
  TEN_ASSERT(send_msg_info->msg && ten_msg_check_integrity(send_msg_info->msg),
             "Should not happen.");

  ten_error_t *err = ten_error_create();

  bool rc = ten_env_send_data(ten_env, send_msg_info->msg,
                              send_data_like_msg_callback, send_msg_info, err);
  if (!rc) {
    if (send_msg_info->handler) {
      // Move the error to the send_msg_info.
      send_msg_info->err = err;
      err = NULL;

      // Inject cmd result into the extension_tester thread to ensure thread
      // safety.
      int rc = ten_runloop_post_task_tail(
          send_msg_info->tester->tester_runloop,
          ten_extension_tester_execute_error_handler_task,
          send_msg_info->tester, send_msg_info);
      if (rc) {
        TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
        TEN_ASSERT(0, "Should not happen.");
      }
    } else {
      ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
    }
  }

  if (err) {
    ten_error_destroy(err);
  }
}

static void test_extension_ten_env_send_audio_frame(ten_env_t *ten_env,
                                                    void *user_audio_frame) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_send_msg_ctx_t *send_msg_info = user_audio_frame;
  TEN_ASSERT(send_msg_info, "Invalid argument.");
  TEN_ASSERT(send_msg_info->msg && ten_msg_check_integrity(send_msg_info->msg),
             "Should not happen.");

  ten_error_t *err = ten_error_create();

  bool rc =
      ten_env_send_audio_frame(ten_env, send_msg_info->msg,
                               send_data_like_msg_callback, send_msg_info, err);
  if (!rc) {
    if (send_msg_info->handler) {
      // Move the error to the send_msg_info.
      send_msg_info->err = err;
      err = NULL;

      // Inject cmd result into the extension_tester thread to ensure thread
      // safety.
      int rc = ten_runloop_post_task_tail(
          send_msg_info->tester->tester_runloop,
          ten_extension_tester_execute_error_handler_task,
          send_msg_info->tester, send_msg_info);
      if (rc) {
        TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
        TEN_ASSERT(0, "Should not happen.");
      }
    } else {
      ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
    }
  }

  if (err) {
    ten_error_destroy(err);
  }
}

static void test_extension_ten_env_send_video_frame(ten_env_t *ten_env,
                                                    void *user_video_frame) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_tester_send_msg_ctx_t *send_msg_info = user_video_frame;
  TEN_ASSERT(send_msg_info, "Invalid argument.");
  TEN_ASSERT(send_msg_info->msg && ten_msg_check_integrity(send_msg_info->msg),
             "Should not happen.");

  ten_error_t *err = ten_error_create();

  bool rc =
      ten_env_send_video_frame(ten_env, send_msg_info->msg,
                               send_data_like_msg_callback, send_msg_info, err);
  if (!rc) {
    if (send_msg_info->handler) {
      // Move the error to the send_msg_info.
      send_msg_info->err = err;
      err = NULL;

      // Inject cmd result into the extension_tester thread to ensure thread
      // safety.
      int rc = ten_runloop_post_task_tail(
          send_msg_info->tester->tester_runloop,
          ten_extension_tester_execute_error_handler_task,
          send_msg_info->tester, send_msg_info);
      if (rc) {
        TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
        TEN_ASSERT(0, "Should not happen.");
      }
    } else {
      ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
    }
  }

  if (err) {
    ten_error_destroy(err);
  }
}

bool ten_env_tester_send_cmd(
    ten_env_tester_t *self, ten_shared_ptr_t *cmd,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_env_send_cmd_options_t *options, ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  if (!self->tester->test_extension_ten_env_proxy) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The test extension is closed.");
    }
    return false;
  }

  ten_env_tester_send_cmd_ctx_t *ctx = ten_env_tester_send_cmd_ctx_create(
      self->tester, ten_shared_ptr_clone(cmd), handler, user_data, options);

  bool rc =
      ten_env_proxy_notify(self->tester->test_extension_ten_env_proxy,
                           test_extension_ten_env_send_cmd, ctx, false, err);
  if (!rc) {
    ten_env_tester_send_cmd_ctx_destroy(ctx);
  }

  return rc;
}

bool ten_env_tester_return_result(
    ten_env_tester_t *self, ten_shared_ptr_t *result,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *error) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  if (!self->tester->test_extension_ten_env_proxy) {
    if (error) {
      ten_error_set(error, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The test extension is closed.");
    }
    return false;
  }

  ten_env_tester_return_result_ctx_t *return_result_info =
      ten_extension_tester_return_result_ctx_create(
          self->tester, ten_shared_ptr_clone(result), handler, user_data);
  TEN_ASSERT(return_result_info, "Allocation failed.");

  bool rc = ten_env_proxy_notify(self->tester->test_extension_ten_env_proxy,
                                 test_extension_ten_env_return_result,
                                 return_result_info, false, error);
  if (!rc) {
    ten_extension_tester_return_result_ctx_destroy(return_result_info);
  }

  return rc;
}

bool ten_env_tester_send_data(
    ten_env_tester_t *self, ten_shared_ptr_t *data,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  if (!self->tester->test_extension_ten_env_proxy) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The test extension is closed.");
    }
    return false;
  }

  ten_env_tester_send_msg_ctx_t *send_msg_info =
      ten_extension_tester_send_msg_ctx_create(
          self->tester, ten_shared_ptr_clone(data), handler, user_data);

  bool rc = ten_env_proxy_notify(self->tester->test_extension_ten_env_proxy,
                                 test_extension_ten_env_send_data,
                                 send_msg_info, false, err);
  if (!rc) {
    ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
  }

  return rc;
}

bool ten_env_tester_send_audio_frame(
    ten_env_tester_t *self, ten_shared_ptr_t *audio_frame,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  if (!self->tester->test_extension_ten_env_proxy) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The test extension is closed.");
    }
    return false;
  }

  ten_env_tester_send_msg_ctx_t *send_msg_info =
      ten_extension_tester_send_msg_ctx_create(
          self->tester, ten_shared_ptr_clone(audio_frame), handler, user_data);

  bool rc = ten_env_proxy_notify(self->tester->test_extension_ten_env_proxy,
                                 test_extension_ten_env_send_audio_frame,
                                 send_msg_info, false, err);
  if (!rc) {
    ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
  }

  return rc;
}

bool ten_env_tester_send_video_frame(
    ten_env_tester_t *self, ten_shared_ptr_t *video_frame,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  if (!self->tester->test_extension_ten_env_proxy) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The test extension is closed.");
    }
    return false;
  }

  ten_env_tester_send_msg_ctx_t *send_msg_info =
      ten_extension_tester_send_msg_ctx_create(
          self->tester, ten_shared_ptr_clone(video_frame), handler, user_data);

  bool rc = ten_env_proxy_notify(self->tester->test_extension_ten_env_proxy,
                                 test_extension_ten_env_send_video_frame,
                                 send_msg_info, false, err);
  if (!rc) {
    ten_extension_tester_send_msg_ctx_destroy(send_msg_info);
  }

  return rc;
}

static void test_app_ten_env_send_close_app_cmd(ten_env_t *ten_env,
                                                void *user_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_app_t *app = ten_env->attached_target.app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_shared_ptr_t *close_app_cmd = ten_cmd_close_app_create();
  TEN_ASSERT(close_app_cmd, "Should not happen.");

  ten_msg_set_src_to_app(close_app_cmd, app);

  bool rc = ten_msg_clear_and_set_dest(close_app_cmd, ten_app_get_uri(app),
                                       NULL, NULL, NULL, NULL);
  TEN_ASSERT(rc, "Should not happen.");

  rc = ten_env_send_cmd(ten_env, close_app_cmd, NULL, NULL, NULL, NULL);
  TEN_ASSERT(rc, "Should not happen.");
}

bool ten_env_tester_stop_test(ten_env_tester_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");
  return ten_env_proxy_notify(self->tester->test_app_ten_env_proxy,
                              test_app_ten_env_send_close_app_cmd, NULL, false,
                              err);
}

static void test_extension_ten_env_log(ten_env_t *self, void *user_data) {
  ten_env_tester_notify_log_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_env_log(self, ctx->level, ten_string_get_raw_str(&ctx->func_name),
              ten_string_get_raw_str(&ctx->file_name), ctx->line_no,
              ten_string_get_raw_str(&ctx->msg));

  ten_env_tester_notify_log_ctx_destroy(ctx);
}

bool ten_env_tester_log(ten_env_tester_t *self, TEN_LOG_LEVEL level,
                        const char *func_name, const char *file_name,
                        size_t line_no, const char *msg, ten_error_t *error) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  if (!self->tester->test_extension_ten_env_proxy) {
    if (error) {
      ten_error_set(error, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The test extension is closed.");
    }
    return false;
  }

  ten_env_tester_notify_log_ctx_t *ctx = ten_env_tester_notify_log_ctx_create(
      self, level, func_name, file_name, line_no, msg);
  TEN_ASSERT(ctx, "Allocation failed.");

  bool rc = ten_env_proxy_notify(self->tester->test_extension_ten_env_proxy,
                                 test_extension_ten_env_log, ctx, false, error);
  if (!rc) {
    ten_env_tester_notify_log_ctx_destroy(ctx);
  }

  return rc;
}

bool ten_env_tester_on_init_done(ten_env_tester_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  ten_extension_tester_on_init_done(self->tester);

  return true;
}

bool ten_env_tester_on_start_done(ten_env_tester_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  ten_extension_tester_on_start_done(self->tester);

  return true;
}

bool ten_env_tester_on_stop_done(ten_env_tester_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  ten_extension_tester_on_stop_done(self->tester);

  return true;
}

bool ten_env_tester_on_deinit_done(ten_env_tester_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Invalid argument.");

  ten_extension_tester_on_deinit_done(self->tester);

  return true;
}

void ten_env_tester_set_close_handler_in_target_lang(
    ten_env_tester_t *self,
    ten_env_tester_close_handler_in_target_lang_func_t close_handler) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(self, true),
             "Invalid use of ten_env_tester %p.", self);

  self->close_handler = close_handler;
}

void ten_env_tester_set_destroy_handler_in_target_lang(
    ten_env_tester_t *self,
    ten_env_tester_destroy_handler_in_target_lang_func_t destroy_handler) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(self, true),
             "Invalid use of ten_env_tester %p.", self);

  self->destroy_handler = destroy_handler;
}
