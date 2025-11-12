//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>
#include <stdint.h>

// For working memory during zstd compression, we need
// - a literal length FSE table: 512 64-bit values == 4096 bytes
// - a match length FSE table: 512 64-bit values == 4096 bytes
// - a offset FSE table: 256 64-bit values == 2048 bytes
// - a Huffman tree: 2048 uint16_t values == 4096 bytes
// - scratch space, one of
//   - to build an FSE table: 512 uint16_t values == 1024 bytes
//   - to build a Huffman tree: 512 uint16_t + 256 uint32_t == 2048 bytes

#define ZSTD_TABLE_SIZE                                                      \
  ((2 * 512 * sizeof(elf_zstd_fse_baseline_entry)) +                         \
   (256 * sizeof(elf_zstd_fse_baseline_entry)) + (2048 * sizeof(uint16_t)) + \
   (512 * sizeof(uint16_t)) + (256 * sizeof(uint32_t)))

#define ZSTD_TABLE_LITERAL_FSE_OFFSET (0)

#define ZSTD_TABLE_MATCH_FSE_OFFSET (512 * sizeof(elf_zstd_fse_baseline_entry))

#define ZSTD_TABLE_OFFSET_FSE_OFFSET \
  (ZSTD_TABLE_MATCH_FSE_OFFSET + 512 * sizeof(elf_zstd_fse_baseline_entry))

#define ZSTD_TABLE_HUFFMAN_OFFSET \
  (ZSTD_TABLE_OFFSET_FSE_OFFSET + 256 * sizeof(elf_zstd_fse_baseline_entry))

#define ZSTD_TABLE_WORK_OFFSET \
  (ZSTD_TABLE_HUFFMAN_OFFSET + 2048 * sizeof(uint16_t))

// Encode the baseline and bits into a single 32-bit value.

#define ZSTD_ENCODE_BASELINE_BITS(baseline, basebits) \
  ((uint32_t)(baseline) | ((uint32_t)(basebits) << 24))

#define ZSTD_DECODE_BASELINE(baseline_basebits) \
  ((uint32_t)(baseline_basebits) & 0xffffff)

#define ZSTD_DECODE_BASEBITS(baseline_basebits) \
  ((uint32_t)(baseline_basebits) >> 24)

#define ZSTD_LITERAL_LENGTH_BASELINE_OFFSET (16)

#define ZSTD_MATCH_LENGTH_BASELINE_OFFSET (32)

// An entry in a zstd FSE table.
typedef struct elf_zstd_fse_entry {
  // The value that this FSE entry represents.
  unsigned char symbol;
  // The number of bits to read to determine the next state.
  unsigned char bits;
  // Add the bits to this base to get the next state.
  uint16_t base;
} elf_zstd_fse_entry;

// An entry in an FSE table used for literal/match/length values.  For these we
// have to map the symbol to a baseline value, and we have to read zero or more
// bits and add that value to the baseline value.  Rather than look the values
// up in a separate table, we grow the FSE table so that we get better memory
// caching.
typedef struct elf_zstd_fse_baseline_entry {
  // The baseline for the value that this FSE entry represents..
  uint32_t baseline;
  // The number of bits to read to add to the baseline.
  unsigned char basebits;
  // The number of bits to read to determine the next state.
  unsigned char bits;
  // Add the bits to this base to get the next state.
  uint16_t base;
} elf_zstd_fse_baseline_entry;

// The information used to decompress a sequence code, which can be a literal
// length, an offset, or a match length.
typedef struct elf_zstd_seq_decode {
  const elf_zstd_fse_baseline_entry *table;
  int table_bits;
} elf_zstd_seq_decode;

TEN_UTILS_PRIVATE_API int elf_zstd_decompress(const unsigned char *pin,
                                              size_t sin,
                                              unsigned char *zdebug_table,
                                              unsigned char *pout, size_t sout);
