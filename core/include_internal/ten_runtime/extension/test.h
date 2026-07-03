//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "ten_runtime/extension/extension.h"

TEN_RUNTIME_PRIVATE_API void ten_adjust_msg_dest_for_standalone_test_scenario(
    ten_shared_ptr_t *msg, ten_extension_t *from_extension);

TEN_RUNTIME_PRIVATE_API bool ten_add_msg_dest_for_standalone_test_scenario(
    ten_shared_ptr_t *msg, ten_extension_t *from_extension);
