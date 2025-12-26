//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/macro/check.h"

const char *ten_env_get_attached_instance_name(ten_env_t *self,
                                               bool check_thread) {
  TEN_ASSERT(self && ten_env_check_integrity(self, check_thread),
             "Invalid argument.");

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    return ten_extension_get_name(extension, true);
  }
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    return ten_extension_group_get_name(extension_group, true);
  }
  case TEN_ENV_ATTACH_TO_ENGINE: {
    ten_engine_t *engine = ten_env_get_attached_engine(self);
    return ten_engine_get_id(engine, true);
  }
  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env_get_attached_app(self);
    return ten_app_get_uri(app);
  }
  case TEN_ENV_ATTACH_TO_ADDON: {
    ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
    return ten_addon_host_get_name(addon_host);
  }
  default:
    TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
    return NULL;
  }
}

void ten_env_get_attached_target_loc(ten_env_t *self, ten_loc_t *loc,
                                     bool check_thread) {
  TEN_ASSERT(self && ten_env_check_integrity(self, check_thread),
             "Invalid argument.");
  TEN_ASSERT(loc && ten_loc_check_integrity(loc), "Invalid argument.");

  // Clear the location first.
  ten_loc_clear(loc);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid extension.");

    // Get location from extension_info.
    if (extension->extension_info) {
      ten_loc_copy(loc, &extension->extension_info->loc);
    } else {
      // Fallback: Only set extension name if extension_info is not available.
      const char *extension_name = ten_extension_get_name(extension, true);
      if (extension_name) {
        ten_loc_set_extension_name(loc, extension_name);
      }
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_ENGINE: {
    ten_engine_t *engine = ten_env_get_attached_engine(self);
    TEN_ASSERT(engine && ten_engine_check_integrity(engine, true),
               "Invalid engine.");

    // Get app_uri from app if available.
    if (engine->app) {
      const char *app_uri = ten_app_get_uri(engine->app);
      if (app_uri && strlen(app_uri) > 0) {
        ten_loc_set_app_uri(loc, app_uri);
      }
    }

    // Get graph_id from engine.
    if (!ten_string_is_empty(&engine->graph_id)) {
      ten_loc_set_graph_id(loc, ten_string_get_raw_str(&engine->graph_id));
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env_get_attached_app(self);
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: The app uri remains unchanged during the lifecycle of app,
    // allowing safe cross-thread access.
    TEN_ASSERT(app && ten_app_check_integrity(app, false), "Invalid app.");

    // Get app_uri.
    const char *app_uri = ten_app_get_uri(app);
    if (app_uri && strlen(app_uri) > 0) {
      ten_loc_set_app_uri(loc, app_uri);
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
  case TEN_ENV_ATTACH_TO_ADDON:
  case TEN_ENV_ATTACH_TO_ADDON_LOADER:
    // These types don't have a well-defined location in the traditional sense
    // (app_uri, graph_id, extension_name), so we just clear the location.
    // The location remains empty after ten_loc_clear().
    break;

  default:
    TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
    break;
  }
}
