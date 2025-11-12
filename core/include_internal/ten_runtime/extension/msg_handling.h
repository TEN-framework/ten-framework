//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_extension_t ten_extension_t;
typedef struct ten_msg_conversion_t ten_msg_conversion_t;

TEN_RUNTIME_PRIVATE_API void ten_extension_handle_in_msg(ten_extension_t *self,
                                                         ten_shared_ptr_t *msg);
