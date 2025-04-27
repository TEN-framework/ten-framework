//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_runtime/common/status_code.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_extension_thread_t ten_extension_thread_t;

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_dispatch_msg(
    ten_extension_thread_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_handle_start_msg_task(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_handle_in_msg_task(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_create_cmd_result_and_dispatch(
    ten_extension_thread_t *self, ten_shared_ptr_t *origin_cmd,
    TEN_STATUS_CODE status_code, const char *detail);
