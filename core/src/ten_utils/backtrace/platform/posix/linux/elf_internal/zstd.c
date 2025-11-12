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

#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zstd.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zutils.h"

static int elf_zstd_build_fse(const int16_t *, int, uint16_t *, int,
                              elf_zstd_fse_entry *);

// Read a zstd FSE table and build the decoding table in *TABLE, updating *PPIN
// as it reads.  ZDEBUG_TABLE is scratch space; it must be enough for 512
// uint16_t values (1024 bytes).  MAXIDX is the maximum number of symbols
// permitted. *TABLE_BITS is the maximum number of bits for symbols in the
// table: the size of *TABLE is at least 1 << *TABLE_BITS.  This updates
// *TABLE_BITS to the actual number of bits.  Returns 1 on success, 0 on
// error.
static int elf_zstd_read_fse(const unsigned char **ppin,
                             const unsigned char *pinend,
                             uint16_t *zdebug_table, int maxidx,
                             elf_zstd_fse_entry *table, int *table_bits) {
  const unsigned char *pin = NULL;
  int16_t *norm = NULL;
  uint16_t *next = NULL;
  uint64_t val = 0;
  unsigned int bits = 0;
  int accuracy_log = 0;
  uint32_t remaining = 0;
  uint32_t threshold = 0;
  int bits_needed = 0;
  int idx = 0;
  int prev0 = 0;

  pin = *ppin;

  norm = (int16_t *)zdebug_table;
  next = zdebug_table + 256;

  if (unlikely(pin + 3 >= pinend)) {
    elf_uncompress_failed();
    return 0;
  }

  // Align PIN to a 32-bit boundary.

  val = 0;
  bits = 0;
  while ((((uintptr_t)pin) & 3) != 0) {
    val |= (uint64_t)*pin << bits;
    bits += 8;
    ++pin;
  }

  if (!elf_fetch_bits(&pin, pinend, &val, &bits)) {
    return 0;
  }

  accuracy_log = (val & 0xf) + 5;
  if (accuracy_log > *table_bits) {
    elf_uncompress_failed();
    return 0;
  }
  *table_bits = accuracy_log;
  val >>= 4;
  bits -= 4;

  // This code is mostly copied from the reference implementation.

  // The number of remaining probabilities, plus 1.  This sets the number of
  // bits that need to be read for the next value.
  remaining = (1 << accuracy_log) + 1;

  // The current difference between small and large values, which depends on
  // the number of remaining values.  Small values use one less bit.
  threshold = 1 << accuracy_log;

  // The number of bits used to compute threshold.
  bits_needed = accuracy_log + 1;

  // The next character value.
  idx = 0;

  // Whether the last count was 0.
  prev0 = 0;

  while (remaining > 1 && idx <= maxidx) {
    uint32_t max = 0;
    int32_t count = 0;

    if (!elf_fetch_bits(&pin, pinend, &val, &bits)) {
      return 0;
    }

    if (prev0) {
      int zidx = 0;

      // Previous count was 0, so there is a 2-bit repeat flag.  If the
      // 2-bit flag is 0b11, it adds 3 and then there is another repeat
      // flag.
      zidx = idx;
      while ((val & 0xfff) == 0xfff) {
        zidx += 3 * 6;
        val >>= 12;
        bits -= 12;
        if (!elf_fetch_bits(&pin, pinend, &val, &bits)) {
          return 0;
        }
      }
      while ((val & 3) == 3) {
        zidx += 3;
        val >>= 2;
        bits -= 2;
        if (!elf_fetch_bits(&pin, pinend, &val, &bits)) {
          return 0;
        }
      }
      // We have at least 13 bits here, don't need to fetch.
      zidx += val & 3;
      val >>= 2;
      bits -= 2;

      if (unlikely(zidx > maxidx)) {
        elf_uncompress_failed();
        return 0;
      }

      for (; idx < zidx; idx++) {
        norm[idx] = 0;
      }

      prev0 = 0;
      continue;
    }

    max = (2 * threshold - 1) - remaining;
    if ((val & (threshold - 1)) < max) {
      // A small value.
      count = (int32_t)((uint32_t)val & (threshold - 1));
      val >>= bits_needed - 1;
      bits -= bits_needed - 1;
    } else {
      // A large value.
      count = (int32_t)((uint32_t)val & (2 * threshold - 1));
      if (count >= (int32_t)threshold) {
        count -= (int32_t)max;
      }
      val >>= bits_needed;
      bits -= bits_needed;
    }

    count--;
    if (count >= 0) {
      remaining -= count;
    } else {
      remaining--;
    }
    if (unlikely(idx >= 256)) {
      elf_uncompress_failed();
      return 0;
    }
    norm[idx] = (int16_t)count;
    ++idx;

    prev0 = count == 0;

    while (remaining < threshold) {
      bits_needed--;
      threshold >>= 1;
    }
  }

  if (unlikely(remaining != 1)) {
    elf_uncompress_failed();
    return 0;
  }

  // If we've read ahead more than a byte, back up.
  while (bits >= 8) {
    --pin;
    bits -= 8;
  }

  *ppin = pin;

  for (; idx <= maxidx; idx++) {
    norm[idx] = 0;
  }

  return elf_zstd_build_fse(norm, idx, next, *table_bits, table);
}

// Build the FSE decoding table from a list of probabilities.  This reads from
// NORM of length IDX, uses NEXT as scratch space, and writes to *TABLE, whose
// size is TABLE_BITS.
static int elf_zstd_build_fse(const int16_t *norm, int idx, uint16_t *next,
                              int table_bits, elf_zstd_fse_entry *table) {
  int table_size = 0;
  int high_threshold = 0;
  int i = 0;
  int pos = 0;
  int step = 0;
  int mask = 0;

  table_size = 1 << table_bits;
  high_threshold = table_size - 1;
  for (i = 0; i < idx; i++) {
    int16_t n = norm[i];
    if (n >= 0) {
      next[i] = (uint16_t)n;
    } else {
      table[high_threshold].symbol = (unsigned char)i;
      high_threshold--;
      next[i] = 1;
    }
  }

  pos = 0;
  step = (table_size >> 1) + (table_size >> 3) + 3;
  mask = table_size - 1;
  for (i = 0; i < idx; i++) {
    int n = (int)norm[i];
    int j = 0;

    for (j = 0; j < n; j++) {
      table[pos].symbol = (unsigned char)i;
      pos = (pos + step) & mask;
      while (unlikely(pos > high_threshold)) {
        pos = (pos + step) & mask;
      }
    }
  }
  if (unlikely(pos != 0)) {
    elf_uncompress_failed();
    return 0;
  }

  for (i = 0; i < table_size; i++) {
    unsigned char sym = table[i].symbol;
    uint16_t next_state = next[sym];
    int high_bit = 0;
    int bits = 0;

    ++next[sym];

    if (next_state == 0) {
      elf_uncompress_failed();
      return 0;
    }
    high_bit = 31 - __builtin_clz(next_state);

    bits = table_bits - high_bit;
    table[i].bits = (unsigned char)bits;
    table[i].base = (uint16_t)((next_state << bits) - table_size);
  }

  return 1;
}

// Given a literal length code, we need to read a number of bits and add that
// to a baseline.  For states 0 to 15 the baseline is the state and the number
// of bits is zero.
static const uint32_t elf_zstd_literal_length_base[] = {
    ZSTD_ENCODE_BASELINE_BITS(16, 1),     ZSTD_ENCODE_BASELINE_BITS(18, 1),
    ZSTD_ENCODE_BASELINE_BITS(20, 1),     ZSTD_ENCODE_BASELINE_BITS(22, 1),
    ZSTD_ENCODE_BASELINE_BITS(24, 2),     ZSTD_ENCODE_BASELINE_BITS(28, 2),
    ZSTD_ENCODE_BASELINE_BITS(32, 3),     ZSTD_ENCODE_BASELINE_BITS(40, 3),
    ZSTD_ENCODE_BASELINE_BITS(48, 4),     ZSTD_ENCODE_BASELINE_BITS(64, 6),
    ZSTD_ENCODE_BASELINE_BITS(128, 7),    ZSTD_ENCODE_BASELINE_BITS(256, 8),
    ZSTD_ENCODE_BASELINE_BITS(512, 9),    ZSTD_ENCODE_BASELINE_BITS(1024, 10),
    ZSTD_ENCODE_BASELINE_BITS(2048, 11),  ZSTD_ENCODE_BASELINE_BITS(4096, 12),
    ZSTD_ENCODE_BASELINE_BITS(8192, 13),  ZSTD_ENCODE_BASELINE_BITS(16384, 14),
    ZSTD_ENCODE_BASELINE_BITS(32768, 15), ZSTD_ENCODE_BASELINE_BITS(65536, 16)};

// The same applies to match length codes.  For states 0 to 31 the baseline is
// the state + 3 and the number of bits is zero.
static const uint32_t elf_zstd_match_length_base[] = {
    ZSTD_ENCODE_BASELINE_BITS(35, 1),     ZSTD_ENCODE_BASELINE_BITS(37, 1),
    ZSTD_ENCODE_BASELINE_BITS(39, 1),     ZSTD_ENCODE_BASELINE_BITS(41, 1),
    ZSTD_ENCODE_BASELINE_BITS(43, 2),     ZSTD_ENCODE_BASELINE_BITS(47, 2),
    ZSTD_ENCODE_BASELINE_BITS(51, 3),     ZSTD_ENCODE_BASELINE_BITS(59, 3),
    ZSTD_ENCODE_BASELINE_BITS(67, 4),     ZSTD_ENCODE_BASELINE_BITS(83, 4),
    ZSTD_ENCODE_BASELINE_BITS(99, 5),     ZSTD_ENCODE_BASELINE_BITS(131, 7),
    ZSTD_ENCODE_BASELINE_BITS(259, 8),    ZSTD_ENCODE_BASELINE_BITS(515, 9),
    ZSTD_ENCODE_BASELINE_BITS(1027, 10),  ZSTD_ENCODE_BASELINE_BITS(2051, 11),
    ZSTD_ENCODE_BASELINE_BITS(4099, 12),  ZSTD_ENCODE_BASELINE_BITS(8195, 13),
    ZSTD_ENCODE_BASELINE_BITS(16387, 14), ZSTD_ENCODE_BASELINE_BITS(32771, 15),
    ZSTD_ENCODE_BASELINE_BITS(65539, 16)};

// Convert the literal length FSE table FSE_TABLE to an FSE baseline table at
// BASELINE_TABLE.  Note that FSE_TABLE and BASELINE_TABLE will overlap.

static int elf_zstd_make_literal_baseline_fse(
    const elf_zstd_fse_entry *fse_table, int table_bits,
    elf_zstd_fse_baseline_entry *baseline_table) {
  size_t count = 1U << table_bits;
  const elf_zstd_fse_entry *pfse = fse_table + count;
  elf_zstd_fse_baseline_entry *pbaseline = baseline_table + count;

  // Convert backward to avoid overlap.

  while (pfse > fse_table) {
    unsigned char symbol = 0;
    unsigned char bits = 0;
    uint16_t base = 0;

    --pfse;
    --pbaseline;
    symbol = pfse->symbol;
    bits = pfse->bits;
    base = pfse->base;
    if (symbol < ZSTD_LITERAL_LENGTH_BASELINE_OFFSET) {
      pbaseline->baseline = (uint32_t)symbol;
      pbaseline->basebits = 0;
    } else {
      unsigned int idx = 0;
      uint32_t basebits = 0;

      if (unlikely(symbol > 35)) {
        elf_uncompress_failed();
        return 0;
      }
      idx = symbol - ZSTD_LITERAL_LENGTH_BASELINE_OFFSET;
      basebits = elf_zstd_literal_length_base[idx];
      pbaseline->baseline = ZSTD_DECODE_BASELINE(basebits);
      pbaseline->basebits = ZSTD_DECODE_BASEBITS(basebits);
    }
    pbaseline->bits = bits;
    pbaseline->base = base;
  }

  return 1;
}

// Convert the offset length FSE table FSE_TABLE to an FSE baseline table at
// BASELINE_TABLE.  Note that FSE_TABLE and BASELINE_TABLE will overlap.
static int elf_zstd_make_offset_baseline_fse(
    const elf_zstd_fse_entry *fse_table, int table_bits,
    elf_zstd_fse_baseline_entry *baseline_table) {
  size_t count = 1U << table_bits;
  const elf_zstd_fse_entry *pfse = fse_table + count;
  elf_zstd_fse_baseline_entry *pbaseline = baseline_table + count;

  // Convert backward to avoid overlap.

  while (pfse > fse_table) {
    unsigned char symbol = 0;
    unsigned char bits = 0;
    uint16_t base = 0;

    --pfse;
    --pbaseline;
    symbol = pfse->symbol;
    bits = pfse->bits;
    base = pfse->base;
    if (unlikely(symbol > 31)) {
      elf_uncompress_failed();
      return 0;
    }

    // The simple way to write this is
    //
    //   pbaseline->baseline = (uint32_t)1 << symbol;
    //   pbaseline->basebits = symbol;
    //
    // That will give us an offset value that corresponds to the one
    // described in the RFC.  However, for offset values > 3, we have to
    // subtract 3.  And for offset values 1, 2, 3 we use a repeated offset.
    // The baseline is always a power of 2, and is never 0, so for these low
    // values we will see one entry that is baseline 1, basebits 0, and one
    // entry that is baseline 2, basebits 1.  All other entries will have
    // baseline >= 4 and basebits >= 2.
    //
    // So we can check for RFC offset <= 3 by checking for basebits <= 1.
    // And that means that we can subtract 3 here and not worry about doing
    // it in the hot loop.

    pbaseline->baseline = (uint32_t)1 << symbol;
    if (symbol >= 2) {
      pbaseline->baseline -= 3;
    }
    pbaseline->basebits = symbol;
    pbaseline->bits = bits;
    pbaseline->base = base;
  }

  return 1;
}

// Convert the match length FSE table FSE_TABLE to an FSE baseline table at
// BASELINE_TABLE.  Note that FSE_TABLE and BASELINE_TABLE will overlap.
static int elf_zstd_make_match_baseline_fse(
    const elf_zstd_fse_entry *fse_table, int table_bits,
    elf_zstd_fse_baseline_entry *baseline_table) {
  size_t count = 1U << table_bits;
  const elf_zstd_fse_entry *pfse = fse_table + count;
  elf_zstd_fse_baseline_entry *pbaseline = baseline_table + count;

  // Convert backward to avoid overlap.

  while (pfse > fse_table) {
    unsigned char symbol = 0;
    unsigned char bits = 0;
    uint16_t base = 0;

    --pfse;
    --pbaseline;
    symbol = pfse->symbol;
    bits = pfse->bits;
    base = pfse->base;
    if (symbol < ZSTD_MATCH_LENGTH_BASELINE_OFFSET) {
      pbaseline->baseline = (uint32_t)symbol + 3;
      pbaseline->basebits = 0;
    } else {
      unsigned int idx = 0;
      uint32_t basebits = 0;

      if (unlikely(symbol > 52)) {
        elf_uncompress_failed();
        return 0;
      }
      idx = symbol - ZSTD_MATCH_LENGTH_BASELINE_OFFSET;
      basebits = elf_zstd_match_length_base[idx];
      pbaseline->baseline = ZSTD_DECODE_BASELINE(basebits);
      pbaseline->basebits = ZSTD_DECODE_BASEBITS(basebits);
    }
    pbaseline->bits = bits;
    pbaseline->base = base;
  }

  return 1;
}

// The fixed tables generated by the #ifdef'ed out main function
// above.

static const elf_zstd_fse_baseline_entry elf_zstd_lit_table[64] = {
    {0, 0, 4, 0},      {0, 0, 4, 16},     {1, 0, 5, 32},     {3, 0, 5, 0},
    {4, 0, 5, 0},      {6, 0, 5, 0},      {7, 0, 5, 0},      {9, 0, 5, 0},
    {10, 0, 5, 0},     {12, 0, 5, 0},     {14, 0, 6, 0},     {16, 1, 5, 0},
    {20, 1, 5, 0},     {22, 1, 5, 0},     {28, 2, 5, 0},     {32, 3, 5, 0},
    {48, 4, 5, 0},     {64, 6, 5, 32},    {128, 7, 5, 0},    {256, 8, 6, 0},
    {1024, 10, 6, 0},  {4096, 12, 6, 0},  {0, 0, 4, 32},     {1, 0, 4, 0},
    {2, 0, 5, 0},      {4, 0, 5, 32},     {5, 0, 5, 0},      {7, 0, 5, 32},
    {8, 0, 5, 0},      {10, 0, 5, 32},    {11, 0, 5, 0},     {13, 0, 6, 0},
    {16, 1, 5, 32},    {18, 1, 5, 0},     {22, 1, 5, 32},    {24, 2, 5, 0},
    {32, 3, 5, 32},    {40, 3, 5, 0},     {64, 6, 4, 0},     {64, 6, 4, 16},
    {128, 7, 5, 32},   {512, 9, 6, 0},    {2048, 11, 6, 0},  {0, 0, 4, 48},
    {1, 0, 4, 16},     {2, 0, 5, 32},     {3, 0, 5, 32},     {5, 0, 5, 32},
    {6, 0, 5, 32},     {8, 0, 5, 32},     {9, 0, 5, 32},     {11, 0, 5, 32},
    {12, 0, 5, 32},    {15, 0, 6, 0},     {18, 1, 5, 32},    {20, 1, 5, 32},
    {24, 2, 5, 32},    {28, 2, 5, 32},    {40, 3, 5, 32},    {48, 4, 5, 32},
    {65536, 16, 6, 0}, {32768, 15, 6, 0}, {16384, 14, 6, 0}, {8192, 13, 6, 0},
};

static const elf_zstd_fse_baseline_entry elf_zstd_match_table[64] = {
    {3, 0, 6, 0},     {4, 0, 4, 0},      {5, 0, 5, 32},     {6, 0, 5, 0},
    {8, 0, 5, 0},     {9, 0, 5, 0},      {11, 0, 5, 0},     {13, 0, 6, 0},
    {16, 0, 6, 0},    {19, 0, 6, 0},     {22, 0, 6, 0},     {25, 0, 6, 0},
    {28, 0, 6, 0},    {31, 0, 6, 0},     {34, 0, 6, 0},     {37, 1, 6, 0},
    {41, 1, 6, 0},    {47, 2, 6, 0},     {59, 3, 6, 0},     {83, 4, 6, 0},
    {131, 7, 6, 0},   {515, 9, 6, 0},    {4, 0, 4, 16},     {5, 0, 4, 0},
    {6, 0, 5, 32},    {7, 0, 5, 0},      {9, 0, 5, 32},     {10, 0, 5, 0},
    {12, 0, 6, 0},    {15, 0, 6, 0},     {18, 0, 6, 0},     {21, 0, 6, 0},
    {24, 0, 6, 0},    {27, 0, 6, 0},     {30, 0, 6, 0},     {33, 0, 6, 0},
    {35, 1, 6, 0},    {39, 1, 6, 0},     {43, 2, 6, 0},     {51, 3, 6, 0},
    {67, 4, 6, 0},    {99, 5, 6, 0},     {259, 8, 6, 0},    {4, 0, 4, 32},
    {4, 0, 4, 48},    {5, 0, 4, 16},     {7, 0, 5, 32},     {8, 0, 5, 32},
    {10, 0, 5, 32},   {11, 0, 5, 32},    {14, 0, 6, 0},     {17, 0, 6, 0},
    {20, 0, 6, 0},    {23, 0, 6, 0},     {26, 0, 6, 0},     {29, 0, 6, 0},
    {32, 0, 6, 0},    {65539, 16, 6, 0}, {32771, 15, 6, 0}, {16387, 14, 6, 0},
    {8195, 13, 6, 0}, {4099, 12, 6, 0},  {2051, 11, 6, 0},  {1027, 10, 6, 0},
};

static const elf_zstd_fse_baseline_entry elf_zstd_offset_table[32] = {
    {1, 0, 5, 0},          {61, 6, 4, 0},         {509, 9, 5, 0},
    {32765, 15, 5, 0},     {2097149, 21, 5, 0},   {5, 3, 5, 0},
    {125, 7, 4, 0},        {4093, 12, 5, 0},      {262141, 18, 5, 0},
    {8388605, 23, 5, 0},   {29, 5, 5, 0},         {253, 8, 4, 0},
    {16381, 14, 5, 0},     {1048573, 20, 5, 0},   {1, 2, 5, 0},
    {125, 7, 4, 16},       {2045, 11, 5, 0},      {131069, 17, 5, 0},
    {4194301, 22, 5, 0},   {13, 4, 5, 0},         {253, 8, 4, 16},
    {8189, 13, 5, 0},      {524285, 19, 5, 0},    {2, 1, 5, 0},
    {61, 6, 4, 16},        {1021, 10, 5, 0},      {65533, 16, 5, 0},
    {268435453, 28, 5, 0}, {134217725, 27, 5, 0}, {67108861, 26, 5, 0},
    {33554429, 25, 5, 0},  {16777213, 24, 5, 0},
};

// This is like elf_fetch_bits, but it fetchs the bits backward, and ensures at
// least 16 bits. This is for zstd decompression.
static int elf_fetch_bits_backward(const unsigned char **ppin,
                                   const unsigned char *pinend, uint64_t *pval,
                                   unsigned int *pbits) {
  unsigned int bits = 0;
  const unsigned char *pin = NULL;
  uint64_t val = 0;
  uint32_t next = 0;

  bits = *pbits;
  if (bits >= 16) {
    return 1;
  }
  pin = *ppin;
  val = *pval;

  if (unlikely(pin <= pinend)) {
    if (bits == 0) {
      elf_uncompress_failed();
      return 0;
    }
    return 1;
  }

  pin -= 4;

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) &&                               \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ ||                     \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  // We've ensured that PIN is aligned.
  next = *(const uint32_t *)pin;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  next = __builtin_bswap32(next);
#endif
#else
  next = pin[0] | (pin[1] << 8) | (pin[2] << 16) | (pin[3] << 24);
#endif

  val <<= 32;
  val |= next;
  bits += 32;

  if (unlikely(pin < pinend)) {
    val >>= (pinend - pin) * 8;
    bits -= (pinend - pin) * 8;
  }

  *ppin = pin;
  *pval = val;
  *pbits = bits;
  return 1;
}

// Initialize backward fetching when the bitstream starts with a 1 bit in the
// last byte in memory (which is the first one that we read).  This is used by
// zstd decompression.  Returns 1 on success, 0 on error.
static int elf_fetch_backward_init(const unsigned char **ppin,
                                   const unsigned char *pinend, uint64_t *pval,
                                   unsigned int *pbits) {
  const unsigned char *pin = NULL;
  unsigned int stream_start = 0;
  uint64_t val = 0;
  unsigned int bits = 0;

  pin = *ppin;
  stream_start = (unsigned int)*pin;
  if (unlikely(stream_start == 0)) {
    elf_uncompress_failed();
    return 0;
  }
  val = 0;
  bits = 0;

  // Align to a 32-bit boundary.
  while ((((uintptr_t)pin) & 3) != 0) {
    val <<= 8;
    val |= (uint64_t)*pin;
    bits += 8;
    --pin;
  }

  val <<= 8;
  val |= (uint64_t)*pin;
  bits += 8;

  *ppin = pin;
  *pval = val;
  *pbits = bits;
  if (!elf_fetch_bits_backward(ppin, pinend, pval, pbits)) {
    return 0;
  }

  *pbits -= __builtin_clz(stream_start) - (sizeof(unsigned int) - 1) * 8 + 1;

  if (!elf_fetch_bits_backward(ppin, pinend, pval, pbits)) {
    return 0;
  }

  return 1;
}

// Read a zstd Huffman table and build the decoding table in *TABLE, reading
// and updating *PPIN.  This sets *PTABLE_BITS to the number of bits of the
// table, such that the table length is 1 << *TABLE_BITS.  ZDEBUG_TABLE is
// scratch space; it must be enough for 512 uint16_t values + 256 32-bit values
// (2048 bytes).  Returns 1 on success, 0 on error.
static int elf_zstd_read_huff(const unsigned char **ppin,
                              const unsigned char *pinend,
                              uint16_t *zdebug_table, uint16_t *table,
                              int *ptable_bits) {
  const unsigned char *pin = NULL;
  unsigned char hdr = 0;
  unsigned char *weights = NULL;
  size_t count = 0;
  uint32_t *weight_mark = NULL;
  size_t i = 0;
  uint32_t weight_mask = 0;
  size_t table_bits = 0;

  pin = *ppin;
  if (unlikely(pin >= pinend)) {
    elf_uncompress_failed();
    return 0;
  }
  hdr = *pin;
  ++pin;

  weights = (unsigned char *)zdebug_table;

  if (hdr < 128) {
    // Table is compressed using FSE.
    elf_zstd_fse_entry *fse_table = NULL;
    int fse_table_bits = 0;
    uint16_t *scratch = NULL;
    const unsigned char *pfse = NULL;
    const unsigned char *pback = NULL;
    uint64_t val = 0;
    unsigned int bits = 0;
    unsigned int state1 = 0;
    unsigned int state2 = 0;

    // SCRATCH is used temporarily by elf_zstd_read_fse.  It overlaps
    // WEIGHTS.
    scratch = zdebug_table;
    fse_table = (elf_zstd_fse_entry *)(scratch + 512);
    fse_table_bits = 6;

    pfse = pin;
    if (!elf_zstd_read_fse(&pfse, pinend, scratch, 255, fse_table,
                           &fse_table_bits)) {
      return 0;
    }

    if (unlikely(pin + hdr > pinend)) {
      elf_uncompress_failed();
      return 0;
    }

    // We no longer need SCRATCH.  Start recording weights.  We need up to
    // 256 bytes of weights and 64 bytes of rank counts, so it won't overlap
    // FSE_TABLE.

    pback = pin + hdr - 1;

    if (!elf_fetch_backward_init(&pback, pfse, &val, &bits)) {
      return 0;
    }

    bits -= fse_table_bits;
    state1 = (val >> bits) & ((1U << fse_table_bits) - 1);
    bits -= fse_table_bits;
    state2 = (val >> bits) & ((1U << fse_table_bits) - 1);

    // There are two independent FSE streams, tracked by STATE1 and STATE2.
    // We decode them alternately.

    count = 0;
    while (1) {
      elf_zstd_fse_entry *pt = NULL;
      uint64_t v = 0;

      pt = &fse_table[state1];

      if (unlikely(pin < pinend) && bits < pt->bits) {
        if (unlikely(count >= 254)) {
          elf_uncompress_failed();
          return 0;
        }
        weights[count] = (unsigned char)pt->symbol;
        weights[count + 1] = (unsigned char)fse_table[state2].symbol;
        count += 2;
        break;
      }

      if (unlikely(pt->bits == 0)) {
        v = 0;
      } else {
        if (!elf_fetch_bits_backward(&pback, pfse, &val, &bits)) {
          return 0;
        }

        bits -= pt->bits;
        v = (val >> bits) & (((uint64_t)1 << pt->bits) - 1);
      }

      state1 = pt->base + v;

      if (unlikely(count >= 255)) {
        elf_uncompress_failed();
        return 0;
      }

      weights[count] = pt->symbol;
      ++count;

      pt = &fse_table[state2];

      if (unlikely(pin < pinend && bits < pt->bits)) {
        if (unlikely(count >= 254)) {
          elf_uncompress_failed();
          return 0;
        }
        weights[count] = (unsigned char)pt->symbol;
        weights[count + 1] = (unsigned char)fse_table[state1].symbol;
        count += 2;
        break;
      }

      if (unlikely(pt->bits == 0)) {
        v = 0;
      } else {
        if (!elf_fetch_bits_backward(&pback, pfse, &val, &bits)) {
          return 0;
        }

        bits -= pt->bits;
        v = (val >> bits) & (((uint64_t)1 << pt->bits) - 1);
      }

      state2 = pt->base + v;

      if (unlikely(count >= 255)) {
        elf_uncompress_failed();
        return 0;
      }

      weights[count] = pt->symbol;
      ++count;
    }

    pin += hdr;
  } else {
    // Table is not compressed.  Each weight is 4 bits.

    count = hdr - 127;
    if (unlikely(pin + ((count + 1) / 2) >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    for (i = 0; i < count; i += 2) {
      unsigned char b = *pin;
      ++pin;
      weights[i] = b >> 4;
      weights[i + 1] = b & 0xf;
    }
  }

  weight_mark = (uint32_t *)(weights + 256);
  memset(weight_mark, 0, 13 * sizeof(uint32_t));
  weight_mask = 0;
  for (i = 0; i < count; ++i) {
    unsigned char w = weights[i];
    if (unlikely(w > 12)) {
      elf_uncompress_failed();
      return 0;
    }
    ++weight_mark[w];
    if (w > 0) {
      weight_mask += 1U << (w - 1);
    }
  }
  if (unlikely(weight_mask == 0)) {
    elf_uncompress_failed();
    return 0;
  }

  table_bits = 32 - __builtin_clz(weight_mask);
  if (unlikely(table_bits > 11)) {
    elf_uncompress_failed();
    return 0;
  }

  // Work out the last weight value, which is omitted because the weights must
  // sum to a power of two.
  {
    uint32_t left = 0;
    uint32_t high_bit = 0;

    left = ((uint32_t)1 << table_bits) - weight_mask;
    if (left == 0) {
      elf_uncompress_failed();
      return 0;
    }
    high_bit = 31 - __builtin_clz(left);
    if (((uint32_t)1 << high_bit) != left) {
      elf_uncompress_failed();
      return 0;
    }

    if (unlikely(count >= 256)) {
      elf_uncompress_failed();
      return 0;
    }

    weights[count] = high_bit + 1;
    ++count;
    ++weight_mark[high_bit + 1];
  }

  if (weight_mark[1] < 2 || (weight_mark[1] & 1) != 0) {
    elf_uncompress_failed();
    return 0;
  }

  // Change WEIGHT_MARK from a count of weights to the index of the first
  // symbol for that weight.  We shift the indexes to also store how many we
  // have seen so far, below.
  {
    uint32_t next = 0;
    for (i = 0; i < table_bits; ++i) {
      uint32_t cur = next;
      next += weight_mark[i + 1] << i;
      weight_mark[i + 1] = cur;
    }
  }

  for (i = 0; i < count; ++i) {
    unsigned char weight = weights[i];
    uint32_t j = 0;

    if (weight == 0) {
      continue;
    }

    uint32_t length = 1U << (weight - 1);
    uint16_t tval = (i << 8) | (table_bits + 1 - weight);
    size_t start = weight_mark[weight];
    for (j = 0; j < length; ++j) {
      table[start + j] = tval;
    }
    weight_mark[weight] += length;
  }

  *ppin = pin;
  *ptable_bits = (int)table_bits;

  return 1;
}

// Read and decompress the literals and store them ending at POUTEND.  This
// works because we are going to use all the literals in the output, so they
// must fit into the output buffer.  HUFFMAN_TABLE, and PHUFFMAN_TABLE_BITS
// store the Huffman table across calls.  SCRATCH is used to read a Huffman
// table.  Store the start of the decompressed literals in *PPLIT.  Update
// *PPIN.  Return 1 on success, 0 on error.
static int elf_zstd_read_literals(const unsigned char **ppin,
                                  const unsigned char *pinend,
                                  unsigned char *pout, unsigned char *poutend,
                                  uint16_t *scratch, uint16_t *huffman_table,
                                  int *phuffman_table_bits,
                                  unsigned char **pplit) {
  const unsigned char *pin = NULL;
  unsigned char *plit = NULL;
  unsigned char hdr = 0;
  uint32_t regenerated_size = 0;
  uint32_t compressed_size = 0;
  int streams = 0;
  uint32_t total_streams_size = 0;
  unsigned int huffman_table_bits = 0;
  uint64_t huffman_mask = 0;

  pin = *ppin;
  if (unlikely(pin >= pinend)) {
    elf_uncompress_failed();
    return 0;
  }
  hdr = *pin;
  ++pin;

  if ((hdr & 3) == 0 || (hdr & 3) == 1) {
    int raw = 0;

    // Raw_Literals_Block or RLE_Literals_Block

    raw = (hdr & 3) == 0;

    switch ((hdr >> 2) & 3) {
    case 0:
    case 2:
      regenerated_size = hdr >> 3;
      break;
    case 1:
      if (unlikely(pin >= pinend)) {
        elf_uncompress_failed();
        return 0;
      }
      regenerated_size = (hdr >> 4) + ((uint32_t)(*pin) << 4);
      ++pin;
      break;
    case 3:
      if (unlikely(pin + 1 >= pinend)) {
        elf_uncompress_failed();
        return 0;
      }
      regenerated_size =
          ((hdr >> 4) + ((uint32_t)*pin << 4) + ((uint32_t)pin[1] << 12));
      pin += 2;
      break;
    default:
      elf_uncompress_failed();
      return 0;
    }

    if (unlikely((size_t)(poutend - pout) < regenerated_size)) {
      elf_uncompress_failed();
      return 0;
    }

    plit = poutend - regenerated_size;

    if (raw) {
      if (unlikely(pin + regenerated_size >= pinend)) {
        elf_uncompress_failed();
        return 0;
      }
      memcpy(plit, pin, regenerated_size);
      pin += regenerated_size;
    } else {
      if (pin >= pinend) {
        elf_uncompress_failed();
        return 0;
      }
      memset(plit, *pin, regenerated_size);
      ++pin;
    }

    *ppin = pin;
    *pplit = plit;

    return 1;
  }

  // Compressed_Literals_Block or Treeless_Literals_Block

  switch ((hdr >> 2) & 3) {
  case 0:
  case 1:
    if (unlikely(pin + 1 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    regenerated_size = (hdr >> 4) | ((uint32_t)(*pin & 0x3f) << 4);
    compressed_size = (uint32_t)*pin >> 6 | ((uint32_t)pin[1] << 2);
    pin += 2;
    streams = ((hdr >> 2) & 3) == 0 ? 1 : 4;
    break;
  case 2:
    if (unlikely(pin + 2 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    regenerated_size = (((uint32_t)hdr >> 4) | ((uint32_t)*pin << 4) |
                        (((uint32_t)pin[1] & 3) << 12));
    compressed_size = (((uint32_t)pin[1] >> 2) | ((uint32_t)pin[2] << 6));
    pin += 3;
    streams = 4;
    break;
  case 3:
    if (unlikely(pin + 3 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    regenerated_size = (((uint32_t)hdr >> 4) | ((uint32_t)*pin << 4) |
                        (((uint32_t)pin[1] & 0x3f) << 12));
    compressed_size = (((uint32_t)pin[1] >> 6) | ((uint32_t)pin[2] << 2) |
                       ((uint32_t)pin[3] << 10));
    pin += 4;
    streams = 4;
    break;
  default:
    elf_uncompress_failed();
    return 0;
  }

  if (unlikely(pin + compressed_size > pinend)) {
    elf_uncompress_failed();
    return 0;
  }

  pinend = pin + compressed_size;
  *ppin = pinend;

  if (unlikely((size_t)(poutend - pout) < regenerated_size)) {
    elf_uncompress_failed();
    return 0;
  }

  plit = poutend - regenerated_size;

  *pplit = plit;

  total_streams_size = compressed_size;
  if ((hdr & 3) == 2) {
    const unsigned char *ptable = NULL;

    // Compressed_Literals_Block.  Read Huffman tree.

    ptable = pin;
    if (!elf_zstd_read_huff(&ptable, pinend, scratch, huffman_table,
                            phuffman_table_bits)) {
      return 0;
    }

    if (unlikely(total_streams_size < (size_t)(ptable - pin))) {
      elf_uncompress_failed();
      return 0;
    }

    total_streams_size -= ptable - pin;
    pin = ptable;
  } else {
    // Treeless_Literals_Block.  Reuse previous Huffman tree.
    if (unlikely(*phuffman_table_bits == 0)) {
      elf_uncompress_failed();
      return 0;
    }
  }

  // Decompress COMPRESSED_SIZE bytes of data at PIN using the huffman table,
  // storing REGENERATED_SIZE bytes of decompressed data at PLIT.

  huffman_table_bits = (unsigned int)*phuffman_table_bits;
  huffman_mask = ((uint64_t)1 << huffman_table_bits) - 1;

  if (streams == 1) {
    const unsigned char *pback = NULL;
    const unsigned char *pbackend = NULL;
    uint64_t val = 0;
    unsigned int bits = 0;
    uint32_t i = 0;

    pback = pin + total_streams_size - 1;
    pbackend = pin;
    if (!elf_fetch_backward_init(&pback, pbackend, &val, &bits)) {
      return 0;
    }

    // This is one of the inner loops of the decompression algorithm, so we
    // put some effort into optimization.  We can't get more than 64 bytes
    // from a single call to elf_fetch_bits_backward, and we can't subtract
    // more than 11 bits at a time.

    if (regenerated_size >= 64) {
      unsigned char *plitstart = NULL;
      unsigned char *plitstop = NULL;

      plitstart = plit;
      plitstop = plit + regenerated_size - 64;
      while (plit < plitstop) {
        uint16_t t = 0;

        if (!elf_fetch_bits_backward(&pback, pbackend, &val, &bits)) {
          return 0;
        }

        if (bits < 16) {
          break;
        }

        while (bits >= 33) {
          t = huffman_table[(val >> (bits - huffman_table_bits)) &
                            huffman_mask];
          *plit = t >> 8;
          ++plit;
          bits -= t & 0xff;

          t = huffman_table[(val >> (bits - huffman_table_bits)) &
                            huffman_mask];
          *plit = t >> 8;
          ++plit;
          bits -= t & 0xff;

          t = huffman_table[(val >> (bits - huffman_table_bits)) &
                            huffman_mask];
          *plit = t >> 8;
          ++plit;
          bits -= t & 0xff;
        }

        while (bits > 11) {
          t = huffman_table[(val >> (bits - huffman_table_bits)) &
                            huffman_mask];
          *plit = t >> 8;
          ++plit;
          bits -= t & 0xff;
        }
      }

      regenerated_size -= plit - plitstart;
    }

    for (i = 0; i < regenerated_size; ++i) {
      uint16_t t = 0;

      if (!elf_fetch_bits_backward(&pback, pbackend, &val, &bits)) {
        return 0;
      }

      if (unlikely(bits < huffman_table_bits)) {
        t = huffman_table[(val << (huffman_table_bits - bits)) & huffman_mask];
        if (unlikely(bits < (t & 0xff))) {
          elf_uncompress_failed();
          return 0;
        }
      } else {
        t = huffman_table[(val >> (bits - huffman_table_bits)) & huffman_mask];
      }

      *plit = t >> 8;
      ++plit;
      bits -= t & 0xff;
    }

    return 1;
  }

  {
    uint32_t stream_size1 = 0;
    uint32_t stream_size2 = 0;
    uint32_t stream_size3 = 0;
    uint32_t stream_size4 = 0;
    uint32_t tot = 0;
    const unsigned char *pback1 = NULL;
    const unsigned char *pback2 = NULL;
    const unsigned char *pback3 = NULL;
    const unsigned char *pback4 = NULL;
    const unsigned char *pbackend1 = NULL;
    const unsigned char *pbackend2 = NULL;
    const unsigned char *pbackend3 = NULL;
    const unsigned char *pbackend4 = NULL;
    uint64_t val1 = 0;
    uint64_t val2 = 0;
    uint64_t val3 = 0;
    uint64_t val4 = 0;
    unsigned int bits1 = 0;
    unsigned int bits2 = 0;
    unsigned int bits3 = 0;
    unsigned int bits4 = 0;
    unsigned char *plit1 = NULL;
    unsigned char *plit2 = NULL;
    unsigned char *plit3 = NULL;
    unsigned char *plit4 = NULL;
    uint32_t regenerated_stream_size = 0;
    uint32_t regenerated_stream_size4 = 0;
    uint16_t t1 = 0;
    uint16_t t2 = 0;
    uint16_t t3 = 0;
    uint16_t t4 = 0;
    uint32_t i = 0;
    uint32_t limit = 0;

    // Read jump table.
    if (unlikely(pin + 5 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    stream_size1 = (uint32_t)*pin | ((uint32_t)pin[1] << 8);
    pin += 2;
    stream_size2 = (uint32_t)*pin | ((uint32_t)pin[1] << 8);
    pin += 2;
    stream_size3 = (uint32_t)*pin | ((uint32_t)pin[1] << 8);
    pin += 2;
    tot = stream_size1 + stream_size2 + stream_size3;
    if (unlikely(tot > total_streams_size - 6)) {
      elf_uncompress_failed();
      return 0;
    }
    stream_size4 = total_streams_size - 6 - tot;

    pback1 = pin + stream_size1 - 1;
    pbackend1 = pin;

    pback2 = pback1 + stream_size2;
    pbackend2 = pback1 + 1;

    pback3 = pback2 + stream_size3;
    pbackend3 = pback2 + 1;

    pback4 = pback3 + stream_size4;
    pbackend4 = pback3 + 1;

    if (!elf_fetch_backward_init(&pback1, pbackend1, &val1, &bits1)) {
      return 0;
    }
    if (!elf_fetch_backward_init(&pback2, pbackend2, &val2, &bits2)) {
      return 0;
    }
    if (!elf_fetch_backward_init(&pback3, pbackend3, &val3, &bits3)) {
      return 0;
    }
    if (!elf_fetch_backward_init(&pback4, pbackend4, &val4, &bits4)) {
      return 0;
    }

    regenerated_stream_size = (regenerated_size + 3) / 4;

    plit1 = plit;
    plit2 = plit1 + regenerated_stream_size;
    plit3 = plit2 + regenerated_stream_size;
    plit4 = plit3 + regenerated_stream_size;

    regenerated_stream_size4 = regenerated_size - regenerated_stream_size * 3;

    // We can't get more than 64 literal bytes from a single call to
    // elf_fetch_bits_backward.  The fourth stream can be up to 3 bytes less,
    // so use as the limit.

    limit = regenerated_stream_size4 <= 64 ? 0 : regenerated_stream_size4 - 64;
    i = 0;
    while (i < limit) {
      if (!elf_fetch_bits_backward(&pback1, pbackend1, &val1, &bits1)) {
        return 0;
      }
      if (!elf_fetch_bits_backward(&pback2, pbackend2, &val2, &bits2)) {
        return 0;
      }
      if (!elf_fetch_bits_backward(&pback3, pbackend3, &val3, &bits3)) {
        return 0;
      }
      if (!elf_fetch_bits_backward(&pback4, pbackend4, &val4, &bits4)) {
        return 0;
      }

      // We can't subtract more than 11 bits at a time.

      do {
        t1 = huffman_table[(val1 >> (bits1 - huffman_table_bits)) &
                           huffman_mask];
        t2 = huffman_table[(val2 >> (bits2 - huffman_table_bits)) &
                           huffman_mask];
        t3 = huffman_table[(val3 >> (bits3 - huffman_table_bits)) &
                           huffman_mask];
        t4 = huffman_table[(val4 >> (bits4 - huffman_table_bits)) &
                           huffman_mask];

        *plit1 = t1 >> 8;
        ++plit1;
        bits1 -= t1 & 0xff;

        *plit2 = t2 >> 8;
        ++plit2;
        bits2 -= t2 & 0xff;

        *plit3 = t3 >> 8;
        ++plit3;
        bits3 -= t3 & 0xff;

        *plit4 = t4 >> 8;
        ++plit4;
        bits4 -= t4 & 0xff;

        ++i;
      } while (bits1 > 11 && bits2 > 11 && bits3 > 11 && bits4 > 11);
    }

    while (i < regenerated_stream_size) {
      int use4 = i < regenerated_stream_size4;

      if (!elf_fetch_bits_backward(&pback1, pbackend1, &val1, &bits1)) {
        return 0;
      }

      if (!elf_fetch_bits_backward(&pback2, pbackend2, &val2, &bits2)) {
        return 0;
      }

      if (!elf_fetch_bits_backward(&pback3, pbackend3, &val3, &bits3)) {
        return 0;
      }

      if (use4) {
        if (!elf_fetch_bits_backward(&pback4, pbackend4, &val4, &bits4)) {
          return 0;
        }
      }

      if (unlikely(bits1 < huffman_table_bits)) {
        t1 = huffman_table[(val1 << (huffman_table_bits - bits1)) &
                           huffman_mask];
        if (unlikely(bits1 < (t1 & 0xff))) {
          elf_uncompress_failed();
          return 0;
        }
      } else {
        t1 = huffman_table[(val1 >> (bits1 - huffman_table_bits)) &
                           huffman_mask];
      }

      if (unlikely(bits2 < huffman_table_bits)) {
        t2 = huffman_table[(val2 << (huffman_table_bits - bits2)) &
                           huffman_mask];
        if (unlikely(bits2 < (t2 & 0xff))) {
          elf_uncompress_failed();
          return 0;
        }
      } else {
        t2 = huffman_table[(val2 >> (bits2 - huffman_table_bits)) &
                           huffman_mask];
      }

      if (unlikely(bits3 < huffman_table_bits)) {
        t3 = huffman_table[(val3 << (huffman_table_bits - bits3)) &
                           huffman_mask];
        if (unlikely(bits3 < (t3 & 0xff))) {
          elf_uncompress_failed();
          return 0;
        }
      } else {
        t3 = huffman_table[(val3 >> (bits3 - huffman_table_bits)) &
                           huffman_mask];
      }

      if (use4) {
        if (unlikely(bits4 < huffman_table_bits)) {
          t4 = huffman_table[(val4 << (huffman_table_bits - bits4)) &
                             huffman_mask];
          if (unlikely(bits4 < (t4 & 0xff))) {
            elf_uncompress_failed();
            return 0;
          }
        } else {
          t4 = huffman_table[(val4 >> (bits4 - huffman_table_bits)) &
                             huffman_mask];
        }

        *plit4 = t4 >> 8;
        ++plit4;
        bits4 -= t4 & 0xff;
      }

      *plit1 = t1 >> 8;
      ++plit1;
      bits1 -= t1 & 0xff;

      *plit2 = t2 >> 8;
      ++plit2;
      bits2 -= t2 & 0xff;

      *plit3 = t3 >> 8;
      ++plit3;
      bits3 -= t3 & 0xff;

      ++i;
    }
  }

  return 1;
}

// Unpack a sequence code compression mode.
static int elf_zstd_unpack_seq_decode(
    int mode, const unsigned char **ppin, const unsigned char *pinend,
    const elf_zstd_fse_baseline_entry *predef, int predef_bits,
    uint16_t *scratch, int maxidx, elf_zstd_fse_baseline_entry *table,
    int table_bits,
    int (*conv)(const elf_zstd_fse_entry *, int, elf_zstd_fse_baseline_entry *),
    elf_zstd_seq_decode *decode) {
  switch (mode) {
  case 0:
    decode->table = predef;
    decode->table_bits = predef_bits;
    break;

  case 1: {
    elf_zstd_fse_entry entry;

    if (unlikely(*ppin >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    entry.symbol = **ppin;
    ++*ppin;
    entry.bits = 0;
    entry.base = 0;
    decode->table_bits = 0;
    if (!conv(&entry, 0, table)) {
      return 0;
    }
  } break;

  case 2: {
    elf_zstd_fse_entry *fse_table = NULL;

    // We use the same space for the simple FSE table and the baseline
    // table.
    fse_table = (elf_zstd_fse_entry *)table;
    decode->table_bits = table_bits;
    if (!elf_zstd_read_fse(ppin, pinend, scratch, maxidx, fse_table,
                           &decode->table_bits)) {
      return 0;
    }
    if (!conv(fse_table, decode->table_bits, table)) {
      return 0;
    }
    decode->table = table;
  } break;

  case 3:
    if (unlikely(decode->table_bits == -1)) {
      elf_uncompress_failed();
      return 0;
    }
    break;

  default:
    elf_uncompress_failed();
    return 0;
  }

  return 1;
}

// Decompress a zstd stream from PIN/SIN to POUT/SOUT.  Code based on RFC 8878.
// Return 1 on success, 0 on error.
int elf_zstd_decompress(const unsigned char *pin, size_t sin,
                        unsigned char *zdebug_table, unsigned char *pout,
                        size_t sout) {
  const unsigned char *pinend = NULL;
  unsigned char *poutstart = NULL;
  unsigned char *poutend = NULL;
  elf_zstd_seq_decode literal_decode;
  elf_zstd_fse_baseline_entry *literal_fse_table = NULL;
  elf_zstd_seq_decode match_decode;
  elf_zstd_fse_baseline_entry *match_fse_table = NULL;
  elf_zstd_seq_decode offset_decode;
  elf_zstd_fse_baseline_entry *offset_fse_table = NULL;
  uint16_t *huffman_table = NULL;
  int huffman_table_bits = 0;
  uint32_t repeated_offset1 = 0;
  uint32_t repeated_offset2 = 0;
  uint32_t repeated_offset3 = 0;
  uint16_t *scratch = NULL;
  unsigned char hdr = 0;
  int has_checksum = 0;
  uint64_t content_size = 0;
  int last_block = 0;

  pinend = pin + sin;
  poutstart = pout;
  poutend = pout + sout;

  literal_decode.table = NULL;
  literal_decode.table_bits = -1;
  literal_fse_table =
      ((elf_zstd_fse_baseline_entry *)(zdebug_table +
                                       ZSTD_TABLE_LITERAL_FSE_OFFSET));

  match_decode.table = NULL;
  match_decode.table_bits = -1;
  match_fse_table =
      ((elf_zstd_fse_baseline_entry *)(zdebug_table +
                                       ZSTD_TABLE_MATCH_FSE_OFFSET));

  offset_decode.table = NULL;
  offset_decode.table_bits = -1;
  offset_fse_table =
      ((elf_zstd_fse_baseline_entry *)(zdebug_table +
                                       ZSTD_TABLE_OFFSET_FSE_OFFSET));
  huffman_table = ((uint16_t *)(zdebug_table + ZSTD_TABLE_HUFFMAN_OFFSET));
  huffman_table_bits = 0;
  scratch = ((uint16_t *)(zdebug_table + ZSTD_TABLE_WORK_OFFSET));

  repeated_offset1 = 1;
  repeated_offset2 = 4;
  repeated_offset3 = 8;

  if (unlikely(sin < 4)) {
    elf_uncompress_failed();
    return 0;
  }

  // These values are the zstd magic number.
  if (unlikely(pin[0] != 0x28 || pin[1] != 0xb5 || pin[2] != 0x2f ||
               pin[3] != 0xfd)) {
    elf_uncompress_failed();
    return 0;
  }

  pin += 4;

  if (unlikely(pin >= pinend)) {
    elf_uncompress_failed();
    return 0;
  }

  hdr = *pin++;

  // We expect a single frame.
  if (unlikely((hdr & (1 << 5)) == 0)) {
    elf_uncompress_failed();
    return 0;
  }
  // Reserved bit must be zero.
  if (unlikely((hdr & (1 << 3)) != 0)) {
    elf_uncompress_failed();
    return 0;
  }
  // We do not expect a dictionary.
  if (unlikely((hdr & 3) != 0)) {
    elf_uncompress_failed();
    return 0;
  }
  has_checksum = (hdr & (1 << 2)) != 0;
  switch (hdr >> 6) {
  case 0:
    if (unlikely(pin >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    content_size = (uint64_t)*pin++;
    break;
  case 1:
    if (unlikely(pin + 1 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    content_size = (((uint64_t)pin[0]) | (((uint64_t)pin[1]) << 8)) + 256;
    pin += 2;
    break;
  case 2:
    if (unlikely(pin + 3 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    content_size = ((uint64_t)pin[0] | (((uint64_t)pin[1]) << 8) |
                    (((uint64_t)pin[2]) << 16) | (((uint64_t)pin[3]) << 24));
    pin += 4;
    break;
  case 3:
    if (unlikely(pin + 7 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    content_size = ((uint64_t)pin[0] | (((uint64_t)pin[1]) << 8) |
                    (((uint64_t)pin[2]) << 16) | (((uint64_t)pin[3]) << 24) |
                    (((uint64_t)pin[4]) << 32) | (((uint64_t)pin[5]) << 40) |
                    (((uint64_t)pin[6]) << 48) | (((uint64_t)pin[7]) << 56));
    pin += 8;
    break;
  default:
    elf_uncompress_failed();
    return 0;
  }

  if (unlikely(content_size != (size_t)content_size ||
               (size_t)content_size != sout)) {
    elf_uncompress_failed();
    return 0;
  }

  last_block = 0;
  while (!last_block) {
    uint32_t block_hdr = 0;
    int block_type = 0;
    uint32_t block_size = 0;

    if (unlikely(pin + 2 >= pinend)) {
      elf_uncompress_failed();
      return 0;
    }
    block_hdr = ((uint32_t)pin[0] | (((uint32_t)pin[1]) << 8) |
                 (((uint32_t)pin[2]) << 16));
    pin += 3;

    last_block = block_hdr & 1;
    block_type = (block_hdr >> 1) & 3;
    block_size = block_hdr >> 3;

    switch (block_type) {
    case 0:
      // Raw_Block
      if (unlikely((size_t)block_size > (size_t)(pinend - pin))) {
        elf_uncompress_failed();
        return 0;
      }
      if (unlikely((size_t)block_size > (size_t)(poutend - pout))) {
        elf_uncompress_failed();
        return 0;
      }
      memcpy(pout, pin, block_size);
      pout += block_size;
      pin += block_size;
      break;

    case 1:
      // RLE_Block
      if (unlikely(pin >= pinend)) {
        elf_uncompress_failed();
        return 0;
      }
      if (unlikely((size_t)block_size > (size_t)(poutend - pout))) {
        elf_uncompress_failed();
        return 0;
      }
      memset(pout, *pin, block_size);
      pout += block_size;
      pin++;
      break;

    case 2: {
      const unsigned char *pblockend = NULL;
      unsigned char *plitstack = NULL;
      unsigned char *plit = NULL;
      uint32_t literal_count = 0;
      unsigned char seq_hdr = 0;
      size_t seq_count = 0;
      size_t seq = 0;
      const unsigned char *pback = NULL;
      uint64_t val = 0;
      unsigned int bits = 0;
      unsigned int literal_state = 0;
      unsigned int offset_state = 0;
      unsigned int match_state = 0;

      // Compressed_Block
      if (unlikely((size_t)block_size > (size_t)(pinend - pin))) {
        elf_uncompress_failed();
        return 0;
      }

      pblockend = pin + block_size;

      // Read the literals into the end of the output space, and leave
      // PLIT pointing at them.

      if (!elf_zstd_read_literals(&pin, pblockend, pout, poutend, scratch,
                                  huffman_table, &huffman_table_bits,
                                  &plitstack)) {
        return 0;
      }

      plit = plitstack;
      literal_count = poutend - plit;

      seq_hdr = *pin;
      pin++;
      if (seq_hdr < 128) {
        seq_count = seq_hdr;
      } else if (seq_hdr < 255) {
        if (unlikely(pin >= pinend)) {
          elf_uncompress_failed();
          return 0;
        }
        seq_count = ((seq_hdr - 128) << 8) + *pin;
        pin++;
      } else {
        if (unlikely(pin + 1 >= pinend)) {
          elf_uncompress_failed();
          return 0;
        }
        seq_count = *pin + (pin[1] << 8) + 0x7f00;
        pin += 2;
      }

      if (seq_count > 0) {
        int (*pfn)(const elf_zstd_fse_entry *, int,
                   elf_zstd_fse_baseline_entry *);

        if (unlikely(pin >= pinend)) {
          elf_uncompress_failed();
          return 0;
        }
        seq_hdr = *pin;
        ++pin;

        pfn = elf_zstd_make_literal_baseline_fse;
        if (!elf_zstd_unpack_seq_decode(
                (seq_hdr >> 6) & 3, &pin, pinend, &elf_zstd_lit_table[0], 6,
                scratch, 35, literal_fse_table, 9, pfn, &literal_decode)) {
          return 0;
        }

        pfn = elf_zstd_make_offset_baseline_fse;
        if (!elf_zstd_unpack_seq_decode(
                (seq_hdr >> 4) & 3, &pin, pinend, &elf_zstd_offset_table[0], 5,
                scratch, 31, offset_fse_table, 8, pfn, &offset_decode)) {
          return 0;
        }

        pfn = elf_zstd_make_match_baseline_fse;
        if (!elf_zstd_unpack_seq_decode(
                (seq_hdr >> 2) & 3, &pin, pinend, &elf_zstd_match_table[0], 6,
                scratch, 52, match_fse_table, 9, pfn, &match_decode)) {
          return 0;
        }
      }

      pback = pblockend - 1;
      if (!elf_fetch_backward_init(&pback, pin, &val, &bits)) {
        return 0;
      }

      bits -= literal_decode.table_bits;
      literal_state = ((val >> bits) & ((1U << literal_decode.table_bits) - 1));

      if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
        return 0;
      }
      bits -= offset_decode.table_bits;
      offset_state = ((val >> bits) & ((1U << offset_decode.table_bits) - 1));

      if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
        return 0;
      }
      bits -= match_decode.table_bits;
      match_state = ((val >> bits) & ((1U << match_decode.table_bits) - 1));

      seq = 0;
      while (1) {
        const elf_zstd_fse_baseline_entry *pt = NULL;
        uint32_t offset_basebits = 0;
        uint32_t offset_baseline = 0;
        uint32_t offset_bits = 0;
        uint32_t offset_base = 0;
        uint32_t offset = 0;
        uint32_t match_baseline = 0;
        uint32_t match_bits = 0;
        uint32_t match_base = 0;
        uint32_t match = 0;
        uint32_t literal_baseline = 0;
        uint32_t literal_bits = 0;
        uint32_t literal_base = 0;
        uint32_t literal = 0;
        uint32_t need = 0;
        uint32_t add = 0;

        pt = &offset_decode.table[offset_state];
        offset_basebits = pt->basebits;
        offset_baseline = pt->baseline;
        offset_bits = pt->bits;
        offset_base = pt->base;

        // This case can be more than 16 bits, which is all that
        // elf_fetch_bits_backward promises.
        need = offset_basebits;
        add = 0;
        if (unlikely(need > 16)) {
          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }
          bits -= 16;
          add = (val >> bits) & ((1U << 16) - 1);
          need -= 16;
          add <<= need;
        }
        if (need > 0) {
          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }
          bits -= need;
          add += (val >> bits) & ((1U << need) - 1);
        }

        offset = offset_baseline + add;

        pt = &match_decode.table[match_state];
        need = pt->basebits;
        match_baseline = pt->baseline;
        match_bits = pt->bits;
        match_base = pt->base;

        add = 0;
        if (need > 0) {
          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }
          bits -= need;
          add = (val >> bits) & ((1U << need) - 1);
        }

        match = match_baseline + add;

        pt = &literal_decode.table[literal_state];
        need = pt->basebits;
        literal_baseline = pt->baseline;
        literal_bits = pt->bits;
        literal_base = pt->base;

        add = 0;
        if (need > 0) {
          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }
          bits -= need;
          add = (val >> bits) & ((1U << need) - 1);
        }

        literal = literal_baseline + add;

        // See the comment in elf_zstd_make_offset_baseline_fse.
        if (offset_basebits > 1) {
          repeated_offset3 = repeated_offset2;
          repeated_offset2 = repeated_offset1;
          repeated_offset1 = offset;
        } else {
          if (unlikely(literal == 0)) {
            ++offset;
          }
          switch (offset) {
          case 1:
            offset = repeated_offset1;
            break;
          case 2:
            offset = repeated_offset2;
            repeated_offset2 = repeated_offset1;
            repeated_offset1 = offset;
            break;
          case 3:
            offset = repeated_offset3;
            repeated_offset3 = repeated_offset2;
            repeated_offset2 = repeated_offset1;
            repeated_offset1 = offset;
            break;
          case 4:
            offset = repeated_offset1 - 1;
            repeated_offset3 = repeated_offset2;
            repeated_offset2 = repeated_offset1;
            repeated_offset1 = offset;
            break;
          }
        }

        ++seq;
        if (seq < seq_count) {
          uint32_t v = 0;

          // Update the three states.

          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }

          need = literal_bits;
          bits -= need;
          v = (val >> bits) & (((uint32_t)1 << need) - 1);

          literal_state = literal_base + v;

          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }

          need = match_bits;
          bits -= need;
          v = (val >> bits) & (((uint32_t)1 << need) - 1);

          match_state = match_base + v;

          if (!elf_fetch_bits_backward(&pback, pin, &val, &bits)) {
            return 0;
          }

          need = offset_bits;
          bits -= need;
          v = (val >> bits) & (((uint32_t)1 << need) - 1);

          offset_state = offset_base + v;
        }

        // The next sequence is now in LITERAL, OFFSET, MATCH.

        // Copy LITERAL bytes from the literals.

        if (unlikely((size_t)(poutend - pout) < literal)) {
          elf_uncompress_failed();
          return 0;
        }

        if (unlikely(literal_count < literal)) {
          elf_uncompress_failed();
          return 0;
        }

        literal_count -= literal;

        // Often LITERAL is small, so handle small cases quickly.
        switch (literal) {
        case 8:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 7:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 6:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 5:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 4:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 3:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 2:
          *pout++ = *plit++;
          // FALLTHROUGH
        case 1:
          *pout++ = *plit++;
          break;

        case 0:
          break;

        default:
          if (unlikely((size_t)(plit - pout) < literal)) {
            uint32_t move = plit - pout;
            while (literal > move) {
              memcpy(pout, plit, move);
              pout += move;
              plit += move;
              literal -= move;
            }
          }

          memcpy(pout, plit, literal);
          pout += literal;
          plit += literal;
        }

        if (match > 0) {
          // Copy MATCH bytes from the decoded output at OFFSET.

          if (unlikely((size_t)(poutend - pout) < match)) {
            elf_uncompress_failed();
            return 0;
          }

          if (unlikely((size_t)(pout - poutstart) < offset)) {
            elf_uncompress_failed();
            return 0;
          }

          if (offset >= match) {
            memcpy(pout, pout - offset, match);
            pout += match;
          } else {
            while (match > 0) {
              uint32_t copy = match < offset ? match : offset;
              memcpy(pout, pout - offset, copy);
              match -= copy;
              pout += copy;
            }
          }
        }

        if (unlikely(seq >= seq_count)) {
          // Copy remaining literals.
          if (literal_count > 0 && plit != pout) {
            if (unlikely((size_t)(poutend - pout) < literal_count)) {
              elf_uncompress_failed();
              return 0;
            }

            if ((size_t)(plit - pout) < literal_count) {
              uint32_t move = plit - pout;
              while (literal_count > move) {
                memcpy(pout, plit, move);
                pout += move;
                plit += move;
                literal_count -= move;
              }
            }

            memcpy(pout, plit, literal_count);
          }

          pout += literal_count;

          break;
        }
      }

      pin = pblockend;
    } break;

    case 3:
    default:
      elf_uncompress_failed();
      return 0;
    }
  }

  if (has_checksum) {
    if (unlikely(pin + 4 > pinend)) {
      elf_uncompress_failed();
      return 0;
    }

    // We don't currently verify the checksum.  Currently running GNU ld with
    // --compress-debug-sections=zstd does not seem to generate a
    // checksum.

    pin += 4;
  }

  if (pin != pinend) {
    elf_uncompress_failed();
    return 0;
  }

  return 1;
}
