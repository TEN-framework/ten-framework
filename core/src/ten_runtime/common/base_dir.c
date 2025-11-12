//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/constant_str.h"
#include "ten_utils/lib/file.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/memory.h"

// Traverse up through the parent folders, searching for a folder containing a
// manifest.json with the specified type and name.
ten_string_t *ten_find_base_dir(const char *start_path, const char *addon_type,
                                const char *addon_name) {
  TEN_ASSERT(start_path && addon_type && strlen(addon_type),
             "Invalid argument.");

  ten_string_t *parent_path = ten_string_create_formatted("%s", start_path);
  if (!parent_path) {
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  while (ten_path_is_dir(parent_path)) {
    ten_string_t *manifest_path = ten_string_clone(parent_path);
    ten_string_append_formatted(manifest_path, "/manifest.json");

    if (ten_path_exists(ten_string_get_raw_str(manifest_path))) {
      // Read manifest.json, and check if there is a top-level "type" field with
      // a value specified in `addon_type` parameter.
      const char *manifest_content =
          ten_file_read(ten_string_get_raw_str(manifest_path));
      if (manifest_content) {
        ten_json_t *json = ten_json_from_string(manifest_content, NULL);
        if (json) {
          do {
            const char *type_in_manifest =
                ten_json_object_peek_string(json, TEN_STR_TYPE);
            if (!type_in_manifest) {
              break;
            }
            if (strcmp(type_in_manifest, addon_type) != 0) {
              break;
            }

            if (addon_name && strlen(addon_name)) {
              const char *name_in_manifest =
                  ten_json_object_peek_string(json, TEN_STR_NAME);
              if (!name_in_manifest) {
                break;
              }
              if (strcmp(name_in_manifest, addon_name) != 0) {
                break;
              }
            }

            ten_string_t *base_dir = ten_path_realpath(parent_path);
            ten_path_to_system_flavor(base_dir);

            ten_json_destroy(json);
            TEN_FREE(manifest_content);
            ten_string_destroy(manifest_path);
            ten_string_destroy(parent_path);

            return base_dir;
          } while (0);

          ten_json_destroy(json);
        }

        TEN_FREE(manifest_content);
      }
    }

    ten_string_destroy(manifest_path);

    ten_string_t *next_parent = ten_path_get_dirname(parent_path);
    if (!next_parent || ten_string_is_empty(next_parent) ||
        ten_string_is_equal(parent_path, next_parent)) {
      // No more parent folders.
      ten_string_destroy(parent_path);
      if (next_parent) {
        ten_string_destroy(next_parent);
      }
      return NULL;
    }

    ten_string_destroy(parent_path);
    parent_path = next_parent;
  }

  return NULL;
}
