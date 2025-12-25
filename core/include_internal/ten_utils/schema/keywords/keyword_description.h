//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "ten_utils/lib/signature.h"

#define TEN_SCHEMA_KEYWORD_DESCRIPTION_SIGNATURE 0x6F66E2F73CDEFE93U

typedef struct ten_schema_keyword_description_t {
  ten_schema_keyword_t hdr;
  ten_signature_t signature;

  // The description field is for documentation purposes only.
  // We don't need to store the actual value.
} ten_schema_keyword_description_t;

TEN_UTILS_PRIVATE_API bool ten_schema_keyword_description_check_integrity(
    ten_schema_keyword_description_t *self);

TEN_UTILS_PRIVATE_API ten_schema_keyword_t *
ten_schema_keyword_description_create_from_value(ten_schema_t *owner,
                                                 ten_value_t *value);
