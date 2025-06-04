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

typedef struct ten_go_extension_tester_t ten_go_extension_tester_t;

ten_go_error_t ten_go_extension_tester_create(
    ten_go_handle_t go_extension_tester,
    ten_go_extension_tester_t **bridge_addr);

void ten_go_extension_tester_finalize(
    ten_go_extension_tester_t *extension_tester);

ten_go_error_t ten_go_extension_tester_set_test_mode_single(
    ten_go_extension_tester_t *extension_tester, const void *addon_name,
    int addon_name_len, const void *property_json_str,
    int property_json_str_len);

ten_go_error_t ten_go_extension_tester_set_timeout(
    ten_go_extension_tester_t *extension_tester, uint64_t timeout_us);

ten_go_error_t ten_go_extension_tester_run(
    ten_go_extension_tester_t *extension_tester);
