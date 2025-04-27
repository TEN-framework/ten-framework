//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/addon/addon_manager.h"

#include "include_internal/ten_runtime/addon/addon_manager.h"
#include "include_internal/ten_runtime/binding/python/addon/addon.h"
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "ten_runtime/addon/extension/extension.h"
#include "ten_utils/macro/mark.h"

PyObject *ten_py_addon_manager_register_addon_as_extension(PyObject *self,
                                                           PyObject *args) {
  const char *name = NULL;
  PyObject *base_dir = NULL;
  PyObject *py_addon_object = NULL;
  PyObject *py_register_ctx = NULL;

  if (!PyArg_ParseTuple(args, "sOOO", &name, &base_dir, &py_addon_object,
                        &py_register_ctx)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when registering addon.");
  }

  if (base_dir != Py_None && !PyUnicode_Check(base_dir)) {
    return ten_py_raise_py_type_error_exception(
        "base_dir must be a string or None.");
  }

  if (!PyObject_TypeCheck(py_addon_object, ten_py_addon_py_type())) {
    return ten_py_raise_py_type_error_exception(
        "Object is not an instance of Python Addon.");
  }

  const char *base_dir_str = NULL;
  if (base_dir != Py_None) {
    base_dir_str = PyUnicode_AsUTF8(base_dir);
    if (!base_dir_str) {
      return ten_py_raise_py_value_error_exception(
          "Failed to convert base_dir to UTF-8 string.");
    }
  }

  ten_py_addon_t *py_addon = (ten_py_addon_t *)py_addon_object;
  ten_addon_register_ctx_t *register_ctx =
      (ten_addon_register_ctx_t *)PyCapsule_GetPointer(
          py_register_ctx, "ten_addon_register_ctx");
  if (!register_ctx) {
    return ten_py_raise_py_value_error_exception(
        "Failed to get register_ctx from capsule.");
  }

  ten_addon_host_t *c_addon_host = ten_addon_register_extension(
      name, base_dir_str, &py_addon->c_addon, register_ctx);
  if (!c_addon_host) {
    return ten_py_raise_py_value_error_exception(
        "Failed to register addon in ten_addon_register_extension.");
  }

  py_addon->c_addon_host = c_addon_host;

  Py_INCREF(py_addon_object);  // Ensure the object is kept alive.

  Py_RETURN_NONE;
}

static void ten_py_addon_register_func(TEN_ADDON_TYPE addon_type,
                                       ten_string_t *addon_name,
                                       void *register_ctx,
                                       TEN_UNUSED void *user_data) {
  TEN_ASSERT(addon_type == TEN_ADDON_TYPE_EXTENSION, "Invalid addon type.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *ten_module = PyImport_ImportModule("ten_runtime");
  TEN_ASSERT(ten_module, "Failed to import ten_runtime module.");

  PyObject *addon_manager = PyObject_GetAttrString(ten_module, "_AddonManager");
  TEN_ASSERT(addon_manager, "Failed to get _AddonManager from ten module.");

  PyObject *register_func =
      PyObject_GetAttrString(addon_manager, "_register_addon");
  TEN_ASSERT(register_func,
             "Failed to get _register_addon from _AddonManager.");

  PyObject *py_register_ctx =
      PyCapsule_New(register_ctx, "ten_addon_register_ctx", NULL);
  TEN_ASSERT(py_register_ctx, "Failed to create capsule for register_ctx.");

  PyObject *register_func_args = Py_BuildValue(
      "(sO)", ten_string_get_raw_str(addon_name), py_register_ctx);
  PyObject *register_func_result =
      PyObject_CallObject(register_func, register_func_args);
  TEN_ASSERT(register_func_result, "Failed to call _register_addon.");

  Py_DECREF(register_func_result);
  Py_DECREF(register_func_args);
  Py_DECREF(register_func);
  Py_DECREF(addon_manager);
  Py_DECREF(ten_module);
  Py_DECREF(py_register_ctx);

  ten_py_gil_state_release_internal(prev_state);
}

PyObject *ten_py_addon_manager_add_extension_addon(PyObject *self,
                                                   PyObject *args) {
  const char *name = NULL;
  if (!PyArg_ParseTuple(args, "s", &name)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when adding addon to addon manager.");
  }

  ten_addon_manager_t *addon_manager = ten_addon_manager_get_instance();

  ten_error_t error;
  TEN_ERROR_INIT(error);

  bool rc =
      ten_addon_manager_add_addon(addon_manager, "extension", name,
                                  ten_py_addon_register_func, NULL, &error);
  TEN_ASSERT(rc, "Failed to add addon to addon manager.");

  ten_error_deinit(&error);

  Py_RETURN_NONE;
}
