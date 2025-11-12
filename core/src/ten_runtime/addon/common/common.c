//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/common/common.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "include_internal/ten_runtime/app/app.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/check.h"

ten_addon_host_t *ten_addon_store_find_by_type(ten_app_t *app,
                                               TEN_ADDON_TYPE addon_type,
                                               const char *addon_name) {
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  TEN_ASSERT(addon_name, "Should not happen.");

  ten_addon_store_t *addon_store = NULL;

  switch (addon_type) {
  case TEN_ADDON_TYPE_EXTENSION:
    addon_store = &app->extension_store;
    break;

  case TEN_ADDON_TYPE_EXTENSION_GROUP:
    addon_store = &app->extension_group_store;
    break;

  case TEN_ADDON_TYPE_PROTOCOL:
    addon_store = &app->protocol_store;
    break;

  case TEN_ADDON_TYPE_ADDON_LOADER:
    addon_store = &app->addon_loader_store;
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  TEN_ASSERT(addon_store, "Should not happen.");

  return ten_addon_store_find(addon_store, addon_name);
}
