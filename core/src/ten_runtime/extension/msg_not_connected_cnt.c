//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/msg_not_connected_cnt.h"

#include "include_internal/ten_runtime/extension/extension.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"

#define TEN_MSG_NOT_CONNECTED_COUNT_RESET_THRESHOLD (1000)

static ten_extension_output_msg_not_connected_count_t *
ten_extension_output_msg_not_connected_count_create(const char *msg_name) {
  TEN_ASSERT(msg_name, "Invalid argument.");

  ten_extension_output_msg_not_connected_count_t *self =
      (ten_extension_output_msg_not_connected_count_t *)TEN_MALLOC(
          sizeof(ten_extension_output_msg_not_connected_count_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_string_init_from_c_str_with_size(&self->msg_name, msg_name,
                                       strlen(msg_name));
  self->count = 0;

  return self;
}

static void ten_extension_output_msg_not_connected_count_destroy(
    ten_extension_output_msg_not_connected_count_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->msg_name);

  TEN_FREE(self);
}

static void ten_extension_output_msg_not_connected_count_hh_destroy(
    ten_hashhandle_t *hh) {
  TEN_ASSERT(hh, "Should not happen.");

  ten_extension_output_msg_not_connected_count_t *entry =
      CONTAINER_OF_FROM_FIELD(
          hh, ten_extension_output_msg_not_connected_count_t, hh_in_map);
  TEN_ASSERT(entry, "Should not happen.");

  ten_extension_output_msg_not_connected_count_destroy(entry);
}

bool ten_extension_increment_msg_not_connected_count(ten_extension_t *extension,
                                                     const char *msg_name) {
  TEN_ASSERT(extension, "Invalid argument.");
  TEN_ASSERT(msg_name, "Invalid argument.");

  ten_extension_output_msg_not_connected_count_t *entry = NULL;

  ten_hashhandle_t *hh = ten_hashtable_find_string(
      &extension->msg_not_connected_count_map, msg_name);
  if (!hh) {
    entry = ten_extension_output_msg_not_connected_count_create(msg_name);
    entry->count = 0;

    ten_hashtable_add_string(
        &extension->msg_not_connected_count_map, &entry->hh_in_map,
        ten_string_get_raw_str(&entry->msg_name),
        ten_extension_output_msg_not_connected_count_hh_destroy);
  } else {
    entry = CONTAINER_OF_FROM_FIELD(
        hh, ten_extension_output_msg_not_connected_count_t, hh_in_map);
    entry->count++;
  }

  if (entry->count % TEN_MSG_NOT_CONNECTED_COUNT_RESET_THRESHOLD == 0) {
    entry->count = 0;  // Reset.
    return true;
  }
  return false;
}
