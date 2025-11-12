//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdbool.h>

#include "common.h"

typedef struct ten_go_app_t ten_go_app_t;

ten_go_app_t *ten_go_app_create(ten_go_handle_t go_app_index);

void ten_go_app_run(ten_go_app_t *app_bridge, bool run_in_background);

void ten_go_app_close(ten_go_app_t *app);

void ten_go_app_wait(ten_go_app_t *app);

// Invoked when the Go app finalizes.
void ten_go_app_finalize(ten_go_app_t *self);
