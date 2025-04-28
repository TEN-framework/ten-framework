//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/random.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ten_utils/macro/memory.h"

int ten_random_string(char *buf, size_t size) {
  if (!buf || size <= 1) {
    return -1;
  }

  int r = ten_random(buf, size - 1);
  if (r < 0) {
    return r;
  }

  uint8_t *p = (uint8_t *)buf;
  for (size_t i = 0; i < size - 1; i++) {
    buf[i] = (p[i] % (uint8_t)(127 - 33)) + '!';
    // '%' is dangerous for printf family.
    // If you call printf("%s", str), it is OK,
    // Buf if you call printf(str), '%' in |str| will crash you.
    // We do not generate '%' anyway...
    if (buf[i] == '%') {
      buf[i] = '?';
    }
  }

  buf[size - 1] = '\0';

  return 0;
}

int ten_random_hex_string(char *buf, size_t size) {
  static char kHex[] = "0123456789ABCDEF";

  char *ran_buf = NULL;
  size_t len = size - 1;
  int i = 0;

  if (size <= 0) {
    goto error;
  }

  memset(buf, 0, size);
  len = (len % 2) == 0 ? len : (len - 1);
  ran_buf = (char *)TEN_MALLOC(len / 2);
  int r = ten_random(ran_buf, len / 2);
  if (r < 0) {
    goto error;
  }

  for (i = 0; i < len; i += 2) {
    uint8_t v = ran_buf[i / 2];
    buf[i] = kHex[v >> 4];
    buf[i + 1] = kHex[v & 0x0f];
  }

  TEN_FREE(ran_buf);

  return 0;

error:
  if (ran_buf) {
    TEN_FREE(ran_buf);
  }

  return -1;
}

int ten_random_base64_string(char *buf, size_t size) {
  (void)buf;
  (void)size;
  return -1;
}

int ten_random_int(int start, int end) {
  uint32_t result = 0;
  int ret = -1;

  if (end <= start) {
    return 0;
  }

  ret = ten_random(&result, sizeof(result));

  if (ret != 0) {
    return (int)result;
  }

  return (int)((result % (end - start)) + start);
}
