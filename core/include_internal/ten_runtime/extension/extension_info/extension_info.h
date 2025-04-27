//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/all_msg_type_dest_info.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"

#define TEN_EXTENSION_INFO_SIGNATURE 0xE313C401D4C0C3C2U

typedef struct ten_property_t ten_property_t;
typedef struct ten_error_t ten_error_t;

// Regardless of whether the extension is in the current process, all extensions
// in a graph will have an extension_info to store the static information of
// that extension.
typedef struct ten_extension_info_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_string_t extension_addon_name;
  ten_loc_t loc;

  // The extension_info of the destination extension for each type of message.
  ten_all_msg_type_dest_info_t msg_dest_info;

  // The definition of properties in the graph related to the current extension.
  ten_value_t *property;

  ten_list_t msg_conversion_contexts;  // ten_msg_conversion_context_t
} ten_extension_info_t;

TEN_RUNTIME_PRIVATE_API bool ten_extensions_info_clone(ten_list_t *from,
                                                       ten_list_t *to,
                                                       ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_extension_info_check_integrity(
    ten_extension_info_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_extension_info_translate_localhost_to_app_uri(
    ten_extension_info_t *self, const char *app_uri);

TEN_RUNTIME_PRIVATE_API bool ten_extension_info_is_desired_extension_group(
    ten_extension_info_t *self, const char *app_uri,
    const char *extension_group_instance_name);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *get_extension_info_in_extensions_info(
    ten_list_t *extensions_info, const char *app_uri, const char *graph_id,
    const char *extension_group_name, const char *extension_addon_name,
    const char *extension_instance_name, bool should_exist, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_extension_info_t *ten_extension_info_from_smart_ptr(
    ten_smart_ptr_t *smart_ptr);

TEN_RUNTIME_PRIVATE_API void ten_extensions_info_fill_app_uri(
    ten_list_t *extensions_info, const char *app_uri);

TEN_RUNTIME_PRIVATE_API void ten_extensions_info_fill_loc_info(
    ten_list_t *extensions_info, const char *app_uri, const char *graph_id);
