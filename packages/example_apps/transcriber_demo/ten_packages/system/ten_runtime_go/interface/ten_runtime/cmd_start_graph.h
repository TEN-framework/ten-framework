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

ten_go_error_t ten_go_cmd_create_start_graph_cmd(uintptr_t *bridge);

ten_go_error_t ten_go_cmd_start_graph_set_predefined_graph_name(
    uintptr_t bridge_addr, const void *predefined_graph_name,
    int predefined_graph_name_len);

ten_go_error_t ten_go_cmd_start_graph_set_graph_from_json_bytes(
    uintptr_t bridge_addr, const void *json_bytes, int json_bytes_len);

ten_go_error_t ten_go_cmd_start_graph_set_long_running_mode(
    uintptr_t bridge_addr, bool long_running_mode);
