//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdint.h>

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_extension_thread_t ten_extension_thread_t;
typedef struct ten_extension_t ten_extension_t;

#if defined(TEN_ENABLE_TEN_RUST_APIS)

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_record_extension_thread_msg_queue_stay_time(
    ten_extension_thread_t *self, int64_t msg_timestamp);

TEN_RUNTIME_PRIVATE_API void ten_extension_record_lifecycle_duration(
    ten_extension_t *self, const char *stage, int64_t duration_us);

TEN_RUNTIME_PRIVATE_API void ten_extension_record_cmd_processing_duration(
    ten_extension_t *self, ten_shared_ptr_t *cmd_result,
    int64_t on_cmd_start_us);

TEN_RUNTIME_PRIVATE_API void ten_extension_record_callback_execution_duration(
    ten_extension_t *self, const char *msg_type, const char *msg_name,
    int64_t duration_us);

#endif
