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

#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zdebug.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf.h"

/**
 * @brief Uncompress the old compressed debug format (zlib-gnu).
 *
 * This function handles the debug format emitted by the compiler option
 * --compress-debug-sections=zlib-gnu. The format consists of:
 * 1. Four bytes "ZLIB" signature.
 * 2. Eight bytes containing the uncompressed size in big-endian order.
 * 3. The actual zlib compressed data stream.
 *
 * @param self The backtrace context.
 * @param compressed Pointer to the compressed data.
 * @param compressed_size Size of the compressed data.
 * @param zdebug_table Work space to hold Huffman tables for decompression.
 * @param on_error Error callback function.
 * @param data User data to pass to error callback.
 * @param uncompressed Pointer to store the uncompressed data. If *uncompressed
 *                     is not NULL and *uncompressed_size is large enough, the
 *                     existing buffer will be used; otherwise, a new buffer
 *                     will be allocated.
 * @param uncompressed_size Pointer to store the size of uncompressed data.
 *
 * @return 1 on successful decompression or if format is not recognized,
 *         0 on memory allocation failure.
 */
int elf_uncompress_zdebug(ten_backtrace_t *self,
                          const unsigned char *compressed,
                          size_t compressed_size, uint16_t *zdebug_table,
                          ten_backtrace_on_error_func_t on_error, void *data,
                          unsigned char **uncompressed,
                          size_t *uncompressed_size) {
  assert(uncompressed && "Invalid argument: uncompressed is NULL.");

  size_t sz = 0;
  size_t i = 0;
  unsigned char *po = NULL;

  *uncompressed = NULL;
  *uncompressed_size = 0;

  // Check for valid compressed data format.
  if (compressed_size < 12 || memcmp(compressed, "ZLIB", 4) != 0) {
    // Not in the expected format, return success but do nothing.
    return 1;
  }

  // Extract the uncompressed size from the 8-byte big-endian value.
  sz = 0;
  for (i = 0; i < 8; i++) {
    sz = (sz << 8) | compressed[i + 4];
  }

  // Either use the provided buffer or allocate a new one.
  if (*uncompressed != NULL && *uncompressed_size >= sz) {
    po = *uncompressed;
  } else {
    po = (unsigned char *)malloc(sz);
    if (po == NULL) {
      // Memory allocation failed.
      if (on_error) {
        on_error(self, "malloc failed", 0, data);
      }
      assert(0 && "Memory allocation failed.");
      return 0;
    }
  }

  // Attempt to decompress the data.
  if (!elf_zlib_inflate_and_verify(compressed + 12, compressed_size - 12,
                                   zdebug_table, po, sz)) {
    // Decompression failed, but we continue anyway.
    // Free the allocated buffer if we created one.
    if (po != *uncompressed) {
      free(po);
    }
    return 1;
  }

  // Decompression successful.
  *uncompressed = po;
  *uncompressed_size = sz;

  return 1;
}

/**
 * @brief Uncompress data from the standard ELF compressed debug format
 * (zlib-gabi).
 *
 * This function handles decompression of debug sections compressed with the
 * official ELF standard approach, which is emitted by the compiler option
 * --compress-debug-sections=zlib-gabi. The compressed data begins with an
 * ELF compression header (b_elf_chdr) that specifies the compression type
 * and uncompressed size.
 *
 * Currently supports ZLIB and ZSTD compression algorithms.
 *
 * @param self The backtrace context.
 * @param compressed Pointer to the compressed data.
 * @param compressed_size Size of the compressed data.
 * @param zdebug_table Work space for decompression algorithms.
 * @param on_error Error callback function.
 * @param data User data to pass to error callback.
 * @param uncompressed Pointer to store the uncompressed data. If not NULL and
 *                     large enough, will be used directly; otherwise, a new
 *                     buffer will be allocated.
 * @param uncompressed_size Pointer to store the size of uncompressed data.
 *
 * @return 1 on success (including cases where we choose to continue despite
 *         decompression issues), 0 on critical errors like memory allocation
 *         failure.
 */
int elf_uncompress_chdr(ten_backtrace_t *self, const unsigned char *compressed,
                        size_t compressed_size, uint16_t *zdebug_table,
                        ten_backtrace_on_error_func_t on_error, void *data,
                        unsigned char **uncompressed,
                        size_t *uncompressed_size) {
  const b_elf_chdr *chdr = NULL;
  char *alc = NULL;
  size_t alc_len = 0;
  unsigned char *po = NULL;

  *uncompressed = NULL;
  *uncompressed_size = 0;

  // The format starts with an ELF compression header.
  if (compressed_size < sizeof(b_elf_chdr)) {
    // Not enough data for a valid header, return success but do nothing.
    return 1;
  }

  chdr = (const b_elf_chdr *)compressed;

  // Either use the provided buffer or allocate a new one.
  if (*uncompressed != NULL && *uncompressed_size >= chdr->ch_size) {
    po = *uncompressed;
  } else {
    alc_len = chdr->ch_size;
    alc = malloc(alc_len);
    if (alc == NULL) {
      // Memory allocation failed.
      if (on_error) {
        on_error(self, "malloc failed", 0, data);
      }
      assert(0 && "Memory allocation failed.");
      return 0;
    }
    po = (unsigned char *)alc;
  }

  switch (chdr->ch_type) {
  case ELFCOMPRESS_ZLIB:
    if (!elf_zlib_inflate_and_verify(compressed + sizeof(b_elf_chdr),
                                     compressed_size - sizeof(b_elf_chdr),
                                     zdebug_table, po, chdr->ch_size)) {
      goto skip;
    }
    break;

  case ELFCOMPRESS_ZSTD:
    if (!elf_zstd_decompress(compressed + sizeof(b_elf_chdr),
                             compressed_size - sizeof(b_elf_chdr),
                             (unsigned char *)zdebug_table, po,
                             chdr->ch_size)) {
      goto skip;
    }
    break;

  default:
    // Unsupported compression algorithm.
    goto skip;
  }

  // Decompression successful.
  *uncompressed = po;
  *uncompressed_size = chdr->ch_size;

  return 1;

skip:
  // Decompression failed, but we continue anyway.
  // Free the allocated buffer if we created one.
  if (alc != NULL) {
    free(alc);
  }
  return 1;
}
