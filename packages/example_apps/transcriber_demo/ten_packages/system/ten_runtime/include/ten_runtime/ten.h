//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

// This header file should be the only header file where outside world should
// include in the C programming language.

#include "ten_runtime/ten_config.h"

#include "ten_runtime/addon/addon.h"                  // IWYU pragma: export
#include "ten_runtime/addon/extension/extension.h"    // IWYU pragma: export
#include "ten_runtime/app/app.h"                      // IWYU pragma: export
#include "ten_runtime/common/error_code.h"            // IWYU pragma: export
#include "ten_runtime/extension/extension.h"          // IWYU pragma: export
#include "ten_runtime/msg/audio_frame/audio_frame.h"  // IWYU pragma: export
#include "ten_runtime/msg/cmd/close_app/cmd.h"        // IWYU pragma: export
#include "ten_runtime/msg/cmd_result/cmd_result.h"    // IWYU pragma: export
#include "ten_runtime/msg/data/data.h"                // IWYU pragma: export
#include "ten_runtime/msg/msg.h"                      // IWYU pragma: export
#include "ten_runtime/msg/video_frame/video_frame.h"  // IWYU pragma: export
#include "ten_runtime/ten_env/ten_env.h"              // IWYU pragma: export
