//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/ten_env_proxy.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_env_proxy_release_lock_mode(ten_env_proxy_t *self, ten_error_t *err) {
  // The outer thread must ensure the validity of the ten_env_proxy instance.
  if (!self) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }
  if (!ten_env_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  bool result = true;

  ten_env_t *ten_env = self->ten_env;
  TEN_ASSERT(ten_env, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads other
  // then the belonging extension thread, and within this function, we only
  // utilize the immutable fields of ten or ten fields protected by locks.
  TEN_ASSERT(ten_env_check_integrity(ten_env, false), "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_EXTENSION,
             "Invalid argument.");

  // If any ten_env_proxy instance exists, then the TEN world will not
  // disappear, and therefore things related to the extension world, such as
  // extension and extension thread, will still exist and will not change.
  // Therefore, it is safe to access extension and extension_thread below.
  ten_extension_thread_t *extension_thread =
      ten_env->attached_target.extension->extension_thread;
  TEN_ASSERT(extension_thread, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads other
  // then the belonging extension thread, and within this function, we only
  // utilize the immutable fields of ten or ten fields protected by locks.
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, false),
             "Should not happen.");

  ten_mutex_lock(self->lock);

  if (self->acquired_lock_mode_thread &&
      ten_thread_equal_to_current_thread(self->acquired_lock_mode_thread)) {
    // It is the current outer thread that has acquired the lock mode lock,
    // therefore release it.
    int rc = ten_mutex_unlock(extension_thread->lock_mode_lock);
    TEN_ASSERT(!rc, "Should not happen.");

    TEN_FREE(self->acquired_lock_mode_thread);
    self->acquired_lock_mode_thread = NULL;
  }

  ten_mutex_unlock(self->lock);

  TEN_ASSERT(result, "Should not happen.");
  return result;
}
