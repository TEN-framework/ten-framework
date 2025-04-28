//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"

static void ten_go_ten_env_detach_proxy(ten_go_ten_env_t *ten_env_bridge,
                                        ten_error_t *err) {
  TEN_ASSERT(ten_env_bridge && ten_go_ten_env_check_integrity(ten_env_bridge),
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

static void ten_env_proxy_notify_on_deinit_done(ten_env_t *ten_env,
                                                void *user_data) {
  TEN_ASSERT(
      ten_env &&
          ten_env_check_integrity(
              ten_env,
              ten_env->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_go_ten_env_t *ten_env_bridge = user_data;
  TEN_ASSERT(ten_env_bridge, "Should not happen.");

  bool rc = ten_env_on_deinit_done(ten_env, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

/**
 * This function should be called only once.
 */
void ten_go_ten_env_on_deinit_done(uintptr_t bridge_addr) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self),
             "Should not happen.");

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {});

  // Because on_deinit_done() will destroy ten_env_proxy, and when
  // on_deinit_done() is executed, ten_env_proxy must exist (since ten_env_proxy
  // is created during the on_init() process, and calling on_deinit_done()
  // before on_init_done() is not allowed), it's safe here not to check whether
  // ten_env_proxy exists or not. However, one thing to note is that
  // on_deinit_done() can only be called once, and this is a principle that
  // already exists.

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = true;

  if (self->c_ten_env_proxy) {
    rc = ten_env_proxy_notify(self->c_ten_env_proxy,
                              ten_env_proxy_notify_on_deinit_done, self, false,
                              &err);
  } else {
    // TODO(Wei): This function is currently specifically designed for the addon
    // because the addon currently does not have a main thread, so it's unable
    // to use the ten_env_proxy mechanism to maintain thread safety. Once the
    // main thread for the addon is determined in the future, these hacks made
    // specifically for the addon can be completely removed, and comprehensive
    // thread safety mechanism can be implemented.
    TEN_ASSERT(self->c_ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
               "Should not happen.");

    rc = ten_env_on_deinit_done(self->c_ten_env, &err);
  }
  TEN_ASSERT(rc, "Should not happen.");

  ten_go_ten_env_detach_proxy(self, &err);
  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return;
}
