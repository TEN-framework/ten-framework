//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/msg/cmd.h"

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/cmd/cmd.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/mark.h"

static ten_py_cmd_t *ten_py_cmd_create_internal(PyTypeObject *py_type) {
  if (!py_type) {
    py_type = ten_py_cmd_py_type();
  }

  ten_py_cmd_t *py_cmd = (ten_py_cmd_t *)py_type->tp_alloc(py_type, 0);
  TEN_ASSERT(py_cmd, "Failed to allocate memory.");

  ten_signature_set(&py_cmd->msg.signature, TEN_PY_MSG_SIGNATURE);
  py_cmd->msg.c_msg = NULL;

  return py_cmd;
}

void ten_py_cmd_destroy(PyObject *self) {
  ten_py_cmd_t *py_cmd = (ten_py_cmd_t *)self;

  TEN_ASSERT(py_cmd && ten_py_msg_check_integrity((ten_py_msg_t *)py_cmd),
             "Invalid argument.");

  ten_py_msg_destroy_c_msg(&py_cmd->msg);
  Py_TYPE(self)->tp_free(self);
}

static ten_py_cmd_t *ten_py_cmd_init(ten_py_cmd_t *py_cmd, const char *name) {
  TEN_ASSERT(py_cmd && ten_py_msg_check_integrity((ten_py_msg_t *)py_cmd),
             "Invalid argument.");

  py_cmd->msg.c_msg = ten_cmd_create(name, NULL);
  return py_cmd;
}

PyObject *ten_py_cmd_create(PyTypeObject *type, PyObject *args,
                            TEN_UNUSED PyObject *kw) {
  const char *name = NULL;
  if (!PyArg_ParseTuple(args, "s", &name)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  ten_py_cmd_t *py_cmd = ten_py_cmd_create_internal(type);
  return (PyObject *)ten_py_cmd_init(py_cmd, name);
}

ten_py_cmd_t *ten_py_cmd_wrap(ten_shared_ptr_t *cmd) {
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  ten_py_cmd_t *py_cmd = ten_py_cmd_create_internal(NULL);
  py_cmd->msg.c_msg = ten_shared_ptr_clone(cmd);
  return py_cmd;
}

void ten_py_cmd_invalidate(ten_py_cmd_t *self) {
  TEN_ASSERT(self, "Invalid argument");
  Py_DECREF(self);
}

PyObject *ten_py_cmd_clone(PyObject *self, PyObject *args) {
  ten_py_cmd_t *py_cmd = (ten_py_cmd_t *)self;
  TEN_ASSERT(py_cmd && ten_py_msg_check_integrity((ten_py_msg_t *)py_cmd),
             "Invalid argument.");
  TEN_ASSERT(py_cmd->msg.c_msg, "Invalid argument.");

  ten_shared_ptr_t *cloned_msg = ten_msg_clone(py_cmd->msg.c_msg, NULL);
  TEN_ASSERT(cloned_msg, "Should not happen.");

  ten_py_cmd_t *cloned_py_cmd = ten_py_cmd_create_internal(NULL);
  cloned_py_cmd->msg.c_msg = cloned_msg;
  return (PyObject *)cloned_py_cmd;
}

bool ten_py_cmd_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_cmd_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception("Python Cmd class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_Cmd", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
  return true;
}
