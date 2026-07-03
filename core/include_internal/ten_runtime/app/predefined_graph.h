//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"

typedef struct ten_app_t ten_app_t;
typedef struct ten_engine_t ten_engine_t;

typedef struct ten_predefined_graph_info_t {
  ten_string_t name;

  bool auto_start;
  bool singleton;

  // ten_shared_ptr_t of ten_extension_info_t
  ten_list_t extensions_info;
  // ten_shared_ptr_t of ten_extension_group_info_t
  ten_list_t extension_groups_info;

  // 'singleton == true' means that there can only be one instance of this
  // predefined_graph in the entire process. In this case, the predefined graph
  // name is the same as the graph ID of that instance. Therefore, the 'engine'
  // field is only meaningful when the 'singleton' field is set to true.
  ten_engine_t *engine;
} ten_predefined_graph_info_t;

TEN_RUNTIME_PRIVATE_API ten_predefined_graph_info_t *
ten_predefined_graph_info_create(void);

TEN_RUNTIME_PRIVATE_API void ten_predefined_graph_info_destroy(
    ten_predefined_graph_info_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_app_start_auto_start_predefined_graph(
    ten_app_t *self, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_app_start_predefined_graph(
    ten_app_t *self, ten_predefined_graph_info_t *predefined_graph_info,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_predefined_graph_info_t *
ten_predefined_graph_infos_get_singleton_by_name(
    ten_list_t *predefined_graph_infos, const char *name);

TEN_RUNTIME_PRIVATE_API ten_engine_t *
ten_app_get_singleton_predefined_graph_engine_by_name(ten_app_t *self,
                                                      const char *name);

TEN_RUNTIME_PRIVATE_API bool ten_app_get_predefined_graphs_from_property(
    ten_app_t *self);
