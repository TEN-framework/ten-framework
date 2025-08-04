//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/global/log.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)
#include "include_internal/ten_rust/ten_rust.h"
#include "ten_utils/macro/mark.h"
#endif

void ten_encrypt_log_data(uint8_t *data, size_t data_len, void *user_data) {
#if defined(TEN_ENABLE_TEN_RUST_APIS)
  Cipher *cipher = (Cipher *)user_data;
  TEN_UNUSED bool rc = ten_cipher_encrypt_inplace(cipher, data, data_len);
  // For now, we just ignore the return value.
#endif
}

void ten_encrypt_log_deinit(void *user_data) {
#if defined(TEN_ENABLE_TEN_RUST_APIS)
  Cipher *cipher = (Cipher *)user_data;
  ten_cipher_destroy(cipher);
#endif
}

void ten_log_rust_log_func(ten_log_t *self, TEN_LOG_LEVEL level,
                           const char *category, const char *func_name,
                           const char *file_name, size_t line_no,
                           const char *msg) {
#if defined(TEN_ENABLE_TEN_RUST_APIS)
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->advanced_impl.impl, "Invalid argument.");
  TEN_ASSERT(self->advanced_impl.config, "Invalid argument.");

  int64_t pid = 0;
  int64_t tid = 0;
  ten_get_pid_tid(&pid, &tid);
  ten_rust_log(self->advanced_impl.config, category, pid, tid, level, func_name,
               file_name, line_no, msg);
#endif
}

void ten_log_rust_config_deinit(void *config) {
#if defined(TEN_ENABLE_TEN_RUST_APIS)
  TEN_ASSERT(config, "Invalid argument.");

  ten_rust_log_config_destroy(config);
#endif
}
