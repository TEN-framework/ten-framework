//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/test/env_tester.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/lib/string.h"

static void ten_go_ten_env_tester_stop_test_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  ten_error_t *test_result = (ten_error_t *)user_data;

  ten_env_tester_stop_test(ten_env_tester, test_result, NULL);

  if (test_result) {
    ten_error_destroy(test_result);
  }
}

ten_go_error_t ten_go_ten_env_tester_stop_test(uintptr_t bridge_addr,
                                               uint32_t error_code,
                                               void *error_message,
                                               uint32_t error_message_size) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  if (!self->c_ten_env_tester_proxy) {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
    return cgo_error;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_error_t *test_result = NULL;

  if (error_code != TEN_ERROR_CODE_OK) {
    test_result = ten_error_create();

    ten_error_set_error_code(test_result, error_code);
    if (error_message && error_message_size > 0) {
      ten_string_t error_message_str;
      ten_string_init_from_c_str_with_size(&error_message_str, error_message,
                                           error_message_size);
      ten_error_set_error_message(test_result,
                                  ten_string_get_raw_str(&error_message_str));
      ten_string_deinit(&error_message_str);
    }
  }

  bool rc = ten_env_tester_proxy_notify(
      self->c_ten_env_tester_proxy,
      ten_go_ten_env_tester_stop_test_proxy_notify, test_result, &err);

  if (!rc) {
    ten_go_error_set_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  return cgo_error;
}
