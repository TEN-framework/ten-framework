//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/file.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

#define FILE_COPY_BUF_SIZE 4096

int ten_file_remove(const char *filename) {
  TEN_ASSERT(filename, "Invalid argument.");

  if (!filename || strlen(filename) == 0) {
    TEN_ASSERT(filename, "Invalid argument.");
    return -1;
  }

  if (!ten_path_exists(filename)) {
    TEN_LOGE("Failed to find %s", filename);
    return -1;
  }

  if (remove(filename)) {
    TEN_LOGE("Failed to remove %s", filename);
    return -1;
  }

  return 0;
}

static char *ten_file_read_from_open_file(FILE *fp) {
  TEN_ASSERT(fp, "Invalid argument.");

  if (fseek(fp, 0, SEEK_END) == -1) {
    TEN_LOGE("Failed to fseek to the end of the file.");
    return NULL;
  }

  unsigned long length = ftell(fp);
  if (length <= 0) {
    TEN_LOGW("File size is 0.");
    return NULL;
  }

  char *buf = (char *)TEN_MALLOC(sizeof(char) * (length + 1));
  TEN_ASSERT(buf, "Failed to allocate memory.");

  rewind(fp);

  length = fread(buf, 1, length, fp);
  buf[length] = '\0';

  return buf;
}

char *ten_file_read(const char *filename) {
  TEN_ASSERT(filename, "Invalid argument.");

  if (!filename || strlen(filename) == 0) {
    TEN_ASSERT(filename, "Invalid argument.");
    return NULL;
  }

  if (!ten_path_exists(filename)) {
    TEN_LOGE("Failed to find %s", filename);
    return NULL;
  }

  FILE *file = fopen(filename, "rb");
  if (!file) {
    TEN_LOGE("Failed to fopen %s", filename);
    return NULL;
  }

  char *buf = ten_file_read_from_open_file(file);

  if (fclose(file)) {
    TEN_LOGE("Failed to fclose %s", filename);
  }

  return buf;
}

int ten_file_write(const char *filename, ten_buf_t buf) {
  TEN_ASSERT(filename && buf.data, "Invalid argument.");

  if (!filename || strlen(filename) == 0) {
    TEN_ASSERT(filename, "Invalid argument.");
    return -1;
  }

  FILE *file = fopen(filename, "wb");
  if (!file) {
    TEN_LOGE("Failed to fopen %s", filename);
    return -1;
  }

  int result = ten_file_write_to_open_file(file, buf);

  if (fclose(file)) {
    TEN_LOGE("Failed to fclose %s", filename);
  }

  return result;
}

int ten_file_write_to_open_file(FILE *fp, ten_buf_t buf) {
  TEN_ASSERT(fp && buf.data, "Invalid argument.");

  size_t wrote_size = fwrite(buf.data, 1, buf.content_size, fp);
  if (wrote_size != buf.content_size) {
    TEN_LOGE("Failed to write fwrite.");
    return -1;
  }

  return 0;
}

int ten_file_copy(const char *src_filename, const char *dest_filename) {
  TEN_ASSERT(src_filename && dest_filename, "Invalid argument.");

  FILE *src_file = NULL;
  FILE *dest_file = NULL;
  char buffer[FILE_COPY_BUF_SIZE];
  unsigned long read_size = 0;
  unsigned long write_size = 0;
  int result = 0;

  src_file = fopen(src_filename, "rb");
  if (!src_file) {
    TEN_LOGE("Failed to fopen source %s: %d", src_filename, errno);
    result = -1;
    goto error;
  }

  dest_file = fopen(dest_filename, "wb");
  if (!dest_file) {
    TEN_LOGE("Failed to fopen destination %s: %d", dest_filename, errno);
    result = -1;
    goto error;
  }

  while ((read_size = fread(buffer, 1, FILE_COPY_BUF_SIZE, src_file)) > 0) {
    write_size = fwrite(buffer, 1, read_size, dest_file);

    if (write_size != read_size) {
      TEN_LOGE("Failed to fwrite to %s", dest_filename);
      result = -1;
      break;
    }

    if (read_size < 0) {
      TEN_LOGE("Failed to fread from %s", src_filename);
      result = -1;
      break;
    }
  }

  result = ten_file_clone_permission_by_fd(fileno(src_file), fileno(dest_file));

error:
  if (src_file) {
    if (fclose(src_file)) {
      TEN_LOGE("Failed to fclose %s", src_filename);
    }
  }
  if (dest_file) {
    if (fclose(dest_file)) {
      TEN_LOGE("Failed to fclose %s", dest_filename);
    }
  }
  return result;
}

int ten_file_copy_to_dir(const char *src_file, const char *dest_dir) {
  TEN_ASSERT(src_file && dest_dir && ten_path_exists(dest_dir),
             "Invalid argument.");

  ten_string_t *src_file_ten_string =
      ten_string_create_formatted("%s", src_file);
  ten_string_t *filename = ten_path_get_filename(src_file_ten_string);
  ten_string_t *dest_file = ten_string_create_formatted(
      "%s/%s", dest_dir, ten_string_get_raw_str(filename));
  ten_path_to_system_flavor(dest_file);

  int rc = ten_file_copy(src_file, ten_string_get_raw_str(dest_file));

  ten_string_destroy(dest_file);
  ten_string_destroy(filename);
  ten_string_destroy(src_file_ten_string);

  return rc;
}
