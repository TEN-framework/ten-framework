//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/mmap.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @file mmap.c
 * @brief This file implements memory-mapped file views using the POSIX mmap
 * API.
 *
 * Memory mapping provides an efficient way to access file contents by mapping
 * file data directly into the process's address space. This implementation
 * handles page-aligned memory mapping to ensure proper system compatibility.
 */

/**
 * @brief Initialize a memory-mapped view of a file.
 *
 * This function creates a memory-mapped view of a file descriptor at the
 * specified offset and size. It handles page alignment requirements by:
 * 1. Calculating the page offset to align memory properly.
 * 2. Adjusting the mapping size to page boundaries.
 * 3. Setting up the returned data pointer to the exact requested offset.
 *
 * @param self Pointer to the ten_mmap_t structure to initialize.
 * @param descriptor File descriptor for the file to map.
 * @param offset Byte offset within the file to start mapping.
 * @param size Number of bytes to map.
 * @return true on success, false on failure.
 */
bool ten_mmap_init(ten_mmap_t *self, int descriptor, off_t offset,
                   uint64_t size) {
  // Input validation - basic error checking.
  if (!self) {
    // Avoid assertion in production code that would crash the program.
    errno = EINVAL;
    assert(0 && "Invalid argument.");
    return false;
  }

  // Check if size can be safely cast to size_t (in case of 32-bit systems).
  if ((uint64_t)(size_t)size != size) {
    errno = EFBIG;  // File too large
    assert(0 && "File too large.");
    return false;
  }

  // Get system page size for alignment calculations.
  int pagesize = getpagesize();
  long in_page_offset = offset % pagesize;
  off_t offset_in_page_cnt = offset - in_page_offset;

  // Enlarge the size to the page boundary at the beginning and at the end. This
  // is required because mmap operates on page-aligned boundaries.
  size += in_page_offset;
  size = (size + (pagesize - 1)) & ~(pagesize - 1);

  // Create the memory mapping with read-only permissions.
  void *map =
      mmap(NULL, size, PROT_READ, MAP_PRIVATE, descriptor, offset_in_page_cnt);
  if (map == MAP_FAILED) {
    int saved_errno = errno;
    (void)fprintf(stderr, "Failed to mmap: %d (%s)\n", saved_errno,
                  strerror(saved_errno));
    return false;
  }

  // Initialize the structure with the mapping information.
  // data points to the exact requested offset within the mapping.
  self->data = (char *)map + in_page_offset;
  self->base = map;
  self->len = size;

  return true;
}

/**
 * @brief Release a memory-mapped view.
 *
 * This function releases the memory-mapped resources allocated in
 * ten_mmap_init. It unmaps the memory region to free system resources.
 *
 * @param self Pointer to the ten_mmap_t structure to deinitialize.
 */
void ten_mmap_deinit(ten_mmap_t *self) {
  // Input validation.
  if (!self || !self->base) {
    assert(0 && "Invalid argument.");
    return;
  }

  // Use a union to safely convert const void* to void*. This avoids
  // const-casting issues while maintaining type safety.
  union {
    const void *cv;
    void *v;
  } const_cast;

  const_cast.cv = self->base;
  if (munmap(const_cast.v, self->len) < 0) {
    int saved_errno = errno;
    (void)fprintf(stderr, "Failed to munmap: %d (%s)\n", saved_errno,
                  strerror(saved_errno));
    // Continue execution instead of asserting - allows recovery in production.
  }

  // Clear the structure to prevent use-after-free.
  self->data = NULL;
  self->base = NULL;
  self->len = 0;
}
