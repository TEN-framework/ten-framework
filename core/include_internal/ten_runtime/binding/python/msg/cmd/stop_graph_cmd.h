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

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_cmd_stop_graph_py_type(void);

TEN_RUNTIME_PRIVATE_API bool ten_py_cmd_stop_graph_init_for_module(
    PyObject *module);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_cmd_stop_graph_register_type(
    PyObject *self, PyObject *args);
