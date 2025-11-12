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

#include <stddef.h>

// Huffman code tables, like the rest of the zlib format, are defined by RFC
// 1951. We store a Huffman code table as a series of tables stored sequentially
// in memory. Each entry in a table is 16 bits. The first, main, table has 256
// entries. It is followed by a set of secondary tables of length 2 to 128
// entries. The maximum length of a code sequence in the deflate format is 15
// bits, so that is all we need. Each secondary table has an index, which is the
// offset of the table in the overall memory storage.
//
// The deflate format says that all codes of a given bit length are
// lexicographically consecutive. Perhaps we could have 130 values that require
// a 15-bit code, perhaps requiring three secondary tables of size 128.  I don't
// know if this is actually possible, but it suggests that the maximum size
// required for secondary tables is 3 * 128 + 3 * 64 ... == 768.  The zlib
// enough program reports 660 as the maximum.  We permit 768, since in addition
// to the 256 for the primary table, with two bytes per entry, and with the two
// tables we need, that gives us a page.
//
// A single table entry needs to store a value or (for the main table only) the
// index and size of a secondary table.  Values range from 0 to 285, inclusive.
// Secondary table indexes, per above, range from 0 to 510.  For a value we need
// to store the number of bits we need to determine that value (one value may
// appear multiple times in the table), which is 1 to 8.  For a secondary table
// we need to store the number of bits used to index into the table, which is 1
// to 7. And of course we need 1 bit to decide whether we have a value or a
// secondary table index.  So each entry needs 9 bits for value/table index, 3
// bits for size, 1 bit what it is.  For simplicity we use 16 bits per entry. */

// Number of entries we allocate to for one code table. We get a page for the
// two code tables we need.

#define ZLIB_HUFFMAN_TABLE_SIZE (1024)

// Bit masks and shifts for the values in the table.

#define ZLIB_HUFFMAN_VALUE_MASK 0x01ff
#define ZLIB_HUFFMAN_BITS_SHIFT 9
#define ZLIB_HUFFMAN_BITS_MASK 0x7
#define ZLIB_HUFFMAN_SECONDARY_SHIFT 12

// For working memory while inflating we need two code tables, we need an array
// of code lengths (max value 15, so we use unsigned char), and an array of
// unsigned shorts used while building a table. The latter two arrays must be
// large enough to hold the maximum number of code lengths, which RFC 1951
// defines as 286 + 30.

#define ZLIB_TABLE_SIZE                               \
  ((2 * ZLIB_HUFFMAN_TABLE_SIZE * sizeof(uint16_t)) + \
   ((286 + 30) * sizeof(uint16_t)) + ((286 + 30) * sizeof(unsigned char)))

#define ZLIB_TABLE_CODELEN_OFFSET                     \
  ((2 * ZLIB_HUFFMAN_TABLE_SIZE * sizeof(uint16_t)) + \
   ((286 + 30) * sizeof(uint16_t)))

#define ZLIB_TABLE_WORK_OFFSET (2 * ZLIB_HUFFMAN_TABLE_SIZE * sizeof(uint16_t))

TEN_UTILS_PRIVATE_API int elf_zlib_inflate_and_verify(const unsigned char *in,
                                                      size_t in_size,
                                                      uint16_t *z_debug_table,
                                                      unsigned char *out,
                                                      size_t out_size);
