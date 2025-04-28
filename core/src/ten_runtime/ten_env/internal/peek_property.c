//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/ten_env/metadata.h"
#include "include_internal/ten_runtime/common/error_code.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/metadata.h"
#include "include_internal/ten_runtime/extension/ten_env/metadata.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/ten_env/metadata.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/ten_env/metadata.h"
#include "include_internal/ten_runtime/ten_env/metadata_cb.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static ten_env_peek_property_sync_context_t *
ten_env_peek_property_sync_context_create(void) {
  ten_env_peek_property_sync_context_t *context =
      TEN_MALLOC(sizeof(ten_env_peek_property_sync_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  context->res = NULL;
  context->completed = ten_event_create(0, 0);

  return context;
}

static void ten_env_peek_property_sync_context_destroy(
    ten_env_peek_property_sync_context_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_event_destroy(self->completed);
  TEN_FREE(self);
}

static void ten_app_peek_property_sync_cb(ten_app_t *app, ten_value_t *res,
                                          void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_env_peek_property_sync_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  context->res = res;
  ten_event_set(context->completed);
}

static ten_env_peek_property_async_context_t *
ten_env_peek_property_async_context_create(ten_env_t *ten_env,
                                           ten_env_peek_property_async_cb_t cb,
                                           void *cb_data) {
  ten_env_peek_property_async_context_t *context =
      TEN_MALLOC(sizeof(ten_env_peek_property_async_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  context->ten_env = ten_env;
  context->cb = cb;
  context->cb_data = cb_data;

  return context;
}

static void ten_env_peek_property_async_context_destroy(
    ten_env_peek_property_async_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_FREE(self);
}

static void ten_extension_peek_property_async_cb(ten_extension_t *extension,
                                                 ten_value_t *res,
                                                 void *cb_data,
                                                 ten_error_t *err) {
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_env_peek_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  if (context->cb) {
    context->cb(context->ten_env, res, context->cb_data, err);
  }

  ten_env_peek_property_async_context_destroy(context);
}

static void ten_extension_group_peek_property_async_cb(
    ten_extension_group_t *extension_group, ten_value_t *res, void *cb_data) {
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_env_peek_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  if (context->cb) {
    context->cb(context->ten_env, res, context->cb_data, NULL);
  }

  ten_env_peek_property_async_context_destroy(context);
}

static void ten_env_peek_property_done_task(TEN_UNUSED void *from, void *arg) {
  ten_env_peek_property_async_context_t *context = arg;
  TEN_ASSERT(context, "Should not happen.");
  TEN_ASSERT(context->from.extension != NULL, "Invalid argument.");

  if (context->cb) {
    context->cb(context->ten_env, context->res, context->cb_data, NULL);
  }

  ten_env_peek_property_async_context_destroy(context);
}

static void ten_app_peek_property_async_cb_go_back_to_extension(
    ten_app_t *app, ten_value_t *res, void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_env_peek_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");
  TEN_ASSERT(context->from.extension != NULL, "Invalid argument.");

  context->res = res;

  int rc = ten_runloop_post_task_tail(
      ten_extension_get_attached_runloop(context->from.extension),
      ten_env_peek_property_done_task, NULL, context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void ten_app_peek_property_async_cb_go_back_to_extension_group(
    ten_app_t *app, ten_value_t *res, void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_env_peek_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");
  TEN_ASSERT(context->from.extension_group != NULL, "Invalid argument.");

  context->res = res;

  int rc = ten_runloop_post_task_tail(
      ten_extension_group_get_attached_runloop(context->from.extension_group),
      ten_env_peek_property_done_task, NULL, context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void ten_app_peek_property_async_cb(ten_app_t *app, ten_value_t *res,
                                           void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_env_peek_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  if (context->cb) {
    context->cb(context->ten_env, res, context->cb_data, NULL);
  }

  ten_env_peek_property_async_context_destroy(context);
}

ten_value_t *ten_env_peek_property(ten_env_t *self, const char *path,
                                   ten_error_t *err) {
  TEN_ASSERT(self && ten_env_check_integrity(self, true),
             "Invalid use of ten_env %p.", self);

  if (ten_env_is_closed(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    }
    return NULL;
  }

  ten_value_t *res = NULL;
  const char **p_path = &path;

  TEN_METADATA_LEVEL level =
      ten_determine_metadata_level(self->attach_to, p_path);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid use of extension %p.", extension);

    ten_extension_thread_t *extension_thread = extension->extension_thread;
    TEN_ASSERT(extension_thread &&
                   ten_extension_thread_check_integrity(extension_thread, true),
               "Invalid use of extension_thread %p.", extension_thread);

    switch (level) {
    case TEN_METADATA_LEVEL_EXTENSION:
      res = ten_extension_peek_property(extension, *p_path, err);
      break;

    case TEN_METADATA_LEVEL_EXTENSION_GROUP: {
      ten_extension_group_t *extension_group =
          extension->extension_thread->extension_group;
      TEN_ASSERT(extension_group &&
                     ten_extension_group_check_integrity(extension_group, true),
                 "Invalid use of extension group %p", extension_group);

      res = ten_extension_group_peek_property(extension_group, path);
      break;
    }

    case TEN_METADATA_LEVEL_APP: {
      ten_app_t *app = extension->extension_context->engine->app;
      // TEN_NOLINTNEXTLINE(thread-check):
      // thread-check: Access the app's property from an extension, that is,
      // from the extension thread.
      TEN_ASSERT(app && ten_app_check_integrity(app, false),
                 "Invalid use of app %p", app);

      if (ten_app_thread_call_by_me(app)) {
        res = ten_app_peek_property(app, path);
      } else {
        ten_env_peek_property_sync_context_t *context =
            ten_env_peek_property_sync_context_create();
        TEN_ASSERT(context, "Should not happen.");

        ten_app_peek_property_async(app, path, ten_app_peek_property_sync_cb,
                                    context);

        ten_event_wait(context->completed, -1);
        res = context->res;

        ten_env_peek_property_sync_context_destroy(context);
      }
      break;
    }

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    TEN_ASSERT(extension_group &&
                   ten_extension_group_check_integrity(extension_group, true),
               "Invalid use of extension_group %p.", extension_group);

    ten_extension_thread_t *extension_thread =
        extension_group->extension_thread;
    TEN_ASSERT(extension_thread &&
                   ten_extension_thread_check_integrity(extension_thread, true),
               "Invalid use of extension_thread %p.", extension_thread);

    switch (level) {
    case TEN_METADATA_LEVEL_EXTENSION_GROUP:
      res = ten_extension_group_peek_property(extension_group, path);
      break;

    case TEN_METADATA_LEVEL_APP: {
      ten_app_t *app = extension_group->extension_context->engine->app;
      // TEN_NOLINTNEXTLINE(thread-check):
      // thread-check: Access the app's property from an extension group,
      // that is, from the extension thread.
      TEN_ASSERT(app && ten_app_check_integrity(app, false),
                 "Invalid use of app %p", app);

      if (ten_app_thread_call_by_me(app)) {
        res = ten_app_peek_property(app, path);
      } else {
        ten_env_peek_property_sync_context_t *context =
            ten_env_peek_property_sync_context_create();
        TEN_ASSERT(context, "Should not happen.");

        ten_app_peek_property_async(app, path, ten_app_peek_property_sync_cb,
                                    context);

        ten_event_wait(context->completed, -1);
        res = context->res;

        ten_env_peek_property_sync_context_destroy(context);
      }
      break;
    }

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env_get_attached_app(self);
    TEN_ASSERT(app && ten_app_check_integrity(app, true),
               "Invalid use of app %p.", app);

    switch (level) {
    case TEN_METADATA_LEVEL_APP: {
      res = ten_app_peek_property(app, path);
      break;
    }

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  if (!res) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_VALUE_NOT_FOUND,
                    "Failed to find value: %s", path);
    }
  }

  return res;
}

bool ten_env_peek_property_async(ten_env_t *self, const char *path,
                                 ten_env_peek_property_async_cb_t cb,
                                 void *cb_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(path && strlen(path), "path should not be empty.");

  const char **p_path = &path;

  ten_env_peek_property_async_context_t *context =
      ten_env_peek_property_async_context_create(self, cb, cb_data);
  TEN_ASSERT(context, "Should not happen.");

  TEN_METADATA_LEVEL level =
      ten_determine_metadata_level(self->attach_to, p_path);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid use of extension %p.", extension);

    ten_extension_thread_t *extension_thread = extension->extension_thread;
    TEN_ASSERT(extension_thread &&
                   ten_extension_thread_check_integrity(extension_thread, true),
               "Invalid use of extension_thread %p.", extension_thread);

    switch (level) {
    case TEN_METADATA_LEVEL_EXTENSION:
      return ten_extension_peek_property_async(
          extension, path, ten_extension_peek_property_async_cb, context, err);
      break;

    case TEN_METADATA_LEVEL_EXTENSION_GROUP: {
      ten_extension_group_t *extension_group =
          extension->extension_thread->extension_group;
      TEN_ASSERT(extension_group &&
                     ten_extension_group_check_integrity(extension_group, true),
                 "Invalid use of extension group %p", extension_group);

      ten_extension_group_peek_property_async(
          extension_group, path, ten_extension_group_peek_property_async_cb,
          context);
      break;
    }

    case TEN_METADATA_LEVEL_APP: {
      ten_app_t *app = extension->extension_context->engine->app;
      // TEN_NOLINTNEXTLINE(thread-check):
      // thread-check: Access the app's property from an extension, that
      // is, from the extension thread.
      TEN_ASSERT(app && ten_app_check_integrity(app, false),
                 "Invalid use of app %p", app);

      context->from.extension = extension;

      ten_app_peek_property_async(
          app, path, ten_app_peek_property_async_cb_go_back_to_extension,
          context);
      break;
    }

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    TEN_ASSERT(extension_group &&
                   ten_extension_group_check_integrity(extension_group, true),
               "Invalid use of extension_group %p.", extension_group);

    ten_extension_thread_t *extension_thread =
        extension_group->extension_thread;
    TEN_ASSERT(extension_thread &&
                   ten_extension_thread_check_integrity(extension_thread, true),
               "Invalid use of extension_thread %p.", extension_thread);

    switch (level) {
    case TEN_METADATA_LEVEL_EXTENSION_GROUP:
      ten_extension_group_peek_property_async(
          extension_group, path, ten_extension_group_peek_property_async_cb,
          context);
      break;

    case TEN_METADATA_LEVEL_APP: {
      ten_app_t *app = extension_group->extension_context->engine->app;
      // TEN_NOLINTNEXTLINE(thread-check):
      // thread-check: Access the app's property from an extension group,
      // that is, from the extension thread.
      TEN_ASSERT(app && ten_app_check_integrity(app, false),
                 "Invalid use of app %p", app);

      context->from.extension_group = extension_group;

      ten_app_peek_property_async(
          app, path, ten_app_peek_property_async_cb_go_back_to_extension_group,
          context);
      break;
    }

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env_get_attached_app(self);
    TEN_ASSERT(app && ten_app_check_integrity(app, true),
               "Invalid use of app %p.", app);

    switch (level) {
    case TEN_METADATA_LEVEL_APP:
      ten_app_peek_property_async(app, path, ten_app_peek_property_async_cb,
                                  context);
      break;

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return true;
}
