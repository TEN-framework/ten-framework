//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/ten_env/metadata.h"
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
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static ten_env_set_property_sync_context_t *
ten_env_set_property_sync_context_create(ten_error_t *err) {
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_env_set_property_sync_context_t *context =
      TEN_MALLOC(sizeof(ten_env_set_property_sync_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  context->err = err;
  context->completed = ten_event_create(0, 0);

  return context;
}

static void ten_env_set_property_sync_context_destroy(
    ten_env_set_property_sync_context_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  self->err = NULL;
  ten_event_destroy(self->completed);
  TEN_FREE(self);
}

static void ten_app_set_property_sync_cb(ten_app_t *app, ten_error_t *err,
                                         void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_env_set_property_sync_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  ten_error_set(context->err, ten_error_code(err), ten_error_message(err));
  ten_event_set(context->completed);
}

static ten_env_set_property_async_context_t *
ten_env_set_property_async_context_create(ten_env_t *ten_env,
                                          ten_env_set_property_async_cb_t cb,
                                          void *cb_data) {
  ten_env_set_property_async_context_t *context =
      TEN_MALLOC(sizeof(ten_env_set_property_async_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  context->ten_env = ten_env;
  context->cb = cb;
  context->cb_data = cb_data;

  context->from.extension = NULL;
  context->from.extension_group = NULL;

  return context;
}

static void ten_env_set_property_async_context_destroy(
    ten_env_set_property_async_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_FREE(self);
}

static void ten_extension_set_property_async_cb(ten_extension_t *extension,
                                                bool res, void *cb_data,
                                                ten_error_t *err) {
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_env_set_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  if (context->cb) {
    context->cb(context->ten_env, res, context->cb_data, err);
  }

  ten_env_set_property_async_context_destroy(context);
}

static void ten_extension_group_set_property_async_cb(
    ten_extension_group_t *extension_group, bool res, void *cb_data) {
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_env_set_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  if (context->cb) {
    context->cb(context->ten_env, res, context->cb_data, NULL);
  }

  ten_env_set_property_async_context_destroy(context);
}

static void ten_env_set_property_done_task(TEN_UNUSED void *from, void *arg) {
  ten_env_set_property_async_context_t *context = arg;
  TEN_ASSERT(context, "Should not happen.");
  TEN_ASSERT(context->from.extension != NULL, "Invalid argument.");

  if (context->cb) {
    context->cb(context->ten_env, context->res, context->cb_data, NULL);
  }

  ten_env_set_property_async_context_destroy(context);
}

static void ten_app_set_property_async_cb_go_back_to_extension(ten_app_t *app,
                                                               ten_error_t *err,
                                                               void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_env_set_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");
  TEN_ASSERT(context->from.extension != NULL, "Invalid argument.");

  context->res = ten_error_is_success(err);

  int rc = ten_runloop_post_task_tail(
      ten_extension_get_attached_runloop(context->from.extension),
      ten_env_set_property_done_task, NULL, context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void ten_app_set_property_async_cb_go_back_to_extension_group(
    ten_app_t *app, ten_error_t *err, void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_env_set_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");
  TEN_ASSERT(context->from.extension_group != NULL, "Invalid argument.");

  context->res = ten_error_is_success(err);

  int rc = ten_runloop_post_task_tail(
      ten_extension_group_get_attached_runloop(context->from.extension_group),
      ten_env_set_property_done_task, NULL, context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void ten_app_set_property_async_cb(ten_app_t *app, ten_error_t *err,
                                          void *cb_data) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_env_set_property_async_context_t *context = cb_data;
  TEN_ASSERT(context, "Should not happen.");

  if (context->cb) {
    context->cb(context->ten_env, ten_error_is_success(err), context->cb_data,
                err);
  }

  ten_env_set_property_async_context_destroy(context);
}

bool ten_env_set_property_internal(ten_env_t *self, const char *path,
                                   ten_value_t *value, bool internal_use,
                                   ten_error_t *err) {
  TEN_ASSERT(self && ten_env_check_integrity(self, true),
             "Invalid use of ten_env %p.", self);
  TEN_ASSERT(path && strlen(path), "path should not be empty.");

  bool result = true;

  if (ten_env_is_closed(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    }
    result = false;
    goto done;
  }

  if (!path || !strlen(path)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "path should not be empty.");
    }
    result = false;
    goto done;
  }

  if (!value) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "value should not be empty.");
    }
    result = false;
    goto done;
  }

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
      result = ten_extension_set_property(extension, path, value, err);
      break;

    case TEN_METADATA_LEVEL_EXTENSION_GROUP: {
      if (true) {
        result = false;
        if (err) {
          ten_error_set(
              err, TEN_ERROR_CODE_GENERIC,
              "Setting properties in higher-level scopes is not allowed. "
              "Properties can only be set within the current scope.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_extension_group_t *extension_group =
            extension->extension_thread->extension_group;
        TEN_ASSERT(extension_group && ten_extension_group_check_integrity(
                                          extension_group, true),
                   "Invalid use of extension group %p", extension_group);

        result = ten_extension_group_set_property(extension_group, path, value);
      }
      break;
    }

    case TEN_METADATA_LEVEL_APP: {
      if (true) {
        result = false;
        if (err) {
          ten_error_set(
              err, TEN_ERROR_CODE_GENERIC,
              "Setting properties in higher-level scopes is not allowed. "
              "Properties can only be set within the current scope.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_app_t *app = extension->extension_context->engine->app;
        // TEN_NOLINTNEXTLINE(thread-check):
        // thread-check: Access the app's property from an extension, that
        // is, from the extension thread.
        TEN_ASSERT(app && ten_app_check_integrity(app, false),
                   "Invalid use of app %p", app);

        if (ten_app_thread_call_by_me(app)) {
          result = ten_app_set_property(app, path, value, err);
        } else {
          bool new_error_created = false;
          if (!err) {
            new_error_created = true;
            err = ten_error_create();
          }

          ten_env_set_property_sync_context_t *context =
              ten_env_set_property_sync_context_create(err);
          TEN_ASSERT(context, "Should not happen.");

          ten_app_set_property_async(app, path, value,
                                     ten_app_set_property_sync_cb, context);

          ten_event_wait(context->completed, -1);
          result = ten_error_is_success(err);

          ten_env_set_property_sync_context_destroy(context);

          if (new_error_created) {
            ten_error_destroy(err);
            err = NULL;
          }
        }
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
      if (true) {
        result = false;
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                        "The set property of extension group is currently not "
                        "supported; use init_property_from_json instead.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        result = ten_extension_group_set_property(extension_group, path, value);
      }
      break;

    case TEN_METADATA_LEVEL_APP: {
      if (true) {
        result = false;
        if (err) {
          ten_error_set(
              err, TEN_ERROR_CODE_GENERIC,
              "Setting properties in higher-level scopes is not allowed. "
              "Properties can only be set within the current scope.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_app_t *app = extension_group->extension_context->engine->app;
        // TEN_NOLINTNEXTLINE(thread-check):
        // thread-check: Access the app's property from an extension group,
        // that is, from the extension thread.
        TEN_ASSERT(app && ten_app_check_integrity(app, false),
                   "Invalid use of app %p", app);

        if (ten_app_thread_call_by_me(app)) {
          result = ten_app_set_property(app, path, value, err);
        } else {
          bool new_error_created = false;
          if (!err) {
            new_error_created = true;
            err = ten_error_create();
          }

          ten_env_set_property_sync_context_t *context =
              ten_env_set_property_sync_context_create(err);
          TEN_ASSERT(context, "Should not happen.");

          ten_app_set_property_async(app, path, value,
                                     ten_app_set_property_sync_cb, context);

          ten_event_wait(context->completed, -1);
          result = ten_error_is_success(err);

          ten_env_set_property_sync_context_destroy(context);

          if (new_error_created) {
            ten_error_destroy(err);
            err = NULL;
          }
        }
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
      if (!internal_use) {
        result = false;
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                        "The set property of app is currently not "
                        "supported; use init_property_from_json instead.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        if (ten_app_thread_call_by_me(app)) {
          result = ten_app_set_property(app, path, value, err);
        } else {
          TEN_ASSERT(0, "Do we really need this?");
        }
      }
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

done:
  if (!result) {
    if (err) {
      TEN_LOGW("Failed to set property: %s.", ten_error_message(err));
    } else {
      TEN_LOGW("Failed to set property.");
    }
  }

  return result;
}

bool ten_env_set_property(ten_env_t *self, const char *path, ten_value_t *value,
                          ten_error_t *err) {
  return ten_env_set_property_internal(self, path, value, false, err);
}

bool ten_env_set_property_async(ten_env_t *self, const char *path,
                                ten_value_t *value,
                                ten_env_set_property_async_cb_t cb,
                                void *cb_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(path && strlen(path), "path should not be empty.");

  if (!path || !strlen(path)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "path should not be empty.");
    }
    return false;
  }

  if (!value) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "value should not be empty.");
    }
    return false;
  }

  const char **p_path = &path;

  ten_env_set_property_async_context_t *context =
      ten_env_set_property_async_context_create(self, cb, cb_data);
  TEN_ASSERT(context, "Should not happen.");

  bool result = true;

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
      ten_extension_set_property_async(extension, path, value,
                                       ten_extension_set_property_async_cb,
                                       context, err);
      break;

    case TEN_METADATA_LEVEL_EXTENSION_GROUP: {
      if (true) {
        result = false;
        if (err) {
          ten_error_set(
              err, TEN_ERROR_CODE_GENERIC,
              "Setting properties in higher-level scopes is not allowed. "
              "Properties can only be set within the current scope.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_extension_group_t *extension_group =
            extension->extension_thread->extension_group;
        TEN_ASSERT(extension_group && ten_extension_group_check_integrity(
                                          extension_group, true),
                   "Invalid use of extension group %p", extension_group);

        ten_extension_group_set_property_async(
            extension_group, path, value,
            ten_extension_group_set_property_async_cb, context);
      }
      break;
    }

    case TEN_METADATA_LEVEL_APP: {
      if (true) {
        result = false;
        if (err) {
          ten_error_set(
              err, TEN_ERROR_CODE_GENERIC,
              "Setting properties in higher-level scopes is not allowed. "
              "Properties can only be set within the current scope.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_app_t *app = extension->extension_context->engine->app;
        // TEN_NOLINTNEXTLINE(thread-check):
        // thread-check: Access the app's property from an extension, that
        // is, from the extension thread.
        TEN_ASSERT(app && ten_app_check_integrity(app, false),
                   "Invalid use of app %p", app);

        context->from.extension = extension;

        ten_app_set_property_async(
            app, path, value,
            ten_app_set_property_async_cb_go_back_to_extension, context);
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
      if (true) {
        result = false;
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                        "The set property of extension group is currently not "
                        "supported; use init_property_from_json instead.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_extension_group_set_property_async(
            extension_group, path, value,
            ten_extension_group_set_property_async_cb, context);
      }
      break;

    case TEN_METADATA_LEVEL_APP: {
      if (true) {
        result = false;
        if (err) {
          ten_error_set(
              err, TEN_ERROR_CODE_GENERIC,
              "Setting properties in higher-level scopes is not allowed. "
              "Properties can only be set within the current scope.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_app_t *app = extension_group->extension_context->engine->app;
        // TEN_NOLINTNEXTLINE(thread-check):
        // thread-check: Access the app's property from an extension group,
        // that is, from the extension thread.
        TEN_ASSERT(app && ten_app_check_integrity(app, false),
                   "Invalid use of app %p", app);

        context->from.extension_group = extension_group;

        ten_app_set_property_async(
            app, path, value,
            ten_app_set_property_async_cb_go_back_to_extension_group, context);
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
    case TEN_METADATA_LEVEL_APP:
      if (true) {
        result = false;
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                        "The set property of app is currently not "
                        "supported; use init_property_from_json instead.");
        }
        TEN_ASSERT(0, "Should not happen.");
      } else {
        ten_app_set_property_async(app, path, value,
                                   ten_app_set_property_async_cb, context);
      }
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

  return result;
}
