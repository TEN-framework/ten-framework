//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/metadata/metadata_info.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/base_dir.h"
#include "include_internal/ten_runtime/extension/base_dir.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/base_dir.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/metadata/default/default.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

bool ten_metadata_info_check_integrity(ten_metadata_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_METADATA_INFO_SIGNATURE) {
    return false;
  }
  return true;
}

ten_metadata_info_t *ten_metadata_info_create(TEN_METADATA_ATTACH_TO attach_to,
                                              ten_env_t *belonging_to) {
  TEN_ASSERT(attach_to != TEN_METADATA_ATTACH_TO_INVALID, "Invalid argument.");
  TEN_ASSERT(belonging_to && ten_env_check_integrity(belonging_to, true),
             "Should not happen.");
  TEN_ASSERT(ten_env_get_attach_to(belonging_to) != TEN_ENV_ATTACH_TO_INVALID,
             "Invalid argument.");

  ten_metadata_info_t *self =
      (ten_metadata_info_t *)TEN_MALLOC(sizeof(ten_metadata_info_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_METADATA_INFO_SIGNATURE);

  self->attach_to = attach_to;
  self->type = TEN_METADATA_INVALID;
  self->value = NULL;
  self->belonging_to = belonging_to;

  return self;
}

void ten_metadata_info_destroy(ten_metadata_info_t *self) {
  TEN_ASSERT(self && ten_metadata_info_check_integrity(self),
             "Should not happen.");

  ten_signature_set(&self->signature, 0);

  if (self->value) {
    ten_string_destroy(self->value);
  }

  self->belonging_to = NULL;

  TEN_FREE(self);
}

static ten_string_t *ten_metadata_info_filename_to_absolute_path(
    ten_metadata_info_t *self, const char *value) {
  TEN_ASSERT(self && value && self->belonging_to, "Invalid argument.");

  ten_string_t filename;
  ten_string_init_formatted(&filename, "%s", value);

  bool is_absolute = ten_path_is_absolute(&filename);
  ten_string_deinit(&filename);

  if (is_absolute) {
    if (ten_path_exists(value)) {
      return ten_string_create_formatted("%s", value);
    } else {
      TEN_LOGW("File '%s' does not exist.", value);
      return NULL;
    }
  }

  ten_string_t *path = NULL;
  switch (ten_env_get_attach_to(self->belonging_to)) {
  case TEN_ENV_ATTACH_TO_APP: {
    const char *base_dir =
        ten_app_get_base_dir(ten_env_get_attached_app(self->belonging_to));
    if (base_dir) {
      path = ten_string_create_from_c_str(base_dir);
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    const char *base_dir = ten_extension_group_get_base_dir(
        ten_env_get_attached_extension_group(self->belonging_to));
    if (base_dir) {
      path = ten_string_create_from_c_str(base_dir);
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION: {
    const char *base_dir = ten_extension_get_base_dir(
        ten_env_get_attached_extension(self->belonging_to));
    if (base_dir) {
      path = ten_string_create_from_c_str(base_dir);
    }
    break;
  }

  case TEN_ENV_ATTACH_TO_ADDON: {
    const char *base_dir = ten_addon_host_get_base_dir(
        ten_env_get_attached_addon(self->belonging_to));
    if (base_dir) {
      path = ten_string_create_from_c_str(base_dir);
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  if (!path) {
    return NULL;
  }

  int rc = ten_path_join_c_str(path, value);
  if (rc) {
    TEN_LOGW("Failed to join path under %s.", ten_string_get_raw_str(path));
    ten_string_destroy(path);
    return NULL;
  }

  if (!ten_path_exists(ten_string_get_raw_str(path))) {
    TEN_LOGW("File '%s' does not exist.", ten_string_get_raw_str(path));
    ten_string_destroy(path);
    return NULL;
  }

  return path;
}

static void ten_metadata_info_get_debug_display(ten_metadata_info_t *self,
                                                ten_string_t *display) {
  TEN_ASSERT(
      self && ten_metadata_info_check_integrity(self) && self->belonging_to,
      "Invalid argument.");
  TEN_ASSERT(display && ten_string_check_integrity(display),
             "Invalid argument.");

  switch (ten_env_get_attach_to(self->belonging_to)) {
  case TEN_ENV_ATTACH_TO_ADDON:
    ten_string_set_formatted(display, "addon(%s)",
                             ten_addon_host_get_name(ten_env_get_attached_addon(
                                 self->belonging_to)));
    break;

  case TEN_ENV_ATTACH_TO_APP: {
    const char *uri =
        ten_app_get_uri(ten_env_get_attached_app(self->belonging_to));
    ten_string_set_formatted(display, "app(%s)", uri);
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    ten_string_set_formatted(
        display, "extension_group(%s)",
        ten_extension_group_get_name(
            ten_env_get_attached_extension_group(self->belonging_to), true));
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION:
    ten_string_set_formatted(
        display, "extension(%s)",
        ten_extension_get_name(
            ten_env_get_attached_extension(self->belonging_to), true));
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

bool ten_metadata_info_set(ten_metadata_info_t *self, TEN_METADATA_TYPE type,
                           const char *value) {
  TEN_ASSERT(self && value, "Should not happen.");

  ten_string_t display;
  TEN_STRING_INIT(display);
  ten_metadata_info_get_debug_display(self, &display);

  bool validated = false;
  ten_string_t *absolute_path = NULL;

  ten_error_t err;
  TEN_ERROR_INIT(err);

  do {
    if (!value) {
      TEN_LOGW("Failed to set metadata for %s, the `value` is required.",
               ten_string_get_raw_str(&display));
      break;
    }

    if (type == TEN_METADATA_JSON_FILENAME) {
      absolute_path = ten_metadata_info_filename_to_absolute_path(self, value);
      if (!absolute_path) {
        TEN_LOGW("Failed to set metadata for %s, file '%s' is not found.",
                 ten_string_get_raw_str(&display), value);
        break;
      }

      value = ten_string_get_raw_str(absolute_path);
    }

    switch (self->attach_to) {
    case TEN_METADATA_ATTACH_TO_MANIFEST:
      switch (type) {
      case TEN_METADATA_JSON_STR:
        if (self->belonging_to->attach_to == TEN_ENV_ATTACH_TO_ADDON) {
          // TODO(Wei): The current protocol's manifest doesn't fully comply
          // with the spec, so we'll bypass the validation of the protocol
          // manifest for now.
          validated = true;
        } else {
          validated = ten_manifest_json_string_is_valid(value, &err);
        }
        break;

      case TEN_METADATA_JSON_FILENAME:
        validated = ten_manifest_json_file_is_valid(value, &err);
        break;

      default:
        TEN_ASSERT(0, "Handle more types.");
        break;
      }
      break;

    case TEN_METADATA_ATTACH_TO_PROPERTY:
      switch (type) {
      case TEN_METADATA_JSON_STR:
        validated = ten_property_json_string_is_valid(value, &err);
        break;

      case TEN_METADATA_JSON_FILENAME:
        validated = ten_property_json_file_is_valid(value, &err);
        break;

      default:
        TEN_ASSERT(0, "Handle more types.");
        break;
      }
      break;

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }

    if (!validated) {
      TEN_LOGW("Failed to set metadata for %s, %s.",
               ten_string_get_raw_str(&display), ten_error_message(&err));
      break;
    }

    self->type = type;
    if (self->value) {
      ten_string_destroy(self->value);
      self->value = NULL;
    }

    self->value = ten_string_create_formatted("%s", value);
  } while (0);

  ten_string_deinit(&display);
  ten_error_deinit(&err);
  if (absolute_path) {
    ten_string_destroy(absolute_path);
  }

  return validated;
}

bool ten_handle_manifest_info_when_on_configure_done(ten_metadata_info_t **self,
                                                     const char *base_dir,
                                                     ten_value_t *manifest,
                                                     ten_error_t *err) {
  TEN_ASSERT(self && *self && ten_metadata_info_check_integrity(*self),
             "Invalid argument.");
  TEN_ASSERT(manifest, "Invalid argument.");

  switch (ten_env_get_attach_to((*self)->belonging_to)) {
  case TEN_ENV_ATTACH_TO_APP:
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
  case TEN_ENV_ATTACH_TO_EXTENSION:
    if ((*self)->type == TEN_METADATA_INVALID) {
      ten_set_default_manifest_info(base_dir, (*self), err);
    }
    break;

  case TEN_ENV_ATTACH_TO_ADDON:
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  if (!ten_metadata_load_from_info(manifest, (*self), err)) {
    return false;
  }

  ten_metadata_info_destroy((*self));
  *self = NULL;

  return true;
}

bool ten_handle_property_info_when_on_configure_done(ten_metadata_info_t **self,
                                                     const char *base_dir,
                                                     ten_value_t *property,
                                                     ten_error_t *err) {
  TEN_ASSERT(self && *self && ten_metadata_info_check_integrity(*self),
             "Invalid argument.");
  TEN_ASSERT(property, "Invalid argument.");

  switch (ten_env_get_attach_to((*self)->belonging_to)) {
  case TEN_ENV_ATTACH_TO_APP:
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
  case TEN_ENV_ATTACH_TO_EXTENSION:
    if ((*self)->type == TEN_METADATA_INVALID) {
      ten_set_default_property_info(base_dir, (*self), NULL);
    }
    break;

  case TEN_ENV_ATTACH_TO_ADDON:
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  if (!ten_metadata_load_from_info(property, (*self), err)) {
    return false;
  }

  ten_metadata_info_destroy((*self));
  *self = NULL;

  return true;
}
