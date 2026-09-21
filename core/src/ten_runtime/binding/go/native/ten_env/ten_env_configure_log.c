// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
#include "ten_runtime/binding/go/interface/ten_runtime/log.h"

#include <string.h>

#include "include_internal/ten_runtime/global/log.h"
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#if defined(TEN_ENABLE_TEN_RUST_APIS)
#include "include_internal/ten_rust/ten_rust.h"
#endif
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"

ten_go_error_t ten_go_configure_log_json(const void *json, int json_len) {
  ten_go_error_t result;
  TEN_GO_ERROR_INIT(result);

  if (json == NULL || json_len <= 0) {
    ten_go_error_set(&result, TEN_ERROR_CODE_INVALID_ARGUMENT,
                     "log configuration is empty");
    return result;
  }

  char *json_copy = TEN_MALLOC((size_t)json_len + 1);
  TEN_ASSERT(json_copy, "Failed to allocate log configuration.");
  memcpy(json_copy, json, (size_t)json_len);
  json_copy[json_len] = '\0';

  char *err_msg = NULL;
  bool success = ten_log_global_configure_from_json(json_copy, &err_msg);
  TEN_FREE(json_copy);

  if (!success) {
    ten_go_error_set(&result, TEN_ERROR_CODE_GENERIC,
                     err_msg ? err_msg : "failed to configure log");
  }

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  if (err_msg) {
    ten_rust_free_cstring(err_msg);
  }
#else
  TEN_UNUSED(err_msg);
#endif
  return result;
}
