//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/thread.h"

#define TEN_SANITIZER_THREAD_CHECK_SIGNATURE 0x6204388773560E59U

#define TEN_SANITIZER_THREAD_CHECK_INIT_VAL           \
  (ten_sanitizer_thread_check_t) {                    \
    TEN_SANITIZER_THREAD_CHECK_SIGNATURE, NULL, false \
  }

typedef struct ten_sanitizer_thread_check_t {
  ten_signature_t signature;

  ten_thread_t *belonging_thread;
  bool is_fake;
} ten_sanitizer_thread_check_t;

TEN_UTILS_PRIVATE_API bool ten_sanitizer_thread_check_check_integrity(
    ten_sanitizer_thread_check_t *self);

TEN_UTILS_API void ten_sanitizer_thread_check_init_with_current_thread(
    ten_sanitizer_thread_check_t *self);

TEN_UTILS_API void ten_sanitizer_thread_check_init_from(
    ten_sanitizer_thread_check_t *self, ten_sanitizer_thread_check_t *other);

TEN_UTILS_API ten_thread_t *ten_sanitizer_thread_check_get_belonging_thread(
    ten_sanitizer_thread_check_t *self);

TEN_UTILS_API void ten_sanitizer_thread_check_set_belonging_thread(
    ten_sanitizer_thread_check_t *self, ten_thread_t *thread);

TEN_UTILS_API void
ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
    ten_sanitizer_thread_check_t *self);

TEN_UTILS_API void ten_sanitizer_thread_check_inherit_from(
    ten_sanitizer_thread_check_t *self, ten_sanitizer_thread_check_t *from);

TEN_UTILS_API bool ten_sanitizer_thread_check_do_check(
    ten_sanitizer_thread_check_t *self);

TEN_UTILS_API void ten_sanitizer_thread_check_deinit(
    ten_sanitizer_thread_check_t *self);
