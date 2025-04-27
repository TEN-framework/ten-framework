//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "include_internal/ten_runtime/test/env_tester.h"
#include "include_internal/ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/lib/signature.h"

#define TEN_PY_TEN_ENV_TESTER_SIGNATURE 0x9DF807EAFAF9F6D5U

typedef struct ten_py_ten_env_tester_t {
  PyObject_HEAD

  ten_signature_t signature;

  ten_env_tester_t *c_ten_env_tester;
  ten_env_tester_proxy_t *c_ten_env_tester_proxy;

  PyObject *actual_py_ten_env_tester;
} ten_py_ten_env_tester_t;

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_ten_env_tester_py_type(void);

TEN_RUNTIME_PRIVATE_API bool ten_py_ten_env_tester_init_for_module(
    PyObject *module);

TEN_RUNTIME_PRIVATE_API ten_py_ten_env_tester_t *ten_py_ten_env_tester_wrap(
    ten_env_tester_t *ten_env_tester);

TEN_RUNTIME_PRIVATE_API void ten_py_ten_env_tester_invalidate(
    ten_py_ten_env_tester_t *py_ten);

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_ten_env_tester_type(void);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_on_start_done(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_on_stop_done(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_on_deinit_done(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_stop_test(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_send_cmd(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_send_data(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_send_audio_frame(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_send_video_frame(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_return_result(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_tester_log(PyObject *self,
                                                            PyObject *args);

TEN_RUNTIME_PRIVATE_API bool ten_py_ten_env_tester_check_integrity(
    ten_py_ten_env_tester_t *self);
