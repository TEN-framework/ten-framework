//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ten_utils/container/list_node.h"

#define ten_list_find_ptr_custom(self, ptr, equal_to)  \
  ten_list_find_ptr_custom_(self, (const void *)(ptr), \
                            (bool (*)(const void *, const void *))(equal_to));

TEN_UTILS_API ten_listnode_t *ten_list_find_ptr_custom_(
    ten_list_t *self, const void *ptr,
    bool (*equal_to)(const void *, const void *));

#define ten_list_find_ptr_cnt_custom(self, ptr, equal_to) \
  ten_list_find_ptr_cnt_custom_(                          \
      self, (const void *)(ptr),                          \
      (bool (*)(const void *, const void *))(equal_to));

TEN_UTILS_API size_t
ten_list_find_ptr_cnt_custom_(ten_list_t *self, const void *ptr,
                              bool (*equal_to)(const void *, const void *));

#define ten_list_cnt_ptr_custom(self, predicate) \
  ten_list_cnt_ptr_custom_(self, (bool (*)(const void *))(predicate));

TEN_UTILS_API size_t ten_list_cnt_ptr_custom_(ten_list_t *self,
                                              bool (*predicate)(const void *));

TEN_UTILS_API ten_listnode_t *ten_list_find_ptr(ten_list_t *self,
                                                const void *ptr);

TEN_UTILS_API bool ten_list_remove_ptr(ten_list_t *self, void *ptr);

TEN_UTILS_API void ten_list_push_ptr_back(
    ten_list_t *self, void *ptr, ten_ptr_listnode_destroy_func_t destroy);

TEN_UTILS_API void ten_list_push_ptr_front(
    ten_list_t *self, void *ptr, ten_ptr_listnode_destroy_func_t destroy);
