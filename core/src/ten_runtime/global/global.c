//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/global/global.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/preserved_metadata.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/mutex.h"

ten_list_t g_apps;
ten_mutex_t *g_apps_mutex = NULL;

void ten_global_init(void) {
  // A pointless call, the sole purpose of which is to prevent the function from
  // being optimized.
  ten_preserved_metadata();

  ten_list_init(&g_apps);
  g_apps_mutex = ten_mutex_create();
}

void ten_global_deinit(void) {
  TEN_ASSERT(g_apps_mutex, "Invalid argument.");

  ten_mutex_lock(g_apps_mutex);
  if (ten_list_size(&g_apps)) {
    ten_mutex_unlock(g_apps_mutex);

    // There are still TEN apps, so do nothing, just return.
    return;
  }
  ten_mutex_unlock(g_apps_mutex);

  if (g_apps_mutex) {
    ten_mutex_destroy(g_apps_mutex);
    g_apps_mutex = NULL;
  }
}

void ten_global_add_app(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_mutex_lock(g_apps_mutex);
  ten_list_push_ptr_back(&g_apps, self, NULL);
  ten_mutex_unlock(g_apps_mutex);
}

void ten_global_del_app(ten_app_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check):
  // thread-check: When this function is called, the app has already been
  // destroyed, and so has the app thread.
  TEN_ASSERT(self && ten_app_check_integrity(self, false),
             "Should not happen.");

  ten_mutex_lock(g_apps_mutex);
  ten_list_remove_ptr(&g_apps, self);
  ten_mutex_unlock(g_apps_mutex);
}
