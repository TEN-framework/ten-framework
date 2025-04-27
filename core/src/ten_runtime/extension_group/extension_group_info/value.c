//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/extension_group_info/value.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension_group/extension_group_info/extension_group_info.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value_merge.h"
#include "ten_utils/value/value_object.h"

ten_shared_ptr_t *ten_extension_group_info_from_value(
    ten_value_t *value, ten_list_t *extension_groups_info, ten_error_t *err) {
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(extension_groups_info, "Should not happen.");

  const char *app_uri = ten_value_object_peek_string(value, TEN_STR_APP);
  const char *graph_id = ten_value_object_peek_string(value, TEN_STR_GRAPH);
  const char *addon_name = ten_value_object_peek_string(value, TEN_STR_ADDON);
  const char *instance_name = ten_value_object_peek_string(value, TEN_STR_NAME);

  ten_shared_ptr_t *self = get_extension_group_info_in_extension_groups_info(
      extension_groups_info, app_uri, graph_id, addon_name, instance_name, NULL,
      err);
  if (!self) {
    return NULL;
  }

  ten_extension_group_info_t *extension_info = ten_shared_ptr_get_data(self);
  TEN_ASSERT(extension_info, "Should not happen.");
  TEN_ASSERT(ten_extension_group_info_check_integrity(extension_info),
             "Should not happen.");

  // Parse 'prop'
  ten_value_t *props_value = ten_value_object_peek(value, TEN_STR_PROPERTY);
  if (props_value) {
    if (!ten_value_is_object(props_value)) {
      // Indicates an error.
      TEN_ASSERT(0,
                 "Failed to parse 'prop' in 'start_graph' command, it's not an "
                 "object.");
      return NULL;
    }

    ten_value_object_merge_with_clone(extension_info->property, props_value);
  }

  return NULL;
}
