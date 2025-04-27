//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/test/extension_tester.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd.h"
#include "include_internal/ten_runtime/binding/python/msg/data.h"
#include "include_internal/ten_runtime/binding/python/msg/video_frame.h"
#include "include_internal/ten_runtime/binding/python/test/env_tester.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/test/extension_tester.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static bool ten_py_extension_tester_check_integrity(
    ten_py_extension_tester_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_EXTENSION_TESTER_SIGNATURE) {
    return false;
  }

  return true;
}

static ten_py_extension_tester_t *ten_py_extension_tester_create_internal(
    PyTypeObject *py_type) {
  if (!py_type) {
    py_type = ten_py_extension_tester_py_type();
  }

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)py_type->tp_alloc(py_type, 0);
  TEN_ASSERT(py_extension_tester, "Failed to allocate memory.");

  ten_signature_set(&py_extension_tester->signature,
                    TEN_PY_EXTENSION_TESTER_SIGNATURE);
  py_extension_tester->c_extension_tester = NULL;

  return py_extension_tester;
}

static void proxy_on_start(ten_extension_tester_t *extension_tester,
                           ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");

  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  ten_py_ten_env_tester_t *py_ten_env_tester =
      ten_py_ten_env_tester_wrap(ten_env_tester);
  py_extension_tester->py_ten_env_tester = (PyObject *)py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester->actual_py_ten_env_tester, "Should not happen.");

  py_ten_env_tester->c_ten_env_tester_proxy =
      ten_env_tester_proxy_create(ten_env_tester, NULL);
  TEN_ASSERT(py_ten_env_tester->c_ten_env_tester_proxy, "Should not happen.");

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension_tester, "_proxy_on_start",
                          "O", py_ten_env_tester->actual_py_ten_env_tester);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_stop(ten_extension_tester_t *extension_tester,
                          ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  ten_py_ten_env_tester_t *py_ten_env_tester =
      (ten_py_ten_env_tester_t *)py_extension_tester->py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");
  TEN_ASSERT(py_ten_env_tester->actual_py_ten_env_tester, "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension_tester, "_proxy_on_stop",
                          "O", py_ten_env_tester->actual_py_ten_env_tester);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_deinit(ten_extension_tester_t *extension_tester,
                            ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  ten_py_ten_env_tester_t *py_ten_env_tester =
      (ten_py_ten_env_tester_t *)py_extension_tester->py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");
  TEN_ASSERT(py_ten_env_tester->actual_py_ten_env_tester, "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension_tester, "_proxy_on_deinit",
                          "O", py_ten_env_tester->actual_py_ten_env_tester);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);

  // Do not release `py_ten_env_tester->c_ten_env_tester_proxy` here, because
  // the upper layer may still need to call the API of `ten_env` (e.g., some
  // asynchronous operations) before `on_deinit_done`. Therefore, the proxy
  // should only be released after `on_deinit_done`.
  //
  // This practice of releasing `ten_env_proxy` at the very end (i.e., after the
  // `on_deinit_done` of the extension and the `on_stop_done` of the app) is a
  // common feature across all language bindings of TEN.
}

static void proxy_on_cmd(ten_extension_tester_t *extension_tester,
                         ten_env_tester_t *ten_env_tester,
                         ten_shared_ptr_t *cmd) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  PyObject *py_ten_env_tester = py_extension_tester->py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");
  TEN_ASSERT(
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      "Should not happen.");

  ten_py_cmd_t *py_cmd = ten_py_cmd_wrap(cmd);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension_tester, "_proxy_on_cmd", "OO",
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      py_cmd);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_cmd_invalidate(py_cmd);

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_data(ten_extension_tester_t *extension_tester,
                          ten_env_tester_t *ten_env_tester,
                          ten_shared_ptr_t *data) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");
  TEN_ASSERT(data && ten_msg_check_integrity(data), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  PyObject *py_ten_env_tester = py_extension_tester->py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");
  TEN_ASSERT(
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      "Should not happen.");

  ten_py_data_t *py_data = ten_py_data_wrap(data);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension_tester, "_proxy_on_data", "OO",
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      py_data);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_data_invalidate(py_data);

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_audio_frame(ten_extension_tester_t *extension_tester,
                                 ten_env_tester_t *ten_env_tester,
                                 ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");
  TEN_ASSERT(audio_frame && ten_msg_check_integrity(audio_frame),
             "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  PyObject *py_ten_env_tester = py_extension_tester->py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");
  TEN_ASSERT(
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      "Should not happen.");

  ten_py_audio_frame_t *py_audio_frame = ten_py_audio_frame_wrap(audio_frame);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension_tester, "_proxy_on_audio_frame", "OO",
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      py_audio_frame);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_audio_frame_invalidate(py_audio_frame);

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_video_frame(ten_extension_tester_t *extension_tester,
                                 ten_env_tester_t *ten_env_tester,
                                 ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");
  TEN_ASSERT(video_frame && ten_msg_check_integrity(video_frame),
             "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  PyObject *py_ten_env_tester = py_extension_tester->py_ten_env_tester;
  TEN_ASSERT(py_ten_env_tester, "Should not happen.");
  TEN_ASSERT(
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      "Should not happen.");

  ten_py_video_frame_t *py_video_frame = ten_py_video_frame_wrap(video_frame);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension_tester, "_proxy_on_video_frame", "OO",
      ((ten_py_ten_env_tester_t *)py_ten_env_tester)->actual_py_ten_env_tester,
      py_video_frame);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_video_frame_invalidate(py_video_frame);

  ten_py_gil_state_release_internal(prev_state);
}

static ten_py_extension_tester_t *ten_py_extension_tester_init(
    ten_py_extension_tester_t *py_extension_tester, TEN_UNUSED PyObject *args,
    TEN_UNUSED PyObject *kw) {
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(
                     (ten_py_extension_tester_t *)py_extension_tester),
             "Invalid argument.");

  py_extension_tester->c_extension_tester = ten_extension_tester_create(
      NULL, proxy_on_start, proxy_on_stop, proxy_on_deinit, proxy_on_cmd,
      proxy_on_data, proxy_on_audio_frame, proxy_on_video_frame);

  ten_binding_handle_set_me_in_target_lang(
      &py_extension_tester->c_extension_tester->binding_handle,
      py_extension_tester);
  py_extension_tester->py_ten_env_tester = Py_None;

  return py_extension_tester;
}

PyObject *ten_py_extension_tester_create(PyTypeObject *type,
                                         TEN_UNUSED PyObject *args,
                                         TEN_UNUSED PyObject *kwds) {
  ten_py_extension_tester_t *py_extension_tester =
      ten_py_extension_tester_create_internal(type);
  return (PyObject *)ten_py_extension_tester_init(py_extension_tester, args,
                                                  kwds);
}

void ten_py_extension_tester_destroy(PyObject *self) {
  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)self;
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(
                     (ten_py_extension_tester_t *)py_extension_tester),
             "Invalid argument.");

  ten_extension_tester_destroy(py_extension_tester->c_extension_tester);
  Py_TYPE(self)->tp_free(self);
}

static PyObject *ten_py_extension_tester_set_test_mode_single(PyObject *self,
                                                              PyObject *args) {
  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)self;
  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 2) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when extension_tester.set_test_mode_single.");
  }

  const char *addon_name = NULL;
  const char *property_json_str = NULL;
  if (!PyArg_ParseTuple(args, "sz", &addon_name, &property_json_str)) {
    return ten_py_raise_py_value_error_exception(
        "Failed to parse arguments when "
        "extension_tester.set_test_mode_single.");
  }

  ten_extension_tester_set_test_mode_single(
      py_extension_tester->c_extension_tester, addon_name, property_json_str);

  Py_RETURN_NONE;
}

static PyObject *ten_py_extension_tester_run(PyObject *self, PyObject *args) {
  ten_py_extension_tester_t *py_extension_tester =
      (ten_py_extension_tester_t *)self;

  TEN_ASSERT(py_extension_tester &&
                 ten_py_extension_tester_check_integrity(py_extension_tester),
             "Invalid argument.");

  TEN_LOGI("ten_py_extension_tester_run");

  PyThreadState *saved_py_thread_state = PyEval_SaveThread();

  // This ia a blocking operation.
  bool rc = ten_extension_tester_run(py_extension_tester->c_extension_tester);

  PyEval_RestoreThread(saved_py_thread_state);

  TEN_LOGI("ten_py_extension_tester_run done: %d", rc);

  if (!rc) {
    return ten_py_raise_py_runtime_error_exception(
        "Failed to run ten_extension_tester.");
  }

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_RETURN_NONE;
}

PyTypeObject *ten_py_extension_tester_py_type(void) {
  static PyMethodDef py_methods[] = {
      {"set_test_mode_single", ten_py_extension_tester_set_test_mode_single,
       METH_VARARGS, NULL},
      {"run", ten_py_extension_tester_run, METH_VARARGS, NULL},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject py_type = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name =
          "libten_runtime_python._ExtensionTester",
      .tp_doc = PyDoc_STR("_ExtensionTester"),
      .tp_basicsize = sizeof(ten_py_extension_tester_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_new = ten_py_extension_tester_create,
      .tp_init = NULL,
      .tp_dealloc = ten_py_extension_tester_destroy,
      .tp_getset = NULL,
      .tp_methods = py_methods,
  };

  return &py_type;
}

bool ten_py_extension_tester_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_extension_tester_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception(
        "Python ExtensionTester class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_ExtensionTester", (PyObject *)py_type) <
      0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
  return true;
}
