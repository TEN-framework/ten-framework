//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/ten.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/rwlock.h"

// Since there is no runloop attached to ten_addon_t, we cannot create a
// ten_env_proxy for ten_addon_t. Therefore, the way to determine the closure of
// the ten_env attached to an addon is to check if the ten_env pointer is null.
// For other types of ten_env, after calling on_deinit_done, the
// ten_env_proxy pointer will be set to null. After this, all ten_env APIs
// should not be able to succeed, and the method is to check if the
// ten_env_proxy pointer is null.
#define TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(ten_env_bridge, err_stmt) \
  do {                                                                 \
    ten_rwlock_lock((ten_env_bridge)->lock, 1);                        \
    if (((ten_env_bridge)->c_ten_env == NULL) &&                       \
        ((ten_env_bridge)->c_ten_env_proxy == NULL)) {                 \
      ten_rwlock_unlock((ten_env_bridge)->lock, 1);                    \
      {                                                                \
        err_stmt                                                       \
      }                                                                \
      goto ten_is_close;                                               \
    }                                                                  \
  } while (0)

#define TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(ten_env_bridge) \
  do {                                                     \
    ten_rwlock_unlock((ten_env_bridge)->lock, 1);          \
  } while (0)

typedef struct ten_go_ten_env_t {
  ten_signature_t signature;

  ten_go_bridge_t bridge;

  // @{
  // Above the binding layer, `c_ten_env_proxy` is used to interact with the C
  // runtime. However, since addon does not have the concept of a main thread,
  // it does not have the concept of `c_ten_env_proxy`. Therefore, only the
  // addon path uses `c_ten_env`, while all other TEN types use
  // `c_ten_env_proxy` for the associated `ten_env` concept.
  ten_env_t *c_ten_env;
  ten_env_proxy_t *c_ten_env_proxy;
  // @}

  ten_rwlock_t *lock;
} ten_go_ten_env_t;

typedef struct ten_go_callback_ctx_t {
  ten_go_handle_t callback_id;
} ten_go_callback_ctx_t;

extern void tenGoOnCmdResult(ten_go_handle_t ten_env_bridge,
                             ten_go_handle_t cmd_result_bridge,
                             ten_go_handle_t result_handler,
                             bool is_completed,
                             ten_go_error_t cgo_error);

extern void tenGoOnError(ten_go_handle_t ten_env_bridge,
                         ten_go_handle_t error_handler,
                         ten_go_error_t cgo_error);

TEN_RUNTIME_PRIVATE_API ten_go_callback_ctx_t *ten_go_callback_ctx_create(
    ten_go_handle_t handler_id);

TEN_RUNTIME_PRIVATE_API void ten_go_callback_ctx_destroy(
    ten_go_callback_ctx_t *self);

ten_go_handle_t tenGoCreateTenEnv(uintptr_t);

void tenGoDestroyTenEnv(ten_go_handle_t go_ten_env);

void tenGoSetPropertyCallback(ten_go_handle_t ten_env, ten_go_handle_t handler,
                              bool result);

void tenGoGetPropertyCallback(ten_go_handle_t ten_env, ten_go_handle_t handler,
                              ten_go_handle_t value);
