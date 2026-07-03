//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/common/error_code.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"

typedef void (*ten_py_get_property_cb)(ten_env_t *ten_env, ten_value_t *value,
                                       ten_error_t *error,
                                       PyObject *py_cb_func);

typedef struct ten_env_notify_peek_property_async_ctx_t {
  ten_string_t path;
  PyObject *py_cb_func;
  ten_py_get_property_cb cb;
} ten_env_notify_peek_property_async_ctx_t;

static ten_env_notify_peek_property_async_ctx_t *
ten_env_notify_peek_property_async_ctx_create(const char *path,
                                              PyObject *py_cb_func,
                                              ten_py_get_property_cb cb) {
  ten_env_notify_peek_property_async_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_peek_property_async_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  if (path) {
    ten_string_init_formatted(&ctx->path, "%s", path);
  } else {
    TEN_STRING_INIT(ctx->path);
  }

  ctx->py_cb_func = py_cb_func;
  ctx->cb = cb;

  if (py_cb_func != NULL) {
    Py_INCREF(py_cb_func);
  }

  return ctx;
}

static void ten_env_notify_peek_property_async_ctx_destroy(
    ten_env_notify_peek_property_async_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->path);

  ctx->py_cb_func = NULL;
  ctx->cb = NULL;

  TEN_FREE(ctx);
}

static void ten_env_proxy_notify_peek_property(ten_env_t *ten_env,
                                               void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_peek_property_async_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *c_value =
      ten_env_peek_property(ten_env, ten_string_get_raw_str(&ctx->path), &err);

  if (c_value) {
    ctx->cb(ten_env, c_value, NULL, ctx->py_cb_func);
  } else {
    ctx->cb(ten_env, NULL, &err, ctx->py_cb_func);
  }

  ten_error_deinit(&err);
  ten_env_notify_peek_property_async_ctx_destroy(ctx);
}

static bool ten_py_peek_property_async(ten_py_ten_env_t *self, const char *path,
                                       PyObject *py_cb_func,
                                       ten_py_get_property_cb cb,
                                       ten_error_t *error) {
  TEN_ASSERT(self && ten_py_ten_env_check_integrity(self), "Invalid argument.");

  bool success = true;

  ten_env_notify_peek_property_async_ctx_t *ctx =
      ten_env_notify_peek_property_async_ctx_create(path, py_cb_func, cb);
  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_peek_property, ctx, false,
                            error)) {
    if (py_cb_func) {
      Py_XDECREF(py_cb_func);
    }

    ten_env_notify_peek_property_async_ctx_destroy(ctx);
    success = false;
    ten_py_raise_py_runtime_error_exception("Failed to get property");
  }

  return success;
}

static void ten_py_get_property_to_json_cb(ten_env_t *ten_env,
                                           ten_value_t *value,
                                           ten_error_t *error,
                                           PyObject *py_cb_func) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  const char *json_str = "";

  if (error) {
    py_error = ten_py_error_wrap(error);

    arglist = Py_BuildValue("(sO)", json_str, py_error);
  } else {
    ten_json_t json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
    bool success = ten_value_to_json(value, &json);
    TEN_ASSERT(success, "Should not happen.");

    bool must_free = false;
    json_str = ten_json_to_string(&json, NULL, &must_free);
    TEN_ASSERT(json_str, "Should not happen.");

    arglist = Py_BuildValue("(sO)", json_str, Py_None);

    ten_json_deinit(&json);

    if (must_free) {
      TEN_FREE(json_str);
    }
  }

  PyObject *result = PyObject_CallObject(py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_py_get_property_int_cb(ten_env_t *ten_env, ten_value_t *value,
                                       ten_error_t *error,
                                       PyObject *py_cb_func) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  int64_t int_value = 0;

  if (error) {
    py_error = ten_py_error_wrap(error);

    arglist = Py_BuildValue("(iO)", int_value, py_error);
  } else {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    int_value = ten_value_get_int64(value, error);

    if (ten_error_is_success(&err)) {
      arglist = Py_BuildValue("(iO)", int_value, Py_None);
    } else {
      py_error = ten_py_error_wrap(&err);
      arglist = Py_BuildValue("(iO)", int_value, py_error);
    }

    ten_error_deinit(&err);
  }

  PyObject *result = PyObject_CallObject(py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_py_get_property_string_cb(ten_env_t *ten_env,
                                          ten_value_t *value,
                                          ten_error_t *error,
                                          PyObject *py_cb_func) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  const char *str_value = "";

  if (error) {
    py_error = ten_py_error_wrap(error);

    arglist = Py_BuildValue("(sO)", str_value, py_error);
  } else {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    str_value = ten_value_peek_raw_str(value, &err);

    if (ten_error_is_success(&err)) {
      arglist = Py_BuildValue("(sO)", str_value, Py_None);
    } else {
      py_error = ten_py_error_wrap(&err);
      arglist = Py_BuildValue("(sO)", str_value, py_error);
    }

    ten_error_deinit(&err);
  }

  PyObject *result = PyObject_CallObject(py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_py_get_property_bool_cb(ten_env_t *ten_env, ten_value_t *value,
                                        ten_error_t *error,
                                        PyObject *py_cb_func) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  bool bool_value = false;

  if (error) {
    py_error = ten_py_error_wrap(error);

    arglist = Py_BuildValue("(OO)", bool_value ? Py_True : Py_False, py_error);
  } else {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    bool_value = ten_value_get_bool(value, &err);

    if (ten_error_is_success(&err)) {
      arglist = Py_BuildValue("(OO)", bool_value ? Py_True : Py_False, Py_None);
    } else {
      py_error = ten_py_error_wrap(&err);
      arglist =
          Py_BuildValue("(OO)", bool_value ? Py_True : Py_False, py_error);
    }

    ten_error_deinit(&err);
  }

  PyObject *result = PyObject_CallObject(py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_py_get_property_float_cb(ten_env_t *ten_env, ten_value_t *value,
                                         ten_error_t *error,
                                         PyObject *py_cb_func) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  double float_value = 0.0;

  if (error) {
    py_error = ten_py_error_wrap(error);

    arglist = Py_BuildValue("(dO)", float_value, py_error);
  } else {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    float_value = ten_value_get_float64(value, &err);

    if (ten_error_is_success(&err)) {
      arglist = Py_BuildValue("(dO)", float_value, Py_None);
    } else {
      py_error = ten_py_error_wrap(&err);
      arglist = Py_BuildValue("(dO)", float_value, py_error);
    }

    ten_error_deinit(&err);
  }

  PyObject *result = PyObject_CallObject(py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_py_is_property_exist_cb(ten_env_t *ten_env, ten_value_t *value,
                                        ten_error_t *error,
                                        PyObject *py_cb_func) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;
  bool is_exist = value != NULL;

  // If the errno is TEN_ERROR_CODE_VALUE_NOT_FOUND, it means the property does
  // not exist which is expected not to be an error.
  if (error && ten_error_code(error) != TEN_ERROR_CODE_VALUE_NOT_FOUND) {
    py_error = ten_py_error_wrap(error);

    arglist = Py_BuildValue("(OO)", is_exist ? Py_True : Py_False, py_error);
  } else {
    arglist = Py_BuildValue("(OO)", is_exist ? Py_True : Py_False, Py_None);
  }

  PyObject *result = PyObject_CallObject(py_cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(py_cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static PyObject *ten_py_ten_env_get_property_async(PyObject *self,
                                                   PyObject *args,
                                                   ten_py_get_property_cb cb) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.get_property_to_json_async.");
  }

  const char *path = NULL;
  PyObject *cb_func = NULL;
  if (!PyArg_ParseTuple(args, "zO", &path, &cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse argument when ten_env.get_property_to_json_async.");
  }

  // Check if cb_func is callable.
  if (!PyCallable_Check(cb_func)) {
    return ten_py_raise_py_value_error_exception(
        "Invalid callback function when ten_env.get_property_to_json_async.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  bool rc = ten_py_peek_property_async(py_ten_env, path, cb_func, cb, &err);
  if (rc) {
    ten_error_deinit(&err);
    Py_RETURN_NONE;
  }

  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);

  return result;
}

PyObject *ten_py_ten_env_get_property_to_json_async(PyObject *self,
                                                    PyObject *args) {
  return ten_py_ten_env_get_property_async(self, args,
                                           ten_py_get_property_to_json_cb);
}

PyObject *ten_py_ten_env_get_property_int_async(PyObject *self,
                                                PyObject *args) {
  return ten_py_ten_env_get_property_async(self, args,
                                           ten_py_get_property_int_cb);
}

PyObject *ten_py_ten_env_get_property_string_async(PyObject *self,
                                                   PyObject *args) {
  return ten_py_ten_env_get_property_async(self, args,
                                           ten_py_get_property_string_cb);
}

PyObject *ten_py_ten_env_get_property_bool_async(PyObject *self,
                                                 PyObject *args) {
  return ten_py_ten_env_get_property_async(self, args,
                                           ten_py_get_property_bool_cb);
}

PyObject *ten_py_ten_env_get_property_float_async(PyObject *self,
                                                  PyObject *args) {
  return ten_py_ten_env_get_property_async(self, args,
                                           ten_py_get_property_float_cb);
}

PyObject *ten_py_ten_env_is_property_exist_async(PyObject *self,
                                                 PyObject *args) {
  return ten_py_ten_env_get_property_async(self, args,
                                           ten_py_is_property_exist_cb);
}
