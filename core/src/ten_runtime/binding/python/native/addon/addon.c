//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/addon/addon.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/binding/common.h"
#include "include_internal/ten_runtime/binding/python/addon/addon.h"
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/extension/extension.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "ten_runtime/addon/extension/extension.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/mark.h"

static bool ten_py_addon_check_integrity(ten_py_addon_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_ADDON_SIGNATURE) {
    return false;
  }

  return true;
}

static PyObject *not_implemented_base_on_init(TEN_UNUSED PyObject *self,
                                              TEN_UNUSED PyObject *args) {
  return ten_py_raise_py_not_implemented_error_exception(
      "The method 'on_init' must be implemented in the subclass of 'Addon'.");
}

static PyObject *not_implemented_base_on_deinit(TEN_UNUSED PyObject *self,
                                                TEN_UNUSED PyObject *args) {
  return ten_py_raise_py_not_implemented_error_exception(
      "The method 'on_deinit' must be implemented in the subclass of 'Addon'.");
}

static PyObject *not_implemented_base_on_create_instance(
    TEN_UNUSED PyObject *self, TEN_UNUSED PyObject *args) {
  return ten_py_raise_py_not_implemented_error_exception(
      "The method 'on_create_instance' must be implemented in the subclass of "
      "'Addon'.");
}

static void proxy_on_init(ten_addon_t *addon, ten_env_t *ten_env) {
  TEN_ASSERT(addon, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  if (!py_ten_env) {
    TEN_ASSERT(0, "Failed to wrap ten.");
    goto done;
  }

  ten_py_addon_t *py_addon = addon->binding_handle.me_in_target_lang;
  if (!py_addon) {
    Py_DECREF(py_ten_env);

    TEN_ASSERT(0, "Invalid addon in target language.");
    goto done;
  }
  TEN_ASSERT(ten_py_addon_check_integrity(py_addon), "Should not happen.");

  PyObject *py_res = PyObject_CallMethod((PyObject *)py_addon, "on_init", "O",
                                         py_ten_env->actual_py_ten_env);
  if (!py_res) {
    ten_py_check_and_clear_py_error();

    Py_DECREF(py_ten_env);

    TEN_ASSERT(0, "Python method on_init failed.");
    goto done;
  }

  Py_XDECREF(py_res);

done:
  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_deinit(ten_addon_t *addon, ten_env_t *ten_env) {
  TEN_ASSERT(addon, "Invalid argument.");
  // TODO(Wei): In the context of Python standalone tests, the Python addon is
  // registered into the TEN world within the extension tester thread (i.e., the
  // Python thread) but is unregistered in the test app thread. It should be
  // modified to also perform the Python addon registration within the test
  // app's `on_configure_done`. This change will allow the underlying thread
  // check to be set to `true`.
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, false), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  if (!py_ten_env) {
    TEN_ASSERT(0, "Failed to wrap ten.");
    goto done;
  }

  ten_py_addon_t *py_addon = addon->binding_handle.me_in_target_lang;
  if (!py_addon) {
    Py_DECREF(py_ten_env);

    TEN_ASSERT(0, "Invalid addon in target language.");
    goto done;
  }
  TEN_ASSERT(ten_py_addon_check_integrity(py_addon), "Should not happen.");

  PyObject *py_res = PyObject_CallMethod((PyObject *)py_addon, "on_deinit", "O",
                                         py_ten_env->actual_py_ten_env);
  if (!py_res) {
    ten_py_check_and_clear_py_error();

    Py_DECREF(py_ten_env);

    TEN_ASSERT(0, "Python method on_deinit failed.");
    goto done;
  }

  Py_XDECREF(py_res);

done:
  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_create_instance_async(ten_addon_t *addon,
                                           ten_env_t *ten_env, const char *name,
                                           void *context) {
  TEN_ASSERT(addon, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, false), "Invalid argument.");
  TEN_ASSERT(name && strlen(name), "Invalid argument.");

  ten_py_addon_t *py_addon = addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(py_addon && ten_py_addon_check_integrity(py_addon),
             "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  if (!py_addon || !name || !strlen(name)) {
    ten_py_raise_py_value_error_exception(
        "Invalid argument when creating instance.");

    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  if (!py_ten_env) {
    TEN_ASSERT(0, "Failed to wrap ten.");
    goto done;
  }

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_addon, "on_create_instance", "Osl",
                          py_ten_env->actual_py_ten_env, name, context);
  if (!py_res) {
    ten_py_check_and_clear_py_error();

    Py_DECREF(py_ten_env);

    TEN_ASSERT(0, "Python method on_create_instance failed.");
    goto done;
  }

  Py_XDECREF(py_res);

done:
  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_destroy_instance_async(ten_addon_t *addon,
                                            ten_env_t *ten_env, void *instance,
                                            void *context) {
  TEN_ASSERT(addon, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, false), "Invalid argument.");
  TEN_ASSERT(instance, "Invalid argument.");

  PyObject *py_instance = NULL;
  ten_py_addon_t *py_addon =
      (ten_py_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(py_addon && ten_py_addon_check_integrity(py_addon),
             "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  switch (py_addon->type) {
  case TEN_ADDON_TYPE_EXTENSION:
    py_instance = ten_binding_handle_get_me_in_target_lang(
        (ten_binding_handle_t *)instance);

    ten_py_extension_t *py_extension = (ten_py_extension_t *)py_instance;

    ten_extension_t *extension = py_extension->c_extension;
    TEN_ASSERT(extension, "Should not happen.");
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");

    TEN_ASSERT(extension == instance, "Should not happen.");

    ten_addon_host_t *addon_host = extension->addon_host;
    TEN_ASSERT(addon_host, "Should not happen.");
    TEN_ASSERT(ten_addon_host_check_integrity(addon_host),
               "Should not happen.");

    // Because the extension increases the reference count of the
    // corresponding `addon_host` when it is created, the reference count must
    // be decreased when the extension is destroyed.
    ten_ref_dec_ref(&addon_host->ref);
    extension->addon_host = NULL;
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  TEN_ASSERT(py_instance, "Failed to get Python instance.");

  // Decrement the reference count of the Python extension and its associated
  // ten object, so that Python GC can reclaim them.
  Py_DECREF(py_instance);

  ten_py_gil_state_release_internal(prev_state);

  ten_env_on_destroy_instance_done(ten_env, context, NULL);
}

static PyObject *ten_py_addon_create(PyTypeObject *type,
                                     TEN_UNUSED PyObject *args,
                                     TEN_UNUSED PyObject *kwds) {
  ten_py_addon_t *py_addon = (ten_py_addon_t *)type->tp_alloc(type, 0);
  if (!py_addon) {
    return ten_py_raise_py_memory_error_exception(
        "Failed to allocate memory for ten_py_addon_t");
  }

  ten_signature_set(&py_addon->signature, TEN_PY_ADDON_SIGNATURE);
  py_addon->type = TEN_ADDON_TYPE_EXTENSION;  // Now we only support extension.
  py_addon->c_addon_host = NULL;

  ten_addon_init(&py_addon->c_addon, proxy_on_init, proxy_on_deinit,
                 proxy_on_create_instance_async,
                 proxy_on_destroy_instance_async, NULL);

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)&py_addon->c_addon, py_addon);

  return (PyObject *)py_addon;
}

static void ten_py_addon_destroy(PyObject *self) {
  ten_py_addon_t *py_addon = (ten_py_addon_t *)self;
  TEN_ASSERT(py_addon && ten_py_addon_check_integrity(py_addon),
             "Invalid argument.");

  TEN_LOGI("[%s] destroy addon host for python addon.",
           ten_string_get_raw_str(&py_addon->c_addon_host->name));

  Py_TYPE(self)->tp_free(self);
}

PyTypeObject *ten_py_addon_py_type(void) {
  static PyMethodDef addon_methods[] = {
      {"on_init", not_implemented_base_on_init, METH_VARARGS,
       "Override to initialize."},
      {"on_deinit", not_implemented_base_on_deinit, METH_VARARGS,
       "Override to de-initialize."},
      {"on_create_instance", not_implemented_base_on_create_instance,
       METH_VARARGS, "Override to create your own instance."},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject py_type = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name = "libten_runtime_python._Addon",
      .tp_doc = PyDoc_STR("_Addon"),
      .tp_basicsize = sizeof(ten_py_addon_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_new = ten_py_addon_create,
      .tp_init = NULL,
      .tp_dealloc = ten_py_addon_destroy,
      .tp_getset = NULL,
      .tp_methods = addon_methods,
  };

  return &py_type;
}

bool ten_py_addon_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_addon_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception("Python Addon class is not ready.");
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_Addon", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");
    return false;
  }

  return true;
}
