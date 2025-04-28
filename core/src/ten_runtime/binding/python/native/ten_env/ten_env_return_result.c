//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd_result.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_return_result_ctx_t {
  ten_shared_ptr_t *c_cmd;
  PyObject *py_cb_func;
} ten_env_notify_return_result_ctx_t;

static ten_env_notify_return_result_ctx_t *
ten_env_notify_return_result_ctx_create(ten_shared_ptr_t *c_cmd,
                                        PyObject *py_cb_func) {
  TEN_ASSERT(c_cmd, "Invalid argument.");

  ten_env_notify_return_result_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_return_result_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_cmd = c_cmd;
  ctx->py_cb_func = py_cb_func;

  if (py_cb_func) {
    Py_INCREF(py_cb_func);
  }

  return ctx;
}

static void ten_env_notify_return_result_ctx_destroy(
    ten_env_notify_return_result_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_cmd) {
    ten_shared_ptr_destroy(ctx->c_cmd);
    ctx->c_cmd = NULL;
  }

  ctx->py_cb_func = NULL;

  TEN_FREE(ctx);
}

static void proxy_return_result_callback(ten_env_t *ten_env,
                                         ten_shared_ptr_t *c_cmd_result,
                                         void *callback_info,
                                         ten_error_t *err) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
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

  if (err) {
    py_error = ten_py_error_wrap(err);
    arglist = Py_BuildValue("(OO)", py_ten_env->actual_py_ten_env, py_error);
  } else {
    arglist = Py_BuildValue("(OO)", py_ten_env->actual_py_ten_env, Py_None);
  }

  PyObject *result = PyObject_CallObject(cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_env_proxy_notify_return_result(ten_env_t *ten_env,
                                               void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_return_result_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = false;
  if (ctx->py_cb_func == NULL) {
    rc = ten_env_return_result(ten_env, ctx->c_cmd, NULL, NULL, &err);
  } else {
    rc =
        ten_env_return_result(ten_env, ctx->c_cmd, proxy_return_result_callback,
                              ctx->py_cb_func, &err);

    if (!rc) {
      // About to call the Python function, so it's necessary to ensure that the
      // GIL has been acquired.
      //
      // Allows C codes to work safely with Python objects.
      PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

      ten_py_error_t *py_err = ten_py_error_wrap(&err);

      ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);

      PyObject *arglist =
          Py_BuildValue("(OO)", py_ten_env->actual_py_ten_env, py_err);

      PyObject *result = PyObject_CallObject(ctx->py_cb_func, arglist);
      Py_XDECREF(result);  // Ensure cleanup if an error occurred.

      bool err_occurred = ten_py_check_and_clear_py_error();
      TEN_ASSERT(!err_occurred, "Should not happen.");

      Py_XDECREF(arglist);

      if (py_err) {
        ten_py_error_invalidate(py_err);
      }

      ten_py_gil_state_release_internal(prev_state);
    }
  }

  ten_error_deinit(&err);

  ten_env_notify_return_result_ctx_destroy(ctx);
}

PyObject *ten_py_ten_env_return_result(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  ten_py_cmd_result_t *py_cmd_result = NULL;
  PyObject *cb_func = NULL;
  if (!PyArg_ParseTuple(args, "O!O", ten_py_cmd_result_py_type(),
                        &py_cmd_result, &cb_func)) {
    return ten_py_raise_py_type_error_exception(
        "Invalid argument type when return result.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.return_result() failed because the TEN is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  // Check if cb_func is callable.
  if (!PyCallable_Check(cb_func)) {
    cb_func = NULL;
  }

  ten_shared_ptr_t *c_result_cmd =
      ten_shared_ptr_clone(py_cmd_result->msg.c_msg);

  ten_env_notify_return_result_ctx_t *notify_info =
      ten_env_notify_return_result_ctx_create(c_result_cmd, cb_func);

  bool rc = ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                                 ten_env_proxy_notify_return_result,
                                 notify_info, false, &err);
  if (!rc) {
    ten_env_notify_return_result_ctx_destroy(notify_info);

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  } else {
    // Destroy the C message from the Python message as the ownership has been
    // transferred to the notify_info.
    ten_py_msg_destroy_c_msg(&py_cmd_result->msg);
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
