//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/common/loc.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_EXTENSION_CONTEXT_SIGNATURE 0x5968C666394DBCCCU

typedef struct ten_env_t ten_env_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_extension_t ten_extension_t;
typedef struct ten_extension_info_t ten_extension_info_t;
typedef struct ten_extension_store_t ten_extension_store_t;
typedef struct ten_extension_context_t ten_extension_context_t;
typedef struct ten_extension_group_info_t ten_extension_group_info_t;
typedef struct ten_extension_group_t ten_extension_group_t;

typedef void (*ten_extension_context_on_closed_func_t)(
    ten_extension_context_t *self, void *on_closed_data);

struct ten_extension_context_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  // Even if enabling runtime Extension-group addition/deletion, all
  // 'extension_threads' relevant operations should be done in the engine's main
  // thread, so we don't need to apply any locking mechanism for it.
  ten_list_t extension_threads;

  size_t extension_threads_total_cnt;
  size_t extension_threads_cnt_of_ready;
  size_t extension_threads_cnt_of_closed;

  ten_list_t extension_groups_info_from_graph;
  ten_list_t extensions_info_from_graph;  // ten_extension_info_t*

  ten_extension_context_on_closed_func_t on_closed;
  void *on_closed_data;

  ten_engine_t *engine;
};

TEN_RUNTIME_PRIVATE_API bool ten_extension_context_check_integrity(
    ten_extension_context_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_extension_context_t *ten_extension_context_create(
    ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API void ten_extension_context_close(
    ten_extension_context_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_context_set_on_closed(
    ten_extension_context_t *self,
    ten_extension_context_on_closed_func_t on_closed, void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_extension_context_on_close(
    ten_extension_context_t *self);

TEN_RUNTIME_PRIVATE_API ten_extension_info_t *
ten_extension_context_get_extension_info_by_name(
    ten_extension_context_t *self, const char *app_uri, const char *graph_id,
    const char *extension_group_name, const char *extension_name);

TEN_RUNTIME_PRIVATE_API bool ten_extension_context_start_extension_group(
    ten_extension_context_t *self, ten_error_t *err);
