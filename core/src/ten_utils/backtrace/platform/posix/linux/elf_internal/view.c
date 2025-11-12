//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/view.h"

#include <assert.h>
#include <stdint.h>

#include "include_internal/ten_utils/backtrace/platform/posix/mmap.h"
#include "ten_utils/macro/mark.h"

/**
 * @brief Create a view of ELF file data for reading.
 *
 * This function creates a view into either a file descriptor or an in-memory
 * buffer. It handles two cases:
 * 1. If memory is NULL, it maps the file descriptor using mmap.
 * 2. If memory is provided, it creates a view directly into the memory buffer.
 *
 * @param self The backtrace context.
 * @param descriptor File descriptor to use when memory is NULL.
 * @param memory Pointer to in-memory ELF data, or NULL to use descriptor.
 * @param memory_size Size of the in-memory buffer (ignored if memory is NULL).
 * @param offset Offset within the file/memory to start the view.
 * @param size Number of bytes to include in the view.
 * @param on_error Error callback function.
 * @param data User data to pass to error callback.
 * @param view Output parameter to store the created view.
 *
 * @return 1 on success, 0 on failure.
 */
int elf_get_view(ten_backtrace_t *self, int descriptor,
                 const unsigned char *memory, size_t memory_size, off_t offset,
                 uint64_t size, ten_backtrace_on_error_func_t on_error,
                 void *data, elf_view *view) {
  assert(self && "Invalid argument.");
  assert(view && "Invalid view argument.");

  // Check for potential integer overflow in offset + size calculation.
  if (size > 0 && (uint64_t)offset > UINT64_MAX - size) {
    on_error(self, "integer overflow in offset + size", 0, data);
    assert(0 && "Integer overflow in offset + size.");
    return 0;
  }

  if (memory == NULL) {
    // Use file descriptor with mmap.
    view->release = 1;
    return ten_mmap_init(&view->view, descriptor, offset, size);
  } else {
    // Use in-memory buffer.
    if ((uint64_t)offset + size > (uint64_t)memory_size) {
      on_error(self, "out of range for in-memory file", 0, data);
      return 0;
    }
    view->view.data = (const void *)(memory + offset);
    view->view.base = NULL;
    view->view.len = size;
    view->release = 0;
    return 1;
  }
}

/**
 * @brief Release a view previously created by elf_get_view.
 *
 * This function properly cleans up resources associated with an ELF view:
 * - For views created from file descriptors (with release=1), it unmaps the
 *   memory.
 * - For views created from in-memory buffers (with release=0), it simply
 *   returns.
 *
 * @param self The backtrace context.
 * @param view Pointer to the view to be released.
 * @param on_error Error callback function (unused but kept for API
 * consistency).
 * @param data User data to pass to error callback (unused).
 */
void elf_release_view(TEN_UNUSED ten_backtrace_t *self, elf_view *view,
                      TEN_UNUSED ten_backtrace_on_error_func_t on_error,
                      TEN_UNUSED void *data) {
  assert(self && "Invalid argument.");
  assert(view && "Invalid view argument.");

  if (view->release) {
    ten_mmap_deinit(&view->view);
  }
}
