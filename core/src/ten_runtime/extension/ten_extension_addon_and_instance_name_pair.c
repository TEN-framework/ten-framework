//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_addon_and_instance_name_pair.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/check.h"

ten_extension_addon_and_instance_name_pair_t *
ten_extension_addon_and_instance_name_pair_create(
    const char *extension_addon_name, const char *extension_instance_name) {
  ten_extension_addon_and_instance_name_pair_t *self =
      (ten_extension_addon_and_instance_name_pair_t *)TEN_MALLOC(
          sizeof(ten_extension_addon_and_instance_name_pair_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_string_init_formatted(&self->addon_name, "%s",
                            extension_addon_name ? extension_addon_name : "");
  ten_string_init_formatted(
      &self->instance_name, "%s",
      extension_instance_name ? extension_instance_name : "");

  return self;
}

void ten_extension_addon_and_instance_name_pair_destroy(
    ten_extension_addon_and_instance_name_pair_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_string_deinit(&self->addon_name);
  ten_string_deinit(&self->instance_name);

  TEN_FREE(self);
}

void ten_extension_addon_and_instance_name_pair_to_json(
    ten_json_t *json, const char *key, ten_string_t *addon_name,
    ten_string_t *instance_name) {
  TEN_ASSERT(json && key && addon_name && instance_name, "Should not happen.");

  if (ten_string_is_empty(addon_name)) {
    ten_json_object_set_string(json, key,
                               ten_string_get_raw_str(instance_name));
  } else {
    ten_json_t extension_group_json = TEN_JSON_INIT_VAL(json->ctx, false);
    ten_json_init_object(&extension_group_json);

    ten_json_object_set_string(&extension_group_json, TEN_STR_ADDON,
                               ten_string_get_raw_str(addon_name));
    ten_json_object_set_string(&extension_group_json, TEN_STR_NAME,
                               ten_string_get_raw_str(instance_name));

    ten_json_object_set(json, key, &extension_group_json);
  }
}
