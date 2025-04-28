//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

bool ten_value_object_merge_with_move(ten_value_t *dest, ten_value_t *src) {
  if (!dest || !src) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  if (!ten_value_is_object(dest) || !ten_value_is_object(src)) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  ten_value_object_foreach(src, src_iter) {
    // Detach the src_item from src.
    ten_list_detach_node(&src->content.object, src_iter.node);

    ten_value_kv_t *src_item = ten_ptr_listnode_get(src_iter.node);
    TEN_ASSERT(src_item && ten_value_kv_check_integrity(src_item),
               "Should not happen.");

    bool found = false;

    ten_value_object_foreach(dest, dest_iter) {
      ten_value_kv_t *dest_item = ten_ptr_listnode_get(dest_iter.node);
      TEN_ASSERT(dest_item && ten_value_kv_check_integrity(dest_item),
                 "Should not happen.");

      if (ten_string_is_equal(&src_item->key, &dest_item->key)) {
        if (ten_value_is_object(src_item->value) &&
            ten_value_is_object(dest_item->value)) {
          ten_value_object_merge_with_move(dest_item->value, src_item->value);
        } else {
          ten_value_kv_reset_to_value(dest_item, src_item->value);
          src_item->value = NULL;
        }

        found = true;
      }
    }

    if (found) {
      ten_listnode_destroy(src_iter.node);
    } else {
      // Move the src_item to dest.
      ten_list_push_back(&dest->content.object, src_iter.node);
    }
  }

  return true;
}

/**
 * @brief Merge value from @a src to @a dest.
 */
bool ten_value_object_merge_with_clone(ten_value_t *dest, ten_value_t *src) {
  if (!dest || !src) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  if (!ten_value_is_object(dest) || !ten_value_is_object(src)) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  // Iterate over the source object.
  ten_value_object_foreach(src, src_iter) {
    ten_value_kv_t *src_item = ten_ptr_listnode_get(src_iter.node);
    TEN_ASSERT(src_item && ten_value_kv_check_integrity(src_item),
               "Should not happen.");

    bool found = false;

    // Iterate over the destination object to find a matching key.
    ten_value_object_foreach(dest, dest_iter) {
      ten_value_kv_t *dest_item = ten_ptr_listnode_get(dest_iter.node);
      TEN_ASSERT(dest_item && ten_value_kv_check_integrity(dest_item),
                 "Should not happen.");

      if (ten_string_is_equal(&src_item->key, &dest_item->key)) {
        found = true;

        // If both values are objects, merge them recursively.
        if (ten_value_is_object(src_item->value) &&
            ten_value_is_object(dest_item->value)) {
          if (!ten_value_object_merge_with_clone(dest_item->value,
                                                 src_item->value)) {
            return false;
          }
        } else {
          // Otherwise, clone the source value and replace the destination
          // value.
          ten_value_t *new_src = ten_value_clone(src_item->value);
          TEN_ASSERT(new_src, "Should not happen.");

          ten_value_kv_reset_to_value(dest_item, new_src);
        }

        break;
      }
    }

    // If no matching key was found, clone the source item and add it to the
    // destination.
    if (!found) {
      ten_value_kv_t *new_src = ten_value_kv_clone(src_item);
      TEN_ASSERT(new_src, "Should not happen.");

      ten_list_push_ptr_back(
          &dest->content.object, new_src,
          (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
    }
  }

  return true;
}

bool ten_value_object_merge_with_json(ten_value_t *dest, ten_json_t *src) {
  if (!dest || !src) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  if (!ten_value_is_object(dest) || !ten_json_is_object(src)) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  const char *key = NULL;
  ten_json_t *prop_json = NULL;
  ten_json_object_foreach(src, key, prop_json) {
    bool found = false;

    ten_value_object_foreach(dest, dest_iter) {
      ten_value_kv_t *dest_item = ten_ptr_listnode_get(dest_iter.node);
      TEN_ASSERT(dest_item && ten_value_kv_check_integrity(dest_item),
                 "Should not happen.");

      if (ten_string_is_equal_c_str(&dest_item->key, key)) {
        if (ten_json_is_object(prop_json) &&
            ten_value_is_object(dest_item->value)) {
          ten_value_object_merge_with_json(dest_item->value, prop_json);
        } else {
          ten_value_t *src_value = ten_value_from_json(prop_json);
          TEN_ASSERT(src_value, "Should not happen.");

          ten_value_kv_reset_to_value(dest_item, src_value);
        }

        found = true;
      }
    }

    if (!found) {
      // Clone the src_item to dest.
      ten_value_kv_t *src_kv =
          ten_value_kv_create(key, ten_value_from_json(prop_json));
      TEN_ASSERT(src_kv, "Should not happen.");

      ten_list_push_ptr_back(
          &dest->content.object, src_kv,
          (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
    }
  }

  return true;
}
