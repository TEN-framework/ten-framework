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

ten_go_error_t ten_go_cmd_create_trigger_life_cycle_cmd(uintptr_t *bridge);

ten_go_error_t ten_go_cmd_trigger_life_cycle_set_stage(uintptr_t bridge_addr,
                                                       const void *stage,
                                                       int stage_len);
