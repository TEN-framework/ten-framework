//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "ten_utils/container/list_int32.h"  // IWYU pragma: keep
#include "ten_utils/container/list_node.h"
#include "ten_utils/container/list_ptr.h"        // IWYU pragma: keep
#include "ten_utils/container/list_smart_ptr.h"  // IWYU pragma: keep
#include "ten_utils/container/list_str.h"        // IWYU pragma: keep
#include "ten_utils/lib/signature.h"

#define TEN_LIST_SIGNATURE 0x5D6CC60B9833B104U
#define TEN_LIST_LOOP_MAX_ALLOWABLE_CNT 100000

#define ten_list_foreach(list, iter)                                           \
  TEN_ASSERT(ten_list_size(list) <= TEN_LIST_LOOP_MAX_ALLOWABLE_CNT,           \
             "The time complexity is too high.");                              \
  for (ten_list_iterator_t iter = {NULL, ten_list_front(list),                 \
                                   ten_list_front(list)                        \
                                       ? ten_list_front(list)->next            \
                                       : NULL,                                 \
                                   0};                                         \
       (iter).node;                                                            \
       ++((iter).index), (iter).prev = (iter).node, (iter).node = (iter).next, \
                           (iter).next = (iter).node ? ((iter).node)->next     \
                                                     : NULL)

#define TEN_LIST_INIT_VAL                                                   \
  (ten_list_t) {                                                            \
    .signature = TEN_LIST_SIGNATURE, .size = 0, .front = NULL, .back = NULL \
  }

typedef struct ten_list_t {
  ten_signature_t signature;
  size_t size;
  ten_listnode_t *front;
  ten_listnode_t *back;
} ten_list_t;

typedef struct ten_list_iterator_t {
  ten_listnode_t *prev;
  ten_listnode_t *node;
  ten_listnode_t *next;
  size_t index;
} ten_list_iterator_t;

TEN_UTILS_API bool ten_list_check_integrity(ten_list_t *self);

/**
 * @brief Create a list object.
 * @return A pointer to the list object.
 */
TEN_UTILS_API ten_list_t *ten_list_create(void);

/**
 * @brief Destroy a list object and release the memory.
 * @param self The list object.
 */
TEN_UTILS_API void ten_list_destroy(ten_list_t *self);

/**
 * @brief Initialize a list.
 * @param self The list to be initialized.
 */
TEN_UTILS_API void ten_list_init(ten_list_t *self);

/**
 * @brief Reset a list to an empty list.
 * @param self The list to be reset.
 */
TEN_UTILS_API void ten_list_reset(ten_list_t *self);

/**
 * @brief Clear a list and release all the nodes.
 * @param self The list to be cleared.
 */
TEN_UTILS_API void ten_list_clear(ten_list_t *self);

/**
 * @brief Check if a list is empty.
 * @param self The list to be checked.
 * @return true if the list is empty, false otherwise.
 */
TEN_UTILS_API bool ten_list_is_empty(ten_list_t *self);

/**
 * @brief Get the size of a list.
 * @param self The list to be checked.
 * @return the size of the list.
 */
TEN_UTILS_API size_t ten_list_size(ten_list_t *self);

/**
 * @brief Swap two lists.
 * @param self The list to be swapped.
 * @param target The target list to be swapped.
 */
TEN_UTILS_API void ten_list_swap(ten_list_t *self, ten_list_t *target);

/**
 * @brief Concatenate two lists.
 * @param self The list to be concatenated.
 * @param target The target list to be concatenated.
 */
TEN_UTILS_API void ten_list_concat(ten_list_t *self, ten_list_t *target);

/**
 * @brief Remove a node from a list and keep node memory.
 * @param self The list to be removed from.
 * @param node The node to be removed.
 */
TEN_UTILS_API void ten_list_detach_node(ten_list_t *self, ten_listnode_t *node);

/**
 * @brief Remove a node from a list and release node memory.
 * @param self The list to be removed from.
 * @param node The node to be removed.
 */
TEN_UTILS_API void ten_list_remove_node(ten_list_t *self, ten_listnode_t *node);

/**
 * @brief Get the front node of a list.
 * @param self The list to be checked.
 * @return the front node of the list. NULL if the list is empty.
 */
TEN_UTILS_API ten_listnode_t *ten_list_front(ten_list_t *self);

/**
 * @brief Get the back node of a list.
 * @param self The list to be checked.
 * @return the back node of the list. NULL if the list is empty.
 */
TEN_UTILS_API ten_listnode_t *ten_list_back(ten_list_t *self);

/**
 * @brief Push a node to the front of a list.
 * @param self The list to be pushed to.
 * @param node The node to be pushed.
 */
TEN_UTILS_API void ten_list_push_front(ten_list_t *self, ten_listnode_t *node);

/**
 * @brief Push a node to the back of a list.
 * @param self The list to be pushed to.
 * @param node The node to be pushed.
 */
TEN_UTILS_API void ten_list_push_back(ten_list_t *self, ten_listnode_t *node);

/**
 * @brief Pop the front node of a list.
 * @param self The list to be popped from.
 * @return the front node of the list. NULL if the list is empty.
 */
TEN_UTILS_API ten_listnode_t *ten_list_pop_front(ten_list_t *self);

TEN_UTILS_API void ten_list_remove_front(ten_list_t *self);

/**
 * @brief Pop the back node of a list.
 * @param self The list to be popped from.
 * @return the back node of the list. NULL if the list is empty.
 */
TEN_UTILS_API ten_listnode_t *ten_list_pop_back(ten_list_t *self);

/**
 * @return
 *  * 1 if left > right
 *  * 0 if left == right
 *  * -1 if left < right
 */
typedef int (*ten_list_node_compare_func_t)(ten_listnode_t *left,
                                            ten_listnode_t *right);

/**
 * @brief Insert a node into a list in order. If the compare function cmp(x, y)
 * returns true, then the node x will stand before the node y in the list.
 *
 * @param self The list to be inserted to.
 * @param node The node to be inserted.
 * @param cmp The compare function.
 * @param skip_if_same If skip_if_same is true, this node won't be pushed into
 * the list if one item in the list equals to it (i.e., cmp(x, node) == 0), and
 * this function will return false.
 *
 * @return Whether this node has been pushed into the list. You have to care
 * about the memory of this node if return false.
 */
TEN_UTILS_API bool ten_list_push_back_in_order(ten_list_t *self,
                                               ten_listnode_t *node,
                                               ten_list_node_compare_func_t cmp,
                                               bool skip_if_same);

TEN_UTILS_API ten_list_iterator_t ten_list_begin(ten_list_t *self);

TEN_UTILS_API ten_list_iterator_t ten_list_end(ten_list_t *self);

TEN_UTILS_API ten_list_iterator_t
ten_list_iterator_next(ten_list_iterator_t self);

TEN_UTILS_API ten_list_iterator_t
ten_list_iterator_prev(ten_list_iterator_t self);

TEN_UTILS_API bool ten_list_iterator_is_end(ten_list_iterator_t self);

TEN_UTILS_API ten_listnode_t *ten_list_iterator_to_listnode(
    ten_list_iterator_t self);

TEN_UTILS_API void ten_list_reverse_new(ten_list_t *src, ten_list_t *dest);

TEN_UTILS_API void ten_list_reverse(ten_list_t *src);
