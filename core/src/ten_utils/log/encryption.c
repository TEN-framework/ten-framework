//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/log/encryption.h"

#include "ten_utils/lib/string.h"

void ten_log_encryption_init(ten_log_encryption_t *self) {
  TEN_ASSERT(self, "Invalid argument");

  self->on_encrypt = NULL;
  self->on_deinit = NULL;
  self->impl = NULL;
}

void ten_log_encryption_deinit(ten_log_encryption_t *self) {
  TEN_ASSERT(self, "Invalid argument");

  if (self->on_deinit) {
    self->on_deinit(self->impl);
  }

  self->on_encrypt = NULL;
  self->on_deinit = NULL;
  self->impl = NULL;
}

void ten_log_encrypt_data(ten_log_t *self, uint8_t *data, size_t data_len) {
  TEN_ASSERT(self, "Invalid argument");
  TEN_ASSERT(data, "Invalid argument");
  TEN_ASSERT(data_len > 0, "Invalid argument");

  if (self->encryption.on_encrypt) {
    self->encryption.on_encrypt(data, data_len, self->encryption.impl);
  }
}

void ten_log_complete_encryption_header(ten_log_t *self, ten_string_t *buf) {
  TEN_ASSERT(self, "Invalid argument");
  TEN_ASSERT(buf, "Invalid argument");

  uint8_t *buf_ptr = (uint8_t *)ten_string_get_raw_str(buf);

  // The 5-byte header represents:
  //
  // First 2 bytes: 0xFF 0xFF
  // 3rd byte:
  //   - first 6 bits represent version number, default is 0
  //   - 7th bit is reserved
  //   - 8th bit is used for parity check
  // 4th and 5th bytes represent data length, equal to buf length minus
  // header length (5 bytes)

  size_t data_len = ten_string_len(buf) - 5;

  buf_ptr[0] = 0xFF;
  buf_ptr[1] = 0xFF;
  buf_ptr[2] = 0x00;
  buf_ptr[3] = (uint8_t)(data_len >> 8);
  buf_ptr[4] = (uint8_t)(data_len & 0xFF);

  // Calculate parity.
  uint8_t parity = 0;
  for (size_t i = 0; i < 5; i++) {
    parity ^= buf_ptr[i];
  }
  buf_ptr[2] |= (parity & 0x01) << 7;
}

uint8_t *ten_log_get_data_excluding_header(ten_log_t *self, ten_string_t *buf) {
  TEN_ASSERT(self, "Invalid argument");
  TEN_ASSERT(buf, "Invalid argument");

  return (uint8_t *)ten_string_get_raw_str(buf) + 5;
}

size_t ten_log_get_data_excluding_header_len(ten_log_t *self,
                                             ten_string_t *buf) {
  TEN_ASSERT(self, "Invalid argument");
  TEN_ASSERT(buf, "Invalid argument");

  return ten_string_len(buf) - 5;
}