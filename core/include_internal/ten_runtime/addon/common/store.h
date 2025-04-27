//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/addon/addon.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_ADDON_STORE_SIGNATURE 0x8A7F2C91E5D63B04U

typedef struct ten_addon_host_t ten_addon_host_t;
typedef struct ten_addon_t ten_addon_t;

typedef struct ten_addon_store_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_list_t store;  // ten_addon_host_t
} ten_addon_store_t;

#define TEN_ADDON_STORE_INIT_VAL                                    \
  (ten_addon_store_t) {                                             \
    TEN_ADDON_STORE_SIGNATURE, TEN_SANITIZER_THREAD_CHECK_INIT_VAL, \
        TEN_LIST_INIT_VAL                                           \
  }

#define TEN_ADDON_STORE_INIT(var)     \
  do {                                \
    (var) = TEN_ADDON_STORE_INIT_VAL; \
    ten_addon_store_init(&(var));     \
  } while (0)

TEN_RUNTIME_PRIVATE_API bool ten_addon_store_check_integrity(
    ten_addon_store_t *store, bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_addon_store_init(ten_addon_store_t *store);

TEN_RUNTIME_PRIVATE_API void ten_addon_store_deinit(ten_addon_store_t *store);

TEN_RUNTIME_PRIVATE_API void ten_addon_store_add(ten_addon_store_t *store,
                                                 ten_addon_host_t *addon);

TEN_RUNTIME_PRIVATE_API ten_addon_t *ten_addon_store_del(
    ten_addon_store_t *store, const char *name);

TEN_RUNTIME_PRIVATE_API void ten_addon_store_del_all(ten_addon_store_t *store);

TEN_RUNTIME_PRIVATE_API ten_addon_host_t *ten_addon_store_find(
    ten_addon_store_t *store, const char *name);
