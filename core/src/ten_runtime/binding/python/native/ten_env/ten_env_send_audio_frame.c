//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/python/msg/msg.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_send_audio_frame_ctx_t {
  ten_shared_ptr_t *c_audio_frame;
  PyObject *py_cb_func;
} ten_env_notify_send_audio_frame_ctx_t;

static ten_env_notify_send_audio_frame_ctx_t *
ten_env_notify_send_audio_frame_ctx_create(ten_shared_ptr_t *c_audio_frame,
                                           PyObject *py_cb_func) {
  TEN_ASSERT(c_audio_frame, "Invalid argument.");

  ten_env_notify_send_audio_frame_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_send_audio_frame_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_audio_frame = c_audio_frame;
  ctx->py_cb_func = py_cb_func;

  if (py_cb_func != NULL) {
    Py_INCREF(py_cb_func);
  }

  return ctx;
}

static void ten_env_notify_send_audio_frame_ctx_destroy(
    ten_env_notify_send_audio_frame_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_audio_frame) {
    ten_shared_ptr_destroy(ctx->c_audio_frame);
    ctx->c_audio_frame = NULL;
  }

  ctx->py_cb_func = NULL;

  TEN_FREE(ctx);
}

static void proxy_send_audio_frame_callback(
    ten_env_t *ten_env, TEN_UNUSED ten_shared_ptr_t *c_cmd_result,
    void *callback_info, ten_error_t *err) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(callback_info, "Should not happen.");

  // About to call the Python function, so it's necessary to ensure that the GIL
  // has been acquired.
  //
  // Allows C codes to work safely with Python objects.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
  PyObject *cb_func = callback_info;

  PyObject *arglist = NULL;
  ten_py_error_t *py_error = NULL;

  if (err) {
    py_error = ten_py_error_wrap(err);

    arglist = Py_BuildValue("(OO)", py_ten_env->actual_py_ten_env, py_error);
  } else {
    arglist = Py_BuildValue("(OO)", py_ten_env->actual_py_ten_env, Py_None);
  }

  PyObject *result = PyObject_CallObject(cb_func, arglist);
  Py_XDECREF(result);  // Ensure cleanup if an error occurred.

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  Py_XDECREF(arglist);
  Py_XDECREF(cb_func);

  if (py_error) {
    ten_py_error_invalidate(py_error);
  }

  ten_py_gil_state_release_internal(prev_state);
}

static void ten_env_proxy_notify_send_audio_frame(ten_env_t *ten_env,
                                                  void *notify_info_) {
  TEN_ASSERT(notify_info_, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_send_audio_frame_ctx_t *notify_info = notify_info_;
  TEN_ASSERT(notify_info, "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool res = false;
  if (notify_info->py_cb_func == NULL) {
    res = ten_env_send_audio_frame(ten_env, notify_info->c_audio_frame, NULL,
                                   NULL, &err);
  } else {
    res = ten_env_send_audio_frame(ten_env, notify_info->c_audio_frame,
                                   proxy_send_audio_frame_callback,
                                   notify_info->py_cb_func, &err);
    if (!res) {
      // About to call the Python function, so it's necessary to ensure that the
      // GIL has been acquired.
      //
      // Allows C codes to work safely with Python objects.
      PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

      ten_py_ten_env_t *py_ten_env = ten_py_ten_env_wrap(ten_env);
      ten_py_error_t *py_err = ten_py_error_wrap(&err);

      PyObject *arglist =
          Py_BuildValue("(OO)", py_ten_env->actual_py_ten_env, py_err);

      PyObject *result = PyObject_CallObject(notify_info->py_cb_func, arglist);
      Py_XDECREF(result);  // Ensure cleanup if an error occurred.

      bool err_occurred = ten_py_check_and_clear_py_error();
      TEN_ASSERT(!err_occurred, "Should not happen.");

      Py_XDECREF(arglist);
      Py_XDECREF(notify_info->py_cb_func);

      ten_py_error_invalidate(py_err);

      ten_py_gil_state_release_internal(prev_state);
    }
  }

  ten_error_deinit(&err);

  ten_env_notify_send_audio_frame_ctx_destroy(notify_info);
}

PyObject *ten_py_ten_env_send_audio_frame(PyObject *self, PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  ten_py_audio_frame_t *py_audio_frame = NULL;
  PyObject *cb_func = NULL;
  if (!PyArg_ParseTuple(args, "O!O", ten_py_audio_frame_py_type(),
                        &py_audio_frame, &cb_func)) {
    return ten_py_raise_py_type_error_exception(
        "Invalid argument type when send audio_frame.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    ten_error_set(
        &err, TEN_ERROR_CODE_TEN_IS_CLOSED,
        "ten_env.send_audio_frame() failed because the TEN is closed.");
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  }

  // Check if cb_func is callable.
  if (!PyCallable_Check(cb_func)) {
    cb_func = NULL;
  }

  ten_shared_ptr_t *cloned_audio_frame =
      ten_shared_ptr_clone(py_audio_frame->msg.c_msg);
  ten_env_notify_send_audio_frame_ctx_t *notify_info =
      ten_env_notify_send_audio_frame_ctx_create(cloned_audio_frame, cb_func);

  if (!ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                            ten_env_proxy_notify_send_audio_frame, notify_info,
                            false, &err)) {
    if (cb_func) {
      Py_XDECREF(cb_func);
    }

    ten_env_notify_send_audio_frame_ctx_destroy(notify_info);

    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    return result;
  } else {
    // Destroy the C message from the Python message as the ownership has been
    // transferred to the notify_info.
    ten_py_msg_destroy_c_msg(&py_audio_frame->msg);
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
