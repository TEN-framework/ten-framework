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
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"

bool ten_env_proxy_acquire_lock_mode(ten_env_proxy_t *self, ten_error_t *err) {
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

  ten_extension_t *extension = ten_env->attached_target.extension;
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, false),
             "Should not happen.");

  ten_extension_thread_t *extension_thread = extension->extension_thread;
  TEN_ASSERT(extension_thread, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads other
  // then the belonging extension thread, and within this function, we only
  // utilize the immutable fields of ten or ten fields protected by locks.
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, false),
             "Should not happen.");

  // Competing for the lock_mode_lock so that only _1_ outer thread can proceed,
  // if the competition is successful, it does _not_ mean that the extension
  // thread has been blocked. The extension thread may still be running, so some
  // judgment is needed to confirm that the extension thread has been blocked.
  // Only then can the outer thread safely use the things in the TEN world
  // directly.
  int rc = ten_mutex_lock(extension_thread->lock_mode_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Should not happen.");

  TEN_ASSERT(!self->acquired_lock_mode_thread, "Should not happen.");
  self->acquired_lock_mode_thread = ten_thread_create_fake(NULL);

  if (extension_thread->in_lock_mode == false) {
    // This means that the extension thread may still be running, so there needs
    // to be some way to wait for the extension thread to be blocked. Only after
    // confirming that the extension thread has been blocked can the outer
    // thread safely directly use the TEN world.
    //
    // The method is to insert a special task into the extension thread's
    // runloop. When the extension thread runloop executes that task, the outer
    // thread can safely continue and directly use the TEN world.

    ten_acquire_lock_mode_result_t *suspend_result =
        TEN_MALLOC(sizeof(ten_acquire_lock_mode_result_t));
    TEN_ASSERT(suspend_result, "Failed to allocate memory.");

    ten_error_init(&suspend_result->err);
    suspend_result->completed = ten_event_create(0, 1);

    ten_runloop_t *extension_thread_runloop =
        ten_extension_thread_get_attached_runloop(extension_thread);
    TEN_ASSERT(extension_thread_runloop, "Should not happen.");
    TEN_ASSERT(ten_runloop_check_integrity(extension_thread_runloop, false),
               "Should not happen.");

    int rc = ten_runloop_post_task_front(
        extension_thread_runloop,
        ten_extension_thread_process_acquire_lock_mode_task, extension_thread,
        suspend_result);
    if (rc) {
      TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }

    // Wait for the extension thread to be suspended successfully.
    rc = ten_event_wait(suspend_result->completed, -1);
    if (rc) {
      TEN_LOGW("Failed to wait for the extension thread to be suspended: %d",
               rc);
      TEN_ASSERT(0, "Should not happen.");
    }

    ten_event_destroy(suspend_result->completed);

    if (ten_error_code(&suspend_result->err) > 0) {
      TEN_ASSERT(0, "Should not happen.");

      result = false;

      rc = ten_mutex_unlock(extension_thread->lock_mode_lock);
      if (rc) {
        TEN_LOGW("Failed to unlock the lock mode lock: %d", rc);
        TEN_ASSERT(0, "Should not happen.");
      }
    }

    TEN_ASSERT(extension_thread->in_lock_mode, "Should not happen.");

    TEN_FREE(suspend_result);
  }

  // Do _not_ _unlock_ the lock mode lock here, because this is the way we used
  // to block the extension thread.

  ten_mutex_unlock(self->lock);

  TEN_ASSERT(result, "Should not happen.");
  return result;
}
