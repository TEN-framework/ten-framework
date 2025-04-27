//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/ten_env.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/sanitizer/thread_check.h"

bool ten_env_check_integrity(ten_env_t *self, bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_ENV_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    // Utilize the check_integrity of extension_thread to examine cases
    // involving the lock_mode of extension_thread.
    ten_extension_thread_t *extension_thread = NULL;
    switch (self->attach_to) {
    case TEN_ENV_ATTACH_TO_EXTENSION:
      extension_thread = self->attached_target.extension->extension_thread;
      break;
    case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
      extension_thread =
          self->attached_target.extension_group->extension_thread;
      break;
    default:
      break;
    }

    if (extension_thread) {
      if (ten_extension_thread_check_integrity_if_in_lock_mode(
              extension_thread)) {
        return true;
      }
    }

    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

ten_env_t *ten_env_create(void) {
  ten_env_t *self = (ten_env_t *)TEN_MALLOC(sizeof(ten_env_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_ENV_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->binding_handle.me_in_target_lang = NULL;
  self->is_closed = false;

  self->destroy_handler = NULL;
  ten_list_init(&self->ten_proxy_list);

  self->attach_to = TEN_ENV_ATTACH_TO_INVALID;
  self->attached_target.addon_host = NULL;

  return self;
}

static ten_env_t *ten_create_with_attach_to(TEN_ENV_ATTACH_TO attach_to_type,
                                            void *attach_to) {
  TEN_ASSERT(attach_to, "Should not happen.");
  TEN_ASSERT(attach_to_type != TEN_ENV_ATTACH_TO_INVALID, "Should not happen.");

  ten_env_t *self = ten_env_create();
  TEN_ASSERT(self, "Should not happen.");

  ten_env_set_attach_to(self, attach_to_type, attach_to);

  return self;
}

ten_env_t *ten_env_create_for_addon(ten_addon_host_t *addon_host) {
  TEN_ASSERT(addon_host, "Invalid argument.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host),
             "Invalid use of addon_host %p.", addon_host);

  return ten_create_with_attach_to(TEN_ENV_ATTACH_TO_ADDON, addon_host);
}

ten_env_t *ten_env_create_for_extension(ten_extension_t *extension) {
  TEN_ASSERT(extension, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Invalid use of extension %p.", extension);

  return ten_create_with_attach_to(TEN_ENV_ATTACH_TO_EXTENSION, extension);
}

ten_env_t *ten_env_create_for_extension_group(
    ten_extension_group_t *extension_group) {
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  return ten_create_with_attach_to(TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
                                   extension_group);
}

ten_env_t *ten_env_create_for_app(ten_app_t *app) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function would be called before the app thread started,
  // so it's thread safe.
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Should not happen.");

  return ten_create_with_attach_to(TEN_ENV_ATTACH_TO_APP, app);
}

ten_env_t *ten_env_create_for_engine(ten_engine_t *engine) {
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  return ten_create_with_attach_to(TEN_ENV_ATTACH_TO_ENGINE, engine);
}

ten_env_t *ten_env_create_for_addon_loader(ten_addon_loader_t *addon_loader) {
  TEN_ASSERT(addon_loader, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_check_integrity(addon_loader, true),
             "Should not happen.");

  return ten_create_with_attach_to(TEN_ENV_ATTACH_TO_ADDON_LOADER,
                                   addon_loader);
}

void ten_env_destroy(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function would be called after the app thread is
  // destroyed.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  if (self->destroy_handler && self->binding_handle.me_in_target_lang) {
    self->destroy_handler(self->binding_handle.me_in_target_lang);
  }

  TEN_ASSERT(ten_list_is_empty(&self->ten_proxy_list), "Should not happen.");

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_signature_set(&self->signature, 0);

  TEN_FREE(self);
}

void ten_env_close(ten_env_t *self) {
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_APP:
    TEN_LOGD("[%s] Close ten of app.",
             ten_app_get_uri(self->attached_target.app));
    break;
  case TEN_ENV_ATTACH_TO_ENGINE:
    TEN_LOGD("[%s] Close ten of engine.",
             ten_engine_get_id(self->attached_target.engine, true));
    break;
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    TEN_LOGD(
        "[%s] Close ten of extension group.",
        ten_string_get_raw_str(&self->attached_target.extension_group->name));
    break;
  case TEN_ENV_ATTACH_TO_EXTENSION:
    TEN_LOGD("[%s] Close ten of extension.",
             ten_string_get_raw_str(&self->attached_target.extension->name));
    break;
  case TEN_ENV_ATTACH_TO_ADDON:
    TEN_LOGV("[%s] Close ten of addon.",
             ten_string_get_raw_str(&self->attached_target.addon_host->name));
    break;
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  self->is_closed = true;
}

bool ten_env_is_closed(ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Should not happen.");

  return self->is_closed;
}

void ten_env_set_destroy_handler_in_target_lang(
    ten_env_t *self,
    ten_env_destroy_handler_in_target_lang_func_t destroy_handler) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  self->destroy_handler = destroy_handler;
}

ten_runloop_t *ten_env_get_attached_runloop(ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Should not happen.");

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_APP:
    return ten_app_get_attached_runloop(ten_env_get_attached_app(self));
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    return ten_extension_group_get_attached_runloop(
        ten_env_get_attached_extension_group(self));
  case TEN_ENV_ATTACH_TO_EXTENSION:
    return ten_extension_get_attached_runloop(
        ten_env_get_attached_extension(self));
  case TEN_ENV_ATTACH_TO_ENGINE:
    return ten_engine_get_attached_runloop(ten_env_get_attached_engine(self));
  default:
    TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
    break;
  }

  return NULL;
}

void *ten_env_get_attached_target(ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Should not happen.");

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    return ten_env_get_attached_extension(self);
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    return ten_env_get_attached_extension_group(self);
  case TEN_ENV_ATTACH_TO_ENGINE:
    return ten_env_get_attached_engine(self);
  case TEN_ENV_ATTACH_TO_APP:
    return ten_env_get_attached_app(self);
  case TEN_ENV_ATTACH_TO_ADDON:
    return ten_env_get_attached_addon(self);
  default:
    TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
    return NULL;
  }
}

TEN_ENV_ATTACH_TO ten_env_get_attach_to(ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self,
                                     // TODO(xilin): Change it to true after
                                     // the addon thread check is ready.
                                     false),
             "Should not happen.");
  return self->attach_to;
}

void ten_env_set_attach_to(ten_env_t *self, TEN_ENV_ATTACH_TO attach_to_type,
                           void *attach_to) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Should not happen.");

  self->attach_to = attach_to_type;
  switch (attach_to_type) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    self->attached_target.extension = attach_to;
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    self->attached_target.extension_group = attach_to;
    break;

  case TEN_ENV_ATTACH_TO_APP:
    self->attached_target.app = attach_to;
    break;

  case TEN_ENV_ATTACH_TO_ADDON:
    self->attached_target.addon_host = attach_to;
    break;

  case TEN_ENV_ATTACH_TO_ENGINE:
    self->attached_target.engine = attach_to;
    break;

  case TEN_ENV_ATTACH_TO_ADDON_LOADER:
    self->attached_target.addon_loader = attach_to;
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

ten_app_t *ten_env_get_belonging_app(ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Should not happen.");

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_APP:
    return ten_env_get_attached_app(self);
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    TEN_ASSERT(extension, "Should not happen.");
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");

    ten_extension_thread_t *extension_thread = extension->extension_thread;
    TEN_ASSERT(extension_thread &&
                   ten_extension_thread_check_integrity(extension_thread, true),
               "Should not happen.");

    ten_extension_context_t *extension_context =
        extension_thread->extension_context;
    TEN_ASSERT(extension_context && ten_extension_context_check_integrity(
                                        extension_context, false),
               "Should not happen.");

    ten_engine_t *engine = extension_context->engine;
    TEN_ASSERT(engine && ten_engine_check_integrity(engine, false),
               "Should not happen.");

    ten_app_t *app = engine->app;
    TEN_ASSERT(app && ten_app_check_integrity(app, false),
               "Should not happen.");

    return app;
  }
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    TEN_ASSERT(extension_group &&
                   ten_extension_group_check_integrity(extension_group, true),
               "Should not happen.");

    ten_extension_thread_t *extension_thread =
        extension_group->extension_thread;
    TEN_ASSERT(extension_thread &&
                   ten_extension_thread_check_integrity(extension_thread, true),
               "Should not happen.");

    ten_extension_context_t *extension_context =
        extension_thread->extension_context;
    TEN_ASSERT(extension_context && ten_extension_context_check_integrity(
                                        extension_context, false),
               "Should not happen.");

    ten_engine_t *engine = extension_context->engine;
    TEN_ASSERT(engine && ten_engine_check_integrity(engine, false),
               "Should not happen.");

    ten_app_t *app = engine->app;
    TEN_ASSERT(app && ten_app_check_integrity(app, false),
               "Should not happen.");

    return app;
  }

  case TEN_ENV_ATTACH_TO_ENGINE: {
    ten_engine_t *engine = ten_env_get_attached_engine(self);
    TEN_ASSERT(engine && ten_engine_check_integrity(engine, true),
               "Should not happen.");

    ten_app_t *app = engine->app;
    TEN_ASSERT(app && ten_app_check_integrity(app, false),
               "Should not happen.");

    return app;
  }

  default:
    TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
    return NULL;
  }
}
