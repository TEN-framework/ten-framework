//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/backtrace/buffer.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/file.h"

#define BUFFER_LINE_MAX_LEN 1024

/**
 * @brief Initializes a backtrace buffer with the provided memory.
 *
 * This function initializes a backtrace buffer structure with user-provided
 * memory. The buffer is used to collect and store backtrace information.
 *
 * @param self     Pointer to the backtrace buffer structure to initialize.
 * @param data     Pointer to pre-allocated memory that will be used as the
 * buffer.
 * @param capacity Size of the pre-allocated memory in bytes.
 *
 * @note The function has assertions to check for invalid arguments in debug
 * builds, but also has runtime checks to safely handle invalid inputs in
 * release builds.
 */
void ten_backtrace_buffer_init(ten_backtrace_buffer_t *self, char *data,
                               size_t capacity) {
  assert(self && "Invalid argument: self pointer is NULL.");
  assert(data && "Invalid argument: data pointer is NULL.");
  assert(capacity > 0 && "Invalid argument: capacity must be greater than 0.");

  // Runtime checks for safety in release builds.
  if (!self || !data || capacity == 0) {
    return;
  }

  self->data = data;
  self->capacity = capacity;
  self->length = 0;
  self->overflow = 0;

  // Ensure the buffer starts with a null terminator to represent an empty
  // string
  self->data[0] = '\0';
}

int ten_backtrace_buffer_dump(ten_backtrace_t *self, uintptr_t pc,
                              const char *filename, int lineno,
                              const char *function, void *data) {
  // Check for null pointers first.
  if (!self || !data) {
    assert(0 && "Invalid argument: null pointer");
    return -1;
  }

  ten_backtrace_buffer_t *buffer = (ten_backtrace_buffer_t *)data;

  // Validate buffer.
  if (!buffer->data || buffer->capacity <= 1) {
    assert(0 && "Invalid buffer: null pointer or capacity is too small.");
    return -1;
  }

  // If overflow, do not continue writing.
  if (buffer->overflow) {
    // Return 0 to continue processing the next frame.
    return 0;
  }

  // Ensure we have valid strings to print.
  const char *safe_filename = filename ? filename : "<unknown file>";
  const char *safe_function = function ? function : "<unknown function>";

  // Normalize the filename to remove ".." path components.
  char normalized_path[NORMALIZE_PATH_BUF_SIZE] = {0};
  if (filename && ten_backtrace_normalize_path(safe_filename, normalized_path,
                                               sizeof(normalized_path))) {
    // Use the normalized path if successful.
    safe_filename = normalized_path;
  }

  // Format the output line.
  char line[BUFFER_LINE_MAX_LEN];
  int result = snprintf(line, sizeof(line), "%s@%s:%d (0x%0" PRIxPTR ")\n",
                        safe_function, safe_filename, lineno, pc);
  if (result < 0) {
    assert(0 && "Failed to format output.");
    return -1;
  }

  // Calculate the new length.
  size_t line_len = (size_t)result;

  // Make sure we have room for the line plus null terminator.
  if (buffer->length + line_len + 1 >= buffer->capacity) {
    buffer->overflow = 1;
    // Still ensure null termination.
    buffer->data[buffer->length] = '\0';
    return 0;
  }

  memcpy(buffer->data + buffer->length, line, line_len);

  buffer->length += line_len;
  buffer->data[buffer->length] = '\0';

  // Return 0 to continue processing the next frame.
  return 0;
}
