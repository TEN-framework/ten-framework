//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/backtrace.h"

/**
 * @brief A buffer structure for reading and parsing DWARF debugging
 * information.
 *
 * This structure maintains the state needed to read DWARF data from a buffer,
 * including position tracking, endianness information, and error handling.
 * It supports operations like reading integers of various sizes, LEB128 values,
 * strings, and other DWARF-specific data formats.
 */
typedef struct dwarf_buf {
  // Name of the buffer source, used in error messages for identification.
  const char *name;

  // Pointer to the beginning of the buffer for offset calculations.
  const unsigned char *start;

  // Current read position within the buffer.
  const unsigned char *buf;

  // Number of bytes remaining to be read from the current position.
  size_t left;

  // Endianness flag: non-zero if data is big-endian, zero if little-endian.
  // Determines how multi-byte values are interpreted.
  int is_bigendian;

  // Callback function for reporting errors encountered during parsing.
  // Must not be NULL when the buffer is used.
  ten_backtrace_on_error_func_t on_error;

  // User-provided context data passed to the error callback function.
  void *data;

  // Flag indicating whether a buffer underflow error has already been reported.
  // Used to prevent multiple error reports for the same underflow condition.
  int reported_underflow;
} dwarf_buf;
