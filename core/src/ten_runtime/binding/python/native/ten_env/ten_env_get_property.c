//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"

typedef struct ten_env_notify_peek_property_ctx_t {
  ten_string_t path;
  ten_value_t *c_value;
  ten_error_t *c_error;
  ten_event_t *completed;
} ten_env_notify_peek_property_ctx_t;

static ten_env_notify_peek_property_ctx_t *
ten_env_notify_peek_property_ctx_create(const void *path,
                                        ten_error_t *c_error) {
  ten_env_notify_peek_property_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_peek_property_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  if (path) {
    ten_string_init_formatted(&ctx->path, "%s", path);
  } else {
    TEN_STRING_INIT(ctx->path);
  }

  ctx->c_value = NULL;
  ctx->completed = ten_event_create(0, 1);
  ctx->c_error = c_error;

  return ctx;
}

static void ten_env_notify_peek_property_ctx_destroy(
    ten_env_notify_peek_property_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->path);
  ctx->c_value = NULL;
  ten_event_destroy(ctx->completed);
  ctx->c_error = NULL;
  TEN_FREE(ctx);
}

static void ten_env_proxy_notify_peek_property(ten_env_t *ten_env,
                                               void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_peek_property_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_value_t *c_value = ten_env_peek_property(
      ten_env, ten_string_get_raw_str(&ctx->path), ctx->c_error);

  // Because this value will be passed out of the TEN world and back into the
  // Python world, and these two worlds are in different threads, copy semantics
  // are used to avoid thread safety issues.
  ctx->c_value = c_value ? ten_value_clone(c_value) : NULL;

  ten_event_set(ctx->completed);
}

static ten_value_t *ten_py_ten_peek_property(ten_py_ten_env_t *self,
                                             const char *path,
                                             ten_error_t *err) {
  TEN_ASSERT(self && ten_py_ten_env_check_integrity(self), "Invalid argument.");

  ten_value_t *c_value = NULL;

  ten_env_notify_peek_property_ctx_t *ctx =
      ten_env_notify_peek_property_ctx_create(path, err);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_peek_property, ctx, false,
                            err)) {
    goto done;
  }

  PyThreadState *saved_py_thread_state = PyEval_SaveThread();
  ten_event_wait(ctx->completed, -1);
  PyEval_RestoreThread(saved_py_thread_state);

  c_value = ctx->c_value;

done:
  ten_env_notify_peek_property_ctx_destroy(ctx);

  return c_value;
}

PyObject *ten_py_ten_env_get_property_to_json(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.get_property_to_json.");
  }

  const char *path = NULL;
  if (!PyArg_ParseTuple(args, "z", &path)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.get_property_to_json.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const char *default_value = "";

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(
        &err, TEN_ERROR_CODE_TEN_IS_CLOSED,
        "ten_env.get_property_to_json() failed because ten is closed.");
    goto error;
  }

  ten_value_t *value = ten_py_ten_peek_property(py_ten_env, path, &err);
  if (!value) {
    goto error;
  }

  ten_json_t json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
  bool success = ten_value_to_json(value, &json);
  TEN_ASSERT(success, "Should not happen.");

  bool must_free = false;
  const char *json_str = ten_json_to_string(&json, NULL, &must_free);
  TEN_ASSERT(json_str, "Should not happen.");

  ten_value_destroy(value);
  ten_json_deinit(&json);

  ten_error_deinit(&err);
  PyObject *result = Py_BuildValue("(sO)", json_str, Py_None);
  if (must_free) {
    TEN_FREE(json_str);
  }

  return result;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(sO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_ten_env_get_property_int(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.get_property_int.");
  }

  const char *path = NULL;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.get_property_int.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  int64_t default_value = 0;

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.get_property_int() failed because ten is closed.");
    goto error;
  }

  ten_value_t *value = ten_py_ten_peek_property(py_ten_env, path, &err);
  if (!value) {
    goto error;
  }

  int64_t int_value = ten_value_get_int64(value, &err);
  if (!ten_error_is_success(&err)) {
    ten_value_destroy(value);
    goto error;
  }

  ten_value_destroy(value);
  ten_error_deinit(&err);
  return Py_BuildValue("(lO)", int_value, Py_None);

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(lO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_ten_env_get_property_string(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.get_property_string.");
  }

  const char *path = NULL;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.get_property_string.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const char *default_value = "";

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.get_property_string() failed because ten_env_proxy "
                  "is invalid.");
    goto error;
  }

  ten_value_t *value = ten_py_ten_peek_property(py_ten_env, path, &err);
  if (!value) {
    goto error;
  }

  const char *str_value = ten_value_peek_raw_str(value, &err);
  if (!str_value) {
    ten_value_destroy(value);
    goto error;
  }

  PyObject *result = Py_BuildValue("(sO)", str_value, Py_None);

  ten_value_destroy(value);
  ten_error_deinit(&err);
  return result;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(sO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_ten_env_get_property_bool(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.get_property_bool.");
  }

  const char *path = NULL;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.get_property_bool.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  bool default_value = false;

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.get_property_bool() failed because ten_env_proxy "
                  "is invalid.");
    goto error;
  }

  ten_value_t *value = ten_py_ten_peek_property(py_ten_env, path, &err);
  if (!value) {
    goto error;
  }

  bool bool_value = ten_value_get_bool(value, &err);
  if (!ten_error_is_success(&err)) {
    ten_value_destroy(value);
    goto error;
  }

  ten_value_destroy(value);
  ten_error_deinit(&err);

  PyObject *py_bool = PyBool_FromLong(bool_value);
  PyObject *result = Py_BuildValue("(OO)", py_bool, Py_None);
  Py_DECREF(py_bool);
  return result;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *py_bool = PyBool_FromLong(default_value);
  PyObject *result = Py_BuildValue("(OO)", py_bool, py_error);
  ten_py_error_invalidate(py_error);
  Py_DECREF(py_bool);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_ten_env_get_property_float(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.get_property_float.");
  }

  const char *path = NULL;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.get_property_float.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  double default_value = 0.0;

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.get_property_float() failed because ten_env_proxy "
                  "is invalid.");
    goto error;
  }

  ten_value_t *value = ten_py_ten_peek_property(py_ten_env, path, &err);
  if (!value) {
    goto error;
  }

  double float_value = ten_value_get_float64(value, &err);
  if (!ten_error_is_success(&err)) {
    ten_value_destroy(value);
    goto error;
  }

  ten_value_destroy(value);
  ten_error_deinit(&err);
  return Py_BuildValue("(dO)", float_value, Py_None);

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(dO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_ten_env_is_property_exist(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.is_property_exist.");
  }

  const char *path = NULL;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.is_property_exist.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  bool default_value = false;

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.is_property_exist() failed because ten is closed.");
    goto error;
  }

  ten_value_t *value = ten_py_ten_peek_property(py_ten_env, path, &err);
  if (!value) {
    goto error;
  }

  ten_error_deinit(&err);
  ten_value_destroy(value);
  PyObject *py_bool = PyBool_FromLong(true);
  PyObject *result = Py_BuildValue("(OO)", py_bool, Py_None);
  Py_DECREF(py_bool);
  return result;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *py_bool = PyBool_FromLong(default_value);
  PyObject *result = Py_BuildValue("(OO)", py_bool, py_error);
  Py_DECREF(py_bool);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}
