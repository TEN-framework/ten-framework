//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/sanitizer/memory_check.h"

// This header file must be included before asan/lsan headers.
#include "ten_utils/macro/check.h"

#if defined(TEN_USE_ASAN)
#include <sanitizer/asan_interface.h>
#include <sanitizer/lsan_interface.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/buffer.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/internal.h"
#include "include_internal/ten_utils/lib/alloc.h"
#include "include_internal/ten_utils/sanitizer/memory_check.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/macro/mark.h"

// Note: Since TEN LOG also involves memory operations, to avoid circular
// dependencies in the memory checker system, use the basic printf family
// functions instead of TEN LOG.

// Note: The `ten_sanitizer_memory_record_t` items stored in
// `ten_sanitizer_memory_records_t` not only contain the actual allocated memory
// region addresses but also include other auxiliary allocations used to record
// these memory regions in `ten_sanitizer_memory_records_t`. However, these
// auxiliary allocations are only for recording the actual allocated memory
// region addresses and do not need to be tracked by LeakSanitizer. Otherwise,
// in addition to the actual memory leaks, each actual memory leak would have
// corresponding internal auxiliary allocations used for recording, which would
// essentially be false alarms. Therefore, `__lsan_disable()` and
// `__lsan_enable()` are used to exclude these auxiliary allocations from being
// tracked by LeakSanitizer.

static ten_sanitizer_memory_records_t g_memory_records = {
    NULL, TEN_LIST_INIT_VAL, {0}, 0};

static bool g_memory_records_enabled = false;

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
static ten_backtrace_t *g_backtrace_for_memory_check = NULL;
#endif

static void ten_sanitizer_memory_record_check_enabled(void) {
#if defined(TEN_ENABLE_MEMORY_CHECK)
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  char *enable_memory_sanitizer = getenv("TEN_ENABLE_MEMORY_TRACKING");
  if (enable_memory_sanitizer && !strcmp(enable_memory_sanitizer, "true")) {
    g_memory_records_enabled = true;
  }
#endif
}

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
static void ten_sanitizer_memory_record_init_backtrace(void) {
  TEN_ASSERT(g_backtrace_for_memory_check == NULL,
             "The backtrace for memory check should be initialized only once.");

  g_backtrace_for_memory_check = ten_backtrace_create();
  TEN_ASSERT(g_backtrace_for_memory_check, "Failed to create backtrace.");
}

static void ten_sanitizer_memory_record_deinit_backtrace(void) {
  if (g_backtrace_for_memory_check) {
    ten_backtrace_destroy(g_backtrace_for_memory_check);
    g_backtrace_for_memory_check = NULL;
  }
}
#endif

void ten_sanitizer_memory_record_init(void) {
#if defined(TEN_ENABLE_MEMORY_CHECK)

  ten_sanitizer_memory_record_check_enabled();

  if (!g_memory_records_enabled) {
    return;
  }

#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
  ten_sanitizer_memory_record_init_backtrace();
#endif

#if defined(TEN_USE_ASAN)
  // Mark the beginning and end of the globally allocated memory queue as
  // poisoned, so that LSan will not consider the memory buffer obtained from
  // there as normal memory, but will instead consider it as leaked.
  __asan_poison_memory_region(&g_memory_records.records_list.front,
                              sizeof(ten_listnode_t *));
  __asan_poison_memory_region(&g_memory_records.records_list.back,
                              sizeof(ten_listnode_t *));
#endif

  g_memory_records.lock = ten_mutex_create();

  ten_hashtable_init(
      &g_memory_records.records_hash,
      offsetof(ten_sanitizer_memory_record_t, hh_in_records_hash));

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif

#endif
}

void ten_sanitizer_memory_record_deinit(void) {
#if defined(TEN_ENABLE_MEMORY_CHECK)

  if (!g_memory_records_enabled) {
    return;
  }

#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

  ten_sanitizer_memory_record_dump();

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
  ten_sanitizer_memory_record_deinit_backtrace();
#endif

  if (g_memory_records.lock) {
    ten_mutex_destroy(g_memory_records.lock);
  }

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif

#endif
}

static ten_sanitizer_memory_record_t *ten_sanitizer_memory_record_create(
    void *addr, size_t size, const char *file_name, uint32_t lineno,
    const char *func_name) {
#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

  ten_sanitizer_memory_record_t *self =
      malloc(sizeof(ten_sanitizer_memory_record_t));
  TEN_ASSERT(self, "Failed to allocate memory.");
  if (!self) {
#if defined(TEN_USE_ASAN)
    __lsan_enable();
#endif
    return NULL;
  }

  self->addr = addr;
  self->size = size;

  self->func_name = (char *)malloc(strlen(func_name) + 1);
  TEN_ASSERT(self->func_name, "Failed to allocate memory.");

  int written =
      snprintf(self->func_name, strlen(func_name) + 1, "%s", func_name);
  TEN_ASSERT(written > 0, "Should not happen.");

  self->file_name = (char *)malloc(strlen(file_name) + 1);
  TEN_ASSERT(self->file_name, "Failed to allocate memory.");
  TEN_ASSERT(strlen(file_name) >= TEN_FILE_PATH_RELATIVE_PREFIX_LENGTH,
             "Should not happen.");

  written =
      snprintf(self->file_name, strlen(file_name) + 1, "%.*s",
               (int)(strlen(file_name) - TEN_FILE_PATH_RELATIVE_PREFIX_LENGTH),
               file_name + TEN_FILE_PATH_RELATIVE_PREFIX_LENGTH);
  TEN_ASSERT(written > 0, "Should not happen.");

  self->lineno = lineno;

  self->node_in_records_list = NULL;

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif

  return self;
}

static void ten_sanitizer_memory_record_destroy(
    ten_sanitizer_memory_record_t *self) {
#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

  TEN_ASSERT(self, "Invalid argument.");

  free(self->func_name);
  free(self->file_name);
  free(self);

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif
}

static void ten_sanitizer_memory_record_add(
    ten_sanitizer_memory_records_t *self,
    ten_sanitizer_memory_record_t *record) {
  TEN_ASSERT(self && record, "Invalid argument.");

  if (!g_memory_records_enabled) {
    return;
  }

#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

  TEN_UNUSED int rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Failed to lock.");

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
  // Capture backtrace to buffer.
  ten_backtrace_capture_to_buffer(g_backtrace_for_memory_check,
                                  record->backtrace_buffer,
                                  sizeof(record->backtrace_buffer), 1);
#endif

#if defined(TEN_USE_ASAN)
  __asan_unpoison_memory_region(&self->records_list.front,
                                sizeof(ten_listnode_t *));
  __asan_unpoison_memory_region(&self->records_list.back,
                                sizeof(ten_listnode_t *));
#endif

  ten_list_push_ptr_back(
      &self->records_list, record,
      (ten_ptr_listnode_destroy_func_t)ten_sanitizer_memory_record_destroy);

  ten_listnode_t *ptr_node = ten_list_back(&self->records_list);
  TEN_ASSERT(ptr_node, "Should not happen.");

  record->node_in_records_list = ptr_node;

  ten_hashtable_add_ptr(&self->records_hash, &record->hh_in_records_hash,
                        &record->addr, NULL);

#if defined(TEN_USE_ASAN)
  __asan_poison_memory_region(&self->records_list.front,
                              sizeof(ten_listnode_t *));
  __asan_poison_memory_region(&self->records_list.back,
                              sizeof(ten_listnode_t *));
#endif

  self->total_size += record->size;

  rc = ten_mutex_unlock(self->lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif
}

static void ten_sanitizer_memory_record_del(
    ten_sanitizer_memory_records_t *self, void *addr) {
  TEN_ASSERT(self && addr, "Invalid argument.");

  if (!g_memory_records_enabled) {
    return;
  }

#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

  TEN_UNUSED int rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Failed to lock.");

#if defined(TEN_USE_ASAN)
  __asan_unpoison_memory_region(&self->records_list.front,
                                sizeof(ten_listnode_t *));
  __asan_unpoison_memory_region(&self->records_list.back,
                                sizeof(ten_listnode_t *));
#endif

  ten_hashhandle_t *hh = ten_hashtable_find_ptr(&self->records_hash, &addr);
  if (hh) {
    ten_sanitizer_memory_record_t *record = CONTAINER_OF_FROM_FIELD(
        hh, ten_sanitizer_memory_record_t, hh_in_records_hash);
    TEN_ASSERT(record, "Should not happen.");

    ten_ptr_listnode_t *ptr_node =
        (ten_ptr_listnode_t *)record->node_in_records_list;
    TEN_ASSERT(ptr_node, "Should not happen.");

    self->total_size -= record->size;
    ten_hashtable_del(&self->records_hash, hh);

    ten_list_remove_node(&self->records_list, record->node_in_records_list);
  }

#if defined(TEN_USE_ASAN)
  __asan_poison_memory_region(&self->records_list.front,
                              sizeof(ten_listnode_t *));
  __asan_poison_memory_region(&self->records_list.back,
                              sizeof(ten_listnode_t *));
#endif

  rc = ten_mutex_unlock(self->lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif
}

void ten_sanitizer_memory_record_dump(void) {
#if defined(TEN_ENABLE_MEMORY_CHECK)

  if (!g_memory_records_enabled) {
    return;
  }

#if defined(TEN_USE_ASAN)
  __lsan_disable();
#endif

  TEN_UNUSED int rc = ten_mutex_lock(g_memory_records.lock);
  TEN_ASSERT(!rc, "Failed to lock.");

  if (g_memory_records.total_size) {
    (void)fprintf(stderr, "Memory allocation summary(%zu bytes):\n",
                  g_memory_records.total_size);
  }

#if defined(TEN_USE_ASAN)
  __asan_unpoison_memory_region(&g_memory_records.records_list.front,
                                sizeof(ten_listnode_t *));
  __asan_unpoison_memory_region(&g_memory_records.records_list.back,
                                sizeof(ten_listnode_t *));
#endif

  size_t idx = 0;
  ten_list_foreach (&g_memory_records.records_list, iter) {
    ten_sanitizer_memory_record_t *info = ten_ptr_listnode_get(iter.node);

    (void)fprintf(stderr, "----------------------------------------\n");
    (void)fprintf(stderr, "#%zu %p(%zu bytes) in %s@%s:%d\n", idx, info->addr,
                  info->size, info->func_name, info->file_name, info->lineno);

#if defined(TEN_MEMORY_CHECK_ENABLE_BACKTRACE)
    (void)fprintf(stderr, "%s\n", info->backtrace_buffer);
#endif

    idx++;
  }

#if defined(TEN_USE_ASAN)
  __asan_poison_memory_region(&g_memory_records.records_list.front,
                              sizeof(ten_listnode_t *));
  __asan_poison_memory_region(&g_memory_records.records_list.back,
                              sizeof(ten_listnode_t *));
#endif

  size_t total_size = g_memory_records.total_size;

  rc = ten_mutex_unlock(g_memory_records.lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

  if (total_size) {
    (void)fprintf(stderr, "Memory leak with %zu bytes.\n", total_size);

#if defined(TEN_USE_ASAN)
    __lsan_enable();
#endif

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

#if defined(TEN_USE_ASAN)
  __lsan_enable();
#endif

#else
  (void)fprintf(stderr, "The memory check is disabled.");
#endif
}

void *ten_sanitizer_memory_malloc(size_t size, const char *file_name,
                                  uint32_t lineno, const char *func_name) {
  void *self = malloc(size);
  TEN_ASSERT(self, "Failed to allocate memory.");
  if (!self) {
    return NULL;
  }

  if (!g_memory_records_enabled) {
    goto done;
  }

  ten_sanitizer_memory_record_t *record = ten_sanitizer_memory_record_create(
      self, size, file_name, lineno, func_name);
  TEN_ASSERT(record, "Should not happen.");
  if (!record) {
    free(self);
    return NULL;
  }

  ten_sanitizer_memory_record_add(&g_memory_records, record);

done:
  return self;
}

void *ten_sanitizer_memory_calloc(size_t cnt, size_t size,
                                  const char *file_name, uint32_t lineno,
                                  const char *func_name) {
  void *self = ten_calloc(cnt, size);
  TEN_ASSERT(self, "Failed to allocate memory.");
  if (!self) {
    return NULL;
  }

  // If memory recording is not enabled, return the allocated memory.
  if (!g_memory_records_enabled) {
    goto done;
  }

  size_t total_size = cnt * size;

  // Create a memory record.
  ten_sanitizer_memory_record_t *record = ten_sanitizer_memory_record_create(
      self, total_size, file_name, lineno, func_name);
  TEN_ASSERT(record, "Should not happen.");
  if (!record) {
    free(self);
    return NULL;
  }

  // Add the record to the global memory records.
  ten_sanitizer_memory_record_add(&g_memory_records, record);

done:
  return self;
}

void ten_sanitizer_memory_free(void *addr) {
  TEN_ASSERT(addr, "Invalid argument.");

  if (!g_memory_records_enabled) {
    goto done;
  }

  ten_sanitizer_memory_record_del(&g_memory_records, addr);

done:
  free(addr);
}

void *ten_sanitizer_memory_realloc(void *addr, size_t size,
                                   const char *file_name, uint32_t lineno,
                                   const char *func_name) {
  // The address can be NULL, if it is NULL, a new block is allocated. The
  // return value maybe NULL if size is 0.
  void *self = ten_realloc(addr, size);

  if (!g_memory_records_enabled) {
    goto done;
  }

  if (self && self != addr) {
    // We only record a new memory.
    ten_sanitizer_memory_record_t *record = ten_sanitizer_memory_record_create(
        self, size, file_name, lineno, func_name);
    if (!record) {
      free(self);
      if (addr != NULL) {
        ten_sanitizer_memory_record_del(&g_memory_records, addr);
      }
      return NULL;
    }

    if (addr != NULL) {
      ten_sanitizer_memory_record_del(&g_memory_records, addr);
    }
    ten_sanitizer_memory_record_add(&g_memory_records, record);
  }

  if (addr && size == 0) {
    // If size is 0, the memory block pointed by address will be deallocated.
    ten_sanitizer_memory_record_del(&g_memory_records, addr);
  }

done:
  return self;
}

char *ten_sanitizer_memory_strdup(const char *str, const char *file_name,
                                  uint32_t lineno, const char *func_name) {
  TEN_ASSERT(str, "Invalid argument.");

  char *self = ten_strdup(str);
  TEN_ASSERT(self, "Failed to allocate memory.");
  if (!self) {
    return NULL;
  }

  if (!g_memory_records_enabled) {
    goto done;
  }

  ten_sanitizer_memory_record_t *record = ten_sanitizer_memory_record_create(
      self, strlen(self), file_name, lineno, func_name);
  if (!record) {
    free(self);
    return NULL;
  }

  ten_sanitizer_memory_record_add(&g_memory_records, record);

done:
  return self;
}

char *ten_sanitizer_memory_strndup(const char *str, size_t size,
                                   const char *file_name, uint32_t lineno,
                                   const char *func_name) {
  TEN_ASSERT(str, "Invalid argument.");

  char *self = ten_strndup(str, size);
  TEN_ASSERT(self, "Failed to allocate memory.");
  if (!self) {
    return NULL;
  }

  if (!g_memory_records_enabled) {
    goto done;
  }

  ten_sanitizer_memory_record_t *record = ten_sanitizer_memory_record_create(
      self, strlen(self), file_name, lineno, func_name);
  if (!record) {
    free(self);
    return NULL;
  }

  ten_sanitizer_memory_record_add(&g_memory_records, record);

done:
  return self;
}
