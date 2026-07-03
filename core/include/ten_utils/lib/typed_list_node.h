//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/macro/check.h"

#define TEN_TYPED_LIST_NODE_SIGNATURE 0x3CE1EAC77F72D345U

/**
 * @brief Because T is surrounded by ## in TEN_DEFINE_TYPED_LIST_NODE_(T), and
 * that would prevent T from expanding itself, so the following
 * TEN_DEFINE_TYPED_LIST_NODE() is to ensure T is expanded first before entering
 * into TEN_DEFINE_TYPED_LIST_NODE_(T)
 *
 * @param T The type of the value stored in the list node.
 */
#define TEN_DEFINE_TYPED_LIST_NODE(T) TEN_DEFINE_TYPED_LIST_NODE_(T)
#define TEN_DEFINE_TYPED_LIST_NODE_(T)                                         \
  typedef struct ten_typed_##T##_list_node_t ten_typed_##T##_list_node_t;      \
  struct ten_typed_##T##_list_node_t {                                         \
    ten_signature_t signature;                                                 \
    ten_typed_##T##_list_node_t *next, *prev;                                  \
    T data;                                                                    \
    void (*construct)(T *, void *); /* NOLINT */                               \
    void (*move)(T *, T *);         /* NOLINT */                               \
    void (*copy)(T *, T *);         /* NOLINT */                               \
    void (*destruct)(T *);          /* NOLINT */                               \
  };                                                                           \
                                                                               \
  inline bool ten_typed_##T##_list_node_check_integrity(                       \
      ten_typed_##T##_list_node_t *self) {                                     \
    TEN_ASSERT(self, "Invalid argument.");                                     \
                                                                               \
    if (ten_signature_get(&self->signature) !=                                 \
        TEN_TYPED_LIST_NODE_SIGNATURE) {                                       \
      TEN_ASSERT(0, "Should not happen.");                                     \
      return false;                                                            \
    }                                                                          \
    return true;                                                               \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_node_init_empty(                            \
      ten_typed_##T##_list_node_t *self, void (*construct)(T *, void *),       \
      void (*move)(T *, T *),  /* NOLINT */                                    \
      void (*copy)(T *, T *),  /* NOLINT */                                    \
      void (*destruct)(T *)) { /* NOLINT */                                    \
    TEN_ASSERT(self, "Invalid argument.");                                     \
                                                                               \
    ten_signature_set(&self->signature, TEN_TYPED_LIST_NODE_SIGNATURE);        \
    self->next = NULL;                                                         \
    self->prev = NULL;                                                         \
                                                                               \
    self->construct = construct;                                               \
    self->move = move;                                                         \
    self->copy = copy;                                                         \
    self->destruct = destruct;                                                 \
                                                                               \
    return true;                                                               \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_node_init(                                  \
      ten_typed_##T##_list_node_t *self, T data,                               \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *)) {        /* NOLINT */                             \
    ten_typed_##T##_list_node_init_empty(self, construct, move, copy,          \
                                         destruct);                            \
    self->data = data;                                                         \
    return true;                                                               \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_node_init_in_place(                         \
      ten_typed_##T##_list_node_t *self, void *data,                           \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *)) {        /* NOLINT */                             \
    ten_typed_##T##_list_node_init_empty(self, construct, move, copy,          \
                                         destruct);                            \
    if (construct) {                                                           \
      construct(&self->data, data);                                            \
    }                                                                          \
    return true;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_node_create_empty(  \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *)) {        /* NOLINT */                             \
    ten_typed_##T##_list_node_t *self =                                        \
        (ten_typed_##T##_list_node_t *)ten_malloc(                             \
            sizeof(ten_typed_##T##_list_node_t));                              \
    TEN_ASSERT(self, "Failed to allocate memory.");                            \
                                                                               \
    ten_typed_##T##_list_node_init_empty(self, construct, move, copy,          \
                                         destruct);                            \
                                                                               \
    return self;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_node_create(        \
      T data, void (*construct)(T *, void *), /* NOLINT */                     \
      void (*move)(T *, T *),                 /* NOLINT */                     \
      void (*copy)(T *, T *),                 /* NOLINT */                     \
      void (*destruct)(T *)) {                /* NOLINT */                     \
    ten_typed_##T##_list_node_t *self =                                        \
        (ten_typed_##T##_list_node_t *)ten_malloc(                             \
            sizeof(ten_typed_##T##_list_node_t));                              \
    TEN_ASSERT(self, "Failed to allocate memory.");                            \
                                                                               \
    ten_typed_##T##_list_node_init(self, data, construct, move, copy,          \
                                   destruct);                                  \
    return self;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t                                           \
      *ten_typed_##T##_list_node_create_in_place(                              \
          void *data, void (*construct)(T *, void *), /* NOLINT */             \
          void (*move)(T *, T *),                     /* NOLINT */             \
          void (*copy)(T *, T *),                     /* NOLINT */             \
          void (*destruct)(T *)) {                    /* NOLINT */             \
    ten_typed_##T##_list_node_t *self =                                        \
        (ten_typed_##T##_list_node_t *)ten_malloc(                             \
            sizeof(ten_typed_##T##_list_node_t));                              \
    TEN_ASSERT(self, "Failed to allocate memory.");                            \
                                                                               \
    ten_typed_##T##_list_node_init_in_place(self, data, construct, move, copy, \
                                            destruct);                         \
    return self;                                                               \
  }                                                                            \
                                                                               \
  inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_node_clone(         \
      ten_typed_##T##_list_node_t *src) {                                      \
    TEN_ASSERT(src, "Invalid argument.");                                      \
                                                                               \
    /* NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast) */               \
    ten_typed_##T##_list_node_t *self =                                        \
        (ten_typed_##T##_list_node_t *)ten_malloc(                             \
            sizeof(ten_typed_##T##_list_node_t));                              \
    TEN_ASSERT(self, "Failed to allocate memory.");                            \
                                                                               \
    ten_typed_##T##_list_node_init_empty(self, src->construct, src->move,      \
                                         src->copy, src->destruct);            \
    src->copy(&self->data, &src->data);                                        \
    return self;                                                               \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_node_deinit(                                \
      ten_typed_##T##_list_node_t *self) {                                     \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_node_check_integrity(self),                \
               "Invalid argument.");                                           \
                                                                               \
    if (self->destruct) {                                                      \
      self->destruct(&(self->data));                                           \
    }                                                                          \
    return true;                                                               \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_node_destroy(                               \
      ten_typed_##T##_list_node_t *self) {                                     \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_node_check_integrity(self),                \
               "Invalid argument.");                                           \
                                                                               \
    if (!ten_typed_##T##_list_node_deinit(self)) {                             \
      return false;                                                            \
    }                                                                          \
    ten_free(self);                                                            \
    return true;                                                               \
  }                                                                            \
                                                                               \
  /* NOLINTNEXTLINE */                                                         \
  inline T *ten_typed_##T##_list_node_get_data(                                \
      ten_typed_##T##_list_node_t *self) {                                     \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_node_check_integrity(self),                \
               "Invalid argument.");                                           \
                                                                               \
    return &self->data;                                                        \
  }                                                                            \
                                                                               \
  inline bool ten_typed_##T##_list_node_set_data(                              \
      ten_typed_##T##_list_node_t *self, T *data, bool move) { /* NOLINT */    \
    TEN_ASSERT(self, "Invalid argument.");                                     \
    TEN_ASSERT(ten_typed_##T##_list_node_check_integrity(self),                \
               "Invalid argument.");                                           \
                                                                               \
    /* Destruct the old data if any */                                         \
    if (self->destruct) {                                                      \
      self->destruct(&self->data);                                             \
    }                                                                          \
    if (move) {                                                                \
      if (self->move) {                                                        \
        self->move(&self->data, data);                                         \
      } else {                                                                 \
        self->data = *data;                                                    \
      }                                                                        \
    } else {                                                                   \
      if (self->copy) {                                                        \
        self->copy(&self->data, data);                                         \
      } else {                                                                 \
        self->data = *data;                                                    \
      }                                                                        \
    }                                                                          \
    return true;                                                               \
  }

#define TEN_DECLARE_TYPED_LIST_NODE_INLINE_ASSETS(T) \
  TEN_DECLARE_TYPED_LIST_NODE_INLINE_ASSETS_(T)
#define TEN_DECLARE_TYPED_LIST_NODE_INLINE_ASSETS_(T)                          \
  extern inline bool ten_typed_##T##_list_node_check_integrity(                \
      ten_typed_##T##_list_node_t *self);                                      \
                                                                               \
  extern inline bool ten_typed_##T##_list_node_init_empty(                     \
      ten_typed_##T##_list_node_t *self,                                       \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *));                                                  \
                                                                               \
  extern inline bool ten_typed_##T##_list_node_init(                           \
      ten_typed_##T##_list_node_t *self, T data,                               \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *));                                                  \
                                                                               \
  extern inline bool ten_typed_##T##_list_node_init_in_place(                  \
      ten_typed_##T##_list_node_t *self, void *data,                           \
      void (*construct)(T *, void *), /* NOLINT */                             \
      void (*move)(T *, T *),         /* NOLINT */                             \
      void (*copy)(T *, T *),         /* NOLINT */                             \
      void (*destruct)(T *));                                                  \
                                                                               \
  extern inline ten_typed_##T##_list_node_t                                    \
      *ten_typed_##T##_list_node_create_empty(                                 \
          void (*construct)(T *, void *), /* NOLINT */                         \
          void (*move)(T *, T *),         /* NOLINT */                         \
          void (*copy)(T *, T *),         /* NOLINT */                         \
          void (*destruct)(T *));                                              \
                                                                               \
  extern inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_node_create( \
      T data, void (*construct)(T *, void *), /* NOLINT */                     \
      void (*move)(T *, T *),                 /* NOLINT */                     \
      void (*copy)(T *, T *),                 /* NOLINT */                     \
      void (*destruct)(T *));                                                  \
                                                                               \
  extern inline ten_typed_##T##_list_node_t                                    \
      *ten_typed_##T##_list_node_create_in_place(                              \
          void *data, void (*construct)(T *, void *), /* NOLINT */             \
          void (*move)(T *, T *),                     /* NOLINT */             \
          void (*copy)(T *, T *),                     /* NOLINT */             \
          void (*destruct)(T *));                                              \
                                                                               \
  extern inline ten_typed_##T##_list_node_t *ten_typed_##T##_list_node_clone(  \
      ten_typed_##T##_list_node_t *src);                                       \
                                                                               \
  extern inline bool ten_typed_##T##_list_node_deinit(                         \
      ten_typed_##T##_list_node_t *self);                                      \
                                                                               \
  extern inline bool ten_typed_##T##_list_node_destroy(                        \
      ten_typed_##T##_list_node_t *self);                                      \
                                                                               \
  /* NOLINTNEXTLINE */                                                         \
  extern inline T *ten_typed_##T##_list_node_get_data(                         \
      ten_typed_##T##_list_node_t *self);                                      \
                                                                               \
  extern inline bool ten_typed_##T##_list_node_set_data(                       \
      ten_typed_##T##_list_node_t *self, T *data, bool move);

#define ten_typed_list_node_t(T) ten_typed_list_node_t_(T)
#define ten_typed_list_node_t_(T) ten_typed_##T##_list_node_t

#define ten_typed_list_node_init(T) ten_typed_list_node_init_(T)
#define ten_typed_list_node_init_(T) ten_typed_##T##_list_node_init

#define ten_typed_list_node_init_in_place(T) \
  ten_typed_list_node_init_in_place_(T)
#define ten_typed_list_node_init_in_place_(T) \
  ten_typed_##T##_list_node_init_in_place

#define ten_typed_list_node_create_empty(T) ten_typed_list_node_create_empty_(T)
#define ten_typed_list_node_create_empty_(T) \
  ten_typed_##T##_list_node_create_empty

#define ten_typed_list_node_create(T) ten_typed_list_node_create_(T)
#define ten_typed_list_node_create_(T) ten_typed_##T##_list_node_create

#define ten_typed_list_node_create_in_place(T) \
  ten_typed_list_node_create_in_place_(T)
#define ten_typed_list_node_create_in_place_(T) \
  ten_typed_##T##_list_node_create_in_place

#define ten_typed_list_node_clone(T) ten_typed_list_node_clone_(T)
#define ten_typed_list_node_clone_(T) ten_typed_##T##_list_node_clone

#define ten_typed_list_node_deinit(T) ten_typed_list_node_deinit_(T)
#define ten_typed_list_node_deinit_(T) ten_typed_##T##_list_node_deinit

#define ten_typed_list_node_destroy(T) ten_typed_list_node_destroy_(T)
#define ten_typed_list_node_destroy_(T) ten_typed_##T##_list_node_destroy

#define ten_typed_list_node_get_data(T) ten_typed_list_node_get_data_(T)
#define ten_typed_list_node_get_data_(T) ten_typed_##T##_list_node_get_data

#define ten_typed_list_node_set_data(T) ten_typed_list_node_set_data_(T)
#define ten_typed_list_node_set_data_(T) ten_typed_##T##_list_node_set_data
