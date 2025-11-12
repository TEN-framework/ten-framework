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
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zlib.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zstd.h"

#define ZDEBUG_TABLE_SIZE \
  (ZLIB_TABLE_SIZE > ZSTD_TABLE_SIZE ? ZLIB_TABLE_SIZE : ZSTD_TABLE_SIZE)

TEN_UTILS_PRIVATE_API int elf_uncompress_zdebug(
    ten_backtrace_t *self, const unsigned char *compressed,
    size_t compressed_size, uint16_t *zdebug_table,
    ten_backtrace_on_error_func_t on_error, void *data,
    unsigned char **uncompressed, size_t *uncompressed_size);

TEN_UTILS_PRIVATE_API int elf_uncompress_chdr(
    ten_backtrace_t *self, const unsigned char *compressed,
    size_t compressed_size, uint16_t *zdebug_table,
    ten_backtrace_on_error_func_t on_error, void *data,
    unsigned char **uncompressed, size_t *uncompressed_size);
