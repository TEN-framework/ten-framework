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
#include "include_internal/ten_runtime/binding/python/test/env_tester.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_py_ten_env_tester_notify_return_result_ctx_t {
  ten_shared_ptr_t *cmd_result;
  PyObject *cb_func;
} ten_py_ten_env_tester_notify_return_result_ctx_t;

static ten_py_ten_env_tester_notify_return_result_ctx_t *
ten_py_ten_env_tester_notify_return_result_ctx_create(
    ten_shared_ptr_t *cmd_result, PyObject *cb_func) {
  ten_py_ten_env_tester_notify_return_result_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_py_ten_env_tester_notify_return_result_ctx_t));
  ctx->cmd_result = ten_shared_ptr_clone(cmd_result);

  // Increase the reference count of the callback function to ensure that it
  // will not be destroyed before the callback is called.
  if (cb_func) {
    Py_INCREF(cb_func);
  }

  ctx->cb_func = cb_func;
  return ctx;
}

static void ten_py_ten_env_tester_notify_return_result_ctx_destroy(
    ten_py_ten_env_tester_notify_return_result_ctx_t *ctx) {
  ten_shared_ptr_destroy(ctx->cmd_result);

  if (ctx->cb_func) {
    Py_XDECREF(ctx->cb_func);
  }

  TEN_FREE(ctx);
}

static void proxy_return_result_callback(ten_env_tester_t *self,
                                         ten_shared_ptr_t *c_cmd_result,
                                         void *user_data, ten_error_t *error) {
  TEN_ASSERT(self && ten_env_tester_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(user_data, "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_ten_env_tester_t *py_ten_env_tester = ten_py_ten_env_tester_wrap(self);
  PyObject *cb_func = user_data;

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;

  if (!error) {
    arglist = Py_BuildValue("(OO)", py_ten_env_tester->actual_py_ten_env_tester,
                            Py_None);
  } else {
    py_error = ten_py_error_wrap(error);
    arglist = Py_BuildValue("(OO)", py_ten_env_tester->actual_py_ten_env_tester,
                            py_error);
  }

  PyObject *result = PyObject_CallObject(cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_py_ten_env_tester_notify_return_result_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  ten_py_ten_env_tester_notify_return_result_ctx_t *ctx =
      (ten_py_ten_env_tester_notify_return_result_ctx_t *)user_data;

  if (ctx->cb_func) {
    Py_INCREF(ctx->cb_func);

    ten_env_tester_return_result(ten_env_tester, ctx->cmd_result,
                                 proxy_return_result_callback, ctx->cb_func,
                                 NULL);
  } else {
    ten_env_tester_return_result(ten_env_tester, ctx->cmd_result, NULL, NULL,
                                 NULL);
  }

  ten_py_ten_env_tester_notify_return_result_ctx_destroy(ctx);
}

PyObject *ten_py_ten_env_tester_return_result(PyObject *self, PyObject *args) {
  ten_py_ten_env_tester_t *py_ten_env_tester = (ten_py_ten_env_tester_t *)self;
  TEN_ASSERT(py_ten_env_tester &&
                 ten_py_ten_env_tester_check_integrity(py_ten_env_tester),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env_tester.return_result.");
  }

  ten_py_cmd_result_t *py_cmd_result = NULL;
  PyObject *cb_func = NULL;
  if (!PyArg_ParseTuple(args, "O!O", ten_py_cmd_result_py_type(),
                        &py_cmd_result, &cb_func)) {
    return ten_py_raise_py_type_error_exception(
        "Invalid argument type when return result.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env_tester->c_ten_env_tester_proxy) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env_tester.return_result() failed because the TEN is "
                  "closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  // Check if cb_func is callable.
  if (!PyCallable_Check(cb_func)) {
    cb_func = NULL;
  }

  ten_py_ten_env_tester_notify_return_result_ctx_t *ctx =
      ten_py_ten_env_tester_notify_return_result_ctx_create(
          py_cmd_result->msg.c_msg, cb_func);

  bool success = ten_env_tester_proxy_notify(
      py_ten_env_tester->c_ten_env_tester_proxy,
      ten_py_ten_env_tester_notify_return_result_proxy_notify, ctx, &err);

  if (!success) {
    ten_py_ten_env_tester_notify_return_result_ctx_destroy(ctx);

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  } else {
    ten_py_msg_destroy_c_msg(&py_cmd_result->msg);
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
