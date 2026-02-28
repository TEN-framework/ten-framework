//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/typed_list_node.h"

#define TEN_TYPED_LIST_SIGNATURE 0xF77BB44C7D13991FU

#define ten_typed_list_foreach(T, list, iter) \
  ten_typed_list_foreach_(T, list, iter)
#define ten_typed_list_foreach_(T, list, iter)                                 \
  for (ten_typed_##T##_list_iterator_t iter =                                  \
           {                                                                   \
               NULL,                                                           \
               ten_typed_##T##_list_front(list),                               \
               ten_typed_##T##_list_front(list)                                \
                   ? ten_typed_##T##_list_front(list)->next                    \
                   : NULL,                                                     \
               0,                                                              \
           };                                                                  \
       (iter).node;                                                            \
       ++((iter).index), (iter).prev = (iter).node, (iter).node = (iter).next, \
                                       (iter).next = (iter).node               \
                                                         ? ((iter).node)->next \
                                                         : NULL)

#define DECLARE_LIST(T) DECLARE_LIST_(T)
#define DECLARE_LIST_(T) \
  typedef struct ten_typed_##T##_list_t ten_typed_##T##_list_t

// Because T is surrounded by ## in TEN_DEFINE_TYPED_LIST_(T), and that would
// prevent T from expanding itself, so the following TEN_DEFINE_TYPED_LIST() is
// to ensure T is expanded first before entering into TEN_DEFINE_TYPED_LIST_(T)
#define TEN_DEFINE_TYPED_LIST(T) TEN_DEFINE_TYPED_LIST_(T)
#define TEN_DEFINE_TYPED_LIST_(T)                                              \
  typedef struct ten_typed_##T##_list_t {                                      \
    ten_signature_t signature;                                                 \
    size_t size;                                                               \
    ten_typed_##T##_list_node_t *front, *back;                                 \
  } ten_typed_##T##_list_t;                                                    \
                                                                               \
  typedef struct ten_typed_##T##_list_iterator_t {                             \
    ten_typed_##T##_list_node_t *prev;                                         \
    ten_typed_##T##_list_node_t *node;                                         \
    ten_typed_##T##_list_node_t *next;                                         \
    size_t index;                                                              \
  } ten_typed_##T##_list_iterator_t;                                           \
                                                                               \
  inline bool ten_typed_##T##_list_check_integrity(                            \
      ten_typed_##T##_list_t *self) {                                          \
    TEN_ASSERT(self, "Invalid argument.");                                     \
                                                                               \
    if (ten_signature_get(&self->signature) != TEN_TYPED_LIST_SIGNATURE) {     \
      TEN_ASSERT(0, "Should not happen.");                                     \
      return false;                                                            \
    }                                                                          \
                                                                               \
    if (!self->size) {                                                         \
      if (self->front != NULL || self->back != NULL) {                         \
        TEN_ASSERT(0, "Should not happen.");                                   \
        return false;                                                          \
      }                                                                        \
    } else {                                                                   \
      if (self->front == NULL || self->back == NULL) {                         \
        TEN_ASSERT(0, "Should not happen.");                                   \
        return false;                                                          \
      }                                                                        \
                                                                               \
      if (self->size == 1) {                                                   \
        if (self->front != self->back) {                                       \
          TEN_ASSERT(0, "Should not happen.");                                 \
          return false;                                                        \
        }                                                                      \
        if ((self->front->prev != NULL) || (self->front->next != NULL)) {      \
          TEN_ASSERT(0, "Should not happen.");                                 \
          return false;                                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      if (self->front->prev != NULL) {                                         \
        TEN_ASSERT(0, "Should not happen.");                                   \
        return false;                                                          \
      }                                                                        \
      if (self->back->next != NULL) {                                          \
        TEN_ASSERT(0, "Should not happen.");                                   \
        return false;                                                          \
      }                                                                        \
    }                                                                          \
    return true;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_front(              \
      ten_typed_##T##_list_t *self) {                                          \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    return self->front;                                                        \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_back(               \
      ten_typed_##T##_list_t *self) {                                          \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    return self->back;                                                         \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_init(ten_typed_##T##_list_t *self) {        \
    TEN_ASSERT(self, "Invalid argument.");                                     \
                                                                               \
    ten_signature_set(&self->signature, TEN_TYPED_LIST_SIGNATURE);             \
    self->size = 0;                                                            \
    self->front = NULL;                                                        \
    self->back = NULL;                                                         \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_clear(ten_typed_##T##_list_t *self) {       \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    ten_typed_list_foreach(T, self, iter) {                                    \
      ten_typed_##T##_list_node_destroy(iter.node);                            \
    }                                                                          \
    self->size = 0;                                                            \
    self->front = NULL;                                                        \
    self->back = NULL;                                                         \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_deinit(ten_typed_##T##_list_t *self) {      \
    ten_typed_##T##_list_clear(self);                                          \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_t *ten_typed_##T##_list_create(void) {           \
    ten_typed_##T##_list_t *self =                                             \
        (ten_typed_##T##_list_t *)ten_malloc(sizeof(ten_typed_##T##_list_t));  \
    TEN_ASSERT(self, "Failed to allocate memory.");                            \
                                                                               \
    ten_typed_##T##_list_init(self);                                           \
    return self;                                                               \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_destroy(ten_typed_##T##_list_t *self) {     \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    ten_typed_##T##_list_deinit(self);                                         \
    ten_free(self);                                                            \
  }                                                                            \
                                                                               \
  inline size_t ten_typed_##T##_list_size(ten_typed_##T##_list_t *self) {      \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    return self->size;                                                         \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_is_empty(ten_typed_##T##_list_t *self) {    \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    return !ten_typed_##T##_list_size(self);                                   \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_swap(ten_typed_##T##_list_t *self,          \
                                        ten_typed_##T##_list_t *target) {      \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(target, "Invalid argument.");                                   \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(target),                   \
               "Invalid argument.");                                           \
                                                                               \
    ten_typed_##T##_list_node_t *self_front = self->front;                     \
    ten_typed_##T##_list_node_t *self_back = self->back;                       \
    size_t self_size = self->size;                                             \
                                                                               \
    self->front = target->front;                                               \
    self->back = target->back;                                                 \
    self->size = target->size;                                                 \
                                                                               \
    target->front = self_front;                                                \
    target->back = self_back;                                                  \
    target->size = self_size;                                                  \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_concat(ten_typed_##T##_list_t *self,        \
                                          ten_typed_##T##_list_t *target) {    \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(target, "Invalid argument.");                                   \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(target),                   \
               "Invalid argument.");                                           \
                                                                               \
    if (ten_typed_##T##_list_size(target) == 0) {                              \
      return;                                                                  \
    }                                                                          \
                                                                               \
    if (ten_typed_##T##_list_size(self) == 0) {                                \
      ten_typed_##T##_list_swap(self, target);                                 \
      return;                                                                  \
    }                                                                          \
                                                                               \
    self->back->next = target->front;                                          \
    target->front->prev = self->back;                                          \
    self->back = target->back;                                                 \
    self->size += target->size;                                                \
                                                                               \
    target->front = NULL;                                                      \
    target->back = NULL;                                                       \
    target->size = 0;                                                          \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_push_front(                                 \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_node_t *node) {       \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(node, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    if (ten_typed_##T##_list_is_empty(self)) {                                 \
      self->back = self->front = node;                                         \
      node->prev = node->next = NULL;                                          \
    } else {                                                                   \
      node->next = self->front;                                                \
      node->prev = NULL;                                                       \
                                                                               \
      self->front->prev = node;                                                \
      self->front = node;                                                      \
    }                                                                          \
    ++self->size;                                                              \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_push_list_node_back(                        \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_node_t *node) {       \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(node, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    if (ten_typed_##T##_list_is_empty(self)) {                                 \
      self->front = self->back = node;                                         \
      node->next = node->prev = NULL;                                          \
    } else {                                                                   \
      node->next = NULL;                                                       \
      node->prev = self->back;                                                 \
                                                                               \
      self->back->next = node;                                                 \
      self->back = node;                                                       \
    }                                                                          \
    ++self->size;                                                              \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_push_back(                                  \
      ten_typed_##T##_list_t *self, T item,                                    \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *)) {                                                 \
    ten_typed_##T##_list_node_t *item_node =                                   \
        ten_typed_list_node_create(T)(item, construct, move, copy, destruct);  \
    ten_typed_##T##_list_push_list_node_back(self, item_node);                 \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_push_back_in_place(                         \
      ten_typed_##T##_list_t *self, void *data,                                \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *)) {                                                 \
    ten_typed_##T##_list_node_t *item_node =                                   \
        ten_typed_list_node_create_in_place(T)(data, construct, move, copy,    \
                                               destruct);                      \
    ten_typed_##T##_list_push_list_node_back(self, item_node);                 \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_pop_front(          \
      ten_typed_##T##_list_t *self) {                                          \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    if (ten_typed_##T##_list_is_empty(self)) {                                 \
      return NULL;                                                             \
    }                                                                          \
    ten_typed_##T##_list_node_t *node = self->front;                           \
    if (ten_typed_##T##_list_size(self) == 1) {                                \
      self->back = self->front = NULL;                                         \
      node->prev = node->next = NULL;                                          \
    } else {                                                                   \
      self->front = self->front->next;                                         \
      self->front->prev = NULL;                                                \
                                                                               \
      node->next = NULL;                                                       \
    }                                                                          \
    --self->size;                                                              \
                                                                               \
    return node;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_pop_back(           \
      ten_typed_##T##_list_t *self) {                                          \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    if (ten_typed_##T##_list_is_empty(self)) {                                 \
      return NULL;                                                             \
    }                                                                          \
    ten_typed_##T##_list_node_t *node = self->back;                            \
    if (ten_typed_##T##_list_size(self) == 1) {                                \
      self->front = self->back = NULL;                                         \
      node->prev = node->next = NULL;                                          \
    } else {                                                                   \
      self->back = self->back->prev;                                           \
      self->back->next = NULL;                                                 \
                                                                               \
      node->prev = NULL;                                                       \
    }                                                                          \
    --self->size;                                                              \
                                                                               \
    return node;                                                               \
  }                                                                            \
                                                                               \
  inline void ten_typed_##T##_list_copy(ten_typed_##T##_list_t *self,          \
                                        ten_typed_##T##_list_t *target) {      \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(target, "Invalid argument.");                                   \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(target),                   \
               "Invalid argument.");                                           \
                                                                               \
    ten_typed_list_foreach(T, target, iter) {                                  \
      TEN_ASSERT(iter.node, "Invalid argument.");                              \
                                                                               \
      ten_typed_list_node_t(T) *new_node =                                     \
          ten_typed_list_node_clone(T)(iter.node);                             \
      ten_typed_list_push_list_node_back(T)(self, new_node);                   \
    }                                                                          \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_iterator_t ten_typed_##T##_list_begin(           \
      ten_typed_##T##_list_t *self) {                                          \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_check_integrity(self),                     \
               "Invalid argument.");                                           \
                                                                               \
    return (ten_typed_##T##_list_iterator_t){                                  \
        NULL,                                                                  \
        ten_typed_##T##_list_front(self),                                      \
        ten_typed_##T##_list_front(self)                                       \
            ? ten_typed_##T##_list_front(self)->next                           \
            : NULL,                                                            \
        0,                                                                     \
    };                                                                         \
  }                                                                            \
                                                                               \
  inline ten_typed_list_node_t(T) *                                            \
      ten_typed_##T##_list_find(ten_typed_##T##_list_t *self, T *item,         \
                                bool (*compare)(const T *, const T *)) {       \
    TEN_ASSERT(                                                                \
        self && ten_typed_##T##_list_check_integrity(self) && item && compare, \
        "Invalid argument.");                                                  \
    ten_typed_list_foreach(T, self, iter) {                                    \
      if (compare(ten_typed_list_node_get_data(T)(iter.node), item)) {         \
        return iter.node;                                                      \
      }                                                                        \
    }                                                                          \
    return NULL;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_iterator_t ten_typed_##T##_list_iterator_next(   \
      ten_typed_##T##_list_iterator_t self) {                                  \
    return (ten_typed_##T##_list_iterator_t){                                  \
        self.node, self.next, self.node ? (self.node)->next : NULL,            \
        self.index + 1};                                                       \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_iterator_is_end(                            \
      ten_typed_##T##_list_iterator_t self) {                                  \
    return self.node == NULL;                                                  \
  }                                                                            \
  inline ten_typed_##T##_list_node_t                                           \
      *ten_typed_##T##_list_iterator_to_list_node(                             \
          ten_typed_##T##_list_iterator_t self) {                              \
    return self.node;                                                          \
  }

#define TEN_TYPED_LIST_INIT_VAL           \
  {.signature = TEN_TYPED_LIST_SIGNATURE, \
   .size = 0,                             \
   .front = NULL,                         \
   .back = NULL}

#define TEN_DECLARE_TYPED_LIST_INLINE_ASSETS(T) \
  TEN_DECLARE_TYPED_LIST_INLINE_ASSETS_(T)
#define TEN_DECLARE_TYPED_LIST_INLINE_ASSETS_(T)                               \
  extern inline bool ten_typed_##T##_list_check_integrity(                     \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_front(       \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_back(        \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline void ten_typed_##T##_list_init(ten_typed_##T##_list_t *self);  \
                                                                               \
  extern inline void ten_typed_##T##_list_clear(ten_typed_##T##_list_t *self); \
                                                                               \
  extern inline void ten_typed_##T##_list_deinit(                              \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline ten_typed_##T##_list_t *ten_typed_##T##_list_create(void);     \
                                                                               \
  extern inline void ten_typed_##T##_list_destroy(                             \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline size_t ten_typed_##T##_list_size(                              \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline bool ten_typed_##T##_list_is_empty(                            \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline void ten_typed_##T##_list_swap(                                \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_t *target);           \
                                                                               \
  extern inline void ten_typed_##T##_list_concat(                              \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_t *target);           \
                                                                               \
  extern inline void ten_typed_##T##_list_push_front(                          \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_node_t *node);        \
                                                                               \
  extern inline void ten_typed_##T##_list_push_list_node_back(                 \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_node_t *node);        \
                                                                               \
  extern inline void ten_typed_##T##_list_push_back(                           \
      ten_typed_##T##_list_t *self, T item,                                    \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *));         /* NOLINT */                             \
                                                                               \
  extern inline void ten_typed_##T##_list_push_back_in_place(                  \
      ten_typed_##T##_list_t *self, void *data,                                \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *));         /* NOLINT */                             \
                                                                               \
  extern inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_pop_front(   \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_pop_back(    \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline void ten_typed_##T##_list_copy(                                \
      ten_typed_##T##_list_t *self, ten_typed_##T##_list_t *target);           \
                                                                               \
  extern inline ten_typed_list_node_t(T) *                                     \
      ten_typed_##T##_list_find(ten_typed_##T##_list_t *self, T *item,         \
                                bool (*compare)(const T *, const T *));        \
                                                                               \
  extern inline ten_typed_##T##_list_iterator_t ten_typed_##T##_list_begin(    \
      ten_typed_##T##_list_t *self);                                           \
                                                                               \
  extern inline ten_typed_##T##_list_iterator_t                                \
      ten_typed_##T##_list_iterator_next(                                      \
          ten_typed_##T##_list_iterator_t self);                               \
                                                                               \
  extern inline bool ten_typed_##T##_list_iterator_is_end(                     \
      ten_typed_##T##_list_iterator_t self);                                   \
                                                                               \
  extern inline ten_typed_##T##_list_node_t                                    \
      *ten_typed_##T##_list_iterator_to_list_node(                             \
          ten_typed_##T##_list_iterator_t self);

#define ten_typed_list_t(T) ten_typed_list_t_(T)
#define ten_typed_list_t_(T) ten_typed_##T##_list_t

#define ten_typed_list_size(T) ten_typed_list_size_(T)
#define ten_typed_list_size_(T) ten_typed_##T##_list_size

#define ten_typed_list_init(T) ten_typed_list_init_(T)
#define ten_typed_list_init_(T) ten_typed_##T##_list_init

#define ten_typed_list_deinit(T) ten_typed_list_deinit_(T)
#define ten_typed_list_deinit_(T) ten_typed_##T##_list_deinit

#define ten_typed_list_create(T) ten_typed_list_create_(T)
#define ten_typed_list_create_(T) ten_typed_##T##_list_create

#define ten_typed_list_destroy(T) ten_typed_list_destroy_(T)
#define ten_typed_list_destroy_(T) ten_typed_##T##_list_destroy

#define ten_typed_list_clear(T) ten_typed_list_clear_(T)
#define ten_typed_list_clear_(T) ten_typed_##T##_list_clear

#define ten_typed_list_copy(T) ten_typed_list_copy_(T)
#define ten_typed_list_copy_(T) ten_typed_##T##_list_copy

#define ten_typed_list_find(T) ten_typed_list_find_(T)
#define ten_typed_list_find_(T) ten_typed_##T##_list_find

#define ten_typed_list_front(T) ten_typed_list_front_(T)
#define ten_typed_list_front_(T) ten_typed_##T##_list_front

#define ten_typed_list_pop_front(T) ten_typed_list_pop_front_(T)
#define ten_typed_list_pop_front_(T) ten_typed_##T##_list_pop_front

#define ten_typed_list_back(T) ten_typed_list_back_(T)
#define ten_typed_list_back_(T) ten_typed_##T##_list_back

#define ten_typed_list_push_list_node_back(T) \
  ten_typed_list_push_list_node_back_(T)
#define ten_typed_list_push_list_node_back_(T) \
  ten_typed_##T##_list_push_list_node_back

#define ten_typed_list_push_back(T) ten_typed_list_push_back_(T)
#define ten_typed_list_push_back_(T) ten_typed_##T##_list_push_back

#define ten_typed_list_push_back_in_place(T) \
  ten_typed_list_push_back_in_place_(T)
#define ten_typed_list_push_back_in_place_(T) \
  ten_typed_##T##_list_push_back_in_place

#define ten_typed_list_swap(T) ten_typed_list_swap_(T)
#define ten_typed_list_swap_(T) ten_typed_##T##_list_swap

#define ten_typed_list_is_empty(T) ten_typed_list_is_empty_(T)
#define ten_typed_list_is_empty_(T) ten_typed_##T##_list_is_empty

#define ten_typed_list_iterator_t(T) ten_typed_list_iterator_t_(T)
#define ten_typed_list_iterator_t_(T) ten_typed_##T##_list_iterator_t

#define ten_typed_list_begin(T) ten_typed_list_begin_(T)
#define ten_typed_list_begin_(T) ten_typed_##T##_list_begin

#define ten_typed_list_iterator_next(T) ten_typed_list_iterator_next_(T)
#define ten_typed_list_iterator_next_(T) ten_typed_##T##_list_iterator_next
