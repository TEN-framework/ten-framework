//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_py_data_t {
  ten_py_msg_t msg;
} ten_py_data_t;

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_data_py_type(void);

TEN_RUNTIME_PRIVATE_API bool ten_py_data_init_for_module(PyObject *module);

TEN_RUNTIME_PRIVATE_API ten_py_data_t *ten_py_data_wrap(ten_shared_ptr_t *data);

TEN_RUNTIME_PRIVATE_API void ten_py_data_invalidate(ten_py_data_t *self);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_data_create(PyTypeObject *type,
                                                     PyObject *args,
                                                     PyObject *kwds);

TEN_RUNTIME_PRIVATE_API void ten_py_data_destroy(PyObject *self);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_data_alloc_buf(PyObject *self,
                                                        PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_data_lock_buf(PyObject *self,
                                                       PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_data_unlock_buf(PyObject *self,
                                                         PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_data_get_buf(PyObject *self,
                                                      PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_data_clone(PyObject *self,
                                                    PyObject *args);
