//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/metadata/metadata.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/macro/mark.h"

void ten_set_default_manifest_info(const char *base_dir,
                                   ten_metadata_info_t *manifest,
                                   TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(manifest, "Should not happen.");

  if (!base_dir || !strlen(base_dir)) {
    const char *instance_name =
        ten_env_get_attached_instance_name(manifest->belonging_to, true);
    TEN_LOGI(
        "Skip the loading of manifest.json because the base_dir of %s is "
        "missing.",
        instance_name);
    return;
  }

  ten_string_t manifest_json_file_path;
  ten_string_init_formatted(&manifest_json_file_path, "%s/manifest.json",
                            base_dir);
  ten_path_to_system_flavor(&manifest_json_file_path);
  if (ten_path_exists(ten_string_get_raw_str(&manifest_json_file_path))) {
    ten_metadata_info_set(manifest, TEN_METADATA_JSON_FILENAME,
                          ten_string_get_raw_str(&manifest_json_file_path));
  }
  ten_string_deinit(&manifest_json_file_path);
}

void ten_set_default_property_info(const char *base_dir,
                                   ten_metadata_info_t *property,
                                   TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(property, "Should not happen.");

  if (!base_dir || !strlen(base_dir)) {
    const char *instance_name =
        ten_env_get_attached_instance_name(property->belonging_to, true);
    TEN_LOGI(
        "Skip the loading of property.json because the base_dir of %s is "
        "missing.",
        instance_name);
    return;
  }

  ten_string_t property_json_file_path;
  ten_string_init_formatted(&property_json_file_path, "%s/property.json",
                            base_dir);
  ten_path_to_system_flavor(&property_json_file_path);
  if (ten_path_exists(ten_string_get_raw_str(&property_json_file_path))) {
    ten_metadata_info_set(property, TEN_METADATA_JSON_FILENAME,
                          ten_string_get_raw_str(&property_json_file_path));
  }
  ten_string_deinit(&property_json_file_path);
}
