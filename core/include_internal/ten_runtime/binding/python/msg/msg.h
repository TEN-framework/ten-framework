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
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"

#define TEN_PY_MSG_SIGNATURE 0x043846812DB094D9U

typedef struct ten_py_msg_t {
  PyObject_HEAD
  ten_signature_t signature;
  ten_shared_ptr_t *c_msg;
} ten_py_msg_t;

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_name(PyObject *self,
                                                      PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_name(PyObject *self,
                                                      PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_dest(PyObject *self,
                                                      PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_property_string(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_property_string(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_property_from_json(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_property_to_json(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_property_int(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_property_int(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_property_bool(PyObject *self,
                                                               PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_property_bool(PyObject *self,
                                                               PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_property_float(PyObject *self,
                                                                PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_property_float(PyObject *self,
                                                                PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_get_property_buf(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_msg_set_property_buf(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_msg_py_type(void);

TEN_RUNTIME_PRIVATE_API void ten_py_msg_destroy_c_msg(ten_py_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_py_msg_move_c_msg(
    ten_py_msg_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_py_msg_init_for_module(PyObject *module);

TEN_RUNTIME_PRIVATE_API bool ten_py_msg_check_integrity(ten_py_msg_t *self);
