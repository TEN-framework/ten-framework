//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/value/value.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_error_t ten_error_t;

/**
 * @brief Note that the ownership of @a value would be transferred into the
 * TEN runtime, so the caller of this function could _not_ consider the
 * value instance is still valid.
 */
TEN_RUNTIME_API bool ten_env_set_property(ten_env_t *self, const char *path,
                                          ten_value_t *value, ten_error_t *err);

TEN_RUNTIME_API ten_value_t *ten_env_peek_property(ten_env_t *self,
                                                   const char *path,
                                                   ten_error_t *err);

TEN_RUNTIME_API bool ten_env_is_property_exist(ten_env_t *self,
                                               const char *path,
                                               ten_error_t *err);

TEN_RUNTIME_API bool ten_env_init_property_from_json(ten_env_t *self,
                                                     const char *json_str,
                                                     ten_error_t *err);
