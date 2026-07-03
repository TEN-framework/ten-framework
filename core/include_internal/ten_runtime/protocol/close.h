//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

typedef struct ten_protocol_t ten_protocol_t;

typedef void (*ten_protocol_on_closed_func_t)(ten_protocol_t *self,
                                              void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_protocol_set_on_closed(
    ten_protocol_t *self, ten_protocol_on_closed_func_t on_closed,
    void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_protocol_on_close(ten_protocol_t *self);

TEN_RUNTIME_API void ten_protocol_close(ten_protocol_t *self);
