//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from https://github.com/gpakosz/uuid4/
//
#include "ten_utils/lib/uuid.h"

#include <inttypes.h>
#include <stdio.h>

#include "ten_utils/macro/check.h"

/**
 * http://xoshiro.di.unimi.it/splitmix64.c
 * Written in 2015 by Sebastiano Vigna (vigna@acm.org)
 *
 * This is a fixed-increment version of Java 8's SplittableRandom generator.
 * See http://dx.doi.org/10.1145/2714064.2660195 and
 * http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html
 * It is a very fast generator passing BigCrush.
 */
static inline uint64_t ten_uuid4_splitmix64(uint64_t *state) {
  uint64_t z = (*state += 0x9E3779B97F4A7C15U);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9U;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBU;
  return z ^ (z >> 31);
}

/**
 * http://www.pcg-random.org/posts/developing-a-seed_seq-alternative.html
 * Written in 2015 by Melissa O'Neil (oneill@pcg-random.org)
 */
uint32_t ten_uuid4_hash(uint32_t value) {
  static uint32_t multiplier = 0x43b0d7e5U;

  value ^= multiplier;
  multiplier *= 0x931e8875U;
  value *= multiplier;
  value ^= value >> 16;

  return value;
}

uint32_t ten_uuid4_mix(uint32_t a, uint32_t b) {
  uint32_t result = 0xca01f9ddU * a - 0x4973f715U * b;
  result ^= result >> 16;
  return result;
}

static void ten_uuid4_randomize(ten_uuid4_t *self, ten_uuid4_state_t *state) {
  self->qwords[0] = ten_uuid4_splitmix64(state);
  self->qwords[1] = ten_uuid4_splitmix64(state);
}

void ten_uuid4_init_to_zeros(ten_uuid4_t *self) {
  TEN_ASSERT(self, "Invalid argment.");

  self->qwords[0] = 0;
  self->qwords[1] = 0;
}

void ten_uuid4_gen_from_seed(ten_uuid4_t *self, ten_uuid4_state_t *seed) {
  TEN_ASSERT(self && seed, "Invalid argment.");

  ten_uuid4_randomize(self, seed);

  self->bytes[6] = (self->bytes[6] & 0xf) | 0x40;
  self->bytes[8] = (self->bytes[8] & 0x3f) | 0x80;
}

void ten_uuid4_gen(ten_uuid4_t *out) {
  TEN_ASSERT(out, "Invalid argment.");

  ten_uuid4_state_t uuid_seed = 0;
  ten_uuid4_seed(&uuid_seed);
  ten_uuid4_gen_from_seed(out, &uuid_seed);
}

void ten_uuid4_gen_string(ten_string_t *out) {
  TEN_ASSERT(out, "Invalid argment.");

  ten_uuid4_state_t uuid_state = 0;
  ten_uuid4_t uuid;
  ten_uuid4_seed(&uuid_state);
  ten_uuid4_gen_from_seed(&uuid, &uuid_state);

  ten_uuid4_to_string(&uuid, out);
}

bool ten_uuid4_is_equal(const ten_uuid4_t *a, const ten_uuid4_t *b) {
  TEN_ASSERT(a && b, "Invalid argment.");

  if ((a->qwords[0] == b->qwords[0]) && (a->qwords[1] == b->qwords[1])) {
    return true;
  }
  return false;
}

void ten_uuid4_copy(ten_uuid4_t *self, ten_uuid4_t *src) {
  TEN_ASSERT(self && src, "Invalid argment.");

  self->qwords[0] = src->qwords[0];
  self->qwords[1] = src->qwords[1];
}

bool ten_uuid4_is_empty(ten_uuid4_t *self) {
  TEN_ASSERT(self, "Invalid argment.");
  return ((self->qwords[0] == 0) && (self->qwords[1] == 0)) ? true : false;
}

bool ten_uuid4_from_string(ten_uuid4_t *self, ten_string_t *in) {
  TEN_ASSERT(self && in, "Invalid argment.");

  static const uint8_t hex[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};

  static const int groups[] = {8, 4, 4, 4, 12};
  int b = 0;

  for (int i = 0; i < (int)(sizeof(groups) / sizeof(groups[0])); ++i) {
    for (int j = 0; j < groups[i]; j += 2) {
      uint8_t a1 = hex[ten_string_get_raw_str(in)[i + j] - '0'];
      uint8_t a2 = hex[ten_string_get_raw_str(in)[i + j + 1] - '0'];
      uint8_t c = (a1 << 4) | (a2 & 0xf);
      self->bytes[b++] = c;
    }
  }

  TEN_ASSERT(b == 16, "Invalid argment.");

  return true;
}

bool ten_uuid4_to_string(const ten_uuid4_t *self, ten_string_t *out) {
  TEN_ASSERT(self && out, "Invalid argment.");

  static const char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  static const int groups[] = {8, 4, 4, 4, 12};
  int b = 0;

  for (size_t i = 0; i < (sizeof(groups) / sizeof(groups[0])); ++i) {
    for (size_t j = 0; j < groups[i]; j += 2) {
      uint8_t byte = self->bytes[b++];

      ten_string_append_formatted(out, "%c", hex[byte >> 4]);
      ten_string_append_formatted(out, "%c", hex[byte & 0xf]);
    }

    if (i < (sizeof(groups) / sizeof(groups[0])) - 1) {
      ten_string_append_formatted(out, "%c", '-');
    }
  }

  return true;
}
