//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/extension_info/value.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension/extension_info/json.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/value.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion_context.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_merge.h"
#include "ten_utils/value/value_object.h"

// Parse the following snippet.
//
// ------------------------
// "name": "...",
// "dest": [{
//   "app": "...",
//   "extension_group": "...",
//   "extension": "...",
//   "msg_conversion": {
//   }
// }]
// ------------------------
static bool parse_msg_dest_value(ten_value_t *value,
                                 ten_list_t *extensions_info,
                                 ten_hashtable_t *static_dests,
                                 ten_extension_info_t *src_extension_info,
                                 ten_error_t *err) {
  TEN_ASSERT(value && ten_value_is_array(value) && extensions_info,
             "Should not happen.");
  TEN_ASSERT(static_dests, "Invalid argument.");
  TEN_ASSERT(src_extension_info,
             "src_extension must be specified in this case.");

  ten_value_array_foreach(value, iter) {
    ten_value_t *item_value = ten_ptr_listnode_get(iter.node);
    if (!ten_value_is_object(item_value)) {
      return false;
    }

    ten_msg_dest_info_t *msg_dest = ten_msg_dest_info_from_value(
        item_value, extensions_info, src_extension_info, err);
    if (!msg_dest) {
      return false;
    }

    ten_hashtable_add_string(
        static_dests, &msg_dest->hh_in_all_msg_type_dest_info,
        ten_string_get_raw_str(&msg_dest->name), ten_msg_dest_info_destroy);
  }

  return true;
}

static bool parse_msg_conversions_value(
    ten_value_t *value, ten_extension_info_t *src_extension_info,
    const char *msg_name, ten_list_t *msg_conversions, ten_error_t *err) {
  TEN_ASSERT(src_extension_info, "Should not happen.");
  TEN_ASSERT(msg_name, "Should not happen.");

  ten_msg_conversion_context_t *msg_conversion =
      ten_msg_conversion_context_from_value(value, src_extension_info, msg_name,
                                            err);
  TEN_ASSERT(msg_conversion &&
                 ten_msg_conversion_context_check_integrity(msg_conversion),
             "Should not happen.");
  if (!msg_conversion) {
    return false;
  }

  return ten_msg_conversion_context_merge(msg_conversions, msg_conversion, err);
}

ten_shared_ptr_t *ten_extension_info_node_from_value(
    ten_value_t *value, ten_list_t *extensions_info, ten_error_t *err) {
  TEN_ASSERT(value && extensions_info, "Invalid argument.");

  const char *app_uri = ten_value_object_peek_string(value, TEN_STR_APP);
  const char *graph_id = ten_value_object_peek_string(value, TEN_STR_GRAPH);
  const char *extension_group_name =
      ten_value_object_peek_string(value, TEN_STR_EXTENSION_GROUP);
  const char *addon_name = ten_value_object_peek_string(value, TEN_STR_ADDON);
  const char *instance_name = ten_value_object_peek_string(value, TEN_STR_NAME);

  ten_shared_ptr_t *self = get_extension_info_in_extensions_info(
      extensions_info, app_uri, graph_id, extension_group_name, addon_name,
      instance_name, false, err);
  if (!self) {
    return NULL;
  }

  ten_extension_info_t *extension_info = ten_shared_ptr_get_data(self);
  TEN_ASSERT(ten_extension_info_check_integrity(extension_info, true),
             "Should not happen.");

  // Parse 'prop'
  ten_value_t *props_value = ten_value_object_peek(value, TEN_STR_PROPERTY);
  if (props_value) {
    if (!ten_value_is_object(props_value)) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "The `property` in graph node should be an object.");
      } else {
        TEN_ASSERT(0, "The `property` in graph node should be an object.");
      }

      return NULL;
    }

    ten_value_object_merge_with_clone(extension_info->property, props_value);
  }

  return self;
}

ten_shared_ptr_t *ten_extension_info_parse_connection_src_part_from_value(
    ten_value_t *value, ten_list_t *extensions_info, ten_error_t *err) {
  TEN_ASSERT(value && extensions_info, "Invalid argument.");

  const char *app_uri = ten_value_object_peek_string(value, TEN_STR_APP);
  const char *graph_id = ten_value_object_peek_string(value, TEN_STR_GRAPH);

  const char *extension_name =
      ten_value_object_peek_string(value, TEN_STR_EXTENSION);
  if (!extension_name || ten_c_string_is_empty(extension_name)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_GRAPH,
                    "The extension in connection is required.");
    } else {
      TEN_ASSERT(0, "The extension in connection is required.");
    }

    return NULL;
  }

  ten_shared_ptr_t *self = get_extension_info_in_extensions_info(
      extensions_info, app_uri, graph_id, NULL, NULL, extension_name, true,
      err);
  if (!self) {
    return NULL;
  }

  ten_extension_info_t *extension_info = ten_shared_ptr_get_data(self);
  TEN_ASSERT(ten_extension_info_check_integrity(extension_info, true),
             "Should not happen.");

  // Parse 'cmd'
  ten_value_t *cmds_value = ten_value_object_peek(value, TEN_STR_CMD);
  if (cmds_value) {
    if (!parse_msg_dest_value(cmds_value, extensions_info,
                              &extension_info->msg_dest_info.cmd,
                              extension_info, err)) {
      return NULL;
    }
  }

  // Parse 'data'
  ten_value_t *data_value = ten_value_object_peek(value, TEN_STR_DATA);
  if (data_value) {
    if (!parse_msg_dest_value(data_value, extensions_info,
                              &extension_info->msg_dest_info.data,
                              extension_info, err)) {
      return NULL;
    }
  }

  // Parse 'video_frame'
  ten_value_t *video_frame_value =
      ten_value_object_peek(value, TEN_STR_VIDEO_FRAME);
  if (video_frame_value) {
    if (!parse_msg_dest_value(video_frame_value, extensions_info,
                              &extension_info->msg_dest_info.video_frame,
                              extension_info, err)) {
      return NULL;
    }
  }

  // Parse 'audio_frame'
  ten_value_t *audio_frame_value =
      ten_value_object_peek(value, TEN_STR_AUDIO_FRAME);
  if (audio_frame_value) {
    if (!parse_msg_dest_value(audio_frame_value, extensions_info,
                              &extension_info->msg_dest_info.audio_frame,
                              extension_info, err)) {
      return NULL;
    }
  }

  return self;
}

ten_shared_ptr_t *ten_extension_info_parse_connection_dest_part_from_value(
    ten_value_t *value, ten_list_t *extensions_info,
    ten_extension_info_t *src_extension_info, const char *origin_cmd_name,
    ten_error_t *err) {
  TEN_ASSERT(value && extensions_info, "Should not happen.");

  const char *app_uri = ten_value_object_peek_string(value, TEN_STR_APP);
  const char *graph_id = ten_value_object_peek_string(value, TEN_STR_GRAPH);

  const char *extension_name =
      ten_value_object_peek_string(value, TEN_STR_EXTENSION);
  if (!extension_name || ten_c_string_is_empty(extension_name)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_GRAPH,
                    "The extension in connection is required.");
    } else {
      TEN_ASSERT(0, "The extension in connection is required.");
    }
    return NULL;
  }

  ten_shared_ptr_t *self = get_extension_info_in_extensions_info(
      extensions_info, app_uri, graph_id, NULL, NULL, extension_name, true,
      err);
  if (!self) {
    return NULL;
  }

  ten_extension_info_t *extension_info = ten_shared_ptr_get_data(self);
  TEN_ASSERT(ten_extension_info_check_integrity(extension_info, true),
             "Should not happen.");

  // Parse 'msg_conversions'
  ten_value_t *msg_conversions_value =
      ten_value_object_peek(value, TEN_STR_MSG_CONVERSION);
  if (msg_conversions_value) {
    if (!parse_msg_conversions_value(
            msg_conversions_value, src_extension_info, origin_cmd_name,
            &extension_info->msg_conversion_contexts, err)) {
      TEN_ASSERT(0, "Should not happen.");
      return NULL;
    }
  }

  return self;
}

ten_value_t *ten_extension_info_node_to_value(ten_extension_info_t *self,
                                              TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe cross-thread
  // access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Should not happen.");

  // Convert the extension info into ten_value_t, which is an object-type value
  // and the snippet is as follows:
  //
  // ------------------------
  // {
  //   "type": "extension",
  //   "name": "...",
  //   "addon": "...",
  //   "extension_group": "...",
  //   "graph": "...",
  //   "app": "...",
  //   "property": {
  //     ...
  //   }
  // }
  // ------------------------
  ten_list_t kv_list = TEN_LIST_INIT_VAL;

  ten_value_t *type_value = ten_value_create_string(TEN_STR_EXTENSION);
  TEN_ASSERT(type_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_TYPE, type_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *name_value = ten_value_create_string(
      ten_string_get_raw_str(&self->loc.extension_name));
  TEN_ASSERT(name_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_NAME, name_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *addon_value = ten_value_create_string(
      ten_string_get_raw_str(&self->extension_addon_name));
  TEN_ASSERT(addon_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_ADDON, addon_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *extension_group_name_value = ten_value_create_string(
      ten_string_get_raw_str(&self->extension_group_name));
  TEN_ASSERT(extension_group_name_value, "Should not happen.");
  ten_list_push_ptr_back(
      &kv_list,
      ten_value_kv_create(TEN_STR_EXTENSION_GROUP, extension_group_name_value),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *graph_id_value =
      ten_value_create_string(ten_string_get_raw_str(&self->loc.graph_id));
  TEN_ASSERT(graph_id_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_GRAPH, graph_id_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *app_uri_value =
      ten_value_create_string(ten_string_get_raw_str(&self->loc.app_uri));
  TEN_ASSERT(app_uri_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_APP, app_uri_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  if (self->property) {
    ten_list_push_ptr_back(
        &kv_list, ten_value_kv_create(TEN_STR_PROPERTY, self->property),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy_key_only);
  }

  ten_value_t *result = ten_value_create_object_with_move(&kv_list);
  TEN_ASSERT(result, "Should not happen.");

  ten_list_clear(&kv_list);

  return result;
}

static ten_value_t *pack_msg_dest(ten_extension_info_t *self,
                                  ten_hashtable_t *msg_dests,
                                  ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Should not happen.");

  ten_list_t dest_list = TEN_LIST_INIT_VAL;

  ten_hashtable_foreach(msg_dests, iter) {
    ten_hashhandle_t *hh = iter.node;
    ten_msg_dest_info_t *msg_dest =
        CONTAINER_OF_FROM_OFFSET(hh, msg_dests->hh_offset);
    TEN_ASSERT(msg_dest, "Should not happen.");
    TEN_ASSERT(ten_msg_dest_info_check_integrity(msg_dest),
               "Should not happen.");

    ten_value_t *msg_dest_value =
        ten_msg_dest_info_to_value(msg_dest, self, err);
    if (!msg_dest_value) {
      return NULL;
    }

    ten_list_push_ptr_back(&dest_list, msg_dest_value,
                           (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
  }

  ten_value_t *result = ten_value_create_array_with_move(&dest_list);
  TEN_ASSERT(result, "Should not happen.");

  ten_list_clear(&dest_list);

  return result;
}

ten_value_t *ten_extension_info_connection_to_value(ten_extension_info_t *self,
                                                    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Should not happen.");

  if (ten_hashtable_is_empty(&self->msg_dest_info.cmd) &&
      ten_hashtable_is_empty(&self->msg_dest_info.data) &&
      ten_hashtable_is_empty(&self->msg_dest_info.video_frame) &&
      ten_hashtable_is_empty(&self->msg_dest_info.audio_frame) &&
      ten_list_is_empty(&self->msg_conversion_contexts)) {
    return NULL;
  }

  // Convert the extension info connections into ten_value_t, which is an
  // object-type value and the snippet is as follows:
  //
  // ------------------------
  // {
  //   "app": "...",
  //   "graph": "...",
  //   "extension_group": "...",
  //   "extension": "...",
  //   "cmd": [
  //     ...
  //   ],
  //   "data": [
  //     ...
  //   ],
  //   "video_frame": [
  //     ...
  //   ],
  //   "audio_frame": [
  //     ...
  //   ]
  // }
  ten_list_t kv_list = TEN_LIST_INIT_VAL;

  ten_value_t *app_uri_value =
      ten_value_create_string(ten_string_get_raw_str(&self->loc.app_uri));
  TEN_ASSERT(app_uri_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_APP, app_uri_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *graph_id_value =
      ten_value_create_string(ten_string_get_raw_str(&self->loc.graph_id));
  TEN_ASSERT(graph_id_value, "Should not happen.");
  ten_list_push_ptr_back(&kv_list,
                         ten_value_kv_create(TEN_STR_GRAPH, graph_id_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *extension_group_value = ten_value_create_string(
      ten_string_get_raw_str(&self->extension_group_name));
  TEN_ASSERT(extension_group_value, "Should not happen.");
  ten_list_push_ptr_back(
      &kv_list,
      ten_value_kv_create(TEN_STR_EXTENSION_GROUP, extension_group_value),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_value_t *extension_value = ten_value_create_string(
      ten_string_get_raw_str(&self->loc.extension_name));
  TEN_ASSERT(extension_value, "Should not happen.");
  ten_list_push_ptr_back(
      &kv_list, ten_value_kv_create(TEN_STR_EXTENSION, extension_value),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  // Parse 'cmd'
  if (!ten_hashtable_is_empty(&self->msg_dest_info.cmd)) {
    ten_value_t *cmd_dest_value =
        pack_msg_dest(self, &self->msg_dest_info.cmd, err);
    if (!cmd_dest_value) {
      return NULL;
    }

    ten_list_push_ptr_back(
        &kv_list, ten_value_kv_create(TEN_STR_CMD, cmd_dest_value),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  // Parse 'data'
  if (!ten_hashtable_is_empty(&self->msg_dest_info.data)) {
    ten_value_t *data_dest_value =
        pack_msg_dest(self, &self->msg_dest_info.data, err);
    if (!data_dest_value) {
      return NULL;
    }

    ten_list_push_ptr_back(
        &kv_list, ten_value_kv_create(TEN_STR_DATA, data_dest_value),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  // Parse 'video_frame'
  if (!ten_hashtable_is_empty(&self->msg_dest_info.video_frame)) {
    ten_value_t *video_frame_dest_value =
        pack_msg_dest(self, &self->msg_dest_info.video_frame, err);
    if (!video_frame_dest_value) {
      return NULL;
    }

    ten_list_push_ptr_back(
        &kv_list,
        ten_value_kv_create(TEN_STR_VIDEO_FRAME, video_frame_dest_value),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  // Parse 'audio_frame'
  if (!ten_hashtable_is_empty(&self->msg_dest_info.audio_frame)) {
    ten_value_t *audio_frame_dest_value =
        pack_msg_dest(self, &self->msg_dest_info.audio_frame, err);
    if (!audio_frame_dest_value) {
      return NULL;
    }

    ten_list_push_ptr_back(
        &kv_list,
        ten_value_kv_create(TEN_STR_AUDIO_FRAME, audio_frame_dest_value),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  ten_value_t *result = ten_value_create_object_with_move(&kv_list);
  TEN_ASSERT(result, "Should not happen.");

  ten_list_clear(&kv_list);

  return result;
}
