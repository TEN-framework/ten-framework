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
#include "ten_utils/macro/check.h"

static void ten_py_ten_env_tester_stop_test_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  ten_error_t *test_result = (ten_error_t *)user_data;

  ten_env_tester_stop_test(ten_env_tester, test_result, NULL);

  if (test_result) {
    ten_error_destroy(test_result);
  }
}

PyObject *ten_py_ten_env_tester_stop_test(PyObject *self, PyObject *args) {
  ten_py_ten_env_tester_t *py_ten_env_tester = (ten_py_ten_env_tester_t *)self;
  TEN_ASSERT(py_ten_env_tester &&
                 ten_py_ten_env_tester_check_integrity(py_ten_env_tester),
             "Invalid argument.");

  // Check if ten_env_tester_proxy is valid.
  if (!py_ten_env_tester->c_ten_env_tester_proxy) {
    ten_py_raise_py_value_error_exception(
        "ten_env_tester.stop_test() failed because ten_env_tester_proxy is "
        "invalid.");
  }

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when ten_env_tester.stop_test.");
  }

  // Parse arguments (TenError | None).
  PyObject *py_error = NULL;
  ten_error_t *test_result = NULL;

  if (!PyArg_ParseTuple(args, "O", &py_error)) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument when ten_env_tester.stop_test.");
  }

  if (py_error && !Py_IsNone(py_error)) {
    ten_py_error_t *py_error_obj = (ten_py_error_t *)py_error;
    test_result = ten_error_create();
    ten_error_copy(test_result, &py_error_obj->c_error);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_proxy_notify(
      py_ten_env_tester->c_ten_env_tester_proxy,
      ten_py_ten_env_tester_stop_test_proxy_notify, test_result, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}
