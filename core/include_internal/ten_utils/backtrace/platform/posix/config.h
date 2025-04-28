//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)

#include "include_internal/ten_utils/backtrace/platform/posix/linux/config_linux.h"

#elif defined(OS_MACOS)

#include "include_internal/ten_utils/backtrace/platform/posix/darwin/config_mac.h"

#endif
