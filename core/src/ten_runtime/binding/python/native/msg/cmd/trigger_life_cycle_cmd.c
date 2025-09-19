//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/msg/cmd/trigger_life_cycle_cmd.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd/cmd.h"
#include "ten_runtime/msg/cmd/trigger_life_cycle/cmd.h"
#include "ten_utils/macro/mark.h"

typedef ten_py_cmd_t ten_py_cmd_trigger_life_cycle_t;

static PyTypeObject *ten_py_cmd_trigger_life_cycle_type = NULL;

static ten_py_cmd_trigger_life_cycle_t *
ten_py_cmd_trigger_life_cycle_create_internal(PyTypeObject *py_type) {
  if (!py_type) {
    py_type = ten_py_cmd_trigger_life_cycle_type;
  }

  ten_py_cmd_trigger_life_cycle_t *py_cmd =
      (ten_py_cmd_trigger_life_cycle_t *)py_type->tp_alloc(py_type, 0);
  TEN_ASSERT(py_cmd, "Failed to allocate memory.");

  ten_signature_set(&py_cmd->msg.signature, TEN_PY_MSG_SIGNATURE);
  py_cmd->msg.c_msg = NULL;

  return py_cmd;
}

static void ten_py_cmd_trigger_life_cycle_destroy(PyObject *self) {
  ten_py_cmd_trigger_life_cycle_t *py_cmd =
      (ten_py_cmd_trigger_life_cycle_t *)self;
  TEN_ASSERT(py_cmd, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity((ten_py_msg_t *)py_cmd),
             "Invalid argument.");

  ten_py_msg_destroy_c_msg(&py_cmd->msg);
  Py_TYPE(self)->tp_free(self);
}

static PyObject *ten_py_cmd_trigger_life_cycle_create(PyTypeObject *type,
                                                      TEN_UNUSED PyObject *args,
                                                      TEN_UNUSED PyObject *kw) {
  ten_py_cmd_trigger_life_cycle_t *py_cmd =
      ten_py_cmd_trigger_life_cycle_create_internal(type);
  py_cmd->msg.c_msg = ten_cmd_trigger_life_cycle_create();
  return (PyObject *)py_cmd;
}

static PyObject *ten_py_cmd_trigger_life_cycle_set_stage(PyObject *self,
                                                         PyObject *args) {
  ten_py_cmd_trigger_life_cycle_t *py_cmd =
      (ten_py_cmd_trigger_life_cycle_t *)self;
  TEN_ASSERT(py_cmd, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity((ten_py_msg_t *)py_cmd),
             "Invalid argument.");

  const char *stage = NULL;
  if (!PyArg_ParseTuple(args, "s", &stage)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  bool result = ten_cmd_trigger_life_cycle_set_stage(py_cmd->msg.c_msg, stage);

  return PyBool_FromLong(result);
}

PyTypeObject *ten_py_cmd_trigger_life_cycle_py_type(void) {
  static PyMethodDef py_methods[] = {
      {"set_stage", ten_py_cmd_trigger_life_cycle_set_stage, METH_VARARGS,
       NULL},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject py_type = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name =
          "libten_runtime_python._TriggerLifeCycleCmd",
      .tp_doc = PyDoc_STR("_TriggerLifeCycleCmd"),
      .tp_basicsize = sizeof(ten_py_cmd_trigger_life_cycle_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_base = NULL,  // Will be set at runtime
      .tp_new = ten_py_cmd_trigger_life_cycle_create,
      .tp_init = NULL,
      .tp_dealloc = ten_py_cmd_trigger_life_cycle_destroy,
      .tp_getset = NULL,
      .tp_methods = py_methods,
  };

  return &py_type;
}

bool ten_py_cmd_trigger_life_cycle_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_cmd_trigger_life_cycle_py_type();

  // Set the base type at runtime
  py_type->tp_base = ten_py_cmd_py_type();

  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception(
        "Python CmdTriggerLifeCycle class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_TriggerLifeCycleCmd",
                            (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
  return true;
}

PyObject *ten_py_cmd_trigger_life_cycle_register_type(TEN_UNUSED PyObject *self,
                                                      PyObject *args) {
  PyObject *cls = NULL;
  if (!PyArg_ParseTuple(args, "O!", &PyType_Type, &cls)) {
    return NULL;
  }

  Py_XINCREF(cls);
  Py_XDECREF(ten_py_cmd_trigger_life_cycle_type);

  ten_py_cmd_trigger_life_cycle_type = (PyTypeObject *)cls;

  Py_RETURN_NONE;
}
