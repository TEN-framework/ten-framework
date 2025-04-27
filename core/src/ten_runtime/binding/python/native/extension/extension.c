//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/extension.h"

#include "include_internal/ten_runtime/binding/python/common.h"
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/extension/extension.h"
#include "include_internal/ten_runtime/binding/python/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd.h"
#include "include_internal/ten_runtime/binding/python/msg/data.h"
#include "include_internal/ten_runtime/binding/python/msg/video_frame.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/mark.h"

static bool ten_py_extension_check_integrity(ten_py_extension_t *self,
                                             bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_EXTENSION_SIGNATURE) {
    return false;
  }

  return ten_extension_check_integrity(self->c_extension, check_thread);
}

static PyObject *stub_on_callback(TEN_UNUSED PyObject *self,
                                  TEN_UNUSED PyObject *args) {
  Py_RETURN_NONE;
}

static void proxy_on_configure(ten_extension_t *extension, ten_env_t *ten_env) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();
  // This function can only be called on the native thread not a Python
  // thread.
  TEN_ASSERT(prev_state == PyGILState_UNLOCKED,
             "The GIL should not be help by the extension thread now.");

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  py_extension->py_ten_env = (PyObject *)py_ten_env;

  py_ten_env->c_ten_env_proxy = ten_env_proxy_create(ten_env, 1, NULL);
  TEN_ASSERT(py_ten_env->c_ten_env_proxy &&
                 ten_env_proxy_check_integrity(py_ten_env->c_ten_env_proxy),
             "Invalid argument.");

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension, "_proxy_on_configure", "O",
                          py_ten_env->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  // We should release the GIL but not destroy the PyThreadState. The
  // PyThreadState will not be released until the last extension calls
  // 'on_deinit_done' in the group.
  py_ten_env->py_thread_state = ten_py_eval_save_thread();

  py_ten_env->need_to_release_gil_state = true;
}

static void proxy_on_init(ten_extension_t *extension, ten_env_t *ten_env) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();
  // This function can only be called on the native thread not a Python
  // thread.
  TEN_ASSERT(prev_state == PyGILState_UNLOCKED,
             "The GIL should not be help by the extension thread now.");

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension, "_proxy_on_init", "O",
                          ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_start(ten_extension_t *extension, ten_env_t *ten_env) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();
  TEN_ASSERT(prev_state == PyGILState_UNLOCKED,
             "The GIL should not be help by the extension thread now.");

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension, "_proxy_on_start", "O",
                          ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_stop(ten_extension_t *extension, ten_env_t *ten_env) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();
  TEN_ASSERT(prev_state == PyGILState_UNLOCKED,
             "The GIL should not be help by the extension thread now.");

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension, "_proxy_on_stop", "O",
                          ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_deinit(ten_extension_t *extension, ten_env_t *ten_env) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();
  TEN_ASSERT(prev_state == PyGILState_UNLOCKED,
             "The GIL should not be help by the extension thread now.");

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  PyObject *py_res =
      PyObject_CallMethod((PyObject *)py_extension, "_proxy_on_deinit", "O",
                          ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_cmd(ten_extension_t *extension, ten_env_t *ten_env,
                         ten_shared_ptr_t *cmd) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  ten_py_cmd_t *py_cmd = ten_py_cmd_wrap(cmd);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension, "_proxy_on_cmd", "OO",
      ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env, py_cmd);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_cmd_invalidate(py_cmd);

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_data(ten_extension_t *extension, ten_env_t *ten_env,
                          ten_shared_ptr_t *data) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(data && ten_msg_check_integrity(data), "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  ten_py_data_t *py_data = ten_py_data_wrap(data);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension, "_proxy_on_data", "OO",
      ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env, py_data);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_data_invalidate(py_data);

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_audio_frame(ten_extension_t *extension, ten_env_t *ten_env,
                                 ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(audio_frame && ten_msg_check_integrity(audio_frame),
             "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension);
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, true),
      "Invalid argument.");

  PyObject *py_ten_env = py_extension->py_ten_env;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  ten_py_audio_frame_t *py_audio_frame = ten_py_audio_frame_wrap(audio_frame);

  PyObject *py_res = PyObject_CallMethod(
      (PyObject *)py_extension, "_proxy_on_audio_frame", "OO",
      ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env, py_audio_frame);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_audio_frame_invalidate(py_audio_frame);

  ten_py_gil_state_release_internal(prev_state);
}

static void proxy_on_video_frame(ten_extension_t *extension, ten_env_t *ten_env,
                                 ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(video_frame && ten_msg_check_integrity(video_frame),
             "Invalid argument.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *py_extension = (PyObject *)ten_binding_handle_get_me_in_target_lang(
      (ten_binding_handle_t *)extension);
  PyObject *py_ten_env = ((ten_py_extension_t *)py_extension)->py_ten_env;
  ten_py_video_frame_t *py_video_frame = ten_py_video_frame_wrap(video_frame);

  PyObject *py_res = PyObject_CallMethod(
      py_extension, "_proxy_on_video_frame", "OO",
      ((ten_py_ten_env_t *)py_ten_env)->actual_py_ten_env, py_video_frame);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_video_frame_invalidate(py_video_frame);

  ten_py_gil_state_release_internal(prev_state);
}

static PyObject *ten_py_extension_create(PyTypeObject *type, PyObject *py_name,
                                         TEN_UNUSED PyObject *kwds) {
  ten_py_extension_t *py_extension =
      (ten_py_extension_t *)type->tp_alloc(type, 0);
  if (!py_extension) {
    TEN_ASSERT(0, "Failed to allocate Python extension.");

    return ten_py_raise_py_memory_error_exception(
        "Failed to allocate memory for ten_py_extension_t");
  }

  const char *name = NULL;
  if (!PyArg_ParseTuple(py_name, "s", &name)) {
    return ten_py_raise_py_type_error_exception("Invalid argument.");
  }

  ten_signature_set(&py_extension->signature, TEN_PY_EXTENSION_SIGNATURE);

  py_extension->c_extension = ten_extension_create(
      name, proxy_on_configure, proxy_on_init, proxy_on_start, proxy_on_stop,
      proxy_on_deinit, proxy_on_cmd, proxy_on_data, proxy_on_audio_frame,
      proxy_on_video_frame, NULL);
  TEN_ASSERT(py_extension->c_extension, "Should not happen.");

  ten_binding_handle_set_me_in_target_lang(
      &py_extension->c_extension->binding_handle, py_extension);
  py_extension->py_ten_env = Py_None;

  return (PyObject *)py_extension;
}

static void ten_py_extension_destroy(PyObject *self) {
  ten_py_extension_t *py_extension = (ten_py_extension_t *)self;

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In TEN world, the destroy operations need to be performed in
  // any threads.
  TEN_ASSERT(
      py_extension && ten_py_extension_check_integrity(py_extension, false),
      "Invalid argument.");

  ten_extension_destroy(py_extension->c_extension);
  py_extension->c_extension = NULL;

  Py_TYPE(self)->tp_free(self);
}

PyTypeObject *ten_py_extension_py_type(void) {
  static PyGetSetDef py_extension_type_properties[] = {
      {NULL, NULL, NULL, NULL, NULL}};

  static PyMethodDef py_extension_type_methods[] = {
      {"on_init", stub_on_callback, METH_VARARGS, NULL},
      {"on_start", stub_on_callback, METH_VARARGS, NULL},
      {"on_stop", stub_on_callback, METH_VARARGS, NULL},
      {"on_deinit", stub_on_callback, METH_VARARGS, NULL},
      {"on_cmd", stub_on_callback, METH_VARARGS, NULL},
      {"on_data", stub_on_callback, METH_VARARGS, NULL},
      {"on_audio_frame", stub_on_callback, METH_VARARGS, NULL},
      {"on_video_frame", stub_on_callback, METH_VARARGS, NULL},
      {NULL, NULL, 0, NULL},
  };

  static PyTypeObject py_extension_type = {
      PyVarObject_HEAD_INIT(NULL, 0).tp_name =
          "libten_runtime_python._Extension",
      .tp_doc = PyDoc_STR("Extension"),
      .tp_basicsize = sizeof(ten_py_extension_t),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_new = ten_py_extension_create,
      .tp_init = NULL,
      .tp_dealloc = ten_py_extension_destroy,
      .tp_getset = py_extension_type_properties,
      .tp_methods = py_extension_type_methods,
  };

  return &py_extension_type;
}

bool ten_py_extension_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_extension_py_type();
  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception(
        "Python Extension class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_Extension", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  return true;
}
