//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/container/hash_handle.h"

#include "ten_utils/container/hash_bucket.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/macro/check.h"

// Note: The hash table will be used in the TEN memory tracking mechanism, so do
// _not_ use the TEN_MALLOC series of APIs in the hash table-related code;
// otherwise, there will be a circular dependency issue.

void ten_hashhandle_init(ten_hashhandle_t *self, ten_hashtable_t *table,
                         const void *key, uint32_t keylen, void *destroy) {
  TEN_ASSERT(self && table && key, "Invalid argument.");

  self->tbl = table;
  self->key = key;
  self->keylen = keylen;
  self->hashval = ten_hash_function(key, keylen);
  self->destroy = destroy;
}

void ten_hashhandle_del_from_app_list(ten_hashhandle_t *hh) {
  TEN_ASSERT(hh, "Invalid argument.");

  if (hh == hh->tbl->head) {
    if (hh->next) {
      hh->tbl->head = FIELD_OF_FROM_OFFSET(hh->next, hh->tbl->hh_offset);
    } else {
      hh->tbl->head = NULL;
    }
  }
  if (hh == hh->tbl->tail) {
    if (hh->prev) {
      hh->tbl->tail = FIELD_OF_FROM_OFFSET(hh->prev, hh->tbl->hh_offset);
    } else {
      hh->tbl->tail = NULL;
    }
  }
  if (hh->prev) {
    ((ten_hashhandle_t *)FIELD_OF_FROM_OFFSET(hh->prev, hh->tbl->hh_offset))
        ->next = hh->next;
  }
  if (hh->next) {
    ((ten_hashhandle_t *)FIELD_OF_FROM_OFFSET(hh->next, hh->tbl->hh_offset))
        ->prev = hh->prev;
  }
}
