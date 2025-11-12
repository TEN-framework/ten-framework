//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/path/common.h"
#include "include_internal/ten_runtime/path/path_table.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"

#define TEN_PATH_SIGNATURE 0xC60A6AEBDC969A43U

typedef struct ten_path_group_t ten_path_group_t;
typedef struct ten_msg_conversion_t ten_msg_conversion_t;

typedef struct ten_path_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  // The belonging path table.
  ten_path_table_t *table;

  // The belonging group.
  ten_shared_ptr_t *group;
  bool last_in_group;

  // The type of the path.
  TEN_PATH_TYPE type;

  // `cmd_name` and `cmd_id` represent the cmd associated with the creation of
  // this path.

  // This field is used to store the command name of the original command
  // corresponding to the cmd_result. This is because some information from the
  // cmd_result can only be obtained when the original command is known.
  //
  // Ex: The schema of the cmd_result is defined within the corresponding
  // original command, ex:
  //
  // "api": {
  //   "cmd_in": [
  //     {
  //       "name": "hello",
  //       "result": {
  //         "property": {}
  //       }
  //     }
  //   ]
  // }
  //
  // We need to use the cmd name of the original command to find the schema of
  // the cmd_result.
  ten_string_t cmd_name;

  // The cmd_id of the command.
  ten_string_t cmd_id;

  // The cmd_id of the parent command.
  //
  // If the command that originally created this path (i.e., the command
  // represented by `cmd_name` and `cmd_id`) has a `parent_cmd` relationship,
  // then this field is used to record the `parent_cmd_id` of that relationship.
  // This allows the `cmd_result` to be transmitted back to the source through
  // the `parent_cmd_id` relationship.
  ten_string_t parent_cmd_id;

  // The source location of the original command (i.e., the command
  // represented by `cmd_name` and `cmd_id`).
  ten_loc_t src_loc;

  // The TEN runtime needs to return the correct `cmd_result`, so it first keeps
  // the `cmd_result` that was received earlier in time. It waits until the
  // conditions are met (e.g., all `cmd_result` from the output paths have been
  // received) before performing the return action. This field is used to store
  // the temporarily kept `cmd_result`.
  ten_shared_ptr_t *cached_cmd_result;

  // Indicates whether the `cmd_result` with the is_final attribute on this path
  // has been received. If it has been received, it means this path has
  // completed its task.
  bool has_received_final_cmd_result;

  ten_msg_conversion_t *result_conversion;

  uint64_t expired_time_us;
} ten_path_t;

TEN_RUNTIME_PRIVATE_API bool ten_path_check_integrity(ten_path_t *self,
                                                      bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_path_init(
    ten_path_t *self, ten_path_table_t *table, TEN_PATH_TYPE type,
    const char *cmd_name, const char *parent_cmd_id, const char *cmd_id,
    ten_loc_t *src_loc);

TEN_RUNTIME_PRIVATE_API void ten_path_deinit(ten_path_t *self);

TEN_RUNTIME_PRIVATE_API void ten_path_set_expired_time(
    ten_path_t *path, uint64_t expired_time_us);

TEN_RUNTIME_PRIVATE_API ten_path_group_t *ten_path_get_group(ten_path_t *self);
