//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/error_code.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

bool ten_value_object_unset(ten_value_t *self, const char *key) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(key, "Invalid argument.");

  if (!ten_value_is_object(self)) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  ten_list_foreach (&self->content.object, iter) {
    ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(kv, "Invalid argument.");
    TEN_ASSERT(ten_value_kv_check_integrity(kv), "Invalid argument.");

    if (ten_string_is_equal_c_str(&kv->key, key)) {
      // Found the key, remove the node from the list
      ten_listnode_t *node = iter.node;
      ten_list_remove_node(&self->content.object, node);

      // The destructor will be called automatically when removing the node
      // which will clean up the kv and its value
      return true;
    }
  }

  // Key not found
  return false;
}

bool ten_value_unset_from_path(ten_value_t *base, const char *path,
                               ten_error_t *err) {
  TEN_ASSERT(base, "Invalid argument.");
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || !strlen(path)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "path should not be empty.");
    }
    return false;
  }

  ten_list_t path_items = TEN_LIST_INIT_VAL;
  if (!ten_value_path_parse(path, &path_items, err)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Failed to parse the path.");
    }
    goto done;
  }

  // For unset operation, we need to find the parent object of the target key
  // and remove the last key from that parent object

  size_t path_size = ten_list_size(&path_items);
  if (path_size == 0) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Invalid path for unset operation.");
    }
    ten_list_clear(&path_items);
    return false;
  }

  ten_value_t *current = base;
  ten_listnode_t *last_node = ten_list_back(&path_items);

  // Navigate to the parent of the target key
  ten_list_foreach (&path_items, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    // If this is the last item, we need to unset it
    if (item_iter.node == last_node) {
      switch (item->type) {
      case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
        if (current->type != TEN_TYPE_OBJECT) {
          if (err) {
            ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                          "Path does not correspond to an object type.");
          }
          ten_list_clear(&path_items);
          return false;
        }

        bool result = ten_value_object_unset(
            current, ten_string_get_raw_str(&item->obj_item_str));
        ten_list_clear(&path_items);
        return result;

      case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
        if (current->type != TEN_TYPE_ARRAY) {
          if (err) {
            ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                          "Path does not correspond to an array type.");
          }
          ten_list_clear(&path_items);
          return false;
        }

        // For arrays, find and remove the item at the specified index
        ten_list_foreach (&current->content.array, array_iter) {
          if (array_iter.index == item->arr_idx) {
            ten_listnode_t *array_node = array_iter.node;
            ten_list_remove_node(&current->content.array, array_node);
            ten_list_clear(&path_items);
            return true;
          }
        }

        // Index not found
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_VALUE_NOT_FOUND,
                        "Array index %zu not found.", item->arr_idx);
        }
        ten_list_clear(&path_items);
        return false;

      default:
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                        "Invalid path item type for unset operation.");
        }
        ten_list_clear(&path_items);
        return false;
      }
    } else {
      // Navigate to the next level
      switch (item->type) {
      case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
        if (current->type != TEN_TYPE_OBJECT) {
          if (err) {
            ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                          "Path does not correspond to the value type.");
          }
          ten_list_clear(&path_items);
          return false;
        }

        bool found = false;
        ten_list_foreach (&current->content.object, object_iter) {
          ten_value_kv_t *kv = ten_ptr_listnode_get(object_iter.node);
          TEN_ASSERT(kv, "Invalid argument.");
          TEN_ASSERT(ten_value_kv_check_integrity(kv), "Invalid argument.");

          if (ten_string_is_equal(&kv->key, &item->obj_item_str)) {
            current = kv->value;
            found = true;
            break;
          }
        }

        if (!found) {
          if (err) {
            ten_error_set(err, TEN_ERROR_CODE_VALUE_NOT_FOUND,
                          "Object key not found: %s",
                          ten_string_get_raw_str(&item->obj_item_str));
          }
          ten_list_clear(&path_items);
          return false;
        }
        break;

      case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
        if (current->type != TEN_TYPE_ARRAY) {
          if (err) {
            ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                          "Path does not correspond to the value type.");
          }
          ten_list_clear(&path_items);
          return false;
        }

        found = false;
        ten_list_foreach (&current->content.array, array_iter) {
          if (array_iter.index == item->arr_idx) {
            ten_value_t *array_item = ten_ptr_listnode_get(array_iter.node);
            TEN_ASSERT(array_item, "Invalid argument.");
            current = array_item;
            found = true;
            break;
          }
        }

        if (!found) {
          if (err) {
            ten_error_set(err, TEN_ERROR_CODE_VALUE_NOT_FOUND,
                          "Array index %zu not found.", item->arr_idx);
          }
          ten_list_clear(&path_items);
          return false;
        }
        break;

      default:
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                        "Invalid path item type.");
        }
        ten_list_clear(&path_items);
        return false;
      }
    }
  }

done:
  ten_list_clear(&path_items);
  return false;
}
