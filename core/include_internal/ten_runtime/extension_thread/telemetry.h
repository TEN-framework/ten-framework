//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/extension_thread/extension_thread.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_record_extension_thread_msg_queue_stay_time(
    ten_extension_thread_t *self, int64_t msg_timestamp);

#endif
