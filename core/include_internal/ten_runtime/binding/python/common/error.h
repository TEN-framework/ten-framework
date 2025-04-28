//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "ten_utils/lib/error.h"

typedef struct ten_py_error_t {
  PyObject_HEAD

  ten_error_t c_error;
} ten_py_error_t;

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_error_py_type(void);

TEN_RUNTIME_PRIVATE_API bool ten_py_error_init_for_module(PyObject *module);

TEN_RUNTIME_PRIVATE_API ten_py_error_t *ten_py_error_wrap(ten_error_t *error);

TEN_RUNTIME_PRIVATE_API void ten_py_error_invalidate(ten_py_error_t *error);

TEN_RUNTIME_PRIVATE_API void ten_py_error_destroy(PyObject *self);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_error_get_error_code(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_error_get_error_message(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API bool ten_py_check_and_clear_py_error(void);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_raise_py_value_error_exception(
    const char *msg, ...);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_raise_py_type_error_exception(
    const char *msg);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_raise_py_memory_error_exception(
    const char *msg);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_raise_py_system_error_exception(
    const char *msg);

TEN_RUNTIME_PRIVATE_API PyObject *
ten_py_raise_py_not_implemented_error_exception(const char *msg);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_raise_py_import_error_exception(
    const char *msg);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_raise_py_runtime_error_exception(
    const char *msg);
