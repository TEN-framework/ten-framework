//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion_context.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/container/list_node_smart_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

static ten_extension_info_t *ten_extension_info_create(void) {
  ten_extension_info_t *self =
      (ten_extension_info_t *)TEN_MALLOC(sizeof(ten_extension_info_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_EXTENSION_INFO_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  TEN_STRING_INIT(self->extension_addon_name);
  TEN_STRING_INIT(self->extension_group_name);

  ten_loc_init_empty(&self->loc);

  self->property = ten_value_create_object_with_move(NULL);

  ten_list_init(&self->msg_conversion_contexts);

  ten_all_msg_type_dest_info_init(&self->msg_dest_info);

  return self;
}

static bool ten_extension_info_is_specified_extension(
    ten_extension_info_t *self, const char *app_uri, const char *graph_id,
    const char *extension_name) {
  TEN_ASSERT(
      self && ten_extension_info_check_integrity(self, true) && extension_name,
      "Should not happen.");

  if (app_uri && !ten_string_is_equal_c_str(&self->loc.app_uri, app_uri)) {
    return false;
  }

  if (graph_id && !ten_string_is_equal_c_str(&self->loc.graph_id, graph_id)) {
    return false;
  }

  if (extension_name &&
      !ten_string_is_equal_c_str(&self->loc.extension_name, extension_name)) {
    return false;
  }

  return true;
}

static void ten_extension_info_destroy(ten_extension_info_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In TEN, the destroy operation should only be allowed to be
  // invoked when there are no thread safety issues present.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Invalid use of extension_info %p.", self);

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_signature_set(&self->signature, 0);

  ten_all_msg_type_dest_info_deinit(&self->msg_dest_info);

  ten_string_deinit(&self->extension_addon_name);
  ten_string_deinit(&self->extension_group_name);

  ten_loc_deinit(&self->loc);

  if (self->property) {
    ten_value_destroy(self->property);
  }
  ten_list_clear(&self->msg_conversion_contexts);

  TEN_FREE(self);
}

// 1. All extension_info will be stored in the `extensions_info`, only including
//    those defined in the `nodes` section. Any extension_info used in
//    `connections` must be declared in the `nodes`.
//
// 2. All extension_info in the `extensions_info` are unique, which is
//    identified by the `loc` field.
//
// 3. Each extension_info in the `extensions_info` is a shared_ptr, and if one
//    is used in the `dest` section, a weak_ptr will be created to reference it
//    to avoid there is a cycle in the graph.
//
// Parameters:
//
// - should_exist: the extension_info should be found in `extensions_info` if
//   true. If we are parsing the `nodes` section, it should be false. And if we
//   are parsing the `connections` section, it should be true.
ten_shared_ptr_t *get_extension_info_in_extensions_info(
    ten_list_t *extensions_info, const char *app_uri, const char *graph_id,
    const char *extension_group_name, const char *extension_addon_name,
    const char *extension_instance_name, bool should_exist, ten_error_t *err) {
  TEN_ASSERT(extensions_info && extension_instance_name, "Should not happen.");

  if (!should_exist) {
    TEN_ASSERT(
        extension_addon_name,
        "Expect to be a create request, the extension_addon_name is required.");
  }

  if (!app_uri) {
    // We use "" to represent 'this' app.
    app_uri = "";
  }

  if (!graph_id) {
    // We use "" to represent 'this' graph.
    graph_id = "";
  }

  ten_extension_info_t *extension_info = NULL;

  // Find the corresponding extension_info according to the instance name of
  // extension.
  ten_listnode_t *extension_info_node = ten_list_find_shared_ptr_custom_3(
      extensions_info, app_uri, graph_id, extension_instance_name,
      ten_extension_info_is_specified_extension);

  if (extension_info_node) {
    extension_info = ten_shared_ptr_get_data(
        ten_smart_ptr_listnode_get(extension_info_node));
    TEN_ASSERT(extension_info &&
                   ten_extension_info_check_integrity(extension_info, true),
               "Should not happen.");

    // The extension addon name should be equal if both specified.
    if (extension_addon_name && !ten_c_string_is_empty(extension_addon_name) &&
        !ten_string_is_equal_c_str(&extension_info->extension_addon_name,
                                   extension_addon_name)) {
      if (err) {
        ten_error_set(
            err, TEN_ERROR_CODE_INVALID_GRAPH,
            "extension '%s' is associated with different addon '%s', "
            "'%s'",
            extension_instance_name, extension_addon_name,
            ten_string_get_raw_str(&extension_info->extension_addon_name));
      } else {
        TEN_ASSERT(
            0,
            "extension '%s' is associated with different addon '%s', "
            "'%s'",
            extension_instance_name, extension_addon_name,
            ten_string_get_raw_str(&extension_info->extension_addon_name));
      }
      return NULL;
    }

    if (!should_exist) {
      if (extension_info_node) {
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_GRAPH,
                        "The extension_info is duplicated, extension : %s",
                        extension_instance_name);
        } else {
          TEN_ASSERT(0, "The extension_info is duplicated, extension: %s",
                     extension_instance_name);
        }

        return NULL;
      }
    }

    return ten_smart_ptr_listnode_get(extension_info_node);
  } else {
    if (should_exist) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_INVALID_GRAPH,
                      "The extension_info is not found, extension: %s.",
                      extension_instance_name);
      } else {
        TEN_ASSERT(0, "The extension_info is not found, extension: %s.",
                   extension_instance_name);
      }

      return NULL;
    }
  }

  ten_extension_info_t *self = ten_extension_info_create();

  ten_string_set_formatted(&self->extension_addon_name, "%s",
                           extension_addon_name);

  const char *extension_group_name_str =
      extension_group_name ? extension_group_name : TEN_STR_UNSPECIFIED_GROUP;
  ten_string_set_formatted(&self->extension_group_name, "%s",
                           extension_group_name_str);

  ten_loc_set(&self->loc, app_uri, graph_id, extension_instance_name);

  ten_shared_ptr_t *shared_self =
      ten_shared_ptr_create(self, ten_extension_info_destroy);
  ten_shared_ptr_t *shared_self_ =
      ten_list_push_smart_ptr_back(extensions_info, shared_self);
  ten_shared_ptr_destroy(shared_self);

  return shared_self_;
}

static bool copy_msg_dest(ten_hashtable_t *to_static_info,
                          ten_hashtable_t *from_static_info,
                          ten_list_t *extensions_info, ten_error_t *err) {
  TEN_ASSERT(to_static_info && extensions_info, "Should not happen.");

  ten_hashtable_foreach(from_static_info, iter) {
    ten_hashhandle_t *hh = iter.node;
    ten_msg_dest_info_t *msg_dest_static_info =
        CONTAINER_OF_FROM_OFFSET(hh, from_static_info->hh_offset);
    TEN_ASSERT(msg_dest_static_info, "Should not happen.");
    TEN_ASSERT(ten_msg_dest_info_check_integrity(msg_dest_static_info),
               "Should not happen.");

    ten_msg_dest_info_t *new_msg_dest_static_info =
        ten_msg_dest_info_clone(msg_dest_static_info, extensions_info, err);
    if (!new_msg_dest_static_info) {
      return false;
    }

    ten_hashtable_add_string(
        to_static_info, &new_msg_dest_static_info->hh_in_all_msg_type_dest_info,
        ten_string_get_raw_str(&new_msg_dest_static_info->name),
        ten_msg_dest_info_destroy);
  }

  return true;
}

static ten_shared_ptr_t *ten_extension_info_clone_except_dest(
    ten_extension_info_t *self, ten_list_t *extensions_info, ten_error_t *err) {
  TEN_ASSERT(extensions_info, "Should not happen.");

  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Invalid use of extension_info %p.", self);

  ten_shared_ptr_t *new_dest = get_extension_info_in_extensions_info(
      extensions_info, ten_string_get_raw_str(&self->loc.app_uri),
      ten_string_get_raw_str(&self->loc.graph_id),
      ten_string_get_raw_str(&self->extension_group_name),
      ten_string_get_raw_str(&self->extension_addon_name),
      ten_string_get_raw_str(&self->loc.extension_name),
      /* should_exist = */ false, err);
  TEN_ASSERT(new_dest, "Should not happen.");

  ten_extension_info_t *new_extension_info = ten_shared_ptr_get_data(new_dest);
  TEN_ASSERT(new_extension_info, "Should not happen.");
  TEN_ASSERT(ten_extension_info_check_integrity(new_extension_info, true),
             "Should not happen.");

  ten_value_object_merge_with_clone(new_extension_info->property,
                                    self->property);

  ten_list_foreach (&self->msg_conversion_contexts, iter) {
    ten_msg_conversion_context_t *msg_conversion =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(msg_conversion &&
                   ten_msg_conversion_context_check_integrity(msg_conversion),
               "Should not happen.");

    bool rc = ten_msg_conversion_context_merge(
        &new_extension_info->msg_conversion_contexts, msg_conversion, err);
    TEN_ASSERT(rc, "Should not happen.");
  }

  return new_dest;
}

static ten_shared_ptr_t *ten_extension_info_clone_dest(
    ten_extension_info_t *self, ten_list_t *extensions_info, ten_error_t *err) {
  TEN_ASSERT(extensions_info, "Should not happen.");

  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Invalid use of extension_info %p.", self);

  ten_shared_ptr_t *exist_dest = get_extension_info_in_extensions_info(
      extensions_info, ten_string_get_raw_str(&self->loc.app_uri),
      ten_string_get_raw_str(&self->loc.graph_id),
      ten_string_get_raw_str(&self->extension_group_name),
      ten_string_get_raw_str(&self->extension_addon_name),
      ten_string_get_raw_str(&self->loc.extension_name),
      /* should_exist = */ true, err);
  TEN_ASSERT(exist_dest, "Should not happen.");

  ten_extension_info_t *exist_extension_info =
      ten_shared_ptr_get_data(exist_dest);
  TEN_ASSERT(exist_extension_info, "Should not happen.");
  TEN_ASSERT(ten_extension_info_check_integrity(exist_extension_info, true),
             "Should not happen.");

  if (!copy_msg_dest(&exist_extension_info->msg_dest_info.cmd,
                     &self->msg_dest_info.cmd, extensions_info, err)) {
    return NULL;
  }

  if (!copy_msg_dest(&exist_extension_info->msg_dest_info.data,
                     &self->msg_dest_info.data, extensions_info, err)) {
    return NULL;
  }

  if (!copy_msg_dest(&exist_extension_info->msg_dest_info.audio_frame,
                     &self->msg_dest_info.audio_frame, extensions_info, err)) {
    return NULL;
  }

  if (!copy_msg_dest(&exist_extension_info->msg_dest_info.video_frame,
                     &self->msg_dest_info.video_frame, extensions_info, err)) {
    return NULL;
  }

  return exist_dest;
}

bool ten_extensions_info_clone(ten_list_t *from, ten_list_t *to,
                               ten_error_t *err) {
  TEN_ASSERT(from && to, "Should not happen.");

  // `ten_extension_info_clone_except_dest()` will call
  // `get_extension_info_in_extensions_info()`. In
  // `get_extension_info_in_extensions_info()`, we need to determine if
  // `extension_info` exists in `extensions_info`. Therefore, we should first
  // clone the `nodes` and then proceed to clone the `connections`.
  ten_list_foreach (from, iter) {
    ten_extension_info_t *extension_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    if (!ten_extension_info_clone_except_dest(extension_info, to, err)) {
      return false;
    }
  }

  ten_list_foreach (from, iter) {
    ten_extension_info_t *extension_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    if (!ten_extension_info_clone_dest(extension_info, to, err)) {
      return false;
    }
  }

  return true;
}

bool ten_extension_info_check_integrity(ten_extension_info_t *self,
                                        bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_INFO_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

ten_extension_info_t *ten_extension_info_from_smart_ptr(
    ten_smart_ptr_t *extension_info_smart_ptr) {
  TEN_ASSERT(extension_info_smart_ptr, "Invalid argument.");
  return ten_smart_ptr_get_data(extension_info_smart_ptr);
}

static void ten_extension_info_fill_app_uri(ten_extension_info_t *self,
                                            const char *app_uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Invalid argument.");
  TEN_ASSERT(app_uri, "Should not happen.");
  TEN_ASSERT(!ten_loc_is_empty(&self->loc), "Should not happen.");

  // Fill the app uri of the extension_info if it is empty.
  if (ten_string_is_empty(&self->loc.app_uri)) {
    ten_string_set_formatted(&self->loc.app_uri, "%s", app_uri);
  }

  // Fill the app uri of each item in the msg_conversions_list if it is empty.
  ten_list_foreach (&self->msg_conversion_contexts, iter) {
    ten_msg_conversion_context_t *conversion_iter =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(conversion_iter &&
                   ten_msg_conversion_context_check_integrity(conversion_iter),
               "Should not happen.");

    if (ten_string_is_empty(&conversion_iter->src_loc.app_uri)) {
      ten_string_set_formatted(&conversion_iter->src_loc.app_uri, "%s",
                               app_uri);
    }
  }
}

// Fill the app uri of each extension_info in the extensions_info.
void ten_extensions_info_fill_app_uri(ten_list_t *extensions_info,
                                      const char *app_uri) {
  ten_list_foreach (extensions_info, iter) {
    ten_extension_info_t *extension_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    TEN_ASSERT(extension_info, "Invalid argument.");
    TEN_ASSERT(ten_extension_info_check_integrity(extension_info, false),
               "Invalid argument.");

    ten_extension_info_fill_app_uri(extension_info, app_uri);
  }
}

static void ten_extension_info_fill_loc_info(ten_extension_info_t *self,
                                             const char *app_uri,
                                             const char *graph_id) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Invalid use of extension_info %p.", self);

  if (ten_string_is_empty(&self->loc.graph_id)) {
    ten_string_set_formatted(&self->loc.graph_id, "%s", graph_id);
  }

  if (ten_string_is_empty(&self->loc.app_uri)) {
    ten_string_set_formatted(&self->loc.app_uri, app_uri);
  }

  // Fill the app uri of each item in the msg_conversions_list if it is empty.
  ten_list_foreach (&self->msg_conversion_contexts, iter) {
    ten_msg_conversion_context_t *conversion_iter =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(conversion_iter &&
                   ten_msg_conversion_context_check_integrity(conversion_iter),
               "Should not happen.");

    if (ten_string_is_empty(&conversion_iter->src_loc.graph_id)) {
      ten_string_set_formatted(&conversion_iter->src_loc.graph_id, "%s",
                               graph_id);
    }

    if (ten_string_is_empty(&self->loc.app_uri)) {
      ten_string_set_formatted(&self->loc.app_uri, app_uri);
    }
  }
}

void ten_extensions_info_fill_loc_info(ten_list_t *extensions_info,
                                       const char *app_uri,
                                       const char *graph_id) {
  ten_list_foreach (extensions_info, iter) {
    ten_extension_info_t *extension_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    TEN_ASSERT(extension_info, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: The graph-related information of the extension remains
    // unchanged during the lifecycle of engine/graph, allowing safe
    // cross-thread access.
    TEN_ASSERT(ten_extension_info_check_integrity(extension_info, false),
               "Invalid argument.");

    ten_extension_info_fill_loc_info(extension_info, app_uri, graph_id);
  }

  // Check if the extension_info in the `dest` section is correct.
  ten_list_foreach (extensions_info, iter) {
    ten_extension_info_t *extension_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));

    ten_hashtable_foreach(&extension_info->msg_dest_info.cmd, iter) {
      ten_hashhandle_t *hh = iter.node;
      ten_msg_dest_info_t *dest_info = CONTAINER_OF_FROM_OFFSET(
          hh, extension_info->msg_dest_info.cmd.hh_offset);
      TEN_ASSERT(dest_info, "Should not happen.");
      TEN_ASSERT(ten_msg_dest_info_check_integrity(dest_info),
                 "Should not happen.");

      ten_list_foreach (&dest_info->dest, dest_iter) {
        ten_extension_info_t *dest_extension_info =
            ten_smart_ptr_get_data(ten_smart_ptr_listnode_get(dest_iter.node));
        TEN_ASSERT(dest_extension_info, "Should not happen.");
        TEN_ASSERT(
            ten_extension_info_check_integrity(dest_extension_info, true),
            "Should not happen.");

        if (ten_string_is_equal_c_str(&dest_extension_info->loc.app_uri,
                                      TEN_STR_LOCALHOST)) {
          TEN_ASSERT(0, "extension_info->loc.app_uri should not be localhost.");
          return;
        }

        if (ten_string_is_empty(&dest_extension_info->loc.graph_id)) {
          TEN_ASSERT(0, "extension_info->loc.graph_id should not be empty.");
          return;
        }
      }
    }
  }
}
