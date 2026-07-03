//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/file.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @file file.c
 * @brief Platform-specific file operations for backtrace functionality.
 *
 * This file provides cross-platform compatible file handling operations used
 * by the backtrace system. It includes open and close operations with proper
 * error handling and platform-specific compatibility adjustments.
 */

// Mac OS X 10.6 does not support O_CLOEXEC.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

// Mac OS does not support FD_CLOEXEC.
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

/**
 * @brief Opens a file for reading with proper error handling.
 *
 * This function opens the specified file in read-only mode with the
 * close-on-exec flag set to prevent file descriptor leakage across exec calls.
 * It handles platform-specific compatibility issues and provides detailed error
 * reporting.
 *
 * @param filename The path to the file to open.
 * @param does_not_exist Optional pointer to receive existence status (set to
 * true if file doesn't exist).
 * @return File descriptor on success, or -1 on failure.
 */
int ten_backtrace_open_file(const char *filename, bool *does_not_exist) {
  // Input validation.
  if (!filename) {
    errno = EINVAL;
    assert(0 && "Invalid argument.");
    return -1;
  }

  if (does_not_exist != NULL) {
    *does_not_exist = false;
  }

  // Open the file with close-on-exec flag when supported.
  int fd = open(filename, (O_RDONLY | O_CLOEXEC));
  if (fd < 0) {
    int saved_errno = errno;
    if (does_not_exist != NULL) {
      *does_not_exist = (saved_errno == ENOENT);
    } else {
      (void)fprintf(stderr, "Failed to open %s: %s\n", filename,
                    strerror(saved_errno));
    }

    return -1;
  }

  // Set FD_CLOEXEC just in case the kernel does not support O_CLOEXEC.
  // This provides a fallback for older systems.
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
    // Non-fatal error - log but continue.
    int saved_errno = errno;
    (void)fprintf(stderr, "Warning: Failed to set FD_CLOEXEC on %s: %s\n",
                  filename, strerror(saved_errno));
  }

  return fd;
}

/**
 * @brief Closes a file descriptor with error checking.
 *
 * This function safely closes the given file descriptor and reports any errors.
 * It provides better error handling than a raw close() call.
 *
 * @param fd File descriptor to close.
 * @return true if closed successfully, false on error.
 */
bool ten_backtrace_close_file(int fd) {
  // Input validation - avoid trying to close invalid descriptors.
  if (fd < 0) {
    assert(0 && "Invalid argument.");
    return true;
  }

  if (close(fd) < 0) {
    int saved_errno = errno;
    (void)fprintf(stderr, "Failed to close file descriptor %d: %s\n", fd,
                  strerror(saved_errno));
    return false;
  }

  return true;
}
