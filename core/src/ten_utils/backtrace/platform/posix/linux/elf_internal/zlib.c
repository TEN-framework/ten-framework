//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zlib.h"

#include <stdio.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zutils.h"

#if defined(BACKTRACE_GENERATE_FIXED_HUFFMAN_TABLE)

// Used by the main function that generates the fixed table to learn the table
// size.
static size_t final_next_secondary;

#endif

/**
 * @brief Build a Huffman code table from an array of lengths in @a codes of
 * length @a codes_len. The table is stored into @a *table. @a z_debug_table is
 * the same as for elf_zlib_inflate, used to find some work space.
 *
 * @return 1 on success, 0 on error.
 */
static int elf_zlib_inflate_table(unsigned char *codes, size_t codes_len,
                                  uint16_t *z_debug_table, uint16_t *table) {
  uint16_t count[16];
  uint16_t start[16];
  uint16_t prev[16];
  uint16_t firstcode[7];
  uint16_t *next = NULL;
  size_t i = 0;
  size_t j = 0;
  unsigned int code = 0;
  size_t next_secondary = 0;

  // Count the number of code of each length.  Set NEXT[val] to be the
  // next value after VAL with the same bit length.

  next =
      (uint16_t *)(((unsigned char *)z_debug_table) + ZLIB_TABLE_WORK_OFFSET);

  memset(&count[0], 0, 16 * sizeof(uint16_t));
  for (i = 0; i < codes_len; ++i) {
    if (unlikely(codes[i] >= 16)) {
      elf_uncompress_failed();
      return 0;
    }

    if (count[codes[i]] == 0) {
      start[codes[i]] = i;
      prev[codes[i]] = i;
    } else {
      next[prev[codes[i]]] = i;
      prev[codes[i]] = i;
    }

    ++count[codes[i]];
  }

  // For each length, fill in the table for the codes of that
  // length.

  memset(table, 0, ZLIB_HUFFMAN_TABLE_SIZE * sizeof(uint16_t));

  // Handle the values that do not require a secondary table.

  code = 0;
  for (j = 1; j <= 8; ++j) {
    unsigned int jcnt = 0;
    unsigned int val = 0;

    jcnt = count[j];
    if (jcnt == 0) {
      continue;
    }

    if (unlikely(jcnt > (1U << j))) {
      elf_uncompress_failed();
      return 0;
    }

    // There are JCNT values that have this length, the values
    // starting from START[j] continuing through NEXT[VAL].  Those
    // values are assigned consecutive values starting at CODE.

    val = start[j];
    for (i = 0; i < jcnt; ++i) {
      uint16_t tval = 0;
      size_t ind = 0;
      unsigned int incr = 0;

      // In the compressed bit stream, the value VAL is encoded as
      // J bits with the value C.

      if (unlikely((val & ~ZLIB_HUFFMAN_VALUE_MASK) != 0)) {
        elf_uncompress_failed();
        return 0;
      }

      tval = val | ((j - 1) << ZLIB_HUFFMAN_BITS_SHIFT);

      // The table lookup uses 8 bits.  If J is less than 8, we
      // don't know what the other bits will be.  We need to fill
      // in all possibilities in the table.  Since the Huffman
      // code is unambiguous, those entries can't be used for any
      // other code.

      for (ind = code; ind < 0x100; ind += 1 << j) {
        if (unlikely(table[ind] != 0)) {
          elf_uncompress_failed();
          return 0;
        }
        table[ind] = tval;
      }

      // Advance to the next value with this length.
      if (i + 1 < jcnt) {
        val = next[val];
      }

      // The Huffman codes are stored in the bitstream with the
      // most significant bit first, as is required to make them
      // unambiguous.  The effect is that when we read them from
      // the bitstream we see the bit sequence in reverse order:
      // the most significant bit of the Huffman code is the least
      // significant bit of the value we read from the bitstream.
      // That means that to make our table lookups work, we need
      // to reverse the bits of CODE.  Since reversing bits is
      // tedious and in general requires using a table, we instead
      // increment CODE in reverse order.  That is, if the number
      // of bits we are currently using, here named J, is 3, we
      // count as 000, 100, 010, 110, 001, 101, 011, 111, which is
      // to say the numbers from 0 to 7 but with the bits
      // reversed.  Going to more bits, aka incrementing J,
      // effectively just adds more zero bits as the beginning,
      // and as such does not change the numeric value of CODE.

      // To increment CODE of length J in reverse order, find the
      // most significant zero bit and set it to one while
      // clearing all higher bits.  In other words, add 1 modulo
      // 2^J, only reversed.

      incr = 1U << (j - 1);
      while ((code & incr) != 0) {
        incr >>= 1;
      }

      if (incr == 0) {
        code = 0;
      } else {
        code &= incr - 1;
        code += incr;
      }
    }
  }

  // Handle the values that require a secondary table.

  // Set FIRSTCODE, the number at which the codes start, for each
  // length.

  for (j = 9; j < 16; j++) {
    unsigned int jcnt = 0;
    unsigned int k = 0;

    jcnt = count[j];
    if (jcnt == 0) {
      continue;
    }

    // There are JCNT values that have this length, the values
    // starting from START[j].  Those values are assigned
    // consecutive values starting at CODE.

    firstcode[j - 9] = code;

    // Reverse add JCNT to CODE modulo 2^J.
    for (k = 0; k < j; ++k) {
      if ((jcnt & (1U << k)) != 0) {
        unsigned int m = 0;
        unsigned int bit = 0;

        bit = 1U << (j - k - 1);
        for (m = 0; m < j - k; ++m, bit >>= 1) {
          if ((code & bit) == 0) {
            code += bit;
            break;
          }
          code &= ~bit;
        }
        jcnt &= ~(1U << k);
      }
    }
    if (unlikely(jcnt != 0)) {
      elf_uncompress_failed();
      return 0;
    }
  }

  // For J from 9 to 15, inclusive, we store COUNT[J] consecutive
  // values starting at START[J] with consecutive codes starting at
  // FIRSTCODE[J - 9].  In the primary table we need to point to the
  // secondary table, and the secondary table will be indexed by J - 9
  // bits.  We count down from 15 so that we install the larger
  // secondary tables first, as the smaller ones may be embedded in
  // the larger ones.

  next_secondary = 0;  // Index of next secondary table (after primary).
  for (j = 15; j >= 9; j--) {
    unsigned int jcnt = 0;
    unsigned int val = 0;
    size_t primary = 0;         // Current primary index.
    size_t secondary = 0;       // Offset to current secondary table.
    size_t secondary_bits = 0;  // Bit size of current secondary table.

    jcnt = count[j];
    if (jcnt == 0) {
      continue;
    }

    val = start[j];
    code = firstcode[j - 9];
    primary = 0x100;
    secondary = 0;
    secondary_bits = 0;
    for (i = 0; i < jcnt; ++i) {
      uint16_t tval = 0;
      size_t ind = 0;
      unsigned int incr = 0;

      if ((code & 0xff) != primary) {
        uint16_t tprimary = 0;

        // Fill in a new primary table entry.

        primary = code & 0xff;

        tprimary = table[primary];
        if (tprimary == 0) {
          // Start a new secondary table.

          if (unlikely((next_secondary & ZLIB_HUFFMAN_VALUE_MASK) !=
                       next_secondary)) {
            elf_uncompress_failed();
            return 0;
          }

          secondary = next_secondary;
          secondary_bits = j - 8;
          next_secondary += 1 << secondary_bits;
          table[primary] = (secondary + ((j - 8) << ZLIB_HUFFMAN_BITS_SHIFT) +
                            (1U << ZLIB_HUFFMAN_SECONDARY_SHIFT));
        } else {
          // There is an existing entry.  It had better be a
          // secondary table with enough bits.
          if (unlikely((tprimary & (1U << ZLIB_HUFFMAN_SECONDARY_SHIFT)) ==
                       0)) {
            elf_uncompress_failed();
            return 0;
          }
          secondary = tprimary & ZLIB_HUFFMAN_VALUE_MASK;
          secondary_bits =
              ((tprimary >> ZLIB_HUFFMAN_BITS_SHIFT) & ZLIB_HUFFMAN_BITS_MASK);
          if (unlikely(secondary_bits < j - 8)) {
            elf_uncompress_failed();
            return 0;
          }
        }
      }

      // Fill in secondary table entries.

      tval = val | ((j - 8) << ZLIB_HUFFMAN_BITS_SHIFT);

      for (ind = code >> 8; ind < (1U << secondary_bits);
           ind += 1U << (j - 8)) {
        if (unlikely(table[secondary + 0x100 + ind] != 0)) {
          elf_uncompress_failed();
          return 0;
        }
        table[secondary + 0x100 + ind] = tval;
      }

      if (i + 1 < jcnt) {
        val = next[val];
      }

      incr = 1U << (j - 1);
      while ((code & incr) != 0) {
        incr >>= 1;
      }

      if (incr == 0) {
        code = 0;
      } else {
        code &= incr - 1;
        code += incr;
      }
    }
  }

#if defined(BACKTRACE_GENERATE_FIXED_HUFFMAN_TABLE)
  final_next_secondary = next_secondary;
#endif

  return 1;
}

// The fixed tables generated by the #ifdef'ed out main function above.

static const uint16_t elf_zlib_default_table[0x170] = {
    0xd00, 0xe50,  0xe10, 0xf18,  0xd10, 0xe70,  0xe30, 0x1230, 0xd08, 0xe60,
    0xe20, 0x1210, 0xe00, 0xe80,  0xe40, 0x1250, 0xd04, 0xe58,  0xe18, 0x1200,
    0xd14, 0xe78,  0xe38, 0x1240, 0xd0c, 0xe68,  0xe28, 0x1220, 0xe08, 0xe88,
    0xe48, 0x1260, 0xd02, 0xe54,  0xe14, 0xf1c,  0xd12, 0xe74,  0xe34, 0x1238,
    0xd0a, 0xe64,  0xe24, 0x1218, 0xe04, 0xe84,  0xe44, 0x1258, 0xd06, 0xe5c,
    0xe1c, 0x1208, 0xd16, 0xe7c,  0xe3c, 0x1248, 0xd0e, 0xe6c,  0xe2c, 0x1228,
    0xe0c, 0xe8c,  0xe4c, 0x1268, 0xd01, 0xe52,  0xe12, 0xf1a,  0xd11, 0xe72,
    0xe32, 0x1234, 0xd09, 0xe62,  0xe22, 0x1214, 0xe02, 0xe82,  0xe42, 0x1254,
    0xd05, 0xe5a,  0xe1a, 0x1204, 0xd15, 0xe7a,  0xe3a, 0x1244, 0xd0d, 0xe6a,
    0xe2a, 0x1224, 0xe0a, 0xe8a,  0xe4a, 0x1264, 0xd03, 0xe56,  0xe16, 0xf1e,
    0xd13, 0xe76,  0xe36, 0x123c, 0xd0b, 0xe66,  0xe26, 0x121c, 0xe06, 0xe86,
    0xe46, 0x125c, 0xd07, 0xe5e,  0xe1e, 0x120c, 0xd17, 0xe7e,  0xe3e, 0x124c,
    0xd0f, 0xe6e,  0xe2e, 0x122c, 0xe0e, 0xe8e,  0xe4e, 0x126c, 0xd00, 0xe51,
    0xe11, 0xf19,  0xd10, 0xe71,  0xe31, 0x1232, 0xd08, 0xe61,  0xe21, 0x1212,
    0xe01, 0xe81,  0xe41, 0x1252, 0xd04, 0xe59,  0xe19, 0x1202, 0xd14, 0xe79,
    0xe39, 0x1242, 0xd0c, 0xe69,  0xe29, 0x1222, 0xe09, 0xe89,  0xe49, 0x1262,
    0xd02, 0xe55,  0xe15, 0xf1d,  0xd12, 0xe75,  0xe35, 0x123a, 0xd0a, 0xe65,
    0xe25, 0x121a, 0xe05, 0xe85,  0xe45, 0x125a, 0xd06, 0xe5d,  0xe1d, 0x120a,
    0xd16, 0xe7d,  0xe3d, 0x124a, 0xd0e, 0xe6d,  0xe2d, 0x122a, 0xe0d, 0xe8d,
    0xe4d, 0x126a, 0xd01, 0xe53,  0xe13, 0xf1b,  0xd11, 0xe73,  0xe33, 0x1236,
    0xd09, 0xe63,  0xe23, 0x1216, 0xe03, 0xe83,  0xe43, 0x1256, 0xd05, 0xe5b,
    0xe1b, 0x1206, 0xd15, 0xe7b,  0xe3b, 0x1246, 0xd0d, 0xe6b,  0xe2b, 0x1226,
    0xe0b, 0xe8b,  0xe4b, 0x1266, 0xd03, 0xe57,  0xe17, 0xf1f,  0xd13, 0xe77,
    0xe37, 0x123e, 0xd0b, 0xe67,  0xe27, 0x121e, 0xe07, 0xe87,  0xe47, 0x125e,
    0xd07, 0xe5f,  0xe1f, 0x120e, 0xd17, 0xe7f,  0xe3f, 0x124e, 0xd0f, 0xe6f,
    0xe2f, 0x122e, 0xe0f, 0xe8f,  0xe4f, 0x126e, 0x290, 0x291,  0x292, 0x293,
    0x294, 0x295,  0x296, 0x297,  0x298, 0x299,  0x29a, 0x29b,  0x29c, 0x29d,
    0x29e, 0x29f,  0x2a0, 0x2a1,  0x2a2, 0x2a3,  0x2a4, 0x2a5,  0x2a6, 0x2a7,
    0x2a8, 0x2a9,  0x2aa, 0x2ab,  0x2ac, 0x2ad,  0x2ae, 0x2af,  0x2b0, 0x2b1,
    0x2b2, 0x2b3,  0x2b4, 0x2b5,  0x2b6, 0x2b7,  0x2b8, 0x2b9,  0x2ba, 0x2bb,
    0x2bc, 0x2bd,  0x2be, 0x2bf,  0x2c0, 0x2c1,  0x2c2, 0x2c3,  0x2c4, 0x2c5,
    0x2c6, 0x2c7,  0x2c8, 0x2c9,  0x2ca, 0x2cb,  0x2cc, 0x2cd,  0x2ce, 0x2cf,
    0x2d0, 0x2d1,  0x2d2, 0x2d3,  0x2d4, 0x2d5,  0x2d6, 0x2d7,  0x2d8, 0x2d9,
    0x2da, 0x2db,  0x2dc, 0x2dd,  0x2de, 0x2df,  0x2e0, 0x2e1,  0x2e2, 0x2e3,
    0x2e4, 0x2e5,  0x2e6, 0x2e7,  0x2e8, 0x2e9,  0x2ea, 0x2eb,  0x2ec, 0x2ed,
    0x2ee, 0x2ef,  0x2f0, 0x2f1,  0x2f2, 0x2f3,  0x2f4, 0x2f5,  0x2f6, 0x2f7,
    0x2f8, 0x2f9,  0x2fa, 0x2fb,  0x2fc, 0x2fd,  0x2fe, 0x2ff,
};

static const uint16_t elf_zlib_default_dist_table[0x100] = {
    0x800, 0x810, 0x808, 0x818, 0x804, 0x814, 0x80c, 0x81c, 0x802, 0x812, 0x80a,
    0x81a, 0x806, 0x816, 0x80e, 0x81e, 0x801, 0x811, 0x809, 0x819, 0x805, 0x815,
    0x80d, 0x81d, 0x803, 0x813, 0x80b, 0x81b, 0x807, 0x817, 0x80f, 0x81f, 0x800,
    0x810, 0x808, 0x818, 0x804, 0x814, 0x80c, 0x81c, 0x802, 0x812, 0x80a, 0x81a,
    0x806, 0x816, 0x80e, 0x81e, 0x801, 0x811, 0x809, 0x819, 0x805, 0x815, 0x80d,
    0x81d, 0x803, 0x813, 0x80b, 0x81b, 0x807, 0x817, 0x80f, 0x81f, 0x800, 0x810,
    0x808, 0x818, 0x804, 0x814, 0x80c, 0x81c, 0x802, 0x812, 0x80a, 0x81a, 0x806,
    0x816, 0x80e, 0x81e, 0x801, 0x811, 0x809, 0x819, 0x805, 0x815, 0x80d, 0x81d,
    0x803, 0x813, 0x80b, 0x81b, 0x807, 0x817, 0x80f, 0x81f, 0x800, 0x810, 0x808,
    0x818, 0x804, 0x814, 0x80c, 0x81c, 0x802, 0x812, 0x80a, 0x81a, 0x806, 0x816,
    0x80e, 0x81e, 0x801, 0x811, 0x809, 0x819, 0x805, 0x815, 0x80d, 0x81d, 0x803,
    0x813, 0x80b, 0x81b, 0x807, 0x817, 0x80f, 0x81f, 0x800, 0x810, 0x808, 0x818,
    0x804, 0x814, 0x80c, 0x81c, 0x802, 0x812, 0x80a, 0x81a, 0x806, 0x816, 0x80e,
    0x81e, 0x801, 0x811, 0x809, 0x819, 0x805, 0x815, 0x80d, 0x81d, 0x803, 0x813,
    0x80b, 0x81b, 0x807, 0x817, 0x80f, 0x81f, 0x800, 0x810, 0x808, 0x818, 0x804,
    0x814, 0x80c, 0x81c, 0x802, 0x812, 0x80a, 0x81a, 0x806, 0x816, 0x80e, 0x81e,
    0x801, 0x811, 0x809, 0x819, 0x805, 0x815, 0x80d, 0x81d, 0x803, 0x813, 0x80b,
    0x81b, 0x807, 0x817, 0x80f, 0x81f, 0x800, 0x810, 0x808, 0x818, 0x804, 0x814,
    0x80c, 0x81c, 0x802, 0x812, 0x80a, 0x81a, 0x806, 0x816, 0x80e, 0x81e, 0x801,
    0x811, 0x809, 0x819, 0x805, 0x815, 0x80d, 0x81d, 0x803, 0x813, 0x80b, 0x81b,
    0x807, 0x817, 0x80f, 0x81f, 0x800, 0x810, 0x808, 0x818, 0x804, 0x814, 0x80c,
    0x81c, 0x802, 0x812, 0x80a, 0x81a, 0x806, 0x816, 0x80e, 0x81e, 0x801, 0x811,
    0x809, 0x819, 0x805, 0x815, 0x80d, 0x81d, 0x803, 0x813, 0x80b, 0x81b, 0x807,
    0x817, 0x80f, 0x81f,
};

/**
 * @brief Inflate a zlib stream from @a in and @a in_size to @a out and @a
 * out_size.
 *
 * @return 1 on success, 0 on some error parsing the stream.
 */
static int elf_zlib_inflate(const unsigned char *in, size_t in_size,
                            uint16_t *z_debug_table, unsigned char *out,
                            size_t out_size) {
  // We can apparently see multiple zlib streams concatenated together, so keep
  // going as long as there is something to read. The last 4 bytes are the
  // checksum.

  unsigned char *orig_out = out;
  const unsigned char *in_end = in + in_size;
  unsigned char *out_end = out + out_size;

  while ((in_end - in) > 4) {
    // If we still have something other than the last 4-byte checksum to read.

    // Read the two byte zlib header.

    if (unlikely((in[0] & 0xf) != 8)) {  // 8 is zlib encoding.
      // Unknown compression method.
      elf_uncompress_failed();
      return 0;
    }

    if (unlikely((in[0] >> 4) > 7)) {
      // Window size too large.  Other than this check, we don't care about the
      // window size.
      elf_uncompress_failed();
      return 0;
    }

    if (unlikely((in[1] & 0x20) != 0)) {
      // Stream expects a predefined dictionary, but we have no dictionary.
      elf_uncompress_failed();
      return 0;
    }

    uint64_t val = (in[0] << 8) | in[1];
    if (unlikely(val % 31 != 0)) {
      // Header check failure.

      elf_uncompress_failed();
      return 0;
    }

    in += 2;

    // Align 'in' to a 32-bit (4-byte) boundary.

    val = 0;
    unsigned int bits = 0;

    while ((((uintptr_t)in) & 3) != 0) {
      val |= (uint64_t)*in << bits;
      bits += 8;
      ++in;
    }

    // Read blocks until one is marked last.

    int last = 0;

    while (!last) {
      if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
        return 0;
      }

      last = val & 1;
      unsigned int type = (val >> 1) & 3;
      val >>= 3;
      bits -= 3;

      if (unlikely(type == 3)) {
        // Invalid block type.

        elf_uncompress_failed();
        return 0;
      }

      if (type == 0) {
        uint16_t len = 0;

        // An uncompressed block.

        // If we've read ahead more than a byte, back up.
        while (bits >= 8) {
          --in;
          bits -= 8;
        }

        val = 0;
        bits = 0;

        if (unlikely((in_end - in) < 4)) {
          // Missing length.

          elf_uncompress_failed();
          return 0;
        }

        len = in[0] | (in[1] << 8);
        uint16_t lenc = in[2] | (in[3] << 8);
        in += 4;
        lenc = ~lenc;

        if (unlikely(len != lenc)) {
          // Corrupt data.

          elf_uncompress_failed();
          return 0;
        }

        if (unlikely(len > (unsigned int)(in_end - in) ||
                     len > (unsigned int)(out_end - out))) {
          // Not enough space in buffers.

          elf_uncompress_failed();
          return 0;
        }

        memcpy(out, in, len);

        out += len;
        in += len;

        // align in to 32-bit (4-byte) again.

        while ((((uintptr_t)in) & 3) != 0) {
          val |= (uint64_t)*in << bits;
          bits += 8;
          ++in;
        }

        // Go around to read the next block.
        continue;
      }

      const uint16_t *tlit = NULL;
      const uint16_t *tdist = NULL;

      if (type == 1) {
        tlit = elf_zlib_default_table;
        tdist = elf_zlib_default_dist_table;
      } else {
        unsigned int nlit = 0;
        unsigned int ndist = 0;
        unsigned int nclen = 0;
        unsigned char codebits[19];
        unsigned char *plenbase = NULL;
        unsigned char *plen = NULL;
        unsigned char *plenend = NULL;

        // Read a Huffman encoding table.  The various magic
        // numbers here are from RFC 1951.

        if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
          return 0;
        }

        nlit = (val & 0x1f) + 257;
        val >>= 5;
        ndist = (val & 0x1f) + 1;
        val >>= 5;
        nclen = (val & 0xf) + 4;
        val >>= 4;
        bits -= 14;
        if (unlikely(nlit > 286 || ndist > 30)) {
          // Values out of range.
          elf_uncompress_failed();
          return 0;
        }

        // Read and build the table used to compress the
        // literal, length, and distance codes.

        memset(&codebits[0], 0, 19);

        // There are always at least 4 elements in the
        // table.

        if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
          return 0;
        }

        codebits[16] = val & 7;
        codebits[17] = (val >> 3) & 7;
        codebits[18] = (val >> 6) & 7;
        codebits[0] = (val >> 9) & 7;
        val >>= 12;
        bits -= 12;

        if (nclen == 4) {
          goto codebitsdone;
        }

        codebits[8] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 5) {
          goto codebitsdone;
        }

        if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
          return 0;
        }

        codebits[7] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 6) {
          goto codebitsdone;
        }

        codebits[9] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 7) {
          goto codebitsdone;
        }

        codebits[6] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 8) {
          goto codebitsdone;
        }

        codebits[10] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 9) {
          goto codebitsdone;
        }

        codebits[5] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 10) {
          goto codebitsdone;
        }

        if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
          return 0;
        }

        codebits[11] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 11) {
          goto codebitsdone;
        }

        codebits[4] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 12) {
          goto codebitsdone;
        }

        codebits[12] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 13) {
          goto codebitsdone;
        }

        codebits[3] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 14) {
          goto codebitsdone;
        }

        codebits[13] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 15) {
          goto codebitsdone;
        }

        if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
          return 0;
        }

        codebits[2] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 16) {
          goto codebitsdone;
        }

        codebits[14] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 17) {
          goto codebitsdone;
        }

        codebits[1] = val & 7;
        val >>= 3;
        bits -= 3;

        if (nclen == 18) {
          goto codebitsdone;
        }

        codebits[15] = val & 7;
        val >>= 3;
        bits -= 3;

      codebitsdone:

        if (!elf_zlib_inflate_table(codebits, 19, z_debug_table,
                                    z_debug_table)) {
          return 0;
        }

        // Read the compressed bit lengths of the literal,
        // length, and distance codes.  We have allocated space
        // at the end of z_debug_table to hold them.

        plenbase =
            (((unsigned char *)z_debug_table) + ZLIB_TABLE_CODELEN_OFFSET);
        plen = plenbase;
        plenend = plen + nlit + ndist;
        while (plen < plenend) {
          uint16_t t = 0;
          unsigned int b = 0;
          uint16_t v = 0;

          if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
            return 0;
          }

          t = z_debug_table[val & 0xff];

          // The compression here uses bit lengths up to 7, so
          // a secondary table is never necessary.
          if (unlikely((t & (1U << ZLIB_HUFFMAN_SECONDARY_SHIFT)) != 0)) {
            elf_uncompress_failed();
            return 0;
          }

          b = (t >> ZLIB_HUFFMAN_BITS_SHIFT) & ZLIB_HUFFMAN_BITS_MASK;
          val >>= b + 1;
          bits -= b + 1;

          v = t & ZLIB_HUFFMAN_VALUE_MASK;
          if (v < 16) {
            *plen++ = v;
          } else if (v == 16) {
            unsigned int c = 0;
            unsigned int prev = 0;

            // Copy previous entry 3 to 6 times.

            if (unlikely(plen == plenbase)) {
              elf_uncompress_failed();
              return 0;
            }

            // We used up to 7 bits since the last
            // elf_fetch_bits, so we have at least 8 bits
            // available here.

            c = 3 + (val & 0x3);
            val >>= 2;
            bits -= 2;
            if (unlikely((unsigned int)(plenend - plen) < c)) {
              elf_uncompress_failed();
              return 0;
            }

            prev = plen[-1];
            switch (c) {
            case 6:
              *plen++ = prev;
              // FALLTHROUGH
            case 5:
              *plen++ = prev;
              // FALLTHROUGH
            case 4:
              *plen++ = prev;
            }
            *plen++ = prev;
            *plen++ = prev;
            *plen++ = prev;
          } else if (v == 17) {
            unsigned int c = 0;

            // Store zero 3 to 10 times.

            // We used up to 7 bits since the last
            // elf_fetch_bits, so we have at least 8 bits
            // available here.

            c = 3 + (val & 0x7);
            val >>= 3;
            bits -= 3;
            if (unlikely((unsigned int)(plenend - plen) < c)) {
              elf_uncompress_failed();
              return 0;
            }

            switch (c) {
            case 10:
              *plen++ = 0;
              // FALLTHROUGH
            case 9:
              *plen++ = 0;
              // FALLTHROUGH
            case 8:
              *plen++ = 0;
              // FALLTHROUGH
            case 7:
              *plen++ = 0;
              // FALLTHROUGH
            case 6:
              *plen++ = 0;
              // FALLTHROUGH
            case 5:
              *plen++ = 0;
              // FALLTHROUGH
            case 4:
              *plen++ = 0;
            }
            *plen++ = 0;
            *plen++ = 0;
            *plen++ = 0;
          } else if (v == 18) {
            unsigned int c = 0;

            // Store zero 11 to 138 times.

            // We used up to 7 bits since the last
            // elf_fetch_bits, so we have at least 8 bits
            // available here.

            c = 11 + (val & 0x7f);
            val >>= 7;
            bits -= 7;
            if (unlikely((unsigned int)(plenend - plen) < c)) {
              elf_uncompress_failed();
              return 0;
            }

            memset(plen, 0, c);
            plen += c;
          } else {
            elf_uncompress_failed();
            return 0;
          }
        }

        // Make sure that the stop code can appear.

        plen = plenbase;
        if (unlikely(plen[256] == 0)) {
          elf_uncompress_failed();
          return 0;
        }

        // Build the decompression tables.

        if (!elf_zlib_inflate_table(plen, nlit, z_debug_table, z_debug_table)) {
          return 0;
        }
        if (!elf_zlib_inflate_table(
                plen + nlit, ndist, z_debug_table,
                (z_debug_table + ZLIB_HUFFMAN_TABLE_SIZE))) {
          return 0;
        }
        tlit = z_debug_table;
        tdist = z_debug_table + ZLIB_HUFFMAN_TABLE_SIZE;
      }

      // Inflate values until the end of the block.  This is the
      // main loop of the inflation code.

      while (1) {
        uint16_t t = 0;
        unsigned int b = 0;
        uint16_t v = 0;
        unsigned int lit = 0;

        if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
          return 0;
        }

        t = tlit[val & 0xff];
        b = (t >> ZLIB_HUFFMAN_BITS_SHIFT) & ZLIB_HUFFMAN_BITS_MASK;
        v = t & ZLIB_HUFFMAN_VALUE_MASK;

        if ((t & (1U << ZLIB_HUFFMAN_SECONDARY_SHIFT)) == 0) {
          lit = v;
          val >>= b + 1;
          bits -= b + 1;
        } else {
          t = tlit[v + 0x100 + ((val >> 8) & ((1U << b) - 1))];
          b = (t >> ZLIB_HUFFMAN_BITS_SHIFT) & ZLIB_HUFFMAN_BITS_MASK;
          lit = t & ZLIB_HUFFMAN_VALUE_MASK;
          val >>= b + 8;
          bits -= b + 8;
        }

        if (lit < 256) {
          if (unlikely(out == out_end)) {
            elf_uncompress_failed();
            return 0;
          }

          *out++ = lit;

          // We will need to write the next byte soon.  We ask
          // for high temporal locality because we will write
          // to the whole cache line soon.
          __builtin_prefetch(out, 1, 3);
        } else if (lit == 256) {
          // The end of the block.
          break;
        } else {
          unsigned int dist = 0;
          unsigned int len = 0;

          // Convert lit into a length.

          if (lit < 265) {
            len = lit - 257 + 3;
          } else if (lit == 285) {
            len = 258;
          } else if (unlikely(lit > 285)) {
            elf_uncompress_failed();
            return 0;
          } else {
            unsigned int extra = 0;

            if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
              return 0;
            }

            // This is an expression for the table of length
            // codes in RFC 1951 3.2.5.
            lit -= 265;
            extra = (lit >> 2) + 1;
            len = (lit & 3) << extra;
            len += 11;
            len += ((1U << (extra - 1)) - 1) << 3;
            len += val & ((1U << extra) - 1);
            val >>= extra;
            bits -= extra;
          }

          if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
            return 0;
          }

          t = tdist[val & 0xff];
          b = (t >> ZLIB_HUFFMAN_BITS_SHIFT) & ZLIB_HUFFMAN_BITS_MASK;
          v = t & ZLIB_HUFFMAN_VALUE_MASK;

          if ((t & (1U << ZLIB_HUFFMAN_SECONDARY_SHIFT)) == 0) {
            dist = v;
            val >>= b + 1;
            bits -= b + 1;
          } else {
            t = tdist[v + 0x100 + ((val >> 8) & ((1U << b) - 1))];
            b = ((t >> ZLIB_HUFFMAN_BITS_SHIFT) & ZLIB_HUFFMAN_BITS_MASK);
            dist = t & ZLIB_HUFFMAN_VALUE_MASK;
            val >>= b + 8;
            bits -= b + 8;
          }

          // Convert dist to a distance.

          if (dist == 0) {
            // A distance of 1.  A common case, meaning
            // repeat the last character LEN times.

            if (unlikely(out == orig_out)) {
              elf_uncompress_failed();
              return 0;
            }

            if (unlikely((unsigned int)(out_end - out) < len)) {
              elf_uncompress_failed();
              return 0;
            }

            memset(out, out[-1], len);
            out += len;
          } else if (unlikely(dist > 29)) {
            elf_uncompress_failed();
            return 0;
          } else {
            if (dist < 4) {
              dist = dist + 1;
            } else {
              unsigned int extra = 0;

              if (!elf_fetch_bits(&in, in_end, &val, &bits)) {
                return 0;
              }

              // This is an expression for the table of
              // distance codes in RFC 1951 3.2.5.
              dist -= 4;
              extra = (dist >> 1) + 1;
              dist = (dist & 1) << extra;
              dist += 5;
              dist += ((1U << (extra - 1)) - 1) << 2;
              dist += val & ((1U << extra) - 1);
              val >>= extra;
              bits -= extra;
            }

            // Go back dist bytes, and copy len bytes from
            // there.

            if (unlikely((unsigned int)(out - orig_out) < dist)) {
              elf_uncompress_failed();
              return 0;
            }

            if (unlikely((unsigned int)(out_end - out) < len)) {
              elf_uncompress_failed();
              return 0;
            }

            if (dist >= len) {
              memcpy(out, out - dist, len);
              out += len;
            } else {
              while (len > 0) {
                unsigned int copy = len < dist ? len : dist;
                memcpy(out, out - dist, copy);
                len -= copy;
                out += copy;
              }
            }
          }
        }
      }
    }
  }

  // We should have filled the output buffer.
  if (unlikely(out != out_end)) {
    elf_uncompress_failed();
    return 0;
  }

  return 1;
}

// Verify the zlib checksum.  The checksum is in the 4 bytes at
// CHECKBYTES, and the uncompressed data is at UNCOMPRESSED /
// UNCOMPRESSED_SIZE.  Returns 1 on success, 0 on failure.
static int elf_zlib_verify_checksum(const unsigned char *checkbytes,
                                    const unsigned char *uncompressed,
                                    size_t uncompressed_size) {
  unsigned int i = 0;
  unsigned int cksum = 0;
  const unsigned char *p = NULL;
  uint32_t s1 = 0;
  uint32_t s2 = 0;
  size_t hsz = 0;

  cksum = 0;
  for (i = 0; i < 4; i++) {
    cksum = (cksum << 8) | checkbytes[i];
  }

  s1 = 1;
  s2 = 0;

  // Minimize modulo operations.

  p = uncompressed;
  hsz = uncompressed_size;
  while (hsz >= 5552) {
    for (i = 0; i < 5552; i += 16) {
      // Manually unroll loop 16 times.
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
      s1 = s1 + *p++;
      s2 = s2 + s1;
    }
    hsz -= 5552;
    s1 %= 65521;
    s2 %= 65521;
  }

  while (hsz >= 16) {
    // Manually unroll loop 16 times.
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;
    s1 = s1 + *p++;
    s2 = s2 + s1;

    hsz -= 16;
  }

  for (i = 0; i < hsz; ++i) {
    s1 = s1 + *p++;
    s2 = s2 + s1;
  }

  s1 %= 65521;
  s2 %= 65521;

  if (unlikely((s2 << 16) + s1 != cksum)) {
    elf_uncompress_failed();
    return 0;
  }

  return 1;
}

/**
 * @brief Inflate a zlib stream from @a in and @a in_size to @a out and @a
 * out_size, and verify the checksum.
 *
 * @return 1 on success, 0 on error.
 */
int elf_zlib_inflate_and_verify(const unsigned char *in, size_t in_size,
                                uint16_t *z_debug_table, unsigned char *out,
                                size_t out_size) {
  if (!elf_zlib_inflate(in, in_size, z_debug_table, out, out_size)) {
    return 0;
  }

  if (!elf_zlib_verify_checksum(in + in_size - 4, out, out_size)) {
    return 0;
  }

  return 1;
}
