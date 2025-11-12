//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/path/path.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/path_group.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/sanitizer/thread_check.h"

bool ten_path_check_integrity(ten_path_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_PATH_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    // In case the extension thread may be lock_mode, we utilize
    // extension_thread_check_integrity to handle this scenario.
    if (self->table->attach_to == TEN_PATH_TABLE_ATTACH_TO_EXTENSION) {
      ten_extension_thread_t *extension_thread =
          self->table->attached_target.extension->extension_thread;
      return ten_extension_thread_check_integrity(extension_thread, true);
    }

    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

void ten_path_init(ten_path_t *self, ten_path_table_t *table,
                   TEN_PATH_TYPE type, const char *cmd_name,
                   const char *parent_cmd_id, const char *cmd_id,
                   ten_loc_t *src_loc) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(cmd_name && strlen(cmd_name), "Invalid argument.");

  ten_signature_set(&self->signature, TEN_PATH_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->table = table;
  self->type = type;

  ten_string_init_formatted(&self->cmd_name, "%s", cmd_name);

  if (parent_cmd_id) {
    ten_string_init_formatted(&self->parent_cmd_id, "%s", parent_cmd_id);
  } else {
    TEN_STRING_INIT(self->parent_cmd_id);
  }
  ten_string_init_formatted(&self->cmd_id, "%s", cmd_id);

  ten_loc_init_from_loc(&self->src_loc, src_loc);

  self->group = NULL;
  self->last_in_group = false;
  self->cached_cmd_result = NULL;
  self->has_received_final_cmd_result = false;
  self->result_conversion = NULL;
  self->expired_time_us = UINT64_MAX;
}

void ten_path_deinit(ten_path_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The belonging thread might be destroyed, so we do not check
  // the thread safety here.
  TEN_ASSERT(ten_path_check_integrity(self, false), "Should not happen.");

  ten_string_deinit(&self->cmd_name);

  ten_string_deinit(&self->cmd_id);
  ten_string_deinit(&self->parent_cmd_id);

  ten_loc_deinit(&self->src_loc);

  if (self->group) {
    ten_shared_ptr_destroy(self->group);
    self->group = NULL;
  }

  if (self->cached_cmd_result) {
    ten_shared_ptr_destroy(self->cached_cmd_result);
    self->cached_cmd_result = NULL;
  }

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_signature_set(&self->signature, 0);
}

void ten_path_set_expired_time(ten_path_t *path, uint64_t expired_time_us) {
  TEN_ASSERT(path && ten_path_check_integrity(path, true), "Invalid argument.");

  path->expired_time_us = expired_time_us;
}

ten_path_group_t *ten_path_get_group(ten_path_t *self) {
  TEN_ASSERT(self && ten_path_check_integrity(self, true), "Invalid argument.");

  ten_path_group_t *path_group =
      (ten_path_group_t *)ten_shared_ptr_get_data(self->group);
  TEN_ASSERT(path_group && ten_path_group_check_integrity(path_group, true),
             "Invalid argument.");

  return path_group;
}
