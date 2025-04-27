//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/base64.h"

#include <stdlib.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/string.h"

bool ten_base64_to_string(ten_string_t *result, ten_buf_t *buf) {
  TEN_ASSERT(result && ten_string_check_integrity(result), "Invalid argument.");
  TEN_ASSERT(buf && ten_buf_check_integrity(buf), "Invalid argument.");

  const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  uint8_t *src_ptr = buf->data;
  size_t src_len = buf->content_size;

  uint64_t x = 0;
  uint64_t l = 0;
  size_t i = 0;
  for (; src_len > 0; src_ptr++, src_len--) {
    x = x << 8 | *src_ptr;
    for (l += 8; l >= 6; l -= 6) {
      ten_string_append_formatted(result, "%c",
                                  base64_chars[(x >> (l - 6)) & 0x3f]);
    }
  }

  if (l > 0) {
    x <<= 6 - l;
    ten_string_append_formatted(result, "%c", base64_chars[x & 0x3f]);
  }

  for (; ten_string_len(result) % 4;) {
    ten_string_append_formatted(result, "%c", '=');
  }

  return true;
}

bool ten_base64_from_string(ten_string_t *str, ten_buf_t *result) {
  TEN_ASSERT(str && ten_string_check_integrity(str), "Invalid argument.");
  TEN_ASSERT(result && ten_buf_check_integrity(result), "Invalid argument.");

  const char base64_decode_chars[] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57,
      58, 59, 60, 61, -1, -1, -1, 0,  -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
      7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
      25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
      37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1};

  const char *src_ptr = ten_string_get_raw_str(str);
  size_t src_size = ten_string_len(str);

  if ((src_size % 4) != 0) {
    return false;
  }

  union {
    uint64_t x;
    char c[4];
  } base64;

  base64.x = 0;
  uint8_t *base64_decoded = (uint8_t *)TEN_MALLOC(src_size);
  TEN_ASSERT(base64_decoded, "Failed to allocate memory.");

  uint8_t *dst_ptr = base64_decoded;
  for (; src_size > 0; src_ptr += 4, src_size -= 4) {
    size_t i = 0;
    size_t j = 0;
    for (i = 0; i < 4; i++) {
      if (base64_decode_chars[src_ptr[i]] == (char)-1) {
        TEN_FREE(base64_decoded);

        result->data = NULL;
        result->content_size = 0;
        result->size = 0;
        result->owns_memory = true;

        return result;
      }
      base64.x = base64.x << 6 | base64_decode_chars[src_ptr[i]];
      j += (src_ptr[i] == '=');
    }
    for (i = 3; i > j; i--) {
      *dst_ptr++ = base64.c[i - 1];
    }
  }
  *dst_ptr = '\0';

  result->data = base64_decoded;
  result->content_size = dst_ptr - base64_decoded + 1;
  result->size = result->content_size;
  result->owns_memory = true;

  return true;
}
