//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/path/path_group.h"

#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/path.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

bool ten_path_group_check_integrity(ten_path_group_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_PATH_GROUP_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    // In case the extension thread may be in lock_mode, we utilize
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

/**
 * @brief Checks whether a given path is associated with a group or not.
 *
 * @return true if the path is part of a group, otherwise, it returns false.
 */
bool ten_path_is_in_a_group(ten_path_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  return self->group ? true : false;
}

/**
 * @brief The three functions @name ten_path_group_create, @name
 * ten_path_group_create_master, and @name ten_path_group_create_slave create a
 * new path group, a master group, and a slave group, respectively. They
 * allocate memory for the structure, initialize various components of the path
 * group, and return the group.
 */
static ten_path_group_t *ten_path_group_create(
    ten_path_table_t *table, TEN_RESULT_RETURN_POLICY policy) {
  ten_path_group_t *self =
      (ten_path_group_t *)TEN_MALLOC(sizeof(ten_path_group_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_PATH_GROUP_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->table = table;
  self->policy = policy;
  ten_list_init(&self->members);

  return self;
}

/**
 * @brief Frees the memory allocated for the path group.
 */
void ten_path_group_destroy(ten_path_group_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_signature_set(&self->signature, 0);

  ten_list_clear(&self->members);

  TEN_FREE(self);
}

/**
 * @brief Creates a group of paths. It takes a list of paths as an argument and
 * initializes each path with a group.
 */
void ten_paths_create_group(ten_list_t *paths,
                            TEN_RESULT_RETURN_POLICY policy) {
  TEN_ASSERT(paths, "Invalid argument.");
  TEN_ASSERT(ten_list_size(paths) > 1, "Invalid argument.");

  ten_path_group_t *path_group = NULL;
  ten_shared_ptr_t *path_group_sp = NULL;

  ten_path_t *last_path = NULL;
  ten_list_foreach (paths, iter) {
    ten_path_t *path = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(path && ten_path_check_integrity(path, true),
               "Invalid argument.");
    TEN_ASSERT(path->table, "Invalid argument.");

    last_path = path;

    // TEN_LOGD("create path group: cmd_id: %s, parent_cmd_id: %s",
    //          ten_string_get_raw_str(&path->cmd_id),
    //          ten_string_get_raw_str(&path->parent_cmd_id));

    if (!path_group_sp) {
      path_group = ten_path_group_create(path->table, policy);
      TEN_ASSERT(path_group, "Failed to create path group.");

      path_group_sp = ten_shared_ptr_create(path_group, ten_path_group_destroy);
      TEN_ASSERT(path_group_sp, "Failed to create path group shared pointer.");

      path->group = path_group_sp;
    } else {
      path->group = ten_shared_ptr_clone(path_group_sp);
    }

    ten_list_push_ptr_back(&path_group->members, path, NULL);
  }

  TEN_ASSERT(last_path, "Should not happen.");
  last_path->last_in_group = true;
}

/**
 * @brief Takes a path as an argument and returns a list of all the paths that
 * belong to the same group as the given path.
 */
ten_list_t *ten_path_group_get_members(ten_path_group_t *self) {
  TEN_ASSERT(self && ten_path_group_check_integrity(self, true),
             "Invalid argument.");

  ten_list_t *members = &self->members;
  TEN_ASSERT(members && ten_list_check_integrity(members),
             "Should not happen.");

  return members;
}
