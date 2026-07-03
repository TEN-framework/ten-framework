//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_store/extension_store.h"

#include <stddef.h>
#include <stdlib.h>

#include "include_internal/ten_runtime/extension/extension.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"
#include "ten_utils/sanitizer/thread_check.h"

static bool ten_extension_store_check_integrity(ten_extension_store_t *self,
                                                bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_STORE_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_extension_store_init(ten_extension_store_t *self,
                                     ptrdiff_t hh_offset) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_EXTENSION_STORE_SIGNATURE);

  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  ten_hashtable_init(&self->hash_table, hh_offset);
}

ten_extension_store_t *ten_extension_store_create(ptrdiff_t offset) {
  ten_extension_store_t *self =
      (ten_extension_store_t *)TEN_MALLOC(sizeof(ten_extension_store_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_extension_store_init(self, offset);
  return self;
}

static void ten_extension_store_deinit(ten_extension_store_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: extension_store will be destroyed _after_ the extension
  // thread is joined. So we can not check the thread safety here.
  TEN_ASSERT(ten_extension_store_check_integrity(self, false),
             "Invalid use of extension_store %p.", self);

  ten_hashtable_deinit(&self->hash_table);
  ten_sanitizer_thread_check_deinit(&self->thread_check);

  ten_signature_set(&self->signature, 0);
}

void ten_extension_store_destroy(ten_extension_store_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: extension_store will be destroyed _after_ the extension
  // thread is joined. So we can not check the thread safety here.
  TEN_ASSERT(ten_extension_store_check_integrity(self, false),
             "Invalid use of extension_store %p.", self);

  ten_extension_store_deinit(self);
  TEN_FREE(self);
}

bool ten_extension_store_add_extension(ten_extension_store_t *self,
                                       ten_extension_t *extension) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_store_check_integrity(self, true),
             "Invalid use of extension_store %p.", self);

  TEN_ASSERT(extension, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Invalid use of extension %p.", extension);

  bool result = true;
  ten_hashhandle_t *found = ten_hashtable_find_string(
      &self->hash_table, ten_string_get_raw_str(&extension->name));
  if (found) {
    TEN_LOGE("Failed to have extension with name: %s",
             ten_extension_get_name(extension, true));
    result = false;
    goto done;
  }

  ten_hashtable_add_string(&self->hash_table, &extension->hh_in_extension_store,
                           ten_string_get_raw_str(&extension->name), NULL);

done:
  return result;
}

void ten_extension_store_del_extension(ten_extension_store_t *self,
                                       ten_extension_t *extension) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_store_check_integrity(self, true),
             "Invalid use of extension_store %p.", self);

  TEN_ASSERT(extension, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Invalid use of extension %p.", extension);

  ten_hashtable_del(&self->hash_table, &extension->hh_in_extension_store);
}

ten_extension_t *ten_extension_store_find_extension(ten_extension_store_t *self,
                                                    const char *extension_name,
                                                    bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_store_check_integrity(self, check_thread),
             "Invalid use of extension_store %p.", self);

  TEN_ASSERT(extension_name, "Should not happen.");

  ten_extension_t *extension = NULL;

  ten_hashhandle_t *hh =
      ten_hashtable_find_string(&self->hash_table, extension_name);
  if (hh) {
    extension =
        CONTAINER_OF_FROM_FIELD(hh, ten_extension_t, hh_in_extension_store);

    return extension;
  }

  return extension;
}
