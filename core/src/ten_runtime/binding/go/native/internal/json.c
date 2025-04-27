//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/json.h"

#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

ten_json_t *ten_go_json_loads(const void *json_bytes, int json_bytes_len,
                              ten_go_error_t *status) {
  TEN_ASSERT(json_bytes && json_bytes_len > 0, "Should not happen.");
  TEN_ASSERT(status, "Should not happen.");

  ten_string_t input;
  ten_string_init_formatted(&input, "%.*s", json_bytes_len, json_bytes);

  ten_error_t c_err;
  ten_error_init(&c_err);

  ten_json_t *json =
      ten_json_from_string(ten_string_get_raw_str(&input), &c_err);

  ten_string_deinit(&input);
  if (!json) {
    ten_go_error_set(status, TEN_ERROR_CODE_INVALID_JSON,
                     ten_error_message(&c_err));
  }

  ten_error_deinit(&c_err);
  return json;
}
