//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

#define TEN_MSG_SIGNATURE 0xA9FA53F77185F856U

typedef struct ten_app_t ten_app_t;
typedef struct ten_extension_t ten_extension_t;
typedef struct ten_error_t ten_error_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_extension_group_t ten_extension_group_t;
typedef struct ten_extension_thread_t ten_extension_thread_t;
typedef struct ten_extension_info_t ten_extension_info_t;
typedef struct ten_schema_store_t ten_schema_store_t;

static_assert(sizeof(TEN_MSG_TYPE) == sizeof(uint32_t),
              "Incorrect TEN_MSG_TYPE size, this would break (de)serialize.");

// - Only msg types have `to_json`/`from_json` functions.
//
//   - If the json contains the fields `ten::type` and `ten::name`, these fields
//     must uniquely map to one actual type of the msg instance; otherwise, an
//     error will be thrown.
//
//   - If the json contains the fields `ten::type` and `ten::name`, these fields
//     must uniquely map to the actual type of the msg instance; otherwise, an
//     error will be thrown.

typedef struct ten_msg_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_MSG_TYPE type;

  // Each message has a "name", which is used for routing. In the graph, you can
  // specify different names to flow to different destination extensions. If a
  // message's name is empty, it can only flow to the destinations in the graph
  // that have not specified a name.
  ten_value_t name;  // string

  ten_loc_t src_loc;
  ten_list_t dest_loc;

  ten_value_t properties;  // object value.

  ten_list_t locked_res;

  int64_t timestamp;
} ten_msg_t;

TEN_RUNTIME_API bool ten_raw_msg_check_integrity(ten_msg_t *self);

TEN_RUNTIME_API bool ten_msg_check_integrity(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_init(ten_msg_t *self,
                                              TEN_MSG_TYPE type);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_deinit(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_copy_field(
    ten_msg_t *self, ten_msg_t *src, ten_list_t *excluded_field_ids);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_set_src_to_loc(ten_msg_t *self,
                                                        ten_loc_t *loc);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_to_loc(ten_shared_ptr_t *self,
                                                    ten_loc_t *loc);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_to_engine(ten_shared_ptr_t *self,
                                                       ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_to_extension(
    ten_shared_ptr_t *self, ten_extension_t *extension);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_to_extension_group(
    ten_shared_ptr_t *self, ten_extension_group_t *extension_group);

TEN_RUNTIME_PRIVATE_API void ten_msg_clear_and_set_dest_from_msg_src(
    ten_shared_ptr_t *self, ten_shared_ptr_t *cmd);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_add_dest(
    ten_msg_t *self, const char *app_uri, const char *graph_id,
    const char *extension_group_name, const char *extension_name);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_clear_dest(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_msg_src_is_empty(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_msg_get_src_graph_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_msg_get_first_dest_uri(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_raw_msg_get_first_dest_uri(
    ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_raw_msg_get_src_loc(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_raw_msg_get_first_dest_loc(
    ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src(ten_shared_ptr_t *self,
                                             const char *app_uri,
                                             const char *graph_id,
                                             const char *extension_group_name,
                                             const char *extension_name);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_uri(ten_shared_ptr_t *self,
                                                 const char *app_uri);

TEN_RUNTIME_PRIVATE_API bool ten_msg_src_uri_is_empty(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_msg_src_graph_id_is_empty(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_uri_if_empty(
    ten_shared_ptr_t *self, const char *app_uri);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_engine_if_unspecified(
    ten_shared_ptr_t *self, ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API size_t ten_raw_msg_get_dest_cnt(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_clear_and_set_dest_to_loc(
    ten_msg_t *self, ten_loc_t *loc);

TEN_RUNTIME_PRIVATE_API void ten_msg_set_src_to_app(ten_shared_ptr_t *self,
                                                    ten_app_t *app);

TEN_RUNTIME_PRIVATE_API bool ten_msg_type_to_handle_when_closing(
    ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API const char *ten_msg_type_to_string(TEN_MSG_TYPE type);

TEN_RUNTIME_PRIVATE_API const char *ten_raw_msg_get_type_string(
    ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_clear_and_set_dest_from_extension_info(
    ten_shared_ptr_t *self, ten_extension_info_t *extension_info);

TEN_RUNTIME_PRIVATE_API void ten_msg_correct_dest(ten_shared_ptr_t *msg,
                                                  ten_engine_t *engine);

inline bool ten_raw_msg_is_cmd_and_result(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD:
  case TEN_MSG_TYPE_CMD_RESULT:
    return true;

  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return false;

  default:
    TEN_ASSERT(0, "Invalid message type %d", self->type);
    return false;
  }
}

inline bool ten_raw_msg_is_cmd(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD:
    return true;

  case TEN_MSG_TYPE_CMD_RESULT:
  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return false;

  default:
    TEN_ASSERT(0, "Invalid message type %d", self->type);
    return false;
  }
}

inline bool ten_raw_msg_is_cmd_result(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD_RESULT:
    return true;

  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD:
  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return false;

  default:
    TEN_ASSERT(0, "Invalid message type %d", self->type);
    return false;
  }
}

TEN_RUNTIME_PRIVATE_API bool ten_msg_has_locked_res(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_clear_and_set_dest_to_loc(
    ten_shared_ptr_t *self, ten_loc_t *loc);

TEN_RUNTIME_PRIVATE_API TEN_MSG_TYPE ten_msg_type_from_type_and_name_string(
    const char *type_str, const char *name_str);

TEN_RUNTIME_PRIVATE_API const char *ten_msg_get_type_string(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API TEN_MSG_TYPE
ten_msg_type_from_type_string(const char *type_str);

TEN_RUNTIME_PRIVATE_API TEN_MSG_TYPE
ten_msg_type_from_unique_name_string(const char *name_str);

// Debug only.
TEN_RUNTIME_PRIVATE_API bool ten_raw_msg_dump(ten_msg_t *msg, ten_error_t *err,
                                              const char *fmt, ...);

TEN_RUNTIME_API bool ten_msg_dump(ten_shared_ptr_t *msg, ten_error_t *err,
                                  const char *fmt, ...);

TEN_RUNTIME_PRIVATE_API bool ten_raw_msg_validate_schema(
    ten_msg_t *self, ten_schema_store_t *schema_store, bool is_msg_out,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_msg_validate_schema(
    ten_shared_ptr_t *self, ten_schema_store_t *schema_store, bool is_msg_out,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool
ten_raw_msg_get_one_field_from_json_include_internal_field(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_raw_msg_put_one_field_to_json(
    ten_msg_t *self, ten_msg_field_process_data_t *field, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_json_t *ten_msg_to_json_include_internal_field(
    ten_shared_ptr_t *self, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_raw_msg_process_field(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API const char *ten_msg_get_src_app_uri(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_msg_get_src_loc(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_msg_get_first_dest_loc(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_list_t *ten_msg_get_dest(ten_shared_ptr_t *self);

TEN_RUNTIME_API size_t ten_msg_get_dest_cnt(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_clear_dest(ten_shared_ptr_t *self);

TEN_RUNTIME_API ten_shared_ptr_t *ten_msg_create_from_msg_type(
    TEN_MSG_TYPE msg_type);

TEN_RUNTIME_PRIVATE_API void ten_raw_msg_destroy(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_raw_msg_get_name(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_raw_msg_set_name(ten_msg_t *self,
                                                  const char *msg_name,
                                                  ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_raw_msg_set_name_with_len(ten_msg_t *self,
                                                           const char *msg_name,
                                                           size_t msg_name_len,
                                                           ten_error_t *err);

/**
 * @brief Set the 'graph_id' in the dest loc to the specified value.
 */
TEN_RUNTIME_PRIVATE_API void
ten_msg_set_dest_engine_if_unspecified_or_predefined_graph_name(
    ten_shared_ptr_t *self, ten_engine_t *target_engine,
    ten_list_t *predefined_graph_infos);

TEN_RUNTIME_PRIVATE_API bool ten_msg_set_name_with_len(ten_shared_ptr_t *self,
                                                       const char *msg_name,
                                                       size_t msg_name_len,
                                                       ten_error_t *err);

inline TEN_MSG_TYPE ten_raw_msg_get_type(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return self->type;
}

inline ten_msg_t *ten_msg_get_raw_msg(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return (ten_msg_t *)ten_shared_ptr_get_data(self);
}

inline bool ten_msg_is_cmd_and_result(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_is_cmd_and_result(ten_msg_get_raw_msg(self));
}

inline bool ten_msg_is_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_is_cmd(ten_msg_get_raw_msg(self));
}

inline bool ten_msg_is_cmd_result(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_is_cmd_result(ten_msg_get_raw_msg(self));
}

inline void ten_raw_msg_set_timestamp(ten_msg_t *self, int64_t timestamp) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  self->timestamp = timestamp;
}

inline int64_t ten_raw_msg_get_timestamp(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return self->timestamp;
}

inline void ten_msg_set_timestamp(ten_shared_ptr_t *self, int64_t timestamp) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  ten_raw_msg_set_timestamp(ten_msg_get_raw_msg(self), timestamp);
}

inline int64_t ten_msg_get_timestamp(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_timestamp(ten_msg_get_raw_msg(self));
}
