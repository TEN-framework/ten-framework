//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/file.h"

#include <io.h>
#include <stdio.h>

#include "ten_utils/lib/path.h"
#include "ten_utils/macro/check.h"

int ten_file_get_fd(FILE *fp) { return _fileno(fp); }

int ten_file_size(const char *filename) {
  TEN_ASSERT(filename, "Invalid argument.");

  TEN_ASSERT(0 && "Need to implement this.", "Invalid argument.");
  return -1;
}

int ten_file_chmod(const char *filename, uint32_t mode) {
  TEN_ASSERT(filename, "Invalid argument.");

  // TODO(ZhangXianyao): Need to implement.
  (void)mode;

  return 0;
}

int ten_file_clone_permission(const char *src_filename,
                              const char *dest_filename) {
  TEN_ASSERT(src_filename && dest_filename, "Invalid argument.");

  // TODO(ZhangXianyao): Need to implement.
  return 0;
}

int ten_file_clone_permission_by_fd(int src_fd, int dest_fd) {
  // TODO(ZhangXianyao): Need to implement.
  (void)src_fd;
  (void)dest_fd;

  return 0;
}

int ten_file_clear_open_file_content(FILE *fp) {
  TEN_ASSERT(fp, "Invalid argument.");

  rewind(fp);
  return _chsize(ten_file_get_fd(fp), 0);
}

char *ten_symlink_file_read(const char *path) {
  TEN_ASSERT(path && ten_path_is_symlink(path), "Invalid argument.");

  // TODO(ZhangXianyao): Need to implement.
  return NULL;
}

int ten_symlink_file_copy(const char *src_file, const char *dest_file) {
  TEN_ASSERT(src_file && ten_path_is_symlink(src_file), "Invalid argument.");

  // TODO(ZhangXianyao): Need to implement.
  return 0;
}
