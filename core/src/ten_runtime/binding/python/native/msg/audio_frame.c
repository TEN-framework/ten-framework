//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/msg/audio_frame.h"

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "longobject.h"
#include "object.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static ten_py_audio_frame_t *ten_py_audio_frame_create_internal(
    PyTypeObject *py_type) {
  if (!py_type) {
    py_type = ten_py_audio_frame_py_type();
  }

  ten_py_audio_frame_t *py_audio_frame =
      (ten_py_audio_frame_t *)py_type->tp_alloc(py_type, 0);
  TEN_ASSERT(py_audio_frame, "Failed to allocate memory.");

  ten_signature_set(&py_audio_frame->msg.signature, TEN_PY_MSG_SIGNATURE);
  py_audio_frame->msg.c_msg = NULL;

  return py_audio_frame;
}

static ten_py_audio_frame_t *ten_py_audio_frame_init(
    ten_py_audio_frame_t *py_audio_frame, const char *name) {
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  py_audio_frame->msg.c_msg = ten_audio_frame_create(name, NULL);

  return py_audio_frame;
}

PyObject *ten_py_audio_frame_create(PyTypeObject *type, PyObject *args,
                                    TEN_UNUSED PyObject *kwds) {
  const char *name = NULL;
  if (!PyArg_ParseTuple(args, "s", &name)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  ten_py_audio_frame_t *audio_frame = ten_py_audio_frame_create_internal(type);
  return (PyObject *)ten_py_audio_frame_init(audio_frame, name);
}

void ten_py_audio_frame_destroy(PyObject *self) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  ten_py_msg_destroy_c_msg(&py_audio_frame->msg);
  Py_TYPE(self)->tp_free(self);
}

ten_py_audio_frame_t *ten_py_audio_frame_wrap(ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(audio_frame && ten_msg_check_integrity(audio_frame),
             "Invalid argument.");

  ten_py_audio_frame_t *py_audio_frame =
      ten_py_audio_frame_create_internal(NULL);
  py_audio_frame->msg.c_msg = ten_shared_ptr_clone(audio_frame);
  return py_audio_frame;
}

void ten_py_audio_frame_invalidate(ten_py_audio_frame_t *self) {
  TEN_ASSERT(self, "Invalid argument");
  Py_DECREF(self);
}

bool ten_py_audio_frame_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_audio_frame_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception(
        "Python AudioFrame class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_AudioFrame", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
  return true;
}

PyObject *ten_py_audio_frame_alloc_buf(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int size = 0;
  if (!PyArg_ParseTuple(args, "i", &size)) {
    return ten_py_raise_py_value_error_exception("Invalid argument.");
  }

  if (size <= 0) {
    return ten_py_raise_py_value_error_exception("Invalid video frame size.");
  }

  ten_audio_frame_alloc_buf(py_audio_frame->msg.c_msg, size);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_lock_buf(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_buf_t *data = ten_audio_frame_peek_buf(py_audio_frame->msg.c_msg);

  if (!ten_msg_add_locked_res_buf(py_audio_frame->msg.c_msg, data->data,
                                  &err)) {
    return ten_py_raise_py_runtime_error_exception(
        "Failed to lock buffer in video frame.");
  }

  return PyMemoryView_FromMemory((char *)data->data, (Py_ssize_t)data->size,
                                 PyBUF_WRITE);
}

PyObject *ten_py_audio_frame_unlock_buf(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  Py_buffer py_buf;
  if (!PyArg_ParseTuple(args, "y*", &py_buf)) {
    return ten_py_raise_py_value_error_exception("Invalid argument.");
  }

  Py_ssize_t size = 0;
  uint8_t *data = py_buf.buf;
  if (!data) {
    return ten_py_raise_py_value_error_exception("Invalid buffer.");
  }

  size = py_buf.len;
  if (size <= 0) {
    return ten_py_raise_py_value_error_exception("Invalid buffer size.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!ten_msg_remove_locked_res_buf(py_audio_frame->msg.c_msg, data, &err)) {
    return ten_py_raise_py_runtime_error_exception(
        "Failed to unlock buffer in audio frame.");
  }

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_buf(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_buf_t *data = ten_audio_frame_peek_buf(py_audio_frame->msg.c_msg);
  if (!data->data) {
    return ten_py_raise_py_system_error_exception("Failed to get buffer.");
  }

  return PyByteArray_FromStringAndSize((const char *)data->data,
                                       (Py_ssize_t)data->size);
}

PyObject *ten_py_audio_frame_get_timestamp(PyObject *self, PyObject *unused) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLongLong(
      ten_audio_frame_get_timestamp(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_timestamp(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int64_t timestamp = 0;
  if (!PyArg_ParseTuple(args, "L", &timestamp)) {
    return ten_py_raise_py_value_error_exception("Invalid timestamp.");
  }

  ten_audio_frame_set_timestamp(py_audio_frame->msg.c_msg, timestamp);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_sample_rate(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_audio_frame_get_sample_rate(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_sample_rate(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int32_t sample_rate = 0;
  if (!PyArg_ParseTuple(args, "i", &sample_rate)) {
    return NULL;
  }

  ten_audio_frame_set_sample_rate(py_audio_frame->msg.c_msg, sample_rate);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_samples_per_channel(PyObject *self,
                                                     PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_audio_frame_get_samples_per_channel(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_samples_per_channel(PyObject *self,
                                                     PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int32_t samples_per_channel = 0;
  if (!PyArg_ParseTuple(args, "i", &samples_per_channel)) {
    return NULL;
  }

  ten_audio_frame_set_samples_per_channel(py_audio_frame->msg.c_msg,
                                          samples_per_channel);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_bytes_per_sample(PyObject *self,
                                                  PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_audio_frame_get_bytes_per_sample(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_bytes_per_sample(PyObject *self,
                                                  PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int32_t size = 0;
  if (!PyArg_ParseTuple(args, "i", &size)) {
    return NULL;
  }

  ten_audio_frame_set_bytes_per_sample(py_audio_frame->msg.c_msg, size);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_number_of_channels(PyObject *self,
                                                    PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_audio_frame_get_number_of_channel(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_number_of_channels(PyObject *self,
                                                    PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int32_t number = 0;
  if (!PyArg_ParseTuple(args, "i", &number)) {
    return NULL;
  }

  ten_audio_frame_set_number_of_channel(py_audio_frame->msg.c_msg, number);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_data_fmt(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_audio_frame_get_data_fmt(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_data_fmt(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int32_t format = 0;
  if (!PyArg_ParseTuple(args, "i", &format)) {
    return NULL;
  }

  ten_audio_frame_set_data_fmt(py_audio_frame->msg.c_msg, format);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_get_line_size(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_audio_frame_get_line_size(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_line_size(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int32_t line_size = 0;
  if (!PyArg_ParseTuple(args, "i", &line_size)) {
    return NULL;
  }

  ten_audio_frame_set_line_size(py_audio_frame->msg.c_msg, line_size);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_is_eof(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  return PyBool_FromLong(ten_audio_frame_is_eof(py_audio_frame->msg.c_msg));
}

PyObject *ten_py_audio_frame_set_eof(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");

  int is_eof = 0;
  if (!PyArg_ParseTuple(args, "p", &is_eof)) {
    return NULL;
  }

  ten_audio_frame_set_eof(py_audio_frame->msg.c_msg, is_eof);

  Py_RETURN_NONE;
}

PyObject *ten_py_audio_frame_clone(PyObject *self, PyObject *args) {
  ten_py_audio_frame_t *py_audio_frame = (ten_py_audio_frame_t *)self;
  TEN_ASSERT(py_audio_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_audio_frame),
             "Invalid argument.");
  TEN_ASSERT(py_audio_frame->msg.c_msg, "Invalid argument.");

  ten_shared_ptr_t *cloned_msg = ten_msg_clone(py_audio_frame->msg.c_msg, NULL);
  TEN_ASSERT(cloned_msg, "Should not happen.");

  ten_py_audio_frame_t *cloned_py_audio_frame =
      ten_py_audio_frame_create_internal(NULL);
  cloned_py_audio_frame->msg.c_msg = cloned_msg;
  return (PyObject *)cloned_py_audio_frame;
}
