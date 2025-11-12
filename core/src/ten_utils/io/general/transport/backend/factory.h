//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/io/general/transport/backend/base.h"

TEN_UTILS_PRIVATE_API ten_transportbackend_factory_t *
ten_get_transportbackend_factory(const char *choise, const ten_string_t *uri);
