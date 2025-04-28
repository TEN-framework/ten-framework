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

typedef struct ten_py_audio_frame_t {
  ten_py_msg_t msg;
} ten_py_audio_frame_t;

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_audio_frame_py_type(void);

TEN_RUNTIME_PRIVATE_API bool ten_py_audio_frame_init_for_module(
    PyObject *module);

TEN_RUNTIME_PRIVATE_API ten_py_audio_frame_t *ten_py_audio_frame_wrap(
    ten_shared_ptr_t *audio_frame);

TEN_RUNTIME_PRIVATE_API void ten_py_audio_frame_invalidate(
    ten_py_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_create(PyTypeObject *type,
                                                            PyObject *args,
                                                            PyObject *kwds);

TEN_RUNTIME_PRIVATE_API void ten_py_audio_frame_destroy(PyObject *self);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_alloc_buf(PyObject *self,
                                                               PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_lock_buf(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_unlock_buf(PyObject *self,
                                                                PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_buf(PyObject *self,
                                                             PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_timestamp(
    PyObject *self, PyObject *unused);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_timestamp(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_sample_rate(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_sample_rate(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_samples_per_channel(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_samples_per_channel(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_bytes_per_sample(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_bytes_per_sample(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_number_of_channels(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_number_of_channels(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_data_fmt(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_data_fmt(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_get_line_size(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_line_size(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_is_eof(PyObject *self,
                                                            PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_set_eof(PyObject *self,
                                                             PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_audio_frame_clone(PyObject *self,
                                                           PyObject *args);
