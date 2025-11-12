//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ten_utils/lib/buf.h"

typedef struct ten_string_t ten_string_t;

TEN_UTILS_API int ten_file_remove(const char *filename);

TEN_UTILS_API int ten_file_size(const char *filename);

TEN_UTILS_API char *ten_file_read(const char *filename);

TEN_UTILS_API char *ten_symlink_file_read(const char *path);

TEN_UTILS_API int ten_file_write(const char *filename, ten_buf_t buf);

TEN_UTILS_API int ten_file_write_to_open_file(FILE *fp, ten_buf_t buf);

TEN_UTILS_API int ten_file_clear_open_file_content(FILE *fp);

TEN_UTILS_API int ten_file_copy(const char *src_filename,
                                const char *dest_filename);

TEN_UTILS_API int ten_file_copy_to_dir(const char *src_file,
                                       const char *dest_dir);

TEN_UTILS_API int ten_symlink_file_copy(const char *src_file,
                                        const char *dest_file);

TEN_UTILS_API int ten_file_get_fd(FILE *fp);

TEN_UTILS_API int ten_file_chmod(const char *filename, uint32_t mode);

TEN_UTILS_API int ten_file_clone_permission(const char *src_filename,
                                            const char *dest_filename);

TEN_UTILS_API int ten_file_clone_permission_by_fd(int src_fd, int dest_fd);

/**
 * @brief Open a file for reading.
 *
 * @param does_not_exist If @a does_not_exist is not NULL, @a *does_not_exist
 * will be set to false normally and set to true if the file does not exist. If
 * the file does not exist and @a does_not_exist is not NULL, the function will
 * return -1.
 *
 * @return -1 on error.
 */
TEN_UTILS_API int ten_file_open(const char *filename, bool *does_not_exist);

/**
 * @brief Close a file opened by ten_file_open().
 *
 * @return true on success, false on error.
 */
TEN_UTILS_API bool ten_file_close(int fd);
