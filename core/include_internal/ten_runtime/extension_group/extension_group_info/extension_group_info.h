//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"

#define TEN_EXTENSION_GROUP_INFO_SIGNATURE 0xBC5114352AFF63AEU

typedef struct ten_property_t ten_property_t;
typedef struct ten_error_t ten_error_t;

// Regardless of whether the extension group is in the current process, all
// extension groups in a graph will have an extension_group_info to store the
// static information of that extension group.
typedef struct ten_extension_group_info_t {
  ten_signature_t signature;

  ten_string_t extension_group_addon_name;
  ten_loc_t loc;

  // The definition of properties in the graph related to the current extension
  // group.
  ten_value_t *property;
} ten_extension_group_info_t;

TEN_RUNTIME_PRIVATE_API bool ten_extension_group_info_check_integrity(
    ten_extension_group_info_t *self);

TEN_RUNTIME_PRIVATE_API ten_extension_group_info_t *
ten_extension_group_info_create(void);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_info_destroy(
    ten_extension_group_info_t *self);

TEN_RUNTIME_PRIVATE_API ten_extension_group_info_t *
ten_extension_group_info_from_smart_ptr(
    ten_smart_ptr_t *extension_group_info_smart_ptr);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *
get_extension_group_info_in_extension_groups_info(
    ten_list_t *extension_groups_info, const char *app_uri,
    const char *graph_id, const char *extension_group_addon_name,
    const char *extension_group_instance_name, bool *new_one_created,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_extension_group_info_clone(
    ten_extension_group_info_t *self, ten_list_t *extension_groups_info);

TEN_RUNTIME_PRIVATE_API void ten_extension_groups_info_fill_app_uri(
    ten_list_t *extension_groups_info, const char *app_uri);

TEN_RUNTIME_PRIVATE_API void ten_extension_groups_info_fill_graph_id(
    ten_list_t *extension_groups_info, const char *graph_id);
