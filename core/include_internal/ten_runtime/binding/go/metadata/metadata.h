//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "src/ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "src/ten_runtime/binding/go/interface/ten_runtime/metadata.h"

#define TEN_GO_METADATA_SIGNATURE 0xC3D0D91971B40C4FU

TEN_RUNTIME_PRIVATE_API bool ten_go_metadata_check_integrity(
    ten_go_metadata_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_metadata_go_handle(ten_go_metadata_t *self);

TEN_RUNTIME_PRIVATE_API ten_metadata_info_t *ten_go_metadata_c_metadata(
    ten_go_metadata_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_metadata_t *ten_go_metadata_wrap(
    ten_metadata_info_t *c_metadata);
