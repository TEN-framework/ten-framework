//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zutils.h"

#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"

/**
 * @brief Empty function that serves as a breakpoint target for debugging
 * decompression failures.
 *
 * This function is intentionally empty and is called when decompression
 * operations fail. When the code is compiled with debug symbols (-g),
 * developers can set a breakpoint on this function to catch and investigate
 * compression-related failures during runtime.
 *
 * The function is called from various places in the decompression code path
 * where errors are detected, providing a centralized location for debugging.
 */
void elf_uncompress_failed(void) {
  // Intentionally empty - used only as a breakpoint target.
}

/**
 * @brief Ensures the bit buffer has at least 15 valid bits for decompression.
 *
 * This function is used during decompression to ensure the bit buffer (*pval)
 * contains at least 15 valid bits by reading additional bytes from the input
 * stream as needed. It handles endianness differences when reading 32-bit
 * chunks from the input stream.
 *
 * @param ppin Pointer to the current position in the input stream; updated on
 * return.
 * @param pinend Pointer to the end of the input stream (for bounds checking).
 * @param pval Pointer to the current bit accumulator; updated with new bits.
 * @param pbits Pointer to the count of valid bits in *pval; updated on return.
 *
 * @return 1 on success, 0 if there's not enough input data remaining.
 */
int elf_fetch_bits(const unsigned char **ppin, const unsigned char *pinend,
                   uint64_t *pval, unsigned int *pbits) {
  unsigned int bits = 0;
  const unsigned char *pin = NULL;
  uint64_t val = 0;
  uint32_t next = 0;

  bits = *pbits;
  // If we already have enough bits, return immediately.
  if (bits >= 15) {
    return 1;
  }
  pin = *ppin;
  val = *pval;

  // Check if we have at least 4 bytes available to read.
  if (unlikely(pinend - pin < 4)) {
    elf_uncompress_failed();
    return 0;
  }

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) &&                               \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ ||                     \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

  // Fast path: read 4 bytes at once when byte order is known at compile time
  // Note: This assumes the input pointer is properly aligned for uint32_t
  // access
  next = *(const uint32_t *)pin;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  // Convert from big-endian to little-endian byte order.
  next = __builtin_bswap32(next);
#endif
#else
  // Fallback path: read bytes individually and combine them.
  // This works regardless of alignment or endianness.
  next = pin[0] | (pin[1] << 8) | (pin[2] << 16) | (pin[3] << 24);
#endif

  // Append the new bits to the existing value.
  val |= (uint64_t)next << bits;
  bits += 32;
  pin += 4;

  // Optimize memory access by prefetching the next chunk of data.
  __builtin_prefetch(pin, 0, 0);

  // Update the caller's variables.
  *ppin = pin;
  *pval = val;
  *pbits = bits;

  return 1;
}
