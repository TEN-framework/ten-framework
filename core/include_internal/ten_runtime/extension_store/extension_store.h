//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_EXTENSION_STORE_SIGNATURE 0x73826F288A62F9D2U

typedef struct ten_extension_t ten_extension_t;

typedef struct ten_extension_store_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_hashtable_t hash_table;
} ten_extension_store_t;

TEN_RUNTIME_PRIVATE_API ten_extension_store_t *ten_extension_store_create(
    ptrdiff_t offset);

TEN_RUNTIME_PRIVATE_API void ten_extension_store_destroy(
    ten_extension_store_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_extension_store_add_extension(
    ten_extension_store_t *self, ten_extension_t *extension);

TEN_RUNTIME_PRIVATE_API void ten_extension_store_del_extension(
    ten_extension_store_t *self, ten_extension_t *extension);

TEN_RUNTIME_PRIVATE_API ten_extension_t *ten_extension_store_find_extension(
    ten_extension_store_t *self, const char *extension_name, bool check_thread);
