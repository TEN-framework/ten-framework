//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/metadata/metadata.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"

#define TEN_METADATA_INFO_SIGNATURE 0xE3E7657449860D3BU
#define TEN_METADATA_INFOS_SIGNATURE 0x6C5D75C01FD2CBBFU

typedef struct ten_env_t ten_env_t;

typedef void (*ten_metadata_info_destroy_handler_in_target_lang_func_t)(
    void *me_in_target_lang);

typedef enum TEN_METADATA_ATTACH_TO {
  TEN_METADATA_ATTACH_TO_INVALID,

  TEN_METADATA_ATTACH_TO_MANIFEST,
  TEN_METADATA_ATTACH_TO_PROPERTY,
} TEN_METADATA_ATTACH_TO;

typedef struct ten_metadata_info_t {
  ten_signature_t signature;

  TEN_METADATA_ATTACH_TO attach_to;

  TEN_METADATA_TYPE type;
  ten_string_t *value;

  // The object (i.e., addon/extension/extension_group/app) that this metadata
  // belongs to. Because the `value` might be a relative path to the base_dir of
  // the belonging object, and we need to check if it is valid in
  // `ten_metadata_info_set()` before `on_init_done()`, so we need to remember
  // this to get the base_dir.
  ten_env_t *belonging_to;
} ten_metadata_info_t;

TEN_RUNTIME_API bool ten_metadata_info_check_integrity(
    ten_metadata_info_t *self);

TEN_RUNTIME_PRIVATE_API ten_metadata_info_t *ten_metadata_info_create(
    TEN_METADATA_ATTACH_TO attach_to, ten_env_t *belonging_to);
