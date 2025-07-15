//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "include_internal/ten_utils/log/formatter.h"
#include "include_internal/ten_utils/log/log.h"
#include "include_internal/ten_utils/log/output.h"
#include "ten_utils/lib/file.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define PATH_SEPARATOR '\\'
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#define PATH_SEPARATOR '/'
#endif

// Function to create directories recursively.
static bool create_directories(const char *path) {
  char *path_copy = TEN_STRDUP(path);
  TEN_ASSERT(path_copy, "Failed to allocate memory.");
  if (!path_copy) {
    return false;
  }

  size_t len = strlen(path_copy);
  // Remove trailing separators.
  while (len > 0 && (path_copy[len - 1] == '/' || path_copy[len - 1] == '\\')) {
    path_copy[len - 1] = '\0';
    len--;
  }

  for (size_t i = 1; i < len; i++) {
    if (path_copy[i] == '/' || path_copy[i] == '\\') {
      char temp = path_copy[i];
      path_copy[i] = '\0';

      // Skip if path is root on Unix or drive letter on Windows.
#if defined(_WIN32) || defined(_WIN64)
      if (i == 2 && path_copy[1] == ':') {
        path_copy[i] = temp;
        continue;
      }
#endif

#if defined(_WIN32) || defined(_WIN64)
      if (!CreateDirectoryA(path_copy, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
          TEN_FREE(path_copy);
          return false;
        }
      }
#else
      if (mkdir(path_copy, 0755) != 0) {
        if (errno != EEXIST) {
          TEN_FREE(path_copy);
          return false;
        }
      }
#endif
      path_copy[i] = temp;
    }
  }

#if defined(_WIN32) || defined(_WIN64)
  if (!CreateDirectoryA(path_copy, NULL)) {
    DWORD err = GetLastError();
    if (err != ERROR_ALREADY_EXISTS) {
      TEN_FREE(path_copy);
      return false;
    }
  }
#else
  if (mkdir(path_copy, 0755) != 0) {
    if (errno != EEXIST) {
      TEN_FREE(path_copy);
      return false;
    }
  }
#endif

  TEN_FREE(path_copy);
  return true;
}

static void ten_log_output_set(ten_log_t *self, TEN_LOG_OUTPUT_TYPE type,
                               const ten_log_output_on_output_func_t output_cb,
                               const ten_log_output_on_close_func_t close_cb,
                               const ten_log_output_on_reload_func_t reload_cb,
                               const ten_log_output_on_deinit_func_t deinit_cb,
                               void *user_data) {
  TEN_ASSERT(self, "Invalid argument.");

  // Close the previous output.
  if (self->output.on_close) {
    self->output.on_close(self);
  }

  // Deinitialize the previous output.
  if (self->output.on_deinit) {
    self->output.on_deinit(self);
  }

  self->output.on_output = output_cb;
  self->output.on_close = close_cb;
  self->output.on_reload = reload_cb;
  self->output.on_deinit = deinit_cb;

  self->output.type = type;
  self->output.user_data = user_data;
}

static void ten_log_close_file(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  if (self->output.type != TEN_LOG_OUTPUT_TYPE_FILE) {
    return;
  }

  ten_log_output_to_file_ctx_t *ctx =
      (ten_log_output_to_file_ctx_t *)self->output.user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  int *fd = ctx->fd;
  TEN_ASSERT(fd && *fd, "Invalid argument.");

#if defined(_WIN32) || defined(_WIN64)
  HANDLE handle = (HANDLE)_get_osfhandle(*fd);
  CloseHandle(handle);
#else
  close(*fd);
#endif

  TEN_FREE(fd);
  ctx->fd = NULL;
}

static int *get_log_fd(const char *log_path) {
  TEN_ASSERT(log_path, "log_path cannot be NULL.");

  // Duplicate the log_path to manipulate it.
  char *path_copy = TEN_STRDUP(log_path);
  TEN_ASSERT(path_copy, "Failed to allocate memory.");
  if (!path_copy) {
    return NULL;
  }

  // Find the directory part of the path.
  char *last_sep = strrchr(path_copy, PATH_SEPARATOR);
  if (last_sep) {
    *last_sep = '\0';  // Terminate the string to get the directory path.

    // Create directories recursively.
    if (create_directories(path_copy) != true) {
      // Failed to create directories
      TEN_FREE(path_copy);
      return NULL;
    }
  }

  TEN_FREE(path_copy);

  // Now, attempt to open the file.
  FILE *fp = fopen(log_path, "ab");
  if (!fp) {
    // Handle fopen failure.
    return NULL;
  }

  int *fd_ptr = TEN_MALLOC(sizeof(int));
  TEN_ASSERT(fd_ptr, "Failed to allocate memory.");
  if (!fd_ptr) {
    (void)fclose(fp);
    return NULL;
  }

  *fd_ptr = ten_file_get_fd(fp);
  return fd_ptr;
}

ten_log_output_to_file_ctx_t *ten_log_output_to_file_ctx_create(
    int *fd, const char *log_path) {
  ten_log_output_to_file_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_log_output_to_file_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->fd = fd;
  ten_string_init_from_c_str_with_size(&ctx->log_path, log_path,
                                       strlen(log_path));
  ten_atomic_store(&ctx->need_reload, 0);

  ctx->mutex = ten_mutex_create();
  TEN_ASSERT(ctx->mutex, "Failed to allocate memory.");

  return ctx;
}

void ten_log_output_to_file_ctx_destroy(ten_log_output_to_file_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->log_path);
  ten_mutex_destroy(ctx->mutex);

  TEN_FREE(ctx);
}

void ten_log_output_init(ten_log_output_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  self->on_output = NULL;
  self->on_close = NULL;
  self->on_reload = NULL;
  self->on_deinit = NULL;

  self->user_data = NULL;
  self->type = TEN_LOG_OUTPUT_TYPE_INVALID;
}

void ten_log_output_to_file(ten_log_t *self, ten_string_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");

  if (self->output.type != TEN_LOG_OUTPUT_TYPE_FILE) {
    return;
  }

  ten_log_output_to_file_ctx_t *ctx =
      (ten_log_output_to_file_ctx_t *)self->output.user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ten_atomic_load(&ctx->need_reload) != 0) {
    ten_mutex_lock(ctx->mutex);

    if (ten_atomic_load(&ctx->need_reload) != 0) {
      if (self->output.on_close) {
        self->output.on_close(self);
      }

      ctx->fd = get_log_fd(ten_string_get_raw_str(&ctx->log_path));
      // TODO(xilin): handle the error
      TEN_ASSERT(ctx->fd, "Should not happen.");

      ten_atomic_store(&ctx->need_reload, 0);
    }

    ten_mutex_unlock(ctx->mutex);
  }

#if defined(_WIN32) || defined(_WIN64)
  HANDLE handle = *(HANDLE *)ctx->fd;

  // WriteFile() is atomic for local files opened with
  // FILE_APPEND_DATA and without FILE_WRITE_DATA
  DWORD written;
  WriteFile(handle, ten_string_get_raw_str(msg), (DWORD)ten_string_len(msg),
            &written, 0);
#else
  int fd = *ctx->fd;

  // TODO(Wei): write() is atomic for buffers less than or equal to PIPE_BUF,
  // therefore we need to have some locking mechanism here to prevent log
  // interleaved.
  ssize_t bytes_written =
      write(fd, ten_string_get_raw_str(msg), ten_string_len(msg));
  if (bytes_written == -1) {
    perror("write failed");
  } else if ((size_t)bytes_written < ten_string_len(msg)) {
    (void)fprintf(stderr, "Partial write: only %zd of %zu bytes written\n",
                  bytes_written, ten_string_len(msg));
  }
#endif
}

static void ten_log_output_to_file_deinit(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->output.on_output == ten_log_output_to_file,
             "Invalid argument.");

  if (self->output.type != TEN_LOG_OUTPUT_TYPE_FILE) {
    return;
  }

  ten_log_output_to_file_ctx_t *ctx =
      (ten_log_output_to_file_ctx_t *)self->output.user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_log_output_to_file_ctx_destroy(ctx);
}

static void ten_log_output_to_file_reload(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  if (self->output.type != TEN_LOG_OUTPUT_TYPE_FILE) {
    return;
  }

  ten_log_output_to_file_ctx_t *ctx =
      (ten_log_output_to_file_ctx_t *)self->output.user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_atomic_store(&ctx->need_reload, 1);
}

void ten_log_set_output_to_file(ten_log_t *self, const char *log_path) {
  TEN_ASSERT(log_path, "Invalid argument.");

  int *fd = get_log_fd(log_path);
  if (!fd) {
    // Failed to open log file or create directories.
    ten_log_set_output_to_stderr(self);
    return;
  }

  ten_log_output_to_file_ctx_t *ctx =
      ten_log_output_to_file_ctx_create(fd, log_path);
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_log_output_set(self, TEN_LOG_OUTPUT_TYPE_FILE, ten_log_output_to_file,
                     ten_log_close_file, ten_log_output_to_file_reload,
                     ten_log_output_to_file_deinit, ctx);

  ten_log_set_formatter(self, ten_log_plain_formatter, NULL);
}

const char *ten_log_get_output_file_path(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  if (self->output.type != TEN_LOG_OUTPUT_TYPE_FILE) {
    return NULL;
  }

  ten_log_output_to_file_ctx_t *ctx =
      (ten_log_output_to_file_ctx_t *)self->output.user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_mutex_lock(ctx->mutex);
  const char *path = ten_string_get_raw_str(&ctx->log_path);
  ten_mutex_unlock(ctx->mutex);

  return path;
}

void ten_log_output_to_stderr(ten_log_t *self, ten_string_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");

#if defined(_WIN32) || defined(_WIN64)
  // WriteFile() is atomic for local files opened with FILE_APPEND_DATA and
  // without FILE_WRITE_DATA
  DWORD written;
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), ten_string_get_raw_str(msg),
            (DWORD)ten_string_len(msg), &written, 0);
#else
  // TODO(Wei): write() is atomic for buffers less than or equal to PIPE_BUF,
  // therefore we need to have some locking mechanism here to prevent log
  // interleaved.
  ssize_t bytes_written =
      write(STDERR_FILENO, ten_string_get_raw_str(msg), ten_string_len(msg));
  if (bytes_written == -1) {
    perror("write failed");
  } else if ((size_t)bytes_written < ten_string_len(msg)) {
    (void)fprintf(stderr, "Partial write: only %zd of %zu bytes written\n",
                  bytes_written, ten_string_len(msg));
  }
#endif
}

void ten_log_set_output_to_stderr(ten_log_t *self) {
  ten_log_output_set(self, TEN_LOG_OUTPUT_TYPE_STDERR, ten_log_output_to_stderr,
                     NULL, NULL, NULL, NULL);

  ten_log_formatter_on_format_func_t formatter_func = NULL;

#if defined(OS_LINUX) || defined(OS_MACOS)
  formatter_func = ten_log_plain_colored_formatter;
#else
  formatter_func = ten_log_plain_formatter;
#endif

  // The default formatter for `stderr` can be overridden using the below
  // environment variable.
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char *formatter_env = getenv("TEN_LOG_FORMATTER");
  if (formatter_env) {
    ten_log_formatter_on_format_func_t formatter_func_from_env =
        ten_log_get_formatter_by_name(formatter_env);

    // If the environment variable specifies a formatter, use it; otherwise,
    // stick to the default.
    if (formatter_func_from_env) {
      formatter_func = formatter_func_from_env;
    }
  }

  ten_log_set_formatter(self, formatter_func, NULL);
}
