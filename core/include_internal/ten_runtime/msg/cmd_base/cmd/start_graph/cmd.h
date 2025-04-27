//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"

typedef struct ten_cmd_start_graph_t {
  ten_cmd_t cmd_hdr;

  ten_value_t long_running_mode;  // bool

  // There are two methods to start a graph:
  // 1) one is by explicitly specifying the graph's content through
  //    extension_groups_info and extensions_info.
  // 2) The other is by specifying only a predefined graph name, and then
  //    finding the corresponding graph definition from the app's predefined
  //    graph database.

  // The name of the specified predefined graph.
  ten_value_t predefined_graph_name;  // string

  // ten_shared_ptr_t of ten_extension_group_info_t
  ten_list_t extension_groups_info;

  // ten_shared_ptr_t of ten_extension_info_t
  ten_list_t extensions_info;
} ten_cmd_start_graph_t;

TEN_RUNTIME_PRIVATE_API void
ten_cmd_start_graph_collect_all_immediate_connectable_apps(
    ten_shared_ptr_t *self, ten_app_t *app, ten_list_t *next);

TEN_RUNTIME_PRIVATE_API ten_list_t *ten_raw_cmd_start_graph_get_extensions_info(
    ten_cmd_start_graph_t *self);

TEN_RUNTIME_PRIVATE_API ten_list_t *ten_cmd_start_graph_get_extensions_info(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_list_t *
ten_raw_cmd_start_graph_get_extension_groups_info(ten_cmd_start_graph_t *self);

TEN_RUNTIME_PRIVATE_API ten_list_t *
ten_cmd_start_graph_get_extension_groups_info(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void
ten_cmd_start_graph_add_missing_extension_group_node(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_start_graph_get_long_running_mode(
    ten_cmd_start_graph_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_cmd_start_graph_get_long_running_mode(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_string_t *
ten_raw_cmd_start_graph_get_predefined_graph_name(ten_cmd_start_graph_t *self);

TEN_RUNTIME_PRIVATE_API ten_string_t *
ten_cmd_start_graph_get_predefined_graph_name(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_list_t
ten_cmd_start_graph_get_requested_extension_names(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_cmd_start_graph_fill_loc_info(
    ten_shared_ptr_t *self, const char *app_uri, const char *graph_id);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_start_graph_init_from_json(
    ten_cmd_start_graph_t *self, ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_start_graph_as_msg_destroy(
    ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_list_t
ten_cmd_start_graph_get_extension_addon_and_instance_name_pairs_of_specified_extension_group(
    ten_shared_ptr_t *self, const char *app_uri, const char *graph_id,
    const char *extension_group_name);

TEN_RUNTIME_PRIVATE_API ten_cmd_start_graph_t *ten_raw_cmd_start_graph_create(
    void);

TEN_RUNTIME_PRIVATE_API ten_msg_t *ten_raw_cmd_start_graph_as_msg_clone(
    ten_msg_t *self, ten_list_t *excluded_field_ids);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_start_graph_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);
