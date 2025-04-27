//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "ten_utils/ten_config.h"

#if defined(BACKTRACE_GENERATE_FIXED_HUFFMAN_TABLE)

/**
 * @brief Used to generate the fixed Huffman table for block type 1.
 */

#include <stdio.h>

static uint16_t table[ZLIB_TABLE_SIZE];
static unsigned char codes[288];

int main() {
  size_t i;

  for (i = 0; i <= 143; ++i) {
    codes[i] = 8;
  }

  for (i = 144; i <= 255; ++i) {
    codes[i] = 9;
  }

  for (i = 256; i <= 279; ++i) {
    codes[i] = 7;
  }

  for (i = 280; i <= 287; ++i) {
    codes[i] = 8;
  }

  if (!elf_zlib_inflate_table(&codes[0], 288, &table[0], &table[0])) {
    fprintf(stderr, "elf_zlib_inflate_table failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  printf("static const uint16_t elf_zlib_default_table[%#zx] =\n",
         final_next_secondary + 0x100);
  printf("{\n");

  for (i = 0; i < final_next_secondary + 0x100; i += 8) {
    size_t j;

    printf(" ");

    for (j = i; j < final_next_secondary + 0x100 && j < i + 8; ++j) {
      printf(" %#x,", table[j]);
    }

    printf("\n");
  }

  printf("};\n");
  printf("\n");

  for (i = 0; i < 32; ++i) {
    codes[i] = 5;
  }

  if (!elf_zlib_inflate_table(&codes[0], 32, &table[0], &table[0])) {
    fprintf(stderr, "elf_zlib_inflate_table failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  printf("static const uint16_t elf_zlib_default_dist_table[%#zx] =\n",
         final_next_secondary + 0x100);
  printf("{\n");

  for (i = 0; i < final_next_secondary + 0x100; i += 8) {
    size_t j;

    printf(" ");

    for (j = i; j < final_next_secondary + 0x100 && j < i + 8; ++j) {
      printf(" %#x,", table[j]);
    }

    printf("\n");
  }

  printf("};\n");

  return 0;
}

#endif
