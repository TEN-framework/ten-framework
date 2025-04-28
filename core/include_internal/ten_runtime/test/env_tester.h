//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/common.h"
#include "ten_utils/lib/signature.h"

#define TEN_ENV_TESTER_SIGNATURE 0x66C619FBA7DC8BD9U

typedef struct ten_extension_tester_t ten_extension_tester_t;

typedef void (*ten_env_tester_close_handler_in_target_lang_func_t)(
    void *me_in_target_lang);

typedef void (*ten_env_tester_destroy_handler_in_target_lang_func_t)(
    void *me_in_target_lang);

typedef struct ten_env_tester_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_extension_tester_t *tester;

  // TODO(Wei): Do we need this close_handler?
  ten_env_tester_close_handler_in_target_lang_func_t close_handler;

  ten_env_tester_destroy_handler_in_target_lang_func_t destroy_handler;
} ten_env_tester_t;

TEN_RUNTIME_API bool ten_env_tester_check_integrity(ten_env_tester_t *self,
                                                    bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_env_tester_t *ten_env_tester_create(
    ten_extension_tester_t *tester);

TEN_RUNTIME_PRIVATE_API void ten_env_tester_destroy(ten_env_tester_t *self);

TEN_RUNTIME_API void ten_env_tester_set_close_handler_in_target_lang(
    ten_env_tester_t *self,
    ten_env_tester_close_handler_in_target_lang_func_t close_handler);

TEN_RUNTIME_API void ten_env_tester_set_destroy_handler_in_target_lang(
    ten_env_tester_t *self,
    ten_env_tester_destroy_handler_in_target_lang_func_t handler);
