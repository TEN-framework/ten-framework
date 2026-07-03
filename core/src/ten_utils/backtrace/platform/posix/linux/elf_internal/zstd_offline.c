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

#if defined(BACKTRACE_GENERATE_ZSTD_FSE_TABLES)

// Used to generate the predefined FSE decoding tables for zstd.

#include <stdio.h>

// These values are straight from RFC 8878.

static int16_t lit[36] = {4, 3, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,
                          2, 1, 1, 1, 2, 2, 2, 2, 2,  2,  2,  2,
                          2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1};

static int16_t match[53] = {1, 4, 3, 2, 2,  2,  2,  2,  2,  1,  1, 1, 1, 1,
                            1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  1, 1, 1, 1,
                            1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  1, 1, 1, 1,
                            1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1};

static int16_t offset[29] = {1, 1, 1, 1, 1, 1, 2, 2, 2, 1,  1,  1,  1,  1, 1,
                             1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1};

static uint16_t next[256];

static void print_table(const elf_zstd_fse_baseline_entry *table, size_t size) {
  size_t i;

  printf("{\n");
  for (i = 0; i < size; i += 3) {
    int j;

    printf(" ");
    for (j = 0; j < 3 && i + j < size; ++j)
      printf(" { %u, %d, %d, %d },", table[i + j].baseline,
             table[i + j].basebits, table[i + j].bits, table[i + j].base);
    printf("\n");
  }
  printf("};\n");
}

int main() {
  elf_zstd_fse_entry lit_table[64];
  elf_zstd_fse_baseline_entry lit_baseline[64];
  elf_zstd_fse_entry match_table[64];
  elf_zstd_fse_baseline_entry match_baseline[64];
  elf_zstd_fse_entry offset_table[32];
  elf_zstd_fse_baseline_entry offset_baseline[32];

  if (!elf_zstd_build_fse(lit, sizeof lit / sizeof lit[0], next, 6,
                          lit_table)) {
    fprintf(stderr, "elf_zstd_build_fse failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (!elf_zstd_make_literal_baseline_fse(lit_table, 6, lit_baseline)) {
    fprintf(stderr, "elf_zstd_make_literal_baseline_fse failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  printf(
      "static const elf_zstd_fse_baseline_entry "
      "elf_zstd_lit_table[64] =\n");
  print_table(lit_baseline, sizeof lit_baseline / sizeof lit_baseline[0]);
  printf("\n");

  if (!elf_zstd_build_fse(match, sizeof match / sizeof match[0], next, 6,
                          match_table)) {
    fprintf(stderr, "elf_zstd_build_fse failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (!elf_zstd_make_match_baseline_fse(match_table, 6, match_baseline)) {
    fprintf(stderr, "elf_zstd_make_match_baseline_fse failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  printf(
      "static const elf_zstd_fse_baseline_entry "
      "elf_zstd_match_table[64] =\n");
  print_table(match_baseline, sizeof match_baseline / sizeof match_baseline[0]);
  printf("\n");

  if (!elf_zstd_build_fse(offset, sizeof offset / sizeof offset[0], next, 5,
                          offset_table)) {
    fprintf(stderr, "elf_zstd_build_fse failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (!elf_zstd_make_offset_baseline_fse(offset_table, 5, offset_baseline)) {
    fprintf(stderr, "elf_zstd_make_offset_baseline_fse failed\n");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  printf(
      "static const elf_zstd_fse_baseline_entry "
      "elf_zstd_offset_table[32] =\n");
  print_table(offset_baseline,
              sizeof offset_baseline / sizeof offset_baseline[0]);
  printf("\n");

  return 0;
}

#endif
