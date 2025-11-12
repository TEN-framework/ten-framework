//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "src/ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/addon/addon.h"
#include "ten_utils/lib/signature.h"

#define TEN_GO_ADDON_SIGNATURE 0x00FCE9927FA352FBU

typedef struct ten_go_addon_t {
  ten_signature_t signature;

  ten_go_bridge_t bridge;

  ten_addon_t c_addon;

  TEN_ADDON_TYPE type;

  ten_string_t addon_name;
} ten_go_addon_t;

TEN_RUNTIME_PRIVATE_API bool ten_go_addon_check_integrity(ten_go_addon_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_addon_go_handle(ten_go_addon_t *self);
