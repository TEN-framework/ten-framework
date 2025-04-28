//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/app.h"
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
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

static ten_notify_data_t *ten_notify_data_create(
    ten_env_proxy_notify_func_t notify_func, void *user_data) {
  TEN_ASSERT(notify_func, "Invalid argument.");

  ten_notify_data_t *self =
      (ten_notify_data_t *)TEN_MALLOC(sizeof(ten_notify_data_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->notify_func = notify_func;
  self->user_data = user_data;

  return self;
}

static void ten_notify_data_destroy(ten_notify_data_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_FREE(self);
}

static void ten_notify_to_app_task(void *self_, void *arg) {
  ten_app_t *app = self_;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_notify_data_t *notify_data = (ten_notify_data_t *)arg;
  TEN_ASSERT(notify_data, "Invalid argument.");

  notify_data->notify_func(app->ten_env, notify_data->user_data);

  ten_notify_data_destroy(notify_data);
}

static void ten_notify_to_extension_task(void *self_, void *arg) {
  ten_extension_t *extension = self_;
  TEN_ASSERT(extension, "Invalid argument.");

  ten_notify_data_t *notify_data = (ten_notify_data_t *)arg;
  TEN_ASSERT(notify_data, "Invalid argument.");

  ten_extension_thread_t *extension_thread = extension->extension_thread;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  notify_data->notify_func(extension->ten_env, notify_data->user_data);

  ten_notify_data_destroy(notify_data);
}

static void ten_notify_to_extension_group_task(void *self_, void *arg) {
  ten_extension_group_t *extension_group = self_;
  TEN_ASSERT(extension_group, "Invalid argument.");

  ten_notify_data_t *notify_data = (ten_notify_data_t *)arg;
  TEN_ASSERT(notify_data, "Invalid argument.");

  ten_extension_thread_t *extension_thread = extension_group->extension_thread;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  notify_data->notify_func(extension_group->ten_env, notify_data->user_data);

  ten_notify_data_destroy(notify_data);
}

bool ten_env_proxy_notify(ten_env_proxy_t *self,
                          ten_env_proxy_notify_func_t notify_func,
                          void *user_data, bool sync, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(notify_func, "Invalid argument.");

  if (!self || !notify_func || !ten_env_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  bool result = true;

  ten_env_t *ten_env = self->ten_env;
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, false),
             "Should not happen.");

  switch (ten_env->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env->attached_target.extension;
    TEN_ASSERT(extension, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads,
    // and the use of extension instance is thread safe here.
    TEN_ASSERT(ten_extension_check_integrity(extension, false),
               "Invalid argument.");

    ten_extension_thread_t *extension_thread = extension->extension_thread;
    TEN_ASSERT(extension_thread, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads,
    // and the use of extension instance is thread safe here.
    TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, false),
               "Invalid argument.");

    if (ten_extension_thread_call_by_me(extension_thread)) {
      notify_func(self->ten_env, user_data);
    } else {
      if (sync) {
        ten_mutex_lock(self->lock);

        if (self->acquired_lock_mode_thread &&
            ten_thread_equal_to_current_thread(
                self->acquired_lock_mode_thread)) {
          // The current outer thread has obtained the power of lock mode, and
          // therefore can perform sync operations.
          notify_func(ten_env, user_data);
        } else {
          if (err) {
            ten_error_set(
                err, TEN_ERROR_CODE_GENERIC,
                "Perform synchronous ten_notify without acquiring lock_mode "
                "first.");
          }
          result = false;
        }

        ten_mutex_unlock(self->lock);
      } else {
        int rc = ten_runloop_post_task_tail(
            ten_extension_get_attached_runloop(extension),
            ten_notify_to_extension_task, extension,
            ten_notify_data_create(notify_func, user_data));
        if (rc) {
          TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
        }

        result = rc == 0;
      }
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env->attached_target.extension_group;
    TEN_ASSERT(extension_group, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads,
    // and the use of extension instance is thread safe here.
    TEN_ASSERT(ten_extension_group_check_integrity(extension_group, false),
               "Invalid argument.");

    ten_extension_thread_t *extension_thread =
        extension_group->extension_thread;
    TEN_ASSERT(extension_thread, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads,
    // and the use of extension instance is thread safe here.
    TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, false),
               "Invalid argument.");

    if (ten_extension_thread_call_by_me(extension_thread)) {
      notify_func(self->ten_env, user_data);
    } else {
      TEN_ASSERT(sync == false, "Unsupported operation.");

      int rc = ten_runloop_post_task_tail(
          ten_extension_group_get_attached_runloop(extension_group),
          ten_notify_to_extension_group_task, extension_group,
          ten_notify_data_create(notify_func, user_data));
      if (rc) {
        TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
      }

      result = rc == 0;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env->attached_target.app;
    TEN_ASSERT(app, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads.
    TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid argument.");

    if (ten_app_thread_call_by_me(app)) {
      notify_func(self->ten_env, user_data);
    } else {
      TEN_ASSERT(sync == false, "Unsupported operation.");

      int rc = ten_runloop_post_task_tail(
          ten_app_get_attached_runloop(app), ten_notify_to_app_task, app,
          ten_notify_data_create(notify_func, user_data));
      if (rc) {
        TEN_LOGW("Failed to post task to app's runloop: %d", rc);
      }

      result = rc == 0;
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Handle more types: %d", ten_env->attach_to);
    break;
  }

  TEN_ASSERT(result, "Should not happen.");
  return result;
}

bool ten_env_proxy_notify_async(ten_env_proxy_t *self,
                                ten_env_proxy_notify_func_t notify_func,
                                void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(notify_func, "Invalid argument.");

  if (!self || !notify_func || !ten_env_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  bool result = true;

  ten_env_t *ten_env = self->ten_env;
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, false),
             "Should not happen.");

  switch (ten_env->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env->attached_target.extension;
    TEN_ASSERT(extension, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads,
    // and the use of extension instance is thread safe here.
    TEN_ASSERT(ten_extension_check_integrity(extension, false),
               "Invalid argument.");

    int rc = ten_runloop_post_task_tail(
        ten_extension_get_attached_runloop(extension),
        ten_notify_to_extension_task, extension,
        ten_notify_data_create(notify_func, user_data));
    if (rc) {
      TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env->attached_target.extension_group;
    TEN_ASSERT(extension_group, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads,
    // and the use of extension instance is thread safe here.
    TEN_ASSERT(ten_extension_group_check_integrity(extension_group, false),
               "Invalid argument.");

    int rc = ten_runloop_post_task_tail(
        ten_extension_group_get_attached_runloop(extension_group),
        ten_notify_to_extension_group_task, extension_group,
        ten_notify_data_create(notify_func, user_data));
    if (rc) {
      TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env->attached_target.app;
    TEN_ASSERT(app, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is intended to be called in any threads.
    TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid argument.");

    int rc = ten_runloop_post_task_tail(
        ten_app_get_attached_runloop(app), ten_notify_to_app_task, app,
        ten_notify_data_create(notify_func, user_data));
    if (rc) {
      TEN_LOGW("Failed to post task to app's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Handle more types: %d", ten_env->attach_to);
    break;
  }

  TEN_ASSERT(result, "Should not happen.");
  return result;
}
