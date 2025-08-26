//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/log.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "include_internal/ten_utils/lib/string.h"
#include "include_internal/ten_utils/log/encryption.h"
#include "include_internal/ten_utils/log/formatter.h"
#include "include_internal/ten_utils/log/log.h"
#include "include_internal/ten_utils/log/output.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"

bool ten_log_check_integrity(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_LOG_SIGNATURE) {
    return false;
  }

  return true;
}

void ten_log_init(ten_log_t *self, bool enable_advanced_log) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature, TEN_LOG_SIGNATURE);
  self->output_level = TEN_LOG_LEVEL_INVALID;

  ten_log_output_init(&self->output);
  ten_log_set_output_to_stderr(self);
  ten_log_encryption_init(&self->encryption);
  ten_log_advanced_impl_init(&self->advanced_impl);

  self->enable_advanced_log = enable_advanced_log;
}

void ten_log_deinit(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  ten_log_deinit_encryption(self);

  if (self->output.on_close) {
    self->output.on_close(self);
  }

  if (self->output.on_deinit) {
    self->output.on_deinit(self);
  }

  ten_log_advanced_impl_deinit(&self->advanced_impl);
}

void ten_log_deinit_encryption(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  ten_log_encryption_deinit(&self->encryption);
}

void ten_log_destroy(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  ten_log_deinit(self);
  TEN_FREE(self);
}

void ten_log_set_encrypt_cb(ten_log_t *self,
                            ten_log_encrypt_on_encrypt_func_t cb,
                            void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  self->encryption.on_encrypt = cb;
  self->encryption.impl = cb_data;
}

void ten_log_set_encrypt_deinit_cb(ten_log_t *self,
                                   ten_log_encrypt_on_deinit_func_t cb) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  self->encryption.on_deinit = cb;
}

void ten_log_reload(ten_log_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  if (self->advanced_impl.reopen_all) {
    self->advanced_impl.reopen_all(self, self->advanced_impl.config);
    return;
  }

  if (self->output.on_reload) {
    self->output.on_reload(self);
  }
}

static const char *funcname(const char *func) { return func ? func : ""; }

const char *filename(const char *path, size_t path_len, size_t *filename_len) {
  TEN_ASSERT(filename_len, "Invalid argument.");

  if (!path || path_len == 0) {
    *filename_len = 0;
    return "";
  }

  const char *filename = NULL;
  size_t pos = 0;

  // Start from the end of the path and go backwards.
  for (size_t i = path_len; i > 0; i--) {
    if (path[i - 1] == '/' || path[i - 1] == '\\') {
      filename = &path[i];
      pos = i;
      break;
    }
  }

  if (!filename) {
    filename = path;
    pos = 0;
  }

  // Calculate the length of the filename.
  *filename_len = path_len - pos;

  return filename;
}

static void ten_log_log_from_va_list(ten_log_t *self, TEN_LOG_LEVEL level,
                                     const char *func_name,
                                     const char *file_name, size_t line_no,
                                     const char *category, ten_value_t *fields,
                                     const char *fmt, va_list ap) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  ten_string_t msg;
  ten_string_init_from_va_list(&msg, fmt, ap);

  ten_log_log(self, level, func_name, file_name, line_no,
              ten_string_get_raw_str(&msg), category, fields);

  ten_string_deinit(&msg);
}

void ten_log_log_formatted(ten_log_t *self, TEN_LOG_LEVEL level,
                           const char *func_name, const char *file_name,
                           size_t line_no, const char *category,
                           ten_value_t *fields, const char *fmt, ...) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  va_list ap;
  va_start(ap, fmt);

  ten_log_log_from_va_list(self, level, func_name, file_name, line_no, category,
                           fields, fmt, ap);

  va_end(ap);
}

void ten_log_log(ten_log_t *self, TEN_LOG_LEVEL level, const char *func_name,
                 const char *file_name, size_t line_no, const char *msg,
                 const char *category, ten_value_t *fields) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  ten_log_log_with_size(
      self, level, func_name, func_name ? strlen(func_name) : 0, file_name,
      file_name ? strlen(file_name) : 0, line_no, msg, msg ? strlen(msg) : 0,
      category, category ? strlen(category) : 0, fields);
}

void ten_log_log_with_size(ten_log_t *self, TEN_LOG_LEVEL level,
                           const char *func_name, size_t func_name_len,
                           const char *file_name, size_t file_name_len,
                           size_t line_no, const char *msg, size_t msg_len,
                           const char *category, size_t category_len,
                           ten_value_t *fields) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  if (self->enable_advanced_log) {
    if (self->advanced_impl.impl) {
      self->advanced_impl.impl(self, level, category, category_len, func_name,
                               func_name_len, file_name, file_name_len, line_no,
                               msg, msg_len, fields);
    }

    return;
  }

  if (level < self->output_level) {
    return;
  }

  ten_string_t buf;

  if (self->encryption.on_encrypt) {
    TEN_STRING_INIT_ENCRYPTION_HEADER(buf);
  } else {
    TEN_STRING_INIT(buf);
  }

  if (self->formatter.on_format) {
    self->formatter.on_format(&buf, level, func_name, func_name_len, file_name,
                              file_name_len, line_no, msg, msg_len);
  } else {
    // Use default plain formatter if none is set.
    ten_log_plain_formatter(&buf, level, func_name, func_name_len, file_name,
                            file_name_len, line_no, msg, msg_len);
  }

  ten_string_append_formatted(&buf, "%s", TEN_LOG_EOL);

  if (self->encryption.on_encrypt) {
    // Skip the 5-byte header.
    ten_log_encrypt_data(self, ten_log_get_data_excluding_header(self, &buf),
                         ten_log_get_data_excluding_header_len(self, &buf));
    ten_log_complete_encryption_header(self, &buf);
  }

  self->output.on_output(self, &buf);

  ten_string_deinit(&buf);
}

void ten_log_advanced_impl_init(ten_log_advanced_impl_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  self->impl = NULL;
  self->on_deinit = NULL;
  self->reopen_all = NULL;
  self->config = NULL;
  self->is_reloadable = false;
}

void ten_log_advanced_impl_deinit(ten_log_advanced_impl_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->on_deinit) {
    self->on_deinit(self->config);
  }

  self->impl = NULL;
  self->on_deinit = NULL;
  self->reopen_all = NULL;
  self->config = NULL;
}

void ten_log_set_advanced_impl_with_config(
    ten_log_t *self, ten_log_advanced_log_func_t impl,
    ten_log_advanced_log_config_on_deinit_func_t on_deinit,
    ten_log_advanced_log_reopen_all_func_t reopen_all, void *config) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_log_check_integrity(self), "Invalid argument.");

  if (!self->advanced_impl.is_reloadable) {
    TEN_ASSERT(self->advanced_impl.impl == NULL, "Invalid argument.");
    TEN_ASSERT(self->advanced_impl.on_deinit == NULL, "Invalid argument.");
    TEN_ASSERT(self->advanced_impl.config == NULL, "Invalid argument.");
  } else {
    ten_log_advanced_impl_deinit(&self->advanced_impl);
  }

  self->advanced_impl.impl = impl;
  self->advanced_impl.on_deinit = on_deinit;
  self->advanced_impl.reopen_all = reopen_all;
  self->advanced_impl.config = config;
}
