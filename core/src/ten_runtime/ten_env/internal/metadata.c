//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_env/internal/metadata.h"

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/ten_env/metadata.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/ten_env/metadata.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/ten_env/metadata.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/ten_env/metadata.h"
#include "include_internal/ten_runtime/ten_env/metadata_cb.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static TEN_METADATA_LEVEL ten_get_metadata_level_from_name(
    const char **p_name, TEN_METADATA_LEVEL default_level) {
  TEN_ASSERT(p_name, "Invalid argument.");

  TEN_METADATA_LEVEL metadata_level = default_level;

  const char *name = *p_name;
  if (!name || strlen(name) == 0) {
    return metadata_level;
  }

  const char *delimiter_position =
      strstr(name, TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER);
  if (delimiter_position != NULL) {
    // Determine which level of the property store where the current property
    // should be accessed.
    if (ten_c_string_starts_with(
            name, TEN_STR_EXTENSION TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER)) {
      metadata_level = TEN_METADATA_LEVEL_EXTENSION;
      *p_name +=
          strlen(TEN_STR_EXTENSION TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER);
    } else if (ten_c_string_starts_with(
                   name, TEN_STR_EXTENSION_GROUP
                             TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER)) {
      metadata_level = TEN_METADATA_LEVEL_EXTENSION_GROUP;
      *p_name += strlen(
          TEN_STR_EXTENSION_GROUP TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER);
    } else if (ten_c_string_starts_with(
                   name, TEN_STR_APP TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER)) {
      metadata_level = TEN_METADATA_LEVEL_APP;
      *p_name += strlen(TEN_STR_APP TEN_STR_PROPERTY_STORE_SCOPE_DELIMITER);
    } else {
      int prefix_length = (int)(delimiter_position - name);
      TEN_LOGE("Unknown level: %.*s", prefix_length, name);
    }
  }

  return metadata_level;
}

TEN_METADATA_LEVEL
ten_determine_metadata_level(TEN_ENV_ATTACH_TO attach_to_type,
                             const char **p_name) {
  TEN_ASSERT(p_name, "Invalid argument.");

  TEN_METADATA_LEVEL level = TEN_METADATA_LEVEL_INVALID;

  switch (attach_to_type) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    level =
        ten_get_metadata_level_from_name(p_name, TEN_METADATA_LEVEL_EXTENSION);
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    level = ten_get_metadata_level_from_name(
        p_name, TEN_METADATA_LEVEL_EXTENSION_GROUP);
    TEN_ASSERT(level != TEN_METADATA_LEVEL_EXTENSION, "Should not happen.");
    break;

  case TEN_ENV_ATTACH_TO_APP:
    level = ten_get_metadata_level_from_name(p_name, TEN_METADATA_LEVEL_APP);
    TEN_ASSERT(level == TEN_METADATA_LEVEL_APP, "Should not happen.");
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return level;
}

bool ten_env_is_property_exist(ten_env_t *self, const char *path,
                               ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(path && strlen(path), "path should not be empty.");

  if (!path || !strlen(path)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "path should not be empty.");
    }
    return false;
  }

  // If the property cannot be found, it should not be an error, just return
  // false.
  ten_value_t *value = ten_env_peek_property(self, path, err);
  if (value != NULL) {
    return true;
  } else {
    return false;
  }
}

static ten_metadata_info_t *ten_env_get_manifest_info(ten_env_t *self) {
  ten_metadata_info_t *manifest_info = NULL;

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    manifest_info = self->attached_target.extension->manifest_info;
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    manifest_info = self->attached_target.extension_group->manifest_info;
    break;

  case TEN_ENV_ATTACH_TO_APP:
    manifest_info = self->attached_target.app->manifest_info;
    break;

  case TEN_ENV_ATTACH_TO_ADDON:
    manifest_info = self->attached_target.addon_host->manifest_info;
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return manifest_info;
}

static ten_metadata_info_t *ten_env_get_property_info(ten_env_t *self) {
  ten_metadata_info_t *property_info = NULL;

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    property_info = self->attached_target.extension->property_info;
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    property_info = self->attached_target.extension_group->property_info;
    break;

  case TEN_ENV_ATTACH_TO_APP:
    property_info = self->attached_target.app->property_info;
    break;

  case TEN_ENV_ATTACH_TO_ADDON:
    property_info = self->attached_target.addon_host->property_info;
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return property_info;
}

bool ten_env_init_manifest_from_json(ten_env_t *self, const char *json_string,
                                     TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  ten_metadata_info_t *manifest_info = ten_env_get_manifest_info(self);
  TEN_ASSERT(manifest_info && ten_metadata_info_check_integrity(manifest_info),
             "Should not happen.");

  return ten_metadata_info_set(manifest_info, TEN_METADATA_JSON_STR,
                               json_string);
}

bool ten_env_init_property_from_json(ten_env_t *self, const char *json_string,
                                     ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  if (ten_env_is_closed(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    }
    return false;
  }

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = self->attached_target.extension;
    TEN_ASSERT(extension, "Should not happen.");
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");

    if (extension->state != TEN_EXTENSION_STATE_ON_CONFIGURE) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Invalid timing to init property.");
      }
      return false;
    }

    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = self->attached_target.app;
    TEN_ASSERT(app, "Should not happen.");
    TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
    if (app->state != TEN_APP_STATE_ON_CONFIGURE) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Invalid timing to init property.");
      }
      return false;
    }

    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid attach_to.");
    }
    return false;
  }

  ten_metadata_info_t *property_info = ten_env_get_property_info(self);
  TEN_ASSERT(property_info && ten_metadata_info_check_integrity(property_info),
             "Should not happen.");

  return ten_metadata_info_set(property_info, TEN_METADATA_JSON_STR,
                               json_string);
}
