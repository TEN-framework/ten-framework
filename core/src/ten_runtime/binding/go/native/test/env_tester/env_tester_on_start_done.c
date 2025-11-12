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

static void ten_go_ten_env_tester_on_start_done_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  ten_go_ten_env_tester_t *self = user_data;
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_env_tester_on_start_done(ten_env_tester, NULL);
}

ten_go_error_t ten_go_ten_env_tester_on_start_done(uintptr_t bridge_addr) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  if (!self->c_ten_env_tester_proxy) {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_GENERIC);
    return cgo_error;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_proxy_notify(
      self->c_ten_env_tester_proxy,
      ten_go_ten_env_tester_on_start_done_proxy_notify, self, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  return cgo_error;
}
