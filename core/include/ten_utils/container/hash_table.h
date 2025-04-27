//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* Modified from https://github.com/troydhanson/uthash. */
#pragma once

#include "ten_utils/ten_config.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"

#define ten_hashtable_foreach(table, iter)                                     \
  for (ten_hashtable_iterator_t iter =                                         \
           {NULL, ten_hashtable_front(table),                                  \
            ten_hashtable_front(table)                                         \
                ? ten_hashtable_front(table)->next                             \
                      ? FIELD_OF_FROM_OFFSET(ten_hashtable_front(table)->next, \
                                             (table)->hh_offset)               \
                      : NULL                                                   \
                : NULL,                                                        \
            0};                                                                \
       (iter).node;                                                            \
       ++((iter).index), (iter).prev = (iter).node, (iter).node = (iter).next, \
                                (iter).next =                                  \
                                    (iter).node                                \
                                        ? ((iter).node)->next                  \
                                              ? FIELD_OF_FROM_OFFSET(          \
                                                    ((iter).node)->next,       \
                                                    (table)->hh_offset)        \
                                              : NULL                           \
                                        : NULL)

typedef struct ten_hashtable_t ten_hashtable_t;
typedef struct ten_hashbucket_t ten_hashbucket_t;
typedef struct ten_hashhandle_t ten_hashhandle_t;

struct ten_hashtable_t {
  ten_hashbucket_t *bkts;

  ten_hashhandle_t *head;  // App-ordered list head.
  ten_hashhandle_t *tail;  // App-ordered list tail.
  ptrdiff_t hh_offset;

  uint32_t bkts_cnt;
  uint32_t bkts_cnt_in_log2;
  uint32_t items_cnt;

  // In a ideal situation (all buckets used equally), no bucket would have
  // more than ceil (#items/#buckets) items. that's the ideal chain length.
  uint32_t ideal_chain_maxlen;

  // non_ideal_items_cnt is the number of items in the hash whose chain position
  // exceeds the ideal chain maxlen. These items pay the penalty for a uneven
  // hash distribution; reaching them in a chain traversal takes >ideal steps.
  uint32_t non_ideal_items_cnt;

  // ineffective expands occur when a bucket doubling was performed, but
  // afterward, more than half the items in the hash had non_ideal chain
  // positions. If this happens on two consecutive expansions we inhibit any
  // further expansion, as it's not helping; this happens when the hash
  // function isn't a good fit for the key domain. When expansion is inhibited
  // the hash will still work, albeit no longer in constant time. */
  uint32_t ineff_expands_times;
  bool noexpand;
};

typedef struct ten_hashtable_iterator_t {
  ten_hashhandle_t *prev;
  ten_hashhandle_t *node;
  ten_hashhandle_t *next;
  size_t index;
} ten_hashtable_iterator_t;

TEN_UTILS_API uint32_t ten_hash_function(const void *key, uint32_t keylen);

/**
 * @brief Create a hash table.
 */
TEN_UTILS_API ten_hashtable_t *ten_hashtable_create(ptrdiff_t hh_offset);

/**
 * @brief Destroy a hash table.
 */
TEN_UTILS_API void ten_hashtable_destroy(ten_hashtable_t *self);

TEN_UTILS_API uint32_t ten_hashtable_items_cnt(ten_hashtable_t *self);

TEN_UTILS_API void ten_hashtable_init(ten_hashtable_t *self,
                                      ptrdiff_t hh_offset);

TEN_UTILS_API void ten_hashtable_deinit(ten_hashtable_t *self);

TEN_UTILS_API void ten_hashtable_clear(ten_hashtable_t *self);

/**
 * @brief Concatenate the second hash table into the first one.
 * @param self The hash table to be concatenated.
 * @param target The hash table to be concatenating to the first one.
 */
TEN_UTILS_API void ten_hashtable_concat(ten_hashtable_t *self,
                                        ten_hashtable_t *target);

TEN_UTILS_API void ten_hashtable_expand_bkts(ten_hashtable_t *self);

TEN_UTILS_API void ten_hashtable_add_by_key(ten_hashtable_t *self,
                                            ten_hashhandle_t *hh,
                                            const void *key, uint32_t keylen,
                                            void *destroy);

TEN_UTILS_API void ten_hashtable_replace_by_key(ten_hashtable_t *self,
                                                ten_hashhandle_t *hh,
                                                void *keyptr,
                                                uint32_t keylen_in,
                                                void *destroy);

TEN_UTILS_API void ten_hashtable_del(ten_hashtable_t *self,
                                     ten_hashhandle_t *hh);

TEN_UTILS_API ten_hashhandle_t *ten_hashtable_front(ten_hashtable_t *self);

TEN_UTILS_API ten_hashhandle_t *ten_hashtable_back(ten_hashtable_t *self);

TEN_UTILS_API ten_hashhandle_t *ten_hashtable_find_by_key(ten_hashtable_t *self,
                                                          const void *key,
                                                          uint32_t keylen);

TEN_UTILS_API ten_hashhandle_t *ten_hashtable_find(ten_hashtable_t *self,
                                                   uint32_t hashval,
                                                   const void *key,
                                                   uint32_t keylen);

/**
 * @brief Add an item to the hash table with string-type key.
 */
static inline void ten_hashtable_add_string(ten_hashtable_t *self,
                                            ten_hashhandle_t *hh,
                                            const char *str, void *destroy) {
  TEN_ASSERT(self && hh && str, "Invalid arguments.");
  ten_hashtable_add_by_key(self, hh, str, strlen(str), destroy);
}

static inline void ten_hashtable_replace_string(ten_hashtable_t *self,
                                                ten_hashhandle_t *hh, char *str,
                                                void *destroy) {
  TEN_ASSERT(self && hh && str, "Invalid arguments.");
  ten_hashtable_replace_by_key(self, hh, str, strlen(str), destroy);
}

static inline ten_hashhandle_t *ten_hashtable_find_string(ten_hashtable_t *self,
                                                          const char *str) {
  TEN_ASSERT(self && str, "Invalid arguments.");
  return ten_hashtable_find_by_key(self, str, strlen(str));
}

/**
 * @brief Add an item to the hash table with integer-type key.
 */
static inline void ten_hashtable_add_int(ten_hashtable_t *self,
                                         ten_hashhandle_t *hh, int32_t *value,
                                         void *destroy) {
  TEN_ASSERT(self && hh, "Invalid arguments.");
  ten_hashtable_add_by_key(self, hh, value, sizeof(int32_t), destroy);
}

static inline void ten_hashtable_replace_int(ten_hashtable_t *self,
                                             ten_hashhandle_t *hh,
                                             int32_t *value, void *destroy) {
  TEN_ASSERT(self && hh, "Invalid arguments.");
  ten_hashtable_replace_by_key(self, hh, value, sizeof(int32_t), destroy);
}

static inline ten_hashhandle_t *ten_hashtable_find_int(ten_hashtable_t *self,
                                                       int32_t *value) {
  TEN_ASSERT(self, "Invalid arguments.");
  return ten_hashtable_find_by_key(self, value, sizeof(int32_t));
}

/**
 * @brief Add an item to the hash table with pointer-type key.
 */
static inline void ten_hashtable_add_ptr(ten_hashtable_t *self,
                                         ten_hashhandle_t *hh, void *ptr,
                                         void *destroy) {
  TEN_ASSERT(self && hh, "Invalid arguments.");
  ten_hashtable_add_by_key(self, hh, ptr, sizeof(void *), destroy);
}

static inline void ten_hashtable_replace_ptr(ten_hashtable_t *self,
                                             ten_hashhandle_t *hh, void *ptr,
                                             void *destroy) {
  TEN_ASSERT(self && hh, "Invalid arguments.");
  ten_hashtable_replace_by_key(self, hh, ptr, sizeof(void *), destroy);
}

static inline ten_hashhandle_t *ten_hashtable_find_ptr(ten_hashtable_t *self,
                                                       void *ptr) {
  TEN_ASSERT(self, "Invalid arguments.");
  return ten_hashtable_find_by_key(self, ptr, sizeof(void *));
}
