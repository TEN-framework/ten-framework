//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/test/env_tester.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/macro/check.h"

bool ten_py_ten_env_tester_check_integrity(ten_py_ten_env_tester_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_TEN_ENV_TESTER_SIGNATURE) {
    return false;
  }

  return true;
}

static void ten_py_ten_env_tester_c_part_destroyed(
    void *ten_env_tester_bridge_) {
  ten_py_ten_env_tester_t *ten_env_tester_bridge =
      (ten_py_ten_env_tester_t *)ten_env_tester_bridge_;

  TEN_ASSERT(ten_env_tester_bridge_ &&
                 ten_py_ten_env_tester_check_integrity(ten_env_tester_bridge_),
             "Should not happen.");

  ten_env_tester_bridge->c_ten_env_tester = NULL;

  ten_py_ten_env_tester_invalidate(ten_env_tester_bridge);
}

static PyObject *create_actual_py_ten_env_tester_instance(
    ten_py_ten_env_tester_t *py_ten_env_tester) {
  // Import the Python module where TenEnvTester is defined.
  PyObject *module_name = PyUnicode_FromString("ten_runtime.test");
  PyObject *module = PyImport_Import(module_name);
  Py_DECREF(module_name);

  if (!module) {
    PyErr_Print();
    return NULL;
  }

  // Get the TenEnvTester class from the module.
  PyObject *ten_env_tester_class =
      PyObject_GetAttrString(module, "TenEnvTester");
  Py_DECREF(module);

  if (!ten_env_tester_class || !PyCallable_Check(ten_env_tester_class)) {
    PyErr_Print();
    Py_XDECREF(ten_env_tester_class);
    return NULL;
  }

  // Create the argument tuple with the _TenEnvTester object
  PyObject *args = PyTuple_Pack(1, py_ten_env_tester);

  // Create an instance of the TenEnvTester class.
  PyObject *ten_env_tester_instance =
      PyObject_CallObject(ten_env_tester_class, args);
  Py_DECREF(ten_env_tester_class);
  Py_DECREF(args);

  if (!ten_env_tester_instance) {
    PyErr_Print();
    return NULL;
  }

  return ten_env_tester_instance;
}

ten_py_ten_env_tester_t *ten_py_ten_env_tester_wrap(
    ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(ten_env_tester, "ten_env_tester should not be NULL.");

  ten_py_ten_env_tester_t *py_ten_env_tester =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  if (py_ten_env_tester) {
    // The `ten_env_tester` has already been wrapped, so we directly returns the
    // previously wrapped result.
    return py_ten_env_tester;
  }

  PyTypeObject *py_ten_env_tester_py_type = ten_py_ten_env_tester_type();

  // Create a new py_ten_env for wrapping.
  py_ten_env_tester =
      (ten_py_ten_env_tester_t *)py_ten_env_tester_py_type->tp_alloc(
          py_ten_env_tester_py_type, 0);
  TEN_ASSERT(ten_env_tester, "Failed to allocate memory.");

  ten_signature_set(&py_ten_env_tester->signature,
                    TEN_PY_TEN_ENV_TESTER_SIGNATURE);
  py_ten_env_tester->c_ten_env_tester = ten_env_tester;
  py_ten_env_tester->c_ten_env_tester_proxy = NULL;

  py_ten_env_tester->actual_py_ten_env_tester =
      create_actual_py_ten_env_tester_instance(py_ten_env_tester);
  if (!py_ten_env_tester->actual_py_ten_env_tester) {
    TEN_ASSERT(0, "Failed to create actual py_ten_env_tester.");
    Py_DECREF(py_ten_env_tester);
    return NULL;
  }

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)ten_env_tester, py_ten_env_tester);

  ten_env_tester_set_destroy_handler_in_target_lang(
      ten_env_tester, ten_py_ten_env_tester_c_part_destroyed);

  return py_ten_env_tester;
}

void ten_py_ten_env_tester_invalidate(
    ten_py_ten_env_tester_t *py_ten_env_tester) {
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");

  if (py_ten_env_tester->actual_py_ten_env_tester) {
    Py_DECREF(py_ten_env_tester->actual_py_ten_env_tester);
    py_ten_env_tester->actual_py_ten_env_tester = NULL;
  }

  Py_DECREF(py_ten_env_tester);
}

static void ten_py_ten_env_tester_destroy(PyObject *self) {
  Py_TYPE(self)->tp_free(self);
}

PyTypeObject *ten_py_ten_env_tester_type(void) {
  static PyMethodDef ten_methods[] = {
      {"on_start_done", ten_py_ten_env_tester_on_start_done, METH_VARARGS,
       NULL},
      {"on_stop_done", ten_py_ten_env_tester_on_stop_done, METH_VARARGS, NULL},
      {"on_deinit_done", ten_py_ten_env_tester_on_deinit_done, METH_VARARGS,
       NULL},
      {"stop_test", ten_py_ten_env_tester_stop_test, METH_VARARGS, NULL},
      {"send_cmd", ten_py_ten_env_tester_send_cmd, METH_VARARGS, NULL},
      {"send_data", ten_py_ten_env_tester_send_data, METH_VARARGS, NULL},
      {"send_audio_frame", ten_py_ten_env_tester_send_audio_frame, METH_VARARGS,
       NULL},
      {"send_video_frame", ten_py_ten_env_tester_send_video_frame, METH_VARARGS,
       NULL},
      {"return_result", ten_py_ten_env_tester_return_result, METH_VARARGS,
       NULL},
      {"log", ten_py_ten_env_tester_log, METH_VARARGS, NULL},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject ty = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name =
          "libten_runtime_python._TenEnvTester",
      .tp_doc = PyDoc_STR("_TenEnvTester"),
      .tp_basicsize = sizeof(ten_py_ten_env_tester_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT,

      // The metadata info will be created by the Python binding and not by the
      // user within the Python environment.
      .tp_new = NULL,

      .tp_init = NULL,
      .tp_dealloc = ten_py_ten_env_tester_destroy,
      .tp_getset = NULL,
      .tp_methods = ten_methods,
  };
  return &ty;
}

bool ten_py_ten_env_tester_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_ten_env_tester_type();
  if (PyType_Ready(ten_py_ten_env_tester_type()) < 0) {
    ten_py_raise_py_system_error_exception(
        "Python TenEnvTester class is not ready.");
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_TenEnvTester", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");
    return false;
  }

  return true;
}
