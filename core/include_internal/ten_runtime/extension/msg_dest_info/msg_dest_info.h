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
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"

#define TEN_MSG_DEST_STATIC_INFO_SIGNATURE 0x43B5CAAF1BB9BC41U
#define TEN_MSG_DEST_RUNTIME_INFO_SIGNATURE 0x834F4E128C7658EEU

typedef struct ten_msg_dest_info_t {
  ten_signature_t signature;
  ten_string_t name;  // The name of a message or an interface.
  ten_list_t dest;    // ten_weak_ptr_t of ten_extension_info_t
} ten_msg_dest_info_t;

TEN_RUNTIME_PRIVATE_API bool ten_msg_dest_info_check_integrity(
    ten_msg_dest_info_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_dest_info_t *ten_msg_dest_info_create(
    const char *msg_name);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_msg_dest_info_clone(
    ten_shared_ptr_t *self, ten_list_t *extensions_info, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_msg_dest_info_destroy(
    ten_msg_dest_info_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_dest_info_translate_localhost_to_app_uri(
    ten_msg_dest_info_t *self, const char *uri);

TEN_RUNTIME_PRIVATE_API bool ten_msg_dest_info_qualified(
    ten_msg_dest_info_t *self, const char *msg_name);
