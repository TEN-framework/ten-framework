//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/msg/video_frame.h"

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "memoryobject.h"
#include "ten_runtime/msg/video_frame/video_frame.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static ten_py_video_frame_t *ten_py_video_frame_create_internal(
    PyTypeObject *py_type) {
  if (!py_type) {
    py_type = ten_py_video_frame_py_type();
  }

  ten_py_video_frame_t *py_video_frame =
      (ten_py_video_frame_t *)py_type->tp_alloc(py_type, 0);
  TEN_ASSERT(py_video_frame, "Failed to allocate memory.");

  ten_signature_set(&py_video_frame->msg.signature, TEN_PY_MSG_SIGNATURE);
  py_video_frame->msg.c_msg = NULL;

  return py_video_frame;
}

static ten_py_video_frame_t *ten_py_video_frame_init(
    ten_py_video_frame_t *py_video_frame, const char *name) {
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  py_video_frame->msg.c_msg = ten_video_frame_create(name, NULL);

  return py_video_frame;
}

PyObject *ten_py_video_frame_create(PyTypeObject *type, PyObject *args,
                                    TEN_UNUSED PyObject *kwds) {
  const char *name = NULL;
  if (!PyArg_ParseTuple(args, "s", &name)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  ten_py_video_frame_t *py_video_frame =
      ten_py_video_frame_create_internal(type);
  return (PyObject *)ten_py_video_frame_init(py_video_frame, name);
}

void ten_py_video_frame_destroy(PyObject *self) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  ten_py_msg_destroy_c_msg(&py_video_frame->msg);
  Py_TYPE(self)->tp_free(self);
}

ten_py_video_frame_t *ten_py_video_frame_wrap(ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(video_frame && ten_msg_check_integrity(video_frame),
             "Invalid argument.");

  ten_py_video_frame_t *py_video_frame =
      ten_py_video_frame_create_internal(NULL);
  py_video_frame->msg.c_msg = ten_shared_ptr_clone(video_frame);
  return py_video_frame;
}

void ten_py_video_frame_invalidate(ten_py_video_frame_t *self) {
  TEN_ASSERT(self, "Invalid argument");
  Py_DECREF(self);
}

PyObject *ten_py_video_frame_alloc_buf(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  int size = 0;
  if (!PyArg_ParseTuple(args, "i", &size)) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_value_error_exception("Invalid video frame size.");
  }

  if (size <= 0) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_value_error_exception("Invalid video frame size.");
  }

  ten_video_frame_alloc_data(py_video_frame->msg.c_msg, size);

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_lock_buf(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_buf_t *data = ten_video_frame_peek_buf(py_video_frame->msg.c_msg);

  if (!ten_msg_add_locked_res_buf(py_video_frame->msg.c_msg, data->data,
                                  &err)) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_system_error_exception(
        "Failed to lock buffer in video frame.");
  }

  return PyMemoryView_FromMemory((char *)data->data, (Py_ssize_t)data->size,
                                 PyBUF_WRITE);
}

PyObject *ten_py_video_frame_unlock_buf(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  Py_buffer py_buf;
  if (!PyArg_ParseTuple(args, "y*", &py_buf)) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_value_error_exception("Invalid buffer.");
  }

  Py_ssize_t size = 0;
  uint8_t *data = py_buf.buf;
  if (!data) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_value_error_exception("Invalid buffer.");
  }

  size = py_buf.len;
  if (size <= 0) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_value_error_exception("Invalid buffer size.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!ten_msg_remove_locked_res_buf(py_video_frame->msg.c_msg, data, &err)) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_system_error_exception(
        "Failed to unlock buffer in video frame.");
  }

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_get_buf(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_buf_t *buf = ten_video_frame_peek_buf(py_video_frame->msg.c_msg);
  uint8_t *data = buf->data;
  size_t data_size = buf->size;
  if (!data) {
    TEN_ASSERT(0, "Should not happen.");
    return ten_py_raise_py_system_error_exception("Failed to get buffer.");
  }

  return PyByteArray_FromStringAndSize((const char *)data,
                                       (Py_ssize_t)data_size);
}

PyObject *ten_py_video_frame_get_width(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  return PyLong_FromLong(ten_video_frame_get_width(py_video_frame->msg.c_msg));
}

PyObject *ten_py_video_frame_set_width(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  int width = 0;
  if (!PyArg_ParseTuple(args, "i", &width)) {
    return ten_py_raise_py_value_error_exception("Invalid video frame width.");
  }

  ten_video_frame_set_width(py_video_frame->msg.c_msg, width);

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_get_height(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  return PyLong_FromLong(ten_video_frame_get_height(py_video_frame->msg.c_msg));
}

PyObject *ten_py_video_frame_set_height(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  int height = 0;
  if (!PyArg_ParseTuple(args, "i", &height)) {
    return ten_py_raise_py_value_error_exception("Invalid video frame height.");
  }

  ten_video_frame_set_height(py_video_frame->msg.c_msg, height);

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_get_timestamp(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_video_frame_get_timestamp(py_video_frame->msg.c_msg));
}

PyObject *ten_py_video_frame_set_timestamp(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  int64_t timestamp = 0;
  if (!PyArg_ParseTuple(args, "L", &timestamp)) {
    return ten_py_raise_py_value_error_exception("Invalid timestamp.");
  }

  ten_video_frame_set_timestamp(py_video_frame->msg.c_msg, timestamp);

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_is_eof(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  return PyBool_FromLong(ten_video_frame_is_eof(py_video_frame->msg.c_msg));
}

PyObject *ten_py_video_frame_set_eof(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  int is_eof = 0;
  if (!PyArg_ParseTuple(args, "p", &is_eof)) {
    return ten_py_raise_py_value_error_exception("Invalid is_eof.");
  }

  ten_video_frame_set_eof(py_video_frame->msg.c_msg, is_eof);

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_get_pixel_fmt(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  return PyLong_FromLong(
      ten_video_frame_get_pixel_fmt(py_video_frame->msg.c_msg));
}

PyObject *ten_py_video_frame_set_pixel_fmt(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");

  int pixel_fmt = 0;
  if (!PyArg_ParseTuple(args, "i", &pixel_fmt)) {
    return ten_py_raise_py_value_error_exception("Invalid pixel format.");
  }

  ten_video_frame_set_pixel_fmt(py_video_frame->msg.c_msg, pixel_fmt);

  Py_RETURN_NONE;
}

PyObject *ten_py_video_frame_clone(PyObject *self, PyObject *args) {
  ten_py_video_frame_t *py_video_frame = (ten_py_video_frame_t *)self;
  TEN_ASSERT(py_video_frame &&
                 ten_py_msg_check_integrity((ten_py_msg_t *)py_video_frame),
             "Invalid argument.");
  TEN_ASSERT(py_video_frame->msg.c_msg, "Invalid argument.");

  ten_shared_ptr_t *cloned_msg = ten_msg_clone(py_video_frame->msg.c_msg, NULL);
  TEN_ASSERT(cloned_msg, "Should not happen.");

  ten_py_video_frame_t *cloned_py_video_frame =
      ten_py_video_frame_create_internal(NULL);
  cloned_py_video_frame->msg.c_msg = cloned_msg;
  return (PyObject *)cloned_py_video_frame;
}

bool ten_py_video_frame_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_video_frame_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception(
        "Python VideoFrame class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_VideoFrame", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
  return true;
}
