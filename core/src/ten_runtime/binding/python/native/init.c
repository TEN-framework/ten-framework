//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/addon/addon.h"
#include "include_internal/ten_runtime/binding/python/addon/addon_manager.h"
#include "include_internal/ten_runtime/binding/python/app/app.h"
#include "include_internal/ten_runtime/binding/python/common/buf.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/extension/extension.h"
#include "include_internal/ten_runtime/binding/python/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd.h"
#include "include_internal/ten_runtime/binding/python/msg/cmd_result.h"
#include "include_internal/ten_runtime/binding/python/msg/data.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/binding/python/msg/video_frame.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/python/test/env_tester.h"
#include "include_internal/ten_runtime/binding/python/test/extension_tester.h"

// Note on memory leaks of Python VM.
//
// The Python VM seems to consistently have memory leaks. In the official Python
// test suite, the ability to detect leaks is even turned off.
// https://github.com/python/buildmaster-config/blob/main/master/custom/factories.py#L240.
//
// Additionally, in Atheris: A Coverage-Guided, Native Python Fuzzer
// (https://github.com/google/atheris/blob/master/native_extension_fuzzing.md#leak-detection),
// it is also recommended to disable the leak detection when using Python.
//
// Therefore, we can only enable LSan in some simple test cases. Beyond that, we
// largely need to rely on the TEN's own memory tracking feature to ensure that
// the TEN itself has no memory leaks. This feature in the TEN can be seen in
// the TEN Memory Sanitizer.

static PyModuleDef *ten_py_runtime_module(void) {
  static struct PyMethodDef module_methods[] = {
      {"_register_addon_as_extension",
       ten_py_addon_manager_register_addon_as_extension, METH_VARARGS,
       "Register an addon as an extension"},
      {"_add_extension_addon_to_addon_manager",
       ten_py_addon_manager_add_extension_addon, METH_VARARGS,
       "Add an addon to the addon manager"},
      {NULL, NULL, 0, NULL}};

  static struct PyModuleDef module_def = {
      PyModuleDef_HEAD_INIT,
      "libten_runtime_python",
      NULL,
      -1,
      module_methods,
      NULL,
      0,
      0,
      0,
  };

  return &module_def;
}

PyMODINIT_FUNC PyInit_libten_runtime_python(void) {
  PyObject *module = PyModule_Create(ten_py_runtime_module());
  if (!module) {
    return ten_py_raise_py_system_error_exception(
        "Failed create ten_runtime_python module.");
  }

  if (!ten_py_addon_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_app_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_ten_env_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_extension_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_msg_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_cmd_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_cmd_result_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_data_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_video_frame_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_audio_frame_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_buf_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_extension_tester_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_ten_env_tester_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  if (!ten_py_error_init_for_module(module)) {
    Py_DECREF(module);
    return NULL;
  }

  return module;
}
