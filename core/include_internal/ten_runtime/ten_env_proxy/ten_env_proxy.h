//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/thread.h"

#define TEN_PROXY_SIGNATURE 0x31C9BF243A8A65BEU

typedef struct ten_env_t ten_env_t;

typedef struct ten_env_proxy_t {
  ten_signature_t signature;

  ten_mutex_t *lock;

  ten_thread_t *acquired_lock_mode_thread;
  size_t thread_cnt;
  ten_env_t *ten_env;
} ten_env_proxy_t;

typedef struct ten_notify_data_t {
  ten_env_proxy_notify_func_t notify_func;
  void *user_data;
} ten_notify_data_t;

TEN_RUNTIME_API bool ten_env_proxy_check_integrity(ten_env_proxy_t *self);

TEN_RUNTIME_API size_t ten_env_proxy_get_thread_cnt(ten_env_proxy_t *self,
                                                    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_env_proxy_acquire(ten_env_proxy_t *self,
                                                   ten_error_t *err);

TEN_RUNTIME_API bool ten_env_proxy_notify_async(
    ten_env_proxy_t *self, ten_env_proxy_notify_func_t notify_func,
    void *user_data, ten_error_t *err);
