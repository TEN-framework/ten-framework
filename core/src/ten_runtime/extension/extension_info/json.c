//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/extension_info/json.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/json.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion_context.h"
#include "include_internal/ten_utils/lib/json.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

static bool pack_msg_dest(ten_extension_info_t *self,
                          ten_hashtable_t *msg_dests, ten_json_t *msg_json,
                          ten_error_t *err) {
  TEN_ASSERT(msg_json, "Should not happen.");

  ten_hashtable_foreach(msg_dests, iter) {
    ten_hashhandle_t *hh = iter.node;
    ten_msg_dest_info_t *msg_dest =
        CONTAINER_OF_FROM_OFFSET(hh, msg_dests->hh_offset);
    TEN_ASSERT(msg_dest, "Should not happen.");
    TEN_ASSERT(ten_msg_dest_info_check_integrity(msg_dest),
               "Should not happen.");

    ten_json_t msg_dest_json = TEN_JSON_INIT_VAL(msg_json->ctx, false);
    ten_json_init_object(&msg_dest_json);
    ten_json_array_append(msg_json, &msg_dest_json);

    bool success =
        ten_msg_dest_info_to_json(msg_dest, self, &msg_dest_json, err);
    if (!success) {
      return false;
    }
  }

  return true;
}

bool ten_extension_info_to_json(ten_extension_info_t *self, ten_json_t *info) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Should not happen.");
  TEN_ASSERT(info, "Should not happen.");

  ten_json_object_set_string(info, TEN_STR_TYPE, TEN_STR_EXTENSION);

  ten_json_object_set_string(info, TEN_STR_NAME,
                             ten_string_get_raw_str(&self->loc.extension_name));

  ten_json_object_set_string(
      info, TEN_STR_ADDON, ten_string_get_raw_str(&self->extension_addon_name));

  ten_json_object_set_string(
      info, TEN_STR_EXTENSION_GROUP,
      ten_string_get_raw_str(&self->extension_group_name));

  // In the graph, if the 'app' and 'graph' fields appear, they should not be
  // empty. However, in extension info, an empty string ("") is used to
  // represent 'this' app and 'this' graph. Therefore, only when the string is
  // non-empty does it indicate that the original graph JSON contains these two
  // fields, and only then do we need to dump them into the JSON.
  if (self->loc.has_graph_id && !ten_string_is_empty(&self->loc.graph_id)) {
    ten_json_object_set_string(info, TEN_STR_GRAPH,
                               ten_string_get_raw_str(&self->loc.graph_id));
  }

  if (self->loc.has_app_uri && !ten_string_is_empty(&self->loc.app_uri)) {
    ten_json_object_set_string(info, TEN_STR_APP,
                               ten_string_get_raw_str(&self->loc.app_uri));
  }

  if (self->property) {
    ten_json_t property_json = TEN_JSON_INIT_VAL(info->ctx, false);

    bool success = ten_value_to_json(self->property, &property_json);
    TEN_ASSERT(success, "Should not happen.");

    ten_json_object_set(info, TEN_STR_PROPERTY, &property_json);
  }

  return true;
}

int ten_extension_info_connections_to_json(ten_extension_info_t *self,
                                           ten_json_t *json, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(self, false),
             "Should not happen.");
  TEN_ASSERT(json, "Invalid argument.");

  if (ten_hashtable_is_empty(&self->msg_dest_info.cmd) &&
      ten_hashtable_is_empty(&self->msg_dest_info.data) &&
      ten_hashtable_is_empty(&self->msg_dest_info.audio_frame) &&
      ten_hashtable_is_empty(&self->msg_dest_info.video_frame) &&
      ten_list_is_empty(&self->msg_conversion_contexts)) {
    return 0;
  }

  TEN_ASSERT(json, "Should not happen.");

  // In the graph, if the 'app' and 'graph' fields appear, they should not be
  // empty. However, in extension info, an empty string ("") is used to
  // represent 'this' app and 'this' graph. Therefore, only when the string is
  // non-empty does it indicate that the original graph JSON contains these two
  // fields, and only then do we need to dump them into the JSON.
  if (self->loc.has_app_uri && !ten_string_is_empty(&self->loc.app_uri)) {
    ten_json_object_set_string(json, TEN_STR_APP,
                               ten_string_get_raw_str(&self->loc.app_uri));
  }

  if (self->loc.has_graph_id && !ten_string_is_empty(&self->loc.graph_id)) {
    ten_json_object_set_string(json, TEN_STR_GRAPH,
                               ten_string_get_raw_str(&self->loc.graph_id));
  }

  ten_json_object_set_string(
      json, TEN_STR_EXTENSION_GROUP,
      ten_string_get_raw_str(&self->extension_group_name));

  ten_json_object_set_string(json, TEN_STR_EXTENSION,
                             ten_string_get_raw_str(&self->loc.extension_name));

  if (!ten_hashtable_is_empty(&self->msg_dest_info.cmd)) {
    ten_json_t cmd_dest_json = TEN_JSON_INIT_VAL(json->ctx, false);
    ten_json_object_peek_or_create_array(json, TEN_STR_CMD, &cmd_dest_json);

    bool success =
        pack_msg_dest(self, &self->msg_dest_info.cmd, &cmd_dest_json, err);
    if (!success) {
      return -1;
    }
  }

  if (!ten_hashtable_is_empty(&self->msg_dest_info.data)) {
    ten_json_t data_dest_json = TEN_JSON_INIT_VAL(json->ctx, false);
    ten_json_object_peek_or_create_array(json, TEN_STR_DATA, &data_dest_json);

    bool success =
        pack_msg_dest(self, &self->msg_dest_info.data, &data_dest_json, err);
    if (!success) {
      return -1;
    }
  }

  if (!ten_hashtable_is_empty(&self->msg_dest_info.video_frame)) {
    ten_json_t video_frame_dest_json = TEN_JSON_INIT_VAL(json->ctx, false);
    ten_json_object_peek_or_create_array(json, TEN_STR_VIDEO_FRAME,
                                         &video_frame_dest_json);

    bool success = pack_msg_dest(self, &self->msg_dest_info.video_frame,
                                 &video_frame_dest_json, err);
    if (!success) {
      return -1;
    }
  }

  if (!ten_hashtable_is_empty(&self->msg_dest_info.audio_frame)) {
    ten_json_t audio_frame_dest_json = TEN_JSON_INIT_VAL(json->ctx, false);
    ten_json_object_peek_or_create_array(json, TEN_STR_AUDIO_FRAME,
                                         &audio_frame_dest_json);

    bool success = pack_msg_dest(self, &self->msg_dest_info.audio_frame,
                                 &audio_frame_dest_json, err);
    if (!success) {
      return -1;
    }
  }

  return 1;
}
