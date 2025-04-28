//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_runtime/extension_group/ten_env/metadata.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/value.h"

#define TEN_PLACEHOLDER_SIGNATURE 0xE7AF02ECD77D2DCCU

typedef enum TEN_PLACEHOLDER_SCOPE {
  TEN_PLACEHOLDER_SCOPE_INVALID,

  TEN_PLACEHOLDER_SCOPE_ENV
} TEN_PLACEHOLDER_SCOPE;

/**
 * The format is as follows:
 *
 * ${scope:variable|default_value}
 *
 * Possible options for scope:
 * - env
 *
 * The '|default_value' part is optional.
 */
typedef struct ten_placeholder_t {
  ten_signature_t signature;

  TEN_PLACEHOLDER_SCOPE scope;
  ten_string_t variable;
  ten_value_t default_value;
} ten_placeholder_t;

TEN_UTILS_API bool ten_c_str_is_placeholder(const char *input);

TEN_UTILS_API ten_placeholder_t *ten_placeholder_create(void);

TEN_UTILS_API void ten_placeholder_destroy(ten_placeholder_t *self);

TEN_UTILS_API void ten_placeholder_init(ten_placeholder_t *self);

TEN_UTILS_API void ten_placeholder_deinit(ten_placeholder_t *self);

TEN_UTILS_API bool ten_placeholder_parse(ten_placeholder_t *self,
                                         const char *input, ten_error_t *err);

TEN_UTILS_API bool ten_placeholder_resolve(ten_placeholder_t *self,
                                           ten_value_t *placeholder_value,
                                           ten_error_t *err);
