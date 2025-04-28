//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension/extension_info/json.h"
#include "ten_utils/container/list_node_smart_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

bool ten_msg_dest_info_check_integrity(ten_msg_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_MSG_DEST_STATIC_INFO_SIGNATURE) {
    return false;
  }
  return true;
}

ten_msg_dest_info_t *ten_msg_dest_info_create(const char *msg_name) {
  TEN_ASSERT(msg_name, "Should not happen.");

  ten_msg_dest_info_t *self =
      (ten_msg_dest_info_t *)TEN_MALLOC(sizeof(ten_msg_dest_info_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_MSG_DEST_STATIC_INFO_SIGNATURE);
  ten_list_init(&self->dest);

  ten_string_init_formatted(&self->name, "%s", msg_name);

  return self;
}

void ten_msg_dest_info_destroy(ten_msg_dest_info_t *self) {
  TEN_ASSERT(self && ten_msg_dest_info_check_integrity(self),
             "Should not happen.");

  ten_signature_set(&self->signature, 0);

  ten_list_clear(&self->dest);
  ten_string_deinit(&self->name);

  TEN_FREE(self);
}

ten_shared_ptr_t *ten_msg_dest_info_clone(ten_shared_ptr_t *self,
                                          ten_list_t *extensions_info,
                                          ten_error_t *err) {
  TEN_ASSERT(self && extensions_info, "Should not happen.");

  ten_msg_dest_info_t *msg_dest_info = ten_shared_ptr_get_data(self);
  TEN_ASSERT(msg_dest_info && ten_msg_dest_info_check_integrity(msg_dest_info),
             "Should not happen.");

  const char *msg_name = ten_string_get_raw_str(&msg_dest_info->name);

  ten_msg_dest_info_t *new_self = ten_msg_dest_info_create(msg_name);

  ten_list_foreach (&msg_dest_info->dest, iter) {
    ten_weak_ptr_t *dest = ten_smart_ptr_listnode_get(iter.node);
    ten_extension_info_t *dest_extension_info =
        ten_extension_info_from_smart_ptr(dest);

    ten_shared_ptr_t *new_dest = get_extension_info_in_extensions_info(
        extensions_info,
        ten_string_get_raw_str(&dest_extension_info->loc.app_uri),
        ten_string_get_raw_str(&dest_extension_info->loc.graph_id),
        ten_string_get_raw_str(&dest_extension_info->loc.extension_group_name),
        NULL, ten_string_get_raw_str(&dest_extension_info->loc.extension_name),
        true, err);
    TEN_ASSERT(new_dest, "Should not happen.");

    if (!new_dest) {
      return NULL;
    }

    // We need to use weak_ptr here to prevent the circular shared_ptr problem
    // in the case of loop graph.
    ten_weak_ptr_t *weak_dest = ten_weak_ptr_create(new_dest);
    ten_list_push_smart_ptr_back(&new_self->dest, weak_dest);
    ten_weak_ptr_destroy(weak_dest);
  }

  return ten_shared_ptr_create(new_self, ten_msg_dest_info_destroy);
}

void ten_msg_dest_info_translate_localhost_to_app_uri(ten_msg_dest_info_t *self,
                                                      const char *uri) {
  TEN_ASSERT(self && uri, "Should not happen.");

  ten_list_foreach (&self->dest, iter) {
    ten_shared_ptr_t *shared_dest =
        ten_weak_ptr_lock(ten_smart_ptr_listnode_get(iter.node));

    ten_extension_info_t *extension_info = ten_shared_ptr_get_data(shared_dest);
    ten_extension_info_translate_localhost_to_app_uri(extension_info, uri);

    ten_shared_ptr_destroy(shared_dest);
  }
}

bool ten_msg_dest_info_qualified(ten_msg_dest_info_t *self,
                                 const char *msg_name) {
  TEN_ASSERT(self && ten_msg_dest_info_check_integrity(self) && msg_name,
             "Should not happen.");

  if (ten_c_string_is_equal(ten_string_get_raw_str(&self->name), msg_name)) {
    return true;
  }

  // "*" is a special rule that matches all names.
  if (ten_string_is_equal_c_str(&self->name, TEN_STR_STAR)) {
    return true;
  }

  return false;
}
