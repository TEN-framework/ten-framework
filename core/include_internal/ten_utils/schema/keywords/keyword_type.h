//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "ten_utils/value/type.h"

#define TEN_SCHEMA_KEYWORD_TYPE_SIGNATURE 0xC24816B665EF018FU

typedef struct ten_schema_t ten_schema_t;

typedef struct ten_schema_keyword_type_t {
  ten_schema_keyword_t hdr;
  ten_signature_t signature;
  TEN_TYPE type;
} ten_schema_keyword_type_t;

TEN_UTILS_PRIVATE_API bool ten_schema_keyword_type_check_integrity(
    ten_schema_keyword_type_t *self);

TEN_UTILS_PRIVATE_API ten_schema_keyword_t *
ten_schema_keyword_type_create_from_value(ten_schema_t *owner,
                                          ten_value_t *value);
