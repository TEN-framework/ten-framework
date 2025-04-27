//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from https://github.com/gpakosz/uuid4/
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "ten_utils/lib/string.h"

typedef uint64_t ten_uuid4_state_t;

typedef union ten_uuid4_t {
  uint8_t bytes[16];
  uint32_t dwords[4];
  uint64_t qwords[2];
} ten_uuid4_t;

TEN_UTILS_PRIVATE_API uint32_t ten_uuid4_mix(uint32_t a, uint32_t b);

TEN_UTILS_PRIVATE_API uint32_t ten_uuid4_hash(uint32_t value);

/**
 * Seeds the state of the PRNG used to generate version 4 UUIDs.
 *
 * @param a pointer to a variable holding the state.
 *
 * @return `true` on success, otherwise `false`.
 */
TEN_UTILS_API void ten_uuid4_seed(ten_uuid4_state_t *seed);

TEN_UTILS_API void ten_uuid4_init_to_zeros(ten_uuid4_t *self);

/**
 * Generates a version 4 UUID, see https://tools.ietf.org/html/rfc4122.
 *
 * @param state the state of the PRNG used to generate version 4 UUIDs.
 * @param out the recipient for the UUID.
 */
TEN_UTILS_PRIVATE_API void ten_uuid4_gen_from_seed(ten_uuid4_t *self,
                                                   ten_uuid4_state_t *seed);

TEN_UTILS_API void ten_uuid4_gen(ten_uuid4_t *out);

TEN_UTILS_API void ten_uuid4_gen_string(ten_string_t *out);

TEN_UTILS_API bool ten_uuid4_is_equal(const ten_uuid4_t *a,
                                      const ten_uuid4_t *b);

TEN_UTILS_API void ten_uuid4_copy(ten_uuid4_t *self, ten_uuid4_t *src);

TEN_UTILS_API bool ten_uuid4_is_empty(ten_uuid4_t *self);

TEN_UTILS_API bool ten_uuid4_from_string(ten_uuid4_t *self, ten_string_t *in);

/**
 * Converts a UUID to a `NUL` terminated string.
 * The string format is like 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx', y is either
 * 8, 9, a or b
 *
 * @param out destination ten string
 *
 * @return `true` on success, otherwise `false`.
 */
TEN_UTILS_API bool ten_uuid4_to_string(const ten_uuid4_t *self,
                                       ten_string_t *out);
