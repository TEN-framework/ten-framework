//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/path.h"  // IWYU pragma: export

typedef struct ten_addon_t ten_addon_t;
typedef struct ten_env_t ten_env_t;

typedef enum TEN_ADDON_TYPE {
  TEN_ADDON_TYPE_INVALID,

  TEN_ADDON_TYPE_EXTENSION,
  TEN_ADDON_TYPE_EXTENSION_GROUP,  // Internal use only.
  TEN_ADDON_TYPE_PROTOCOL,
  TEN_ADDON_TYPE_ADDON_LOADER,
} TEN_ADDON_TYPE;

typedef void (*ten_addon_on_configure_func_t)(ten_addon_t *addon,
                                              ten_env_t *ten_env);

typedef void (*ten_addon_on_create_instance_func_t)(ten_addon_t *addon,
                                                    ten_env_t *ten_env,
                                                    const char *name,
                                                    void *context);

typedef void (*ten_addon_on_destroy_instance_func_t)(ten_addon_t *addon,
                                                     ten_env_t *ten_env,
                                                     void *instance,
                                                     void *context);

typedef void (*ten_addon_on_destroy_func_t)(ten_addon_t *addon);

TEN_RUNTIME_API ten_addon_t *ten_addon_create(
    ten_addon_on_configure_func_t on_configure,
    ten_addon_on_create_instance_func_t on_create_instance,
    ten_addon_on_destroy_instance_func_t on_destroy_instance,
    ten_addon_on_destroy_func_t on_destroy);

TEN_RUNTIME_API void ten_addon_destroy(ten_addon_t *self);
