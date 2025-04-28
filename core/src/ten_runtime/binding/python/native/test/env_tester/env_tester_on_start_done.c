//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/binding/python/test/env_tester.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static void ten_py_ten_env_tester_on_start_done_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_env_tester_on_start_done(ten_env_tester, NULL);
}

PyObject *ten_py_ten_env_tester_on_start_done(PyObject *self,
                                              TEN_UNUSED PyObject *args) {
  ten_py_ten_env_tester_t *py_ten_env_tester = (ten_py_ten_env_tester_t *)self;
  TEN_ASSERT(py_ten_env_tester &&
                 ten_py_ten_env_tester_check_integrity(py_ten_env_tester),
             "Invalid argument.");

  if (!py_ten_env_tester->c_ten_env_tester_proxy) {
    return ten_py_raise_py_value_error_exception(
        "ten_env_tester.on_start_done() failed because ten_env_tester_proxy is "
        "invalid.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_proxy_notify(
      py_ten_env_tester->c_ten_env_tester_proxy,
      ten_py_ten_env_tester_on_start_done_proxy_notify, NULL, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
