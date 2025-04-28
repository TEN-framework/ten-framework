//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/msg.h"

#include <string.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/predefined_graph.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_store/extension_store.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg_info.h"
#include "include_internal/ten_runtime/schema_store/msg.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/cmd/close_app/cmd.h"
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/msg/msg.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_json.h"
#include "ten_utils/value/value_kv.h"

// Raw msg interface
bool ten_raw_msg_check_integrity(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_MSG_SIGNATURE) {
    return false;
  }
  return true;
}

void ten_raw_msg_init(ten_msg_t *self, TEN_MSG_TYPE type) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_MSG_SIGNATURE);

  self->type = type;
  ten_value_init_string(&self->name);

  ten_loc_init_empty(&self->src_loc);
  ten_list_init(&self->dest_loc);

  ten_value_init_object_with_move(&self->properties, NULL);

  ten_list_init(&self->locked_res);

  self->timestamp = 0;
}

void ten_raw_msg_deinit(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  TEN_LOGV("Destroy c_msg %p", self);

  ten_signature_set(&self->signature, 0);
  ten_value_deinit(&self->name);

  ten_loc_deinit(&self->src_loc);
  ten_list_clear(&self->dest_loc);

  ten_list_clear(&self->locked_res);

  ten_value_deinit(&self->properties);
}

void ten_raw_msg_set_src_to_loc(ten_msg_t *self, ten_loc_t *loc) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  ten_loc_set_from_loc(&self->src_loc, loc);
}

void ten_msg_set_src_to_loc(ten_shared_ptr_t *self, ten_loc_t *loc) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  ten_raw_msg_set_src_to_loc(ten_shared_ptr_get_data(self), loc);
}

// The semantics of the following function is to replace the destination
// information to one which is specified through the parameters.
static bool ten_raw_msg_clear_and_set_dest(ten_msg_t *self, const char *uri,
                                           const char *graph_id,
                                           const char *extension_group_name,
                                           const char *extension_name,
                                           TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT((uri != NULL || extension_name != NULL), "Should not happen.");

  ten_list_clear(&self->dest_loc);
  ten_list_push_ptr_back(
      &self->dest_loc,
      ten_loc_create(uri, graph_id, extension_group_name, extension_name),
      (ten_ptr_listnode_destroy_func_t)ten_loc_destroy);

  return true;
}

void ten_raw_msg_add_dest(ten_msg_t *self, const char *uri,
                          const char *graph_id,
                          const char *extension_group_name,
                          const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_list_push_ptr_back(
      &self->dest_loc,
      ten_loc_create(uri, graph_id, extension_group_name, extension_name),
      (ten_ptr_listnode_destroy_func_t)ten_loc_destroy);
}

void ten_raw_msg_clear_dest(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_list_clear(&self->dest_loc);
}

static void ten_raw_msg_clear_and_set_dest_from_msg_src(ten_msg_t *self,
                                                        ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_t *raw_msg = ten_msg_get_raw_msg(cmd);

  ten_raw_msg_clear_and_set_dest(
      self, ten_string_get_raw_str(&raw_msg->src_loc.app_uri),
      ten_string_get_raw_str(&raw_msg->src_loc.graph_id),
      ten_string_get_raw_str(&raw_msg->src_loc.extension_group_name),
      ten_string_get_raw_str(&raw_msg->src_loc.extension_name), NULL);
}

void ten_msg_clear_and_set_dest_from_msg_src(ten_shared_ptr_t *self,
                                             ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_msg_check_integrity(self) && cmd &&
                 ten_msg_check_integrity(cmd),
             "Should not happen.");

  ten_msg_t *raw_msg = ten_msg_get_raw_msg(self);
  ten_raw_msg_clear_and_set_dest_from_msg_src(raw_msg, cmd);
}

static bool ten_raw_msg_src_is_empty(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return ten_loc_is_empty(&self->src_loc);
}

const char *ten_raw_msg_get_first_dest_uri(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(ten_list_size(&self->dest_loc) == 1, "Should not happen.");

  ten_loc_t *first_loc = ten_ptr_listnode_get(ten_list_front(&self->dest_loc));
  return ten_string_get_raw_str(&first_loc->app_uri);
}

/**
 * @brief Retrieves the first destination location from a raw message.
 *
 * This function returns a pointer to the first destination location in the
 * message's destination list. The function verifies that the message has at
 * least one destination before retrieving it.
 *
 * @param self Pointer to the raw message structure.
 * @return A pointer to the first destination location (ten_loc_t).
 *
 * @note The returned pointer points to memory owned by the message and should
 *       not be freed by the caller.
 * @note This is the raw message version of ten_msg_get_first_dest_loc().
 */
ten_loc_t *ten_raw_msg_get_first_dest_loc(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  // Check if the destination list is empty.
  if (ten_list_size(&self->dest_loc) == 0) {
    TEN_ASSERT(0, "The destination list is empty.");
    return NULL;
  }

  ten_loc_t *first_loc = ten_ptr_listnode_get(ten_list_front(&self->dest_loc));
  return first_loc;
}

bool ten_msg_check_integrity(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  ten_msg_t *raw_msg = ten_shared_ptr_get_data(self);
  if (ten_raw_msg_check_integrity(raw_msg) == false) {
    return false;
  }
  return true;
}

bool ten_msg_src_is_empty(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_src_is_empty(ten_msg_get_raw_msg(self));
}

const char *ten_msg_get_first_dest_uri(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_first_dest_uri(ten_msg_get_raw_msg(self));
}

static void ten_raw_msg_set_src(ten_msg_t *self, const char *uri,
                                const char *graph_id,
                                const char *extension_group_name,
                                const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  ten_loc_set(&self->src_loc, uri, graph_id, extension_group_name,
              extension_name);
}

void ten_msg_set_src(ten_shared_ptr_t *self, const char *uri,
                     const char *graph_id, const char *extension_group_name,
                     const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  ten_raw_msg_set_src(ten_msg_get_raw_msg(self), uri, graph_id,
                      extension_group_name, extension_name);
}

void ten_msg_set_src_to_app(ten_shared_ptr_t *self, ten_app_t *app) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Should not happen.");

  ten_msg_set_src(self, ten_app_get_uri(app), NULL, NULL, NULL);
}

void ten_msg_set_src_to_engine(ten_shared_ptr_t *self, ten_engine_t *engine) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_msg_set_src(self, ten_app_get_uri(engine->app),
                  ten_engine_get_id(engine, true), NULL, NULL);
}

void ten_msg_set_src_to_extension(ten_shared_ptr_t *self,
                                  ten_extension_t *extension) {
  TEN_ASSERT(self && ten_msg_check_integrity(self) && extension &&
                 ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_extension_group_t *extension_group =
      extension->extension_thread->extension_group;
  TEN_ASSERT(
      extension_group &&
          // TEN_NOLINTNEXTLINE(thread-check)
          // thread-check: we might be in other threads except extension threads
          // (ex: the JS main thread), and here, we only need to get the name of
          // the extension_group and the engine, and those pointers and the name
          // values will not be changed after they are created, and before the
          // entire engine is closed, so it's thread-safe here.
          ten_extension_group_check_integrity(extension_group, false),
      "Should not happen.");

  ten_engine_t *engine =
      extension_group->extension_thread->extension_context->engine;
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: see above.
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_msg_set_src(self, ten_app_get_uri(engine->app),
                  ten_engine_get_id(engine, false),
                  ten_extension_group_get_name(extension_group, true),
                  ten_extension_get_name(extension, true));
}

void ten_msg_set_src_to_extension_group(
    ten_shared_ptr_t *self, ten_extension_group_t *extension_group) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_engine_t *engine =
      extension_group->extension_thread->extension_context->engine;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_msg_set_src(self, ten_app_get_uri(engine->app),
                  ten_engine_get_id(engine, false),
                  ten_extension_group_get_name(extension_group, true), NULL);
}

bool ten_msg_src_uri_is_empty(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return strlen(ten_msg_get_src_app_uri(self)) == 0;
}

bool ten_msg_src_graph_id_is_empty(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return strlen(ten_msg_get_src_graph_id(self)) == 0;
}

void ten_msg_set_src_uri(ten_shared_ptr_t *self, const char *uri) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  ten_string_set_formatted(&(ten_msg_get_raw_msg(self)->src_loc.app_uri), "%s",
                           uri);
}

void ten_msg_set_src_uri_if_empty(ten_shared_ptr_t *self, const char *uri) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  if (ten_msg_src_uri_is_empty(self)) {
    ten_string_set_formatted(&(ten_msg_get_raw_msg(self)->src_loc.app_uri),
                             "%s", uri);
  }
}

void ten_msg_set_src_engine_if_unspecified(ten_shared_ptr_t *self,
                                           ten_engine_t *engine) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Invalid argument.");

  if (ten_msg_src_graph_id_is_empty(self)) {
    ten_string_copy(&(ten_msg_get_raw_msg(self)->src_loc.graph_id),
                    &engine->graph_id);
  }
}

bool ten_msg_clear_and_set_dest(ten_shared_ptr_t *self, const char *uri,
                                const char *graph_id,
                                const char *extension_group_name,
                                const char *extension_name, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  return ten_raw_msg_clear_and_set_dest(ten_msg_get_raw_msg(self), uri,
                                        graph_id, extension_group_name,
                                        extension_name, err);
}

void ten_raw_msg_clear_and_set_dest_to_loc(ten_msg_t *self, ten_loc_t *loc) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self) && loc,
             "Should not happen.");

  if (!loc) {
    ten_raw_msg_clear_dest(self);
  } else {
    ten_raw_msg_clear_and_set_dest(
        self, ten_string_get_raw_str(&loc->app_uri),
        ten_string_get_raw_str(&loc->graph_id),
        ten_string_get_raw_str(&loc->extension_group_name),
        ten_string_get_raw_str(&loc->extension_name), NULL);
  }
}

void ten_msg_clear_and_set_dest_to_loc(ten_shared_ptr_t *self, ten_loc_t *loc) {
  TEN_ASSERT(self && ten_msg_check_integrity(self) && loc,
             "Should not happen.");

  ten_raw_msg_clear_and_set_dest_to_loc(ten_shared_ptr_get_data(self), loc);
}

static void ten_msg_clear_dest_graph_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  ten_list_foreach (ten_msg_get_dest(self), iter) {
    ten_loc_t *loc = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(loc && ten_loc_check_integrity(loc), "Should not happen.");

    ten_string_clear(&loc->graph_id);
  }
}

void ten_msg_set_dest_engine_if_unspecified_or_predefined_graph_name(
    ten_shared_ptr_t *self, ten_engine_t *engine,
    ten_list_t *predefined_graph_infos) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(engine, "Should not happen.");

  ten_list_foreach (ten_msg_get_dest(self), iter) {
    ten_loc_t *dest_loc = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(dest_loc && ten_loc_check_integrity(dest_loc),
               "Should not happen.");

    if (ten_string_is_empty(&dest_loc->graph_id)) {
      ten_string_copy(&dest_loc->graph_id, &engine->graph_id);
    } else if (predefined_graph_infos) {
      // Otherwise, if the target_engine is one of the _singleton_ predefined
      // graph engine, and the destination graph_id is the "name" of that
      // _singleton_ predefined graph engine, then replace the destination
      // graph_id to the "graph_name" of the _singleton_ predefined graph
      // engine.

      ten_predefined_graph_info_t *singleton_predefined_graph =
          ten_predefined_graph_infos_get_singleton_by_name(
              predefined_graph_infos,
              ten_string_get_raw_str(&dest_loc->graph_id));
      if (singleton_predefined_graph && singleton_predefined_graph->engine) {
        TEN_ASSERT(engine == singleton_predefined_graph->engine,
                   "Otherwise, the message should not be transferred to this "
                   "engine.");

        ten_string_copy(&dest_loc->graph_id,
                        &singleton_predefined_graph->engine->graph_id);
      }
    }
  }
}

void ten_msg_clear_and_set_dest_from_extension_info(
    ten_shared_ptr_t *self, ten_extension_info_t *extension_info) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Invalid argument.");

  TEN_ASSERT(extension_info, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The graph-related information of the extension remains
  // unchanged during the lifecycle of engine/graph, allowing safe
  // cross-thread access.
  TEN_ASSERT(ten_extension_info_check_integrity(extension_info, false),
             "Invalid use of extension_info %p.", extension_info);

  ten_msg_clear_and_set_dest_to_loc(self, &extension_info->loc);
}

ten_list_t *ten_msg_get_dest(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return &ten_msg_get_raw_msg(self)->dest_loc;
}

size_t ten_raw_msg_get_dest_cnt(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return ten_list_size(&self->dest_loc);
}

size_t ten_msg_get_dest_cnt(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_dest_cnt(ten_shared_ptr_get_data(self));
}

const char *ten_msg_get_src_app_uri(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_string_get_raw_str(&ten_msg_get_raw_msg(self)->src_loc.app_uri);
}

const char *ten_msg_get_src_graph_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_string_get_raw_str(&ten_msg_get_raw_msg(self)->src_loc.graph_id);
}

void ten_msg_clear_dest(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  ten_list_clear(&ten_msg_get_raw_msg(self)->dest_loc);
}

ten_loc_t *ten_raw_msg_get_src_loc(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return &self->src_loc;
}

ten_loc_t *ten_msg_get_src_loc(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_src_loc(ten_shared_ptr_get_data(self));
}

/**
 * @brief Retrieves the first destination location from a message.
 *
 * This function returns a pointer to the first destination location in the
 * message's destination list. The caller should verify that the message has at
 * least one destination before calling this function.
 *
 * @param self A shared pointer to the message.
 * @return A pointer to the first destination location (ten_loc_t).
 *         Returns NULL if the destination list is empty.
 *
 * @note The returned pointer points to memory owned by the message and should
 * not be freed by the caller.
 */
ten_loc_t *ten_msg_get_first_dest_loc(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  ten_msg_t *raw_msg = ten_msg_get_raw_msg(self);
  TEN_ASSERT(raw_msg, "Should not happen.");

  // Check if the destination list is empty.
  if (ten_raw_msg_get_dest_cnt(raw_msg) == 0) {
    TEN_ASSERT(0, "The destination list is empty.");
    return NULL;
  }

  return ten_raw_msg_get_first_dest_loc(raw_msg);
}

TEN_MSG_TYPE ten_msg_type_from_type_and_name_string(const char *type_str,
                                                    const char *name_str) {
  TEN_MSG_TYPE msg_type = TEN_MSG_TYPE_INVALID;

  if (!type_str) {
    // If the 'type' of the message is not specified, it would be a 'custom'
    // command.
    msg_type = TEN_MSG_TYPE_CMD;
  } else {
    // Find the correct message type.
    for (size_t i = 0; i < ten_msg_info_size; i++) {
      if (ten_msg_info[i].msg_type_name &&
          ten_c_string_is_equal(type_str, ten_msg_info[i].msg_type_name)) {
        msg_type = (TEN_MSG_TYPE)i;
        break;
      }
    }
  }

  switch (msg_type) {
  case TEN_MSG_TYPE_CMD:
    TEN_ASSERT(name_str, "Invalid argument.");

    // If it is a command, determine if it is a special command name to
    // identify whether it is actually a specialized message type.
    for (size_t i = 0; i < ten_msg_info_size; i++) {
      if (ten_msg_info[i].msg_unique_name &&
          ten_c_string_is_equal(name_str, ten_msg_info[i].msg_unique_name)) {
        msg_type = (TEN_MSG_TYPE)i;
        break;
      }
    }
    break;

  default:
    break;
  }

  if (!(msg_type > TEN_MSG_TYPE_INVALID && msg_type < TEN_MSG_TYPE_LAST)) {
    return TEN_MSG_TYPE_INVALID;
  }

  return msg_type;
}

TEN_MSG_TYPE ten_msg_type_from_unique_name_string(const char *name_str) {
  TEN_ASSERT(name_str, "Invalid argument.");

  TEN_MSG_TYPE msg_type = TEN_MSG_TYPE_INVALID;

  for (size_t i = 0; i < ten_msg_info_size; i++) {
    if (ten_msg_info[i].msg_unique_name &&
        ten_c_string_is_equal(name_str, ten_msg_info[i].msg_unique_name)) {
      msg_type = (TEN_MSG_TYPE)i;
      break;
    }
  }

  return msg_type;
}

const char *ten_msg_type_to_string(const TEN_MSG_TYPE type) {
  if (type >= ten_msg_info_size) {
    return NULL;
  }
  return ten_msg_info[type].msg_type_name;
}

static bool ten_raw_msg_get_one_field_from_json_internal(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    bool include_internal_field, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(field, "Should not happen.");
  TEN_ASSERT(
      field->field_value && ten_value_check_integrity(field->field_value),
      "Should not happen.");

  ten_json_t *json = (ten_json_t *)user_data;
  TEN_ASSERT(json, "Should not happen.");

  if (!field->is_user_defined_properties) {
    // Internal fields are uniformly stored in the "ten" section.
    if (include_internal_field) {
      ten_json_t ten_json = TEN_JSON_INIT_VAL(json->ctx, false);
      bool success =
          ten_json_object_peek_or_create_object(json, TEN_STR_TEN, &ten_json);
      TEN_ASSERT(success, "Should not happen.");

      ten_json_t result_json = TEN_JSON_INIT_VAL(ten_json.ctx, false);
      success =
          ten_json_object_peek(&ten_json, field->field_name, &result_json);
      if (!success) {
        // Some fields are optional, and it is allowed for the corresponding
        // JSON block to be absent during deserialization.
        return true;
      }

      if (!ten_value_set_from_json(field->field_value, &result_json)) {
        // If the field value cannot be set from the JSON, it means that the
        // JSON format is incorrect.
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                        "Invalid JSON format for field %s.", field->field_name);
        }

        return false;
      }
    }
  } else {
    // User-defined fields are stored in the root of the JSON. The field value
    // is an object which includes all the key-value pairs.
    TEN_ASSERT(ten_value_is_object(field->field_value), "Should not happen.");

    const char *key = NULL;
    ten_json_t *item = NULL;
    ten_json_object_foreach(json, key, item) {
      if (ten_c_string_is_equal(key, TEN_STR_TEN)) {
        // The "ten" section is reserved for internal usage.
        continue;
      }

      ten_value_t *value = ten_value_from_json(item);
      if (!value) {
        // If the value cannot be created from the JSON, it means that the JSON
        // format is incorrect.
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                        "Invalid JSON format for field %s.", field->field_name);
        }

        return false;
      }

      // TODO(xilin): If the key is already existed, should we overwrite it?
      ten_list_push_ptr_back(
          &field->field_value->content.object, ten_value_kv_create(key, value),
          (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
    }
  }

  // During JSON deserialization, the field value may be modified, so we set the
  // value_is_changed_after_process flag.
  field->value_is_changed_after_process = true;

  return true;
}

static bool ten_raw_msg_get_one_field_from_json(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    ten_error_t *err) {
  return ten_raw_msg_get_one_field_from_json_internal(self, field, user_data,
                                                      false, err);
}

bool ten_raw_msg_get_one_field_from_json_include_internal_field(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    ten_error_t *err) {
  return ten_raw_msg_get_one_field_from_json_internal(self, field, user_data,
                                                      true, err);
}

static bool ten_raw_msg_put_one_field_to_json_internal(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    bool include_internal_field, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(field, "Should not happen.");
  TEN_ASSERT(
      field->field_value && ten_value_check_integrity(field->field_value),
      "Should not happen.");

  ten_json_t *json = (ten_json_t *)user_data;
  TEN_ASSERT(json, "Should not happen.");

  if (!field->is_user_defined_properties) {
    // Internal fields are uniformly stored in the "ten" section.

    if (include_internal_field) {
      ten_json_t ten_json = TEN_JSON_INIT_VAL(json->ctx, false);
      bool success =
          ten_json_object_peek_or_create_object(json, TEN_STR_TEN, &ten_json);
      TEN_ASSERT(success, "Should not happen.");

      ten_json_t field_json = TEN_JSON_INIT_VAL(ten_json.ctx, false);
      success = ten_value_to_json(field->field_value, &field_json);
      TEN_ASSERT(success, "Should not happen.");

      ten_json_object_set(&ten_json, field->field_name, &field_json);
    }
  } else {
    TEN_ASSERT(ten_value_is_object(field->field_value), "Should not happen.");

    ten_value_object_foreach(field->field_value, iter) {
      ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Should not happen.");

      // The User-defined fields are stored in the root of the JSON.
      ten_json_t kv_json = TEN_JSON_INIT_VAL(json->ctx, false);
      bool success = ten_value_to_json(kv->value, &kv_json);
      TEN_ASSERT(success, "Should not happen.");

      ten_json_object_set(json, ten_string_get_raw_str(&kv->key), &kv_json);
    }
  }

  // The field value is not modified during JSON serialization.
  field->value_is_changed_after_process = false;

  return true;
}

static bool ten_raw_msg_put_one_field_to_json_include_internal_field(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(field, "Should not happen.");
  TEN_ASSERT(
      field->field_value && ten_value_check_integrity(field->field_value),
      "Should not happen.");

  return ten_raw_msg_put_one_field_to_json_internal(self, field, user_data,
                                                    true, err);
}

bool ten_raw_msg_put_one_field_to_json(ten_msg_t *self,
                                       ten_msg_field_process_data_t *field,
                                       void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(field, "Should not happen.");
  TEN_ASSERT(
      field->field_value && ten_value_check_integrity(field->field_value),
      "Should not happen.");

  return ten_raw_msg_put_one_field_to_json_internal(self, field, user_data,
                                                    false, err);
}

bool ten_raw_msg_process_field(ten_msg_t *self,
                               ten_raw_msg_process_one_field_func_t cb,
                               void *user_data, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self) && cb,
             "Should not happen.");

  for (size_t i = 0; i < ten_msg_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_msg_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}

static ten_json_t *ten_raw_msg_to_json_include_internal_field(
    ten_msg_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_json_t *json = ten_json_create_root_object();
  TEN_ASSERT(json, "Should not happen.");

  bool rc = ten_raw_msg_loop_all_fields(
      self, ten_raw_msg_put_one_field_to_json_include_internal_field, json,
      err);

  if (!rc) {
    ten_json_destroy(json);
    return NULL;
  }

  return json;
}

ten_json_t *ten_msg_to_json_include_internal_field(ten_shared_ptr_t *self,
                                                   ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  return ten_raw_msg_to_json_include_internal_field(ten_msg_get_raw_msg(self),
                                                    err);
}

void ten_raw_msg_copy_field(ten_msg_t *self, ten_msg_t *src,
                            ten_list_t *excluded_field_ids) {
  TEN_ASSERT(src && ten_raw_msg_check_integrity(src), "Should not happen.");

  for (size_t i = 0; i < ten_msg_fields_info_size; ++i) {
    if (excluded_field_ids) {
      bool skip = false;

      ten_list_foreach (excluded_field_ids, iter) {
        if (ten_msg_fields_info[i].field_id ==
            ten_int32_listnode_get(iter.node)) {
          skip = true;
          break;
        }
      }

      if (skip) {
        continue;
      }
    }

    ten_msg_copy_field_func_t copy_field = ten_msg_fields_info[i].copy_field;
    if (copy_field) {
      copy_field(self, src, excluded_field_ids);
    }
  }
}

static bool ten_raw_msg_init_from_json(ten_msg_t *self, ten_json_t *json,
                                       ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(json, "Should not happen.");
  TEN_ASSERT(ten_json_check_integrity(json), "Should not happen.");

  bool rc = ten_raw_msg_loop_all_fields(
      self, ten_raw_msg_get_one_field_from_json, json, err);

  if (!rc) {
    if (err) {
      ten_error_set(
          err, TEN_ERROR_CODE_INVALID_JSON,
          "Failed to init a message from json, because the fields are "
          "incorrect.");
    }
    return false;
  }

  return true;
}

bool ten_msg_from_json(ten_shared_ptr_t *self, ten_json_t *json,
                       ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(json, "Should not happen.");
  TEN_ASSERT(ten_json_check_integrity(json), "Should not happen.");

  return ten_raw_msg_init_from_json(ten_shared_ptr_get_data(self), json, err);
}

ten_shared_ptr_t *ten_msg_clone(ten_shared_ptr_t *self,
                                ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  ten_msg_t *raw_result = NULL;
  ten_shared_ptr_t *result = NULL;

  ten_raw_msg_clone_func_t clone = ten_msg_info[ten_msg_get_type(self)].clone;
  if (clone) {
    raw_result = clone(ten_msg_get_raw_msg(self), excluded_field_ids);
    if (raw_result) {
      result = ten_shared_ptr_create(raw_result, ten_raw_msg_destroy);

      if (ten_msg_is_cmd_and_result(self)) {
        // Create a relationship between the newly generated command and the
        // original command.
        ten_cmd_base_save_cmd_id_to_parent_cmd_id(result);

        // The rule is simple:
        //
        // If generating/cloning a new message, that message will contain a new
        // command ID. If don't want a new command ID, then don't generate/clone
        // a new message.
        ten_raw_cmd_base_gen_new_cmd_id_forcibly((ten_cmd_base_t *)raw_result);
      }
    }
  }

  return result;
}

ten_shared_ptr_t *ten_msg_create_from_msg_type(TEN_MSG_TYPE msg_type) {
  switch (msg_type) {
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
    return ten_cmd_close_app_create();
  case TEN_MSG_TYPE_CMD:
    return ten_cmd_custom_create_empty();
  case TEN_MSG_TYPE_CMD_START_GRAPH:
    return ten_cmd_start_graph_create();
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
    return ten_cmd_stop_graph_create();
  case TEN_MSG_TYPE_CMD_TIMEOUT:
    return ten_cmd_timeout_create(0);
  case TEN_MSG_TYPE_CMD_RESULT:
    return ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_OK, NULL);
  case TEN_MSG_TYPE_DATA:
    return ten_data_create_empty();
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return ten_audio_frame_create_empty();
  case TEN_MSG_TYPE_VIDEO_FRAME:
    return ten_video_frame_create_empty();
  default:
    return NULL;
  }
}

bool ten_msg_type_to_handle_when_closing(ten_shared_ptr_t *msg) {
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  switch (ten_msg_get_type(msg)) {
  case TEN_MSG_TYPE_CMD_RESULT:
    return true;
  default:
    return false;
  }
}

const char *ten_raw_msg_get_type_string(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return ten_msg_type_to_string(ten_raw_msg_get_type(self));
}

const char *ten_msg_get_type_string(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_type_string(ten_shared_ptr_get_data(self));
}

TEN_MSG_TYPE ten_msg_type_from_type_string(const char *type_str) {
  TEN_MSG_TYPE msg_type = TEN_MSG_TYPE_INVALID;

  // Find the correct message type.
  for (size_t i = 0; i < ten_msg_info_size; i++) {
    if (ten_msg_info[i].msg_type_name &&
        ten_c_string_is_equal(type_str, ten_msg_info[i].msg_type_name)) {
      msg_type = (TEN_MSG_TYPE)i;
      break;
    }
  }

  if (!(msg_type > TEN_MSG_TYPE_INVALID && msg_type < TEN_MSG_TYPE_LAST)) {
    return TEN_MSG_TYPE_INVALID;
  }

  return msg_type;
}

/**
 * @brief Corrects the destination location in a message.
 *
 * This function performs several important corrections to ensure messages are
 * properly routed:
 * 1. Resolves 'localhost' references to the actual app URI.
 * 2. Sets appropriate engine/graph information for different message types.
 * 3. Handles special cases for certain command types.
 *
 * @param msg The message whose destination needs to be corrected.
 * @param engine The engine context used for resolving destinations.
 */
void ten_msg_correct_dest(ten_shared_ptr_t *msg, ten_engine_t *engine) {
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  const char *app_uri = ten_app_get_uri(engine->app);

  ten_msg_t *raw_msg = ten_msg_get_raw_msg(msg);
  ten_list_foreach (&raw_msg->dest_loc, iter) {
    ten_loc_t *dest_loc = ten_ptr_listnode_get(iter.node);

    bool is_local_app = false;

    if (ten_string_is_equal_c_str(&dest_loc->app_uri, app_uri)) {
      is_local_app = true;
    } else if (ten_string_is_equal_c_str(&dest_loc->app_uri,
                                         TEN_STR_LOCALHOST)) {
      // When an extension uses 'localhost' to indicate the destination is the
      // local app, we need to replace 'localhost' with the actual URI of the
      // app.
      ten_string_set_from_c_str(&dest_loc->app_uri, app_uri);
      is_local_app = true;
    }

    if (is_local_app) {
      // For local app destinations, extensions might omit the engine graph ID.
      // We need to add this information except for special commands that are
      // handled directly by the app rather than by engines.
      //
      // Special cases:
      // - start_graph: Handled by the app to create a new graph.
      // - close_app: Handled by the app to shut down.
      TEN_MSG_TYPE msg_type = ten_msg_get_type(msg);
      switch (msg_type) {
      case TEN_MSG_TYPE_CMD_START_GRAPH:
      case TEN_MSG_TYPE_CMD_CLOSE_APP:
        ten_msg_clear_dest_graph_id(msg);
        break;

      default:
        // For all other message types, ensure the engine/graph information is
        // properly set.
        ten_msg_set_dest_engine_if_unspecified_or_predefined_graph_name(
            msg, engine, &engine->app->predefined_graph_infos);
        break;
      }
    }
  }

  // Special handling for start_graph commands:
  // In a start_graph command, extension URIs might be specified as 'localhost',
  // indicating they should be located in the local app.
  // We need to replace these 'localhost' references with the actual app URI.
  if (ten_msg_get_type(msg) == TEN_MSG_TYPE_CMD_START_GRAPH) {
    ten_list_t *extensions_info = ten_cmd_start_graph_get_extensions_info(msg);
    ten_list_foreach (extensions_info, iter) {
      ten_extension_info_t *extension_info =
          ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
      ten_extension_info_translate_localhost_to_app_uri(extension_info,
                                                        app_uri);
    }
  }
}

static bool ten_raw_msg_dump_internal(ten_msg_t *msg, ten_error_t *err,
                                      const char *fmt, va_list ap) {
  TEN_ASSERT(msg && ten_raw_msg_check_integrity(msg), "Should not happen.");

  ten_json_t *msg_json = ten_raw_msg_to_json_include_internal_field(msg, err);
  TEN_ASSERT(msg_json, "Failed to convert msg type(%s), name(%s) to JSON.",
             ten_msg_type_to_string(msg->type),
             ten_value_peek_raw_str(&msg->name, err));
  if (!msg_json) {
    return false;
  }

  bool must_free = false;
  const char *msg_json_str = ten_json_to_string(msg_json, NULL, &must_free);

  ten_string_t description;
  TEN_STRING_INIT(description);
  ten_string_append_from_va_list(&description, fmt, ap);

  const char *p = ten_string_get_raw_str(&description);

  ten_string_t dump_str;
  TEN_STRING_INIT(dump_str);

  while (*p) {
    if ('^' != *p) {
      ten_string_append_formatted(&dump_str, "%c", *p++);
      continue;
    }

    switch (*++p) {
    // The message content.
    case 'm':
      ten_string_append_formatted(&dump_str, "%s", msg_json_str);
      p++;
      break;

      // If next char can't match any mode, keep it.
    default:
      ten_string_append_formatted(&dump_str, "%c", *p++);
      break;
    }
  }

  TEN_LOGE("%s", ten_string_get_raw_str(&dump_str));

  ten_string_deinit(&dump_str);
  ten_string_deinit(&description);

  if (must_free) {
    TEN_FREE(msg_json_str);
  }
  ten_json_destroy(msg_json);

  return true;
}

bool ten_raw_msg_dump(ten_msg_t *msg, ten_error_t *err, const char *fmt, ...) {
  TEN_ASSERT(msg && ten_raw_msg_check_integrity(msg), "Should not happen.");

  va_list ap;
  va_start(ap, fmt);
  bool rc = ten_raw_msg_dump_internal(msg, err, fmt, ap);
  va_end(ap);

  return rc;
}

bool ten_msg_dump(ten_shared_ptr_t *msg, ten_error_t *err, const char *fmt,
                  ...) {
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  va_list ap;
  va_start(ap, fmt);
  bool rc =
      ten_raw_msg_dump_internal(ten_shared_ptr_get_data(msg), err, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * @note The ownership of @a value is transferred to the @a msg.
 */
static bool ten_raw_msg_set_property(ten_msg_t *self, const char *path,
                                     ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!path || !strlen(path)) {
    // If the path is empty, clear and set all properties.
    ten_value_deinit(&self->properties);
    bool result = ten_value_init_object_with_move(&self->properties,
                                                  ten_value_peek_object(value));
    if (result) {
      // The contents of `value` have been completely moved into `properties`,
      // so the outer `value` wrapper is removed to avoid memory leaks.
      ten_value_destroy(value);
    }
    return result;
  }

  bool success = true;

  ten_list_t paths = TEN_LIST_INIT_VAL;
  if (!ten_value_path_parse(path, &paths, NULL)) {
    goto done;
  }

  bool in_ten_namespace = false;

  ten_list_foreach (&paths, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM: {
      if ((item_iter.index == 0) &&
          !strcmp(TEN_STR_TEN, ten_string_get_raw_str(&item->obj_item_str))) {
        // It is the 'ten::' namespace.
        in_ten_namespace = true;

        // Remove the 'ten' namespace path part.
        ten_list_remove_front(&paths);

        ten_raw_msg_set_ten_property_func_t set_ten_property =
            ten_msg_info[ten_raw_msg_get_type(self)].set_ten_property;
        if (set_ten_property) {
          success = set_ten_property(self, &paths, value, err);
        }

        ten_value_destroy(value);
      }
      break;
    }

    default:
      break;
    }
  }

  if (!in_ten_namespace) {
    success = ten_value_set_from_path_list_with_move(&self->properties, &paths,
                                                     value, err);
  }

done:
  ten_list_clear(&paths);

  return success;
}

bool ten_msg_set_property(ten_shared_ptr_t *self, const char *path,
                          ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  return ten_raw_msg_set_property(ten_shared_ptr_get_data(self), path, value,
                                  err);
}

ten_value_t *ten_raw_msg_peek_property(ten_msg_t *self, const char *path,
                                       ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  if (!path || !strlen(path)) {
    // If the path is empty, return all properties.
    return &self->properties;
  }

  ten_value_t *result = NULL;

  ten_list_t paths = TEN_LIST_INIT_VAL;
  if (!ten_value_path_parse(path, &paths, NULL)) {
    goto done;
  }

  bool in_ten_namespace = false;

  ten_list_foreach (&paths, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM: {
      if ((item_iter.index == 0) &&
          !strcmp(TEN_STR_TEN, ten_string_get_raw_str(&item->obj_item_str))) {
        // It is the 'ten::' namespace.
        in_ten_namespace = true;

        // Remove the 'ten' namespace path part.
        ten_list_remove_front(&paths);

        ten_raw_msg_peek_ten_property_func_t peek_ten_property =
            ten_msg_info[ten_raw_msg_get_type(self)].peek_ten_property;
        if (peek_ten_property) {
          result = peek_ten_property(self, &paths, err);
        }
      }
      break;
    }

    default:
      break;
    }
  }

  if (!in_ten_namespace) {
    result = ten_value_peek_from_path(&self->properties, path, err);
  }

done:
  ten_list_clear(&paths);

  return result;
}

ten_value_t *ten_msg_peek_property(ten_shared_ptr_t *self, const char *path,
                                   ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  return ten_raw_msg_peek_property(ten_msg_get_raw_msg(self), path, err);
}

static bool ten_raw_msg_has_locked_res(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return !ten_list_is_empty(&self->locked_res);
}

bool ten_msg_has_locked_res(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_has_locked_res(ten_shared_ptr_get_data(self));
}

const char *ten_raw_msg_get_name(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return ten_value_peek_raw_str(&self->name, NULL);
}

const char *ten_msg_get_name(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");

  ten_msg_t *raw_msg = ten_msg_get_raw_msg(self);
  TEN_ASSERT(raw_msg, "Should not happen.");

  return ten_raw_msg_get_name(raw_msg);
}

bool ten_raw_msg_set_name_with_len(ten_msg_t *self, const char *msg_name,
                                   size_t msg_name_len, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  if (msg_name == NULL) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "%s",
                    "Failed to set message name to an empty string.");
    }
    return false;
  }

  ten_string_set_formatted(ten_value_peek_string(&self->name), "%.*s",
                           msg_name_len, msg_name);
  return true;
}

bool ten_raw_msg_set_name(ten_msg_t *self, const char *msg_name,
                          ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_set_name_with_len(self, msg_name, strlen(msg_name), err);
}

bool ten_msg_set_name_with_len(ten_shared_ptr_t *self, const char *msg_name,
                               size_t msg_name_len, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_set_name_with_len(ten_shared_ptr_get_data(self), msg_name,
                                       msg_name_len, err);
}

bool ten_msg_set_name(ten_shared_ptr_t *self, const char *msg_name,
                      ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_set_name(ten_shared_ptr_get_data(self), msg_name, err);
}

bool ten_raw_msg_validate_schema(ten_msg_t *self,
                                 ten_schema_store_t *schema_store,
                                 bool is_msg_out, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(schema_store && ten_schema_store_check_integrity(schema_store),
             "Invalid argument.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  const char *msg_name = ten_raw_msg_get_name(self);
  if (ten_raw_msg_is_cmd_and_result(self)) {
    TEN_ASSERT(msg_name && strlen(msg_name), "Should not happen.");
  }

  ten_msg_schema_t *schema = ten_schema_store_get_msg_schema(
      schema_store, ten_raw_msg_get_type(self), msg_name, is_msg_out);
  if (!schema) {
    return true;
  }

  if (!ten_msg_schema_adjust_properties(schema, &self->properties, err)) {
    return false;
  }

  if (!ten_msg_schema_validate_properties(schema, &self->properties, err)) {
    return false;
  }

  return true;
}

bool ten_msg_validate_schema(ten_shared_ptr_t *self,
                             ten_schema_store_t *schema_store, bool is_msg_out,
                             ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  TEN_ASSERT(schema_store && ten_schema_store_check_integrity(schema_store),
             "Should not happen.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_msg_t *msg = ten_shared_ptr_get_data(self);
  ten_raw_msg_validate_schema_func_t validate_schema =
      ten_msg_info[ten_raw_msg_get_type(msg)].validate_schema;
  if (validate_schema) {
    return validate_schema(msg, schema_store, is_msg_out, err);
  }

  return true;
}

TEN_MSG_TYPE ten_msg_get_type(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_type(ten_msg_get_raw_msg(self));
}
