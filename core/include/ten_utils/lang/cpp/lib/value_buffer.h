//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lang/cpp/lib/error.h"
#include "ten_utils/lang/cpp/lib/value.h"

namespace ten {
namespace value_buffer {

inline uint8_t *serialize_to_buffer(const value_t &value, size_t *buffer_size,
                                    error_t *err = nullptr) {
  // TODO(Wei): use C layer buffer protocol

  return nullptr;
}

inline value_t deserialize_from_buffer(const uint8_t *buffer,
                                       size_t buffer_size,
                                       size_t *bytes_consumed = nullptr,
                                       error_t *err = nullptr) {
  // TODO(Wei): use C layer buffer protocol

  // Return an invalid value as placeholder
  return value_t();
}

}  // namespace value_buffer
}  // namespace ten
