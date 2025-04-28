//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/app/app.h"

#include "include_internal/ten_runtime/binding/python/app/app.h"
#include "include_internal/ten_runtime/binding/python/common.h"
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/mark.h"

static bool ten_py_app_check_integrity(ten_py_app_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_APP_SIGNATURE) {
    return false;
  }

  return ten_app_check_integrity(self->c_app, check_thread);
}

static void proxy_on_configure(ten_app_t *app, ten_env_t *ten_env) {
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  TEN_LOGI("proxy_on_configure");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // The current state may be PyGILState_LOCKED or PyGILState_UNLOCKED which
  // depends on whether the app is running in the Python thread or native
  // thread.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_app_t *py_app =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)app);
  TEN_ASSERT(py_app && ten_py_app_check_integrity(py_app, true),
             "Should not happen.");

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  py_ten_env->c_ten_env_proxy = ten_env_proxy_create(ten_env, 1, NULL);

  // Call python function.
  PyObject *py_res = PyObject_CallMethod((PyObject *)py_app, "on_configure",
                                         "O", py_ten_env->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  if (prev_state == PyGILState_UNLOCKED) {
    // Since the original environment did not hold the GIL, we release the gil
    // here. However, an optimization has been made to avoid releasing the
    // thread state, allowing it to be reused later.
    //
    // The effect of not calling PyGILState_Release here is that, since the
    // number of calls to PyGILState_Ensure and PyGILState_Release are not
    // equal, the Python thread state will not be released, only the gil will be
    // released. It is not until on_deinit_done is reached that the
    // corresponding PyGILState_Release for PyGILState_Ensure is called,
    // achieving numerical consistency between PyGILState_Ensure and
    // PyGILState_Release, and only then will the Python thread state be
    // released.
    py_ten_env->py_thread_state = ten_py_eval_save_thread();
  } else {
    // No need to release the GIL.
  }

  py_ten_env->need_to_release_gil_state = true;

  TEN_LOGI("proxy_on_configure done");
}

static void proxy_on_init(ten_app_t *app, ten_env_t *ten_env) {
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  TEN_LOGI("proxy_on_init");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_app_t *py_app =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)app);
  TEN_ASSERT(py_app && ten_py_app_check_integrity(py_app, true),
             "Should not happen.");

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);

  PyObject *py_res = PyObject_CallMethod((PyObject *)py_app, "on_init", "O",
                                         py_ten_env->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);

  TEN_LOGI("proxy_on_init done");
}

static void proxy_on_deinit(ten_app_t *app, ten_env_t *ten_env) {
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  TEN_LOGI("proxy_on_deinit");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_app_t *py_app =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)app);
  TEN_ASSERT(py_app && ten_py_app_check_integrity(py_app, true),
             "Should not happen.");

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  TEN_ASSERT(py_ten_env, "Should not happen.");

  PyObject *py_res = PyObject_CallMethod((PyObject *)py_app, "on_deinit", "O",
                                         py_ten_env->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);

  TEN_LOGI("proxy_on_deinit done");
}

static PyObject *ten_py_app_create(PyTypeObject *type, PyObject *args,
                                   TEN_UNUSED PyObject *kwds) {
  ten_py_app_t *py_app = (ten_py_app_t *)type->tp_alloc(type, 0);
  if (!py_app) {
    TEN_ASSERT(0, "Failed to allocate Python app.");

    return ten_py_raise_py_memory_error_exception(
        "Failed to allocate memory for ten_py_app_t");
  }

  ten_signature_set(&py_app->signature, TEN_PY_APP_SIGNATURE);
  py_app->c_app = NULL;

  if (PyTuple_Size(args)) {
    TEN_ASSERT(0, "Expect 0 argument.");

    Py_DECREF(py_app);
    return ten_py_raise_py_type_error_exception("Expect 0 argument.");
  }

  py_app->c_app =
      ten_app_create(proxy_on_configure, proxy_on_init, proxy_on_deinit, NULL);
  if (!py_app->c_app) {
    TEN_ASSERT(0, "Failed to create TEN app.");

    Py_DECREF(py_app);
    return ten_py_raise_py_system_error_exception("Failed to create TEN app.");
  }

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)py_app->c_app, py_app);

  return (PyObject *)py_app;
}

static PyObject *ten_py_app_run(PyObject *self, PyObject *args) {
  ten_py_app_t *py_app = (ten_py_app_t *)self;

  TEN_ASSERT(py_app && ten_py_app_check_integrity(py_app, true),
             "Invalid argument.");

  int run_in_background_flag = 1;
  if (!PyArg_ParseTuple(args, "i", &run_in_background_flag)) {
    return NULL;
  }

  TEN_LOGI("ten_py_app_run: %d", run_in_background_flag);

  bool rc = false;
  if (run_in_background_flag == 0) {
    PyThreadState *saved_py_thread_state = PyEval_SaveThread();

    // Blocking operation.
    rc = ten_app_run(py_app->c_app, false, NULL);

    PyEval_RestoreThread(saved_py_thread_state);
  } else {
    rc = ten_app_run(py_app->c_app, true, NULL);
  }

  TEN_LOGI("ten_py_app_run done: %d", rc);

  if (!rc) {
    return ten_py_raise_py_runtime_error_exception("Failed to run ten_app.");
  }

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_RETURN_NONE;
}

static PyObject *ten_py_app_close(PyObject *self, PyObject *args) {
  ten_py_app_t *py_app = (ten_py_app_t *)self;

  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function is designed to be called from any thread (not
  // just the app thread).
  TEN_ASSERT(ten_py_app_check_integrity(py_app, false), "Invalid argument.");

  if (PyTuple_Size(args)) {
    return ten_py_raise_py_type_error_exception("Expect 0 argument.");
  }

  bool rc = ten_app_close(py_app->c_app, NULL);
  if (!rc) {
    return ten_py_raise_py_runtime_error_exception("Failed to close TEN app.");
  }

  Py_RETURN_NONE;
}

static PyObject *ten_py_app_wait(PyObject *self, PyObject *args) {
  ten_py_app_t *py_app = (ten_py_app_t *)self;

  TEN_ASSERT(py_app && ten_py_app_check_integrity(py_app, true),
             "Invalid argument.");

  TEN_LOGI("ten_py_app_wait");

  if (PyTuple_Size(args)) {
    return ten_py_raise_py_type_error_exception("Expect 0 argument.");
  }

  PyThreadState *saved_py_thread_state = PyEval_SaveThread();

  // Blocking operation.
  bool rc = ten_app_wait(py_app->c_app, NULL);

  PyEval_RestoreThread(saved_py_thread_state);

  if (!rc) {
    return ten_py_raise_py_runtime_error_exception(
        "Failed to wait for TEN app.");
  }

  TEN_LOGI("ten_py_app_wait done");

  Py_RETURN_NONE;
}

static void ten_py_app_destroy(PyObject *self) {
  ten_py_app_t *py_app = (ten_py_app_t *)self;
  TEN_ASSERT(py_app, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function is designed to be called from any thread (not
  // just the app thread).
  TEN_ASSERT(ten_py_app_check_integrity(py_app, false), "Invalid argument.");

  ten_app_close(py_app->c_app, NULL);

  ten_app_destroy(py_app->c_app);
  py_app->c_app = NULL;

  Py_TYPE(self)->tp_free(self);
}

// Defines a new Python class named App.
static PyTypeObject *ten_py_app_py_type(void) {
  // Currently, we don't have any properties defined in the Python App class.
  static PyGetSetDef py_app_type_properties[] = {
      {NULL, NULL, NULL, NULL, NULL}};

  static PyMethodDef py_app_type_methods[] = {
      {"run", ten_py_app_run, METH_VARARGS, NULL},
      {"close", ten_py_app_close, METH_VARARGS, NULL},
      {"wait", ten_py_app_wait, METH_VARARGS, NULL},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject py_app_type = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name = "libten_runtime_python._App",
      .tp_doc = PyDoc_STR("_App"),
      .tp_basicsize = sizeof(ten_py_app_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_new = ten_py_app_create,
      .tp_init = NULL,
      .tp_dealloc = ten_py_app_destroy,
      .tp_getset = py_app_type_properties,
      .tp_methods = py_app_type_methods,
  };

  return &py_app_type;
}

bool ten_py_app_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_app_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception("Python App class is not ready.");
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_App", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");
    return false;
  }

  return true;
}
