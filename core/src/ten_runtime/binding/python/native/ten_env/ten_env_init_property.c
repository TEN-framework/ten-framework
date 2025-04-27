//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <string.h>

#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_init_property_ctx_t {
  ten_string_t value;
  ten_event_t *completed;
} ten_env_notify_init_property_ctx_t;

typedef struct ten_env_notify_init_property_async_ctx_t {
  ten_string_t value;
  PyObject *py_cb_func;
} ten_env_notify_init_property_async_ctx_t;

static ten_env_notify_init_property_ctx_t *
ten_env_notify_init_property_ctx_create(const void *value, size_t value_len) {
  ten_env_notify_init_property_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_init_property_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_string_init_formatted(&ctx->value, "%.*s", value_len, value);
  ctx->completed = ten_event_create(0, 1);

  return ctx;
}

static void ten_env_notify_init_property_ctx_destroy(
    ten_env_notify_init_property_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->value);
  ten_event_destroy(self->completed);

  TEN_FREE(self);
}

static ten_env_notify_init_property_async_ctx_t *
ten_env_notify_init_property_async_ctx_create(const void *value,
                                              size_t value_len,
                                              PyObject *py_cb_func) {
  ten_env_notify_init_property_async_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_init_property_async_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_string_init_formatted(&ctx->value, "%.*s", value_len, value);
  ctx->py_cb_func = py_cb_func;

  if (py_cb_func != NULL) {
    Py_INCREF(py_cb_func);
  }

  return ctx;
}

static void ten_env_notify_init_property_async_ctx_destroy(
    ten_env_notify_init_property_async_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->value);
  self->py_cb_func = NULL;

  TEN_FREE(self);
}

static void ten_env_proxy_notify_init_property_from_json(ten_env_t *ten_env,
                                                         void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_init_property_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_init_property_from_json(ten_env, ten_string_get_raw_str(&ctx->value),
                                  &err);

  ten_event_set(ctx->completed);

  ten_error_deinit(&err);
}

static void ten_env_proxy_notify_init_property_from_json_async(
    ten_env_t *ten_env, void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_init_property_async_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_init_property_from_json(
      ten_env, ten_string_get_raw_str(&ctx->value), &err);

  // About to call the Python function, so it's necessary to ensure that the
  // GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;

  if (rc) {
    arglist = Py_BuildValue("(O)", Py_None);
  } else {
    py_error = ten_py_error_wrap(&err);
    arglist = Py_BuildValue("(O)", py_error);
  }

  PyObject *result = PyObject_CallObject(ctx->py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(ctx->py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);

  ten_error_deinit(&err);

  ten_env_notify_init_property_async_ctx_destroy(ctx);
}

PyObject *ten_py_ten_env_init_property_from_json(PyObject *self,
                                                 PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.init_property_from_json.");
  }

  const char *json_str = NULL;
  if (!PyArg_ParseTuple(args, "s", &json_str)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when "
        "ten_env.init_property_from_json.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_env_notify_init_property_ctx_t *ctx =
      ten_env_notify_init_property_ctx_create(json_str, strlen(json_str));

  if (!ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                            ten_env_proxy_notify_init_property_from_json, ctx,
                            false, &err)) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_env_notify_init_property_ctx_destroy(ctx);
    ten_error_deinit(&err);
    return result;
  }

  PyThreadState *saved_py_thread_state = PyEval_SaveThread();
  ten_event_wait(ctx->completed, -1);
  PyEval_RestoreThread(saved_py_thread_state);

  ten_env_notify_init_property_ctx_destroy(ctx);
  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_ten_env_init_property_from_json_async(PyObject *self,
                                                       PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.init_property_from_json_async.");
  }

  const char *json_str = NULL;
  PyObject *py_cb_func = NULL;

  if (!PyArg_ParseTuple(args, "sO", &json_str, &py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when "
        "ten_env.init_property_from_json_async.");
  }

  // Check if cb_func is callable.
  if (!PyCallable_Check(py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Invalid callback function when "
        "ten_env.init_property_from_json_async.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_env_notify_init_property_async_ctx_t *ctx =
      ten_env_notify_init_property_async_ctx_create(json_str, strlen(json_str),
                                                    py_cb_func);

  if (!ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                            ten_env_proxy_notify_init_property_from_json_async,
                            ctx, false, &err)) {
    Py_XDECREF(py_cb_func);
    ten_env_notify_init_property_async_ctx_destroy(ctx);

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
