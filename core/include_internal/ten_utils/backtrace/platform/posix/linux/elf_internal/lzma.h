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

#include "include_internal/ten_utils/backtrace/backtrace.h"

// Number of LZMA states.
#define LZMA_STATES (12)

// Number of LZMA position states.  The pb value of the property byte
// is the number of bits to include in these states, and the maximum
// value of pb is 4.
#define LZMA_POS_STATES (16)

// Number of LZMA distance states.  These are used match distances
// with a short match length: up to 4 bytes.
#define LZMA_DIST_STATES (4)

// Number of LZMA distance slots.  LZMA uses six bits to encode larger
// match lengths, so 1 << 6 possible probabilities.
#define LZMA_DIST_SLOTS (64)

// LZMA distances 0 to 3 are encoded directly, larger values use a
// probability model.
#define LZMA_DIST_MODEL_START (4)

// The LZMA probability model ends at 14.
#define LZMA_DIST_MODEL_END (14)

// LZMA distance slots for distances less than 127.
#define LZMA_FULL_DISTANCES (128)

// LZMA uses four alignment bits.
#define LZMA_ALIGN_SIZE (16)

// LZMA match length is encoded with 4, 5, or 10 bits, some of which
// are already known.
#define LZMA_LEN_LOW_SYMBOLS (8)
#define LZMA_LEN_MID_SYMBOLS (8)
#define LZMA_LEN_HIGH_SYMBOLS (256)

// LZMA literal encoding.
#define LZMA_LITERAL_CODERS_MAX (16)
#define LZMA_LITERAL_CODER_SIZE (0x300)

// LZMA is based on a large set of probabilities, each managed
// independently.  Each probability is an 11 bit number that we store
// in a uint16_t.  We use a single large array of probabilities.

// Lengths of entries in the LZMA probabilities array.  The names used
// here are copied from the Linux kernel implementation.

#define LZMA_PROB_IS_MATCH_LEN (LZMA_STATES * LZMA_POS_STATES)
#define LZMA_PROB_IS_REP_LEN LZMA_STATES
#define LZMA_PROB_IS_REP0_LEN LZMA_STATES
#define LZMA_PROB_IS_REP1_LEN LZMA_STATES
#define LZMA_PROB_IS_REP2_LEN LZMA_STATES
#define LZMA_PROB_IS_REP0_LONG_LEN (LZMA_STATES * LZMA_POS_STATES)
#define LZMA_PROB_DIST_SLOT_LEN (LZMA_DIST_STATES * LZMA_DIST_SLOTS)
#define LZMA_PROB_DIST_SPECIAL_LEN (LZMA_FULL_DISTANCES - LZMA_DIST_MODEL_END)
#define LZMA_PROB_DIST_ALIGN_LEN LZMA_ALIGN_SIZE
#define LZMA_PROB_MATCH_LEN_CHOICE_LEN 1
#define LZMA_PROB_MATCH_LEN_CHOICE2_LEN 1
#define LZMA_PROB_MATCH_LEN_LOW_LEN (LZMA_POS_STATES * LZMA_LEN_LOW_SYMBOLS)
#define LZMA_PROB_MATCH_LEN_MID_LEN (LZMA_POS_STATES * LZMA_LEN_MID_SYMBOLS)
#define LZMA_PROB_MATCH_LEN_HIGH_LEN LZMA_LEN_HIGH_SYMBOLS
#define LZMA_PROB_REP_LEN_CHOICE_LEN 1
#define LZMA_PROB_REP_LEN_CHOICE2_LEN 1
#define LZMA_PROB_REP_LEN_LOW_LEN (LZMA_POS_STATES * LZMA_LEN_LOW_SYMBOLS)
#define LZMA_PROB_REP_LEN_MID_LEN (LZMA_POS_STATES * LZMA_LEN_MID_SYMBOLS)
#define LZMA_PROB_REP_LEN_HIGH_LEN LZMA_LEN_HIGH_SYMBOLS
#define LZMA_PROB_LITERAL_LEN \
  (LZMA_LITERAL_CODERS_MAX * LZMA_LITERAL_CODER_SIZE)

// Offsets into the LZMA probabilities array.  This is mechanically
// generated from the above lengths.

#define LZMA_PROB_IS_MATCH_OFFSET 0
#define LZMA_PROB_IS_REP_OFFSET \
  (LZMA_PROB_IS_MATCH_OFFSET + LZMA_PROB_IS_MATCH_LEN)
#define LZMA_PROB_IS_REP0_OFFSET \
  (LZMA_PROB_IS_REP_OFFSET + LZMA_PROB_IS_REP_LEN)
#define LZMA_PROB_IS_REP1_OFFSET \
  (LZMA_PROB_IS_REP0_OFFSET + LZMA_PROB_IS_REP0_LEN)
#define LZMA_PROB_IS_REP2_OFFSET \
  (LZMA_PROB_IS_REP1_OFFSET + LZMA_PROB_IS_REP1_LEN)
#define LZMA_PROB_IS_REP0_LONG_OFFSET \
  (LZMA_PROB_IS_REP2_OFFSET + LZMA_PROB_IS_REP2_LEN)
#define LZMA_PROB_DIST_SLOT_OFFSET \
  (LZMA_PROB_IS_REP0_LONG_OFFSET + LZMA_PROB_IS_REP0_LONG_LEN)
#define LZMA_PROB_DIST_SPECIAL_OFFSET \
  (LZMA_PROB_DIST_SLOT_OFFSET + LZMA_PROB_DIST_SLOT_LEN)
#define LZMA_PROB_DIST_ALIGN_OFFSET \
  (LZMA_PROB_DIST_SPECIAL_OFFSET + LZMA_PROB_DIST_SPECIAL_LEN)
#define LZMA_PROB_MATCH_LEN_CHOICE_OFFSET \
  (LZMA_PROB_DIST_ALIGN_OFFSET + LZMA_PROB_DIST_ALIGN_LEN)
#define LZMA_PROB_MATCH_LEN_CHOICE2_OFFSET \
  (LZMA_PROB_MATCH_LEN_CHOICE_OFFSET + LZMA_PROB_MATCH_LEN_CHOICE_LEN)
#define LZMA_PROB_MATCH_LEN_LOW_OFFSET \
  (LZMA_PROB_MATCH_LEN_CHOICE2_OFFSET + LZMA_PROB_MATCH_LEN_CHOICE2_LEN)
#define LZMA_PROB_MATCH_LEN_MID_OFFSET \
  (LZMA_PROB_MATCH_LEN_LOW_OFFSET + LZMA_PROB_MATCH_LEN_LOW_LEN)
#define LZMA_PROB_MATCH_LEN_HIGH_OFFSET \
  (LZMA_PROB_MATCH_LEN_MID_OFFSET + LZMA_PROB_MATCH_LEN_MID_LEN)
#define LZMA_PROB_REP_LEN_CHOICE_OFFSET \
  (LZMA_PROB_MATCH_LEN_HIGH_OFFSET + LZMA_PROB_MATCH_LEN_HIGH_LEN)
#define LZMA_PROB_REP_LEN_CHOICE2_OFFSET \
  (LZMA_PROB_REP_LEN_CHOICE_OFFSET + LZMA_PROB_REP_LEN_CHOICE_LEN)
#define LZMA_PROB_REP_LEN_LOW_OFFSET \
  (LZMA_PROB_REP_LEN_CHOICE2_OFFSET + LZMA_PROB_REP_LEN_CHOICE2_LEN)
#define LZMA_PROB_REP_LEN_MID_OFFSET \
  (LZMA_PROB_REP_LEN_LOW_OFFSET + LZMA_PROB_REP_LEN_LOW_LEN)
#define LZMA_PROB_REP_LEN_HIGH_OFFSET \
  (LZMA_PROB_REP_LEN_MID_OFFSET + LZMA_PROB_REP_LEN_MID_LEN)
#define LZMA_PROB_LITERAL_OFFSET \
  (LZMA_PROB_REP_LEN_HIGH_OFFSET + LZMA_PROB_REP_LEN_HIGH_LEN)

#define LZMA_PROB_TOTAL_COUNT (LZMA_PROB_LITERAL_OFFSET + LZMA_PROB_LITERAL_LEN)

// Check that the number of LZMA probabilities is the same as the
// Linux kernel implementation.

#if LZMA_PROB_TOTAL_COUNT != 1846 + (1 << 4) * 0x300
#error Wrong number of LZMA probabilities
#endif

// Expressions for the offset in the LZMA probabilities array of a
// specific probability.

#define LZMA_IS_MATCH(state, pos) \
  (LZMA_PROB_IS_MATCH_OFFSET + (state) * LZMA_POS_STATES + (pos))
#define LZMA_IS_REP(state) (LZMA_PROB_IS_REP_OFFSET + (state))
#define LZMA_IS_REP0(state) (LZMA_PROB_IS_REP0_OFFSET + (state))
#define LZMA_IS_REP1(state) (LZMA_PROB_IS_REP1_OFFSET + (state))
#define LZMA_IS_REP2(state) (LZMA_PROB_IS_REP2_OFFSET + (state))
#define LZMA_IS_REP0_LONG(state, pos) \
  (LZMA_PROB_IS_REP0_LONG_OFFSET + (state) * LZMA_POS_STATES + (pos))
#define LZMA_DIST_SLOT(dist, slot) \
  (LZMA_PROB_DIST_SLOT_OFFSET + (dist) * LZMA_DIST_SLOTS + (slot))
#define LZMA_DIST_SPECIAL(dist) (LZMA_PROB_DIST_SPECIAL_OFFSET + (dist))
#define LZMA_DIST_ALIGN(dist) (LZMA_PROB_DIST_ALIGN_OFFSET + (dist))
#define LZMA_MATCH_LEN_CHOICE LZMA_PROB_MATCH_LEN_CHOICE_OFFSET
#define LZMA_MATCH_LEN_CHOICE2 LZMA_PROB_MATCH_LEN_CHOICE2_OFFSET
#define LZMA_MATCH_LEN_LOW(pos, sym) \
  (LZMA_PROB_MATCH_LEN_LOW_OFFSET + ((pos) * LZMA_LEN_LOW_SYMBOLS) + (sym))
#define LZMA_MATCH_LEN_MID(pos, sym) \
  (LZMA_PROB_MATCH_LEN_MID_OFFSET + ((pos) * LZMA_LEN_MID_SYMBOLS) + (sym))
#define LZMA_MATCH_LEN_HIGH(sym) (LZMA_PROB_MATCH_LEN_HIGH_OFFSET + (sym))
#define LZMA_REP_LEN_CHOICE LZMA_PROB_REP_LEN_CHOICE_OFFSET
#define LZMA_REP_LEN_CHOICE2 LZMA_PROB_REP_LEN_CHOICE2_OFFSET
#define LZMA_REP_LEN_LOW(pos, sym) \
  (LZMA_PROB_REP_LEN_LOW_OFFSET + ((pos) * LZMA_LEN_LOW_SYMBOLS) + (sym))
#define LZMA_REP_LEN_MID(pos, sym) \
  (LZMA_PROB_REP_LEN_MID_OFFSET + ((pos) * LZMA_LEN_MID_SYMBOLS) + (sym))
#define LZMA_REP_LEN_HIGH(sym) (LZMA_PROB_REP_LEN_HIGH_OFFSET + (sym))
#define LZMA_LITERAL(code, size) \
  (LZMA_PROB_LITERAL_OFFSET + ((code) * LZMA_LITERAL_CODER_SIZE) + (size))

TEN_UTILS_PRIVATE_API int elf_uncompress_lzma(
    ten_backtrace_t *self, const unsigned char *compressed,
    size_t compressed_size, ten_backtrace_on_error_func_t on_error, void *data,
    unsigned char **uncompressed, size_t *uncompressed_size);
