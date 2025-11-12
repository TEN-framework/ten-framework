//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

typedef struct ten_migrate_t ten_migrate_t;

TEN_UTILS_PRIVATE_API void ten_migrate_task_create_and_insert(
    ten_migrate_t *migrate);
