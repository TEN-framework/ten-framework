//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_utils/lib/json.h"

TEN_RUNTIME_PRIVATE_API ten_json_t *ten_go_json_loads(const void *json_bytes,
                                                      int json_bytes_len,
                                                      ten_go_error_t *status);
