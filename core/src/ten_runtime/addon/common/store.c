//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/common/store.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

bool ten_addon_store_check_integrity(ten_addon_store_t *self,
                                     bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_ADDON_STORE_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

// The addon store should be initted only once.
void ten_addon_store_init(ten_addon_store_t *store) {
  TEN_ASSERT(store, "Can not init empty addon store.");

  ten_sanitizer_thread_check_init_with_current_thread(&store->thread_check);
  ten_list_init(&store->store);
}

void ten_addon_store_deinit(ten_addon_store_t *store) {
  TEN_ASSERT(store && ten_addon_store_check_integrity(store, false),
             "Invalid argument.");

  TEN_ASSERT(ten_list_is_empty(&store->store), "Should not happen.");

  ten_sanitizer_thread_check_deinit(&store->thread_check);
}

static void ten_addon_host_remove_from_store(ten_addon_host_t *addon_host) {
  TEN_ASSERT(addon_host, "Invalid argument.");

  ten_ref_dec_ref(&addon_host->ref);
}

ten_addon_host_t *ten_addon_store_find(ten_addon_store_t *store,
                                       const char *name) {
  TEN_ASSERT(store, "Invalid argument.");
  TEN_ASSERT(ten_addon_store_check_integrity(store, true), "Invalid argument.");
  TEN_ASSERT(name, "Invalid argument.");

  ten_addon_host_t *result = NULL;

  ten_list_foreach (&store->store, iter) {
    ten_addon_host_t *addon = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(addon, "Should not happen.");

    if (ten_string_is_equal_c_str(&addon->name, name)) {
      result = addon;
      break;
    }
  }

  return result;
}

void ten_addon_store_add(ten_addon_store_t *store, ten_addon_host_t *addon) {
  TEN_ASSERT(store, "Invalid argument.");
  TEN_ASSERT(ten_addon_store_check_integrity(store, true), "Invalid argument.");
  TEN_ASSERT(addon, "Invalid argument.");

  ten_list_push_ptr_back(
      &store->store, addon,
      (ten_ptr_listnode_destroy_func_t)ten_addon_host_remove_from_store);
}

ten_addon_t *ten_addon_store_del(ten_addon_store_t *store, const char *name) {
  TEN_ASSERT(store, "Invalid argument.");
  TEN_ASSERT(ten_addon_store_check_integrity(store, true), "Invalid argument.");
  TEN_ASSERT(name, "Invalid argument.");

  ten_addon_t *addon = NULL;

  ten_list_foreach (&store->store, iter) {
    ten_addon_host_t *addon_host = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(addon_host, "Should not happen.");

    if (ten_string_is_equal_c_str(&addon_host->name, name)) {
      addon = addon_host->addon;
      ten_list_remove_node(&store->store, iter.node);
      break;
    }
  }

  return addon;
}

void ten_addon_store_del_all(ten_addon_store_t *store) {
  TEN_ASSERT(store, "Invalid argument.");
  TEN_ASSERT(ten_addon_store_check_integrity(store, true), "Invalid argument.");

  // Clear the store's list, which will call the destroy function for each node,
  // properly decreasing the refcount of each addon.
  ten_list_clear(&store->store);
}
