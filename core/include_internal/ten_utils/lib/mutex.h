//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/thread.h"

#define TEN_MUTEX_SIGNATURE 0x0EFAAE3C19611249U

/**
 * @brief Set the owner thread.
 * @param mutex The mutex handle.
 */
TEN_UTILS_PRIVATE_API void ten_mutex_set_owner(ten_mutex_t *mutex,
                                               ten_tid_t owner);
