//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/path/common.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_PATH_TABLE_SIGNATURE 0xB7C015B5FD691797U

typedef struct ten_loc_t ten_loc_t;
typedef struct ten_path_group_t ten_path_group_t;
typedef struct ten_extension_t ten_extension_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_path_t ten_path_t;
typedef struct ten_path_out_t ten_path_out_t;
typedef struct ten_path_in_t ten_path_in_t;
typedef struct ten_app_t ten_app_t;
typedef struct ten_msg_conversion_t ten_msg_conversion_t;

typedef enum TEN_PATH_TABLE_ATTACH_TO {
  TEN_PATH_TABLE_ATTACH_TO_INVALID,

  TEN_PATH_TABLE_ATTACH_TO_APP,
  TEN_PATH_TABLE_ATTACH_TO_ENGINE,
  TEN_PATH_TABLE_ATTACH_TO_EXTENSION,
} TEN_PATH_TABLE_ATTACH_TO;

typedef struct ten_path_table_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_PATH_TABLE_ATTACH_TO attach_to;
  union {
    ten_app_t *app;
    ten_engine_t *engine;
    ten_extension_t *extension;
  } attached_target;

  ten_list_t in_paths;   // ten_path_in_t
  ten_list_t out_paths;  // ten_path_out_t
} ten_path_table_t;

TEN_RUNTIME_PRIVATE_API ten_path_table_t *ten_path_table_create(
    TEN_PATH_TABLE_ATTACH_TO attach_to, void *attached_target);

TEN_RUNTIME_PRIVATE_API void ten_path_table_destroy(ten_path_table_t *self);

TEN_RUNTIME_PRIVATE_API void ten_path_table_check_empty(ten_path_table_t *self);

TEN_RUNTIME_PRIVATE_API ten_path_in_t *ten_path_table_add_in_path(
    ten_path_table_t *self, ten_shared_ptr_t *cmd,
    ten_msg_conversion_t *result_conversion);

TEN_RUNTIME_PRIVATE_API ten_path_out_t *ten_path_table_add_out_path(
    ten_path_table_t *self, ten_shared_ptr_t *cmd);

TEN_RUNTIME_PRIVATE_API bool ten_path_table_process_cmd_result(
    ten_path_table_t *self, TEN_PATH_TYPE path_type,
    ten_shared_ptr_t *cmd_result, ten_shared_ptr_t **processed_cmd_result);

TEN_RUNTIME_PRIVATE_API ten_string_t *ten_path_table_get_graph_id(
    ten_path_table_t *self);
