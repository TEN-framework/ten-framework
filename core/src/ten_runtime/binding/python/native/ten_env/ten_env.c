//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/ten_env.h"

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "object.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/check.h"

bool ten_py_ten_env_check_integrity(ten_py_ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_TEN_ENV_SIGNATURE) {
    return false;
  }

  return true;
}

static void ten_py_ten_env_c_part_destroyed(void *ten_env_bridge_) {
  ten_py_ten_env_t *ten_env_bridge = (ten_py_ten_env_t *)ten_env_bridge_;
  TEN_ASSERT(ten_env_bridge && ten_py_ten_env_check_integrity(ten_env_bridge),
             "Should not happen.");

  ten_py_ten_env_invalidate(ten_env_bridge);
}

static PyObject *create_actual_py_ten_env_instance(
    ten_py_ten_env_t *py_ten_env) {
  // Import the Python module where TenEnv is defined.
  PyObject *module_name = PyUnicode_FromString("ten_runtime.ten_env");
  PyObject *module = PyImport_Import(module_name);
  Py_DECREF(module_name);

  if (!module) {
    PyErr_Print();
    return NULL;
  }

  // Get the TenEnv class from the module.
  PyObject *ten_env_class = PyObject_GetAttrString(module, "TenEnv");
  Py_DECREF(module);

  if (!ten_env_class || !PyCallable_Check(ten_env_class)) {
    PyErr_Print();
    Py_XDECREF(ten_env_class);
    return NULL;
  }

  // Create the argument tuple with the _TenEnv object
  PyObject *args = PyTuple_Pack(1, py_ten_env);

  // Create an instance of the TenEnv class.
  PyObject *ten_env_instance = PyObject_CallObject(ten_env_class, args);
  Py_DECREF(ten_env_class);
  Py_DECREF(args);

  if (!ten_env_instance) {
    PyErr_Print();
    return NULL;
  }

  return ten_env_instance;
}

ten_py_ten_env_t *ten_py_ten_env_wrap(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");

  ten_py_ten_env_t *py_ten_env =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  if (py_ten_env) {
    return py_ten_env;
  }

  PyTypeObject *py_ten_env_py_type = ten_py_ten_env_type();

  // Create a new py_ten_env.
  py_ten_env =
      (ten_py_ten_env_t *)py_ten_env_py_type->tp_alloc(py_ten_env_py_type, 0);
  TEN_ASSERT(py_ten_env, "Failed to allocate memory.");

  ten_signature_set(&py_ten_env->signature, TEN_PY_TEN_ENV_SIGNATURE);
  py_ten_env->c_ten_env = ten_env;
  py_ten_env->c_ten_env_proxy = NULL;
  py_ten_env->need_to_release_gil_state = false;
  py_ten_env->py_thread_state = NULL;

  py_ten_env->actual_py_ten_env = create_actual_py_ten_env_instance(py_ten_env);
  if (!py_ten_env->actual_py_ten_env) {
    TEN_ASSERT(0, "Should not happen.");
    Py_DECREF(py_ten_env);
    return NULL;
  }

  ten_binding_handle_set_me_in_target_lang((ten_binding_handle_t *)ten_env,
                                           py_ten_env);
  ten_env_set_destroy_handler_in_target_lang(ten_env,
                                             ten_py_ten_env_c_part_destroyed);

  return py_ten_env;
}

static PyObject *ten_py_ten_env_get_attach_to(ten_py_ten_env_t *self,
                                              void *closure) {
  return PyLong_FromLong(self->c_ten_env->attach_to);
}

void ten_py_ten_env_invalidate(ten_py_ten_env_t *py_ten_env) {
  TEN_ASSERT(py_ten_env, "Should not happen.");

  if (py_ten_env->actual_py_ten_env) {
    Py_DECREF(py_ten_env->actual_py_ten_env);
    py_ten_env->actual_py_ten_env = NULL;
  }

  Py_DECREF(py_ten_env);
}

static void ten_py_ten_env_destroy(PyObject *self) {
  Py_TYPE(self)->tp_free(self);
}

PyTypeObject *ten_py_ten_env_type(void) {
  static PyGetSetDef ten_getset[] = {
      {"_attach_to", (getter)ten_py_ten_env_get_attach_to, NULL, NULL, NULL},
      {NULL, NULL, NULL, NULL, NULL}};

  static PyMethodDef ten_methods[] = {
      {"on_configure_done", ten_py_ten_env_on_configure_done, METH_VARARGS,
       NULL},
      {"on_init_done", ten_py_ten_env_on_init_done, METH_VARARGS, NULL},
      {"on_start_done", ten_py_ten_env_on_start_done, METH_VARARGS, NULL},
      {"on_stop_done", ten_py_ten_env_on_stop_done, METH_VARARGS, NULL},
      {"on_deinit_done", ten_py_ten_env_on_deinit_done, METH_VARARGS, NULL},
      {"on_create_instance_done", ten_py_ten_env_on_create_instance_done,
       METH_VARARGS, NULL},
      {"send_cmd", ten_py_ten_env_send_cmd, METH_VARARGS, NULL},
      {"send_data", ten_py_ten_env_send_data, METH_VARARGS, NULL},
      {"send_video_frame", ten_py_ten_env_send_video_frame, METH_VARARGS, NULL},
      {"send_audio_frame", ten_py_ten_env_send_audio_frame, METH_VARARGS, NULL},
      {"get_property_to_json", ten_py_ten_env_get_property_to_json,
       METH_VARARGS, NULL},
      {"set_property_from_json", ten_py_ten_env_set_property_from_json,
       METH_VARARGS, NULL},
      {"return_result", ten_py_ten_env_return_result, METH_VARARGS, NULL},
      {"get_property_int", ten_py_ten_env_get_property_int, METH_VARARGS, NULL},
      {"set_property_int", ten_py_ten_env_set_property_int, METH_VARARGS, NULL},
      {"get_property_string", ten_py_ten_env_get_property_string, METH_VARARGS,
       NULL},
      {"set_property_string", ten_py_ten_env_set_property_string, METH_VARARGS,
       NULL},
      {"get_property_bool", ten_py_ten_env_get_property_bool, METH_VARARGS,
       NULL},
      {"set_property_bool", ten_py_ten_env_set_property_bool, METH_VARARGS,
       NULL},
      {"get_property_float", ten_py_ten_env_get_property_float, METH_VARARGS,
       NULL},
      {"set_property_float", ten_py_ten_env_set_property_float, METH_VARARGS,
       NULL},
      {"is_property_exist", ten_py_ten_env_is_property_exist, METH_VARARGS,
       NULL},
      {"init_property_from_json", ten_py_ten_env_init_property_from_json,
       METH_VARARGS, NULL},
      {"get_property_to_json_async", ten_py_ten_env_get_property_to_json_async,
       METH_VARARGS, NULL},
      {"set_property_from_json_async",
       ten_py_ten_env_set_property_from_json_async, METH_VARARGS, NULL},
      {"get_property_int_async", ten_py_ten_env_get_property_int_async,
       METH_VARARGS, NULL},
      {"set_property_int_async", ten_py_ten_env_set_property_int_async,
       METH_VARARGS, NULL},
      {"get_property_string_async", ten_py_ten_env_get_property_string_async,
       METH_VARARGS, NULL},
      {"set_property_string_async", ten_py_ten_env_set_property_string_async,
       METH_VARARGS, NULL},
      {"get_property_bool_async", ten_py_ten_env_get_property_bool_async,
       METH_VARARGS, NULL},
      {"set_property_bool_async", ten_py_ten_env_set_property_bool_async,
       METH_VARARGS, NULL},
      {"get_property_float_async", ten_py_ten_env_get_property_float_async,
       METH_VARARGS, NULL},
      {"set_property_float_async", ten_py_ten_env_set_property_float_async,
       METH_VARARGS, NULL},
      {"is_property_exist_async", ten_py_ten_env_is_property_exist_async,
       METH_VARARGS, NULL},
      {"init_property_from_json_async",
       ten_py_ten_env_init_property_from_json_async, METH_VARARGS, NULL},
      {"log", ten_py_ten_env_log, METH_VARARGS, NULL},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject ty = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name = "libten_runtime_python._TenEnv",
      .tp_doc = PyDoc_STR("_TenEnv"),
      .tp_basicsize = sizeof(ten_py_ten_env_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT,

      // The metadata info will be created by the Python binding and not by the
      // user within the Python environment.
      .tp_new = NULL,

      .tp_init = NULL,
      .tp_dealloc = ten_py_ten_env_destroy,
      .tp_getset = ten_getset,
      .tp_methods = ten_methods,
  };
  return &ty;
}

bool ten_py_ten_env_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_ten_env_type();
  if (PyType_Ready(ten_py_ten_env_type()) < 0) {
    ten_py_raise_py_system_error_exception("Python TenEnv class is not ready.");
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_TenEnv", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");
    return false;
  }

  return true;
}
