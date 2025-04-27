//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_extension_group_t ten_extension_group_t;

TEN_RUNTIME_PRIVATE_API bool ten_extension_group_dispatch_msg(
    ten_extension_group_t *self, ten_shared_ptr_t *msg, ten_error_t *err);
