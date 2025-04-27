//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/ten_env_proxy.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

// There's no need to check for thread-safety, as ten_env_proxy is inherently
// designed to be used in a multi-threaded environment.
bool ten_env_proxy_check_integrity(ten_env_proxy_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PROXY_SIGNATURE) {
    return false;
  }

  if (!self->lock) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  return true;
}

ten_env_proxy_t *ten_env_proxy_create(ten_env_t *ten_env,
                                      size_t initial_thread_cnt,
                                      ten_error_t *err) {
  if (!ten_env) {
    const char *err_msg = "Create ten_env_proxy without specifying ten_env.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return NULL;
  }

  // Checking 1: The platform currently only supports creating a `ten_env_proxy`
  // from the `ten_env` of an extension, an extension_group, and an app.
  switch (ten_env->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
  case TEN_ENV_ATTACH_TO_APP:
    break;

  default: {
    const char *err_msg = "Create ten_env_proxy from unsupported ten.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return NULL;
  }
  }

  // Checking 2: The creation of `ten_env_proxy` must occur within the belonging
  // thread of the corresponding `ten_env`.
  switch (ten_env->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env->attached_target.extension;
    TEN_ASSERT(extension, "Should not happen.");
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");

    ten_extension_thread_t *extension_thread = extension->extension_thread;
    TEN_ASSERT(extension_thread, "Should not happen.");

    if (!ten_extension_thread_call_by_me(extension_thread)) {
      const char *err_msg =
          "ten_env_proxy needs to be created in extension thread.";
      TEN_ASSERT(0, "%s", err_msg);
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
      }
      return NULL;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env->attached_target.extension_group;
    TEN_ASSERT(extension_group &&
                   ten_extension_group_check_integrity(extension_group, true),
               "Should not happen.");

    ten_extension_thread_t *extension_thread =
        extension_group->extension_thread;
    TEN_ASSERT(extension_thread, "Should not happen.");

    if (!ten_extension_thread_call_by_me(extension_thread)) {
      const char *err_msg =
          "ten_env_proxy needs to be created in extension thread.";
      TEN_ASSERT(0, "%s", err_msg);
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
      }
      return NULL;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env->attached_target.app;
    TEN_ASSERT(app, "Should not happen.");

    if (!ten_app_thread_call_by_me(app)) {
      const char *err_msg = "ten_env_proxy needs to be created in app thread.";
      TEN_ASSERT(0, "%s", err_msg);
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
      }
      return NULL;
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  ten_env_proxy_t *self = TEN_MALLOC(sizeof(ten_env_proxy_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_PROXY_SIGNATURE);
  self->lock = ten_mutex_create();
  self->acquired_lock_mode_thread = NULL;
  self->thread_cnt = initial_thread_cnt;
  self->ten_env = ten_env;

  // The created `ten_env_proxy` needs to be recorded, and the corresponding
  // `ten_env` cannot be destroyed as long as any `ten_env_proxy` has not yet
  // been destroyed.
  ten_env_add_ten_proxy(ten_env, self);

  return self;
}

static void ten_env_proxy_destroy(ten_env_proxy_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_proxy_check_integrity(self), "Invalid argument.");

  ten_mutex_destroy(self->lock);
  self->lock = NULL;

  if (self->acquired_lock_mode_thread) {
    TEN_FREE(self->acquired_lock_mode_thread);
    self->acquired_lock_mode_thread = NULL;
  }
  self->ten_env = NULL;

  TEN_FREE(self);
}

bool ten_env_proxy_acquire(ten_env_proxy_t *self, ten_error_t *err) {
  if (!self || !ten_env_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  ten_mutex_lock(self->lock);
  self->thread_cnt++;
  ten_mutex_unlock(self->lock);

  return true;
}

static void ten_notify_proxy_is_deleted(void *self_, TEN_UNUSED void *arg) {
  ten_env_t *self = (ten_env_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);

  ten_env_proxy_t *ten_env_proxy = arg;
  TEN_ASSERT(ten_env_proxy, "Invalid argument.");
  TEN_ASSERT(ten_env_proxy_check_integrity(ten_env_proxy), "Invalid argument.");

  ten_env_delete_ten_proxy(self, ten_env_proxy);
  ten_env_proxy_destroy(ten_env_proxy);
}

bool ten_env_proxy_release(ten_env_proxy_t *self, ten_error_t *err) {
  if (!self || !ten_env_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  bool result = true;

  ten_mutex_lock(self->lock);

  TEN_ASSERT(self->thread_cnt, "Should not happen.");

  if (!self->thread_cnt) {
    const char *err_msg =
        "Unpaired calls of ten_proxy_acquire and ten_proxy_release.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }

    goto done;
  }

  self->thread_cnt--;
  if (!self->thread_cnt) {
    ten_env_t *ten_env = self->ten_env;
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: ten_env_proxy is used in any threads other then the
    // belonging extension thread, so we cannot check thread safety here, and
    // the following post_task is thread safe.
    TEN_ASSERT(ten_env, "Should not happen.");
    TEN_ASSERT(ten_env_check_integrity(ten_env, false), "Should not happen.");

    ten_mutex_unlock(self->lock);

    TEN_LOGI("Post task to ten_env's runloop to notify proxy is deleted.");

    int rc =
        ten_runloop_post_task_tail(ten_env_get_attached_runloop(ten_env),
                                   ten_notify_proxy_is_deleted, ten_env, self);
    if (rc) {
      TEN_LOGW("Failed to post task to ten_env's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }

    return true;
  }

done:
  ten_mutex_unlock(self->lock);
  return result;
}

size_t ten_env_proxy_get_thread_cnt(ten_env_proxy_t *self, ten_error_t *err) {
  if (!self || !ten_env_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return -1;
  }

  ten_mutex_lock(self->lock);
  size_t cnt = self->thread_cnt;
  ten_mutex_unlock(self->lock);

  return cnt;
}
