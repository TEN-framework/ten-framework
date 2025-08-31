//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "ten_utils/lib/error.h"

typedef struct ten_app_t ten_app_t;
typedef struct ten_list_t ten_list_t;
typedef struct ten_string_t ten_string_t;
typedef struct ten_value_t ten_value_t;

typedef bool (*ten_app_ten_namespace_prop_init_from_value_func_t)(
    ten_app_t *self, ten_value_t *value);

typedef struct ten_app_ten_namespace_prop_info_t {
  const char *name;
  ten_app_ten_namespace_prop_init_from_value_func_t init_from_value;
} ten_app_ten_namespace_prop_info_t;

TEN_RUNTIME_PRIVATE_API void ten_app_handle_metadata(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_app_handle_ten_namespace_properties(
    ten_app_t *self);

TEN_RUNTIME_PRIVATE_API bool
ten_app_get_predefined_graph_extensions_and_groups_info_by_name(
    ten_app_t *self, const char *name, ten_list_t *extensions_info,
    ten_list_t *extension_groups_info, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_app_get_ten_namespace_properties(
    ten_app_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_app_init_one_event_loop_per_engine(
    ten_app_t *self, ten_value_t *value);

TEN_RUNTIME_PRIVATE_API bool ten_app_init_long_running_mode(ten_app_t *self,
                                                            ten_value_t *value);

TEN_RUNTIME_PRIVATE_API bool ten_app_init_uri(ten_app_t *self,
                                              ten_value_t *value);

TEN_RUNTIME_PRIVATE_API bool ten_app_init_deprecated_log(ten_app_t *self,
                                                         ten_value_t *value);

TEN_RUNTIME_PRIVATE_API bool ten_app_init_addon(ten_app_t *self,
                                                ten_value_t *value);

TEN_RUNTIME_PRIVATE_API bool ten_app_init_advanced_log(ten_app_t *self,
                                                       ten_value_t *value);
