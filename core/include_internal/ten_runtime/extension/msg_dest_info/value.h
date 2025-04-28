//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/value/value.h"

typedef struct ten_msg_dest_info_t ten_msg_dest_info_t;
typedef struct ten_extension_info_t ten_extension_info_t;

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_msg_dest_info_to_value(
    ten_msg_dest_info_t *self, ten_extension_info_t *src_extension_info,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_msg_dest_info_from_value(
    ten_value_t *value, ten_list_t *extensions_info,
    ten_extension_info_t *src_extension_info, ten_error_t *err);
