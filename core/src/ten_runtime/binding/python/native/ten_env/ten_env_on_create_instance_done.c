//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/extension/extension.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "object.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_utils/macro/mark.h"

PyObject *ten_py_ten_env_on_create_instance_done(PyObject *self,
                                                 TEN_UNUSED PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_py_extension_t *extension = NULL;
  void *context = NULL;

  if (!PyArg_ParseTuple(args, "O!l", ten_py_extension_py_type(), &extension,
                        &context)) {
    ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env.on_create_instance_done.");
  }

  bool rc = ten_env_on_create_instance_done(
      py_ten_env->c_ten_env, extension->c_extension, context, &err);
  TEN_ASSERT(rc, "Should not happen.");

  // It's necessary to keep the reference of the extension object to
  // prevent the python object from being destroyed when GC is triggered util
  // the addon's 'on_destroy_instance' function is called.
  Py_INCREF(extension);

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
