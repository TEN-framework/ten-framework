//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_json.h"

typedef struct ten_env_notify_set_property_ctx_t {
  bool result;
  ten_string_t path;
  ten_value_t *c_value;
  ten_error_t *c_error;
  ten_event_t *completed;
} ten_env_notify_set_property_ctx_t;

static ten_env_notify_set_property_ctx_t *
ten_env_notify_set_property_ctx_create(const void *path, ten_value_t *value,
                                       ten_error_t *c_error) {
  ten_env_notify_set_property_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_set_property_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->result = true;
  ten_string_init_formatted(&ctx->path, "%s", path);
  ctx->c_value = value;
  ctx->completed = ten_event_create(0, 1);
  ctx->c_error = c_error;
  return ctx;
}

static void ten_env_notify_set_property_ctx_destroy(
    ten_env_notify_set_property_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->path);
  self->c_value = NULL;
  ten_event_destroy(self->completed);
  self->c_error = NULL;
  TEN_FREE(self);
}

static void ten_env_proxy_notify_set_property(ten_env_t *ten_env,
                                              void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_set_property_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ctx->result = ten_env_set_property(
      ten_env, ten_string_get_raw_str(&ctx->path), ctx->c_value, &err);
  TEN_ASSERT(ctx->result, "Should not happen.");

  ten_event_set(ctx->completed);

  ten_error_deinit(&err);
}

static void ten_py_ten_env_set_property(ten_py_ten_env_t *self,
                                        const void *path, ten_value_t *value,
                                        ten_error_t *err) {
  TEN_ASSERT(self && ten_py_ten_env_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");

  ten_env_notify_set_property_ctx_t *ctx =
      ten_env_notify_set_property_ctx_create(path, value, err);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_set_property, ctx, false,
                            err)) {
    goto done;
  }

  PyThreadState *saved_py_thread_state = PyEval_SaveThread();
  ten_event_wait(ctx->completed, -1);
  PyEval_RestoreThread(saved_py_thread_state);

done:
  ten_env_notify_set_property_ctx_destroy(ctx);
}

PyObject *ten_py_ten_env_set_property_from_json(PyObject *self,
                                                PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_from_json.");
  }

  const char *path = NULL;
  const char *json_str = NULL;
  if (!PyArg_ParseTuple(args, "ss", &path, &json_str)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when ten_env.set_property_from_json.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.set_property_from_json() failed because "
                  "ten_env_proxy is invalid.");

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_json_t *json = ten_json_from_string(json_str, &err);
  if (!json) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *value = ten_value_from_json(json);
  TEN_ASSERT(value, "value should not be NULL.");

  ten_json_destroy(json);

  ten_py_ten_env_set_property(py_ten_env, path, value, &err);

  if (ten_error_is_success(&err)) {
    ten_error_deinit(&err);
    Py_RETURN_NONE;
  }

  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);
  return result;
}

PyObject *ten_py_ten_env_set_property_int(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_int.");
  }

  const char *path = NULL;
  int value = 0;
  if (!PyArg_ParseTuple(args, "si", &path, &value)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when ten_env.set_property_int.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(
        &err, TEN_ERROR_CODE_TEN_IS_CLOSED,
        "ten_env.set_property_int() failed because ten_env_proxy is invalid.");

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_int64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  ten_py_ten_env_set_property(py_ten_env, path, c_value, &err);
  if (ten_error_is_success(&err)) {
    ten_error_deinit(&err);
    Py_RETURN_NONE;
  }

  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);
  return result;
}

PyObject *ten_py_ten_env_set_property_string(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_string.");
  }

  const char *path = NULL;
  const char *value = NULL;
  if (!PyArg_ParseTuple(args, "ss", &path, &value)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when ten_env.set_property_string.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.set_property_string() failed because ten_env_proxy "
                  "is invalid.");

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_string(value);
  TEN_ASSERT(c_value, "Should not happen.");

  ten_py_ten_env_set_property(py_ten_env, path, c_value, &err);
  if (ten_error_is_success(&err)) {
    ten_error_deinit(&err);
    Py_RETURN_NONE;
  }

  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);
  return result;
}

PyObject *ten_py_ten_env_set_property_bool(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_bool.");
  }

  const char *path = NULL;
  int value = 0;
  if (!PyArg_ParseTuple(args, "si", &path, &value)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when ten_env.set_property_bool.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(
        &err, TEN_ERROR_CODE_TEN_IS_CLOSED,
        "ten_env.set_property_bool() failed because ten_env_proxy is invalid.");

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_bool(value > 0);
  TEN_ASSERT(c_value, "Should not happen.");

  ten_py_ten_env_set_property(py_ten_env, path, c_value, &err);
  if (ten_error_is_success(&err)) {
    ten_error_deinit(&err);
    Py_RETURN_NONE;
  }

  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);
  return result;
}

PyObject *ten_py_ten_env_set_property_float(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.set_property_float.");
  }

  const char *path = NULL;
  double value = 0.0;
  if (!PyArg_ParseTuple(args, "sd", &path, &value)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when ten_env.set_property_float.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.set_property_float() failed because ten_env_proxy "
                  "is invalid.");

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  ten_value_t *c_value = ten_value_create_float64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  ten_py_ten_env_set_property(py_ten_env, path, c_value, &err);
  if (ten_error_is_success(&err)) {
    ten_error_deinit(&err);
    Py_RETURN_NONE;
  }

  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);
  return result;
}
