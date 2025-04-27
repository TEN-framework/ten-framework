//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/container/hash_table.h"

#include "ten_utils/container/hash_bucket.h"
#include "ten_utils/container/hash_handle.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

// Note: The hash table will be used in the TEN memory tracking mechanism, so do
// _not_ use the TEN_MALLOC series of APIs in the hash table-related code;
// otherwise, there will be a circular dependency issue.

#define HASH_TBL_INIT_BKT_CNT 32U      // Initial number of buckets
#define HASH_TBL_INIT_BKT_CNT_LOG2 5U  // lg2 of initial number of buckets

// The hash function is from Jenkins.
#define HASH_MIX(a, b, c) \
  do {                    \
    (a) -= (b);           \
    (a) -= (c);           \
    (a) ^= ((c) >> 13U);  \
    (b) -= (c);           \
    (b) -= (a);           \
    (b) ^= ((a) << 8U);   \
    (c) -= (a);           \
    (c) -= (b);           \
    (c) ^= ((b) >> 13U);  \
    (a) -= (b);           \
    (a) -= (c);           \
    (a) ^= ((c) >> 12U);  \
    (b) -= (c);           \
    (b) -= (a);           \
    (b) ^= ((a) << 16U);  \
    (c) -= (a);           \
    (c) -= (b);           \
    (c) ^= ((b) >> 5U);   \
    (a) -= (b);           \
    (a) -= (c);           \
    (a) ^= ((c) >> 3U);   \
    (b) -= (c);           \
    (b) -= (a);           \
    (b) ^= ((a) << 10U);  \
    (c) -= (a);           \
    (c) -= (b);           \
    (c) ^= ((b) >> 15U);  \
  } while (0)

PURE uint32_t ten_hash_function(const void *key, const uint32_t keylen) {
  TEN_ASSERT(key, "Invalid argument.");

  const uint8_t *key_ = (const uint8_t *)key;

  uint32_t hashval = 0xfeedbeefU;
  uint32_t i = 0x9e3779b9U;
  uint32_t j = i;
  uint32_t k = keylen;

  while (k >= 12U) {
    i += (key_[0] + ((uint32_t)key_[1] << 8U) + ((uint32_t)key_[2] << 16U) +
          ((uint32_t)key_[3] << 24U));

    j += (key_[4] + ((uint32_t)key_[5] << 8U) + ((uint32_t)key_[6] << 16U) +
          ((uint32_t)key_[7] << 24U));

    hashval += (key_[8] + ((uint32_t)key_[9] << 8U) +
                ((uint32_t)key_[10] << 16U) + ((uint32_t)key_[11] << 24U));

    HASH_MIX(i, j, hashval);

    key_ += 12;
    k -= 12U;
  }

  hashval += keylen;

  switch (k) {
  case 11:
    hashval += ((uint32_t)key_[10] << 24U);
  case 10:
    hashval += ((uint32_t)key_[9] << 16U);
  case 9:
    hashval += ((uint32_t)key_[8] << 8U);
  case 8:
    j += ((uint32_t)key_[7] << 24U);
  case 7:
    j += ((uint32_t)key_[6] << 16U);
  case 6:
    j += ((uint32_t)key_[5] << 8U);
  case 5:
    j += key_[4];
  case 4:
    i += ((uint32_t)key_[3] << 24U);
  case 3:
    i += ((uint32_t)key_[2] << 16U);
  case 2:
    i += ((uint32_t)key_[1] << 8U);
  case 1:
    i += key_[0];
  default:;
  }

  HASH_MIX(i, j, hashval);

  return hashval;
}

static uint32_t ten_hash_get_bucket_idx(const uint32_t hashval,
                                        const uint32_t bucket_cnt) {
  return (hashval & (bucket_cnt - 1U));
}

// Hash table.

static bool ten_hashtable_check_integrity(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  uint32_t tbl_items_cnt = 0;
  for (uint32_t i = 0; i < self->bkts_cnt; ++i) {
    uint32_t bkt_items_cnt = 0;
    ten_hashhandle_t *hh = self->bkts[i].head;
    ten_hashhandle_t *prev = NULL;
    while (hh) {
      if (prev != hh->hh_prev) {
        // TEN_LOGE("Invalid prev %p, actual %p", hh->hh_prev, prev);
        return false;
      }
      bkt_items_cnt++;
      prev = hh;
      hh = hh->hh_next;
    }
    tbl_items_cnt += bkt_items_cnt;
    if (self->bkts[i].items_cnt != bkt_items_cnt) {
      return false;
    }
  }
  if (tbl_items_cnt != self->items_cnt) {
    return false;
  }

  tbl_items_cnt = 0;
  ten_hashhandle_t *hh = self->head;
  void *prev = NULL;
  while (hh) {
    tbl_items_cnt++;
    if (prev != hh->prev) {
      // TEN_LOGE("Invalid prev %p, actual %p", hh->prev, prev);
      return false;
    }
    prev = CONTAINER_OF_FROM_OFFSET(hh, self->hh_offset);
    hh = hh->next ? FIELD_OF_FROM_OFFSET(hh->next, self->hh_offset) : NULL;
  }
  if (tbl_items_cnt != self->items_cnt) {
    return false;
  }

  return true;
}

ten_hashtable_t *ten_hashtable_create(ptrdiff_t hh_offset) {
  ten_hashtable_t *self = (ten_hashtable_t *)calloc(1, sizeof(ten_hashtable_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_hashtable_init(self, hh_offset);

  return self;
}

void ten_hashtable_destroy(ten_hashtable_t *self) {
  TEN_ASSERT(self && self->items_cnt == 0, "Invalid argument.");
  free(self->bkts);
  free(self);
}

void ten_hashtable_init(ten_hashtable_t *self, ptrdiff_t hh_offset) {
  TEN_ASSERT(self, "Invalid argument.");

  self->bkts_cnt = HASH_TBL_INIT_BKT_CNT;
  self->bkts_cnt_in_log2 = HASH_TBL_INIT_BKT_CNT_LOG2;
  self->bkts = (ten_hashbucket_t *)calloc(
      1, HASH_TBL_INIT_BKT_CNT * sizeof(ten_hashbucket_t));
  TEN_ASSERT(self->bkts, "Failed to allocate memory.");
  self->items_cnt = 0;

  self->head = NULL;
  self->tail = NULL;
  self->hh_offset = hh_offset;
}

void ten_hashtable_deinit(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_hashtable_clear(self);

  if (self->bkts) {
    free(self->bkts);
    self->bkts = NULL;
  }
  self->bkts_cnt = 0;
  self->bkts_cnt_in_log2 = 0;
  self->head = NULL;
  self->tail = NULL;
}

void ten_hashtable_clear(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_hashtable_foreach(self, iter) { ten_hashtable_del(self, iter.node); }
}

void ten_hashtable_concat(ten_hashtable_t *self, ten_hashtable_t *target) {
  TEN_ASSERT(self && target, "Invalid argument.");

  ten_hashtable_foreach(target, iter) {
    ten_hashhandle_t *hh = iter.node;
    ten_hashtable_add_by_key(self, hh, hh->key, hh->keylen, hh->destroy);
  }

  if (target->bkts) {
    free(target->bkts);
    target->bkts = NULL;
  }

  target->head = NULL;
  target->tail = NULL;
  target->hh_offset = 0;
  target->bkts_cnt = 0;
  target->bkts_cnt_in_log2 = 0;
  target->items_cnt = 0;
  target->ideal_chain_maxlen = 0;
  target->non_ideal_items_cnt = 0;
  target->ineff_expands_times = 0;
  target->noexpand = false;
}

void ten_hashtable_expand_bkts(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  // Bucket expansion has the effect of doubling the number of buckets and
  // redistributing the items into the new buckets.
  const size_t new_bkt_cnt = sizeof(ten_hashbucket_t) * self->bkts_cnt * 2U;
  ten_hashbucket_t *new_bkts = (ten_hashbucket_t *)calloc(1, new_bkt_cnt);
  TEN_ASSERT(new_bkts, "Failed to allocate memory.");

  // The calculation of tbl->ideal_chain_maxlen below deserves some
  // explanation. First, keep in mind that we're calculating the ideal maximum
  // chain length based on the *new* (doubled) bucket count.
  // In fractions this is just n/b (n=number of items, b=new num buckets).
  // Since the ideal chain length is a integer, we want to calculate
  //
  // ceil(n / b)
  //
  // We don't depend on floating point arithmetic in this hash, so to calculate
  // ceil(n/b) with integers we could write
  //
  // ceil(n / b) = (n / b) + ((n % b) ? 1 : 0)
  //
  // But b is always a power of two. We keep its base 2 log handy (call it lb),
  // so now we can write this with a bit shift and logical AND :
  //
  // ceil(n / b) = (n >> lb) + ((n & (b - 1)) ? 1 : 0)
  self->ideal_chain_maxlen =
      (self->items_cnt >> (self->bkts_cnt_in_log2 + 1U)) +
      (((self->items_cnt & ((self->bkts_cnt * 2U) - 1U)) != 0U) ? 1U : 0U);
  self->non_ideal_items_cnt = 0;

  // Spread all elements in the hash table to new buckets.
  for (uint32_t i = 0; i < self->bkts_cnt; i++) {
    ten_hashhandle_t *curr_hh = self->bkts[i].head;

    // Spread all elements in a old bucket to new buckets
    while (curr_hh != NULL) {
      ten_hashhandle_t *next_hh = curr_hh->hh_next;

      uint32_t new_bkt_idx =
          ten_hash_get_bucket_idx(curr_hh->hashval, self->bkts_cnt * 2U);
      ten_hashbucket_t *new_bkt = &(new_bkts[new_bkt_idx]);

      if (++(new_bkt->items_cnt) > self->ideal_chain_maxlen) {
        self->non_ideal_items_cnt++;
        if (new_bkt->items_cnt >
            new_bkt->expand_mult * self->ideal_chain_maxlen) {
          new_bkt->expand_mult++;
        }
      }

      // Insert 'curr_hh' to the front of the new bucket.
      curr_hh->hh_prev = NULL;
      curr_hh->hh_next = new_bkt->head;
      if (new_bkt->head != NULL) {
        new_bkt->head->hh_prev = curr_hh;
      }
      new_bkt->head = curr_hh;
      curr_hh = next_hh;
    }
  }

  free(self->bkts);
  self->bkts_cnt *= 2U;
  self->bkts_cnt_in_log2++;
  self->bkts = new_bkts;
  self->ineff_expands_times =
      (self->non_ideal_items_cnt > (self->items_cnt >> 1U))
          ? (self->ineff_expands_times + 1U)
          : 0U;
  if (self->ineff_expands_times > 1U) {
    // TEN_LOGW("Probably Inefficient hash function.");
    self->noexpand = true;
  }
}

static void ten_hashtable_add_to_app_list(ten_hashtable_t *self,
                                          ten_hashhandle_t *hh) {
  TEN_ASSERT(self && hh, "Invalid argument.");

  hh->next = NULL;
  if (self->tail) {
    hh->prev = CONTAINER_OF_FROM_OFFSET(self->tail, self->hh_offset);
    self->tail->next = CONTAINER_OF_FROM_OFFSET(hh, self->hh_offset);
  } else {
    TEN_ASSERT(self->head == NULL, "Invalid argument.");
    self->head = hh;
    hh->prev = NULL;
  }
  self->tail = hh;
}

static void ten_hashtable_add_by_hash_val(ten_hashtable_t *self,
                                          ten_hashhandle_t *hh,
                                          uint32_t hashval) {
  TEN_ASSERT(self && hh, "Invalid argument.");

  self->items_cnt++;
  const uint32_t bkt_idx = ten_hash_get_bucket_idx(hashval, self->bkts_cnt);
  ten_hashbucket_add(&(self->bkts[bkt_idx]), hh);

  // Insert to the end of the app-ordered list.
  ten_hashtable_add_to_app_list(self, hh);
}

void ten_hashtable_add_by_key(ten_hashtable_t *self, ten_hashhandle_t *hh,
                              const void *key, uint32_t keylen, void *destroy) {
  TEN_ASSERT(self && hh && key, "Invalid argument.");

  ten_hashhandle_init(hh, self, key, keylen, destroy);
  ten_hashtable_add_by_hash_val(self, hh, hh->hashval);
  TEN_ASSERT(ten_hashtable_check_integrity(self), "Invalid integrity.");
}

static void ten_hashtable_replace_by_hash_val(ten_hashtable_t *self,
                                              ten_hashhandle_t *hh,
                                              uint32_t hashval, void *key,
                                              uint32_t keylen) {
  TEN_ASSERT(self && hh && key, "Invalid argument.");

  ten_hashhandle_t *replaced = ten_hashtable_find(self, hashval, key, keylen);
  if (replaced) {
    ten_hashtable_del(self, replaced);
  }
  ten_hashtable_add_by_hash_val(self, hh, hashval);
}

void ten_hashtable_replace_by_key(ten_hashtable_t *self, ten_hashhandle_t *hh,
                                  void *key, uint32_t keylen, void *destroy) {
  TEN_ASSERT(self && hh && key, "Invalid argument.");

  ten_hashhandle_init(hh, self, key, keylen, destroy);
  ten_hashtable_replace_by_hash_val(self, hh, hh->hashval, key, keylen);
  TEN_ASSERT(ten_hashtable_check_integrity(self), "Invalid integrity.");
}

void ten_hashtable_del(ten_hashtable_t *self, ten_hashhandle_t *hh) {
  TEN_ASSERT(self && hh && self == hh->tbl, "Invalid argument.");

  const uint32_t bkt_idx = ten_hash_get_bucket_idx(hh->hashval, self->bkts_cnt);
  ten_hashbucket_del(&(self->bkts[bkt_idx]), hh);

  ten_hashhandle_del_from_app_list(hh);  // Remove from the app-ordered list.

  if (hh->destroy) {
    hh->destroy(CONTAINER_OF_FROM_OFFSET(hh, self->hh_offset));
  }

  self->items_cnt--;
  TEN_ASSERT(ten_hashtable_check_integrity(self), "Invalid integrity.");
}

ten_hashhandle_t *ten_hashtable_front(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  return self->head;
}

ten_hashhandle_t *ten_hashtable_back(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  return self->tail;
}

uint32_t ten_hashtable_items_cnt(ten_hashtable_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  return self->items_cnt;
}

ten_hashhandle_t *ten_hashtable_find_by_key(ten_hashtable_t *self,
                                            const void *key, uint32_t keylen) {
  TEN_ASSERT(self && key, "Invalid argument.");
  uint32_t hashval = ten_hash_function(key, keylen);
  return ten_hashtable_find(self, hashval, key, keylen);
}

ten_hashhandle_t *ten_hashtable_find(ten_hashtable_t *self, uint32_t hashval,
                                     const void *key, uint32_t keylen) {
  TEN_ASSERT(self && key, "Invalid argument.");
  const uint32_t bkt_idx = ten_hash_get_bucket_idx(hashval, self->bkts_cnt);
  return ten_hashbucket_find(&(self->bkts[bkt_idx]), hashval, key, keylen);
}
