//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/metadata/manifest.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "ten_utils/lib/file.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/memory.h"

bool ten_manifest_get_type_and_name(const char *filename, TEN_ADDON_TYPE *type,
                                    ten_string_t *name, ten_error_t *err) {
  TEN_ASSERT(type, "Invalid argument.");
  TEN_ASSERT(name, "Invalid argument.");

  if (!filename || strlen(filename) == 0) {
    TEN_LOGW("Try to load manifest but file name not provided");
    return false;
  }

  char *buf = ten_file_read(filename);
  if (!buf) {
    TEN_LOGW("Can not read content from %s", filename);
    return false;
  }

  ten_json_t *json = ten_json_from_string(buf, err);
  TEN_FREE(buf);
  if (!json) {
    return false;
  }

  const char *type_str = ten_json_object_peek_string(json, TEN_STR_TYPE);
  *type = ten_addon_type_from_string(type_str);

  const char *name_str = ten_json_object_peek_string(json, TEN_STR_NAME);
  ten_string_set_from_c_str(name, name_str);

  ten_json_destroy(json);

  return true;
}
