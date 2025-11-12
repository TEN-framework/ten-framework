//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "ten_utils/lib/signature.h"

#define TEN_SCHEMA_KEYWORD_ITEMS_SIGNATURE 0x8AD7BCDE1BA0ADE7U

/**
 * @brief Structure representing the "items" keyword in an array schema.
 *
 * This structure defines the validation rules for items within an array.
 * In this implementation, all items in the array must conform to the same
 * schema.
 */
typedef struct ten_schema_keyword_items_t {
  ten_schema_keyword_t hdr;   // Base keyword header
  ten_signature_t signature;  // Signature for integrity validation

  // The schema that all array items must validate against.
  // The type of the items must be same.
  ten_schema_t *item_schema;
} ten_schema_keyword_items_t;

TEN_UTILS_PRIVATE_API bool ten_schema_keyword_items_check_integrity(
    ten_schema_keyword_items_t *self);

TEN_UTILS_PRIVATE_API ten_schema_keyword_t *
ten_schema_keyword_items_create_from_value(ten_schema_t *owner,
                                           ten_value_t *value);
