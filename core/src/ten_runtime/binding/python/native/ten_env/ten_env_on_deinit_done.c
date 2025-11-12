//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common.h"
#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "pystate.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static void ten_env_proxy_notify_on_deinit_done(ten_env_t *ten_env,
                                                void *user_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_py_ten_env_t *py_ten_env = user_data;
  TEN_ASSERT(py_ten_env, "Should not happen.");

  bool rc = ten_env_on_deinit_done(ten_env, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  // Notify the Python side to do the cleanup.
  //
  // About to call the Python function, so it's necessary to ensure that the
  // GIL has been acquired.
  PyGILState_STATE prev_state = ten_py_gil_state_ensure_internal();

  PyObject *py_res =
      PyObject_CallMethod(py_ten_env->actual_py_ten_env, "_on_release", NULL);
  Py_XDECREF(py_res);

  bool err_occurred = ten_py_check_and_clear_py_error();
  TEN_ASSERT(!err_occurred, "Should not happen.");

  ten_py_gil_state_release_internal(prev_state);

  if (py_ten_env->need_to_release_gil_state) {
    if (!ten_py_is_holding_gil()) {
      TEN_ASSERT(py_ten_env->py_thread_state != NULL, "Should not happen.");

      // The gil is not held by the current thread, so we have to acquire the
      // gil before we can release the gil state.
      ten_py_eval_restore_thread(py_ten_env->py_thread_state);

      // Release the gil state and the gil.
      ten_py_gil_state_release_internal(PyGILState_UNLOCKED);
    } else {
      // Release the gil state but keep holding the gil.
      ten_py_gil_state_release_internal(PyGILState_LOCKED);
    }
  }
}

static void ten_py_ten_env_detach_proxy(ten_py_ten_env_t *ten_env_bridge,
                                        ten_error_t *err) {
  TEN_ASSERT(ten_env_bridge && ten_py_ten_env_check_integrity(ten_env_bridge),
             "Should not happen.");

  ten_env_proxy_t *c_ten_env_proxy = ten_env_bridge->c_ten_env_proxy;
  if (c_ten_env_proxy) {
    TEN_ASSERT(ten_env_proxy_get_thread_cnt(c_ten_env_proxy, err) == 1,
               "Should not happen.");

    bool rc = ten_env_proxy_release(c_ten_env_proxy, err);
    TEN_ASSERT(rc, "Should not happen.");
  }

  ten_env_bridge->c_ten_env = NULL;
  ten_env_bridge->c_ten_env_proxy = NULL;
}

PyObject *ten_py_ten_env_on_deinit_done(PyObject *self,
                                        TEN_UNUSED PyObject *args) {
  ten_py_ten_env_t *py_ten_env = (ten_py_ten_env_t *)self;
  TEN_ASSERT(py_ten_env && ten_py_ten_env_check_integrity(py_ten_env),
             "Invalid argument.");

  if (!py_ten_env->c_ten_env_proxy && !py_ten_env->c_ten_env) {
    return ten_py_raise_py_value_error_exception(
        "ten_env.on_deinit_done() failed because ten_env(_proxy) is invalid.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (py_ten_env->c_ten_env_proxy) {
    bool rc = ten_env_proxy_notify(py_ten_env->c_ten_env_proxy,
                                   ten_env_proxy_notify_on_deinit_done,
                                   py_ten_env, false, &err);
    TEN_ASSERT(rc, "Failed to notify on deinit done.");
  } else {
    TEN_ASSERT(0, "c_ten_env_proxy is NULL.");
  }

  // This is already the very end, so releasing `ten_env_proxy` here is
  // appropriate. Additionally, since this function is called from Python so
  // the Python GIL is held, it ensures that no other Python code is currently
  // using `ten_env`.
  ten_py_ten_env_detach_proxy(py_ten_env, &err);

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
