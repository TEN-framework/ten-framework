//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static void ten_env_proxy_notify_on_configure_done(ten_env_t *ten_env,
                                                   TEN_UNUSED void *user_data) {
  TEN_ASSERT(
      ten_env &&
          ten_env_check_integrity(
              ten_env,
              ten_env->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_on_configure_done(ten_env, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

void ten_go_ten_env_on_configure_done(uintptr_t bridge_addr) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {});

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_proxy_notify(self->c_ten_env_proxy,
                                 ten_env_proxy_notify_on_configure_done, NULL,
                                 false, &err);

  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return;
}
