//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/value/value_path.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/common/error_code.h"
#include "include_internal/ten_utils/value/constant_str.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

typedef enum TEN_VALUE_PATH_SNIPPET_ITEM_TYPE {
  TEN_VALUE_PATH_SNIPPET_ITEM_TYPE_INVALID,

  TEN_VALUE_PATH_SNIPPET_ITEM_TYPE_STRING,
  TEN_VALUE_PATH_SNIPPET_ITEM_TYPE_INDEX,
} TEN_VALUE_PATH_SNIPPET_ITEM_TYPE;

typedef struct ten_value_path_snippet_item_t {
  TEN_VALUE_PATH_SNIPPET_ITEM_TYPE type;

  union {
    ten_string_t str;
    size_t index;
  };
} ten_value_path_snippet_item_t;

static void ten_value_path_snippet_item_destroy(
    ten_value_path_snippet_item_t *item) {
  TEN_ASSERT(item, "Invalid argument.");

  switch (item->type) {
  case TEN_VALUE_PATH_SNIPPET_ITEM_TYPE_STRING:
    ten_string_deinit(&item->str);
    break;

  default:
    break;
  }

  TEN_FREE(item);
}

static ten_value_path_item_t *ten_value_path_item_create(void) {
  ten_value_path_item_t *item =
      (ten_value_path_item_t *)TEN_MALLOC(sizeof(ten_value_path_item_t));
  TEN_ASSERT(item, "Failed to allocate memory.");

  // Initialize all memory within `item` to 0, so that the type within it (such
  // as `ten_string_t`) recognizes it as being in an uninitialized state.
  memset(item, 0, sizeof(ten_value_path_item_t));

  item->type = TEN_VALUE_PATH_ITEM_TYPE_INVALID;

  return item;
}

static void ten_value_path_item_destroy(ten_value_path_item_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  switch (self->type) {
  case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
    ten_string_deinit(&self->obj_item_str);
    break;

  default:
    break;
  }

  TEN_FREE(self);
}

static ten_value_path_item_t *ten_value_path_parse_between_bracket(
    ten_string_t *content, bool is_first) {
  ten_value_path_item_t *item = ten_value_path_item_create();

  if (is_first) {
    item->type = TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM;
    ten_string_init_from_string(&item->obj_item_str, content);
  } else {
    if (!ten_c_string_is_equal(
            ten_string_get_raw_str(content) + ten_string_len(content) - 1,
            TEN_STR_VALUE_PATH_ARRAY_END)) {
      // It's not a valid array specifier, i.e., starting from '[', but not
      // ended with ']'.
      goto error;
    } else {
      ten_string_erase_back(content, 1);

      if (ten_c_string_is_equal(
              ten_string_get_raw_str(content) + ten_string_len(content) - 1,
              TEN_STR_VALUE_PATH_ARRAY_END)) {
        // It's not a valid array specifier, i.e., starting from '[', but
        // ended with multiple ']'.
        goto error;
      }
    }

    item->type = TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM;
    item->arr_idx = strtol(ten_string_get_raw_str(content), NULL, 10);
  }

  return item;

error:
  ten_value_path_item_destroy(item);
  return NULL;
}

static bool ten_value_path_parse_between_colon(ten_string_t *path,
                                               ten_list_t *result) {
  TEN_ASSERT(path, "Invalid argument.");
  TEN_ASSERT(result, "Invalid argument.");

  bool rc = true;

  ten_list_t split_by_bracket = TEN_LIST_INIT_VAL;
  ten_string_split(path, TEN_STR_VALUE_PATH_ARRAY_START, &split_by_bracket);

  ten_list_foreach (&split_by_bracket, iter) {
    ten_string_t *content_between_bracket = ten_str_listnode_get(iter.node);
    TEN_ASSERT(content_between_bracket, "Invalid argument.");

    ten_value_path_item_t *item = ten_value_path_parse_between_bracket(
        content_between_bracket, iter.index == 0 ? true : false);

    if (!item) {
      rc = false;
      goto done;
    }

    ten_list_push_ptr_back(
        result, item,
        (ten_ptr_listnode_destroy_func_t)ten_value_path_item_destroy);
  }

done:
  ten_list_clear(&split_by_bracket);
  return rc;
}

bool ten_value_path_parse(const char *path, ten_list_t *result,
                          ten_error_t *err) {
  TEN_ASSERT(path, "Invalid argument.");
  TEN_ASSERT(result, "Invalid argument.");

  if (!path || !strlen(path)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "path should not be empty.");
    }
    return false;
  }

  bool rc = true;

  ten_list_t split_by_colon = TEN_LIST_INIT_VAL;
  ten_c_string_split(path, TEN_STR_VALUE_PATH_OBJECT_DELIMITER,
                     &split_by_colon);

  ten_list_foreach (&split_by_colon, colon_iter) {
    ten_string_t *content_between_colon = ten_str_listnode_get(colon_iter.node);
    TEN_ASSERT(content_between_colon, "Invalid argument.");

    if (!ten_value_path_parse_between_colon(content_between_colon, result)) {
      ten_list_clear(result);

      rc = false;
      goto done;
    }
  }

done:
  ten_list_clear(&split_by_colon);
  return rc;
}

ten_value_t *ten_value_peek_from_path(ten_value_t *base, const char *path,
                                      ten_error_t *err) {
  TEN_ASSERT(base, "Invalid argument.");
  TEN_ASSERT(path, "Invalid argument.");

  ten_value_t *result = NULL;

  ten_list_t path_items = TEN_LIST_INIT_VAL;
  if (!ten_value_path_parse(path, &path_items, err)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Failed to parse the path.");
    }
    goto done;
  }

  ten_list_foreach (&path_items, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_INVALID:
      TEN_ASSERT(0, "Should not happen.");

      result = NULL;
      goto done;

    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
      if (base->type != TEN_TYPE_OBJECT) {
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                        "Path is not corresponding to the value type.");
        }
        result = NULL;
        goto done;
      }

      ten_list_foreach (&base->content.object, object_iter) {
        ten_value_kv_t *kv = ten_ptr_listnode_get(object_iter.node);
        TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Invalid argument.");

        if (ten_string_is_equal(&kv->key, &item->obj_item_str)) {
          if (item_iter.next) {
            base = kv->value;
          } else {
            result = kv->value;
          }
          break;
        }
      }
      break;

    case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
      if (base->type != TEN_TYPE_ARRAY) {
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                        "Path is not corresponding to the value type.");
        }
        result = NULL;
        goto done;
      }

      ten_list_foreach (&base->content.array, array_iter) {
        if (array_iter.index == item->arr_idx) {
          ten_value_t *array_item = ten_ptr_listnode_get(array_iter.node);
          TEN_ASSERT(array_item, "Invalid argument.");

          if (item_iter.next) {
            base = array_item;
          } else {
            result = array_item;
          }
          break;
        }
      }
      break;

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
  }

done:
  ten_list_clear(&path_items);

  if (!result) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_VALUE_NOT_FOUND,
                    "Failed to find value: %s", path);
    }
  }

  return result;
}

bool ten_value_set_from_path_list_with_move(ten_value_t *base,
                                            ten_list_t *paths,
                                            ten_value_t *value,
                                            TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(base, "Invalid argument.");
  TEN_ASSERT(paths, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  bool result = true;

  ten_list_foreach (paths, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_INVALID:
      result = false;
      goto done;

    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM: {
      if (base->type != TEN_TYPE_OBJECT) {
        result = false;
        goto done;
      }

      bool found = false;

      ten_list_foreach (&base->content.object, object_iter) {
        ten_value_kv_t *kv = ten_ptr_listnode_get(object_iter.node);
        TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Invalid argument.");

        if (ten_string_is_equal(&kv->key, &item->obj_item_str)) {
          found = true;

          if (item_iter.next == NULL) {
            // Override the original value.
            ten_value_destroy(kv->value);
            kv->value = value;
          } else {
            ten_value_path_item_t *next_item =
                ten_ptr_listnode_get(item_iter.next);
            TEN_ASSERT(next_item, "Invalid argument.");

            switch (next_item->type) {
            case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
              if (!ten_value_is_object(kv->value)) {
                ten_value_destroy(kv->value);
                kv->value = ten_value_create_object_with_move(NULL);
              }
              break;
            case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
              if (!ten_value_is_array(kv->value)) {
                ten_value_destroy(kv->value);
                kv->value = ten_value_create_array_with_move(NULL);
              }
              break;
            default:
              TEN_ASSERT(0, "Should not happen.");
              break;
            }
          }

          base = kv->value;
          break;
        }
      }

      if (!found) {
        ten_value_t *new_base = NULL;

        if (item_iter.next == NULL) {
          // Override the original value.
          new_base = value;
        } else {
          ten_value_path_item_t *next_item =
              ten_ptr_listnode_get(item_iter.next);
          TEN_ASSERT(next_item, "Invalid argument.");

          switch (next_item->type) {
          case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
            new_base = ten_value_create_object_with_move(NULL);
            break;
          case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
            new_base = ten_value_create_array_with_move(NULL);
            break;
          default:
            TEN_ASSERT(0, "Should not happen.");
            break;
          }
        }

        ten_list_push_ptr_back(
            &base->content.object,
            ten_value_kv_create(ten_string_get_raw_str(&item->obj_item_str),
                                new_base),
            (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

        base = new_base;
      }
      break;
    }

    case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM: {
      if (base->type != TEN_TYPE_ARRAY) {
        result = NULL;
        goto done;
      }

      bool found = false;

      ten_list_foreach (&base->content.array, array_iter) {
        if (array_iter.index == item->arr_idx) {
          found = true;

          ten_listnode_t *array_item_node = array_iter.node;
          TEN_ASSERT(array_item_node, "Invalid argument.");

          ten_value_t *old_value = ten_ptr_listnode_get(array_item_node);
          if (item_iter.next == NULL) {
            // Override the original value.
            ten_value_destroy(old_value);

            ten_ptr_listnode_replace(
                array_item_node, value,
                (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
          } else {
            ten_value_path_item_t *next_item =
                ten_ptr_listnode_get(item_iter.next);
            TEN_ASSERT(next_item, "Invalid argument.");

            switch (next_item->type) {
            case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
              if (!ten_value_is_object(old_value)) {
                ten_value_destroy(old_value);

                ten_ptr_listnode_replace(
                    array_item_node, ten_value_create_object_with_move(NULL),
                    (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
              }
              break;
            case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
              if (!ten_value_is_array(old_value)) {
                ten_value_destroy(old_value);

                ten_ptr_listnode_replace(
                    array_item_node, ten_value_create_array_with_move(NULL),
                    (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
              }
              break;
            default:
              TEN_ASSERT(0, "Should not happen.");
              break;
            }
          }

          base = ten_ptr_listnode_get(array_item_node);
          break;
        }
      }

      if (!found) {
        for (size_t i = ten_list_size(&base->content.array); i < item->arr_idx;
             i++) {
          ten_list_push_ptr_back(
              &base->content.array, ten_value_create_invalid(),
              (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
        }

        ten_value_t *new_base = NULL;

        if (item_iter.next == NULL) {
          new_base = value;
        } else {
          ten_value_path_item_t *next_item =
              ten_ptr_listnode_get(item_iter.next);
          TEN_ASSERT(next_item, "Invalid argument.");

          switch (next_item->type) {
          case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM:
            new_base = ten_value_create_object_with_move(NULL);
            break;
          case TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM:
            new_base = ten_value_create_array_with_move(NULL);
            break;
          default:
            TEN_ASSERT(0, "Should not happen.");
            break;
          }
        }

        ten_list_push_ptr_back(
            &base->content.array, new_base,
            (ten_ptr_listnode_destroy_func_t)ten_value_destroy);

        base = new_base;
      }
      break;
    }

    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
  }

done:
  return result;
}

bool ten_value_set_from_path_str_with_move(ten_value_t *base, const char *path,
                                           ten_value_t *value,
                                           ten_error_t *err) {
  TEN_ASSERT(base, "Invalid argument.");
  TEN_ASSERT(path, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  bool result = true;

  ten_list_t paths = TEN_LIST_INIT_VAL;
  if (!ten_value_path_parse(path, &paths, err)) {
    goto done;
  }

  ten_value_set_from_path_list_with_move(base, &paths, value, err);

done:
  ten_list_clear(&paths);

  return result;
}
