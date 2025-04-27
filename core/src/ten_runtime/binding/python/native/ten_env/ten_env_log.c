//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <string.h>

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/log.h"
#include "ten_runtime/ten_env/internal/log.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_log_ctx_t {
  int32_t level;
  const char *func_name;
  const char *file_name;
  size_t line_no;
  const char *msg;
  ten_event_t *completed;
} ten_env_notify_log_ctx_t;

static ten_env_notify_log_ctx_t *ten_env_notify_log_ctx_create(
    int32_t level, const char *func_name, const char *file_name, size_t line_no,
    const char *msg) {
  ten_env_notify_log_ctx_t *ctx = TEN_MALLOC(sizeof(ten_env_notify_log_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->level = level;
  ctx->func_name = func_name;
  ctx->file_name = file_name;
  ctx->line_no = line_no;
  ctx->msg = msg;
  ctx->completed = ten_event_create(0, 1);

  return ctx;
}

static void ten_env_notify_log_ctx_destroy(ten_env_notify_log_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_event_destroy(ctx->completed);

  TEN_FREE(ctx);
}

static void ten_env_proxy_notify_log(ten_env_t *ten_env, void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_log_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_env_log(ten_env, ctx->level, ctx->func_name, ctx->file_name, ctx->line_no,
              ctx->msg);

  ten_event_set(ctx->completed);
}

PyObject *ten_py_ten_env_log(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 5) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.log.");
  }

  TEN_LOG_LEVEL level = TEN_LOG_LEVEL_INVALID;
  const char *func_name = NULL;
  const char *file_name = NULL;
  size_t line_no = 0;
  const char *msg = NULL;
  if (!PyArg_ParseTuple(args, "izzis", &level, &func_name, &file_name, &line_no,
                        &msg)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.log.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.log() failed because ten is closed.");

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_env_notify_log_ctx_t *ctx =
      ten_env_notify_log_ctx_create(level, func_name, file_name, line_no, msg);

  if (py_ten_env->c_ten_env_proxy) {
    if (!ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                              ten_env_proxy_notify_log, ctx, false, &err)) {
      PyObject *result = (PyObject *)ten_py_error_wrap(&err);
      ten_error_deinit(&err);
      ten_env_notify_log_ctx_destroy(ctx);
      return result;
    }

    // The current implementation of the logging API is fully synchronous. One
    // reason for not designing it as asynchronous is that if `exit()` is called
    // immediately after logging, the log message may not actually be output,
    // which is very unfriendly for diagnosing issues. However, in the future,
    // it might be possible to support both synchronous and asynchronous logging
    // APIs. For example, developers could choose the mode via a parameter in
    // the logging API, as asynchronous implementation offers better performance
    // compared to the synchronous approach.
    PyThreadState *saved_py_thread_state = PyEval_SaveThread();
    ten_event_wait(ctx->completed, -1);
    PyEval_RestoreThread(saved_py_thread_state);
  } else {
    // TODO(Wei): This function is currently specifically designed for the addon
    // because the addon currently does not have a main thread, so it's unable
    // to use the ten_env_proxy mechanism to maintain thread safety. Once the
    // main thread for the addon is determined in the future, these hacks made
    // specifically for the addon can be completely removed, and comprehensive
    // thread safety mechanism can be implemented.
    TEN_ASSERT(py_ten_env->c_ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
               "Should not happen.");

    ten_env_log_without_check_thread(py_ten_env->c_ten_env, ctx->level,
                                     ctx->func_name, ctx->file_name,
                                     ctx->line_no, ctx->msg);
  }

  ten_error_deinit(&err);
  ten_env_notify_log_ctx_destroy(ctx);

  Py_RETURN_NONE;
}
