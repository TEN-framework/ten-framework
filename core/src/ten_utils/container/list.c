//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/container/list.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_list_check_integrity(ten_list_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_LIST_SIGNATURE) {
    return false;
  }

  if (!self->size) {
    if (self->front != NULL || self->back != NULL) {
      // Conflicting between 'size' and actual 'data'.
      return false;
    }
  } else {
    if (self->front == NULL || self->back == NULL) {
      return false;
    }

    if (self->size == 1) {
      if (self->front != self->back) {
        return false;
      }
    } else {
      if (self->front == self->back) {
        return false;
      }
    }

    if (self->front->prev != NULL) {
      return false;
    }
    if (self->back->next != NULL) {
      return false;
    }
  }
  return true;
}

ten_list_t *ten_list_create(void) {
  ten_list_t *self = (ten_list_t *)TEN_MALLOC(sizeof(ten_list_t));
  TEN_ASSERT(self, "Failed to allocate memory.");
  ten_list_init(self);
  return self;
}

void ten_list_destroy(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");
  ten_list_clear(self);
  TEN_FREE(self);
}

void ten_list_init(ten_list_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_LIST_SIGNATURE);
  self->size = 0;
  self->front = NULL;
  self->back = NULL;
}

void ten_list_clear(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  ten_list_foreach (self, iter) {
    ten_listnode_destroy(iter.node);
  }

  self->size = 0;
  self->front = NULL;
  self->back = NULL;
}

void ten_list_reset(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  self->size = 0;
  self->front = NULL;
  self->back = NULL;
}

bool ten_list_is_empty(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");
  return !ten_list_size(self);
}

size_t ten_list_size(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");
  return self->size;
}

void ten_list_swap(ten_list_t *self, ten_list_t *target) {
  TEN_ASSERT(self && target && ten_list_check_integrity(self) &&
                 ten_list_check_integrity(target),
             "Invalid argument.");

  ten_listnode_t *self_front = self->front;
  ten_listnode_t *self_back = self->back;
  size_t self_size = self->size;

  self->front = target->front;
  self->back = target->back;
  self->size = target->size;

  target->front = self_front;
  target->back = self_back;
  target->size = self_size;
}

void ten_list_concat(ten_list_t *self, ten_list_t *target) {
  TEN_ASSERT(self && target && ten_list_check_integrity(self) &&
                 ten_list_check_integrity(target),
             "Invalid argument.");

  if (ten_list_size(target) == 0) {
    return;
  }

  if (ten_list_size(self) == 0) {
    ten_list_swap(self, target);
    return;
  }

  self->back->next = target->front;
  target->front->prev = self->back;
  self->back = target->back;
  self->size += target->size;

  target->front = NULL;
  target->back = NULL;
  target->size = 0;
}

void ten_list_detach_node(ten_list_t *self, ten_listnode_t *curr_node) {
  if (!self || !ten_list_check_integrity(self)) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  if (ten_list_is_empty(self)) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  if (!curr_node) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  if (ten_list_size(self) == 1) {
    TEN_ASSERT(self->front == curr_node, "Invalid list.");

    self->front = NULL;
    self->back = NULL;
  } else {
    if (curr_node == self->front) {
      TEN_ASSERT(curr_node->prev == NULL && curr_node->next->prev == curr_node,
                 "Invalid list.");

      self->front->next->prev = NULL;
      self->front = self->front->next;
    } else if (curr_node == self->back) {
      TEN_ASSERT(curr_node->next == NULL && curr_node->prev->next == curr_node,
                 "Invalid list.");

      self->back->prev->next = NULL;
      self->back = self->back->prev;
    } else {
      TEN_ASSERT(curr_node->prev->next == curr_node &&
                     curr_node->next->prev == curr_node,
                 "Invalid list.");

      curr_node->prev->next = curr_node->next;
      curr_node->next->prev = curr_node->prev;
    }
  }

  --self->size;
}

void ten_list_remove_node(ten_list_t *self, ten_listnode_t *node) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && node &&
                 !ten_list_is_empty(self),
             "Invalid argument.");

  ten_list_detach_node(self, node);
  ten_listnode_destroy(node);
}

ten_listnode_t *ten_list_front(ten_list_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_list_check_integrity(self), "Invalid argument.");
  return self->front;
}

ten_listnode_t *ten_list_back(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");
  return self->back;
}

void ten_list_push_front(ten_list_t *self, ten_listnode_t *node) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && node,
             "Invalid argument.");

  if (ten_list_is_empty(self)) {
    self->back = self->front = node;
    node->prev = node->next = NULL;
  } else {
    node->next = self->front;
    node->prev = NULL;

    self->front->prev = node;
    self->front = node;
  }
  ++self->size;
}

void ten_list_push_back(ten_list_t *self, ten_listnode_t *node) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && node,
             "Invalid argument.");

  if (ten_list_is_empty(self)) {
    self->front = self->back = node;
    node->next = node->prev = NULL;
  } else {
    node->next = NULL;
    node->prev = self->back;

    self->back->next = node;
    self->back = node;
  }
  ++self->size;
}

ten_listnode_t *ten_list_pop_front(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  if (ten_list_is_empty(self)) {
    return NULL;
  }
  ten_listnode_t *node = self->front;
  if (ten_list_size(self) == 1) {
    self->back = self->front = NULL;
    node->prev = node->next = NULL;
  } else {
    self->front = self->front->next;
    self->front->prev = NULL;

    node->next = NULL;
  }
  --self->size;

  return node;
}

void ten_list_remove_front(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  ten_listnode_t *node = ten_list_pop_front(self);
  ten_listnode_destroy(node);
}

ten_listnode_t *ten_list_pop_back(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  if (ten_list_is_empty(self)) {
    return NULL;
  }
  ten_listnode_t *node = self->back;
  if (ten_list_size(self) == 1) {
    self->front = self->back = NULL;
    node->prev = node->next = NULL;
  } else {
    self->back = self->back->prev;
    self->back->next = NULL;

    node->prev = NULL;
  }
  --self->size;

  return node;
}

bool ten_list_push_back_in_order(ten_list_t *self, ten_listnode_t *node,
                                 ten_list_node_compare_func_t cmp,
                                 bool skip_if_same) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && node && cmp,
             "Invalid argument.");

  if (ten_list_is_empty(self)) {
    ten_list_push_back(self, node);
    return true;
  }

  ten_list_foreach (self, iter) {
    int result = cmp(iter.node, node);
    if (result == 0 && skip_if_same) {
      return false;
    }

    if (result > 0) {
      continue;
    }

    if (iter.node == ten_list_front(self)) {
      ten_list_push_front(self, node);
      return true;
    }

    iter.prev->next = node;
    node->prev = iter.prev;
    node->next = iter.node;
    iter.node->prev = node;
    ++self->size;
    return true;
  }

  ten_list_push_back(self, node);
  return true;
}

ten_list_iterator_t ten_list_begin(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  return (ten_list_iterator_t){
      NULL,
      ten_list_front(self),
      ten_list_front(self) ? ten_list_front(self)->next : NULL,
      0,
  };
}

ten_list_iterator_t ten_list_end(ten_list_t *self) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  if (ten_list_size(self)) {
    return (ten_list_iterator_t){
        ten_list_back(self) ? ten_list_back(self)->prev : NULL,
        ten_list_back(self),
        NULL,
        ten_list_size(self) - 1,
    };
  } else {
    return (ten_list_iterator_t){
        NULL,
        NULL,
        NULL,
        0,
    };
  }
}

ten_list_iterator_t ten_list_iterator_next(ten_list_iterator_t self) {
  return (ten_list_iterator_t){self.node, self.next,
                               self.next ? (self.next)->next : NULL,
                               self.index + 1};
}

ten_list_iterator_t ten_list_iterator_prev(ten_list_iterator_t self) {
  return (ten_list_iterator_t){self.prev ? (self.prev)->prev : NULL, self.prev,
                               self.node, self.index > 0 ? self.index - 1 : 0};
}

bool ten_list_iterator_is_end(ten_list_iterator_t self) {
  return self.node == NULL ? true : false;
}

ten_listnode_t *ten_list_iterator_to_listnode(ten_list_iterator_t self) {
  return self.node;
}

void ten_list_reverse_new(ten_list_t *src, ten_list_t *dest) {
  TEN_ASSERT(src && ten_list_check_integrity(src), "Invalid argument.");
  TEN_ASSERT(dest && ten_list_check_integrity(dest), "Invalid argument.");

  ten_list_foreach (src, iter) {
    ten_listnode_t *node = iter.node;
    TEN_ASSERT(node, "Invalid argument.");

    ten_list_push_front(dest, node);
  }

  ten_list_init(src);
}

void ten_list_reverse(ten_list_t *src) {
  TEN_ASSERT(src && ten_list_check_integrity(src), "Invalid argument.");

  ten_list_t dest = TEN_LIST_INIT_VAL;
  ten_list_foreach (src, iter) {
    ten_listnode_t *node = iter.node;
    TEN_ASSERT(node, "Invalid argument.");

    ten_list_push_front(&dest, node);
  }

  ten_list_init(src);

  ten_list_swap(&dest, src);
}
