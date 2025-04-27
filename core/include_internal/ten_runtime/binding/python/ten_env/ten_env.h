//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/ten_env/ten_env.h"

#define TEN_PY_TEN_ENV_SIGNATURE 0xCCCC1DD4BB4CA743U

typedef struct ten_py_ten_env_t {
  PyObject_HEAD

  ten_signature_t signature;

  // @{
  // Above the binding layer, `c_ten_env_proxy` is used to interact with the C
  // runtime. However, since addon does not have the concept of a main thread,
  // it does not have the concept of `c_ten_env_proxy`. Therefore, only the
  // addon path uses `c_ten_env`, while all other TEN types use
  // `c_ten_env_proxy` for the associated `ten_env` concept.
  ten_env_t *c_ten_env;
  ten_env_proxy_t *c_ten_env_proxy;
  // @}

  PyObject *actual_py_ten_env;

  // Mark whether the gil state need to be released after 'on_deinit_done'.
  bool need_to_release_gil_state;
  PyThreadState *py_thread_state;
} ten_py_ten_env_t;

TEN_RUNTIME_PRIVATE_API bool ten_py_ten_env_check_integrity(
    ten_py_ten_env_t *self);

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_ten_env_type(void);

TEN_RUNTIME_PRIVATE_API ten_py_ten_env_t *ten_py_ten_env_wrap(
    ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API void ten_py_ten_env_invalidate(
    ten_py_ten_env_t *py_ten);

TEN_RUNTIME_PRIVATE_API bool ten_py_ten_env_init_for_module(PyObject *module);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_on_configure_done(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_on_init_done(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_on_start_done(PyObject *self,
                                                               PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_on_stop_done(PyObject *self,
                                                              PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_on_deinit_done(PyObject *self,
                                                                PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_on_create_instance_done(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_send_cmd(PyObject *self,
                                                          PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_send_data(PyObject *self,
                                                           PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_send_video_frame(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_send_audio_frame(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_return_result(PyObject *self,
                                                               PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_to_json(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_from_json(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_is_property_exist(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_int(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_int(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_string(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_string(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_bool(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_bool(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_float(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_float(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_init_property_from_json(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_to_json_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_from_json_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_is_property_exist_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_int_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_int_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_string_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_string_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_bool_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_bool_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_get_property_float_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_set_property_float_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_init_property_from_json_async(
    PyObject *self, PyObject *args);

TEN_RUNTIME_PRIVATE_API PyObject *ten_py_ten_env_log(PyObject *self,
                                                     PyObject *args);
