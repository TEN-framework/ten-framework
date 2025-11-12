//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* Modified from https://github.com/troydhanson/uthash. */
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

typedef struct ten_hashtable_t ten_hashtable_t;
typedef struct ten_hashhandle_t ten_hashhandle_t;

struct ten_hashhandle_t {
  ten_hashtable_t *tbl;

  void *prev;  // previous hash handle in app-ordered list
  void *next;  // next hash handle in app-ordered list

  ten_hashhandle_t *hh_prev;  // previous item in bucket
  ten_hashhandle_t *hh_next;  // next item in bucket

  const void *key;   // ptr to key data
  uint32_t keylen;   // key len
  uint32_t hashval;  // result of hash function

  void (*destroy)(ten_hashhandle_t *);
};

TEN_UTILS_API void ten_hashhandle_init(ten_hashhandle_t *self,
                                       ten_hashtable_t *table, const void *key,
                                       uint32_t keylen, void *destroy);

TEN_UTILS_API void ten_hashhandle_del_from_app_list(ten_hashhandle_t *hh);
