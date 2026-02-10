//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/test/env_tester.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/lib/string.h"

typedef struct ten_env_tester_notify_log_ctx_t {
  int32_t level;
  ten_string_t func_name;
  ten_string_t file_name;
  size_t line_no;
  ten_string_t msg;
  ten_string_t category;
  uint8_t *fields_buf;
  size_t fields_buf_size;
} ten_env_tester_notify_log_ctx_t;

static ten_env_tester_notify_log_ctx_t *ten_env_tester_notify_log_ctx_create(
    int32_t level, const char *func_name, size_t func_name_len,
    const char *file_name, size_t file_name_len, size_t line_no,
    const char *msg, size_t msg_len, const char *category, size_t category_len,
    const uint8_t *fields_buf, size_t fields_buf_size) {
  ten_env_tester_notify_log_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_tester_notify_log_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->level = level;
  ten_string_init_from_c_str_with_size(&ctx->func_name, func_name,
                                       func_name_len);
  ten_string_init_from_c_str_with_size(&ctx->file_name, file_name,
                                       file_name_len);
  ctx->line_no = line_no;
  ten_string_init_from_c_str_with_size(&ctx->msg, msg, msg_len);
  ten_string_init_from_c_str_with_size(&ctx->category, category, category_len);

  // Copy fields buffer if provided
  if (fields_buf != NULL && fields_buf_size > 0) {
    ctx->fields_buf = TEN_MALLOC(fields_buf_size);
    TEN_ASSERT(ctx->fields_buf, "Failed to allocate memory for fields buffer.");
    memcpy(ctx->fields_buf, fields_buf, fields_buf_size);
    ctx->fields_buf_size = fields_buf_size;
  } else {
    ctx->fields_buf = NULL;
    ctx->fields_buf_size = 0;
  }

  return ctx;
}

static void ten_env_tester_notify_log_ctx_destroy(
    ten_env_tester_notify_log_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->func_name);
  ten_string_deinit(&ctx->file_name);
  ten_string_deinit(&ctx->msg);
  ten_string_deinit(&ctx->category);

  if (ctx->fields_buf != NULL) {
    TEN_FREE(ctx->fields_buf);
  }

  TEN_FREE(ctx);
}

static void ten_go_ten_env_tester_log_proxy_notify(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  ten_env_tester_notify_log_ctx_t *ctx =
      (ten_env_tester_notify_log_ctx_t *)user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_env_tester_log(
      ten_env_tester, ctx->level, ten_string_get_raw_str(&ctx->func_name),
      ten_string_get_raw_str(&ctx->file_name), ctx->line_no,
      ten_string_get_raw_str(&ctx->msg), ten_string_get_raw_str(&ctx->category),
      ctx->fields_buf, ctx->fields_buf_size, NULL);

  ten_env_tester_notify_log_ctx_destroy(ctx);
}

ten_go_error_t ten_go_ten_env_tester_log(
    uintptr_t bridge_addr, int level, const void *func_name, int func_name_len,
    const void *file_name, int file_name_len, int line_no, const void *msg,
    int msg_len, const void *category, int category_len, const void *fields_buf,
    int fields_buf_size) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  if (!self->c_ten_env_tester_proxy) {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_GENERIC);
    return cgo_error;
  }

  // According to the document of `unsafe.StringData()`, the underlying data
  // (i.e., value here) of an empty GO string is unspecified. So it's unsafe to
  // access. We should handle this case explicitly.
  const char *func_name_value = NULL;
  if (func_name_len > 0) {
    func_name_value = (const char *)func_name;
  }

  const char *file_name_value = NULL;
  if (file_name_len > 0) {
    file_name_value = (const char *)file_name;
  }

  const char *msg_value = NULL;
  if (msg_len > 0) {
    msg_value = (const char *)msg;
  }

  const char *category_value = NULL;
  if (category_len > 0) {
    category_value = (const char *)category;
  }

  // Handle fields buffer
  const uint8_t *fields_buf_value = NULL;
  size_t fields_buf_size_value = 0;
  if (fields_buf != NULL && fields_buf_size > 0) {
    fields_buf_value = (const uint8_t *)fields_buf;
    fields_buf_size_value = (size_t)fields_buf_size;
  }

  ten_env_tester_notify_log_ctx_t *ctx = ten_env_tester_notify_log_ctx_create(
      level, func_name_value, func_name_len, file_name_value, file_name_len,
      line_no, msg_value, msg_len, category_value, category_len,
      fields_buf_value, fields_buf_size_value);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_proxy_notify(self->c_ten_env_tester_proxy,
                                        ten_go_ten_env_tester_log_proxy_notify,
                                        ctx, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  return cgo_error;
}
