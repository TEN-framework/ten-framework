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

#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/lzma.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/crc32.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zutils.h"

// Read an LZMA varint from BUF, reading and updating *POFFSET,
// setting *VAL.  Returns 0 on error, 1 on success.
static int elf_lzma_varint(const unsigned char *compressed,
                           size_t compressed_size, size_t *poffset,
                           uint64_t *val) {
  size_t off = 0;
  int i = 0;
  uint64_t v = 0;
  unsigned char b = 0;

  off = *poffset;
  i = 0;
  v = 0;
  while (1) {
    if (unlikely(off >= compressed_size)) {
      elf_uncompress_failed();
      return 0;
    }
    b = compressed[off];
    v |= (b & 0x7f) << (i * 7);
    ++off;
    if ((b & 0x80) == 0) {
      *poffset = off;
      *val = v;
      return 1;
    }
    ++i;
    if (unlikely(i >= 9)) {
      elf_uncompress_failed();
      return 0;
    }
  }
}

// Normalize the LZMA range decoder, pulling in an extra input byte if
// needed.
static void elf_lzma_range_normalize(const unsigned char *compressed,
                                     size_t compressed_size, size_t *poffset,
                                     uint32_t *prange, uint32_t *pcode) {
  if (*prange < (1U << 24)) {
    if (unlikely(*poffset >= compressed_size)) {
      // We assume this will be caught elsewhere.
      elf_uncompress_failed();
      return;
    }
    *prange <<= 8;
    *pcode <<= 8;
    *pcode += compressed[*poffset];
    ++*poffset;
  }
}

// Read and return a single bit from the LZMA stream, reading and
// updating *PROB.  Each bit comes from the range coder.
static int elf_lzma_bit(const unsigned char *compressed, size_t compressed_size,
                        uint16_t *prob, size_t *poffset, uint32_t *prange,
                        uint32_t *pcode) {
  uint32_t bound = 0;

  elf_lzma_range_normalize(compressed, compressed_size, poffset, prange, pcode);
  bound = (*prange >> 11) * (uint32_t)*prob;
  if (*pcode < bound) {
    *prange = bound;
    *prob += ((1U << 11) - *prob) >> 5;
    return 0;
  } else {
    *prange -= bound;
    *pcode -= bound;
    *prob -= *prob >> 5;
    return 1;
  }
}

// Read an integer of size BITS from the LZMA stream, most significant
// bit first.  The bits are predicted using PROBS.
static uint32_t elf_lzma_integer(const unsigned char *compressed,
                                 size_t compressed_size, uint16_t *probs,
                                 uint32_t bits, size_t *poffset,
                                 uint32_t *prange, uint32_t *pcode) {
  uint32_t sym = 1;
  uint32_t i = 0;

  for (i = 0; i < bits; i++) {
    int bit = elf_lzma_bit(compressed, compressed_size, probs + sym, poffset,
                           prange, pcode);
    sym <<= 1;
    sym += bit;
  }
  return sym - (1 << bits);
}

// Read an integer of size BITS from the LZMA stream, least
// significant bit first.  The bits are predicted using PROBS.
static uint32_t elf_lzma_reverse_integer(const unsigned char *compressed,
                                         size_t compressed_size,
                                         uint16_t *probs, uint32_t bits,
                                         size_t *poffset, uint32_t *prange,
                                         uint32_t *pcode) {
  uint32_t sym = 1;
  uint32_t val = 0;
  uint32_t i = 0;

  for (i = 0; i < bits; i++) {
    int bit = elf_lzma_bit(compressed, compressed_size, probs + sym, poffset,
                           prange, pcode);
    sym <<= 1;
    sym += bit;
    val += bit << i;
  }
  return val;
}

// Read a length from the LZMA stream.  IS_REP picks either LZMA_MATCH
// or LZMA_REP probabilities.
static uint32_t elf_lzma_len(const unsigned char *compressed,
                             size_t compressed_size, uint16_t *probs,
                             int is_rep, unsigned int pos_state,
                             size_t *poffset, uint32_t *prange,
                             uint32_t *pcode) {
  uint16_t *probs_choice = NULL;
  uint16_t *probs_sym = NULL;
  uint32_t bits = 0;
  uint32_t len = 0;

  probs_choice = probs + (is_rep ? LZMA_REP_LEN_CHOICE : LZMA_MATCH_LEN_CHOICE);
  if (elf_lzma_bit(compressed, compressed_size, probs_choice, poffset, prange,
                   pcode)) {
    probs_choice =
        probs + (is_rep ? LZMA_REP_LEN_CHOICE2 : LZMA_MATCH_LEN_CHOICE2);
    if (elf_lzma_bit(compressed, compressed_size, probs_choice, poffset, prange,
                     pcode)) {
      probs_sym =
          probs + (is_rep ? LZMA_REP_LEN_HIGH(0) : LZMA_MATCH_LEN_HIGH(0));
      bits = 8;
      len = 2 + 8 + 8;
    } else {
      probs_sym = probs + (is_rep ? LZMA_REP_LEN_MID(pos_state, 0)
                                  : LZMA_MATCH_LEN_MID(pos_state, 0));
      bits = 3;
      len = 2 + 8;
    }
  } else {
    probs_sym = probs + (is_rep ? LZMA_REP_LEN_LOW(pos_state, 0)
                                : LZMA_MATCH_LEN_LOW(pos_state, 0));
    bits = 3;
    len = 2;
  }

  len += elf_lzma_integer(compressed, compressed_size, probs_sym, bits, poffset,
                          prange, pcode);
  return len;
}

// Uncompress one LZMA block from a minidebug file.  The compressed
// data is at COMPRESSED + *POFFSET.  Update *POFFSET.  Store the data
// into the memory at UNCOMPRESSED, size UNCOMPRESSED_SIZE.  CHECK is
// the stream flag from the xz header.  Return 1 on successful
// decompression.
static int elf_uncompress_lzma_block(const unsigned char *compressed,
                                     size_t compressed_size,
                                     unsigned char check, uint16_t *probs,
                                     unsigned char *uncompressed,
                                     size_t uncompressed_size,
                                     size_t *poffset) {
  size_t off = 0;
  size_t block_header_offset = 0;
  size_t block_header_size = 0;
  unsigned char block_flags = 0;
  uint64_t header_compressed_size = 0;
  uint64_t header_uncompressed_size = 0;
  unsigned char lzma2_properties = 0;
  uint32_t computed_crc = 0;
  uint32_t stream_crc = 0;
  size_t uncompressed_offset = 0;
  size_t dict_start_offset = 0;
  unsigned int lc = 0;
  unsigned int lp = 0;
  unsigned int pb = 0;
  uint32_t range = 0;
  uint32_t code = 0;
  uint32_t lstate = 0;
  uint32_t dist[4] = {0};

  off = *poffset;
  block_header_offset = off;

  // Block header size is a single byte.
  if (unlikely(off >= compressed_size)) {
    elf_uncompress_failed();
    return 0;
  }
  block_header_size = (compressed[off] + 1) * 4;
  if (unlikely(off + block_header_size > compressed_size)) {
    elf_uncompress_failed();
    return 0;
  }

  // Block flags.
  block_flags = compressed[off + 1];
  if (unlikely((block_flags & 0x3c) != 0)) {
    elf_uncompress_failed();
    return 0;
  }

  off += 2;

  // Optional compressed size.
  header_compressed_size = 0;
  if ((block_flags & 0x40) != 0) {
    *poffset = off;
    if (!elf_lzma_varint(compressed, compressed_size, poffset,
                         &header_compressed_size)) {
      return 0;
    }
    off = *poffset;
  }

  // Optional uncompressed size.
  header_uncompressed_size = 0;
  if ((block_flags & 0x80) != 0) {
    *poffset = off;
    if (!elf_lzma_varint(compressed, compressed_size, poffset,
                         &header_uncompressed_size)) {
      return 0;
    }
    off = *poffset;
  }

  // The recipe for creating a minidebug file is to run the xz program
  // with no arguments, so we expect exactly one filter: lzma2.

  if (unlikely((block_flags & 0x3) != 0)) {
    elf_uncompress_failed();
    return 0;
  }

  if (unlikely(off + 2 >= block_header_offset + block_header_size)) {
    elf_uncompress_failed();
    return 0;
  }

  // The filter ID for LZMA2 is 0x21.
  if (unlikely(compressed[off] != 0x21)) {
    elf_uncompress_failed();
    return 0;
  }
  ++off;

  // The size of the filter properties for LZMA2 is 1.
  if (unlikely(compressed[off] != 1)) {
    elf_uncompress_failed();
    return 0;
  }
  ++off;

  lzma2_properties = compressed[off];
  ++off;

  if (unlikely(lzma2_properties > 40)) {
    elf_uncompress_failed();
    return 0;
  }

  // The properties describe the dictionary size, but we don't care
  // what that is.

  // Block header padding.
  if (unlikely(off + 4 > compressed_size)) {
    elf_uncompress_failed();
    return 0;
  }

  off = (off + 3) & ~(size_t)3;

  if (unlikely(off + 4 > compressed_size)) {
    elf_uncompress_failed();
    return 0;
  }

  // Block header CRC.
  computed_crc =
      elf_crc32(0, compressed + block_header_offset, block_header_size - 4);
  stream_crc = (compressed[off] | (compressed[off + 1] << 8) |
                (compressed[off + 2] << 16) | (compressed[off + 3] << 24));
  if (unlikely(computed_crc != stream_crc)) {
    elf_uncompress_failed();
    return 0;
  }
  off += 4;

  // Read a sequence of LZMA2 packets.

  uncompressed_offset = 0;
  dict_start_offset = 0;
  lc = 0;
  lp = 0;
  pb = 0;
  lstate = 0;
  while (off < compressed_size) {
    unsigned char control = 0;

    range = 0xffffffff;
    code = 0;

    control = compressed[off];
    ++off;
    if (unlikely(control == 0)) {
      // End of packets.
      break;
    }

    if (control == 1 || control >= 0xe0) {
      // Reset dictionary to empty.
      dict_start_offset = uncompressed_offset;
    }

    if (control < 0x80) {
      size_t chunk_size = 0;

      // The only valid values here are 1 or 2.  A 1 means to
      // reset the dictionary (done above).  Then we see an
      // uncompressed chunk.

      if (unlikely(control > 2)) {
        elf_uncompress_failed();
        return 0;
      }

      // An uncompressed chunk is a two byte size followed by
      // data.

      if (unlikely(off + 2 > compressed_size)) {
        elf_uncompress_failed();
        return 0;
      }

      chunk_size = compressed[off] << 8;
      chunk_size += compressed[off + 1];
      ++chunk_size;

      off += 2;

      if (unlikely(off + chunk_size > compressed_size)) {
        elf_uncompress_failed();
        return 0;
      }
      if (unlikely(uncompressed_offset + chunk_size > uncompressed_size)) {
        elf_uncompress_failed();
        return 0;
      }

      memcpy(uncompressed + uncompressed_offset, compressed + off, chunk_size);
      uncompressed_offset += chunk_size;
      off += chunk_size;
    } else {
      size_t uncompressed_chunk_start = 0;
      size_t uncompressed_chunk_size = 0;
      size_t compressed_chunk_size = 0;
      size_t limit = 0;

      // An LZMA chunk.  This starts with an uncompressed size and
      // a compressed size.

      if (unlikely(off + 4 >= compressed_size)) {
        elf_uncompress_failed();
        return 0;
      }

      uncompressed_chunk_start = uncompressed_offset;

      uncompressed_chunk_size = (control & 0x1f) << 16;
      uncompressed_chunk_size += compressed[off] << 8;
      uncompressed_chunk_size += compressed[off + 1];
      ++uncompressed_chunk_size;

      compressed_chunk_size = compressed[off + 2] << 8;
      compressed_chunk_size += compressed[off + 3];
      ++compressed_chunk_size;

      off += 4;

      // Bit 7 (0x80) is set.
      // Bits 6 and 5 (0x40 and 0x20) are as follows:
      // 0: don't reset anything
      // 1: reset state
      // 2: reset state, read properties
      // 3: reset state, read properties, reset dictionary (done above)

      if (control >= 0xc0) {
        unsigned char props = 0;

        // Bit 6 is set, read properties.

        if (unlikely(off >= compressed_size)) {
          elf_uncompress_failed();
          return 0;
        }
        props = compressed[off];
        ++off;
        if (unlikely(props > (4 * 5 + 4) * 9 + 8)) {
          elf_uncompress_failed();
          return 0;
        }
        pb = 0;
        while (props >= 9 * 5) {
          props -= 9 * 5;
          ++pb;
        }
        lp = 0;
        while (props > 9) {
          props -= 9;
          ++lp;
        }
        lc = props;
        if (unlikely(lc + lp > 4)) {
          elf_uncompress_failed();
          return 0;
        }
      }

      if (control >= 0xa0) {
        size_t i = 0;

        // Bit 5 or 6 is set, reset LZMA state.

        lstate = 0;
        memset(&dist, 0, sizeof dist);
        for (i = 0; i < LZMA_PROB_TOTAL_COUNT; i++) {
          probs[i] = 1 << 10;
        }
        range = 0xffffffff;
        code = 0;
      }

      // Read the range code.

      if (unlikely(off + 5 > compressed_size)) {
        elf_uncompress_failed();
        return 0;
      }

      // The byte at compressed[off] is ignored for some
      // reason.

      code = ((compressed[off + 1] << 24) + (compressed[off + 2] << 16) +
              (compressed[off + 3] << 8) + compressed[off + 4]);
      off += 5;

      // This is the main LZMA decode loop.

      limit = off + compressed_chunk_size;
      *poffset = off;
      while (*poffset < limit) {
        unsigned int pos_state = 0;

        if (unlikely(uncompressed_offset ==
                     (uncompressed_chunk_start + uncompressed_chunk_size))) {
          // We've decompressed all the expected bytes.
          break;
        }

        pos_state =
            ((uncompressed_offset - dict_start_offset) & ((1 << pb) - 1));

        if (elf_lzma_bit(compressed, compressed_size,
                         probs + LZMA_IS_MATCH(lstate, pos_state), poffset,
                         &range, &code)) {
          uint32_t len = 0;

          if (elf_lzma_bit(compressed, compressed_size,
                           probs + LZMA_IS_REP(lstate), poffset, &range,
                           &code)) {
            int short_rep = 0;
            uint32_t next_dist = 0;

            // Repeated match.

            short_rep = 0;
            if (elf_lzma_bit(compressed, compressed_size,
                             probs + LZMA_IS_REP0(lstate), poffset, &range,
                             &code)) {
              if (elf_lzma_bit(compressed, compressed_size,
                               probs + LZMA_IS_REP1(lstate), poffset, &range,
                               &code)) {
                if (elf_lzma_bit(compressed, compressed_size,
                                 probs + LZMA_IS_REP2(lstate), poffset, &range,
                                 &code)) {
                  next_dist = dist[3];
                  dist[3] = dist[2];
                } else {
                  next_dist = dist[2];
                }
                dist[2] = dist[1];
              } else {
                next_dist = dist[1];
              }

              dist[1] = dist[0];
              dist[0] = next_dist;
            } else {
              if (!elf_lzma_bit(compressed, compressed_size,
                                (probs + LZMA_IS_REP0_LONG(lstate, pos_state)),
                                poffset, &range, &code)) {
                short_rep = 1;
              }
            }

            if (lstate < 7) {
              lstate = short_rep ? 9 : 8;
            } else {
              lstate = 11;
            }

            if (short_rep) {
              len = 1;
            } else {
              len = elf_lzma_len(compressed, compressed_size, probs, 1,
                                 pos_state, poffset, &range, &code);
            }
          } else {
            uint32_t dist_state = 0;
            uint32_t dist_slot = 0;
            uint16_t *probs_dist = NULL;

            // Match.

            if (lstate < 7) {
              lstate = 7;
            } else {
              lstate = 10;
            }
            dist[3] = dist[2];
            dist[2] = dist[1];
            dist[1] = dist[0];
            len = elf_lzma_len(compressed, compressed_size, probs, 0, pos_state,
                               poffset, &range, &code);

            if (len < 4 + 2) {
              dist_state = len - 2;
            } else {
              dist_state = 3;
            }
            probs_dist = probs + LZMA_DIST_SLOT(dist_state, 0);
            dist_slot = elf_lzma_integer(compressed, compressed_size,
                                         probs_dist, 6, poffset, &range, &code);
            if (dist_slot < LZMA_DIST_MODEL_START) {
              dist[0] = dist_slot;
            } else {
              uint32_t limit = (dist_slot >> 1) - 1;
              dist[0] = 2 + (dist_slot & 1);
              if (dist_slot < LZMA_DIST_MODEL_END) {
                dist[0] <<= limit;
                probs_dist =
                    (probs + LZMA_DIST_SPECIAL(dist[0] - dist_slot - 1));
                dist[0] += elf_lzma_reverse_integer(compressed, compressed_size,
                                                    probs_dist, limit, poffset,
                                                    &range, &code);
              } else {
                uint32_t dist0 = 0;
                uint32_t i = 0;

                dist0 = dist[0];
                for (i = 0; i < limit - 4; i++) {
                  uint32_t mask = 0;

                  elf_lzma_range_normalize(compressed, compressed_size, poffset,
                                           &range, &code);
                  range >>= 1;
                  code -= range;
                  mask = -(code >> 31);
                  code += range & mask;
                  dist0 <<= 1;
                  dist0 += mask + 1;
                }
                dist0 <<= 4;
                probs_dist = probs + LZMA_DIST_ALIGN(0);
                dist0 += elf_lzma_reverse_integer(compressed, compressed_size,
                                                  probs_dist, 4, poffset,
                                                  &range, &code);
                dist[0] = dist0;
              }
            }
          }

          if (unlikely(uncompressed_offset - dict_start_offset < dist[0] + 1)) {
            elf_uncompress_failed();
            return 0;
          }
          if (unlikely(uncompressed_offset + len > uncompressed_size)) {
            elf_uncompress_failed();
            return 0;
          }

          if (dist[0] == 0) {
            // A common case, meaning repeat the last
            // character LEN times.
            memset(uncompressed + uncompressed_offset,
                   uncompressed[uncompressed_offset - 1], len);
            uncompressed_offset += len;
          } else if (dist[0] + 1 >= len) {
            memcpy(uncompressed + uncompressed_offset,
                   uncompressed + uncompressed_offset - dist[0] - 1, len);
            uncompressed_offset += len;
          } else {
            while (len > 0) {
              uint32_t copy = len < dist[0] + 1 ? len : dist[0] + 1;
              memcpy(uncompressed + uncompressed_offset,
                     (uncompressed + uncompressed_offset - dist[0] - 1), copy);
              len -= copy;
              uncompressed_offset += copy;
            }
          }
        } else {
          unsigned char prev = 0;
          unsigned char low = 0;
          size_t high = 0;
          uint16_t *lit_probs = NULL;
          unsigned int sym = 0;

          // Literal value.

          if (uncompressed_offset > 0) {
            prev = uncompressed[uncompressed_offset - 1];
          } else {
            prev = 0;
          }
          low = prev >> (8 - lc);
          high = (((uncompressed_offset - dict_start_offset) & ((1 << lp) - 1))
                  << lc);
          lit_probs = probs + LZMA_LITERAL(low + high, 0);
          if (lstate < 7) {
            sym = elf_lzma_integer(compressed, compressed_size, lit_probs, 8,
                                   poffset, &range, &code);
          } else {
            unsigned int match = 0;
            unsigned int bit = 0;
            unsigned int match_bit = 0;
            unsigned int idx = 0;

            sym = 1;
            if (uncompressed_offset >= dist[0] + 1) {
              match = uncompressed[uncompressed_offset - dist[0] - 1];
            } else {
              match = 0;
            }
            match <<= 1;
            bit = 0x100;
            do {
              match_bit = match & bit;
              match <<= 1;
              idx = bit + match_bit + sym;
              sym <<= 1;
              if (elf_lzma_bit(compressed, compressed_size, lit_probs + idx,
                               poffset, &range, &code)) {
                ++sym;
                bit &= match_bit;
              } else {
                bit &= ~match_bit;
              }
            } while (sym < 0x100);
          }

          if (unlikely(uncompressed_offset >= uncompressed_size)) {
            elf_uncompress_failed();
            return 0;
          }

          uncompressed[uncompressed_offset] = (unsigned char)sym;
          ++uncompressed_offset;
          if (lstate <= 3) {
            lstate = 0;
          } else if (lstate <= 9) {
            lstate -= 3;
          } else {
            lstate -= 6;
          }
        }
      }

      elf_lzma_range_normalize(compressed, compressed_size, poffset, &range,
                               &code);

      off = *poffset;
    }
  }

  // We have reached the end of the block.  Pad to four byte
  // boundary.
  off = (off + 3) & ~(size_t)3;
  if (unlikely(off > compressed_size)) {
    elf_uncompress_failed();
    return 0;
  }

  switch (check) {
  case 0:
    // No check.
    break;

  case 1:
    // CRC32
    if (unlikely(off + 4 > compressed_size)) {
      elf_uncompress_failed();
      return 0;
    }
    computed_crc = elf_crc32(0, uncompressed, uncompressed_offset);
    stream_crc = (compressed[off] | (compressed[off + 1] << 8) |
                  (compressed[off + 2] << 16) | (compressed[off + 3] << 24));
    if (computed_crc != stream_crc) {
      elf_uncompress_failed();
      return 0;
    }
    off += 4;
    break;

  case 4:
    // CRC64.  We don't bother computing a CRC64 checksum.
    if (unlikely(off + 8 > compressed_size)) {
      elf_uncompress_failed();
      return 0;
    }
    off += 8;
    break;

  case 10:
    // SHA.  We don't bother computing a SHA checksum.
    if (unlikely(off + 32 > compressed_size)) {
      elf_uncompress_failed();
      return 0;
    }
    off += 32;
    break;

  default:
    elf_uncompress_failed();
    return 0;
  }

  *poffset = off;

  return 1;
}

// Uncompress LZMA data found in a minidebug file.  The minidebug
// format is described at
// https://sourceware.org/gdb/current/onlinedocs/gdb/MiniDebugInfo.html.
// Returns 0 on error, 1 on successful decompression.  For this
// function we return 0 on failure to decompress, as the calling code
// will carry on in that case.
int elf_uncompress_lzma(ten_backtrace_t *self, const unsigned char *compressed,
                        size_t compressed_size,
                        ten_backtrace_on_error_func_t on_error, void *data,
                        unsigned char **uncompressed,
                        size_t *uncompressed_size) {
  size_t header_size = 0;
  size_t footer_size = 0;
  unsigned char check = 0;
  uint32_t computed_crc = 0;
  uint32_t stream_crc = 0;
  size_t offset = 0;
  size_t index_size = 0;
  size_t footer_offset = 0;
  size_t index_offset = 0;
  uint64_t index_compressed_size = 0;
  uint64_t index_uncompressed_size = 0;
  unsigned char *mem = NULL;
  uint16_t *probs = NULL;
  size_t compressed_block_size = 0;

  // The format starts with a stream header and ends with a stream
  // footer.
  header_size = 12;
  footer_size = 12;
  if (unlikely(compressed_size < header_size + footer_size)) {
    elf_uncompress_failed();
    return 0;
  }

  // The stream header starts with a magic string.
  if (unlikely(memcmp(compressed,
                      "\375"
                      "7zXZ\0",
                      6) != 0)) {
    elf_uncompress_failed();
    return 0;
  }

  // Next come stream flags.  The first byte is zero, the second byte
  // is the check.
  if (unlikely(compressed[6] != 0)) {
    elf_uncompress_failed();
    return 0;
  }
  check = compressed[7];
  if (unlikely((check & 0xf8) != 0)) {
    elf_uncompress_failed();
    return 0;
  }

  // Next comes a CRC of the stream flags.
  computed_crc = elf_crc32(0, compressed + 6, 2);
  stream_crc = (compressed[8] | (compressed[9] << 8) | (compressed[10] << 16) |
                (compressed[11] << 24));
  if (unlikely(computed_crc != stream_crc)) {
    elf_uncompress_failed();
    return 0;
  }

  // Now that we've parsed the header, parse the footer, so that we
  // can get the uncompressed size.

  // The footer ends with two magic bytes.

  offset = compressed_size;
  if (unlikely(memcmp(compressed + offset - 2, "YZ", 2) != 0)) {
    elf_uncompress_failed();
    return 0;
  }
  offset -= 2;

  // Before that are the stream flags, which should be the same as the
  // flags in the header.
  if (unlikely(compressed[offset - 2] != 0 ||
               compressed[offset - 1] != check)) {
    elf_uncompress_failed();
    return 0;
  }
  offset -= 2;

  // Before that is the size of the index field, which precedes the
  // footer.
  index_size =
      (compressed[offset - 4] | (compressed[offset - 3] << 8) |
       (compressed[offset - 2] << 16) | (compressed[offset - 1] << 24));
  index_size = (index_size + 1) * 4;
  offset -= 4;

  // Before that is a footer CRC.
  computed_crc = elf_crc32(0, compressed + offset, 6);
  stream_crc =
      (compressed[offset - 4] | (compressed[offset - 3] << 8) |
       (compressed[offset - 2] << 16) | (compressed[offset - 1] << 24));
  if (unlikely(computed_crc != stream_crc)) {
    elf_uncompress_failed();
    return 0;
  }
  offset -= 4;

  // The index comes just before the footer.
  if (unlikely(offset < index_size + header_size)) {
    elf_uncompress_failed();
    return 0;
  }

  footer_offset = offset;
  offset -= index_size;
  index_offset = offset;

  // The index starts with a zero byte.
  if (unlikely(compressed[offset] != 0)) {
    elf_uncompress_failed();
    return 0;
  }
  ++offset;

  // Next is the number of blocks.  We expect zero blocks for an empty
  // stream, and otherwise a single block.
  if (unlikely(compressed[offset] == 0)) {
    *uncompressed = NULL;
    *uncompressed_size = 0;
    return 1;
  }
  if (unlikely(compressed[offset] != 1)) {
    elf_uncompress_failed();
    return 0;
  }
  ++offset;

  // Next is the compressed size and the uncompressed size.
  if (!elf_lzma_varint(compressed, compressed_size, &offset,
                       &index_compressed_size)) {
    return 0;
  }
  if (!elf_lzma_varint(compressed, compressed_size, &offset,
                       &index_uncompressed_size)) {
    return 0;
  }

  // Pad to a four byte boundary.
  offset = (offset + 3) & ~(size_t)3;

  // Next is a CRC of the index.
  computed_crc = elf_crc32(0, compressed + index_offset, offset - index_offset);
  stream_crc =
      (compressed[offset] | (compressed[offset + 1] << 8) |
       (compressed[offset + 2] << 16) | (compressed[offset + 3] << 24));
  if (unlikely(computed_crc != stream_crc)) {
    elf_uncompress_failed();
    return 0;
  }
  offset += 4;

  // We should now be back at the footer.
  if (unlikely(offset != footer_offset)) {
    elf_uncompress_failed();
    return 0;
  }

  // Allocate space to hold the uncompressed data.  If we succeed in
  // uncompressing the LZMA data, we never free this memory.
  mem = (unsigned char *)malloc(index_uncompressed_size);
  if (unlikely(mem == NULL)) {
    return 0;
  }

  *uncompressed = mem;
  *uncompressed_size = index_uncompressed_size;

  // Allocate space for probabilities.
  probs = (uint16_t *)malloc(LZMA_PROB_TOTAL_COUNT * sizeof(uint16_t));
  if (unlikely(probs == NULL)) {
    free(mem);
    return 0;
  }

  // Uncompress the block, which follows the header.
  offset = 12;
  if (!elf_uncompress_lzma_block(compressed, compressed_size, check, probs, mem,
                                 index_uncompressed_size, &offset)) {
    free(mem);
    return 0;
  }

  compressed_block_size = offset - 12;
  if (unlikely(compressed_block_size !=
               ((index_compressed_size + 3) & ~(size_t)3))) {
    elf_uncompress_failed();
    free(mem);
    return 0;
  }

  offset = (offset + 3) & ~(size_t)3;
  if (unlikely(offset != index_offset)) {
    elf_uncompress_failed();
    free(mem);
    return 0;
  }

  return 1;
}
