//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/log/log.h"

// The first 5 bytes are used to store the encryption header.
// We set the first 5 bytes to magic number 0x11111111.
#define TEN_STRING_INIT_ENCRYPTION_HEADER(var) \
  do {                                         \
    (var) = TEN_STRING_INIT_VAL;               \
    (var).pre_buf[0] = 0x11;                   \
    (var).pre_buf[1] = 0x11;                   \
    (var).pre_buf[2] = 0x11;                   \
    (var).pre_buf[3] = 0x11;                   \
    (var).pre_buf[4] = 0x11;                   \
    (var).buf = (var).pre_buf;                 \
    (var).first_unused_idx = 5;                \
  } while (0)

TEN_UTILS_PRIVATE_API void ten_log_encryption_init(ten_log_encryption_t *self);

TEN_UTILS_PRIVATE_API void ten_log_encryption_deinit(
    ten_log_encryption_t *self);

TEN_UTILS_PRIVATE_API void ten_log_encrypt_data(ten_log_t *self, uint8_t *data,
                                                size_t data_len);

TEN_UTILS_PRIVATE_API void ten_log_complete_encryption_header(
    ten_log_t *self, ten_string_t *buf);

TEN_UTILS_PRIVATE_API uint8_t *ten_log_get_data_excluding_header(
    ten_log_t *self, ten_string_t *buf);

TEN_UTILS_PRIVATE_API size_t
ten_log_get_data_excluding_header_len(ten_log_t *self, ten_string_t *buf);
