//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

// TODO(Wei): Note that this function has no effect on windows now.
TEN_RUNTIME_API void ten_global_setup_signal_stuff(void);

#if !defined(OS_WINDOWS)
TEN_RUNTIME_PRIVATE_API void ten_global_signal_alt_stack_create(void);

TEN_RUNTIME_PRIVATE_API void ten_global_signal_alt_stack_destroy(void);
#endif
