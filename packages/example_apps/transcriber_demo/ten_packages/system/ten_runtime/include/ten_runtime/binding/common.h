//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

typedef struct ten_binding_handle_t ten_binding_handle_t;

TEN_RUNTIME_API void ten_binding_handle_set_me_in_target_lang(
    ten_binding_handle_t *self, void *me_in_target_lang);

TEN_RUNTIME_API void *ten_binding_handle_get_me_in_target_lang(
    ten_binding_handle_t *self);
