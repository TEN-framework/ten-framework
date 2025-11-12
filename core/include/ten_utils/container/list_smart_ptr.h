//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdbool.h>

#include "ten_utils/container/list_node.h"

/**
 * @return The smart_ptr stored in the list.
 */
TEN_UTILS_API ten_smart_ptr_t *ten_list_push_smart_ptr_back(
    ten_list_t *self, ten_smart_ptr_t *ptr);

/**
 * @param ptr The raw pointer.
 */
TEN_UTILS_API ten_listnode_t *ten_list_find_shared_ptr(ten_list_t *self,
                                                       const void *ptr);

#define ten_list_find_shared_ptr_custom(self, ptr, equal_to) \
  ten_list_find_shared_ptr_custom_(                          \
      self, ptr, (bool (*)(const void *, const void *))(equal_to))

TEN_UTILS_API ten_listnode_t *ten_list_find_shared_ptr_custom_(
    ten_list_t *self, const void *ptr,
    bool (*equal_to)(const void *, const void *));

#define ten_list_find_shared_ptr_custom_2(self, ptr_1, ptr_2, equal_to) \
  ten_list_find_shared_ptr_custom_2_(                                   \
      self, ptr_1, ptr_2,                                               \
      (bool (*)(const void *, const void *, const void *))(equal_to))

TEN_UTILS_API ten_listnode_t *ten_list_find_shared_ptr_custom_2_(
    ten_list_t *self, const void *ptr_1, const void *ptr_2,
    bool (*equal_to)(const void *, const void *, const void *));

#define ten_list_find_shared_ptr_custom_3(self, ptr_1, ptr_2, ptr_3, equal_to) \
  ten_list_find_shared_ptr_custom_3_(                                          \
      self, ptr_1, ptr_2, ptr_3,                                               \
      (bool (*)(const void *, const void *, const void *, const void *))(      \
          equal_to))

TEN_UTILS_API ten_listnode_t *ten_list_find_shared_ptr_custom_3_(
    ten_list_t *self, const void *ptr_1, const void *ptr_2, const void *ptr_3,
    bool (*equal_to)(const void *, const void *, const void *, const void *));

#define ten_list_find_shared_ptr_custom_4(self, ptr_1, ptr_2, ptr_3, ptr_4, \
                                          equal_to)                         \
  ten_list_find_shared_ptr_custom_4_(                                       \
      self, ptr_1, ptr_2, ptr_3, ptr_4,                                     \
      (bool (*)(const void *, const void *, const void *, const void *,     \
                const void *))(equal_to))

TEN_UTILS_API ten_listnode_t *ten_list_find_shared_ptr_custom_4_(
    ten_list_t *self, const void *ptr_1, const void *ptr_2, const void *ptr_3,
    const void *ptr_4,
    bool (*equal_to)(const void *, const void *, const void *, const void *,
                     const void *));
