//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"

// As the source files are compiled in `out/<os>/<cpu>`, the `__FILE__`
// will be a relative path starts with '../../../'.
#define TEN_FILE_PATH_RELATIVE_PREFIX_LENGTH 9

// #define TEN_MEMORY_CHECK_ENABLE_BACKTRACE
#define TEN_MEMORY_CHECK_BACKTRACE_BUFFER_SIZE 4096

typedef struct ten_sanitizer_memory_record_t {
  void *addr;
  size_t size;

  // Do not use `ten_string_t` here to avoid a circular dependency between
  // `ten_string_t` and `ten_malloc`.
  char *func_name;
  char *file_name;

  uint32_t lineno;

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
  char backtrace_buffer[TEN_MEMORY_CHECK_BACKTRACE_BUFFER_SIZE];
#endif

  ten_listnode_t *node_in_records_list;
  ten_hashhandle_t hh_in_records_hash;
} ten_sanitizer_memory_record_t;

typedef struct ten_sanitizer_memory_records_t {
  ten_mutex_t *lock;

  // The contents of `records_hash` and `records_list` are exactly the same;
  // `records_hash` is only used to speed up the search in `records_list`.
  ten_list_t records_list;       // ten_sanitizer_memory_record_t
  ten_hashtable_t records_hash;  // ten_sanitizer_memory_record_t

  size_t total_size;
} ten_sanitizer_memory_records_t;

TEN_UTILS_API char *ten_sanitizer_memory_strndup(const char *str, size_t size,
                                                 const char *file_name,
                                                 uint32_t lineno,
                                                 const char *func_name);
