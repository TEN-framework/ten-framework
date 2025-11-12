//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/lib/signature.h"

#define TEN_SCHEMA_PRIMITIVE_SIGNATURE 0x368E9692BBD7548DU

/**
 * @brief Definition of the schema for primitive data types.
 *
 * This structure represents schemas for primitive types in the TEN framework,
 * including int, uint, float, double, string, and bool.
 *
 * Currently, only the 'type' keyword is supported in the schema definition.
 * Special types like 'buf' and 'ptr' are also handled as primitives in the
 * current implementation, though they represent more complex data structures.
 *
 * The structure inherits from ten_schema_t and includes a signature field
 * for integrity verification.
 */
typedef struct ten_schema_primitive_t {
  ten_schema_t hdr;           // Base schema header.
  ten_signature_t signature;  // Integrity verification signature.
} ten_schema_primitive_t;

TEN_UTILS_PRIVATE_API bool ten_schema_primitive_check_integrity(
    ten_schema_primitive_t *self);

TEN_UTILS_PRIVATE_API ten_schema_primitive_t *ten_schema_primitive_create(void);

TEN_UTILS_PRIVATE_API void ten_schema_primitive_destroy(
    ten_schema_primitive_t *self);
