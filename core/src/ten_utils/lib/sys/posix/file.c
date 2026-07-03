//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/file.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ten_utils/lib/path.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

// Mac OS X 10.6 does not support O_CLOEXEC.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

// Mac OS does not support FD_CLOEXEC.
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

int ten_file_get_fd(FILE *fp) { return fileno(fp); }

int ten_file_size(const char *filename) {
  TEN_ASSERT(filename, "Invalid argument.");

  struct stat file_info;
  if (stat(filename, &file_info) == 0) {
    return file_info.st_size;
  }
  return -1;
}

int ten_file_chmod(const char *filename, uint32_t mode) {
  TEN_ASSERT(filename, "Invalid argument.");

  return chmod(filename, mode);
}

int ten_file_clone_permission(const char *src_filename,
                              const char *dest_filename) {
  TEN_ASSERT(src_filename && dest_filename, "Invalid argument.");

  int result = 0;
  struct stat file_stat;

  result = stat(src_filename, &file_stat);
  if (result) {
    return result;
  }

  result = chmod(dest_filename, file_stat.st_mode);

  return result;
}

int ten_file_clone_permission_by_fd(int src_fd, int dest_fd) {
  int result = 0;
  struct stat file_stat;

  result = fstat(src_fd, &file_stat);
  if (result) {
    return result;
  }

  result = fchmod(dest_fd, file_stat.st_mode);

  return result;
}

int ten_file_clear_open_file_content(FILE *fp) {
  TEN_ASSERT(fp, "Invalid argument.");

  rewind(fp);
  return ftruncate(ten_file_get_fd(fp), 0);
}

char *ten_symlink_file_read(const char *path) {
  TEN_ASSERT(path && ten_path_exists(path) && ten_path_is_symlink(path),
             "Invalid argument.");

  char *buf = NULL;
  struct stat st;
  if (lstat(path, &st) == 0) {
    long len = st.st_size + 1;
    buf = TEN_MALLOC(len);
    ssize_t rc = readlink(path, buf, len);
    TEN_ASSERT(rc == len - 1, "Failed to read symlink.");

    buf[len - 1] = '\0';
  }

  return buf;
}

int ten_symlink_file_copy(const char *src_file, const char *dest_file) {
  TEN_ASSERT(src_file && ten_path_is_symlink(src_file), "Invalid argument.");

  char *link_target = ten_symlink_file_read(src_file);
  if (link_target) {
    int rc = ten_path_make_symlink(link_target, dest_file);
    TEN_FREE(link_target);
    return rc;
  }

  return -1;
}

/**
 * @brief Open a file for reading.
 */
int ten_file_open(const char *filename, bool *does_not_exist) {
  TEN_ASSERT(filename, "Invalid argument.");

  if (does_not_exist != NULL) {
    *does_not_exist = false;
  }

  int fd = open(filename, (O_RDONLY | O_CLOEXEC));
  if (fd < 0) {
    if (does_not_exist != NULL) {
      *does_not_exist = true;
    } else {
      TEN_LOGE("Failed to open %s", filename);
    }

    return -1;
  }

  // Set FD_CLOEXEC just in case the kernel does not support O_CLOEXEC. It
  // doesn't matter if this fails for some reason.
  //
  // TODO(Wei): At some point it should be safe to only do this if O_CLOEXEC ==
  // 0.
  fcntl(fd, F_SETFD, FD_CLOEXEC);

  return fd;
}

/**
 * @brief Close @a fd.
 */
bool ten_file_close(int fd) {
  if (close(fd) < 0) {
    TEN_LOGE("Failed to close %d", fd);
    return false;
  }
  return true;
}
