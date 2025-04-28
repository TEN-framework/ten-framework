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

typedef struct ten_py_addon_manager_register_addon_decorator_t {
  PyObject_HEAD
} ten_py_addon_manager_register_addon_decorator_t;

TEN_RUNTIME_PRIVATE_API PyObject *
ten_py_addon_manager_register_addon_as_extension(PyObject *self,
                                                 PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_addon_manager_add_extension_addon(
    PyObject *self, PyObject *args);
