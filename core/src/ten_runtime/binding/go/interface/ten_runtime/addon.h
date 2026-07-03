//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common.h"

void ten_go_addon_finalize(uintptr_t bridge_addr);

ten_go_error_t ten_go_addon_register_extension(const void *addon_name,
                                               int addon_name_len,
                                               uintptr_t go_addon,
                                               uintptr_t register_ctx,
                                               uintptr_t *bridge_addr);

ten_go_error_t ten_go_addon_manager_add_extension_addon(const void *addon_name,
                                                        int addon_name_len);
