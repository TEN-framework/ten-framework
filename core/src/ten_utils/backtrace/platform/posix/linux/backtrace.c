//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/backtrace/backtrace.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/buffer.h"
#include "include_internal/ten_utils/backtrace/common.h"
#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"

/**
 * @brief Creates a new backtrace object for Linux platform.
 *
 * This function allocates memory for a ten_backtrace_posix_t structure and
 * initializes its fields with default values. It sets up the common fields
 * with default callback functions for dumping stack traces and handling errors.
 * All function pointers for symbol resolution are initialized to NULL and
 * will be set up when needed.
 *
 * @return A pointer to the newly created backtrace object cast to
 * ten_backtrace_t.
 *
 * @note The returned object must be freed with `ten_backtrace_destroy()` when
 * no longer needed.
 */
ten_backtrace_t *ten_backtrace_create(void) {
  ten_backtrace_posix_t *self = malloc(sizeof(ten_backtrace_posix_t));
  if (!self) {
    assert(0 && "Failed to allocate memory.");
    return NULL;  // Return NULL if malloc fails, even after assert
  }

  ten_backtrace_common_init(&self->common, ten_backtrace_default_dump,
                            ten_backtrace_default_error);

  // Initialize all function pointers and data pointers to NULL.
  self->on_get_file_line = NULL;
  self->on_get_file_line_data = NULL;
  self->on_get_syminfo = NULL;
  self->on_get_syminfo_data = NULL;

  ten_atomic_store(&self->file_line_init_failed, 0);

  return (ten_backtrace_t *)self;
}

/**
 * @brief Destroys a backtrace object and frees associated resources.
 *
 * This function properly cleans up resources associated with the backtrace
 * object by first calling the common deinitialization function and then
 * freeing the memory allocated for the object itself.
 *
 * @param self Pointer to the backtrace object to destroy. Must not be NULL.
 */
void ten_backtrace_destroy(ten_backtrace_t *self) {
  if (!self) {
    assert(0 && "Invalid argument: NULL pointer provided.");

    // Return early to avoid dereferencing NULL pointer.
    return;
  }

  ten_backtrace_common_deinit(self);

  free(self);
}

/**
 * @brief Dumps the current call stack using multiple backtrace methods.
 *
 * This function captures the current call stack and processes it using
 * both glibc and libgcc implementations for Linux platforms. It first attempts
 * to use glibc's backtrace functions which provide basic symbol information,
 * then tries libgcc's unwinder which can provide more detailed file and line
 * information when debug symbols are available.
 *
 * Using both methods provides more comprehensive backtrace information,
 * as one method might succeed where the other fails or provides incomplete
 * information.
 *
 * @param self Pointer to the backtrace object. Must not be NULL.
 * @param skip Number of stack frames to skip from the top of the call stack.
 *             This is useful to exclude the backtrace function itself and
 *             its immediate callers from the output.
 */
void ten_backtrace_dump(ten_backtrace_t *self, size_t skip) {
  if (!self) {
    assert(0 && "Invalid argument: NULL pointer provided.");

    // Return early to avoid dereferencing NULL pointer.
    return;
  }

  // First try glibc's backtrace which provides basic symbol information
  ten_backtrace_dump_using_glibc(self, skip);

  // Then try libgcc's unwinder which can provide more detailed information
  // when debug symbols are available
  ten_backtrace_dump_using_libgcc(self, skip);
}

/**
 * @brief Captures the current call stack and writes it to a provided buffer.
 *
 * This function captures the current call stack and writes the formatted
 * backtrace information to the provided buffer. It skips a specified number
 * of frames from the top of the call stack.
 *
 * @param self Pointer to the backtrace object. Must not be NULL.
 * @param buffer Pointer to the buffer where the backtrace will be written. Must
 * not be NULL.
 * @param buffer_size Size of the buffer in bytes. Must be greater than 0.
 * @param skip Number of stack frames to skip from the top of the call stack.
 *             This is useful to exclude this function and its immediate
 *             callers.
 *
 * @return 1 on success, 0 on failure (when invalid arguments are provided).
 * @note The buffer will be populated with the backtrace information, and it's
 * the caller's responsibility to ensure the buffer is large enough.
 */
int ten_backtrace_capture_to_buffer(ten_backtrace_t *self, char *buffer,
                                    size_t buffer_size, size_t skip) {
  assert(self);
  assert(buffer);
  assert(buffer_size);

  if (!self || !buffer || buffer_size == 0) {
    assert(0 &&
           "Invalid arguments: NULL pointer or zero buffer size provided.");
    return 0;
  }

  ten_backtrace_buffer_t backtrace_buffer;
  ten_backtrace_buffer_init(&backtrace_buffer, buffer, buffer_size);

  ten_backtrace_common_t *common = (ten_backtrace_common_t *)self;
  common->on_dump_file_line = ten_backtrace_buffer_dump;
  common->on_error = ten_backtrace_default_error;
  common->cb_data = &backtrace_buffer;

  // Capture backtrace, adding 1 to skip to account for this function itself.
  ten_backtrace_dump_using_libgcc(self, skip + 1);

  return 1;
}
