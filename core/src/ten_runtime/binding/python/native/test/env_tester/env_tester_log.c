//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/test/env_tester.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_tester_notify_log_ctx_t {
  int32_t level;
  ten_string_t func_name;
  ten_string_t file_name;
  size_t line_no;
  ten_string_t msg;
} ten_env_tester_notify_log_ctx_t;

static ten_env_tester_notify_log_ctx_t *ten_env_tester_notify_log_ctx_create(
    int32_t level, const char *func_name, const char *file_name, size_t line_no,
    const char *msg) {
  ten_env_tester_notify_log_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_tester_notify_log_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->level = level;

  if (func_name) {
    ten_string_init_from_c_str_with_size(&ctx->func_name, func_name,
                                         strlen(func_name));
  } else {
    TEN_STRING_INIT(ctx->func_name);
  }

  if (file_name) {
    ten_string_init_from_c_str_with_size(&ctx->file_name, file_name,
                                         strlen(file_name));
  } else {
    TEN_STRING_INIT(ctx->file_name);
  }

  ctx->line_no = line_no;
  if (msg) {
    ten_string_init_from_c_str_with_size(&ctx->msg, msg, strlen(msg));
  } else {
    TEN_STRING_INIT(ctx->msg);
  }

  return ctx;
}

static void ten_env_tester_notify_log_ctx_destroy(
    ten_env_tester_notify_log_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->func_name);
  ten_string_deinit(&ctx->file_name);
  ten_string_deinit(&ctx->msg);

  TEN_FREE(ctx);
}

static void ten_py_ten_env_tester_log_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  ten_env_tester_notify_log_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_env_tester_log(ten_env_tester, ctx->level,
                     ten_string_get_raw_str(&ctx->func_name),
                     ten_string_get_raw_str(&ctx->file_name), ctx->line_no,
                     ten_string_get_raw_str(&ctx->msg), NULL);

  ten_env_tester_notify_log_ctx_destroy(ctx);
}

PyObject *ten_py_ten_env_tester_log(PyObject *self, TEN_UNUSED PyObject *args) {
  ten_py_ten_env_tester_t *py_ten_env_tester = (ten_py_ten_env_tester_t *)self;
  TEN_ASSERT(py_ten_env_tester &&
                 ten_py_ten_env_tester_check_integrity(py_ten_env_tester),
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

  if (!py_ten_env_tester->c_ten_env_tester_proxy) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env_tester.log() failed because ten is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_env_tester_notify_log_ctx_t *ctx = ten_env_tester_notify_log_ctx_create(
      level, func_name, file_name, line_no, msg);

  if (!ten_env_tester_proxy_notify(py_ten_env_tester->c_ten_env_tester_proxy,
                                   ten_py_ten_env_tester_log_proxy_notify, ctx,
                                   &err)) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    ten_env_tester_notify_log_ctx_destroy(ctx);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
