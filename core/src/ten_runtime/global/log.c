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
