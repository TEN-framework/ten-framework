//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_json.h"

typedef struct ten_env_notify_set_property_async_ctx_t {
  ten_string_t path;
  ten_value_t *c_value;
  PyObject *py_cb_func;
} ten_env_notify_set_property_async_ctx_t;

static ten_env_notify_set_property_async_ctx_t *
ten_env_notify_set_property_async_ctx_create(const void *path,
                                             ten_value_t *value,
                                             PyObject *py_cb_func) {
  ten_env_notify_set_property_async_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_set_property_async_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_string_init_formatted(&ctx->path, "%s", path);
  ctx->c_value = value;
  ctx->py_cb_func = py_cb_func;

  if (py_cb_func != NULL) {
    Py_INCREF(py_cb_func);
  }

  return ctx;
}

static void ten_env_notify_set_property_async_ctx_destroy(
    ten_env_notify_set_property_async_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->path);
  self->c_value = NULL;
  self->py_cb_func = NULL;

  TEN_FREE(self);
}

static void ten_env_proxy_notify_set_property_async(ten_env_t *ten_env,
                                                    void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_set_property_async_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  TEN_ASSERT(ctx->py_cb_func, "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_set_property(ten_env, ten_string_get_raw_str(&ctx->path),
                                 ctx->c_value, &err);

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;

  if (!rc) {
    py_error = ten_py_error_wrap(&err);

    arglist = Py_BuildValue("(O)", py_error);
  } else {
    arglist = Py_BuildValue("(O)", Py_None);
  }

  ten_error_deinit(&err);

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

  ten_env_notify_set_property_async_ctx_destroy(ctx);
}

static bool ten_py_ten_env_set_property_async(ten_py_ten_env_t *self,
                                              const void *path,
                                              ten_value_t *value,
                                              PyObject *py_cb_func,
                                              ten_error_t *err) {
  TEN_ASSERT(self && ten_py_ten_env_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(py_cb_func && PyCallable_Check(py_cb_func), "Invalid argument.");

  bool success = true;

  ten_env_notify_set_property_async_ctx_t *ctx =
      ten_env_notify_set_property_async_ctx_create(path, value, py_cb_func);
  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_set_property_async, ctx, false,
                            err)) {
    Py_XDECREF(py_cb_func);

    ten_env_notify_set_property_async_ctx_destroy(ctx);
    success = false;
    ten_py_raise_py_runtime_error_exception("Failed to set property");
  }

  return success;
}

PyObject *ten_py_ten_env_set_property_from_json_async(PyObject *self,
                                                      PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 3) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_from_json_async.");
  }

  const char *path = NULL;
  const char *json_str = NULL;
  PyObject *py_cb_func = NULL;
  if (!PyArg_ParseTuple(args, "ssO", &path, &json_str, &py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.set_property_from_json_async.");
  }

  ten_json_t *json = ten_json_from_string(json_str, NULL);
  if (!json) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse json when ten_env.set_property_from_json_async.");
  }

  ten_value_t *value = ten_value_from_json(json);
  if (!value) {
    ten_json_destroy(json);
    return ten_py_raise_py_value_error_exception(
        "Failed to convert json to value when "
        "ten_env.set_property_from_json_async.");
  }

  ten_json_destroy(json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    ten_value_destroy(value);
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  bool rc = ten_py_ten_env_set_property_async(py_ten_env, path, value,
                                              py_cb_func, &err);
  if (!rc) {
    ten_value_destroy(value);
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_ten_env_set_property_string_async(PyObject *self,
                                                   PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 3) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_string_async.");
  }

  const char *path = NULL;
  const char *value = NULL;
  PyObject *py_cb_func = NULL;
  if (!PyArg_ParseTuple(args, "ssO", &path, &value, &py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.set_property_string_async.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_string(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_py_ten_env_set_property_async(py_ten_env, path, c_value,
                                              py_cb_func, &err);
  if (!rc) {
    ten_value_destroy(c_value);
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_ten_env_set_property_int_async(PyObject *self,
                                                PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 3) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_int_async.");
  }

  const char *path = NULL;
  int value = 0;
  PyObject *py_cb_func = NULL;
  if (!PyArg_ParseTuple(args, "siO", &path, &value, &py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.set_property_int_async.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_int64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_py_ten_env_set_property_async(py_ten_env, path, c_value,
                                              py_cb_func, &err);
  if (!rc) {
    ten_value_destroy(c_value);
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_ten_env_set_property_bool_async(PyObject *self,
                                                 PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 3) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_bool_async.");
  }

  const char *path = NULL;
  int value = 0;
  PyObject *py_cb_func = NULL;
  if (!PyArg_ParseTuple(args, "siO", &path, &value, &py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.set_property_bool_async.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_bool(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_py_ten_env_set_property_async(py_ten_env, path, c_value,
                                              py_cb_func, &err);
  if (!rc) {
    ten_value_destroy(c_value);
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_ten_env_set_property_float_async(PyObject *self,
                                                  PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 3) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_float_async.");
  }

  const char *path = NULL;
  double value = 0.0;
  PyObject *py_cb_func = NULL;
  if (!PyArg_ParseTuple(args, "sdO", &path, &value, &py_cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.set_property_float_async.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_float64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_py_ten_env_set_property_async(py_ten_env, path, c_value,
                                              py_cb_func, &err);
  if (!rc) {
    ten_value_destroy(c_value);
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}