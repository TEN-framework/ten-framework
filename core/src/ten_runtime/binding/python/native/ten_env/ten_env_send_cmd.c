//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd_result.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/ten_env/internal/send.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_send_cmd_ctx_t {
  ten_shared_ptr_t *c_cmd;
  PyObject *py_cb_func;
  bool is_ex;
} ten_env_notify_send_cmd_ctx_t;

static ten_env_notify_send_cmd_ctx_t *ten_env_notify_send_cmd_ctx_create(
    ten_shared_ptr_t *c_cmd, PyObject *py_cb_func, bool is_ex) {
  TEN_ASSERT(c_cmd, "Invalid argument.");

  ten_env_notify_send_cmd_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_send_cmd_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_cmd = c_cmd;
  ctx->py_cb_func = py_cb_func;
  ctx->is_ex = is_ex;

  if (py_cb_func != NULL) {
    Py_INCREF(py_cb_func);
  }

  return ctx;
}

static void ten_env_notify_send_cmd_ctx_destroy(
    ten_env_notify_send_cmd_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_cmd) {
    ten_shared_ptr_destroy(ctx->c_cmd);
    ctx->c_cmd = NULL;
  }

  ctx->py_cb_func = NULL;

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

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  PyObject *cb_func = callback_info;

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  ten_py_cmd_result_t *cmd_result_bridge = NULL;

  if (err) {
    py_error = ten_py_error_wrap(err);

    arglist = Py_BuildValue("(OOO)", py_ten_env->actual_py_ten_env, Py_None,
                            py_error);
  } else {
    cmd_result_bridge = ten_py_cmd_result_wrap(c_cmd_result);

    arglist = Py_BuildValue("(OOO)", py_ten_env->actual_py_ten_env,
                            cmd_result_bridge, Py_None);
  }

  PyObject *result = PyObject_CallObject(cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);

  bool is_completed = ten_cmd_result_is_completed(c_cmd_result, NULL);
  if (is_completed) {
    Py_XDECREF(cb_func);
  }

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  if (cmd_result_bridge) {
    ten_py_cmd_result_invalidate(cmd_result_bridge);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_env_proxy_notify_send_cmd(ten_env_t *ten_env, void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_send_cmd_ctx_t *notify_info = user_data;

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_send_cmd_options_t options = TEN_ENV_SEND_CMD_OPTIONS_INIT_VAL;
  if (notify_info->is_ex) {
    options.enable_multiple_results = true;
  }

  bool res = false;
  if (notify_info->py_cb_func == NULL) {
    res = ten_env_send_cmd(ten_env, notify_info->c_cmd, NULL, NULL, &options,
                           &err);
  } else {
    res = ten_env_send_cmd(ten_env, notify_info->c_cmd, proxy_send_cmd_callback,
                           notify_info->py_cb_func, &options, &err);
    if (!res) {
      // About to call the Python function, so it's necessary to ensure that the
      // GIL has been acquired.
      //
      // Allows C codes to work safely with Python objects.
      PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

      ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
      ten_py_error_t *py_err = ten_py_error_wrap(&err);

      PyObject *arglist = Py_BuildValue("(OOO)", py_ten_env->actual_py_ten_env,
                                        Py_None, py_err);

      PyObject *result = PyObject_CallObject(notify_info->py_cb_func, arglist);
      Py_XDECREF(result);  // Ensure cleanup if an error occurred.

      bool err_occurred = ten_py_check_and_clear_py_error();
      TEN_ASSERT(!err_occurred, "Should not happen.");

      Py_XDECREF(arglist);
      Py_XDECREF(notify_info->py_cb_func);

      ten_py_error_invalidate(py_err);

      ten_py_gil_state_release_internal(prev_state);
    }
  }

  ten_error_deinit(&err);

  ten_env_notify_send_cmd_ctx_destroy(notify_info);
}

PyObject *ten_py_ten_env_send_cmd(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 3) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.send_cmd.");
  }

  ten_py_cmd_t *py_cmd = NULL;
  PyObject *cb_func = NULL;
  int is_ex = false;
  if (!PyArg_ParseTuple(args, "O!Op", ten_py_cmd_py_type(), &py_cmd, &cb_func,
                        &is_ex)) {
    return ten_py_raise_py_type_error_exception(
        "Invalid argument type when send cmd.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.send_cmd() failed because the TEN is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  // Check if cb_func is callable.
  if (!PyCallable_Check(cb_func)) {
    cb_func = NULL;
  }

  ten_shared_ptr_t *cloned_cmd = ten_shared_ptr_clone(py_cmd->msg.c_msg);
  ten_env_notify_send_cmd_ctx_t *notify_info =
      ten_env_notify_send_cmd_ctx_create(cloned_cmd, cb_func, is_ex);

  if (!ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                            ten_env_proxy_notify_send_cmd, notify_info, false,
                            &err)) {
    if (cb_func) {
      Py_XDECREF(cb_func);
    }

    ten_env_notify_send_cmd_ctx_destroy(notify_info);

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  } else {
    // Destroy the C message from the Python message as the ownership has been
    // transferred to the notify_info.
    ten_py_msg_destroy_c_msg(&py_cmd->msg);
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
