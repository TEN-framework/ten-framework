//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/extension_group_info/extension_group_info.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/common/loc.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

bool ten_extension_group_info_check_integrity(
    ten_extension_group_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_GROUP_INFO_SIGNATURE) {
    return false;
  }

  return true;
}

ten_extension_group_info_t *ten_extension_group_info_create(void) {
  ten_extension_group_info_t *self = (ten_extension_group_info_t *)TEN_MALLOC(
      sizeof(ten_extension_group_info_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_EXTENSION_GROUP_INFO_SIGNATURE);

  TEN_STRING_INIT(self->extension_group_addon_name);

  ten_loc_init_empty(&self->loc);
  self->property = ten_value_create_object_with_move(NULL);

  return self;
}

void ten_extension_group_info_destroy(ten_extension_group_info_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_info_check_integrity(self),
             "Invalid use of extension_info %p.", self);

  ten_signature_set(&self->signature, 0);

  ten_string_deinit(&self->extension_group_addon_name);

  ten_loc_deinit(&self->loc);

  if (self->property) {
    ten_value_destroy(self->property);
  }

  TEN_FREE(self);
}

static bool ten_extension_group_info_is_specified_extension_group(
    ten_extension_group_info_t *self, const char *app_uri, const char *graph_id,
    const char *extension_group_name) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension group remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_group_info_check_integrity(self),
             "Invalid use of extension_group_info %p.", self);

  TEN_ASSERT(extension_group_name, "Invalid argument.");

  if (app_uri && !ten_string_is_equal_c_str(&self->loc.app_uri, app_uri)) {
    return false;
  }

  if (graph_id && !ten_string_is_equal_c_str(&self->loc.graph_id, graph_id)) {
    return false;
  }

  if (extension_group_name &&
      !ten_string_is_equal_c_str(&self->loc.extension_group_name,
                                 extension_group_name)) {
    return false;
  }

  return true;
}

ten_extension_group_info_t *ten_extension_group_info_from_smart_ptr(
    ten_smart_ptr_t *extension_group_info_smart_ptr) {
  TEN_ASSERT(extension_group_info_smart_ptr, "Invalid argument.");
  return ten_smart_ptr_get_data(extension_group_info_smart_ptr);
}

ten_shared_ptr_t *get_extension_group_info_in_extension_groups_info(
    ten_list_t *extension_groups_info, const char *app_uri,
    const char *graph_id, const char *extension_group_addon_name,
    const char *extension_group_instance_name, bool *new_one_created,
    ten_error_t *err) {
  TEN_ASSERT(extension_groups_info, "Should not happen.");
  TEN_ASSERT(
      extension_group_instance_name && strlen(extension_group_instance_name),
      "Invalid argument.");

  ten_extension_group_info_t *extension_group_info = NULL;

  // Find the corresponding extension_group_info according to the instance name
  // of extension_group only. Check if there are any other extension groups with
  // different extension_group settings. This step is to find if there are any
  // other extension groups with different extension_group addon name.
  ten_listnode_t *extension_group_info_node = ten_list_find_shared_ptr_custom_3(
      extension_groups_info, app_uri, graph_id, extension_group_instance_name,
      ten_extension_group_info_is_specified_extension_group);
  if (extension_group_info_node) {
    extension_group_info = ten_shared_ptr_get_data(
        ten_smart_ptr_listnode_get(extension_group_info_node));
    TEN_ASSERT(extension_group_info && ten_extension_group_info_check_integrity(
                                           extension_group_info),
               "Should not happen.");

    if (new_one_created) {
      *new_one_created = false;
    }

    if (strlen(extension_group_addon_name) &&
        !ten_string_is_empty(
            &extension_group_info->extension_group_addon_name) &&
        !ten_string_is_equal_c_str(
            &extension_group_info->extension_group_addon_name,
            extension_group_addon_name)) {
      if (err) {
        ten_error_set(
            err, TEN_ERROR_CODE_INVALID_GRAPH,
            "extension group '%s' is associated with different addon '%s', "
            "'%s'",
            extension_group_instance_name, extension_group_addon_name,
            ten_string_get_raw_str(
                &extension_group_info->extension_group_addon_name));
      } else {
        TEN_ASSERT(0,
                   "extension group '%s' is associated with different addon "
                   "'%s', '%s'",
                   extension_group_instance_name, extension_group_addon_name,
                   ten_string_get_raw_str(
                       &extension_group_info->extension_group_addon_name));
      }

      return NULL;
    }

    // If we know the extension group addon name now, add this data to the
    // extension_info.
    if (strlen(extension_group_addon_name) &&
        ten_string_is_empty(
            &extension_group_info->extension_group_addon_name)) {
      ten_string_set_formatted(
          &extension_group_info->extension_group_addon_name, "%s",
          extension_group_addon_name);
    }

    return ten_smart_ptr_listnode_get(extension_group_info_node);
  }

  ten_extension_group_info_t *self = ten_extension_group_info_create();

  ten_loc_set(&self->loc, app_uri, graph_id, extension_group_instance_name,
              NULL);

  // Add the extension group addon name if we know it now.
  if (strlen(extension_group_addon_name)) {
    ten_string_set_formatted(&self->extension_group_addon_name, "%s",
                             extension_group_addon_name);
  }

  ten_shared_ptr_t *shared_self =
      ten_shared_ptr_create(self, ten_extension_group_info_destroy);
  ten_shared_ptr_t *shared_self_ =
      ten_list_push_smart_ptr_back(extension_groups_info, shared_self);
  ten_shared_ptr_destroy(shared_self);

  if (new_one_created) {
    *new_one_created = true;
  }
  return shared_self_;
}

ten_shared_ptr_t *ten_extension_group_info_clone(
    ten_extension_group_info_t *self, ten_list_t *extension_groups_info) {
  TEN_ASSERT(extension_groups_info, "Should not happen.");

  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_group_info_check_integrity(self),
             "Invalid use of extension_group_info %p.", self);

  bool new_extension_group_info_created = false;
  ten_shared_ptr_t *new_dest =
      get_extension_group_info_in_extension_groups_info(
          extension_groups_info, ten_string_get_raw_str(&self->loc.app_uri),
          ten_string_get_raw_str(&self->loc.graph_id),
          ten_string_get_raw_str(&self->extension_group_addon_name),
          ten_string_get_raw_str(&self->loc.extension_group_name),
          &new_extension_group_info_created, NULL);

  return new_dest;
}

static void ten_extension_group_info_fill_app_uri(
    ten_extension_group_info_t *self, const char *app_uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_info_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(app_uri, "Should not happen.");
  TEN_ASSERT(!ten_loc_is_empty(&self->loc), "Should not happen.");

  // Fill the app uri of the extension_info if it is empty.
  if (ten_string_is_empty(&self->loc.app_uri)) {
    ten_string_set_formatted(&self->loc.app_uri, "%s", app_uri);
  }
}

void ten_extension_groups_info_fill_app_uri(ten_list_t *extension_groups_info,
                                            const char *app_uri) {
  ten_list_foreach (extension_groups_info, iter) {
    ten_extension_group_info_t *extension_group_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    TEN_ASSERT(extension_group_info && ten_extension_group_info_check_integrity(
                                           extension_group_info),
               "Invalid argument.");

    ten_extension_group_info_fill_app_uri(extension_group_info, app_uri);
  }
}

static void ten_extension_group_info_fill_graph_id(
    ten_extension_group_info_t *self, const char *graph_id) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_info_check_integrity(self),
             "Invalid use of extension_group_info %p.", self);

  ten_string_set_formatted(&self->loc.graph_id, "%s", graph_id);
}

void ten_extension_groups_info_fill_graph_id(ten_list_t *extension_groups_info,
                                             const char *graph_id) {
  TEN_ASSERT(extension_groups_info, "Invalid argument.");
  TEN_ASSERT(ten_list_check_integrity(extension_groups_info),
             "Invalid argument.");

  ten_list_foreach (extension_groups_info, iter) {
    ten_extension_group_info_t *extension_group_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    TEN_ASSERT(extension_group_info && ten_extension_group_info_check_integrity(
                                           extension_group_info),
               "Invalid argument.");

    ten_extension_group_info_fill_graph_id(extension_group_info, graph_id);
  }
}
